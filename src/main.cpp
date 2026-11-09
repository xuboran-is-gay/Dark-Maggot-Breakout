#include "game.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void usage(const char* argv0) {
    std::printf(
        "暗蛆突围 · DARK MAGGOT BREAKOUT\n"
        "用法: %s [选项]\n\n"
        "  --shot <场景> <输出.bmp>  无头渲染截图（hub/deploy/stash/missions/shop/\n"
        "                            shop_armor/shop_bag/shop_supply/settings/\n"
        "                            raid/map/pause/result）\n"
        "  --map <0-2>               截图或测试使用的封锁区\n"
        "  --seed <整数>             固定地图随机种子\n"
        "  --no-save                 不读写本地存档\n"
        "  --keydebug                在终端打印键盘事件（排查按键问题）\n"
        "  --help                    显示本帮助\n\n"
        "正常启动不需要参数。WASD 移动，鼠标瞄准，左键开火。\n", argv0);
}

int main(int argc, char** argv) {
    GameOptions opts;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (!std::strcmp(a, "--help") || !std::strcmp(a, "-h")) {
            usage(argv[0]);
            return 0;
        } else if (!std::strcmp(a, "--shot")) {
            if (i + 2 >= argc) {
                std::fprintf(stderr, "--shot 需要 <场景> <输出.bmp>\n");
                return 2;
            }
            opts.screenshot = true;
            opts.shot = argv[++i];
            opts.out = argv[++i];
        } else if (!std::strcmp(a, "--map")) {
            if (i + 1 >= argc) { std::fprintf(stderr, "--map 需要 0/1/2\n"); return 2; }
            opts.map = std::atoi(argv[++i]);
            if (opts.map < 0 || opts.map > 2) opts.map = 0;
        } else if (!std::strcmp(a, "--seed")) {
            if (i + 1 >= argc) { std::fprintf(stderr, "--seed 需要一个整数\n"); return 2; }
            opts.seed = std::strtoull(argv[++i], nullptr, 0);
        } else if (!std::strcmp(a, "--no-save")) {
            opts.noSave = true;
        } else if (!std::strcmp(a, "--keydebug")) {
            opts.keydebug = true;
        } else {
            std::fprintf(stderr, "未知参数: %s\n", a);
            usage(argv[0]);
            return 2;
        }
    }

    Game game;
    if (!game.init(opts)) {
        game.shutdown();
        return 1;
    }
    int rc = game.run();
    game.shutdown();
    return rc;
}
