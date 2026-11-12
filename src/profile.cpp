#include "profile.h"
#include <fstream>
#include <sstream>
#include <cstdio>

int Profile::gearValue() const {
    int v = WEAPONS[std::clamp((int)weapon, 0, (int)WPN_COUNT - 1)].price;
    v += ARMORS[std::clamp((int)armor, 0, (int)ARM_COUNT - 1)].price;
    v += BACKPACKS[std::clamp(backpack, 0, 3)].price;
    for (int i = 0; i < AMMO_COUNT; ++i) {
        int box = ammoBox((AmmoId)i);
        if (box > 0) v += (int)std::lround((double)ammo[i] * ammoPrice((AmmoId)i) / box);
    }
    v += bandages * 60 + medkits * 420;
    return v;
}

int Profile::stashCount() const {
    int n = 0;
    for (auto& s : stash) n += s.count;
    return n;
}

int Profile::stashWeight() const {
    float w = 0.f;
    for (auto& s : stash) if (s.id >= 0 && s.id < IT_COUNT) w += ITEMS[s.id].weight * s.count;
    return (int)std::lround(w * 1000.f);
}

int Profile::stashValue() const {
    int v = 0;
    for (auto& s : stash) if (s.id >= 0 && s.id < IT_COUNT) v += ITEMS[s.id].value * s.count;
    return v;
}

int Profile::countOf(ItemId id) const {
    for (auto& s : stash) if (s.id == (int)id) return s.count;
    return 0;
}

int Profile::addItem(ItemId id, int count) {
    if (count <= 0) return 0;
    for (auto& s : stash) if (s.id == (int)id) { s.count += count; return count; }
    stash.push_back({(int)id, count});
    return count;
}

bool Profile::removeItem(ItemId id, int count) {
    if (count <= 0) return true;
    for (size_t i = 0; i < stash.size(); ++i) {
        if (stash[i].id == (int)id) {
            if (stash[i].count < count) return false;
            stash[i].count -= count;
            if (stash[i].count <= 0) stash.erase(stash.begin() + (long)i);
            return true;
        }
    }
    return false;
}

int Profile::sellStack(int index, int count) {
    if (index < 0 || index >= (int)stash.size()) return 0;
    ItemStack& s = stash[(size_t)index];
    if (s.id < 0 || s.id >= IT_COUNT) return 0;
    int n = count <= 0 ? s.count : std::min(count, s.count);
    int gained = ITEMS[s.id].value * n;
    coins += gained;
    s.count -= n;
    if (s.count <= 0) stash.erase(stash.begin() + index);
    return gained;
}

int Profile::sellAll() {
    int gained = 0;
    for (auto& s : stash) if (s.id >= 0 && s.id < IT_COUNT) gained += ITEMS[s.id].value * s.count;
    coins += gained;
    stash.clear();
    return gained;
}

int Profile::sellJunk() {
    int gained = 0;
    for (auto& s : stash) if (s.id >= 0 && s.id < IT_COUNT && ITEMS[s.id].rarity <= 0)
        gained += ITEMS[s.id].value * s.count;
    coins += gained;
    for (size_t i = 0; i < stash.size();) {
        if (stash[i].id >= 0 && stash[i].id < IT_COUNT && ITEMS[stash[i].id].rarity <= 0)
            stash.erase(stash.begin() + (long)i);
        else ++i;
    }
    return gained;
}

void Profile::loseGearOnDeath() {
    if (weapon != WPN_PISTOL && weapon != WPN_NONE) weaponOwned[(int)weapon] = false;
    if (armor != ARM_NONE) armorOwned[(int)armor] = false;
    if (backpack != 0) bagOwned[backpack] = false;
    weapon = WPN_PISTOL;
    armor = ARM_NONE;
    backpack = 0;
    ammo[0] = 18; ammo[1] = 0; ammo[2] = 0;
    bandages = 1;
    medkits = 0;
}

int Profile::missionProgress(int index) const {
    if (index < 0 || index >= 5) return 0;
    switch (MISSIONS[index].metric) {
        case MM_EXTRACTS: return extracted;
        case MM_KILLS: return kills;
        case MM_LOOT: return lootValue;
        case MM_FACTORY: return factoryExtracts;
        case MM_BOSS: return bossKills;
    }
    return 0;
}

bool Profile::claimMission(int index) {
    if (index < 0 || index >= 5 || missionClaimed[index]) return false;
    const MissionDef& m = MISSIONS[index];
    if (missionProgress(index) < m.target) return false;
    missionClaimed[index] = true;
    coins += m.rewardCoins;
    if (m.weapon != WPN_NONE) {
        if ((int)weapon < (int)m.weapon) {
            weapon = m.weapon;
            weaponOwned[(int)m.weapon] = true;
        } else coins += WEAPONS[m.weapon].price;
    }
    if (m.armor != ARM_NONE) {
        if ((int)armor < (int)m.armor) {
            armor = m.armor;
            armorOwned[(int)m.armor] = true;
        } else coins += ARMORS[m.armor].price;
    }
    return true;
}

