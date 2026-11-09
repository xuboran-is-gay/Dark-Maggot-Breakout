#include "game.h"
#include <cstdio>

namespace {
std::string kgText(int grams) {
    int whole = grams / 1000;
    int frac = (grams % 1000) / 100;
    return std::to_string(whole) + "." + std::to_string(frac) + " kg";
}

std::string kindName(ItemKind k) {
    switch (k) {
        case IK_JUNK: return "废料";
        case IK_FOOD: return "食物";
        case IK_TECH: return "电子";
        case IK_ORGANIC: return "有机";
        case IK_TREASURE: return "珍藏";
        case IK_AMMO: return "弹药";
        case IK_MED: return "医疗";
    }
    return "未知";
}

int sellPrice(int id, int count) {
    if (id < 0 || id >= IT_COUNT) return 0;
    return ITEMS[id].value * count;
}
} // namespace

void Game::drawHub() {
    drawBackdrop(time_, C_ACCENT);
    drawTopBar(false, "", "");

    // ---- operator panel -------------------------------------------------
    ui_.panel({48, 96, 372, 682}, Col(12, 18, 11, 228), C_BORDER, 12);
    ui_.fillF(48, 96, 4, 682, C_ACCENT);
    visual_.drawGlow(renderer_, {130.f, 200.f}, 170.f, C_ACCENT, .10f);
    drawWorm(126.f, 196.f, 1.45f, time_, Col(178, 213, 112), Col(74, 98, 48));
    ui_.text("无名蛆", 200, 142, 26, C_TEXT);
    ui_.text("Lv." + std::to_string(profile_.level()) + "  ·  腐化新贵", 200, 176, 13, C_ACCENT);
    ui_.text("经验  " + std::to_string(profile_.xpIntoLevel()) + " / " + std::to_string(profile_.xpForNext()),
             200, 202, 11, C_MUTED);
    ui_.bar({200, 220, 190, 6}, (float)profile_.xpIntoLevel() / (float)profile_.xpForNext(),
            C_ACCENT, Col(8, 11, 8, 220), C_BORDER);

    ui_.text("当前配装", 72, 292, 12, C_MUTED);
    drawGearLine(72, 310, 324);
    ui_.text("备弹  " + std::to_string(profile_.ammo[0]) + " / " + std::to_string(profile_.ammo[1]) +
             " / " + std::to_string(profile_.ammo[2]), 72, 368, 11, C_MUTED);

    // ---- menu -----------------------------------------------------------
    struct Entry { const char* label; Screen to; bool primary; const char* note; };
    const Entry entries[] = {
        {"潜入行动", Screen::DEPLOY, true, "进入封锁区"},
        {"黑市交易", Screen::SHOP, false, "装备与补给"},
        {"蛆巢仓库", Screen::STASH, false, "战利品与销赃"},
        {"任务委托", Screen::MISSIONS, false, "领取悬赏"},
        {"设置", Screen::SETTINGS, false, "音量与存档"},
        {"退出", Screen::HUB, false, "回到黑暗"},
    };
    int y = 394;
    for (int i = 0; i < 6; ++i) {
        Rect r{72, (float)y, 324, 46};
        bool clicked = ui_.button(r, entries[i].label, true, entries[i].primary, 18);
        ui_.text(entries[i].note, r.right() - 14, r.y + 16, 10,
                 entries[i].primary ? C_INK.withA(150) : C_MUTED, ALIGN_RIGHT);
        if (clicked) {
            if (i == 5) {
                saveGame(true);
                running_ = false;
            } else {
                setScreen(entries[i].to);
            }
        }
        y += 55;
    }
    ui_.text("本地存档 · 自动保存", 72, 752, 10, Col(104, 116, 88));

    // ---- hero -----------------------------------------------------------
    Rect hero{452, 96, 780, 356};
    ui_.panel(hero, Col(11, 17, 10, 224), C_BORDER, 16);
    ui_.fillF(hero.x, hero.y, 5, hero.h, withAlpha(C_ACCENT, 200));
    for (int i = 0; i < 4; ++i) {
        ui_.ring({1000.f, 250.f}, 96.f + i * 28.f, withAlpha(C_ACCENT, 18), 1.f, 40);
    }
    ui_.text("封锁区 · 第 07 区", 486, 126, 12, C_ACCENT);
    ui_.text("在腐败里，", 486, 154, 44, C_TEXT);
    ui_.text("向上蠕动。", 486, 210, 44, C_ACCENT);
    ui_.line({486, 274}, {852, 274}, C_BORDER, 1.f);
    ui_.wrapped("你是一条被丢进封锁区的蛆。这里的每一枚硬币都沾着别人的体液，每一口吃的都可能要命。"
                "带得走的叫战利品，带不走的叫肥料。", 486, 290, 14, C_MUTED, 430, 3);
    ui_.text("“别怕，下面还有更深的。”", 486, 384, 13, C_ACCENT2);
    visual_.drawGlow(renderer_, {1004.f, 250.f}, 160.f, C_ACCENT, .10f);
    drawWorm(1004.f, 250.f, 2.3f, time_ * .9f, Col(166, 204, 102), Col(68, 92, 44));

    // ---- stats ----------------------------------------------------------
    drawStatCard({452, 472, 246, 104}, "成功撤离", std::to_string(profile_.extracted), C_ACCENT);
    drawStatCard({719, 472, 246, 104}, "累计击杀", std::to_string(profile_.kills), C_GOLD);
    drawStatCard({986, 472, 246, 104}, "仓库估值", formatMoney(profile_.stashValue()), C_CYAN);

    // ---- briefing -------------------------------------------------------
    Rect bp{452, 596, 780, 182};
    ui_.panel(bp, Col(12, 18, 11, 228), C_BORDER, 12);
    ui_.text("行动简报", 474, 614, 12, C_MUTED);
    const MapDef& md = MAPS[deployMap_];
    ui_.text(md.name, 474, 636, 30, C_TEXT);
    ui_.text(md.risk + std::string("  ·  ") + md.code, 474, 676, 12,
             deployMap_ == 2 ? C_DANGER : (deployMap_ == 1 ? C_GOLD : C_ACCENT));
    ui_.wrapped(md.desc, 474, 700, 12, C_MUTED, 470, 3);
    ui_.text("敌人 " + std::to_string(md.enemies) + "   ·   容器 " + std::to_string(md.containers) +
             "   ·   限时 " + formatTime((float)md.timer), 474, 750, 11, C_MUTED);

    int nextMission = -1;
    for (int i = 0; i < 5; ++i) {
        if (!profile_.missionClaimed[i] && profile_.missionProgress(i) < MISSIONS[i].target) {
            nextMission = i;
            break;
        }
    }
    ui_.line({966, 620}, {966, 756}, C_BORDER, 1.f);
    ui_.text("当前委托", 990, 616, 11, C_MUTED);
    if (nextMission >= 0) {
        ui_.text(MISSIONS[nextMission].name, 990, 638, 18, C_TEXT);
        float prog = (float)std::min(profile_.missionProgress(nextMission), MISSIONS[nextMission].target) /
                     (float)MISSIONS[nextMission].target;
        ui_.bar({990, 668, 214, 7}, prog, C_GOLD, Col(8, 11, 8, 220), C_BORDER);
        ui_.text(std::to_string(profile_.missionProgress(nextMission)) + " / " +
                 std::to_string(MISSIONS[nextMission].target), 1204, 682, 11, C_MUTED, ALIGN_RIGHT);
    } else {
        ui_.text("没有进行中的委托", 990, 640, 15, C_ACCENT);
    }
    if (ui_.button({990, 710, 214, 40}, "准备潜入", true, true, 16)) {
        setScreen(Screen::DEPLOY);
    }
}

