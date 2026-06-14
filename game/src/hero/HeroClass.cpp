#include "HeroClass.h"
#include "Skills.h"
#include <algorithm>

void HeroClassRegistry::init()
{
    m_classes.clear();

    int id = 1;

    auto addClass = [&](FactionId f, const char* name, SpecialtyType spec,
                        const char* specDesc, bool atk, bool lp, bool bp,
                        bool dp, bool np, bool fp, bool flp,
                        std::vector<int> pool) {
        HeroClassDef c;
        c.id = id++; c.name = name; c.faction = f;
        c.specialty = spec; c.specialtyDesc = specDesc;
        c.scalesAttack = atk; c.scalesLightPower = lp;
        c.scalesBloodPower = bp; c.scalesDeathPower = dp;
        c.scalesNaturePower = np; c.scalesForgePower = fp;
        c.scalesFleshPower = flp;
        c.skillPool = pool;
        m_classes.push_back(c);
    };

    using F = FactionId;
    using S = SpecialtyType;

    // ── HOLY ORDER ─────────────────────────────────────────────────────────────
    addClass(F::HolyOrder, "Inquisitor", S::HeresyDetection,
        "Nullifies one enemy spell per battle",
        false, true, false, false, false, false, false,
        {SID::LIGHT_MAGIC, SID::OFFENSE, SID::DEFENSE_SKILL, SID::DESPERATION, SID::LEADERSHIP});

    addClass(F::HolyOrder, "Confessor", S::LastRites,
        "Enemy kills charge nearby Holy allies' Desperation meter by +20",
        false, true, false, false, false, false, false,
        {SID::LIGHT_MAGIC, SID::FIRST_AID, SID::INSPIRATION, SID::LEADERSHIP, SID::SCOUTING});

    addClass(F::HolyOrder, "Crusader", S::Veteran,
        "Units gain +1 Attack and Defense per previous battle this campaign",
        true, false, false, false, false, false, false,
        {SID::OFFENSE, SID::DEFENSE_SKILL, SID::LEADERSHIP, SID::TACTICS, SID::LOGISTICS});

    addClass(F::HolyOrder, "Flagellant Marshal", S::BloodPenance,
        "All units +2 ATK at battle start; hero loses 5 HP per round from round 2",
        true, true, false, false, false, false, false,
        {SID::DESPERATION, SID::INSPIRATION, SID::OFFENSE, SID::LIGHT_MAGIC, SID::TACTICS});

    // ── BLOODSWORN ─────────────────────────────────────────────────────────────
    addClass(F::Bloodsworn, "Blood Prince", S::Feast,
        "Drain 2 HP from largest friendly unit each round to heal hero",
        false, false, true, false, false, false, false,
        {SID::BLOOD_MAGIC, SID::BLOOD_POOL, SID::LEADERSHIP, SID::TACTICS, SID::SCOUTING});

    addClass(F::Bloodsworn, "Crimson Mage", S::Exsanguinate,
        "One Blood Power spell per battle costs no pool",
        false, false, true, false, false, false, false,
        {SID::BLOOD_MAGIC, SID::BLOOD_POOL, SID::OFFENSE, SID::LIGHT_MAGIC, SID::ARCHERY});

    addClass(F::Bloodsworn, "Thrall Master", S::Swarm,
        "All Fledglings start battle at ascension threshold",
        true, false, false, false, false, false, false,
        {SID::LEADERSHIP, SID::OFFENSE, SID::TACTICS, SID::BLOOD_POOL, SID::LOGISTICS});

    addClass(F::Bloodsworn, "Assassin Lord", S::Predator,
        "Permanent +1 Attack for each enemy hero killed this campaign",
        true, false, true, false, false, false, false,
        {SID::OFFENSE, SID::BLOOD_MAGIC, SID::TACTICS, SID::SCOUTING, SID::BLOOD_POOL});

    // ── THORNKIN ───────────────────────────────────────────────────────────────
    addClass(F::Thornkin, "Beastcaller", S::WildGrowth,
        "Dead companions respawn as spirit versions at half strength",
        false, false, false, false, true, false, false,
        {SID::NATURE_MAGIC, SID::SYMBIOSIS, SID::FIRST_AID, SID::LEADERSHIP, SID::SCOUTING});

    addClass(F::Thornkin, "Pathfinder", S::Overgrowth,
        "Place 3 forest tiles on combat map before battle starts",
        true, false, false, false, false, false, false,
        {SID::LOGISTICS, SID::SCOUTING, SID::TACTICS, SID::NATURE_MAGIC, SID::OFFENSE});

    addClass(F::Thornkin, "Stormbark", S::LightningRod,
        "First enemy spell each battle is redirected back at the caster",
        false, false, false, false, true, false, false,
        {SID::NATURE_MAGIC, SID::OFFENSE, SID::LIGHT_MAGIC, SID::TACTICS, SID::SYMBIOSIS});

    addClass(F::Thornkin, "Warsinger", S::Harmony,
        "All bonded pairs gain +1 Attack and Defense while hero is alive",
        true, false, false, false, true, false, false,
        {SID::LEADERSHIP, SID::OFFENSE, SID::DEFENSE_SKILL, SID::NATURE_MAGIC, SID::SYMBIOSIS});

    // ── ETERNAL EMPIRE ─────────────────────────────────────────────────────────
    addClass(F::EternalEmpire, "Death Herald", S::SoulHarvest,
        "Enemy unit kills heal hero for 5 HP per unit killed",
        false, false, false, true, false, false, false,
        {SID::DEATH_MAGIC, SID::ETERNAL_CMD, SID::NECROMANCY, SID::LEADERSHIP, SID::FIRST_AID});

    addClass(F::EternalEmpire, "Iron General", S::EternalLegion,
        "Reraised units retain their formation bonuses",
        true, false, false, false, false, false, false,
        {SID::OFFENSE, SID::DEFENSE_SKILL, SID::LEADERSHIP, SID::TACTICS, SID::ETERNAL_CMD});

    addClass(F::EternalEmpire, "Lich", S::Phylactery,
        "Hero respawns next battle at half stats if killed — once per campaign",
        false, false, false, true, false, false, false,
        {SID::DEATH_MAGIC, SID::ETERNAL_CMD, SID::NECROMANCY, SID::TACTICS, SID::MYSTICISM});

    addClass(F::EternalEmpire, "Grave Diplomat", S::NegotiatedWeakness,
        "Reveals enemy hero specialty before battle begins",
        true, false, false, true, false, false, false,
        {SID::ETERNAL_CMD, SID::LEADERSHIP, SID::DEATH_MAGIC, SID::NECROMANCY, SID::SCOUTING});

    // ── CRIMSON WARDENS ────────────────────────────────────────────────────────
    addClass(F::CrimsonWardens, "Warden Captain", S::CoordinatedStrike,
        "Marked target takes bonus damage from every attacker same round",
        true, false, false, false, false, false, false,
        {SID::WARDEN_MARK, SID::OFFENSE, SID::LEADERSHIP, SID::TACTICS, SID::NECROMANCY});

    addClass(F::CrimsonWardens, "Blood Sage", S::Elixir,
        "Once per battle fully heal one friendly unit",
        false, false, true, false, false, false, false,
        {SID::BLOOD_MAGIC, SID::FIRST_AID, SID::WARDEN_MARK, SID::MYSTICISM, SID::SCOUTING});

    addClass(F::CrimsonWardens, "Oathmaster", S::BloodWeb,
        "All allies heal 4 HP per enemy unit killed in battle",
        false, false, true, false, false, false, false,
        {SID::BLOOD_MAGIC, SID::WARDEN_MARK, SID::NECROMANCY, SID::FIRST_AID, SID::TACTICS});

    addClass(F::CrimsonWardens, "Inquisitor Hunter", S::BloodScent,
        "Always knows exact location of Bloodsworn heroes on world map",
        true, false, true, false, false, false, false,
        {SID::WARDEN_MARK, SID::OFFENSE, SID::SCOUTING, SID::TACTICS, SID::BLOOD_MAGIC});

    // ── VOIDKIN ────────────────────────────────────────────────────────────────
    addClass(F::Voidkin, "Void Weaver", S::VoidLink,
        "When a Void ally dies, adjacent enemies -1 ATK and nearby Void allies +1 ATK (2 rounds)",
        false, false, false, false, true, false, false,
        {SID::NATURE_MAGIC, SID::POSSESSION, SID::TACTICS, SID::SCOUTING, SID::LEADERSHIP});

    addClass(F::Voidkin, "Shadow Stalker", S::GhostWalk,
        "Hero is always invisible on world map",
        true, false, false, false, false, false, false,
        {SID::SCOUTING, SID::OFFENSE, SID::LOGISTICS, SID::POSSESSION, SID::TACTICS});

    addClass(F::Voidkin, "Blight Caller", S::BlightAura,
        "Sacred terrain tiles are passively corrupted each turn near hero",
        false, false, false, false, true, false, false,
        {SID::NATURE_MAGIC, SID::POSSESSION, SID::SCOUTING, SID::TACTICS, SID::LEADERSHIP});

    addClass(F::Voidkin, "Fell Druid", S::Wither,
        "Enemy units in aura lose 1 stat permanently each round",
        true, false, false, false, true, false, false,
        {SID::NATURE_MAGIC, SID::OFFENSE, SID::POSSESSION, SID::TACTICS, SID::DEFENSE_SKILL});

    // ── IRON ASSEMBLY ──────────────────────────────────────────────────────────
    addClass(F::IronAssembly, "Master Engineer", S::Efficient,
        "All units cost 20% fewer resources to craft",
        false, false, false, false, false, true, false,
        {SID::FORGE_MAGIC, SID::BLUEPRINT, SID::LOGISTICS, SID::TACTICS, SID::LEADERSHIP});

    addClass(F::IronAssembly, "Warlord Mechanic", S::IronDiscipline,
        "Constructs are immune to morale and fear effects",
        true, false, false, false, false, false, false,
        {SID::OFFENSE, SID::DEFENSE_SKILL, SID::LEADERSHIP, SID::TACTICS, SID::BLUEPRINT});

    addClass(F::IronAssembly, "Salvage Lord", S::Recycler,
        "All units gain permanent +1 ATK after each battle won (max +5)",
        true, false, false, false, false, true, false,
        {SID::FORGE_MAGIC, SID::BLUEPRINT, SID::OFFENSE, SID::TACTICS, SID::LOGISTICS});

    addClass(F::IronAssembly, "Runesmith", S::LivingRune,
        "Hero gains +1 ATK and +1 DEF permanently after each battle won (max +5 each)",
        false, false, false, false, false, true, false,
        {SID::FORGE_MAGIC, SID::BLUEPRINT, SID::DEFENSE_SKILL, SID::FIRST_AID, SID::LEADERSHIP});

    // ── AMALGAMATE ─────────────────────────────────────────────────────────────
    addClass(F::Amalgamate, "Evolver", S::RapidEvolution,
        "Units gain adaptations after taking only 1 hit of a damage type",
        false, false, false, false, false, false, true,
        {SID::FLESH_MAGIC, SID::ADAPTATION, SID::TACTICS, SID::LEADERSHIP, SID::SCOUTING});

    addClass(F::Amalgamate, "Hive Controller", S::Collective,
        "OrganicMech units share the best adaptation count at each round start",
        false, false, false, false, false, false, true,
        {SID::FLESH_MAGIC, SID::ADAPTATION, SID::LEADERSHIP, SID::FIRST_AID, SID::TACTICS});

    addClass(F::Amalgamate, "Flesh Architect", S::Infestation,
        "Flesh terrain spreads to adjacent tiles every round passively",
        true, false, false, false, false, false, true,
        {SID::FLESH_MAGIC, SID::ADAPTATION, SID::OFFENSE, SID::TACTICS, SID::LOGISTICS});

    addClass(F::Amalgamate, "Apex Hunter", S::Apex,
        "Hero starts battle with all adaptations The Evolved has accumulated",
        true, false, false, false, false, false, false,
        {SID::OFFENSE, SID::ADAPTATION, SID::TACTICS, SID::SCOUTING, SID::FLESH_MAGIC});

    // ── CONVERGENCE ────────────────────────────────────────────────────────────
    addClass(F::Convergence, "Lightbringer", S::Radiance,
        "Buff and debuff spells last 4 rounds instead of 2",
        false, true, false, false, false, false, false,
        {SID::LIGHT_MAGIC, SID::MIRRORING, SID::NATURE_MAGIC, SID::LEADERSHIP, SID::TACTICS});

    addClass(F::Convergence, "Oathbound", S::Covenant,
        "When a buff is cast, adjacent allies receive half the buff value",
        false, true, false, false, true, false, false,
        {SID::MIRRORING, SID::LEADERSHIP, SID::FIRST_AID, SID::LIGHT_MAGIC, SID::NATURE_MAGIC});

    addClass(F::Convergence, "Shadowlord", S::PredatorMirror,
        "First spell cast each battle costs no mana",
        false, false, false, true, false, false, false,
        {SID::MIRRORING, SID::DEATH_MAGIC, SID::BLOOD_MAGIC, SID::OFFENSE, SID::TACTICS});

    addClass(F::Convergence, "Voidcaller", S::Corruption,
        "Enemy units lose -1 DEF each round (starting round 2)",
        false, false, true, true, false, false, false,
        {SID::MIRRORING, SID::DEATH_MAGIC, SID::NATURE_MAGIC, SID::TACTICS, SID::SCOUTING});

    addClass(F::Convergence, "Ironweaver", S::Synthesis,
        "Hero regenerates +2 extra mana per round in combat",
        false, false, false, false, false, true, false,
        {SID::MIRRORING, SID::FORGE_MAGIC, SID::FLESH_MAGIC, SID::TACTICS, SID::LEADERSHIP});

    addClass(F::Convergence, "Fleshbinder", S::AdaptationMirror,
        "When any ally dies, all friendly OrganicMech units gain +1 ATK or DEF",
        false, false, false, false, false, false, true,
        {SID::MIRRORING, SID::FLESH_MAGIC, SID::ADAPTATION, SID::LEADERSHIP, SID::FIRST_AID});
}

const HeroClassDef* HeroClassRegistry::getClass(int id) const {
    for (auto& c : m_classes) if (c.id == id) return &c;
    return nullptr;
}

std::vector<const HeroClassDef*> HeroClassRegistry::getClassesForFaction(FactionId f) const {
    std::vector<const HeroClassDef*> result;
    for (auto& c : m_classes)
        if (c.faction == f) result.push_back(&c);
    return result;
}
