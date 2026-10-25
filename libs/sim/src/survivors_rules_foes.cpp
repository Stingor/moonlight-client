#include <array>
#include <cstdint>
#include <optional>
#include <span>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

#include "survivors_shipped.h"

namespace ro::sim::survivors {

namespace {

using enum FoeSize;
using enum SurvivorsElement;

// ── LES MONSTRES DE L'ARÈNE 1, leurs vagues et leur terrain ─────────────────
// Un foe_id est la classe RO du monstre. Son niveau, sa taille, son élément,
// sa marche (ms par case) et son intervalle de contact sont le Level, la Size,
// l'Element, le WalkSpeed et l'AttackDelay de db/pre-re/mob_db.yml du serveur ;
// ses PV, ses dégâts et son XP sont ceux du mode, pas ceux de RO.

constexpr std::uint16_t kFabreId = 1007;
constexpr std::uint16_t kDropsId = 1113;
constexpr std::uint16_t kWillowId = 1010;
constexpr std::uint16_t kRockerId = 1052;
constexpr std::uint16_t kCondorId = 1009;
constexpr std::uint16_t kSavageBabeId = 1167;
constexpr std::uint16_t kPoporingId = 1031;
constexpr std::uint16_t kSmokieId = 1056;
constexpr std::uint16_t kSkeletonId = 1076;
constexpr std::uint16_t kZombieId = 1015;
constexpr std::uint16_t kPoisonSporeId = 1077;
constexpr std::uint16_t kYoyoId = 1057;
constexpr std::uint16_t kOrcWarriorId = 1023;
constexpr std::uint16_t kSavageId = 1166;
constexpr std::uint16_t kMantisId = 1139;
constexpr std::uint16_t kOrcSkeletonId = 1152;
constexpr std::uint16_t kArcherSkeletonId = 1016;
constexpr std::uint16_t kMunakId = 1026;
constexpr std::uint16_t kGolemId = 1040;
constexpr std::uint16_t kBigfootId = 1060;
constexpr std::uint16_t kHunterFlyId = 1035;
constexpr std::uint16_t kMummyId = 1041;
constexpr std::uint16_t kBathoryId = 1102;
constexpr std::uint16_t kSoheeId = 1170;
constexpr std::uint16_t kWhisperId = 1179;
constexpr std::uint16_t kNightmareId = 1061;
constexpr std::uint16_t kWraithId = 1192;
constexpr std::uint16_t kEvilDruidId = 1117;

// Pupa ne bouge ni ne touche : un mur vivant, posé en anneau.
constexpr std::uint32_t kStillWalkMs = 0;
constexpr std::int32_t kNoTouchDamage = 0;
constexpr std::uint32_t kNoTouchMs = 0;

// ── Les comportements ──────────────────────────────────────────────────────

constexpr FoeBehaviour kStill{.move = FoeMove::kStill};
// Un volant passe au-dessus de la horde, en oscillant droit sur le héros.
constexpr FoeBehaviour kFlyer{.move = FoeMove::kFlyer, .traits = kFoeOverHorde};
constexpr FoeBehaviour kGhostFlyer{
    .move = FoeMove::kFlyer, .traits = static_cast<std::uint8_t>(kFoeOverHorde | kFoePassesWalls)};
constexpr FoeBehaviour kThief{.move = FoeMove::kThief};
constexpr FoeBehaviour kWolfPack{.pack = 6};
constexpr FoeBehaviour kKnockbackImmune{.traits = kFoeKnockbackImmune};
// Smokie se cache une seconde toutes les six : ni visé, ni touché, ni touchant.
constexpr FoeBehaviour kHider{.traits = kFoeHides, .period_ms = 6000, .active_ms = 1000};
// Poporing laisse en mourant une flaque qui blesse chaque seconde.
constexpr FoeBehaviour kPuddler{.puddle_damage = 2,
                                .puddle_radius_units = CellTenths(15),
                                .puddle_ms = 4000,
                                .puddle_period_ms = 1000};
// Savage et Nightmare, à moins de six cases, annoncent puis filent quatre
// cases à seize cases par seconde : 250 ms de course.
constexpr FoeBehaviour kCharger{.move = FoeMove::kCharger,
                                .period_ms = 5000,
                                .telegraph_ms = 500,
                                .active_ms = 250,
                                .reach_units = Cells(6),
                                .dash_units_per_s = Cells(16)};
// Archer Skeleton se tient à six cases et tire, au rythme de son AttackDelay.
constexpr FoeBehaviour kArcher{.move = FoeMove::kRanged,
                               .period_ms = 2864,
                               .telegraph_ms = 500,
                               .reach_units = Cells(6),
                               .shot_damage = 6,
                               .shot_units_per_s = Cells(10),
                               .shot_radius_units = CellTenths(3)};
// Bathory saute de trois cases vers le héros, après une courte annonce.
constexpr FoeBehaviour kBlinker{
    .move = FoeMove::kBlinker, .period_ms = 4000, .telegraph_ms = 300, .reach_units = Cells(3)};
constexpr FoeBehaviour kBossImmunities{
    .traits = static_cast<std::uint8_t>(kFoeKnockbackImmune | kFoeFreezeImmune)};

// Un monstre commun : niveau RO, taille, élément, PV, marche en ms par case,
// dégâts et intervalle de contact, XP, comportement.
[[nodiscard]] constexpr FoeRule Common(std::uint16_t foe_id, std::uint8_t ro_level, FoeSize size,
                                       SurvivorsElement element, std::int32_t hp,
                                       std::uint32_t walk_ms, std::int32_t touch_damage,
                                       std::uint32_t touch_ms, std::uint32_t exp,
                                       const FoeBehaviour& behaviour = {}) noexcept {
  return FoeRule{.foe_id = foe_id,
                 .ro_level = ro_level,
                 .rank = FoeRank::kCommon,
                 .size = size,
                 .element = element,
                 .hp = hp,
                 .ro_walk_ms_per_cell = walk_ms,
                 .touch_damage = touch_damage,
                 .touch_interval_ms = touch_ms,
                 .exp = exp,
                 .behaviour = behaviour};
}

// Un boss : comme un commun, à son rang.
[[nodiscard]] constexpr FoeRule Boss(FoeRank rank, std::uint16_t foe_id, std::uint8_t ro_level,
                                     FoeSize size, SurvivorsElement element, std::int32_t hp,
                                     std::uint32_t walk_ms, std::int32_t touch_damage,
                                     std::uint32_t touch_ms, std::uint32_t exp,
                                     const FoeBehaviour& behaviour = {}) noexcept {
  FoeRule rule =
      Common(foe_id, ro_level, size, element, hp, walk_ms, touch_damage, touch_ms, exp, behaviour);
  rule.rank = rank;
  return rule;
}

// Un monstre qui porte une carte : il la lâche selon la règle de chute.
[[nodiscard]] constexpr FoeRule WithCard(FoeRule rule, std::uint16_t card_item_id) noexcept {
  rule.card_item_id = card_item_id;
  return rule;
}

// Common(Id, niveau, taille, élément, PV, ms par case, contact, ms, XP[, comportement]).
// WithCard(…, carte) : un porteur de carte.
constexpr std::array<FoeRule, 45> kFoes{{
    Common(kLunaticId, 3, kSmall, kNeutral, 11, 200, 3, 1456, 1),
    Common(kFabreId, 2, kSmall, kEarth, 22, 400, 4, 1672, 1),
    WithCard(Common(kPoringId, 1, kMedium, kWater, 15, 400, 4, 1872, 1), kPoringCardId),
    Common(kDropsId, 3, kMedium, kFire, 28, 400, 6, 1372, 2),
    WithCard(Common(kChonchonId, 4, kSmall, kWind, 14, 200, 4, 1076, 1), kChonchonCardId),
    Common(kWillowId, 4, kMedium, kEarth, 28, 200, 6, 1672, 2),
    WithCard(
        Common(kPupaId, 2, kSmall, kEarth, 60, kStillWalkMs, kNoTouchDamage, kNoTouchMs, 2, kStill),
        kPupaCardId),
    Common(kRockerId, 9, kMedium, kEarth, 30, 200, 7, 1864, 2),
    Common(kCondorId, 5, kMedium, kWind, 24, 150, 6, 1148, 2),
    Common(kSavageBabeId, 7, kSmall, kEarth, 20, 400, 5, 1624, 2),
    WithCard(Common(kHornetId, 8, kSmall, kWind, 20, 150, 6, 1292, 2), kHornetCardId),
    Common(kFamiliarId, 8, kSmall, kDark, 18, 150, 5, 1276, 2, kFlyer),
    WithCard(Common(kThiefBugId, 6, kSmall, kNeutral, 22, 150, 6, 1288, 2, kThief),
             kThiefBugCardId),
    WithCard(Common(kSporeId, 16, kMedium, kWater, 40, 200, 8, 1872, 3), kSporeCardId),
    Common(kPoporingId, 14, kMedium, kPoison, 36, 300, 8, 1672, 3, kPuddler),
    Common(kSmokieId, 18, kSmall, kEarth, 36, 200, 6, 1576, 3, kHider),
    Common(kSkeletonId, 10, kMedium, kUndead, 34, 200, 7, 2228, 3),
    Common(kZombieId, 15, kMedium, kUndead, 60, 400, 8, 2612, 4),
    Common(kPoisonSporeId, 19, kMedium, kPoison, 48, 200, 6, 1672, 4),
    Common(kWolfId, 25, kMedium, kEarth, 40, 200, 7, 1054, 5, kWolfPack),
    Common(kYoyoId, 21, kSmall, kEarth, 35, 200, 7, 1054, 4),
    Common(kOrcWarriorId, 24, kMedium, kEarth, 50, 200, 7, 1864, 6),
    Common(kSavageId, 26, kLarge, kEarth, 100, 150, 7, 1960, 10, kCharger),
    Common(kGoblinId, 25, kMedium, kWind, 60, 100, 12, 1120, 6),
    Common(kMantisId, 26, kMedium, kEarth, 65, 200, 8, 1528, 8),
    Common(kOrcSkeletonId, 28, kMedium, kUndead, 45, 200, 12, 2420, 7),
    // Sa flèche est son attaque : il ne touche pas au contact.
    Common(kArcherSkeletonId, 31, kMedium, kUndead, 40, 300, kNoTouchDamage, kNoTouchMs, 8,
           kArcher),
    Common(kMunakId, 30, kMedium, kUndead, 50, 200, 10, 2468, 8),
    WithCard(Common(kGolemId, 25, kLarge, kNeutral, 150, 300, 10, 1608, 15, kKnockbackImmune),
             kGolemCardId),
    Common(kBigfootId, 25, kLarge, kEarth, 110, 300, 12, 1260, 12),
    Common(kHunterFlyId, 42, kSmall, kWind, 45, 150, 8, 676, 8, kFlyer),
    Common(kMummyId, 37, kMedium, kUndead, 85, 300, 12, 1772, 12),
    Common(kGhoulId, 40, kMedium, kUndead, 90, 250, 14, 2456, 12),
    Common(kBathoryId, 44, kMedium, kDark, 70, 100, 8, 1504, 12, kBlinker),
    Common(kSoheeId, 33, kMedium, kWater, 75, 250, 12, 2112, 12),
    Common(kWhisperId, 34, kSmall, kGhost, 40, 150, 12, 1960, 10, kGhostFlyer),
    Common(kNightmareId, 49, kLarge, kGhost, 125, 150, 11, 1816, 18, kCharger),
    Common(kWraithId, 53, kLarge, kUndead, 150, 300, 22, 1816, 20),
    Common(kEvilDruidId, 58, kLarge, kUndead, 170, 300, 24, 2276, 22),
    // Baphomet Jr. ne naît que des invocations de Baphomet.
    Common(kBaphometJrId, 50, kSmall, kDark, 400, 100, 15, 868, 5),
    // Les mini-boss et le boss final : PV fixes, marche de leur BossRule.
    WithCard(Boss(FoeRank::kMiniBoss, kMasteringId, 2, kMedium, kWater, 600, 300, 12, 1072, 20),
             kMasteringCardId),
    Boss(FoeRank::kMiniBoss, kEclipseId, 6, kMedium, kNeutral, 900, 200, 11, 1456, 30),
    Boss(FoeRank::kMiniBoss, kDragonFlyId, 8, kSmall, kWind, 1200, 100, 12, 1076, 40, kFlyer),
    Boss(FoeRank::kMiniBoss, kVagabondWolfId, 24, kMedium, kEarth, 2400, 150, 11, 1048, 60),
    Boss(FoeRank::kBoss, kBaphometId, 81, kLarge, kDark, 11000, 100, 35, 768, 0, kBossImmunities),
}};

// ── Les vagues ─────────────────────────────────────────────────────────────
// Chaque liste de monstres va avec ses poids, dans le même ordre. Les actes 2
// et 3 s'ouvrent sur un répit de 20 s : deux vagues, la seconde au plancher
// plein.

constexpr std::uint32_t kActBreatherMs = 20000;
// À 14:50, les naissances ordinaires cessent : dix secondes avant Baphomet.
constexpr std::uint32_t kSilenceMs = 14 * kMinuteMs + 50000;

constexpr std::array<std::uint16_t, 3> kWave0Foes{kLunaticId, kFabreId, kPoringId};
constexpr std::array<std::uint32_t, 3> kWave0Weights{50, 30, 20};
constexpr std::array<std::uint16_t, 4> kWave1Foes{kLunaticId, kFabreId, kPoringId, kDropsId};
constexpr std::array<std::uint32_t, 4> kWave1Weights{30, 30, 20, 20};
constexpr std::array<std::uint16_t, 4> kWave2Foes{kFabreId, kDropsId, kChonchonId, kPoringId};
constexpr std::array<std::uint32_t, 4> kWave2Weights{25, 25, 30, 20};
constexpr std::array<std::uint16_t, 4> kWave3Foes{kDropsId, kWillowId, kChonchonId, kLunaticId};
constexpr std::array<std::uint32_t, 4> kWave3Weights{25, 30, 20, 25};
constexpr std::array<std::uint16_t, 4> kWave4Foes{kWillowId, kRockerId, kCondorId, kSavageBabeId};
constexpr std::array<std::uint32_t, 4> kWave4Weights{30, 25, 25, 20};
constexpr std::array<std::uint16_t, 4> kWave5Foes{kRockerId, kHornetId, kFamiliarId, kThiefBugId};
constexpr std::array<std::uint32_t, 4> kWave5Weights{30, 25, 25, 20};
constexpr std::array<std::uint16_t, 4> kWave6Foes{kSporeId, kPoporingId, kSmokieId, kFamiliarId};
constexpr std::array<std::uint32_t, 4> kWave6Weights{30, 25, 25, 20};
constexpr std::array<std::uint16_t, 3> kWave7Foes{kSkeletonId, kZombieId, kFamiliarId};
constexpr std::array<std::uint32_t, 3> kWave7Weights{40, 30, 30};
constexpr std::array<std::uint16_t, 4> kWave8Foes{kZombieId, kSkeletonId, kPoisonSporeId,
                                                  kSmokieId};
constexpr std::array<std::uint32_t, 4> kWave8Weights{25, 25, 30, 20};
constexpr std::array<std::uint16_t, 4> kWave9Foes{kWolfId, kYoyoId, kSavageId, kOrcWarriorId};
constexpr std::array<std::uint32_t, 4> kWave9Weights{30, 25, 10, 30};
constexpr std::array<std::uint16_t, 4> kWave10Foes{kOrcWarriorId, kMantisId, kBigfootId, kSavageId};
constexpr std::array<std::uint32_t, 4> kWave10Weights{30, 30, 20, 12};
constexpr std::array<std::uint16_t, 4> kWave11Foes{kOrcSkeletonId, kArcherSkeletonId, kMunakId,
                                                   kHunterFlyId};
constexpr std::array<std::uint32_t, 4> kWave11Weights{30, 20, 30, 20};
constexpr std::array<std::uint16_t, 4> kWave12Foes{kGolemId, kMummyId, kGhoulId, kWhisperId};
constexpr std::array<std::uint32_t, 4> kWave12Weights{15, 30, 30, 25};
constexpr std::array<std::uint16_t, 5> kWave13Foes{kGhoulId, kBathoryId, kSoheeId, kNightmareId,
                                                   kArcherSkeletonId};
constexpr std::array<std::uint32_t, 5> kWave13Weights{25, 20, 25, 15, 15};
constexpr std::array<std::uint16_t, 4> kWave14Foes{kWraithId, kEvilDruidId, kNightmareId, kMummyId};
constexpr std::array<std::uint32_t, 4> kWave14Weights{25, 20, 25, 30};

// Une vague : sa réserve, son plancher, l'intervalle et la taille de ses lots.
[[nodiscard]] constexpr WaveRule Wave(std::uint32_t from_ms, std::span<const std::uint16_t> foe_ids,
                                      std::span<const std::uint32_t> weights, std::uint16_t floor,
                                      std::uint32_t interval_ms, std::uint8_t batch) noexcept {
  return WaveRule{.from_ms = from_ms,
                  .foe_ids = foe_ids,
                  .weights = weights,
                  .floor = floor,
                  .interval_ms = interval_ms,
                  .batch = batch};
}

// Wave(début, réserve, poids, plancher, intervalle en ms, lot).
constexpr std::array<WaveRule, 18> kWaves{{
    Wave(0, kWave0Foes, kWave0Weights, 15, 1000, 2),
    Wave(1 * kMinuteMs, kWave1Foes, kWave1Weights, 30, 1000, 3),
    Wave(2 * kMinuteMs, kWave2Foes, kWave2Weights, 45, 900, 3),
    Wave(3 * kMinuteMs, kWave3Foes, kWave3Weights, 60, 900, 4),
    Wave(4 * kMinuteMs, kWave4Foes, kWave4Weights, 75, 800, 4),
    Wave(5 * kMinuteMs, kWave5Foes, kWave5Weights, 90, 800, 5),
    Wave(6 * kMinuteMs, kWave6Foes, kWave6Weights, 110, 700, 5),
    Wave(7 * kMinuteMs, kWave7Foes, kWave7Weights, 80, 700, 6),
    Wave(7 * kMinuteMs + kActBreatherMs, kWave7Foes, kWave7Weights, 130, 700, 6),
    Wave(8 * kMinuteMs, kWave8Foes, kWave8Weights, 150, 600, 6),
    Wave(9 * kMinuteMs, kWave9Foes, kWave9Weights, 170, 600, 7),
    Wave(10 * kMinuteMs, kWave10Foes, kWave10Weights, 190, 500, 7),
    Wave(11 * kMinuteMs, kWave11Foes, kWave11Weights, 160, 500, 8),
    Wave(11 * kMinuteMs + kActBreatherMs, kWave11Foes, kWave11Weights, 210, 500, 8),
    Wave(12 * kMinuteMs, kWave12Foes, kWave12Weights, 230, 450, 8),
    Wave(13 * kMinuteMs, kWave13Foes, kWave13Weights, 260, 400, 9),
    Wave(14 * kMinuteMs, kWave14Foes, kWave14Weights, 300, 350, 10),
    // La vague silencieuse : sa réserve ne sert qu'à garder des poids valides.
    Wave(kSilenceMs, kWave14Foes, kWave14Weights, 0, 1000, 0),
}};

// Le départ est le centre de la carte : sur prt_fild08, la case (200, 200),
// praticable, dont la fenêtre de 40 × 40 l'est à 99 % (`roc-cli survivors
// arena-openness`, ADR 0133).
constexpr std::array<ArenaRule, 1> kArenas{{
    {.arena_id = 1, .key = "prt_fild08", .map_name = "prt_fild08", .start = std::nullopt},
}};

constexpr std::array<ExpSegment, 3> kExpCurve{{
    {.from_level = 1, .base = 5, .per_level = 10},
    {.from_level = 20, .base = 220, .per_level = 40},
    {.from_level = 40, .base = 3500, .per_level = 200},
}};

// La sous-table de niveau 1 (`Level: 1`) de db/pre-re/attr_fix.yml du serveur,
// recopiée telle quelle : attaque en ligne, défense en colonne, en %, dans
// l'ordre de SurvivorsElement (Neutral, Water, Earth, Fire, Wind, Poison, Holy,
// Dark, Ghost, Undead). Les valeurs négatives y restent ; ElementPct les ramène
// à 0.
constexpr std::array<ElementRow, kSurvivorsElementCount> kElements{{
    {100, 100, 100, 100, 100, 100, 100, 100, 25, 100},  // Neutral
    {100, 25, 100, 150, 50, 100, 75, 100, 100, 100},    // Water
    {100, 100, 100, 50, 150, 100, 75, 100, 100, 100},   // Earth
    {100, 50, 150, 25, 100, 100, 75, 100, 100, 125},    // Fire
    {100, 175, 50, 100, 25, 100, 75, 100, 100, 100},    // Wind
    {100, 100, 125, 125, 125, 0, 75, 50, 100, -25},     // Poison
    {100, 100, 100, 100, 100, 100, 0, 125, 100, 150},   // Holy
    {100, 100, 100, 100, 100, 50, 125, 0, 100, -25},    // Dark
    {25, 100, 100, 100, 100, 100, 75, 75, 125, 100},    // Ghost
    {100, 100, 100, 100, 100, 50, 100, 0, 100, 0},      // Undead
}};

}  // namespace

std::span<const FoeRule> ShippedFoes() { return kFoes; }

std::span<const WaveRule> ShippedWaves() { return kWaves; }

std::span<const ArenaRule> ShippedArenas() { return kArenas; }

std::span<const ExpSegment> ShippedExpCurve() { return kExpCurve; }

std::span<const ElementRow> ShippedElements() { return kElements; }

}  // namespace ro::sim::survivors