void Game::drawDeploy() {
    drawBackdrop(time_, MAPS[deployMap_].accent);
    drawTopBar(true, "潜入准备", "选择封锁区 · 检查弹药 · 活着回来");

    const float cardY = 96.f, cardH = 208.f, cardW = 252.f;
    for (int i = 0; i < 3; ++i) {
        const MapDef& md = MAPS[i];
        Rect r{48.f + i * 268.f, cardY, cardW, cardH};
        bool unlocked = mapUnlocked(i);
        bool sel = i == deployMap_;
        bool hov = ui_.hovered(r);
        Col acc = i == 0 ? C_ACCENT : (i == 1 ? C_GOLD : C_DANGER);
        Col fill = sel ? Col(24, 34, 21, 245) : (hov && unlocked ? Col(19, 27, 17, 238) : Col(12, 18, 11, 226));
        ui_.chamfer(r, 8, fill);
        ui_.chamferOutline(r, 8, sel ? acc : (hov && unlocked ? C_ACCENT2 : C_BORDER), sel ? 2.f : 1.f);
        ui_.fillF(r.x, r.y, r.w, 4, acc);
        ui_.text(md.risk, r.x + 16, r.y + 18, 12, acc);
        ui_.text(md.name, r.x + 16, r.y + 38, 27, C_TEXT);
        ui_.text(md.code, r.x + 16, r.y + 72, 10, C_MUTED);
        ui_.wrapped(md.desc, r.x + 16, r.y + 96, 11, C_MUTED, r.w - 32, 2);
        ui_.line({r.x + 16, r.y + 142}, {r.right() - 16, r.y + 142}, C_BORDER, 1.f);
        ui_.text("敌人", r.x + 16, r.y + 152, 10, C_MUTED);
        ui_.text(std::to_string(md.enemies), r.x + 16, r.y + 166, 16, C_TEXT);
        ui_.text("容器", r.x + 76, r.y + 152, 10, C_MUTED);
        ui_.text(std::to_string(md.containers), r.x + 76, r.y + 166, 16, C_TEXT);
        ui_.text("限时", r.x + 136, r.y + 152, 10, C_MUTED);
        ui_.text(formatTime((float)md.timer), r.x + 136, r.y + 166, 14, C_TEXT);
        ui_.text("战备", r.x + 196, r.y + 152, 10, C_MUTED);
        ui_.text(md.gearReq == 0 ? "0" : formatMoney(md.gearReq), r.x + 196, r.y + 166, 13,
                 profile_.gearValue() >= md.gearReq ? C_ACCENT : C_DANGER);
        if (md.boss) {
            ui_.chamfer({r.right() - 62, r.y + 14, 46, 22}, 4, withAlpha(C_DANGER, 45));
            ui_.text("BOSS", r.right() - 39, r.y + 17, 10, C_DANGER, ALIGN_CENTER);
        }
        if (!unlocked) {
            ui_.fillF(r.x + 1, r.y + 4, r.w - 2, r.h - 5, Col(6, 8, 6, 205));
            ui_.text("锁 定", r.x + r.w * .5f, r.y + 64, 22, C_MUTED, ALIGN_CENTER);
            ui_.text("需成功撤离 " + std::to_string(md.unlockExtracts) + " 次", r.x + r.w * .5f,
                     r.y + 100, 11, C_MUTED, ALIGN_CENTER);
            ui_.text("当前 " + std::to_string(profile_.extracted) + " 次", r.x + r.w * .5f,
                     r.y + 120, 11, Col(106, 114, 94), ALIGN_CENTER);
        }
        if (unlocked && hov && input_.released && r.contains(input_.pressPos)) {
            deployMap_ = i;
            audio_.play(SFX_UI_CLICK, .55f, 1.1f);
        }
    }

    // ---- selected map detail -------------------------------------------
    const MapDef& md = MAPS[deployMap_];
    Col acc = deployMap_ == 0 ? C_ACCENT : (deployMap_ == 1 ? C_GOLD : C_DANGER);
    Rect det{48, 326, 788, 456};
    ui_.panel(det, Col(12, 18, 11, 230), C_BORDER, 12);
    ui_.fillF(det.x, det.y, 4, det.h, acc);
    ui_.text("目标区域", 72, 344, 11, C_MUTED);
    ui_.text(md.name, 72, 364, 38, C_TEXT);
    ui_.text(std::string(md.risk) + "  ·  " + md.code, 72, 414, 13, acc);
    int effectiveGear = knifeRun_ ? 0 : profile_.gearValue();
    bool gearOk = effectiveGear >= md.gearReq;
    ui_.text("入场战备", 600, 348, 11, C_MUTED);
    ui_.text(md.gearReq == 0 ? "0 · 无门槛" : formatMoney(md.gearReq), 812, 342, 22,
             gearOk ? C_ACCENT : C_DANGER, ALIGN_RIGHT);
    ui_.text(knifeRun_ ? "跑刀 · 有效战备 0" : "当前 " + formatMoney(profile_.gearValue()), 812, 372, 11,
             gearOk ? C_MUTED : C_DANGER, ALIGN_RIGHT);
    ui_.wrapped(md.desc, 72, 444, 14, C_MUTED, 720, 3);
    ui_.line({72, 502}, {812, 502}, C_BORDER, 1.f);
    ui_.text("撤离方式", 72, 522, 13, C_ACCENT);
    ui_.text("2 个随机撤离点 · 绿色信号烟", 72, 546, 12, C_MUTED);
    ui_.diamond({82, 580}, 5, C_ACCENT);
    ui_.text("进入区域后，地图与边缘指示器会标出撤离点", 96, 571, 12, C_MUTED);
    ui_.diamond({82, 604}, 5, C_ACCENT);
    ui_.text("在撤离点内长按 E 三秒，期间不能移动", 96, 595, 12, C_MUTED);
    ui_.diamond({82, 628}, 5, C_ACCENT);
    ui_.text(knifeRun_ ? "跑刀：不携带装备，死亡只失去本次战利品"
                       : "死亡会丢失武器、护甲、背包、弹药与本次战利品",
             96, 619, 12, knifeRun_ ? C_ACCENT.withA(210) : C_DANGER.withA(210));
    ui_.line({72, 660}, {812, 660}, C_BORDER, 1.f);
    ui_.text("行动建议", 72, 676, 12, C_GOLD);
    const char* tip = deployMap_ == 0
        ? "贴着墙走。拾荒蛆的枪法很差，但它们的数量不差。先搜医疗箱，再考虑打架。"
        : (deployMap_ == 1 ? "货架之间没有真正的掩体。听到多把枪声时，先退，再绕。值钱货多在冷库和收银台。"
                           : "蛆王在通风管附近。没有铁皮龟壳以上，别正面对枪。带足 5.56 和止痛针。");
    ui_.wrapped(tip, 72, 700, 13, C_TEXT, 720, 3);
    ui_.text("入场后：WASD 移动 · 长按左键连发 · V 切刀 · E 搜刮/撤离 · TAB 地图", 72, 752, 10, C_MUTED);

    // ---- loadout --------------------------------------------------------
    Rect lo{860, 96, 372, 686};
    ui_.panel(lo, Col(12, 18, 11, 232), C_BORDER, 12);
    ui_.fillF(lo.x, lo.y, 4, lo.h, C_ACCENT);
    ui_.text("本次配装", 882, 112, 22, C_TEXT);
    ui_.text(knifeRun_ ? "跑刀：装备留在家中" : "死亡将失去全部装备", 1212, 120, 10,
             knifeRun_ ? C_ACCENT : C_DANGER, ALIGN_RIGHT);

    const WeaponDef& wp = WEAPONS[profile_.weapon];
    ui_.text(wp.name, 882, 152, 21, C_TEXT);
    ui_.text(ammoName(wp.ammo), 1212, 158, 11, C_MUTED, ALIGN_RIGHT);
    ui_.text("伤害 " + std::to_string(wp.damage) + (wp.pellets > 1 ? "×" + std::to_string(wp.pellets) : "") +
             "   ·   射速 " + std::to_string((int)wp.rpm) + "   ·   弹匣 " + std::to_string(wp.mag),
             882, 180, 11, C_MUTED);
    int reserve = profile_.ammo[(int)wp.ammo];
    ui_.text("备弹", 882, 206, 11, C_MUTED);
    ui_.text(std::to_string(reserve) + " 发", 1212, 202, 18, reserve > 0 ? C_TEXT : C_DANGER, ALIGN_RIGHT);
    ui_.bar({882, 230, 330, 4}, clamp01(reserve / 120.f), reserve > 0 ? C_ACCENT : C_DANGER,
            Col(8, 11, 8, 220), C_BORDER);

    const ArmorDef& ar = ARMORS[profile_.armor];
    const BackpackDef& bp = BACKPACKS[std::clamp(profile_.backpack, 0, 3)];
    ui_.text("护甲", 882, 250, 11, C_MUTED);
    ui_.text(ar.name, 882, 266, 14, C_TEXT);
    ui_.text(ar.armor > 0 ? ("护甲 " + std::to_string(ar.armor)) : "无防护", 1212, 270, 10, C_MUTED, ALIGN_RIGHT);
    ui_.text("背包", 882, 292, 11, C_MUTED);
    ui_.text(bp.name, 882, 308, 14, C_TEXT);
    ui_.text(std::to_string(bp.capacity) + " kg", 1212, 312, 10, C_MUTED, ALIGN_RIGHT);

    ui_.line({882, 338}, {1212, 338}, C_BORDER, 1.f);
    ui_.text("弹药补给", 882, 352, 12, C_GOLD);
    for (int a = 0; a < AMMO_COUNT; ++a) {
        float y = 376.f + a * 34.f;
        Col ac = a == 0 ? C_GOLD : (a == 1 ? C_DANGER : C_ACCENT);
        ui_.text(ammoShort((AmmoId)a), 882, y + 6, 13, ac);
        ui_.text(std::to_string(profile_.ammo[a]), 1008, y + 4, 15, C_TEXT);
        std::string price = "¥" + std::to_string(ammoPrice((AmmoId)a));
        if (ui_.button({1074, y, 138, 28}, "+" + std::to_string(ammoBox((AmmoId)a)) + "  " + price,
                       profile_.coins >= ammoPrice((AmmoId)a), false, 12)) {
            buyAmmo((AmmoId)a);
        }
    }
    ui_.line({882, 492}, {1212, 492}, C_BORDER, 1.f);
    ui_.text("医疗补给", 882, 506, 12, C_CYAN);
    ui_.text("绷带", 882, 530, 13, C_MUTED);
    ui_.text("×" + std::to_string(profile_.bandages), 1008, 530, 13, C_TEXT);
    if (ui_.button({1074, 524, 138, 28}, "补 2  ¥120", profile_.coins >= 120, false, 12)) buyMeds(0);
    ui_.text("止痛针", 882, 564, 13, C_MUTED);
    ui_.text("×" + std::to_string(profile_.medkits), 1008, 564, 13, C_TEXT);
    if (ui_.button({1074, 558, 138, 28}, "补 1  ¥420", profile_.coins >= 420, false, 12)) buyMeds(1);

    std::string runLabel = knifeRun_ ? "跑刀模式：开 · 无枪/甲/包/药" : "跑刀模式：关 · 正常装备";
    if (ui_.chip({882, 590, 330, 26}, runLabel, knifeRun_, true, 11)) {
        knifeRun_ = !knifeRun_;
        audio_.play(SFX_UI_CLICK, .55f, 1.05f);
    }
    bool can = mapUnlocked(deployMap_) && (knifeRun_ || (profile_.weapon != WPN_NONE && reserve > 0)) && gearOk;
    if (ui_.button({882, 622, 330, 54}, "开始潜入", can, true, 22)) {
        beginRaid();
    }
    if (!can) {
        std::string why;
        if (!mapUnlocked(deployMap_)) why = "累计撤离 " + std::to_string(md.unlockExtracts) + " 次后解锁";
        else if (!gearOk) {
            why = knifeRun_ ? "跑刀有效战备 0，无法进入该区域"
                            : "战备不足，还差 " + formatMoney(std::max(0, md.gearReq - effectiveGear));
        }
        else if (!knifeRun_ && profile_.weapon == WPN_NONE) why = "先在黑市买一把武器";
        else if (!knifeRun_ && reserve <= 0) why = "至少携带一个弹匣的备弹";
        else why = "无法进入该区域";
        ui_.text(why, 1047, 684, 11, C_DANGER, ALIGN_CENTER);
    } else {
        ui_.text(knifeRun_ ? "跑刀进入：只带蛆牙短刀" : "进入后自动装填弹匣", 1047, 684, 11,
                 knifeRun_ ? C_GOLD : C_MUTED, ALIGN_CENTER);
    }
    ui_.text("撤离成功：战利品进入仓库并结算经验", 882, 718, 10, C_MUTED);
    ui_.text(knifeRun_ ? "撤离失败：家中装备不受影响" : "撤离失败：装备留在封锁区", 882, 734, 10, C_MUTED);
    ui_.text("放弃行动：保装备，丢战利品", 882, 750, 10, C_MUTED);
}

