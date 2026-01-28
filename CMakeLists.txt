cmake_minimum_required(VERSION 3.15)
project(PixelAnimator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# 链接库路径
link_directories(${CMAKE_SOURCE_DIR}/lib)

# 设置源文件目录
set(SOURCES 
    src/main.cpp
)

# 设置头文件目录
set(HEADERS
    include/common.h
)

# 添加ImGui源文件
set(IMGUI_SOURCES
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_impl_opengl3.cpp
    imgui/imgui_impl_sdl3.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
)


# 添加可执行文件
add_executable(${PROJECT_NAME} 
    ${SOURCES}
    ${HEADERS}
    ${IMGUI_SOURCES}
)

# 拷贝库文件 文件
file(COPY ${PROJECT_SOURCE_DIR}/lib/SDL3.dll DESTINATION ${PROJECT_BINARY_DIR})
file(COPY ${PROJECT_SOURCE_DIR}/lib/SDL3_image.dll DESTINATION ${PROJECT_BINARY_DIR})

# 设置头文件搜索路径
target_include_directories(${PROJECT_NAME} PRIVATE
    include
    imgui
)

# 链接库
target_link_libraries(${PROJECT_NAME}
    SDL3
    SDL3_image
    opengl32
    glew32
)

