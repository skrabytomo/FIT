#include "SaveLoad.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

// ── HexCoord serialization ────────────────────────────────────────────────────
static json coordToJson(int q, int r)  { return {{"q", q}, {"r", r}}; }
static void jsonToCoord(const json& j, int& q, int& r)
{
    q = j.at("q").get<int>();
    r = j.at("r").get<int>();
}

// ── TileSave ──────────────────────────────────────────────────────────────────
static json tileToJson(const TileSave& t)
{
    return {
        {"q", t.q}, {"r", t.r},
        {"ter", t.terrain},
        {"exp", t.explored},
        {"vis", t.visible},
        {"hid", t.heroId},
        {"tid", t.townId},
        {"rid", t.resourceId},
    };
}
static TileSave tileFromJson(const json& j)
{
    TileSave t;
    t.q          = j.at("q").get<int>();
    t.r          = j.at("r").get<int>();
    t.terrain    = j.at("ter").get<int>();
    t.explored   = j.at("exp").get<bool>();
    t.visible    = j.at("vis").get<bool>();
    t.heroId     = j.value("hid", 0u);
    t.townId     = j.value("tid", 0u);
    t.resourceId = j.value("rid", 0u);
    return t;
}

// ── DwellingSave ──────────────────────────────────────────────────────────────
static json dwellingToJson(const DwellingSave& d)
{
    return {{"bid", d.buildingId}, {"tier", d.tier}, {"path", d.path},
            {"avail", d.available}, {"accum", d.accumulated}};
}
static DwellingSave dwellingFromJson(const json& j)
{
    DwellingSave d;
    d.buildingId  = j.at("bid").get<int>();
    d.tier        = j.at("tier").get<int>();
    d.path        = j.at("path").get<int>();
    d.available   = j.at("avail").get<int>();
    d.accumulated = j.at("accum").get<int>();
    return d;
}

// ── HeroSave ──────────────────────────────────────────────────────────────────
static json heroToJson(const HeroSave& h)
{
    return {
        {"id", h.id}, {"name", h.name},
        {"faction", h.faction}, {"classId", h.classId},
        {"posQ", h.posQ}, {"posR", h.posR},
        {"movePool", h.movePool}, {"maxMove", h.maxMove},
        {"level", h.level}, {"attack", h.attack}, {"defense", h.defense},
        {"visionRange", h.visionRange},
        {"xp", h.xp}, {"xpToNext", h.xpToNext},
        {"hp", h.hp}, {"maxHp", h.maxHp},
        {"mana", h.mana}, {"maxMana", h.maxMana},
        {"lightPower", h.lightPower}, {"bloodPower", h.bloodPower},
        {"deathPower", h.deathPower}, {"naturePower", h.naturePower},
        {"forgePower", h.forgePower}, {"fleshPower", h.fleshPower},
    };
}
static HeroSave heroFromJson(const json& j)
{
    HeroSave h;
    h.id          = j.at("id").get<uint32_t>();
    h.name        = j.at("name").get<std::string>();
    h.faction     = j.at("faction").get<int>();
    h.classId     = j.at("classId").get<int>();
    h.posQ        = j.at("posQ").get<int>();
    h.posR        = j.at("posR").get<int>();
    h.movePool    = j.at("movePool").get<int>();
    h.maxMove     = j.at("maxMove").get<int>();
    h.level       = j.at("level").get<int>();
    h.attack      = j.at("attack").get<int>();
    h.defense     = j.at("defense").get<int>();
    h.visionRange = j.at("visionRange").get<int>();
    h.xp          = j.value("xp", 0);
    h.xpToNext    = j.value("xpToNext", 100);
    h.hp          = j.value("hp", 100);
    h.maxHp       = j.value("maxHp", 100);
    h.mana        = j.value("mana", 10);
    h.maxMana     = j.value("maxMana", 10);
    h.lightPower  = j.value("lightPower", 0);
    h.bloodPower  = j.value("bloodPower", 0);
    h.deathPower  = j.value("deathPower", 0);
    h.naturePower = j.value("naturePower", 0);
    h.forgePower  = j.value("forgePower", 0);
    h.fleshPower  = j.value("fleshPower", 0);
    return h;
}