void Game::drawStash() {
    drawBackdrop(time_, C_CYAN);
    drawTopBar(true, "蛆巢仓库", "销赃 · 估价 · 清点你的收藏");

    Rect left{48, 96, 720, 682};
    ui_.panel(left, Col(12, 18, 11, 228), C_BORDER, 12);
    ui_.fillF(left.x, left.y, 4, left.h, C_CYAN);
    ui_.text("仓库", 72, 112, 26, C_TEXT);
    ui_.text("库存 " + std::to_string(profile_.stashCount()) + " 件", 760, 122, 12, C_MUTED, ALIGN_RIGHT);
    ui_.text("估值 " + formatMoney(profile_.stashValue()), 72, 148, 12, C_GOLD);
    ui_.text("重量 " + kgText(profile_.stashWeight()), 748, 148, 12, C_MUTED, ALIGN_RIGHT);
    ui_.line({72, 174}, {744, 174}, C_BORDER, 1.f);

    const int cols = 6, visibleRows = 5;
    int rows = (int)profile_.stash.size() / cols + ((int)profile_.stash.size() % cols ? 1 : 0);
    int maxScroll = std::max(0, rows - visibleRows);
    stashScroll_ = std::clamp(stashScroll_, 0, maxScroll);
    if (profile_.stash.empty()) {
        ui_.text("仓库里只有空气和霉味。", 408, 360, 18, C_MUTED, ALIGN_CENTER);
        ui_.text("带点东西回来吧。", 408, 390, 13, Col(102, 114, 92), ALIGN_CENTER);
    }
    for (int vis = 0; vis < visibleRows * cols; ++vis) {
        int idx = stashScroll_ * cols + vis;
        if (idx >= (int)profile_.stash.size()) break;
        int row = vis / cols, col = vis % cols;
        Rect cell{68.f + col * 112.f, 188.f + row * 104.f, 104.f, 96.f};
        const ItemStack& st = profile_.stash[(size_t)idx];
        if (st.id < 0 || st.id >= IT_COUNT) continue;
        const ItemDef& it = ITEMS[st.id];
        bool sel = stashSel_ == idx;
        bool hov = ui_.hovered(cell);
        ui_.chamfer(cell, 5, sel ? Col(32, 45, 28, 245) : (hov ? Col(23, 33, 21, 238) : Col(16, 24, 15, 230)));
        ui_.chamferOutline(cell, 5, sel ? C_ACCENT : (hov ? C_ACCENT2 : C_BORDER), sel ? 1.6f : 1.f);
        ui_.fillF(cell.x, cell.y + 5, 3, cell.h - 10, withAlpha(rarityColor(it.rarity), 220));
        ui_.text(it.name, cell.x + 11, cell.y + 12, 13, C_TEXT);
        ui_.text("¥" + std::to_string(it.value), cell.x + 11, cell.y + 62, 12, C_GOLD);
        ui_.text("×" + std::to_string(st.count), cell.right() - 10, cell.y + 12, 12, C_MUTED, ALIGN_RIGHT);
        ui_.text(kindName(it.kind), cell.x + 11, cell.y + 40, 10, withAlpha(it.color, 220));
        if (hov && input_.released && cell.contains(input_.pressPos)) {
            stashSel_ = idx;
            audio_.play(SFX_UI_CLICK, .5f, 1.15f);
        }
    }
    ui_.text("滚轮翻页", 72, 752, 10, C_MUTED);
    ui_.text(std::to_string(stashScroll_ + 1) + " / " + std::to_string(maxScroll + 1),
             744, 752, 10, C_MUTED, ALIGN_RIGHT);

    Rect right{788, 96, 444, 682};
    ui_.panel(right, Col(12, 18, 11, 232), C_BORDER, 12);
    ui_.fillF(right.x, right.y, 4, right.h, C_CYAN);
    ui_.text("物品详情", 812, 112, 20, C_TEXT);
    ui_.text("黑市会按标价收购", 1208, 120, 10, C_MUTED, ALIGN_RIGHT);
    ui_.line({812, 146}, {1208, 146}, C_BORDER, 1.f);

    if (stashSel_ >= 0 && stashSel_ < (int)profile_.stash.size()) {
        const ItemStack& st = profile_.stash[(size_t)stashSel_];
        const ItemDef& it = ITEMS[st.id];
        Col rc = rarityColor(it.rarity);
        ui_.fillF(812, 168, 72, 72, withAlpha(it.color, 36));
        ui_.chamferOutline({812, 168, 72, 72}, 6, rc, 1.5f);
        ui_.diamond({848, 204}, 18, withAlpha(it.color, 220));
        ui_.text(it.name, 902, 168, 25, C_TEXT);
        ui_.text(std::string(rarityName(it.rarity)) + " · " + kindName(it.kind), 902, 204, 12, rc);
        ui_.text("数量 ×" + std::to_string(st.count), 902, 224, 12, C_MUTED);
        ui_.wrapped(it.desc, 812, 260, 14, C_MUTED, 396, 3);

        ui_.line({812, 326}, {1208, 326}, C_BORDER, 1.f);
        ui_.text("单价", 812, 344, 11, C_MUTED);
        ui_.text("¥" + std::to_string(it.value), 1208, 340, 17, C_GOLD, ALIGN_RIGHT);
        ui_.text("重量", 812, 372, 11, C_MUTED);
        ui_.text(std::to_string((int)std::lround(it.weight * 1000)) + " g", 1208, 368, 15, C_TEXT, ALIGN_RIGHT);
        ui_.text("这一组可卖", 812, 400, 11, C_MUTED);
        ui_.text("¥" + std::to_string(sellPrice(st.id, st.count)), 1208, 396, 19, C_GOLD, ALIGN_RIGHT);
        ui_.line({812, 430}, {1208, 430}, C_BORDER, 1.f);

        if (ui_.button({812, 446, 190, 40}, "卖出 1 个", st.count > 0, false, 15)) {
            int gained = profile_.sellStack(stashSel_, 1);
            audio_.play(SFX_COIN, .8f, 1.12f);
            notify("卖出 " + std::string(it.name) + "  +¥" + std::to_string(gained), C_GOLD);
            if (stashSel_ >= (int)profile_.stash.size()) stashSel_ = -1;
            saveGame(true);
        }
        if (ui_.button({1018, 446, 190, 40}, "整组卖出", st.count > 0, true, 15)) {
            int gained = profile_.sellStack(stashSel_, -1);
            audio_.play(SFX_COIN, .9f, 1.0f);
            notify("整组卖出  +¥" + std::to_string(gained), C_GOLD);
            stashSel_ = -1;
            saveGame(true);
        }
        ui_.text("卖出后金币立即入账。稀有物品建议留着做委托。", 812, 500, 10, C_MUTED);
    } else {
        ui_.text("选择一个物品", 1010, 300, 18, C_MUTED, ALIGN_CENTER);
        ui_.text("仓库里的每一件垃圾", 1010, 330, 12, Col(102, 114, 92), ALIGN_CENTER);
        ui_.text("都曾属于某个没爬出来的蛆。", 1010, 350, 12, Col(102, 114, 92), ALIGN_CENTER);
        ui_.diamond({1010, 400}, 24, withAlpha(C_CYAN, 70));
    }

    ui_.line({812, 540}, {1208, 540}, C_BORDER, 1.f);
    ui_.text("库存总值", 812, 558, 11, C_MUTED);
    ui_.text(formatMoney(profile_.stashValue()), 1208, 552, 22, C_GOLD, ALIGN_RIGHT);
    if (ui_.button({812, 586, 396, 40}, "卖出所有垃圾（稀有度 0）", !profile_.stash.empty(), false, 14)) {
        int before = profile_.coins;
        int gained = profile_.sellJunk();
        if (gained > 0) {
            audio_.play(SFX_COIN, .8f, .95f);
            notify("清理垃圾  +¥" + std::to_string(profile_.coins - before), C_GOLD);
            stashSel_ = -1;
            saveGame(true);
        } else {
            notify("没有可清理的垃圾", C_MUTED);
        }
    }
    if (ui_.button({812, 634, 396, 40}, "全部卖出", !profile_.stash.empty(), false, 14)) {
        int gained = profile_.sellAll();
        audio_.play(SFX_COIN, .9f, 1.05f);
        notify("清空仓库  +¥" + std::to_string(gained), C_GOLD);
        stashSel_ = -1;
        saveGame(true);
    }
    ui_.text("金币 " + formatMoney(profile_.coins) + "  ·  卖出后可在黑市升级装备", 812, 692, 11, C_MUTED);
    ui_.text("死亡会重置为初始配装，仓库不受影响。", 812, 714, 11, C_MUTED);
    ui_.text("仓库重量不计入负重，只影响你的强迫症。", 812, 736, 10, Col(94, 106, 86));
}

