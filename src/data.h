#pragma once
#include "core.h"

enum WeaponId {
    WPN_NONE = 0, WPN_PISTOL, WPN_SMG, WPN_SHOTGUN, WPN_RIFLE, WPN_COUNT
};
enum AmmoId { AMMO_9MM = 0, AMMO_12G, AMMO_556, AMMO_COUNT };
enum ArmorId { ARM_NONE = 0, ARM_LIGHT, ARM_PLATE, ARM_CARAPACE, ARM_COUNT };

struct WeaponDef {
    WeaponId id;
    const char* name;
    const char* shortName;
    const char* desc;
    AmmoId ammo;
    int damage;
    int pellets;
    float rpm;
    float spread;      // degrees
    float range;       // pixels
    int mag;
    float reload;
    int price;
    float weight;
    Col accent;
};

struct ArmorDef {
    ArmorId id;
    const char* name;
    const char* desc;
    int armor;
    float reduction;
    int price;
    float weight;
    Col accent;
};

struct BackpackDef {
    const char* name;
    const char* desc;
    int capacity;     // in "kg" (game units)
    int price;
    Col accent;
};

enum ItemKind { IK_JUNK, IK_FOOD, IK_TECH, IK_ORGANIC, IK_TREASURE, IK_AMMO, IK_MED };
enum ItemId {
    IT_SCREW = 0, IT_BREAD, IT_OIL, IT_PHONE, IT_MUSHROOM, IT_PROTEIN,
    IT_CHIP, IT_EGG, IT_GLAND, IT_DOGTAG, IT_EYE, IT_HAIR,
    IT_AMMO9, IT_AMMO12G, IT_AMMO556, IT_BANDAGE, IT_MEDKIT, IT_COUNT
};

struct ItemDef {
    const char* name;
    const char* desc;
    float weight;
    int value;
    int rarity;       // 0 common .. 4 legendary
    ItemKind kind;
    Col color;
};

struct MapDef {
    const char* name;
    const char* code;
    const char* risk;
    const char* desc;
    int w, h;
    int enemies;
    int containers;
    int extracts;
    int timer;
    int unlockExtracts;
    bool boss;
    Col floor;
    Col wall;
    Col accent;
    int gearReq;   // minimum loadout value required to deploy; 0 = no threshold
};

enum MissionMetric { MM_EXTRACTS, MM_KILLS, MM_LOOT, MM_FACTORY, MM_BOSS };

struct MissionDef {
    const char* name;
    const char* desc;
    MissionMetric metric;
    int target;
    int rewardCoins;
    WeaponId weapon;
    int weaponCount;
    ArmorId armor;
    int armorCount;
};

struct EnemyDef {
    const char* name;
    int hp;
    float speed;
    float vision;
    float damage;
    float interval;
    float accuracy;
    float range;
    float radius;
    int armor;
    bool boss;
    Col color;
};

extern const WeaponDef WEAPONS[WPN_COUNT];
extern const ArmorDef ARMORS[ARM_COUNT];
extern const BackpackDef BACKPACKS[4];
extern const ItemDef ITEMS[IT_COUNT];
extern const MapDef MAPS[3];
extern const MissionDef MISSIONS[5];
extern const EnemyDef ENEMIES[4];

enum EnemyType { EN_SCAV = 0, EN_MERC, EN_BRUTE, EN_KING };

const char* ammoName(AmmoId a);
const char* ammoShort(AmmoId a);
int ammoPrice(AmmoId a);
int ammoBox(AmmoId a);
float ammoUnitWeight(AmmoId a);
Col rarityColor(int rarity);
const char* rarityName(int rarity);
int mapCount();
