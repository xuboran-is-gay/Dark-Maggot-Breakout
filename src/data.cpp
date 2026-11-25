#include "data.h"

const WeaponDef WEAPONS[WPN_COUNT] = {
    { WPN_NONE, "空手", "空手", "你是一条蛆。用牙咬可能都比这靠谱。", AMMO_9MM, 0, 0, 60, 0, 0, 0, 0, 0, 0, C_MUTED },
    { WPN_PISTOL, "锈钉手枪", "锈钉", "从尸体上拔下来的老古董。声音大，后坐力也不小，但总比用牙咬强。",
      AMMO_9MM, 19, 1, 260, 3.5f, 740, 12, 1.35f, 350, 1.0f, C_GOLD },
    { WPN_SMG, "唾沫冲锋枪", "唾沫", "喷得快，散得也快。适合在走廊里把天花板打成筛子。",
      AMMO_9MM, 13, 1, 760, 5.4f, 610, 30, 1.65f, 1350, 2.4f, C_CYAN },
    { WPN_SHOTGUN, "骨渣霰弹枪", "骨渣", "近距离谈判专家。只要你能活着走到对方面前。",
      AMMO_12G, 8, 9, 85, 10.5f, 430, 6, 2.35f, 1650, 3.4f, C_DANGER },
    { WPN_RIFLE, "腐酸步枪", "腐酸", "净化队制式武器。射得远，也贵得远。丢了会心疼很久。",
      AMMO_556, 35, 1, 470, 2.2f, 1020, 20, 2.0f, 3300, 3.8f, C_ACCENT },
    { WPN_MACHINE, "蛆卵冲锋枪", "蛆卵", "从蛆蛹里孵化出的连发武器。射速快，弹匣大，但精度感人。",
      AMMO_9MM, 11, 1, 940, 7.2f, 520, 45, 1.9f, 2100, 2.8f, Col(224, 176, 96) },
    { WPN_CARBINE, "锈斑卡宾", "锈斑", " shortened rifle. 比步枪灵活，威力略小，中距离好手。",
      AMMO_556, 27, 1, 610, 3.0f, 900, 24, 1.8f, 2600, 3.1f, Col(190, 210, 150) },
    { WPN_SAW, "骨锯轻机枪", "骨锯", "弹链供弹。架住走廊，让所有东西都变成肉泥。",
      AMMO_762, 24, 1, 620, 8.0f, 940, 60, 3.2f, 5200, 6.2f, Col(236, 138, 92) },
    { WPN_BOLT, "长钉狙击枪", "长钉", "一发入魂。拉栓很慢，但打中就是赚到。",
      AMMO_762, 92, 1, 38, 1.4f, 1600, 5, 3.4f, 4600, 4.4f, Col(214, 196, 140) },
    { WPN_DMR, "腐蚀精确步枪", "腐蚀", "净化队狙击手的家伙。半自动，稳，准，狠。",
      AMMO_556, 58, 1, 210, 2.0f, 1340, 15, 2.5f, 4100, 4.0f, Col(140, 232, 186) },
};

// slot, armour hp, damage reduction, rig slots, speed multiplier, price, weight
const ArmorDef ARMORS[ARM_COUNT] = {
    { ARM_NONE, "赤裸软体", "没有护甲。你的身体就是护甲。", ASL_BODY, 0, 0.f, 0, 1.f, 0, 0, C_MUTED },
    { ARM_CAP, "布头巾", "挡不住子弹，但能挡住别人的目光。", ASL_HELMET, 10, .10f, 0, 1.f, 260, 0.3f, C_GOLD },
    { ARM_STEEL, "铁皮头盔", "从垃圾桶里捡的。至少不会让流弹削掉半个脑袋。", ASL_HELMET, 28, .22f, 0, .96f, 900, 1.4f, C_CYAN },
    { ARM_RIOT, "防暴头罩", "净化队标准装备。视野差，但脑袋还在。", ASL_HELMET, 52, .34f, 0, .92f, 2200, 2.2f, Col(160, 204, 188) },
    { ARM_TANK, "蛆王颅壳", "厚得离谱。戴上它你就是会走路的掩体。", ASL_HELMET, 88, .46f, 0, .86f, 4800, 3.4f, C_ACCENT },
    { ARM_LIGHT, "硬纸胸甲", "聊胜于无。至少能挡住一块飞来的面包。", ASL_BODY, 26, .26f, 0, 1.f, 420, 0.8f, C_GOLD },
    { ARM_PLATE, "铁皮龟壳", "从报废洗衣机上拆下来的。沉，但真能挡子弹。", ASL_BODY, 68, .44f, 0, .92f, 1400, 2.2f, C_CYAN },
    { ARM_CARAPACE, "蛆王甲壳", "还带着原主人的体温。大部分子弹会在上面打滑。", ASL_BODY, 115, .58f, 0, .86f, 4200, 3.0f, C_ACCENT },
    { ARM_IRON, "净化队板甲", "正经军用护甲，又重又硬。穿上就不想跑快。", ASL_BODY, 150, .66f, 0, .78f, 6200, 4.6f, Col(196, 176, 132) },
    { ARM_WING, "轻骨架胸甲", "用蛆壳纤维编的。几乎不影响移动。", ASL_BODY, 44, .32f, 0, 1.04f, 1800, 1.1f, Col(184, 214, 152) },
    { ARM_POUCH, "腰间杂物袋", "多挂两个袋子，就多两次活命的机会。", ASL_RIG, 0, 0.f, 4, 1.f, 260, 0.5f, C_GOLD },
    { ARM_VEST, "战术蛆衣", "兜很多。装得多，跑得也还行。", ASL_RIG, 0, 0.f, 8, .99f, 800, 1.0f, C_CYAN },
    { ARM_CHEST, "缝合胸挂", "前胸全是弹匣袋。换弹快一点，命就长一点。", ASL_RIG, 0, 0.f, 12, .97f, 1800, 1.6f, Col(214, 182, 136) },
    { ARM_MILRIG, "军规战术挂", "净化队的完整战术系统。兜多，还轻。", ASL_RIG, 0, 0.f, 18, .94f, 3600, 1.9f, Col(140, 220, 190) },
    { ARM_PLATE_RIG, "复合板胸挂", "挂上它你就是一座会移动的墙。", ASL_RIG, 12, .12f, 24, .82f, 5400, 3.2f, C_ACCENT },
};