void Game::drawMissions() {
    drawBackdrop(time_, C_GOLD);
    drawTopBar(true, "任务委托", "蛆巢长老的悬赏 · 完成即可领取");
    ui_.text("完成委托会累计进度，奖励只需领取一次。", 48, 84, 12, C_MUTED);

    for (int i = 0; i < 5; ++i) {
        const MissionDef& m = MISSIONS[i];
        Rect r{48.f, 104.f + i * 110.f, 1184.f, 100.f};
        bool claimed = profile_.missionClaimed[i];
        int prog = profile_.missionProgress(i);
        bool done = prog >= m.target;
        bool claimable = done && !claimed;
        Col acc = claimable ? C_GOLD : (claimed ? Col(82, 92, 70) : (done ? C_ACCENT : C_BORDER));
        ui_.panel(r, claimed ? Col(10, 14, 9, 210) : Col(13, 19, 12, 232), acc, 8);
        ui_.fillF(r.x, r.y, 4, r.h, withAlpha(acc, claimed ? 90 : 210));
        ui_.text("0" + std::to_string(i + 1), r.x + 16, r.y + 12, 11, C_MUTED);
        ui_.text(m.name, r.x + 62, r.y + 12, 21, claimed ? C_MUTED : C_TEXT);
        ui_.text(m.desc, r.x + 62, r.y + 42, 11, C_MUTED);
        float k = clamp01((float)prog / (float)m.target);
        ui_.bar({r.x + 62, r.y + 68, 560, 7}, k, claimed ? Col(72, 80, 60) : (done ? C_GOLD : C_ACCENT),
                Col(7, 9, 7, 220), C_BORDER);
        ui_.text(std::to_string(std::min(prog, m.target)) + " / " + std::to_string(m.target),
                 r.x + 62, r.y + 80, 10, claimed ? C_MUTED : C_TEXT);

        ui_.line({r.x + 650, r.y + 14}, {r.x + 650, r.y + 86}, C_BORDER, 1.f);
        ui_.text("奖励", r.x + 670, r.y + 14, 11, C_MUTED);
        ui_.text("¥" + formatMoney(m.rewardCoins), r.x + 670, r.y + 32, 17, C_GOLD);
        std::string extra;
        if (m.weapon != WPN_NONE) extra += std::string(WEAPONS[m.weapon].name) + " ×" + std::to_string(m.weaponCount);
        if (m.armor != ARM_NONE) {
            if (!extra.empty()) extra += "  ·  ";
            extra += std::string(ARMORS[m.armor].name) + " ×" + std::to_string(m.armorCount);
        }
        if (!extra.empty()) ui_.text(extra, r.x + 670, r.y + 58, 10, C_ACCENT);

        std::string label = claimed ? "已领取" : (claimable ? "领取奖励" : "进行中");
        if (ui_.button({r.right() - 186, r.y + 26, 162, 48}, label, claimable, claimable, 17)) {
            if (profile_.claimMission(i)) {
                audio_.play(SFX_MISSION, .9f, 1.0f);
                notify("委托完成：" + std::string(m.name), C_GOLD);
                saveGame(true);
            }
        }
        if (claimable) {
            ui_.text("可领取", r.right() - 105, r.y + 80, 10, C_GOLD, ALIGN_CENTER);
        }
    }

    int claimable = 0;
    for (int i = 0; i < 5; ++i) {
        if (!profile_.missionClaimed[i] && profile_.missionProgress(i) >= MISSIONS[i].target) claimable++;
    }
    if (claimable > 0) {
        ui_.text("有 " + std::to_string(claimable) + " 项奖励等待领取", 640, 674, 12, C_GOLD, ALIGN_CENTER);
    } else {
        ui_.text("所有能领的都已经领了。剩下的，用命去换。", 640, 674, 12, C_MUTED, ALIGN_CENTER);
    }
    ui_.text("TIP：击杀、撤离与带出价值都会永久累计，死亡不会清零。", 640, 704, 11, C_MUTED, ALIGN_CENTER);
}

