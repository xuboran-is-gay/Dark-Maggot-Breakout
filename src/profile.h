#pragma once
#include "core.h"
#include "data.h"
#include <unordered_map>

struct ItemStack {
    int id = 0;
    int count = 0;
};

struct Settings {
    float volume = .8f;
    bool muted = false;
    bool shake = true;
    bool fullscreen = false;
    bool autofire = true;
};

// Persistent save. Everything the player owns outside a raid lives here.
struct Profile {
    int coins = 1800;
    WeaponId weapon = WPN_PISTOL;
    ArmorId armor = ARM_NONE;
    int backpack = 0;
    int ammo[AMMO_COUNT] = {24, 0, 0};
    int bandages = 2;
    int medkits = 0;

    // Permanent unlocks. Gear is only lost when carried into a raid and dies.
    bool weaponOwned[WPN_COUNT] = {false, true, false, false, false};
    bool armorOwned[ARM_COUNT] = {true, false, false, false};
    bool bagOwned[4] = {true, false, false, false};

    std::vector<ItemStack> stash;
    int extracted = 0;
    int kills = 0;
    int lootValue = 0;
    int factoryExtracts = 0;
    int bossKills = 0;
    int raids = 0;
    int deaths = 0;
    int xp = 0;

    bool missionClaimed[5] = {false, false, false, false, false};
    Settings settings;

    int level() const { return 1 + xp / 700; }
    int xpIntoLevel() const { return xp % 700; }
    int xpForNext() const { return 700; }
    int backpackCapacity() const { return BACKPACKS[std::clamp(backpack, 0, 3)].capacity; }
    int gearValue() const;   // value of the gear + supplies currently carried
    bool ownsWeapon(WeaponId w) const { return (int)w >= 0 && (int)w < WPN_COUNT && weaponOwned[(int)w]; }
    bool ownsArmor(ArmorId a) const { return (int)a >= 0 && (int)a < ARM_COUNT && armorOwned[(int)a]; }
    bool ownsBackpack(int b) const { return b >= 0 && b < 4 && bagOwned[b]; }

    int stashCount() const;
    int stashWeight() const;
    int stashValue() const;
    int countOf(ItemId id) const;
    int addItem(ItemId id, int count = 1);
    bool removeItem(ItemId id, int count = 1);
    int sellStack(int index, int count = -1);       // returns coins gained
    int sellAll();                                   // sells everything in the stash
    int sellJunk();                                  // sells rarity <= 1, returns coins
    void loseGearOnDeath();
    int missionProgress(int index) const;
    bool claimMission(int index);
    void addXp(int amount);
    std::string gearSummary() const;

    bool load(const std::string& path);
    bool save(const std::string& path) const;
};

std::string itemSummary(const std::vector<ItemStack>& items);