// ── TownSave ──────────────────────────────────────────────────────────────────
static json townToJson(const TownSave& t)
{
    json dwArr = json::array();
    for (auto& d : t.dwellings) dwArr.push_back(dwellingToJson(d));

    json bldArr = json::array();
    for (int b : t.builtBuildings) bldArr.push_back(b);

    json incArr = json::array();
    for (int a : t.weeklyIncomeAmounts) incArr.push_back(a);

    return {
        {"id", t.id}, {"name", t.name},
        {"faction", t.faction},
        {"posQ", t.posQ}, {"posR", t.posR},
        {"ownerId", t.ownerId},
        {"buildings", bldArr},
        {"dwellings", dwArr},
        {"fortHP", t.fortHP}, {"fortMaxHP", t.fortMaxHP},
        {"income", incArr},
    };
}
static TownSave townFromJson(const json& j)
{
    TownSave t;
    t.id      = j.at("id").get<uint32_t>();
    t.name    = j.at("name").get<std::string>();
    t.faction = j.at("faction").get<int>();
    t.posQ    = j.at("posQ").get<int>();
    t.posR    = j.at("posR").get<int>();
    t.ownerId = j.at("ownerId").get<uint32_t>();
    t.fortHP    = j.value("fortHP", 0);
    t.fortMaxHP = j.value("fortMaxHP", 0);

    for (auto& b : j.at("buildings")) t.builtBuildings.push_back(b.get<int>());
    for (auto& d : j.at("dwellings")) t.dwellings.push_back(dwellingFromJson(d));

    if (j.contains("income")) {
        int i = 0;
        for (auto& v : j.at("income")) {
            if (i < RESOURCE_COUNT) t.weeklyIncomeAmounts[i++] = v.get<int>();
        }
    }
    return t;
}

// ── Public API ────────────────────────────────────────────────────────────────
bool SaveLoad::saveGame(const std::string& path, const GameSaveData& data)
{
    try {
        json j;
        j["version"]  = data.version;
        j["day"]      = data.day;
        j["week"]     = data.week;
        j["mapRadius"]   = data.mapRadius;
        j["mapSizeEnum"] = data.mapSizeEnum;

        // Resources
        json resArr = json::array();
        for (int v : data.resourceAmounts) resArr.push_back(v);
        j["resources"] = resArr;

        // Heroes
        json heroArr = json::array();
        for (auto& h : data.heroes) heroArr.push_back(heroToJson(h));
        j["heroes"] = heroArr;

        // Towns
        json townArr = json::array();
        for (auto& t : data.towns) townArr.push_back(townToJson(t));
        j["towns"] = townArr;

        // Tiles
        json tileArr = json::array();
        for (auto& t : data.tiles) tileArr.push_back(tileToJson(t));
        j["tiles"] = tileArr;

        std::ofstream f(path);
        if (!f.is_open()) return false;
        f << j.dump(2);
        return true;
    }
    catch (...) {
        return false;
    }
}

bool SaveLoad::loadGame(const std::string& path, GameSaveData& out)
{
    try {
        std::ifstream f(path);
        if (!f.is_open()) return false;
        json j;
        f >> j;

        out.version     = j.value("version", 1);
        out.day         = j.value("day", 1);
        out.week        = j.value("week", 1);
        out.mapRadius   = j.value("mapRadius", 16);
        out.mapSizeEnum = j.value("mapSizeEnum", 0);

        out.resourceAmounts.fill(0);
        if (j.contains("resources")) {
            int i = 0;
            for (auto& v : j.at("resources")) {
                if (i < RESOURCE_COUNT) out.resourceAmounts[i++] = v.get<int>();
            }
        }

        out.heroes.clear();
        if (j.contains("heroes"))
            for (auto& jh : j.at("heroes")) out.heroes.push_back(heroFromJson(jh));

        out.towns.clear();
        if (j.contains("towns"))
            for (auto& jt : j.at("towns")) out.towns.push_back(townFromJson(jt));

        out.tiles.clear();
        if (j.contains("tiles"))
            for (auto& jt : j.at("tiles")) out.tiles.push_back(tileFromJson(jt));

        return true;
    }
    catch (...) {
        return false;
    }
}

