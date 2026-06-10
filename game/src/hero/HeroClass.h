#pragma once
#include <string>
#include <vector>
#include "../hero/Hero.h"

// ── Hero specialty — unique passive ───────────────────────────────────────────
enum class SpecialtyType
{
    // Holy Order
    HeresyDetection,    // Inquisitor — nullifies one enemy spell per battle
    LastRites,          // Confessor — deaths near hero double Inspiration
    Veteran,            // Crusader — units +1 stats per previous battle
    BloodPenance,       // Flagellant Marshal — hero loses HP, Desperation faster

    // Bloodsworn
    Feast,              // Blood Prince — hero drains HP from one unit per round
    Exsanguinate,       // Crimson Mage — one Blood spell free per battle
    Swarm,              // Thrall Master — Fledglings start at ascension threshold
    Predator,           // Assassin Lord — permanent Attack per hero killed

    // Thornkin
    WildGrowth,         // Beastcaller — dead companions respawn as spirit
    Overgrowth,         // Pathfinder — place 3 forest tiles before battle
    LightningRod,       // Stormbark — first enemy spell redirected back
    Harmony,            // Warsinger — all pairs +1 stats while hero lives

    // Eternal Empire
    SoulHarvest,        // Death Herald — enemy kills restore hero HP
    EternalLegion,      // Iron General — reraised units keep formation bonuses
    Phylactery,         // Lich — hero respawns next battle at half stats
    NegotiatedWeakness, // Grave Diplomat — reveals enemy specialty before battle

    // Crimson Wardens
    CoordinatedStrike,  // Warden Captain — marked target bonus from all attackers
    Elixir,             // Blood Sage — once per battle fully heal one unit
    BloodWeb,           // Oathmaster — linked units heal on any kill
    BloodScent,         // Inquisitor Hunter — always knows Bloodsworn hero location

    // Voidkin
    VoidLink,           // Void Weaver — possession spreads on death
    GhostWalk,          // Shadow Stalker — hero invisible on world map
    BlightAura,         // Blight Caller — corrupts sacred terrain passively
    Wither,             // Fell Druid — enemies lose 1 stat per round in aura

    // Iron Assembly
    Efficient,          // Master Engineer — units cost 20% less to craft
    IronDiscipline,     // Warlord Mechanic — constructs immune to morale/fear
    Recycler,           // Salvage Lord — salvage buffs permanent across battles
    LivingRune,         // Runesmith — one unit gets permanent stat increase per battle

    // Amalgamate
    RapidEvolution,     // Evolver — adaptations after 1 hit
    Collective,         // Hive Controller — transfer adaptation between units
    Infestation,        // Flesh Architect — flesh terrain spreads every round
    Apex,               // Apex Hunter — hero starts with all Evolved's adaptations

    // Convergence
    Radiance,           // Lightbringer — mirror lasts 7 rounds
    Covenant,           // Oathbound — linked units share mirrored buffs
    PredatorMirror,     // Shadowlord — mirror activates instantly
    Corruption,         // Voidcaller — mirrored terrain lingers
    Synthesis,          // Ironweaver — hold two mirrors simultaneously
    AdaptationMirror,   // Fleshbinder — units gain adaptation per mirrored battle

    None
};

// ── Hero class definition ──────────────────────────────────────────────────────
struct HeroClassDef
{
    int           id          = 0;
    std::string   name;
    FactionId     faction     = FactionId::None;
    SpecialtyType specialty   = SpecialtyType::None;
    std::string   specialtyDesc;

    // Primary casting stats this class scales
    bool scalesAttack      = false;
    bool scalesLightPower  = false;
    bool scalesBloodPower  = false;
    bool scalesDeathPower  = false;
    bool scalesNaturePower = false;
    bool scalesForgePower  = false;
    bool scalesFleshPower  = false;

    // Skill pool — IDs of skills this class can offer on level up
    std::vector<int> skillPool;

    // Stat growth per level (base values, RNG picks from these)
    int attackGrowth  = 1;   // +1 attack every N levels
    int defenseGrowth = 1;
    int manaGrowth    = 1;
};

// ── Class registry ─────────────────────────────────────────────────────────────
class HeroClassRegistry
{
public:
    void init();

    const HeroClassDef* getClass(int id)  const;
    std::vector<const HeroClassDef*> getClassesForFaction(FactionId f) const;

    const std::vector<HeroClassDef>& classes() const { return m_classes; }

private:
    std::vector<HeroClassDef> m_classes;
};
