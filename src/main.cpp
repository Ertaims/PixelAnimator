/**
 * @brief 程序入口：初始化 SDL/OpenGL/ImGui 由 App 负责，此处仅构造并运行 App。
 */
#include "app/App.h"
#include <cstdio>

int main(int, char**)
{
    App app;

    if (!app.init())
    {
        std::fprintf(stderr, "App::init() failed.\n");
        return 1;
    }

    app.run();
    app.shutdown();

    return 0;
}
