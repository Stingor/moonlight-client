// CE QUE L'INCANTATION MONTRE PAR COMPÉTENCE.
//
// Après l'effet d'incantation que la résolution de la pose donne, le client
// aiguille sur la compétence. Soixante-quinze ont leur cas : un ou deux effets
// sur le lanceur, qui vivent ce que dure l'incantation. Les autres passent par
// son registre des actions, puis, à son défaut, par deux tables qui donnent un
// premier et un second effet. Rien de tout cela ne vient des tables de
// compétences : sans ce fichier, ces incantations ne montraient que leur effet
// générique.
//
// CD_COMPETENTIA pose en plus un cercle de visée sous son lanceur, celui que le
// client peint autour de la case visée d'un sort au sol : voir
// `ro/render/ground_scope.h`.

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "ro/render/effect_frame.h"
#include "ro/render/ground_skill_visual.h"
#include "ro/render/map_effect_math.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "effects/effect_node_view.h"
#include "effects/effect_own_skill_cases.h"
#include "effects/effect_view.h"
#include "world/actor_sounds.h"
#include "world/actor_view.h"
#include "world/hit_popup_rules.h"

namespace moonclient {
namespace {

// CE QUE VIT L'EFFET : sa vie à lui, la durée de l'incantation, ou cette durée
// et cinquante millisecondes de plus.
enum class CastLife : std::uint8_t { kOwn, kCast, kCastAndFifty };

// SOUS QUEL RÉGLAGE IL NAÎT : toujours, seulement en effets complets, ou
// seulement sous `/mineffect`.
enum class Under : std::uint8_t { kAny, kFull, kMinimal };

struct CastSpawn {
  std::uint16_t skill_id = 0;
  std::uint16_t effect_id = 0;
  CastLife life = CastLife::kCast;
  Under under = Under::kAny;
};

constexpr CastLife kOwn = CastLife::kOwn;
constexpr CastLife kCast = CastLife::kCast;
constexpr CastLife kPlusFifty = CastLife::kCastAndFifty;
constexpr Under kFull = Under::kFull;
constexpr Under kMinimal = Under::kMinimal;

// LES CAS RÉGULIERS, triés par compétence ; ceux d'une même compétence gardent
// l'ordre où le client les fait naître.
constexpr std::array kCastSpawns = std::to_array<CastSpawn>({
    {737, 1425},
    {737, 1426},        // NPC_RAYOFGENESIS
    {2006, 1626},       // RK_IGNITIONBREAK
    {2032, 768, kOwn},  // GC_POISONSMOKE
    {2038, 1629},
    {2038, 1630},  // AB_JUDEX
    {2052, 1559},  // AB_CLEARANCE
    {2202, 1587},
    {2202, 1588},                // WL_SOULEXPANSION
    {2204, 1446, kCast, kFull},  // WL_JACKFROST
    {2204, 1543, kCast, kMinimal},
    {2206, 803, kOwn},           // WL_RECOGNIZEDSPELL
    {2212, 1552},                // WL_HELLINFERNO
    {2213, 1415, kCast, kFull},  // WL_COMET
    {2213, 1421, kCast, kFull},
    {2213, 1536, kCast, kMinimal},
    {2213, 1539, kCast, kMinimal},
    {2214, 1635, kOwn},
    {2214, 1636},                // WL_CHAINLIGHTNING
    {2217, 1277, kCast, kFull},  // WL_TETRAVORTEX
    {2217, 1261, kCast, kFull},
    {2217, 1440, kCast, kMinimal},
    {2217, 1431, kCast, kMinimal},
    {2236, 1649},      // RA_AIMEDBOLT
    {2302, 16, kOwn},  // SC_MAELSTROM
    {2317, 1687},      // LG_OVERBRAND
    {2317, 1688, kCast, kFull},
    {2321, 1425},
    {2321, 1426},  // LG_RAYOFGENESIS
    {2343, 1573},  // SR_GATEOFHELL
    {2413, 1581},
    {2413, 1582},  // WM_METALICSOUND
    {2429, 1642},
    {2429, 1643},                // WM_SOUND_OF_DESTRUCTION
    {2454, 1563, kCast, kFull},  // SO_VARETYR_SPEAR
    {2454, 1564},
    {2477, 1422, kCast, kFull},  // GN_CARTCANNON
    {2477, 1423, kCast, kFull},
    {2477, 1499, kCast, kMinimal},
    {2477, 1500, kCast, kMinimal},
    {2482, 1620},  // GN_WALLOFTHORN
    {2482, 1621, kCast, kFull},
    {2492, 1641},  // GN_MANDRAGORA
    {2517, 1578},
    {2517, 1579},  // SR_HOWLINGOFLION
    {5214, 1738},
    {5214, 1739},  // AG_DEADLY_PROJECTION
    {5215, 1734},
    {5215, 1735},  // AG_DESTRUCTIVE_HURRICANE
    {5216, 1753},
    {5216, 1754},  // AG_RAIN_OF_CRYSTAL
    {5217, 1773},
    {5217, 1774},  // AG_MYSTERY_ILLUSION
    {5218, 1761},
    {5218, 1762},  // AG_VIOLENT_QUAKE
    {5220, 1742},
    {5220, 1743},  // AG_SOUL_VC_STRIKE
    {5221, 1890},
    {5221, 1891},  // AG_STRANTUM_TREMOR
    {5222, 1796},
    {5222, 1797},  // AG_ALL_BLOOM
    {5225, 1856},
    {5225, 1857},  // AG_CRYSTAL_IMPACT
    {5227, 1778},
    {5227, 1779},  // AG_TORNADO_STORM
    {5229, 1805},
    {5229, 1806},  // AG_FLORAL_FLARE_ROAD
    {5230, 1769},
    {5230, 1770},        // AG_ASTRAL_STRIKE
    {5232, 1802, kOwn},  // AG_CLIMAX : voir `kClimaxSecond`
    {5233, 1747},
    {5233, 1748},  // AG_ROCK_DOWN
    {5234, 1734},
    {5234, 1735},  // AG_STORM_CANNON
    {5235, 1757},
    {5235, 1758},  // AG_CRIMSON_ARROW
    {5237, 1782},
    {5237, 1783},  // AG_FROZEN_SLASH
    {5246, 1976},  // IQ_FIRST_FAITH_POWER
    {5247, 1979},  // IQ_JUDGE
    {5254, 1983},  // IQ_THIRD_EXOR_FLAME
    {5269, 1809},  // CD_MEDIALE_VOTUM
    {5271, 1812},  // CD_ARGUTUS_VITA
    {5272, 1814},  // CD_ARGUTUS_TELUM
    {5273, 1823},  // CD_ARBITRIUM
    {5273, 1822, kCast, kFull},
    {5275, 1816},                // CD_PRESENS_ACIES
    {5278, 1851, kCast, kFull},  // CD_COMPETENTIA
    {5279, 1958},                // CD_PNEUMATICUS_PROCELLA
    {5280, 1925, kCast, kFull},  // CD_DILECTIO_HEAL
    {5280, 1926},
    {5281, 1861},                // CD_RELIGIO
    {5282, 1888, kCast, kFull},  // CD_BENEDICTUM
    {5282, 1889},
    {5302, 1935},  // MT_SUMMON_ABR_BATTLE_WARIOR
    {5303, 1916},  // MT_SUMMON_ABR_DUAL_CANNON
    {5304, 1901},  // MT_SUMMON_ABR_MOTHER_NET
    {5305, 1908},  // MT_SUMMON_ABR_INFINITY
    {5316, 2014},
    {5316, 2015},  // ABC_CHAIN_REACTION_SHOT
    {5318, 2127},
    {5318, 2128},  // ABC_ABYSS_SLAYER
    {5329, 1969},  // WH_HAWKBOOMERANG
    {5331, 1873},  // WH_DEEPBLINDTRAP
    {5332, 1880},
    {5332, 1881},              // WH_SOLIDTRAP
    {5333, 1877},              // WH_SWIFTTRAP
    {5335, 1921},              // WH_FLAMETRAP
    {5361, 2123, kPlusFifty},  // TR_MUSICAL_INTERLUDE
    {5363, 2076},
    {5363, 2077},  // TR_NIPELHEIM_REQUIEM
    {5367, 2004},
    {5367, 2005},  // EM_ACTIVITY_BURN
    {5368, 1993},
    {5368, 1994},  // EM_INCREASING_ACTIVITY
    {5376, 2021},
    {5376, 2022},  // EM_SUMMON_ELEMENTAL_DILUVIO
    {5379, 2010},
    {5379, 2011},              // EM_SUMMON_ELEMENTAL_SERPENS
    {8057, 2333, kPlusFifty},  // MH_HEILIGE_PFERD
});

// LES CAS QUI CALCULENT, et qui ne sont pas dans la table.
constexpr std::uint16_t kSorcererPsychicWave = 2449;
constexpr std::uint16_t kSorcererDiamondDust = 2447;
constexpr std::uint16_t kNpcPsychicWave = 736;
constexpr std::uint16_t kSorcererElementalCure = 2464;
constexpr std::uint16_t kMechanicArmsCannon = 2261;
constexpr std::uint16_t kAgDestructiveHurricane = 5215;
constexpr std::uint16_t kAgStormCannon = 5234;
constexpr std::uint16_t kAgClimax = 5232;
constexpr std::uint16_t kCdCompetentia = 5278;

// Sur la case visée, sans porteur : Diamond Dust et les deux Psychic Wave.
constexpr std::uint16_t kDiamondDustFull = 1284;
constexpr std::uint16_t kDiamondDustMinimal = 1452;
constexpr std::uint16_t kPsychicWaveFull = 1290;
constexpr std::uint16_t kPsychicWaveMinimal = 1456;

// Elemental Cure : un éclat posé là où se tient le lanceur, et un bruit au
// centre de la carte.
constexpr std::uint16_t kElementalCureEffect = 216;
constexpr std::string_view kElementalCureSound = "_heal_effect.wav";

// Le second effet de Destructive Hurricane et de Storm Cannon sonne à sa
// naissance.
constexpr std::string_view kBeginSpellSound = "effect\\EF_BeginSpell.wav";

// Le second effet de Climax attend deux cents millisecondes, qu'il retranche de
// sa vie.
constexpr std::uint16_t kClimaxSecond = 1803;
constexpr std::uint32_t kClimaxSecondDelayMs = 200;

// LA GUEULE D'ARMS CANNON par quart de vue — ]0, 90[, [90, 180[, [180, 270[,
// le reste —, complète puis allégée. La complète a sa seconde moitié au numéro
// suivant ; les deux se suivent de leur paire de flammes.
constexpr std::array<std::uint16_t, 4> kCannonMouthFull = {1349, 1347, 1353, 1351};
constexpr std::array<std::uint16_t, 4> kCannonMouthMinimal = {1487, 1486, 1489, 1488};
constexpr std::array<std::uint16_t, 2> kCannonFlameFull = {1384, 1385};
constexpr std::array<std::uint16_t, 2> kCannonFlameMinimal = {1492, 1493};

constexpr float kQuarterDegrees = 90.0F;
constexpr float kHalfTurnDegrees = 180.0F;
constexpr float kThreeQuartersDegrees = 270.0F;
constexpr float kFullTurnDegrees = 360.0F;
constexpr std::uint32_t kFiftyMs = 50;

std::size_t QuarterOf(float view_degrees) {
  if (view_degrees > 0.0F && view_degrees < kQuarterDegrees) {
    return 0;
  }
  if (view_degrees >= kQuarterDegrees && view_degrees < kHalfTurnDegrees) {
    return 1;
  }
  if (view_degrees >= kHalfTurnDegrees && view_degrees < kThreeQuartersDegrees) {
    return 2;
  }
  return 3;
}

// LES TABLES PAR DÉFAUT : le premier effet, puis le second, de compétences
// que l'aiguillage ne nomme pas. Zéro, aucun second.
struct DefaultCastPair {
  std::uint16_t skill_id = 0;
  std::uint16_t first = 0;
  std::uint16_t second = 0;
};

constexpr std::array kDefaultCastPairs = std::to_array<DefaultCastPair>({
    {5315, 2155, 0},    {5319, 2089, 2090}, {5321, 2155, 0},    {5338, 2187, 0},
    {5339, 2163, 2164}, {5340, 2212, 2213}, {5341, 2228, 2229}, {5342, 2220, 2221},
    {5343, 2204, 2205}, {5344, 2247, 2248}, {5345, 2254, 2256}, {5346, 2247, 2248},
    {5347, 2242, 2243}, {5348, 2236, 2237}, {5357, 2061, 2062}, {5369, 2028, 2029},
    {5370, 2043, 2044}, {5371, 2032, 2033}, {5372, 2057, 2058}, {5373, 2135, 2136},
    {5375, 2037, 2038}, {5377, 2100, 2101}, {5378, 2105, 2106}, {5380, 2138, 0},
});

// Les compétences qui ont leur cas, en plus de celles de la table.
constexpr std::array<std::uint16_t, 5> kComputedCases = {
    kNpcPsychicWave, kSorcererDiamondDust, kSorcererPsychicWave, kSorcererElementalCure,
    kMechanicArmsCannon};

glm::vec3 GroundOf(const MapStage& stage, ro::sim::WalkPlanner& planner,
                   const ro::sim::Actor& actor, std::uint32_t at_ms) {
  const ro::sim::FractionalCell cell =
      ro::sim::InterpolatedCell(actor, planner.PathFor(actor), at_ms);
  const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
  return glm::vec3(ground[0], ground[1], ground[2]);
}

std::int32_t TicksOf(std::uint32_t duration_ms) {
  return static_cast<std::int32_t>(duration_ms /
                                   static_cast<std::uint32_t>(ro::render::kEffectTickMs));
}

}  // namespace

bool OwnSkillCases::OnCast(EffectsInPlay& effects, const MapStage& stage,
                           ro::sim::WalkPlanner& planner, const ro::sim::Actor& caster,
                           const ro::sim::Cast& cast) {
  const auto rows = std::ranges::equal_range(kCastSpawns, cast.skill_id, {}, &CastSpawn::skill_id);
  const bool computed = std::ranges::find(kComputedCases, cast.skill_id) != kComputedCases.end();
  if (rows.empty() && !computed) {
    return false;
  }
  const std::uint32_t now_ms = cast.started_ms;
  const bool minimal = effects.minimal_effects_;
  const glm::vec3 feet = GroundOf(stage, planner, caster, now_ms);
  const ro::render::MapEffectFrame client_frame{stage.scene.gnd.width(), stage.scene.gnd.height()};
  const auto play_centre_sound = [&](std::string_view sound) {
    const glm::vec3 centre = client_frame.ToWorld(glm::vec3(0.0F));
    SownNamedSound heard{sound, caster.position};
    heard.point = std::array<float, 3>{centre.x, centre.y, centre.z};
    effects.per_map_.just_sown_named_sounds.push_back(heard);
  };
  const auto on_caster = [&](std::uint32_t effect_id, std::int32_t life_ticks) {
    static_cast<void>(effects.PlayById(effect_id, feet, now_ms, caster.id, /*endless=*/false,
                                       /*life_floor_ticks=*/0, /*announce_sound=*/true,
                                       life_ticks));
  };
  const auto on_target_cell = [&](std::uint32_t effect_id) {
    const std::array<float, 3> cell =
        GroundPositionAt(stage, cast.target_cell.x, cast.target_cell.y);
    static_cast<void>(effects.PlayById(effect_id, glm::vec3(cell[0], cell[1], cell[2]), now_ms,
                                       /*owner_id=*/0, /*endless=*/false, /*life_floor_ticks=*/0,
                                       /*announce_sound=*/true, TicksOf(cast.duration_ms)));
  };

  switch (cast.skill_id) {
    case kNpcPsychicWave:
    case kSorcererPsychicWave:
      on_target_cell(minimal ? kPsychicWaveMinimal : kPsychicWaveFull);
      return true;
    case kSorcererDiamondDust:
      on_target_cell(minimal ? kDiamondDustMinimal : kDiamondDustFull);
      return true;
    case kSorcererElementalCure:
      play_centre_sound(kElementalCureSound);
      // POSÉ À LA PLACE DU LANCEUR, sans le suivre.
      static_cast<void>(effects.PlayById(kElementalCureEffect, feet, now_ms));
      return true;
    case kMechanicArmsCannon: {
      const float body_degrees =
          ClientBodyDegrees(caster, ro::sim::FacingAt(caster, planner.PathFor(caster), now_ms));
      float view_degrees = ro::render::GroundSkillViewAngleDegrees(
          effects.camera_ ? effects.camera_->yaw_degrees : 0.0F, body_degrees);
      if (view_degrees >= kFullTurnDegrees) {
        view_degrees -= kFullTurnDegrees;
      }
      const std::size_t quarter = QuarterOf(view_degrees);
      if (minimal) {
        on_caster(kCannonMouthMinimal[quarter], 0);
        for (const std::uint16_t flame : kCannonFlameMinimal) {
          on_caster(flame, 0);
        }
      } else {
        on_caster(kCannonMouthFull[quarter], 0);
        on_caster(kCannonMouthFull[quarter] + 1U, 0);
        for (const std::uint16_t flame : kCannonFlameFull) {
          on_caster(flame, 0);
        }
      }
      return true;
    }
    default:
      break;
  }

  for (const CastSpawn& spawn : rows) {
    if (spawn.effect_id == 0 || (spawn.under == Under::kFull && minimal) ||
        (spawn.under == Under::kMinimal && !minimal)) {
      continue;
    }
    std::int32_t life_ticks = 0;
    if (spawn.life == CastLife::kCast) {
      life_ticks = TicksOf(cast.duration_ms);
    } else if (spawn.life == CastLife::kCastAndFifty) {
      life_ticks = TicksOf(cast.duration_ms + kFiftyMs);
    }
    on_caster(spawn.effect_id, life_ticks);
  }
  if (cast.skill_id == kAgDestructiveHurricane || cast.skill_id == kAgStormCannon) {
    effects.per_map_.just_sown_named_sounds.push_back(
        SownNamedSound{kBeginSpellSound, caster.position});
  }
  if (cast.skill_id == kCdCompetentia) {
    effects.nodes_->Scopes().Place(cast.skill_id, feet, now_ms, cast.duration_ms, minimal);
  }
  if (cast.skill_id == kAgClimax) {
    const std::uint32_t rest_ms =
        cast.duration_ms > kClimaxSecondDelayMs ? cast.duration_ms - kClimaxSecondDelayMs : 0;
    effects.per_map_.pending_hits.push_back(
        EffectMapMemory::PendingHit{.effect_id = kClimaxSecond,
                                    .target_id = caster.id,
                                    .due_ms = now_ms + kClimaxSecondDelayMs,
                                    .life_exact_ticks = TicksOf(rest_ms)});
  }
  return true;
}

void OwnSkillCases::OnCastByDefault(EffectsInPlay& effects, const MapStage& stage,
                                    ro::sim::WalkPlanner& planner, const ro::sim::Actor& caster,
                                    const ro::sim::Cast& cast) {
  const auto found =
      std::ranges::find(kDefaultCastPairs, cast.skill_id, &DefaultCastPair::skill_id);
  if (found == kDefaultCastPairs.end()) {
    return;
  }
  const glm::vec3 feet = GroundOf(stage, planner, caster, cast.started_ms);
  for (const std::uint16_t effect_id : {found->first, found->second}) {
    if (effect_id != 0) {
      static_cast<void>(effects.PlayById(effect_id, feet, cast.started_ms, caster.id,
                                         /*endless=*/false, /*life_floor_ticks=*/0,
                                         /*announce_sound=*/true, TicksOf(cast.duration_ms)));
    }
  }
}

}  // namespace moonclient