void Game::drawShop() {
    drawBackdrop(time_, C_GOLD);
    drawTopBar(true, "黑市交易", "老蛆的摊位 · 概不赊账");
    const char* tabs[4] = {"武器", "护甲", "背包", "补给"};
    for (int i = 0; i < 4; ++i) {
        if (ui_.chip({48.f + i * 154.f, 90.f, 142.f, 34.f}, tabs[i], shopTab_ == i, true, 14)) {
            shopTab_ = i;
        }
    }
    ui_.text("“钱货两清。出了这个门，别说你认识我。”", 1220, 100, 11, C_MUTED, ALIGN_RIGHT);

    if (shopTab_ == 0) {
        for (int i = 1; i < WPN_COUNT; ++i) {
            const WeaponDef& w = WEAPONS[i];
            int col = (i - 1) % 2, row = (i - 1) / 2;
            Rect r{48.f + col * 388.f, 142.f + row * 164.f, 376.f, 150.f};
            bool owned = profile_.weapon == (WeaponId)i;
            Col acc = w.accent;
            ui_.panel(r, Col(13, 19, 12, 232), owned ? C_ACCENT2 : C_BORDER, 8);
            ui_.fillF(r.x, r.y, 4, r.h, acc);
            ui_.text(w.name, r.x + 18, r.y + 13, 20, C_TEXT);
            ui_.text("¥" + formatMoney(w.price), r.right() - 18, r.y + 18, 18, C_GOLD, ALIGN_RIGHT);
            ui_.text(w.shortName, r.x + 18, r.y + 42, 10, C_MUTED);
            ui_.wrapped(w.desc, r.x + 18, r.y + 58, 9, C_MUTED, 336, 2);
            ui_.text("伤害 " + std::to_string(w.damage) + (w.pellets > 1 ? "×" + std::to_string(w.pellets) : ""),
                     r.x + 18, r.y + 121, 10, C_TEXT);
            ui_.text("射速 " + std::to_string((int)w.rpm), r.x + 94, r.y + 121, 10, C_TEXT);
            ui_.text("弹匣 " + std::to_string(w.mag), r.x + 172, r.y + 121, 10, C_TEXT);
            if (owned) {
                ui_.text("已装备", r.right() - 20, r.y + 102, 11, C_ACCENT, ALIGN_RIGHT);
            } else if (ui_.button({r.right() - 136, r.y + 92, 118, 40}, "购买", profile_.coins >= w.price, true, 14)) {
                buyWeapon((WeaponId)i);
            }
        }
    } else if (shopTab_ == 1) {
        for (int i = 1; i < ARM_COUNT; ++i) {
            const ArmorDef& a = ARMORS[i];
            int col = (i - 1) % 2, row = (i - 1) / 2;
            Rect r{48.f + col * 388.f, 142.f + row * 164.f, 376.f, 150.f};
            bool owned = profile_.armor == (ArmorId)i;
            ui_.panel(r, Col(13, 19, 12, 232), owned ? C_ACCENT2 : C_BORDER, 8);
            ui_.fillF(r.x, r.y, 4, r.h, a.accent);
            ui_.text(a.name, r.x + 18, r.y + 13, 20, C_TEXT);
            ui_.text("¥" + formatMoney(a.price), r.right() - 18, r.y + 18, 18, C_GOLD, ALIGN_RIGHT);
            ui_.wrapped(a.desc, r.x + 18, r.y + 46, 10, C_MUTED, 330, 2);
            ui_.text("护甲 " + std::to_string(a.armor), r.x + 18, r.y + 121, 11, C_CYAN);
            ui_.text("减伤 " + std::to_string((int)std::lround(a.reduction * 100)) + "%", r.x + 94, r.y + 121, 11, C_CYAN);
            ui_.text(kgText((int)std::lround(a.weight * 1000)), r.x + 174, r.y + 121, 10, C_MUTED);
            if (owned) ui_.text("已装备", r.right() - 20, r.y + 102, 11, C_ACCENT, ALIGN_RIGHT);
            else if (ui_.button({r.right() - 136, r.y + 92, 118, 40}, "购买", profile_.coins >= a.price, true, 14)) {
                buyArmor((ArmorId)i);
            }
        }
    } else if (shopTab_ == 2) {
        for (int i = 1; i < 4; ++i) {
            const BackpackDef& b = BACKPACKS[i];
            int col = (i - 1) % 2, row = (i - 1) / 2;
            Rect r{48.f + col * 388.f, 142.f + row * 164.f, 376.f, 150.f};
            bool owned = profile_.backpack == i;
            ui_.panel(r, Col(13, 19, 12, 232), owned ? C_ACCENT2 : C_BORDER, 8);
            ui_.fillF(r.x, r.y, 4, r.h, b.accent);
            ui_.text(b.name, r.x + 18, r.y + 13, 20, C_TEXT);
            ui_.text("¥" + formatMoney(b.price), r.right() - 18, r.y + 18, 18, C_GOLD, ALIGN_RIGHT);
            ui_.wrapped(b.desc, r.x + 18, r.y + 46, 10, C_MUTED, 330, 2);
            ui_.text("容量 " + std::to_string(b.capacity) + " kg", r.x + 18, r.y + 121, 11, C_GOLD);
            if (owned) ui_.text("已装备", r.right() - 20, r.y + 102, 11, C_ACCENT, ALIGN_RIGHT);
            else if (ui_.button({r.right() - 136, r.y + 92, 118, 40}, "购买", profile_.coins >= b.price, true, 14)) {
                buyBackpack(i);
            }
        }
    } else {
        ui_.text("弹药", 48, 142, 12, C_GOLD);
        for (int a = 0; a < AMMO_COUNT; ++a) {
            Rect r{48.f, 166.f + a * 66.f, 760.f, 56.f};
            ui_.panel(r, Col(13, 19, 12, 228), C_BORDER, 6);
            ui_.text(ammoName((AmmoId)a), r.x + 18, r.y + 10, 16, C_TEXT);
            ui_.text("持有 " + std::to_string(profile_.ammo[a]) + " 发", r.x + 300, r.y + 12, 13, C_MUTED);
            ui_.text("每箱 " + std::to_string(ammoBox((AmmoId)a)) + " 发", r.x + 470, r.y + 12, 12, C_MUTED);
            ui_.text("¥" + std::to_string(ammoPrice((AmmoId)a)), r.x + 580, r.y + 10, 17, C_GOLD, ALIGN_RIGHT);
            if (ui_.button({r.right() - 152, r.y + 8, 134, 38}, "购买", profile_.coins >= ammoPrice((AmmoId)a), true, 14)) {
                buyAmmo((AmmoId)a);
            }
        }
        ui_.text("医疗", 48, 388, 12, C_CYAN);
        Rect b1{48, 412, 760, 56};
        ui_.panel(b1, Col(13, 19, 12, 228), C_BORDER, 6);
        ui_.text("蛆用绷带 ×2", b1.x + 18, b1.y + 10, 16, C_TEXT);
        ui_.text("持有 " + std::to_string(profile_.bandages), b1.x + 300, b1.y + 12, 13, C_MUTED);
        ui_.text("¥120", b1.x + 580, b1.y + 10, 17, C_GOLD, ALIGN_RIGHT);
        if (ui_.button({b1.right() - 152, b1.y + 8, 134, 38}, "购买", profile_.coins >= 120, true, 14)) buyMeds(0);
        Rect b2{48, 480, 760, 56};
        ui_.panel(b2, Col(13, 19, 12, 228), C_BORDER, 6);
        ui_.text("止痛针 ×1", b2.x + 18, b2.y + 10, 16, C_TEXT);
        ui_.text("持有 " + std::to_string(profile_.medkits), b2.x + 300, b2.y + 12, 13, C_MUTED);
        ui_.text("¥420", b2.x + 580, b2.y + 10, 17, C_GOLD, ALIGN_RIGHT);
        if (ui_.button({b2.right() - 152, b2.y + 8, 134, 38}, "购买", profile_.coins >= 420, true, 14)) buyMeds(1);
    }

    Rect note{860, 142, 372, 500};
    ui_.panel(note, Col(11, 17, 10, 220), C_BORDER, 10);
    ui_.fillF(note.x, note.y, 4, note.h, withAlpha(C_GOLD, 170));
    ui_.text("老蛆的忠告", note.x + 20, note.y + 18, 12, C_GOLD);
    const char* tips[4] = {
        "枪是消耗品，命也是。区别是枪可以再买。",
        "别把全部身家带进封锁区。带得走的是钱，带不走的是别人的钱。",
        "听到蛆王的声音就回头。它不缺你这一口。",
        "仓库里的东西不卖，就只是发霉的纪念品。",
    };
    ui_.wrapped(tips[shopTab_], note.x + 20, note.y + 48, 14, C_TEXT, 332, 4);
    ui_.line({note.x + 20, note.y + 200}, {note.right() - 20, note.y + 200}, C_BORDER, 1.f);
    ui_.text("当前配装", note.x + 20, note.y + 220, 11, C_MUTED);
    ui_.text(WEAPONS[profile_.weapon].name, note.x + 20, note.y + 242, 18, C_TEXT);
    ui_.text(ARMORS[profile_.armor].name, note.x + 20, note.y + 270, 13, C_MUTED);
    ui_.text(BACKPACKS[std::clamp(profile_.backpack, 0, 3)].name, note.x + 20, note.y + 292, 13, C_MUTED);
    ui_.text("金币", note.x + 20, note.y + 338, 11, C_MUTED);
    ui_.text(formatMoney(profile_.coins), note.right() - 20, note.y + 330, 28, C_GOLD, ALIGN_RIGHT);
    ui_.text("所有购买立即装备并保存。", note.x + 20, note.y + 390, 10, C_MUTED);
}

