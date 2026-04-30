/**
 * @file App_ImportExport.cpp
 * @brief App 导入/导出相关逻辑：PNG、精灵图、导入导出配置弹窗
 */

#include "app/App.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "io/ImageExporter.h"
#include "io/ImageImporter.h"
#include "imgui.h"
#include "render/Texture.h"
#include "ui/windows/ProjectWindow.h"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <sstream>
#include <string_view>

namespace
{
    /**
     * @brief 返回字符串的小写副本。
     *
     * 主要用于扩展名比较，避免 Windows 路径中大小写不同导致判断失败。
     */
    std::string toLowerCopy(const std::string& value)
    {
        std::string result = value;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return result;
    }

    /**
     * @brief 判断字符串是否以指定后缀结尾，忽略大小写。
     *
     * 用于识别 .png 等文件扩展名，让用户输入 PNG/png/PnG 都能正常处理。
     */
    bool endsWithInsensitive(const std::string& text, std::string_view suffix)
    {
        if (text.size() < suffix.size()) return false;
        const std::string lower = toLowerCopy(text);
        return lower.compare(lower.size() - suffix.size(), suffix.size(), suffix.data()) == 0;
    }

    /**
     * @brief 规范化 PNG 导出路径。
     *
     * 如果用户保存时没有输入 .png 后缀，就自动补上，避免导出文件无扩展名。
     */
    std::string normalizePngPath(const std::string& path)
    {
        if (path.empty()) return path;
        if (endsWithInsensitive(path, ".png")) return path;
        return path + ".png";
    }

    /**
     * @brief 从文件路径推导项目名称。
     *
     * 优先去掉 .pxanim.json 这样的复合扩展名；解析失败时回退为 Untitled。
     */
    std::string projectNameFromPath(const std::string& path)
    {
        try
        {
            const std::filesystem::path p(path);
            const std::string filename = p.filename().string();
            if (filename.size() > std::string(".pxanim.json").size())
            {
                const std::string lowerFilename = toLowerCopy(filename);
                if (lowerFilename.size() >= 12
                    && lowerFilename.rfind(".pxanim.json") == lowerFilename.size() - 12)
                {
                    return filename.substr(0, filename.size() - 12);
                }
            }
            const std::string stem = p.stem().string();
            if (!stem.empty()) return stem;
        }
        catch (...)
        {
        }
        return "Untitled";
    }

    /**
     * @brief 导入自动分组时使用的高区分度调色板。
     *
     * 按组索引循环取色，让导入后的行/列分组在时间轴上更容易区分。
     */
    uint32_t importGroupColorByIndex(size_t index)
    {
        static const uint32_t kPalette[] = {
            0x5EA1FFFFu,
            0x6ED6A0FFu,
            0xF2B566FFu,
            0xC98CFFFFu,
            0x82D8F4FFu,
            0xF48AA1FFu,
            0xA2D56DFFu,
            0xF4D36AFFu
        };
        return kPalette[index % (sizeof(kPalette) / sizeof(kPalette[0]))];
    }
}
/**
 * @brief 打开导出文件选择弹窗。
 *
 * 根据当前导出类型准备默认文件名，并在精灵图导出时缓存本次导出配置；
 * 弹窗关闭后的真实导出动作会在 pollDialogResults() 中统一处理。
 */
void App::requestExportDialog(ExportKind kind)
{
    if (m_exportDialogInFlight) return;
    if (!m_activeContext || !m_activeContext->hasProject())
    {
        showError("No active project to export.");
        return;
    }

    m_exportDialogKind = kind;
    m_exportDialogSpriteMode = m_spriteSheetExportMode;
    m_exportDialogUseSelectedFrames = m_spriteSheetExportUseSelectedFrames;
    m_exportDialogColumnsPerRow = std::max(1, m_spriteSheetExportColumnsPerRow);
    m_exportDialogUseCustomGroups = m_spriteSheetExportUseCustomGroups;
    m_exportDialogGroupSpacing = std::max(0, m_spriteSheetExportGroupSpacing);
    m_exportDialogCustomGroups.clear();
    if (m_spriteSheetExportUseCustomGroups)
    {
        // 仅在“分组模式”下解析分组，避免影响原有行/列/网格流程。
        std::string parseError;
        if (!buildResolvedSpriteGroups(m_exportDialogCustomGroups, parseError))
        {
            showError(parseError.empty() ? "Invalid custom sprite groups." : parseError);
            return;
        }
    }

    static const SDL_DialogFileFilter filters[] = {
        {"PNG Image", "png"},
        {"All Files", "*"}
    };

    std::string baseName = "export";
    const Project* project = m_activeContext->getProject();
    if (project && !project->getName().empty()) baseName = project->getName();

    std::string defaultPath;
    if (kind == ExportKind::CurrentFramePng)
    {
        defaultPath = baseName + "_frame_"
            + std::to_string(m_activeContext->getCurrentFrameIndex() + 1) + ".png";
    }
    else
    {
        // 根据配置模式拼接默认导出文件名，方便用户区分不同布局。
        const char* modeText = "row";
        if (m_spriteSheetExportUseCustomGroups) modeText = "grouped";
        else if (m_spriteSheetExportMode == SpriteSheetExportMode::Column)
            modeText = "column";
        else if (m_spriteSheetExportMode == SpriteSheetExportMode::RowColumn)
            modeText = "rowcolumn";

        const char* scopeText = m_spriteSheetExportUseCustomGroups
            ? "custom"
            : (m_spriteSheetExportUseSelectedFrames ? "selected" : "all");
        defaultPath = baseName + "_spritesheet_" + modeText + "_" + scopeText + ".png";
    }

    m_exportDialogInFlight = true;
    SDL_ShowSaveFileDialog(
        &App::onExportDialogClosed,
        this,
        m_window,
        filters,
        2,
        defaultPath.c_str());
}

/**
 * @brief 打开导入 PNG 文件选择弹窗。
 *
 * 这里只负责弹出系统文件选择器并记录导入类型；
 * 用户选中文件后由 onImportDialogClosed() 写入待处理结果。
 */