void Profile::addXp(int amount) {
    if (amount <= 0) return;
    xp += amount;
}

std::string Profile::gearSummary() const {
    return std::string(WEAPONS[weapon].name) + " · " + ARMORS[armor].name + " · " + BACKPACKS[std::clamp(backpack, 0, 3)].name;
}

bool Profile::save(const std::string& path) const {
    std::string tmp = path + ".tmp";
    std::ofstream f(tmp, std::ios::trunc);
    if (!f) return false;
    f << "MAGGOT_SAVE 4\n";
    f << "coins " << coins << "\n";
    f << "gear " << (int)weapon << " " << (int)armor << " " << backpack << "\n";
    f << "ammo " << ammo[0] << " " << ammo[1] << " " << ammo[2] << "\n";
    f << "meds " << bandages << " " << medkits << "\n";
    f << "stats " << extracted << " " << kills << " " << lootValue << " " << factoryExtracts
      << " " << bossKills << " " << raids << " " << deaths << " " << xp << "\n";
    f << "settings " << settings.volume << " " << settings.muted << " " << settings.shake << " " << settings.fullscreen << "\n";
    f << "autofire " << (settings.autofire ? 1 : 0) << "\n";
    f << "missions";
    for (int i = 0; i < 5; ++i) f << " " << (missionClaimed[i] ? 1 : 0);
    f << "\n";
    f << "owned";
    for (bool v : weaponOwned) f << " " << (v ? 1 : 0);
    for (bool v : armorOwned) f << " " << (v ? 1 : 0);
    for (bool v : bagOwned) f << " " << (v ? 1 : 0);
    f << "\n";
    for (auto& s : stash) if (s.count > 0) f << "item " << s.id << " " << s.count << "\n";
    f.close();
    if (!f) return false;
    std::remove(path.c_str());
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

bool Profile::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    std::string header;
    int version = 0;
    if (!(f >> header >> version) || header != "MAGGOT_SAVE" || version < 1) return false;
    std::string key;
    Profile def;
    *this = def;
    stash.clear();
    while (f >> key) {
        if (key == "coins") f >> coins;
        else if (key == "gear") { int w, a; f >> w >> a >> backpack; weapon = (WeaponId)std::clamp(w, 0, (int)WPN_COUNT - 1); armor = (ArmorId)std::clamp(a, 0, (int)ARM_COUNT - 1); }
        else if (key == "ammo") f >> ammo[0] >> ammo[1] >> ammo[2];
        else if (key == "meds") f >> bandages >> medkits;
        else if (key == "stats") f >> extracted >> kills >> lootValue >> factoryExtracts >> bossKills >> raids >> deaths >> xp;
        else if (key == "settings") f >> settings.volume >> settings.muted >> settings.shake >> settings.fullscreen;
        else if (key == "autofire") { int v = 1; f >> v; settings.autofire = v != 0; }
        else if (key == "missions") for (int i = 0; i < 5; ++i) { int v = 0; f >> v; missionClaimed[i] = v != 0; }
        else if (key == "owned") {
            for (bool& v : weaponOwned) { int n = 0; f >> n; v = n != 0; }
            for (bool& v : armorOwned) { int n = 0; f >> n; v = n != 0; }
            for (bool& v : bagOwned) { int n = 0; f >> n; v = n != 0; }
        }
        else if (key == "item") { int id, n; f >> id >> n; if (id >= 0 && id < IT_COUNT && n > 0) addItem((ItemId)id, n); }
        else { std::string rest; std::getline(f, rest); }
    }
    coins = std::clamp(coins, 0, 100000000);
    backpack = std::clamp(backpack, 0, 3);
    if (version < 4) {
        weaponOwned[weapon] = true;
        armorOwned[armor] = true;
        bagOwned[backpack] = true;
    }
    // Starters and the currently equipped set must always be usable.
    weaponOwned[WPN_PISTOL] = true;
    armorOwned[ARM_NONE] = true;
    bagOwned[0] = true;
    if (weapon >= 0 && weapon < WPN_COUNT) weaponOwned[(int)weapon] = true;
    if (armor >= 0 && armor < ARM_COUNT) armorOwned[(int)armor] = true;
    bagOwned[backpack] = true;
    for (int i = 0; i < AMMO_COUNT; ++i) ammo[i] = std::clamp(ammo[i], 0, 99999);
    bandages = std::clamp(bandages, 0, 999);
    medkits = std::clamp(medkits, 0, 999);
    settings.volume = clampF(settings.volume, 0.f, 1.f);
    return true;
}

std::string itemSummary(const std::vector<ItemStack>& items) {
    std::string out;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) out += " · ";
        out += ITEMS[std::clamp(items[i].id, 0, IT_COUNT - 1)].name;
        if (items[i].count > 1) out += "×" + std::to_string(items[i].count);
    }
    return out;
}