// ── Pack live game state into SaveData ────────────────────────────────────────
GameSaveData SaveLoad::packState(const HexMap& map,
                                 const std::vector<Hero>& heroes,
                                 const std::vector<Town>& towns,
                                 const Resources& playerRes,
                                 int day, int week,
                                 MapSize mapSize)
{
    GameSaveData save;
    save.day         = day;
    save.week        = week;
    save.mapRadius   = map.radius();
    save.mapSizeEnum = static_cast<int>(mapSize);
    save.resourceAmounts = playerRes.amounts;

    // Heroes
    for (auto& h : heroes) {
        HeroSave hs;
        hs.id          = h.id;
        hs.name        = h.name;
        hs.faction     = static_cast<int>(h.faction);
        hs.classId     = h.classId;
        hs.posQ        = h.pos.q;
        hs.posR        = h.pos.r;
        hs.movePool    = h.movePool;
        hs.maxMove     = h.maxMove;
        hs.level       = h.level;
        hs.attack      = h.attack;
        hs.defense     = h.defense;
        hs.visionRange = h.visionRange;
        // Extended stats omitted from Hero base struct — defaults
        hs.xp          = 0;
        hs.xpToNext    = 100;
        hs.hp          = 100;
        hs.maxHp       = 100;
        hs.mana        = 10;
        hs.maxMana     = 10;
        save.heroes.push_back(hs);
    }

    // Towns
    for (auto& t : towns) {
        TownSave ts;
        ts.id         = t.id;
        ts.name       = t.name;
        ts.faction    = static_cast<int>(t.faction);
        ts.posQ       = t.pos.q;
        ts.posR       = t.pos.r;
        ts.ownerId    = t.ownerId;
        ts.builtBuildings = t.builtBuildings;
        ts.fortHP     = t.fortHP;
        ts.fortMaxHP  = t.fortMaxHP;
        ts.weeklyIncomeAmounts = t.weeklyIncome.amounts;
        for (auto& d : t.dwellings) {
            DwellingSave ds;
            ds.buildingId  = d.buildingId;
            ds.tier        = d.tier;
            ds.path        = static_cast<int>(d.path);
            ds.available   = d.available;
            ds.accumulated = d.accumulated;
            ts.dwellings.push_back(ds);
        }
        save.towns.push_back(ts);
    }

    // Tiles (fog of war + entity references)
    for (auto c : map.coords()) {
        const HexTile* tile = map.getTile(c);
        if (!tile) continue;
        TileSave ts;
        ts.q          = c.q;
        ts.r          = c.r;
        ts.terrain    = static_cast<int>(tile->terrain);
        ts.explored   = tile->explored;
        ts.visible    = tile->visible;
        ts.heroId     = tile->heroId;
        ts.townId     = tile->townId;
        ts.resourceId = tile->resourceId;
        save.tiles.push_back(ts);
    }

    return save;
}

// ── Unpack SaveData into live game state ──────────────────────────────────────
void SaveLoad::unpackState(const GameSaveData& save,
                           HexMap& map,
                           std::vector<Hero>& heroes,
                           std::vector<Town>& towns,
                           Resources& playerRes,
                           int& day, int& week)
{
    day  = save.day;
    week = save.week;
    playerRes.amounts = save.resourceAmounts;

    // Restore tile fog/entity state (map must already be created with correct size)
    for (auto& ts : save.tiles) {
        HexTile* tile = map.getTile({ts.q, ts.r});
        if (!tile) continue;
        tile->terrain    = static_cast<Terrain>(ts.terrain);
        tile->explored   = ts.explored;
        tile->visible    = ts.visible;
        tile->heroId     = ts.heroId;
        tile->townId     = ts.townId;
        tile->resourceId = ts.resourceId;
    }

    // Restore heroes
    heroes.clear();
    for (auto& hs : save.heroes) {
        Hero h;
        h.id          = hs.id;
        h.name        = hs.name;
        h.faction     = static_cast<FactionId>(hs.faction);
        h.classId     = hs.classId;
        h.pos         = {hs.posQ, hs.posR};
        h.movePool    = hs.movePool;
        h.maxMove     = hs.maxMove;
        h.level       = hs.level;
        h.attack      = hs.attack;
        h.defense     = hs.defense;
        h.visionRange = hs.visionRange;
        heroes.push_back(h);
    }

    // Restore towns
    towns.clear();
    for (auto& ts : save.towns) {
        Town t;
        t.id         = ts.id;
        t.name       = ts.name;
        t.faction    = static_cast<FactionId>(ts.faction);
        t.pos        = {ts.posQ, ts.posR};
        t.ownerId    = ts.ownerId;
        t.builtBuildings = ts.builtBuildings;
        t.fortHP     = ts.fortHP;
        t.fortMaxHP  = ts.fortMaxHP;
        t.weeklyIncome.amounts = ts.weeklyIncomeAmounts;
        for (auto& ds : ts.dwellings) {
            DwellingState d;
            d.buildingId  = ds.buildingId;
            d.tier        = ds.tier;
            d.path        = static_cast<UpgradePath>(ds.path);
            d.available   = ds.available;
            d.accumulated = ds.accumulated;
            t.dwellings.push_back(d);
        }
        towns.push_back(t);
    }
}