void App::requestImportDialog(ImportKind kind)
{
    if (m_importDialogInFlight) return;
    if (!m_activeContext || !m_activeContext->hasProject())
    {
        showError("No active project to import into.");
        return;
    }

    m_importDialogKind = kind;
    m_importDialogSpriteSheetRowMajor = m_spriteSheetImportRowMajor;
    static const SDL_DialogFileFilter filters[] = {
        {"PNG Image", "png"},
        {"All Files", "*"}
    };

    m_importDialogInFlight = true;
    SDL_ShowOpenFileDialog(
        &App::onImportDialogClosed,
        this,
        m_window,
        filters,
        2,
        nullptr,
        false);
}

/**
 * @brief SDL 导出文件弹窗关闭回调。
 *
 * 该回调可能不在主渲染流程中执行，因此只把路径和导出配置写入 pending 状态；
 * 实际导出留给主线程中的 pollDialogResults() 处理，避免 UI/项目状态竞争。
 */
void SDLCALL App::onExportDialogClosed(void* userdata, const char* const* filelist, int filter)
{
    (void)filter;
    App* app = static_cast<App*>(userdata);
    if (!app) return;

    std::lock_guard<std::mutex> guard(app->m_dialogMutex);
    app->m_exportDialogInFlight = false;

    if (!filelist)
    {
        app->m_pendingDialogError = SDL_GetError();
        if (app->m_pendingDialogError.empty()) app->m_pendingDialogError = "Export dialog failed.";
        app->m_pendingDialogErrorReady = true;
        return;
    }

    if (!filelist[0]) return;

    app->m_pendingExportPath = filelist[0];
    app->m_pendingExportKind = app->m_exportDialogKind;
    app->m_pendingExportSpriteMode = app->m_exportDialogSpriteMode;
    app->m_pendingExportUseSelectedFrames = app->m_exportDialogUseSelectedFrames;
    app->m_pendingExportColumnsPerRow = app->m_exportDialogColumnsPerRow;
    app->m_pendingExportUseCustomGroups = app->m_exportDialogUseCustomGroups;
    app->m_pendingExportGroupSpacing = app->m_exportDialogGroupSpacing;
    app->m_pendingExportCustomGroups = app->m_exportDialogCustomGroups;
    app->m_pendingExportReady = true;
}

/**
 * @brief SDL 导入文件弹窗关闭回调。
 *
 * 只收集用户选择的路径和导入模式，不直接修改项目数据；
 * 这样可以把真实导入流程集中到主循环里执行。
 */
void SDLCALL App::onImportDialogClosed(void* userdata, const char* const* filelist, int filter)
{
    (void)filter;
    App* app = static_cast<App*>(userdata);
    if (!app) return;

    std::lock_guard<std::mutex> guard(app->m_dialogMutex);
    app->m_importDialogInFlight = false;

    if (!filelist)
    {
        app->m_pendingDialogError = SDL_GetError();
        if (app->m_pendingDialogError.empty()) app->m_pendingDialogError = "Import dialog failed.";
        app->m_pendingDialogErrorReady = true;
        return;
    }

    if (!filelist[0]) return;

    app->m_pendingImportPath = filelist[0];
    app->m_pendingImportKind = app->m_importDialogKind;
    app->m_pendingImportSpriteSheetRowMajor = app->m_importDialogSpriteSheetRowMajor;
    app->m_pendingImportReady = true;
}

/**
 * @brief 将当前项目导出到指定 PNG 路径。
 *
 * 支持三类导出：
 * - 当前帧 PNG；
 * - 普通精灵表（行、列、行列网格）；
 * - 基于时间轴分组的自定义分组精灵表。
 */
bool App::exportToPath(const std::string& path,
                       ExportKind kind,
                       SpriteSheetExportMode spriteMode,
                       bool useSelectedFrames,
                       int columnsPerRow,
                       bool useCustomGroups,
                       const std::vector<SpriteSheetGroupResolved>& customGroups,
                       int groupSpacing)
{
    if (!m_activeContext || !m_activeContext->hasProject())
    {
        showError("No active project to export.");
        return false;
    }

    Project* project = m_activeContext->getProject();
    if (!project)
    {
        showError("No project data to export.");
        return false;
    }

    const std::string finalPath = normalizePngPath(path);
    std::string error;

    if (kind == ExportKind::CurrentFramePng)
    {
        if (!ImageExporter::exportSingleFramePng(*project, m_activeContext->getCurrentFrameIndex(), finalPath, &error))
        {
            showError(error.empty() ? "Failed to export current frame." : error);
            return false;
        }
        return true;
    }

    // 分组模式优先：每组可独立行排/列排，再进行组级拼接。
    if (useCustomGroups)
    {
        std::vector<ImageExporter::SpriteGroup> groups;
        groups.reserve(customGroups.size());
        for (const SpriteSheetGroupResolved& customGroup : customGroups)
        {
            ImageExporter::SpriteGroup group;
            group.name = customGroup.name;
            group.frameIndices = customGroup.frameIndices;
            group.layout = customGroup.mode == SpriteSheetExportMode::Column
                ? ImageExporter::SpriteSheetLayout::Column
                : ImageExporter::SpriteSheetLayout::Row;
            groups.push_back(std::move(group));
        }

        if (!ImageExporter::exportGroupedSpriteSheetPng(
                *project,
                groups,
                std::max(0, groupSpacing),
                finalPath,
                &error))
        {
            showError(error.empty() ? "Failed to export grouped sprite sheet." : error);
            return false;
        }
        return true;
    }

    // 传统模式：行 / 列 / 行列网格。
    ImageExporter::SpriteSheetLayout layout = ImageExporter::SpriteSheetLayout::Row;
    if (spriteMode == SpriteSheetExportMode::Column) layout = ImageExporter::SpriteSheetLayout::Column;
    else if (spriteMode == SpriteSheetExportMode::RowColumn)
        layout = ImageExporter::SpriteSheetLayout::RowColumn;

    std::vector<int> frameIndices;
    if (useSelectedFrames)
    {
        frameIndices = m_activeContext->getSelectedFrameIndices();
        if (frameIndices.empty())
        {
            showError("No selected frames to export.");
            return false;
        }
    }

    if (!ImageExporter::exportSpriteSheetPng(*project,
                                             frameIndices,
                                             layout,
                                             std::max(1, columnsPerRow),
                                             finalPath,
                                             &error))
    {
        showError(error.empty() ? "Failed to export sprite sheet." : error);
        return false;
    }
    return true;
}