const BackpackDef BACKPACKS[4] = {
    { "破洞塑料袋", "装得不多，漏得不少。每个蛆的第一件行李。", 12, 0, C_MUTED },
    { "缝合蛆囊", "用肠线缝的。能装，但最好不要闻。", 18, 800, C_GOLD },
    { "军规尸袋", "净化队同款。防水、防撕、防不了子弹。", 26, 2400, C_CYAN },
    { "蛆王胃袋", "空间异常大。里面偶尔会传来别人的声音。", 36, 6000, C_ACCENT },
};

const ItemDef ITEMS[IT_COUNT] = {
    { "生锈螺丝", "值不了几个钱，但总比空手强。", .20f, 90, 0, IK_JUNK, Col(154, 142, 116) },
    { "发霉面包", "别吃。真的。", .40f, 120, 0, IK_FOOD, Col(154, 132, 88) },
    { "半罐尸油", "工业润滑剂，黑市硬通货。", .80f, 260, 0, IK_JUNK, Col(164, 118, 53) },
    { "旧手机", "屏幕碎了，但里面的蛆片还能卖。", .30f, 390, 1, IK_TECH, Col(101, 160, 192) },
    { "荧光菌", "在黑暗里发绿光。蛆巢的夜灯。", .50f, 620, 1, IK_ORGANIC, Col(143, 220, 104) },
    { "医用蛆蛋白", "高级营养膏，人蛆通用。", .60f, 900, 2, IK_ORGANIC, Col(219, 196, 231) },
    { "军用芯片", "从净化部队的残骸里抠出来的。", .20f, 1600, 2, IK_TECH, Col(99, 213, 197) },
    { "黄金蛆卵", "腐败纪元最保值的硬通货。", .90f, 2400, 3, IK_TREASURE, Col(238, 195, 83) },
    { "蛆王腺体", "还在搏动。建议不要贴身携带。", 1.20f, 5200, 4, IK_ORGANIC, Col(214, 96, 78) },
    { "净化队狗牌", "记录着一个已经不需要名字的人。", .10f, 750, 1, IK_JUNK, Col(184, 190, 174) },
    { "完整的眼球", "别问是谁的。黑市有人收。", .20f, 480, 1, IK_ORGANIC, Col(218, 213, 190) },
    { "一撮腿毛", "……至少是纯天然的。", .05f, 40, 0, IK_JUNK, Col(122, 100, 74) },
    { "9mm 散装弹", "捡来的子弹。能压进手枪和冲锋枪。", .012f, 3, 0, IK_AMMO, C_GOLD },
    { "12号骨弹", "霰弹枪的粮食。重，但值得。", .050f, 8, 0, IK_AMMO, C_DANGER },
    { "5.56 酸弹", "步枪弹。净化队留下的最后一点温柔。", .015f, 12, 1, IK_AMMO, C_ACCENT },
    { "7.62 骨芯弹", "机枪和狙击弹。重，贵，能穿墙。", .022f, 18, 1, IK_AMMO, Col(224, 152, 108) },
    { "蛆用绷带", "止血。不能治脑子。", .20f, 60, 0, IK_MED, C_TEXT },
    { "止痛针", "让你暂时忘记自己正在流血。", .30f, 220, 1, IK_MED, C_CYAN },
};

