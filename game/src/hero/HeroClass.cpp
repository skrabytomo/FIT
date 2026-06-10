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
        "Deaths near hero double Inspiration meter gain",
        false, true, false, false, false, false, false,
        {SID::LIGHT_MAGIC, SID::FIRST_AID, SID::INSPIRATION, SID::LEADERSHIP, SID::SCOUTING});

    addClass(F::HolyOrder, "Crusader", S::Veteran,
        "Units gain +1 Attack and Defense per previous battle this campaign",
        true, false, false, false, false, false, false,
        {SID::OFFENSE, SID::DEFENSE_SKILL, SID::LEADERSHIP, SID::TACTICS, SID::LOGISTICS});

    addClass(F::HolyOrder, "Flagellant Marshal", S::BloodPenance,
        "Hero loses HP each round — Desperation meter charges faster",
        true, true, false, false, false, false, false,
        {SID::DESPERATION, SID::INSPIRATION, SID::OFFENSE, SID::LIGHT_MAGIC, SID::TACTICS});

    // ── BLOODSWORN ─────────────────────────────────────────────────────────────
    addClass(F::Bloodsworn, "Blood Prince", S::Feast,
        "Hero drains HP from one friendly unit per round to fill Blood Pool",
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
        "Enemy kills restore hero HP equal to unit tier killed",
        false, false, false, true, false, false, false,
        {SID::DEATH_MAGIC, SID::ETERNAL_CMD, SID::SCOUTING, SID::LEADERSHIP, SID::FIRST_AID});

    addClass(F::EternalEmpire, "Iron General", S::EternalLegion,
        "Reraised units retain their formation bonuses",
        true, false, false, false, false, false, false,
        {SID::OFFENSE, SID::DEFENSE_SKILL, SID::LEADERSHIP, SID::TACTICS, SID::ETERNAL_CMD});

    addClass(F::EternalEmpire, "Lich", S::Phylactery,
        "Hero respawns next battle at half stats if killed — once per campaign",
        false, false, false, true, false, false, false,
        {SID::DEATH_MAGIC, SID::ETERNAL_CMD, SID::OFFENSE, SID::TACTICS, SID::SCOUTING});

    addClass(F::EternalEmpire, "Grave Diplomat", S::NegotiatedWeakness,
        "Reveals enemy hero specialty before battle begins",
        true, false, false, true, false, false, false,
        {SID::ETERNAL_CMD, SID::LEADERSHIP, SID::DEATH_MAGIC, SID::TACTICS, SID::SCOUTING});

    // ── CRIMSON WARDENS ────────────────────────────────────────────────────────
    addClass(F::CrimsonWardens, "Warden Captain", S::CoordinatedStrike,
        "Marked target takes bonus damage from every attacker same round",
        true, false, false, false, false, false, false,
        {SID::WARDEN_MARK, SID::OFFENSE, SID::LEADERSHIP, SID::TACTICS, SID::DEFENSE_SKILL});

    addClass(F::CrimsonWardens, "Blood Sage", S::Elixir,
        "Once per battle fully heal one friendly unit",
        false, false, true, false, false, false, false,
        {SID::BLOOD_MAGIC, SID::FIRST_AID, SID::WARDEN_MARK, SID::LEADERSHIP, SID::SCOUTING});

    addClass(F::CrimsonWardens, "Oathmaster", S::BloodWeb,
        "Linked units heal when any linked unit makes a kill",
        false, false, true, false, false, false, false,
        {SID::BLOOD_MAGIC, SID::WARDEN_MARK, SID::LEADERSHIP, SID::FIRST_AID, SID::TACTICS});

    addClass(F::CrimsonWardens, "Inquisitor Hunter", S::BloodScent,
        "Always knows exact location of Bloodsworn heroes on world map",
        true, false, true, false, false, false, false,
        {SID::WARDEN_MARK, SID::OFFENSE, SID::SCOUTING, SID::TACTICS, SID::BLOOD_MAGIC});

    // ── VOIDKIN ────────────────────────────────────────────────────────────────
    addClass(F::Voidkin, "Void Weaver", S::VoidLink,
        "Possession spreads to adjacent ally unit on possessee death",
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
        "Salvage buffs from combat are permanent across battles",
        true, false, false, false, false, true, false,
        {SID::FORGE_MAGIC, SID::BLUEPRINT, SID::OFFENSE, SID::TACTICS, SID::LOGISTICS});

    addClass(F::IronAssembly, "Runesmith", S::LivingRune,
        "One unit gets a permanent stat increase after each battle won",
        false, false, false, false, false, true, false,
        {SID::FORGE_MAGIC, SID::BLUEPRINT, SID::DEFENSE_SKILL, SID::FIRST_AID, SID::LEADERSHIP});

    // ── AMALGAMATE ─────────────────────────────────────────────────────────────
    addClass(F::Amalgamate, "Evolver", S::RapidEvolution,
        "Units gain adaptations after taking only 1 hit of a damage type",
        false, false, false, false, false, false, true,
        {SID::FLESH_MAGIC, SID::ADAPTATION, SID::TACTICS, SID::LEADERSHIP, SID::SCOUTING});

    addClass(F::Amalgamate, "Hive Controller", S::Collective,
        "Transfer one adaptation between any two friendly units",
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
        "Mirror ability lasts 7 rounds instead of 5",
        false, true, false, false, false, false, false,
        {SID::LIGHT_MAGIC, SID::MIRRORING, SID::NATURE_MAGIC, SID::LEADERSHIP, SID::TACTICS});

    addClass(F::Convergence, "Oathbound", S::Covenant,
        "Linked units share the active mirrored buffs",
        false, true, false, false, true, false, false,
        {SID::MIRRORING, SID::LEADERSHIP, SID::FIRST_AID, SID::LIGHT_MAGIC, SID::NATURE_MAGIC});

    addClass(F::Convergence, "Shadowlord", S::PredatorMirror,
        "Mirror ability activates instantly with no delay",
        false, false, false, true, false, false, false,
        {SID::MIRRORING, SID::DEATH_MAGIC, SID::BLOOD_MAGIC, SID::OFFENSE, SID::TACTICS});

    addClass(F::Convergence, "Voidcaller", S::Corruption,
        "Mirrored terrain effects persist after mirror expires",
        false, false, true, true, false, false, false,
        {SID::MIRRORING, SID::DEATH_MAGIC, SID::NATURE_MAGIC, SID::TACTICS, SID::SCOUTING});

    addClass(F::Convergence, "Ironweaver", S::Synthesis,
        "Can hold two active mirrors simultaneously",
        false, false, false, false, false, true, false,
        {SID::MIRRORING, SID::FORGE_MAGIC, SID::FLESH_MAGIC, SID::TACTICS, SID::LEADERSHIP});

    addClass(F::Convergence, "Fleshbinder", S::AdaptationMirror,
        "Units gain one adaptation per mirrored battle that persists",
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