/**
 * @brief 从 PNG 路径导入内容到当前项目。
 *
 * 单帧 PNG 会直接导入当前帧；精灵图 PNG 会先记录预览信息并打开配置弹窗，
 * 让用户选择切片尺寸、导入策略和自动分组方式。
 */
bool App::importFromPath(const std::string& path, ImportKind kind, bool spriteSheetRowMajor)
{
    if (!m_activeContext || !m_activeContext->hasProject())
    {
        showError("No active project to import into.");
        return false;
    }

    Project* project = m_activeContext->getProject();
    if (!project)
    {
        showError("No project data to import into.");
        return false;
    }

    if (path.empty())
    {
        showError("Import path is empty.");
        return false;
    }

    if (kind == ImportKind::CurrentFramePng)
    {
        // 单帧导入：直接写入当前主帧，尺寸必须与画布一致。
        std::string error;
        const int targetFrame = m_activeContext->getCurrentFrameIndex();
        if (!ImageImporter::importSingleFramePng(*project, targetFrame, path, &error))
        {
            showError(error.empty() ? "Failed to import current frame." : error);
            return false;
        }
        m_activeContext->setProjectDirty(true, "Import Frame");
        return true;
    }

    // 精灵图导入先进入配置弹窗，允许用户配置切片尺寸、导入策略、自动分组。
    m_spriteSheetImportPendingPath = path;
    m_spriteSheetImportRowMajor = spriteSheetRowMajor;
    m_spriteSheetImportUseGridCountMode = false;
    m_spriteSheetImportUseCustomSlice = false;
    m_spriteSheetImportStrategy = SpriteSheetImportStrategy::AppendAfterCurrent;
    m_spriteSheetImportGrouping = SpriteSheetImportGrouping::None;
    m_spriteSheetImportPreviewWidth = 0;
    m_spriteSheetImportPreviewHeight = 0;
    m_spriteSheetImportPreviewColumns = 0;
    m_spriteSheetImportPreviewRows = 0;
    m_spriteSheetImportPreviewFrames = 0;

    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface)
    {
        showError(std::string("Failed to load sprite sheet: ") + SDL_GetError());
        return false;
    }
    m_spriteSheetImportPreviewWidth = surface->w;
    m_spriteSheetImportPreviewHeight = surface->h;
    SDL_DestroySurface(surface);

    // 载入整张精灵图纹理，用于弹窗中的切片缩图预览。
    // 每次重新选择导入文件时都会重建纹理，避免旧图残留。
    render::deleteTexture(m_spriteSheetImportPreviewTexture);
    m_spriteSheetImportPreviewTexture = render::loadTextureFromFile(path.c_str());
    m_spriteSheetImportTileSelected.clear();

    // 默认切片尺寸跟随当前画布尺寸，用户可在弹窗中改为自定义值。
    m_spriteSheetImportSliceWidth = std::max(1, project->getWidth());
    m_spriteSheetImportSliceHeight = std::max(1, project->getHeight());
    // 默认行列数按“整图 / 画布”估算；若不能整除则回退为 1x1。
    m_spriteSheetImportGridCols = 1;
    m_spriteSheetImportGridRows = 1;
    if (m_spriteSheetImportPreviewWidth % m_spriteSheetImportSliceWidth == 0) m_spriteSheetImportGridCols = std::max(1, m_spriteSheetImportPreviewWidth / m_spriteSheetImportSliceWidth);
    if (m_spriteSheetImportPreviewHeight % m_spriteSheetImportSliceHeight == 0) m_spriteSheetImportGridRows = std::max(1, m_spriteSheetImportPreviewHeight / m_spriteSheetImportSliceHeight);

    if (m_spriteSheetImportPreviewWidth % m_spriteSheetImportSliceWidth == 0
        && m_spriteSheetImportPreviewHeight % m_spriteSheetImportSliceHeight == 0)
    {
        m_spriteSheetImportPreviewColumns = m_spriteSheetImportPreviewWidth / m_spriteSheetImportSliceWidth;
        m_spriteSheetImportPreviewRows = m_spriteSheetImportPreviewHeight / m_spriteSheetImportSliceHeight;
        m_spriteSheetImportPreviewFrames = m_spriteSheetImportPreviewColumns * m_spriteSheetImportPreviewRows;
    }

    m_spriteSheetImportPopupRequested = true;
    return true;
}

/**
 * @brief 渲染精灵图导入配置弹窗。
 *
 * 弹窗中可以设置遍历顺序、切片尺寸/行列数、导入策略、自动分组，
 * 并通过缩略图选择具体要导入的切片。
 */