void Game::drawSettings() {
    drawBackdrop(time_, C_ACCENT2);
    drawTopBar(true, "设置", "舒适地当一条蛆");
    Rect p{300, 106, 680, 620};
    ui_.panel(p, Col(12, 18, 11, 234), C_BORDER, 12);
    ui_.fillF(p.x, p.y, 4, p.h, C_ACCENT2);
    ui_.text("音频", 328, 124, 12, C_MUTED);
    ui_.text("主音量", 328, 152, 16, C_TEXT);
    if (ui_.button({520, 146, 38, 30}, "-", profile_.settings.volume > 0.f, false, 16)) {
        profile_.settings.volume = clamp01(profile_.settings.volume - .1f);
        audio_.setMaster(profile_.settings.volume);
        saveGame(true);
    }
    ui_.bar({572, 156, 280, 8}, profile_.settings.volume, C_ACCENT, Col(8, 11, 8, 220), C_BORDER);
    ui_.text(std::to_string((int)std::lround(profile_.settings.volume * 100)) + "%", 904, 150, 15, C_TEXT, ALIGN_RIGHT);
    if (ui_.button({920, 146, 38, 30}, "+", profile_.settings.volume < 1.f, false, 16)) {
        profile_.settings.volume = clamp01(profile_.settings.volume + .1f);
        audio_.setMaster(profile_.settings.volume);
        saveGame(true);
    }
    ui_.text("静音", 328, 212, 16, C_TEXT);
    if (ui_.chip({520, 206, 120, 30}, profile_.settings.muted ? "已静音" : "关闭", profile_.settings.muted, true, 13)) {
        profile_.settings.muted = !profile_.settings.muted;
        audio_.setMuted(profile_.settings.muted);
        saveGame(true);
    }
    ui_.line({328, 258}, {952, 258}, C_BORDER, 1.f);
    ui_.text("画面", 328, 276, 12, C_MUTED);
    ui_.text("屏幕震动", 328, 304, 16, C_TEXT);
    if (ui_.chip({520, 298, 120, 30}, profile_.settings.shake ? "开启" : "关闭", profile_.settings.shake, true, 13)) {
        profile_.settings.shake = !profile_.settings.shake;
        saveGame(true);
    }
    ui_.text("全屏", 328, 348, 16, C_TEXT);
    if (ui_.chip({520, 342, 120, 30}, profile_.settings.fullscreen ? "全屏" : "窗口", profile_.settings.fullscreen, true, 13)) {
        profile_.settings.fullscreen = !profile_.settings.fullscreen;
        if (!screenshotMode_) {
            SDL_SetWindowFullscreen(window_, profile_.settings.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
            SDL_RenderSetLogicalSize(renderer_, (int)VIEW_W, (int)VIEW_H);
        }
        saveGame(true);
    }
    if (ui_.chip({660, 342, 160, 30}, profile_.settings.autofire ? "长按连发 开" : "长按连发 关",
                 profile_.settings.autofire, true, 12)) {
        profile_.settings.autofire = !profile_.settings.autofire;
        saveGame(true);
    }
    ui_.line({328, 392}, {952, 392}, C_BORDER, 1.f);
    ui_.text("存档", 328, 410, 12, C_MUTED);
    ui_.text("存档路径", 328, 438, 14, C_TEXT);
    ui_.wrapped(savePath_.empty() ? "（本次运行未启用存档）" : savePath_, 440, 438, 11, C_MUTED, 500, 2);
    ui_.text("游戏会在购买、结算、领取奖励与退出时自动保存。", 328, 474, 11, C_MUTED);
    ui_.line({328, 508}, {952, 508}, C_BORDER, 1.f);
    ui_.text("危险区域", 328, 524, 12, C_DANGER);
    ui_.text("清空存档会重置金币、装备、仓库、任务与统计。", 328, 548, 12, C_MUTED);
    if (!confirmReset_) {
        if (ui_.button({328, 580, 240, 40}, "清空存档", true, false, 14)) confirmReset_ = true;
    } else {
        if (ui_.button({328, 580, 240, 40}, "再次点击确认", true, false, 14)) {
            Settings keep = profile_.settings;
            profile_ = Profile{};
            profile_.settings = keep;
            saveGame(true);
            notify("存档已清空，一切从头开始", C_DANGER);
            confirmReset_ = false;
            stashSel_ = -1;
            setScreen(Screen::HUB);
        }
        if (ui_.button({584, 580, 160, 40}, "取消", true, false, 14)) confirmReset_ = false;
    }
    if (ui_.button({760, 580, 192, 40}, "返回主菜单", true, true, 14)) setScreen(Screen::HUB);
    ui_.text("暗蛆突围 v0.1  ·  C++17 + SDL2  ·  全程程序化美术与音效", 640, 696, 10, C_MUTED, ALIGN_CENTER);
}

void Game::drawResult() {
    bool good = result_.success;
    bool dead = result_.died;
    Col acc = good ? C_ACCENT : (dead ? C_DANGER : C_GOLD);
    drawBackdrop(time_, acc);
    drawTopBar(false, "行动结算", result_.success ? "带着东西活着出来了" : "封锁区吞掉了这次行动");

    Rect p{150, 96, 980, 650};
    ui_.panel(p, Col(11, 17, 10, 238), withAlpha(acc, 120), 16);
    ui_.fillF(p.x, p.y, 5, p.h, acc);
    std::string title = good ? "撤离成功" : (dead ? "行动失败" : "已放弃行动");
    ui_.textShadow(title, 640, 118, 52, acc, ALIGN_CENTER, 0, 3);
    ui_.text((result_.mapIndex >= 0 && result_.mapIndex < 3 ? MAPS[result_.mapIndex].name : std::string("未知区域")) +
             "  ·  " + result_.reason, 640, 184, 14, C_TEXT, ALIGN_CENTER);

    drawStatCard({190, 218, 196, 90}, "击杀", std::to_string(result_.kills), C_GOLD);
    drawStatCard({404, 218, 196, 90}, "搜刮容器", std::to_string(result_.containersSearched), C_TEXT);
    drawStatCard({618, 218, 196, 90}, "战利品价值", formatMoney(result_.lootValue), good ? C_GOLD : C_DANGER);
    drawStatCard({832, 218, 196, 90}, "行动用时", formatTime(result_.time), C_CYAN);

    ui_.line({190, 334}, {1028, 334}, C_BORDER, 1.f);
    ui_.text("本次战利品", 190, 350, 12, C_MUTED);
    if (good && !result_.loot.empty()) {
        int n = 0;
        for (auto& st : result_.loot) {
            if (n >= 10) break;
            int col = n % 2, row = n / 2;
            const ItemDef& it = ITEMS[st.id];
            float x = 190.f + col * 410.f;
            float y = 376.f + row * 30.f;
            ui_.fillF(x, y + 8, 5, 5, rarityColor(it.rarity));
            ui_.text(it.name + (st.count > 1 ? " ×" + std::to_string(st.count) : ""), x + 14, y, 14, C_TEXT);
            ui_.text("¥" + std::to_string(it.value * st.count), x + 360, y, 12, C_GOLD, ALIGN_RIGHT);
            n++;
        }
        ui_.text("已放入蛆巢仓库", 1028, 350, 11, C_ACCENT, ALIGN_RIGHT);
    } else if (dead) {
        if (resultKnife_) {
            ui_.text("你只带了一把刀；家中装备没有损失，本次战利品留在封锁区。", 190, 378, 14, C_GOLD);
            ui_.text("跑刀模式：无枪、无甲、无背包、无药品。", 190, 406, 12, C_MUTED);
        } else {
            ui_.text("你带进来的东西，连同这次找到的东西，都留在了封锁区。", 190, 378, 14, C_DANGER);
            ui_.text("配装已重置为：锈钉手枪 / 赤裸软体 / 破洞塑料袋 / 9mm ×18", 190, 406, 12, C_MUTED);
        }
    } else {
        ui_.text("你放弃了本次行动，战利品掉在原地。装备和剩余弹药被带回来了。", 190, 378, 14, C_GOLD);
    }

    ui_.line({190, 530}, {1028, 530}, C_BORDER, 1.f);
    ui_.text("获得经验", 190, 546, 12, C_MUTED);
    ui_.text("+" + std::to_string(lastXp_), 280, 540, 22, C_ACCENT);
    ui_.text("Lv." + std::to_string(profile_.level()) + "  ·  " + std::to_string(profile_.xpIntoLevel()) +
             " / " + std::to_string(profile_.xpForNext()) + " XP", 1028, 548, 12, C_MUTED, ALIGN_RIGHT);
    ui_.bar({400, 550, 200, 7}, (float)profile_.xpIntoLevel() / (float)profile_.xpForNext(),
            C_ACCENT, Col(8, 11, 8, 220), C_BORDER);
    ui_.text(profile_.gearSummary(), 190, 586, 13, C_TEXT);
    ui_.text("金币 " + formatMoney(profile_.coins) + "   ·   仓库 " + formatMoney(profile_.stashValue()),
             1028, 586, 12, C_GOLD, ALIGN_RIGHT);
    int claimable = 0;
    for (int i = 0; i < 5; ++i) {
        if (!profile_.missionClaimed[i] && profile_.missionProgress(i) >= MISSIONS[i].target) claimable++;
    }
    if (claimable > 0) {
        ui_.text("有 " + std::to_string(claimable) + " 项任务奖励可以领取", 640, 618, 12, C_GOLD, ALIGN_CENTER);
    } else {
        ui_.text("装备带得走，命带得走，只有战利品需要你自己去拿。", 640, 618, 11, C_MUTED, ALIGN_CENTER);
    }

    if (ui_.button({278, 654, 300, 52}, "返回巢穴", true, false, 19)) {
        setScreen(Screen::HUB);
    }
    if (ui_.button({610, 654, 392, 52}, "再次潜入", true, true, 20)) {
        deployMap_ = std::clamp(result_.mapIndex, 0, 2);
        beginRaid();
    }
    ui_.text("再次潜入会重新开始计时、生成敌人与物资。", 640, 716, 10, C_MUTED, ALIGN_CENTER);
}

void Game::buyAmmo(AmmoId a) {
    int price = ammoPrice(a);
    if (profile_.coins < price) {
        notify("金币不足", C_DANGER);
        return;
    }
    profile_.coins -= price;
    profile_.ammo[(int)a] += ammoBox(a);
    audio_.play(SFX_BUY, .75f, 1.f);
    notify(std::string("买入 ") + ammoName(a) + " ×" + std::to_string(ammoBox(a)), C_GOLD);
    saveGame(true);
}

void Game::buyMeds(int kind) {
    int price = kind == 0 ? 120 : 420;
    if (profile_.coins < price) {
        notify("金币不足", C_DANGER);
        return;
    }
    profile_.coins -= price;
    if (kind == 0) profile_.bandages += 2;
    else profile_.medkits += 1;
    audio_.play(SFX_BUY, .75f, 1.1f);
    notify(kind == 0 ? "买入 蛆用绷带 ×2" : "买入 止痛针 ×1", C_CYAN);
    saveGame(true);
}

void Game::buyWeapon(WeaponId w) {
    if (w <= WPN_NONE || w >= WPN_COUNT) return;
    const WeaponDef& def = WEAPONS[w];
    if (profile_.weapon == w) return;
    if (profile_.coins < def.price) {
        notify("金币不足", C_DANGER);
        return;
    }
    profile_.coins -= def.price;
    profile_.weapon = w;
    audio_.play(SFX_BUY, .85f, .96f);
    notify("已装备 " + std::string(def.name), C_ACCENT);
    saveGame(true);
}

void Game::buyArmor(ArmorId a) {
    if (a <= ARM_NONE || a >= ARM_COUNT) return;
    const ArmorDef& def = ARMORS[a];
    if (profile_.armor == a) return;
    if (profile_.coins < def.price) {
        notify("金币不足", C_DANGER);
        return;
    }
    profile_.coins -= def.price;
    profile_.armor = a;
    audio_.play(SFX_BUY, .85f, .96f);
    notify("已装备 " + std::string(def.name), C_CYAN);
    saveGame(true);
}

void Game::buyBackpack(int b) {
    if (b <= 0 || b >= 4) return;
    const BackpackDef& def = BACKPACKS[b];
    if (profile_.backpack == b) return;
    if (profile_.coins < def.price) {
        notify("金币不足", C_DANGER);
        return;
    }
    profile_.coins -= def.price;
    profile_.backpack = b;
    audio_.play(SFX_BUY, .85f, .96f);
    notify("已装备 " + std::string(def.name), C_GOLD);
    saveGame(true);
}