const MapDef MAPS[3] = {
    { "臭水沟", "THE SEWER", "低风险",
      "新手蛆的第一口汤。拾荒蛆多，净化队少。别在下水道里迷路，虽然到处都一个味。",
      46, 34, 4, 9, 2, 420, 0, false,
      Col(20, 24, 18), Col(39, 43, 34), Col(143, 193, 82), 0 },
    { "腐烂超市", "ROT MART", "中风险",
      "货架之间全是枪线。值钱的东西不少，值钱的命没有。生鲜区已经不生鲜了。",
      60, 44, 7, 13, 2, 420, 1, false,
      Col(24, 20, 17), Col(44, 38, 31), Col(216, 148, 68), 800 },
    { "尸油工厂", "TALLOW WORKS", "高风险",
      "蛆王的地盘。要么带着腺体爬出去，要么变成下一桶尸油。通风管里全是咀嚼声。",
      72, 56, 10, 17, 2, 360, 3, true,
      Col(23, 17, 20), Col(43, 33, 36), Col(218, 91, 67), 2200 },
};

const MissionDef MISSIONS[5] = {
    { "初次蠕动", "成功撤离 1 次。活着回来，比什么都重要。", MM_EXTRACTS, 1, 600, WPN_NONE, 0, ARM_NONE, 0 },
    { "下水道清道夫", "累计击杀 12 只敌对蛆。它们也只是一些想发财的蛆。", MM_KILLS, 12, 1200, WPN_SMG, 1, ARM_NONE, 0 },
    { "满载而归", "累计带出价值 8,000 的垃圾。在蛆的世界，垃圾就是钱。", MM_LOOT, 8000, 2000, WPN_NONE, 0, ARM_LIGHT, 1 },
    { "工厂深潜", "在尸油工厂成功撤离 1 次。那里的空气会记住你的味道。", MM_FACTORY, 1, 3000, WPN_RIFLE, 1, ARM_NONE, 0 },
    { "弑王", "击杀蛆王 1 次。成为新的传说，或者新的肥料。", MM_BOSS, 1, 5000, WPN_NONE, 0, ARM_CARAPACE, 1 },
};

const EnemyDef ENEMIES[4] = {
    { "拾荒蛆", 45, 70.f, 360.f, 6.f, 1.35f, .66f, 420.f, 11.f, 0, false, Col(201, 194, 166) },
    { "净化蛆", 72, 86.f, 470.f, 9.f, .72f, .78f, 560.f, 12.f, 12, false, Col(135, 180, 171) },
    { "暴食蛆", 125, 62.f, 360.f, 14.f, 1.9f, .54f, 300.f, 16.f, 10, false, Col(183, 128, 92) },
    { "蛆王", 560, 66.f, 640.f, 18.f, 1.3f, .66f, 460.f, 27.f, 26, true, Col(214, 112, 84) },
};

const char* ammoName(AmmoId a) {
    static const char* n[AMMO_COUNT] = { "9mm 腐弹", "12号骨弹", "5.56 酸弹", "7.62 骨芯弹" };
    return n[(int)a];
}
const char* ammoShort(AmmoId a) {
    static const char* n[AMMO_COUNT] = { "9mm", "12G", "5.56", "7.62" };
    return n[(int)a];
}
int ammoPrice(AmmoId a) {
    static const int p[AMMO_COUNT] = { 110, 120, 260, 340 };
    return p[(int)a];
}
int ammoBox(AmmoId a) {
    static const int c[AMMO_COUNT] = { 30, 12, 20, 20 };
    return c[(int)a];
}
float ammoUnitWeight(AmmoId a) {
    static const float w[AMMO_COUNT] = { .012f, .05f, .015f, .022f };
    return w[(int)a];
}
ArmorSlot armorSlot(ArmorId a) {
    if (a < ARM_CAP) return ASL_BODY;
    if (a <= ARM_TANK) return ASL_HELMET;
    if (a < ARM_LIGHT) return ASL_BODY;
    if (a <= ARM_WING) return ASL_BODY;
    if (a < ARM_POUCH) return ASL_RIG;
    if (a <= ARM_PLATE_RIG) return ASL_RIG;
    return ASL_BODY;
}
Col rarityColor(int rarity) {
    switch (rarity) {
        case 1: return Col(112, 190, 132);
        case 2: return Col(96, 174, 220);
        case 3: return Col(196, 132, 224);
        case 4: return Col(238, 176, 70);
        default: return Col(158, 158, 140);
    }
}
const char* rarityName(int rarity) {
    switch (rarity) {
        case 1: return "常见";
        case 2: return "稀有";
        case 3: return "史诗";
        case 4: return "传说";
        default: return "垃圾";
    }
}
int mapCount() { return 3; }