void App::renderSpriteSheetImportPopup()
{
    if (m_spriteSheetImportPopupRequested)
    {
        ImGui::OpenPopup("Import Sprite Sheet");
        m_spriteSheetImportPopupRequested = false;
    }

    // 让弹窗尺寸可随窗口空间自适应：
    // - 出现时给一个较舒适的初始尺寸；
    // - 同时设置最小/最大尺寸约束，避免过小挤压或过大超出可视区域。
    const ImVec2 workSize = ImGui::GetMainViewport()->WorkSize;
    const ImVec2 minPopupSize(620.0f, 500.0f);
    const ImVec2 maxPopupSize(std::max(minPopupSize.x, workSize.x * 0.95f),
                              std::max(minPopupSize.y, workSize.y * 0.95f));
    const ImVec2 initialPopupSize(std::min(maxPopupSize.x, 860.0f),
                                  std::min(maxPopupSize.y, 760.0f));
    ImGui::SetNextWindowSizeConstraints(minPopupSize, maxPopupSize);
    ImGui::SetNextWindowSize(initialPopupSize, ImGuiCond_Appearing);

    if (!ImGui::BeginPopupModal("Import Sprite Sheet", nullptr, ImGuiWindowFlags_None)) return;

    ImGui::TextUnformatted("Traversal Order");
    int order = m_spriteSheetImportRowMajor ? 0 : 1;
    ImGui::RadioButton("Row-major (Left->Right, Top->Bottom)", &order, 0);
    ImGui::RadioButton("Column-major (Top->Bottom, Left->Right)", &order, 1);
    m_spriteSheetImportRowMajor = (order == 0);

    ImGui::Separator();
    ImGui::TextUnformatted("Slice Size");
    // 切片配置提供两种方式：
    // 按“每帧尺寸”输入（Width/Height）
    // 按“行列数量”输入（Rows/Columns），自动计算每帧尺寸
    int sliceMode = m_spriteSheetImportUseGridCountMode ? 1 : 0;
    ImGui::RadioButton("By Frame Size", &sliceMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("By Rows / Columns", &sliceMode, 1);
    m_spriteSheetImportUseGridCountMode = (sliceMode == 1);

    if (!m_spriteSheetImportUseGridCountMode)
    {
        ImGui::Checkbox("Use Custom Slice Size", &m_spriteSheetImportUseCustomSlice);
        if (m_spriteSheetImportUseCustomSlice)
        {
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputInt("Slice Width", &m_spriteSheetImportSliceWidth);
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputInt("Slice Height", &m_spriteSheetImportSliceHeight);
            m_spriteSheetImportSliceWidth = std::max(1, m_spriteSheetImportSliceWidth);
            m_spriteSheetImportSliceHeight = std::max(1, m_spriteSheetImportSliceHeight);
        }
        else if (m_activeContext && m_activeContext->hasProject())
        {
            Project* project = m_activeContext->getProject();
            m_spriteSheetImportSliceWidth = std::max(1, project->getWidth());
            m_spriteSheetImportSliceHeight = std::max(1, project->getHeight());
        }
    }
    else
    {
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("Rows", &m_spriteSheetImportGridRows);
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("Columns", &m_spriteSheetImportGridCols);
        m_spriteSheetImportGridRows = std::max(1, m_spriteSheetImportGridRows);
        m_spriteSheetImportGridCols = std::max(1, m_spriteSheetImportGridCols);
    }

    // 统一计算“本次将实际使用”的切片尺寸：
    // - 按尺寸模式：直接使用 Slice Width/Height
    // - 按行列模式：由整图尺寸 / 行列数自动推导
    int effectiveSliceWidth = m_spriteSheetImportSliceWidth;
    int effectiveSliceHeight = m_spriteSheetImportSliceHeight;
    bool effectiveSliceValid = false;
    if (m_spriteSheetImportUseGridCountMode)
    {
        if (m_spriteSheetImportGridCols > 0
            && m_spriteSheetImportGridRows > 0
            && m_spriteSheetImportPreviewWidth > 0
            && m_spriteSheetImportPreviewHeight > 0
            && m_spriteSheetImportPreviewWidth % m_spriteSheetImportGridCols == 0
            && m_spriteSheetImportPreviewHeight % m_spriteSheetImportGridRows == 0)
        {
            effectiveSliceWidth = m_spriteSheetImportPreviewWidth / m_spriteSheetImportGridCols;
            effectiveSliceHeight = m_spriteSheetImportPreviewHeight / m_spriteSheetImportGridRows;
            effectiveSliceValid = true;
        }
    }
    else
    {
        if (effectiveSliceWidth > 0
            && effectiveSliceHeight > 0
            && m_spriteSheetImportPreviewWidth > 0
            && m_spriteSheetImportPreviewHeight > 0
            && m_spriteSheetImportPreviewWidth % effectiveSliceWidth == 0
            && m_spriteSheetImportPreviewHeight % effectiveSliceHeight == 0)
        {
            effectiveSliceValid = true;
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Import Strategy");
    int strategy = static_cast<int>(m_spriteSheetImportStrategy);
    ImGui::RadioButton("Append After Current", &strategy, static_cast<int>(SpriteSheetImportStrategy::AppendAfterCurrent));
    ImGui::RadioButton("Replace All Frames", &strategy, static_cast<int>(SpriteSheetImportStrategy::ReplaceAllFrames));
    ImGui::RadioButton("Import As New Project", &strategy, static_cast<int>(SpriteSheetImportStrategy::NewProject));
    m_spriteSheetImportStrategy = static_cast<SpriteSheetImportStrategy>(strategy);

    ImGui::Separator();
    ImGui::TextUnformatted("Auto Group Imported Frames");
    int grouping = static_cast<int>(m_spriteSheetImportGrouping);
    ImGui::RadioButton("None", &grouping, static_cast<int>(SpriteSheetImportGrouping::None));
    ImGui::RadioButton("Group By Row", &grouping, static_cast<int>(SpriteSheetImportGrouping::ByRow));
    ImGui::RadioButton("Group By Column", &grouping, static_cast<int>(SpriteSheetImportGrouping::ByColumn));
    m_spriteSheetImportGrouping = static_cast<SpriteSheetImportGrouping>(grouping);

    ImGui::Separator();
    ImGui::Text("Sheet: %dx%d", m_spriteSheetImportPreviewWidth, m_spriteSheetImportPreviewHeight);
    ImGui::Text("Slice: %dx%d", effectiveSliceWidth, effectiveSliceHeight);
    if (effectiveSliceValid)
    {
        m_spriteSheetImportPreviewColumns = m_spriteSheetImportPreviewWidth / effectiveSliceWidth;
        m_spriteSheetImportPreviewRows = m_spriteSheetImportPreviewHeight / effectiveSliceHeight;
        m_spriteSheetImportPreviewFrames = m_spriteSheetImportPreviewColumns * m_spriteSheetImportPreviewRows;
        ImGui::Text("Detected Grid: %d x %d (frames=%d)",
                    m_spriteSheetImportPreviewColumns,
                    m_spriteSheetImportPreviewRows,
                    m_spriteSheetImportPreviewFrames);
    }
    else
    {
        ImGui::TextUnformatted("Detected Grid: invalid (sheet size not divisible by current slice config)");
    }

    // ---------------- 切片缩图预览 + 选择性导入 ----------------
    // 规则：
    // 默认全选；
    // 点击缩图可切换该切片是否导入；
    // 提供全选/清空/反选快捷按钮。
    int selectedTileCount = 0;
    if (effectiveSliceValid && m_spriteSheetImportPreviewFrames > 0)
    {
        if (m_spriteSheetImportTileSelected.size() != static_cast<size_t>(m_spriteSheetImportPreviewFrames))
        {
            m_spriteSheetImportTileSelected.assign(static_cast<size_t>(m_spriteSheetImportPreviewFrames), 1);
        }

        for (uint8_t flag : m_spriteSheetImportTileSelected)
        {
            if (flag != 0) ++selectedTileCount;
        }

        ImGui::Separator();
        ImGui::Text("Selectable Tiles: %d / %d", selectedTileCount, m_spriteSheetImportPreviewFrames);
        if (ImGui::Button("Select All"))
        {
            std::fill(m_spriteSheetImportTileSelected.begin(), m_spriteSheetImportTileSelected.end(), static_cast<uint8_t>(1));
            selectedTileCount = m_spriteSheetImportPreviewFrames;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear"))
        {
            std::fill(m_spriteSheetImportTileSelected.begin(), m_spriteSheetImportTileSelected.end(), static_cast<uint8_t>(0));
            selectedTileCount = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Invert"))
        {
            for (uint8_t& flag : m_spriteSheetImportTileSelected)
                flag = flag == 0 ? 1 : 0;
            selectedTileCount = 0;
            for (uint8_t flag : m_spriteSheetImportTileSelected)
            {
                if (flag != 0) ++selectedTileCount;
            }
        }

        // 预览区高度根据当前弹窗可用空间动态分配，避免窗口拉伸时布局僵硬。
        const float footerReserve = ImGui::GetFrameHeightWithSpacing() + 20.0f; // 预留 Import/Cancel 区域
        const float previewHeight = std::clamp(ImGui::GetContentRegionAvail().y - footerReserve, 160.0f, 420.0f);
        ImGui::BeginChild("##SpriteSheetTilePreview",
                          ImVec2(0.0f, previewHeight),
                          true,
                          ImGuiWindowFlags_HorizontalScrollbar);
        const float tilePreviewSize = 52.0f;
        const float cardWidth = 74.0f; // 卡片宽度（包含缩图与编号），用于统一对齐
        const float spacingX = ImGui::GetStyle().ItemSpacing.x;
        const float availWidth = ImGui::GetContentRegionAvail().x;
        int tilesPerRow = static_cast<int>((availWidth + spacingX) / (cardWidth + spacingX));
        tilesPerRow = std::max(1, tilesPerRow);

        // 预览布局策略：
        // - 按行列模式：严格按用户输入列数显示；
        // - 其它模式：按可用宽度自适应列数。
        if (m_spriteSheetImportUseGridCountMode && effectiveSliceValid) tilesPerRow = std::max(1, m_spriteSheetImportGridCols);

        tilesPerRow = std::min(tilesPerRow, m_spriteSheetImportPreviewFrames);

        if (ImGui::BeginTable("##SpriteSheetTileTable",
                              tilesPerRow,
                              ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingFixedFit))
        {
            for (int tileIndex = 0; tileIndex < m_spriteSheetImportPreviewFrames; ++tileIndex)
            {
                if (tileIndex % tilesPerRow == 0) ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(tileIndex % tilesPerRow);

                // tileIndex 为“预览顺序索引”，映射到源图中的行列用于 UV 裁剪。
                const int sourceRow = tileIndex / m_spriteSheetImportPreviewColumns;
                const int sourceCol = tileIndex % m_spriteSheetImportPreviewColumns;

                ImGui::PushID(tileIndex);
                const bool selected = m_spriteSheetImportTileSelected[static_cast<size_t>(tileIndex)] != 0;
                if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.75f, 0.90f));

                bool clicked = false;
                const float imageOffsetX = std::max(0.0f, (cardWidth - tilePreviewSize) * 0.5f);
                const float baseX = ImGui::GetCursorPosX();
                ImGui::SetCursorPosX(baseX + imageOffsetX);

                if (m_spriteSheetImportPreviewTexture != 0)
                {
                    const ImVec2 uv0(
                        static_cast<float>(sourceCol * effectiveSliceWidth) / static_cast<float>(m_spriteSheetImportPreviewWidth),
                        static_cast<float>(sourceRow * effectiveSliceHeight) / static_cast<float>(m_spriteSheetImportPreviewHeight));
                    const ImVec2 uv1(
                        static_cast<float>((sourceCol + 1) * effectiveSliceWidth) / static_cast<float>(m_spriteSheetImportPreviewWidth),
                        static_cast<float>((sourceRow + 1) * effectiveSliceHeight) / static_cast<float>(m_spriteSheetImportPreviewHeight));
                    clicked = ImGui::ImageButton("##tile",
                                                 reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_spriteSheetImportPreviewTexture)),
                                                 ImVec2(tilePreviewSize, tilePreviewSize),
                                                 uv0,
                                                 uv1);
                }
                else
                {
                    clicked = ImGui::Button("Tile", ImVec2(tilePreviewSize, tilePreviewSize));
                }

                if (selected) ImGui::PopStyleColor();

                if (clicked)
                {
                    uint8_t& flag = m_spriteSheetImportTileSelected[static_cast<size_t>(tileIndex)];
                    flag = flag == 0 ? 1 : 0;
                }

                char label[16] = {};
                std::snprintf(label, sizeof(label), "#%d", tileIndex + 1);
                const float labelWidth = ImGui::CalcTextSize(label).x;
                ImGui::SetCursorPosX(baseX + std::max(0.0f, (cardWidth - labelWidth) * 0.5f));
                ImGui::TextUnformatted(label);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();

        // 缩图点击后重新统计一次，保证下方 Import 校验读到最新值。
        selectedTileCount = 0;
        for (uint8_t flag : m_spriteSheetImportTileSelected)
        {
            if (flag != 0) ++selectedTileCount;
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Import", ImVec2(120.0f, 0.0f)))
    {
        if (!effectiveSliceValid)
        {
            showError("Invalid slice config. Please check slice size or rows/columns.");
            ImGui::EndPopup();
            return;
        }
        if (m_spriteSheetImportPreviewFrames > 0 && selectedTileCount <= 0)
        {
            showError("No tiles selected to import.");
            ImGui::EndPopup();
            return;
        }

        if (!m_activeContext || !m_activeContext->hasProject())
        {
            showError("No active project to import into.");
        }
        else
        {
            Project* project = m_activeContext->getProject();
            std::string error;
            ImageImporter::SpriteSheetSliceResult sliceResult;
            if (!ImageImporter::sliceSpriteSheetPng(m_spriteSheetImportPendingPath,
                                                    effectiveSliceWidth,
                                                    effectiveSliceHeight,
                                                    m_spriteSheetImportRowMajor,
                                                    sliceResult,
                                                    &error))
            {
                showError(error.empty() ? "Failed to import sprite sheet." : error);
            }
            else
            {
                // 先根据“切片选择状态”过滤导入帧。
                // filteredIndexMap 记录“过滤后帧”对应的原始切片索引，后续分组需要用到。
                std::vector<std::vector<uint32_t>> filteredFrames;
                std::vector<int> filteredTileRows;
                std::vector<int> filteredTileCols;
                filteredFrames.reserve(sliceResult.frames.size());
                filteredTileRows.reserve(sliceResult.frames.size());
                filteredTileCols.reserve(sliceResult.frames.size());

                for (size_t i = 0; i < sliceResult.frames.size(); ++i)
                {
                    // 选择状态以“物理网格位置（row,col）”为准，而非切片遍历顺序。
                    // 这样在 column-major 模式下，预览勾选与实际导入也能一一对应。
                    const int physicalIndex = sliceResult.tileRows[i] * m_spriteSheetImportPreviewColumns
                        + sliceResult.tileCols[i];
                    const bool selected = physicalIndex >= 0
                        && physicalIndex < static_cast<int>(m_spriteSheetImportTileSelected.size())
                        ? m_spriteSheetImportTileSelected[static_cast<size_t>(physicalIndex)] != 0
                        : true;
                    if (!selected) continue;

                    filteredFrames.push_back(sliceResult.frames[i]);
                    filteredTileRows.push_back(sliceResult.tileRows[i]);
                    filteredTileCols.push_back(sliceResult.tileCols[i]);
                }

                if (filteredFrames.empty())
                {
                    showError("No tiles selected to import.");
                    m_spriteSheetImportPendingPath.clear();
                    ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                    return;
                }

                AppContext* targetContext = m_activeContext;
                Project* targetProject = project;
                int firstImportedIndex = 0;

                // 策略 1：追加到当前帧后。
                if (m_spriteSheetImportStrategy == SpriteSheetImportStrategy::AppendAfterCurrent)
                {
                    const int insertAfter = targetContext->getCurrentFrameIndex();
                    int anchor = insertAfter;
                    for (size_t i = 0; i < filteredFrames.size(); ++i)
                    {
                        targetProject->insertFrameAfter(anchor, 0x00000000);
                        ++anchor;
                        targetProject->getFrame(anchor).pixels = filteredFrames[i];
                        targetContext->onFrameInserted(anchor, -1, targetProject->getFrameCount());
                    }
                    firstImportedIndex = insertAfter + 1;
                }

                // 策略 2：替换当前项目全部帧（并把画布尺寸切到导入切片尺寸）。
                if (m_spriteSheetImportStrategy == SpriteSheetImportStrategy::ReplaceAllFrames)
                {
                    targetProject->resizeCanvas(effectiveSliceWidth, effectiveSliceHeight, 0x00000000);
                    targetProject->setFrameCount(static_cast<int>(filteredFrames.size()), 0x00000000);
                    for (size_t i = 0; i < filteredFrames.size(); ++i)
                        targetProject->getFrame(static_cast<int>(i)).pixels = filteredFrames[i];
                    targetContext->clearFrameGroups();
                    firstImportedIndex = 0;
                }

                // 策略 3：导入为新项目。
                if (m_spriteSheetImportStrategy == SpriteSheetImportStrategy::NewProject)
                {
                    std::unique_ptr<Project> newProject = std::make_unique<Project>(
                        effectiveSliceWidth,
                        effectiveSliceHeight,
                        static_cast<int>(filteredFrames.size()),
                        0x00000000);
                    newProject->setName(projectNameFromPath(m_spriteSheetImportPendingPath));
                    for (size_t i = 0; i < filteredFrames.size(); ++i)
                        newProject->getFrame(static_cast<int>(i)).pixels = filteredFrames[i];

                    createSessionFromProject(std::move(newProject), "");
                    targetContext = m_activeContext;
                    targetProject = targetContext ? targetContext->getProject() : nullptr;
                    firstImportedIndex = 0;
                }

                if (!targetContext || !targetProject)
                {
                    showError("Failed to apply imported frames.");
                    m_spriteSheetImportPendingPath.clear();
                    ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                    return;
                }

                // 自动分组（可选）：
                // - ByRow：同一切片行建一个分组
                // - ByColumn：同一切片列建一个分组
                if (m_spriteSheetImportGrouping != SpriteSheetImportGrouping::None)
                {
                    targetContext->clearFrameGroups();

                    if (m_spriteSheetImportGrouping == SpriteSheetImportGrouping::ByRow)
                    {
                        for (int row = 0; row < sliceResult.rows; ++row)
                        {
                            std::vector<int> groupFrames;
                            for (size_t i = 0; i < filteredFrames.size(); ++i)
                            {
                                if (filteredTileRows[i] == row) groupFrames.push_back(firstImportedIndex + static_cast<int>(i));
                            }
                            if (!groupFrames.empty())
                            {
                                // 每个组按序号分配不同颜色，保证时间轴上肉眼可区分。
                                const uint32_t groupColor = importGroupColorByIndex(static_cast<size_t>(row));
                                targetContext->addFrameGroup("Row " + std::to_string(row + 1),
                                                             groupFrames,
                                                             targetProject->getFrameCount(),
                                                             groupColor);
                            }
                        }
                    }
                    else
                    {
                        for (int col = 0; col < sliceResult.columns; ++col)
                        {
                            std::vector<int> groupFrames;
                            for (size_t i = 0; i < filteredFrames.size(); ++i)
                            {
                                if (filteredTileCols[i] == col) groupFrames.push_back(firstImportedIndex + static_cast<int>(i));
                            }
                            if (!groupFrames.empty())
                            {
                                // 列分组同样按列号映射颜色，避免“所有组同色”。
                                const uint32_t groupColor = importGroupColorByIndex(static_cast<size_t>(col));
                                targetContext->addFrameGroup("Column " + std::to_string(col + 1),
                                                             groupFrames,
                                                             targetProject->getFrameCount(),
                                                             groupColor);
                            }
                        }
                    }
                }

                targetContext->setSingleFrameSelection(firstImportedIndex, targetProject->getFrameCount());
                targetContext->setProjectDirty(true, "Import Sprite Sheet");
                m_spriteSheetImportPendingPath.clear();
                ImGui::CloseCurrentPopup();
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
    {
        m_spriteSheetImportPendingPath.clear();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

/**
 * @brief 解析用户输入的帧列表文本。
 *
 * 支持单帧、区间和混合输入，例如 "1,3-5,8"；
 * 输出统一转换为 0-based 帧索引，并自动去重。
 */
bool App::parseFrameListText(const std::string& text,
                             int maxFrameCount,
                             std::vector<int>& outIndices,
                             std::string& outError) const
{
    outIndices.clear();
    outError.clear();

    if (maxFrameCount <= 0)
    {
        outError = "Project has no frames.";
        return false;
    }

    // 支持输入格式：
    // 单值：1, 5, 12
    // 区间：3-7（闭区间）
    // 混合：1,2,5-8,10
    // 分隔符支持逗号/空格/分号，统一先归一化为逗号再解析。
    std::string normalized = text;
    for (char& ch : normalized)
    {
        if (ch == ' ' || ch == ';' || ch == '\t' || ch == '\n' || ch == '\r') ch = ',';
    }

    std::vector<bool> used(static_cast<size_t>(maxFrameCount), false);
    std::stringstream ss(normalized);
    std::string token;
    while (std::getline(ss, token, ','))
    {
        if (token.empty()) continue;

        const size_t dashPos = token.find('-');
        if (dashPos == std::string::npos)
        {
            int value = 0;
            try
            {
                value = std::stoi(token);
            }
            catch (...)
            {
                outError = "Invalid frame token: " + token;
                return false;
            }

            if (value < 1 || value > maxFrameCount)
            {
                outError = "Frame index out of range (1-based): " + std::to_string(value);
                return false;
            }

            const int idx = value - 1;
            if (!used[static_cast<size_t>(idx)])
            {
                outIndices.push_back(idx);
                used[static_cast<size_t>(idx)] = true;
            }
            continue;
        }

        const std::string left = token.substr(0, dashPos);
        const std::string right = token.substr(dashPos + 1);
        int startValue = 0;
        int endValue = 0;
        try
        {
            startValue = std::stoi(left);
            endValue = std::stoi(right);
        }
        catch (...)
        {
            outError = "Invalid frame range token: " + token;
            return false;
        }

        if (startValue < 1 || endValue < 1 || startValue > maxFrameCount || endValue > maxFrameCount)
        {
            outError = "Frame range out of bounds (1-based): " + token;
            return false;
        }

        if (startValue > endValue) std::swap(startValue, endValue);

        for (int value = startValue; value <= endValue; ++value)
        {
            const int idx = value - 1;
            if (!used[static_cast<size_t>(idx)])
            {
                outIndices.push_back(idx);
                used[static_cast<size_t>(idx)] = true;
            }
        }
    }

    if (outIndices.empty())
    {
        outError = "Frame list is empty.";
        return false;
    }
    return true;
}

/**
 * @brief 从时间轴分组构建精灵图导出的分组数据。
 *
 * 自定义分组导出不再手动输入帧号，而是直接读取 AppContext 中的时间轴分组，
 * 并转换成 ImageExporter 可以使用的分组描述。
 */
bool App::buildResolvedSpriteGroups(std::vector<SpriteSheetGroupResolved>& outGroups, std::string& outError) const
{
    outGroups.clear();
    outError.clear();

    if (!m_activeContext || !m_activeContext->hasProject())
    {
        outError = "No active project to export.";
        return false;
    }

    Project* project = m_activeContext->getProject();
    if (!project)
    {
        outError = "No project data to export.";
        return false;
    }

    const int frameCount = project->getFrameCount();
    if (frameCount <= 0)
    {
        outError = "Project has no frames.";
        return false;
    }

    // 自定义分组导出直接沿用“时间轴右键创建”的分组数据，
    // 不再要求用户在导出弹窗重复输入帧号。
    const std::vector<AppContext::FrameGroup>& groups = m_activeContext->getFrameGroups();
    if (groups.empty())
    {
        outError = "No frame groups. Please create groups in timeline first.";
        return false;
    }

    for (size_t i = 0; i < groups.size(); ++i)
    {
        const AppContext::FrameGroup& group = groups[i];
        if (group.frameIndices.empty()) continue;

        SpriteSheetGroupResolved resolved;
        resolved.name = group.name.empty() ? ("Group " + std::to_string(i + 1)) : group.name;
        resolved.mode = SpriteSheetExportMode::Row; // 当前分组导出先默认“组内按行排”
        resolved.frameIndices = group.frameIndices;
        outGroups.push_back(std::move(resolved));
    }

    if (outGroups.empty())
    {
        outError = "No valid frame groups to export.";
        return false;
    }

    return true;
}

/**
 * @brief 渲染精灵图导出配置弹窗。
 *
 * 提供行、列、行列网格和时间轴分组四种导出模式；
 * 用户确认后会继续打开系统保存弹窗选择最终 PNG 路径。
 */
void App::renderSpriteSheetExportPopup()
{
    // 菜单点击后只设置请求标志，真正 OpenPopup 放在渲染帧中执行。
    if (m_spriteSheetExportPopupRequested)
    {
        ImGui::OpenPopup("Export Sprite Sheet");
        m_spriteSheetExportPopupRequested = false;
    }

    if (!ImGui::BeginPopupModal("Export Sprite Sheet", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

    // 首次打开时加载模式图标（row/column/row&column）。
    if (!m_spriteSheetExportIconsLoaded)
    {
        const char* rowCandidates[] = {"src/assets/row.png", "../src/assets/row.png", "../../src/assets/row.png"};
        const char* colCandidates[] = {"src/assets/column.png", "../src/assets/column.png", "../../src/assets/column.png"};
        const char* rowColCandidates[] = {"src/assets/row&column.png", "../src/assets/row&column.png", "../../src/assets/row&column.png"};

        for (const char* p : rowCandidates)
        {
            m_spriteSheetRowIconTexture = render::loadTextureFromFile(p);
            if (m_spriteSheetRowIconTexture != 0) break;
        }
        for (const char* p : colCandidates)
        {
            m_spriteSheetColumnIconTexture = render::loadTextureFromFile(p);
            if (m_spriteSheetColumnIconTexture != 0) break;
        }
        for (const char* p : rowColCandidates)
        {
            m_spriteSheetRowColumnIconTexture = render::loadTextureFromFile(p);
            if (m_spriteSheetRowColumnIconTexture != 0) break;
        }
        m_spriteSheetExportIconsLoaded = true;
    }

    ImGui::TextUnformatted("Select sprite sheet mode");
    ImGui::Separator();

    auto drawModeButton = [this](SpriteSheetExportMode mode, unsigned int texture, const char* fallbackLabel) {
        const bool selected = (!m_spriteSheetExportUseCustomGroups && m_spriteSheetExportMode == mode);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.75f, 0.90f));

        bool clicked = false;
        if (texture != 0)
        {
            clicked = ImGui::ImageButton(
                fallbackLabel,
                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(texture)),
                ImVec2(44.0f, 44.0f));
        }
        else
        {
            clicked = ImGui::Button(fallbackLabel, ImVec2(80.0f, 44.0f));
        }

        if (selected) ImGui::PopStyleColor();

        if (clicked)
        {
            m_spriteSheetExportUseCustomGroups = false;
            m_spriteSheetExportMode = mode;
        }
    };

    drawModeButton(SpriteSheetExportMode::Row, m_spriteSheetRowIconTexture, "Row");
    ImGui::SameLine();
    drawModeButton(SpriteSheetExportMode::Column, m_spriteSheetColumnIconTexture, "Column");
    ImGui::SameLine();
    drawModeButton(SpriteSheetExportMode::RowColumn, m_spriteSheetRowColumnIconTexture, "Row+Column");

    ImGui::SameLine();
    // 第四种逻辑模式：分组自定义（不使用图标，使用文本按钮避免增加资源依赖）。
    //
    // 关键修复说明：
    // - 这里必须用“点击前状态”控制 Push/Pop 是否配对；
    // - 不能在 Button 点击后再用 m_spriteSheetExportUseCustomGroups 判断 Pop，
    //   否则当按钮把 false 改成 true 时，会出现“未 Push 却 Pop”的栈失衡，
    //   进而触发 ImGui 的 "Calling PopStyleColor() too many times!" 断言。
    const bool customModeSelectedBeforeClick = m_spriteSheetExportUseCustomGroups;
    if (customModeSelectedBeforeClick) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.75f, 0.90f));
    if (ImGui::Button("Custom Groups", ImVec2(130.0f, 44.0f))) m_spriteSheetExportUseCustomGroups = true;
    if (customModeSelectedBeforeClick) ImGui::PopStyleColor();

    ImGui::Separator();

    if (!m_spriteSheetExportUseCustomGroups)
    {
        // 传统模式配置（保持原逻辑）。
        ImGui::TextUnformatted("Export range");
        int range = m_spriteSheetExportUseSelectedFrames ? 1 : 0;
        ImGui::RadioButton("All Frames", &range, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Selected Frames", &range, 1);
        m_spriteSheetExportUseSelectedFrames = (range == 1);

        if (m_spriteSheetExportMode == SpriteSheetExportMode::RowColumn)
        {
            ImGui::TextUnformatted("Grid config");
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputInt("Columns Per Row", &m_spriteSheetExportColumnsPerRow);
            if (m_spriteSheetExportColumnsPerRow < 1) m_spriteSheetExportColumnsPerRow = 1;
        }

        // 预估输出尺寸，帮助用户在导出前确认布局结果。
        if (m_activeContext && m_activeContext->hasProject())
        {
            Project* project = m_activeContext->getProject();
            const int frameW = project->getWidth();
            const int frameH = project->getHeight();
            int frameCount = project->getFrameCount();
            if (m_spriteSheetExportUseSelectedFrames) frameCount = static_cast<int>(m_activeContext->getSelectedFrameIndices().size());

            frameCount = std::max(0, frameCount);
            int outW = 0;
            int outH = 0;
            if (frameCount > 0)
            {
                if (m_spriteSheetExportMode == SpriteSheetExportMode::Row)
                {
                    outW = frameW * frameCount;
                    outH = frameH;
                }
                else if (m_spriteSheetExportMode == SpriteSheetExportMode::Column)
                {
                    outW = frameW;
                    outH = frameH * frameCount;
                }
                else
                {
                    const int cols = std::min(frameCount, std::max(1, m_spriteSheetExportColumnsPerRow));
                    const int rows = (frameCount + cols - 1) / cols;
                    outW = frameW * cols;
                    outH = frameH * rows;
                }
            }
            ImGui::Text("Preview: frames=%d, output=%dx%d", frameCount, outW, outH);
        }
    }
    else
    {
        // 分组自定义模式配置（读取时间轴分组）：
        // - 不再手工输入帧号；
        // - 引导用户在时间轴中 Ctrl 多选 + 右键创建分组。
        ImGui::TextUnformatted("Custom group settings");
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("Group Spacing", &m_spriteSheetExportGroupSpacing);
        if (m_spriteSheetExportGroupSpacing < 0) m_spriteSheetExportGroupSpacing = 0;

        const std::vector<AppContext::FrameGroup>* groups = m_activeContext
            ? &m_activeContext->getFrameGroups()
            : nullptr;
        if (!groups || groups->empty())
        {
            ImGui::Separator();
            ImGui::TextUnformatted("No timeline groups found.");
            ImGui::TextUnformatted("Create groups in Timeline: Ctrl+Click frames -> Right Click -> Group Selected Frames...");
        }
        else
        {
            ImGui::Separator();
            ImGui::Text("Groups from timeline: %d", static_cast<int>(groups->size()));
            for (size_t i = 0; i < groups->size(); ++i)
            {
                const AppContext::FrameGroup& group = (*groups)[i];
                ImGui::BulletText("%s  (frames: %d)", group.name.c_str(), static_cast<int>(group.frameIndices.size()));
            }
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Export", ImVec2(120.0f, 0.0f)))
    {
        if (!m_spriteSheetExportUseCustomGroups
            && m_spriteSheetExportUseSelectedFrames
            && (!m_activeContext || m_activeContext->getSelectedFrameIndices().empty()))
        {
            showError("No selected frames to export.");
        }
        else if (m_spriteSheetExportUseCustomGroups)
        {
            // 先在弹窗内做一次解析校验，错误尽早反馈给用户。
            std::vector<SpriteSheetGroupResolved> resolvedGroups;
            std::string parseError;
            if (!buildResolvedSpriteGroups(resolvedGroups, parseError))
            {
                showError(parseError.empty() ? "Invalid custom groups." : parseError);
            }
            else
            {
                requestExportDialog(ExportKind::SpriteSheetConfiguredPng);
                ImGui::CloseCurrentPopup();
            }
        }
        else
        {
            requestExportDialog(ExportKind::SpriteSheetConfiguredPng);
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
    {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}


