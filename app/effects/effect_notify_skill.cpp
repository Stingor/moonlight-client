// CE QUE LE PAQUET DES DÉGÂTS D'UNE COMPÉTENCE MONTRE DE LUI-MÊME.
//
// Le client fait passer chaque coup de compétence par trois mains : le message
// du coup à l'attaquant (l'effet de la compétence et son impact), son registre
// des actions, puis une poignée de cas que le paquet traite seul. Ce fichier
// porte ces derniers : un effet sur la cible, parfois sur le lanceur, que rien
// d'autre ne pose. Sans eux, ces compétences ne montraient rien du tout.
//
// Les cas se rangent en deux familles. Ceux qui demandent les DEUX acteurs à
// l'écran, et ceux qui ne demandent que la cible : une unité au sol ou un
// lanceur hors de vue suffisent alors.
//
// ⚠ CE QUE CE PORTAGE NE FAIT PAS ENCORE, et qui reste à faire :
//  - l'état que certains cas donnent au lanceur (NPC_COMET, WL_COMET,
//    RK_DEATHBOUND, WL_FROSTMISTY) ;
//  - MH_GLANZEN_SPIES, dont l'effet est un fichier d'écran que le client
//    tourne d'un angle calculé, ce que nous ne savons pas encore faire.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>

#include "ro/render/effect_class.h"
#include "ro/render/effect_node.h"
#include "ro/render/ground_skill_visual.h"
#include "ro/render/map_effect_math.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "effects/effect_class_view.h"
#include "effects/effect_own_skill_cases.h"
#include "effects/effect_view.h"
#include "world/actor_sounds.h"
#include "world/actor_view.h"
#include "world/hit_popup_rules.h"

namespace moonclient {
namespace {

// Les compétences, par leur numéro du serveur.
constexpr std::uint16_t kAssassinVenomSplasher = 141;
constexpr std::uint16_t kNpcHellJudgement = 662;
constexpr std::uint16_t kNpcComet = 708;
constexpr std::uint16_t kNpcWideCriticalWound = 749;
constexpr std::uint16_t kNpcHellJudgementSecond = 768;
constexpr std::uint16_t kRuneKnightDeathBound = 2003;
constexpr std::uint16_t kRuneKnightWindCutter = 2005;
constexpr std::uint16_t kWarlockComet = 2213;
constexpr std::uint16_t kShadowChaserFeintBomb = 2304;
constexpr std::uint16_t kSuraFallenEmpire = 2329;
constexpr std::uint16_t kSorcererVaretyrSpear = 2454;

// L'effet que porte la compétence, et ce qui le retarde.
struct Spawn {
  std::uint32_t effect_id = 0;
  std::uint32_t delay_ms = 0;
};

// LES CAS SANS CONDITION : la cible reçoit ces effets, dans cet ordre, dès que
// le paquet arrive. Le lanceur peut être hors de vue.
struct TargetCase {
  std::uint16_t skill_id = 0;
  std::array<std::uint32_t, 3> effect_ids{};
};

constexpr std::array<TargetCase, 23> kTargetCases = {{
    {2238, {146, 0, 0}},         // RA_ELECTRICSHOCKER
    {2239, {585, 0, 0}},         // RA_CLUSTERBOMB
    {2253, {583, 0, 0}},         // RA_FIRINGTRAP
    {2254, {135, 0, 0}},         // RA_ICEBOUNDTRAP
    {2259, {583, 49, 0}},        // NC_FLAMELAUNCHER
    {2481, {901, 0, 0}},         // GN_SPORE_EXPLOSION
    {2490, {1361, 0, 0}},        // GN_HELLS_PLANT
    {2517, {1580, 0, 0}},        // SR_HOWLINGOFLION
    {5273, {1824, 1825, 0}},     // CD_ARBITRIUM
    {5294, {1864, 1865, 0}},     // SHC_FATAL_SHADOW_CROW
    {8047, {2329, 0, 0}},        // MH_TWISTER_CUTTER
    {8048, {2326, 2327, 0}},     // MH_ABSOLUTE_ZEPHYR
    {8050, {2337, 0, 0}},        // MH_BLAZING_AND_FURIOUS
    {8053, {2322, 0, 0}},        // MH_TOXIN_OF_MANDARA
    {8054, {2323, 2324, 0}},     // MH_NEEDLE_STINGER
    {8445, {2035, 2036, 0}},     // EM_EL_FLAMEROCK
    {8448, {2019, 2020, 0}},     // EM_EL_AGE_OF_ICE
    {8451, {2098, 2099, 0}},     // EM_EL_STORM_WIND
    {8454, {2104, 0, 0}},        // EM_EL_AVALANCHE
    {8457, {2009, 0, 0}},        // EM_EL_DEADLY_POISON
    {8601, {1934, 1937, 0}},     // ABR_BATTLE_BUSTER
    {8602, {1915, 1917, 0}},     // ABR_DUAL_CANNON_FIRE
    {8605, {1906, 1907, 1909}},  // ABR_INFINITY_BUSTER
}};

const TargetCase* TargetCaseOf(std::uint16_t skill_id) {
  const auto found = std::ranges::find(kTargetCases, skill_id, &TargetCase::skill_id);
  return found == kTargetCases.end() ? nullptr : &*found;
}

// Le niveau que le serveur annonce pour les victimes qu'éclabousse Venom
// Splasher autour de sa cible.
constexpr std::int16_t kSplasherBurstLevel = -1;

// Le délai que Fallen Empire donne à son effet.
constexpr std::uint32_t kFallenEmpireDelayMs = 100;

// LE SOUFFLE D'UN DRAGON : une gueule sur le lanceur, tournée selon l'angle sous
// lequel la vue le montre, puis le souffle sur la cible.
struct Breath {
  // La gueule pour chaque quart de vue : ]0, 90[, [90, 180[, [180, 270[, puis le
  // reste du tour.
  std::array<std::uint32_t, 4> mouth_by_quarter{};
  // Ce qui s'ajoute au numéro de la gueule sous `/mineffect`.
  std::uint32_t minimal_mouth_shift = 0;
  // Le souffle sur la cible, complet puis allégé.
  std::array<std::uint32_t, 2> full_blast{};
  std::uint32_t minimal_blast = 0;
};

constexpr Breath kFireBreath{.mouth_by_quarter = {1381, 1380, 1383, 1382},
                             .minimal_mouth_shift = 141,
                             .full_blast = {1378, 1379},
                             .minimal_blast = 1520};
constexpr Breath kWaterBreath{.mouth_by_quarter = {1389, 1388, 1391, 1390},
                              .minimal_mouth_shift = 139,
                              .full_blast = {1386, 1387},
                              .minimal_blast = 1526};

constexpr std::uint16_t kNpcDragonBreath = 731;
constexpr std::uint16_t kRuneKnightDragonBreath = 2008;
constexpr std::uint16_t kRuneKnightDragonBreathWater = 5004;

// La gueule attend le lanceur, le souffle un peu plus.
constexpr std::uint32_t kBreathMouthDelayMs = 200;
constexpr std::uint32_t kBreathBlastDelayMs = 350;

// LES BRUITS DE CE PAQUET sont poussés au centre de la carte : ils s'entendent
// d'où l'on est, pas de la cible. Le souffle, les pièges et le canon partagent
// le premier.
constexpr std::string_view kBlastSound = "effect\\폭염룡.wav";
constexpr std::string_view kShockwaveTrapSound = "effect\\hunter_shockwavetrap.wav";

constexpr std::uint16_t kRangerElectricShocker = 2238;
constexpr std::uint16_t kRangerFiringTrap = 2253;
constexpr std::uint16_t kMechanicFlameLauncher = 2259;

// LES RAYONS DE GENÈSE attendent que le lanceur ait passé une image de son
// geste : cent millisecondes par pas de sa cadence, et cent de plus.
constexpr std::uint16_t kNpcRayOfGenesis = 737;
constexpr std::uint16_t kRoyalGuardRayOfGenesis = 2321;
constexpr std::uint16_t kMinstrelMetallicSound = 2413;
constexpr float kRayDelayPerCadenceStepMs = 100.0F;
constexpr float kRayDelayFloorMs = 100.0F;

// LA FLÈCHE D'UNLUCKY RUSH, une classe qui vole du lanceur à sa cible.
constexpr std::uint16_t kAbyssUnluckyRush = 5315;
constexpr std::uint32_t kCrimsonArrow = 2154;

// LE CANON DE CHARIOT : deux effets sur le lanceur, puis deux sur la cible.
constexpr std::uint16_t kGeneticCartCannon = 2477;
constexpr std::uint32_t kCannonCasterDelayMs = 200;
constexpr std::uint32_t kCannonTargetDelayMs = 400;

// LES ÉCLATS DE JACK FROST : trois, à un rayon de cinq à quatorze unités du
// client autour de la cible, l'un après l'autre d'un pas de quatre-vingts à
// cent vingt degrés.
constexpr std::uint16_t kWarlockJackFrost = 2204;
constexpr std::int32_t kFrostShards = 3;
constexpr std::uint32_t kFrostStepDraws = 5;
constexpr std::uint32_t kFrostStepBase = 8;
constexpr float kFrostStepDegreesPerUnit = 10.0F;
constexpr std::uint32_t kFrostRadiusDraws = 10;
constexpr float kFrostRadiusBase = 5.0F;
constexpr std::uint32_t kFrostShardKinds = 3;
constexpr std::uint32_t kFrostFirstShard = 1443;
constexpr std::uint32_t kFrostMinimalShift = 97;

// LES OBUS D'ARMS CANNON : une paire tombe à quatre-vingt-dix millisecondes
// d'intervalle sur une case voisine de la cible, jusqu'à quatre cent cinquante,
// où la dernière frappe la cible même.
constexpr std::uint16_t kMechanicArmsCannon = 2261;
constexpr std::uint32_t kShellStepMs = 90;
constexpr std::uint32_t kShellLastMs = 450;
constexpr std::uint32_t kShellOffsetDraws = 3;
constexpr float kShellOffsetUnit = 5.0F;
constexpr float kShellFarOffset = 10.0F;
constexpr std::uint32_t kShellQuarterDraws = 4;
constexpr std::array<std::uint32_t, 2> kFullShell = {1355, 1356};
constexpr std::array<std::uint32_t, 2> kMinimalShell = {1490, 1491};

// Le pi du client, tel qu'il l'écrit.
constexpr float kClientPi = 3.141592F;

constexpr float kQuarterDegrees = 90.0F;
constexpr float kHalfTurnDegrees = 180.0F;
constexpr float kThreeQuartersDegrees = 270.0F;
constexpr float kFullTurnDegrees = 360.0F;

// Le quart de vue d'un angle déjà ramené une fois dans le tour.
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

}  // namespace

void OwnSkillCases::OnNotifySkill(EffectsInPlay& effects, const ro::sim::World& world,
                                  const MapStage& stage, ro::sim::WalkPlanner& planner,
                                  const ro::sim::Strike& strike) {
  const ro::sim::Actor* const target = world.Find(strike.target);
  if (target == nullptr) {
    return;
  }
  // UNE UNITÉ AU SOL N'EST PAS UN LANCEUR pour ces cas : seuls ceux qui ne
  // demandent que la cible jouent.
  const ro::sim::Actor* const caster =
      strike.by_ground_unit ? nullptr : world.Find(strike.attacker);
  const std::uint32_t now_ms = strike.started_ms;

  const auto spawn_on = [&](const ro::sim::Actor& bearer, Spawn spawn) {
    if (spawn.delay_ms != 0) {
      effects.per_map_.pending_hits.push_back(EffectMapMemory::PendingHit{
          .effect_id = spawn.effect_id, .target_id = bearer.id, .due_ms = now_ms + spawn.delay_ms});
      return;
    }
    const ro::sim::FractionalCell cell =
        ro::sim::InterpolatedCell(bearer, planner.PathFor(bearer), now_ms);
    const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
    static_cast<void>(effects.PlayById(spawn.effect_id, glm::vec3(ground[0], ground[1], ground[2]),
                                       now_ms, bearer.id));
  };
  const auto on_target = [&](std::initializer_list<std::uint32_t> effect_ids) {
    for (const std::uint32_t effect_id : effect_ids) {
      spawn_on(*target, Spawn{.effect_id = effect_id});
    }
  };
  const ro::render::MapEffectFrame client_frame{stage.scene.gnd.width(), stage.scene.gnd.height()};
  const auto play_centre_sound = [&](std::string_view sound) {
    const glm::vec3 centre = client_frame.ToWorld(glm::vec3(0.0F));
    SownNamedSound heard{sound, target->position};
    heard.point = std::array<float, 3>{centre.x, centre.y, centre.z};
    effects.per_map_.just_sown_named_sounds.push_back(heard);
  };
  // LA CIBLE DANS LE REPÈRE DU CLIENT, où les éclats se calculent.
  const auto target_in_client = [&] {
    const ro::sim::FractionalCell cell =
        ro::sim::InterpolatedCell(*target, planner.PathFor(*target), now_ms);
    const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
    return client_frame.ToNative(glm::vec3(ground[0], ground[1], ground[2]));
  };
  // Les tirages viennent de l'instant et de la cible, comme les autres tirages
  // du client : un paquet rejoué se rejoue à l'identique.
  std::uint32_t draws = now_ms ^ target->id;
  const auto draw = [&draws](std::uint32_t range) {
    return ro::render::NextEffectNodeDraw(draws) % range;
  };

  // LES CAS QUI DEMANDENT LES DEUX ACTEURS.
  if (caster != nullptr) {
    switch (strike.skill_id) {
      case kNpcHellJudgement:
      case kNpcHellJudgementSecond:
        if (caster->id != target->id) {
          on_target({372});
        }
        return;
      case kNpcComet:
        on_target({92});
        return;
      case kNpcWideCriticalWound:
        // Seul le joueur de ce client voit la blessure, de chaque côté.
        if (caster->id == effects.player_id_) {
          spawn_on(*caster, Spawn{.effect_id = 1121});
        }
        if (target->id == effects.player_id_) {
          on_target({1121});
        }
        return;
      case kRuneKnightDeathBound:
        on_target({183});
        return;
      case kWarlockComet:
        if (effects.minimal_effects_) {
          on_target({1534, 1535});
        } else {
          on_target({1413, 1414});
        }
        return;
      case kSorcererVaretyrSpear:
        on_target({1561, 1562});
        return;
      case kNpcRayOfGenesis:
      case kRoyalGuardRayOfGenesis:
      case kMinstrelMetallicSound: {
        // La cadence du geste que le coup vient de donner au lanceur ; sans vue
        // des acteurs, aucune, et le délai se réduit à son plancher.
        float cadence_steps = 0.0F;
        if (effects.actor_view_.pace_of) {
          if (const auto pace =
                  effects.actor_view_.pace_of(*caster, strike.skill_id, strike.attack_motion_ms)) {
            cadence_steps = pace->cadence_steps;
          }
        }
        const auto delay_ms = static_cast<std::uint32_t>(
            (cadence_steps * kRayDelayPerCadenceStepMs) + kRayDelayFloorMs);
        spawn_on(*target,
                 Spawn{.effect_id = strike.skill_id == kMinstrelMetallicSound ? 1583U : 1427U,
                       .delay_ms = delay_ms});
        return;
      }
      case kAbyssUnluckyRush: {
        // La flèche part du lanceur, liée à la cible qu'elle rejoint.
        const auto ground_of = [&](const ro::sim::Actor& actor) {
          const ro::sim::FractionalCell cell =
              ro::sim::InterpolatedCell(actor, planner.PathFor(actor), now_ms);
          const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
          return glm::vec3(ground[0], ground[1], ground[2]);
        };
        EffectClassesInPlay::Birth birth;
        birth.effect_id = kCrimsonArrow;
        birth.owner_id = caster->id;
        birth.ground_position = ground_of(*caster);
        birth.bound_id = target->id;
        birth.bound_ground_position = ground_of(*target);
        static_cast<void>(effects.classes_.Spawn(birth, now_ms));
        return;
      }
      case kGeneticCartCannon:
        if (effects.minimal_effects_) {
          spawn_on(*caster, Spawn{.effect_id = 1498, .delay_ms = kCannonCasterDelayMs});
          spawn_on(*target, Spawn{.effect_id = 1496, .delay_ms = kCannonTargetDelayMs});
          spawn_on(*target, Spawn{.effect_id = 1497, .delay_ms = kCannonTargetDelayMs});
        } else {
          spawn_on(*caster, Spawn{.effect_id = 1360, .delay_ms = kCannonCasterDelayMs});
          spawn_on(*caster, Spawn{.effect_id = 1361, .delay_ms = kCannonCasterDelayMs});
          spawn_on(*target, Spawn{.effect_id = 1358, .delay_ms = kCannonTargetDelayMs});
          spawn_on(*target, Spawn{.effect_id = 1359, .delay_ms = kCannonTargetDelayMs});
        }
        return;
      default:
        break;
    }
  }

  // LES CAS QUI NE DEMANDENT QUE LA CIBLE.
  switch (strike.skill_id) {
    case kAssassinVenomSplasher:
      // Seules les victimes alentour reçoivent l'éclat ; la cible centrale
      // arrive sous un autre niveau.
      if (strike.announced_level == kSplasherBurstLevel) {
        on_target({129});
      }
      return;
    case kRuneKnightWindCutter:
      // Le lanceur hors de vue compte comme distinct de la cible.
      if (caster == nullptr || caster->id != target->id) {
        on_target({1693});
      }
      return;
    case kShadowChaserFeintBomb:
      if (effects.minimal_effects_) {
        on_target({1463, 1464});
      } else {
        on_target({1312, 1313});
      }
      return;
    case kSuraFallenEmpire:
      spawn_on(*target, Spawn{.effect_id = 1572, .delay_ms = kFallenEmpireDelayMs});
      return;
    case kNpcDragonBreath:
    case kRuneKnightDragonBreath:
    case kRuneKnightDragonBreathWater: {
      // Sans lanceur à l'écran, ni gueule ni souffle.
      if (caster == nullptr) {
        return;
      }
      const Breath& breath =
          strike.skill_id == kRuneKnightDragonBreathWater ? kWaterBreath : kFireBreath;
      const float body_degrees =
          ClientBodyDegrees(*caster, ro::sim::FacingAt(*caster, planner.PathFor(*caster), now_ms));
      float view_degrees = ro::render::GroundSkillViewAngleDegrees(
          effects.camera_ ? effects.camera_->yaw_degrees : 0.0F, body_degrees);
      if (view_degrees >= kFullTurnDegrees) {
        view_degrees -= kFullTurnDegrees;
      }
      std::uint32_t mouth = breath.mouth_by_quarter[QuarterOf(view_degrees)];
      if (effects.minimal_effects_) {
        mouth += breath.minimal_mouth_shift;
      }
      // UNE SEULE GUEULE À LA FOIS sur le lanceur, même attendue.
      const bool awaited = std::ranges::any_of(
          effects.per_map_.pending_hits, [&](const EffectMapMemory::PendingHit& due) {
            return due.effect_id == mouth && due.target_id == caster->id;
          });
      if (!awaited && !effects.Carries(mouth, caster->id)) {
        spawn_on(*caster, Spawn{.effect_id = mouth, .delay_ms = kBreathMouthDelayMs});
      }
      if (effects.minimal_effects_) {
        spawn_on(*target,
                 Spawn{.effect_id = breath.minimal_blast, .delay_ms = kBreathBlastDelayMs});
      } else {
        for (const std::uint32_t blast : breath.full_blast) {
          spawn_on(*target, Spawn{.effect_id = blast, .delay_ms = kBreathBlastDelayMs});
        }
      }
      play_centre_sound(kBlastSound);
      return;
    }
    case kWarlockJackFrost: {
      const glm::vec3 centre = target_in_client();
      const float step_degrees =
          static_cast<float>(draw(kFrostStepDraws) + kFrostStepBase) * kFrostStepDegreesPerUnit;
      for (std::int32_t shard = 0; shard < kFrostShards; ++shard) {
        const float radius = static_cast<float>(draw(kFrostRadiusDraws)) + kFrostRadiusBase;
        ro::render::NativeMatrix turn = ro::render::kNativeIdentity;
        ro::render::MultiplyRotationY(
            turn, ((static_cast<float>(shard) * step_degrees) / kHalfTurnDegrees) * kClientPi);
        const glm::vec3 point = centre + ro::render::Transform(glm::vec3(radius, 0.0F, 0.0F), turn);
        std::uint32_t effect_id = kFrostFirstShard + draw(kFrostShardKinds);
        if (effects.minimal_effects_) {
          effect_id += kFrostMinimalShift;
        }
        static_cast<void>(effects.PlayById(effect_id, client_frame.ToWorld(point), now_ms));
      }
      return;
    }
    case kMechanicArmsCannon: {
      const std::array<std::uint32_t, 2>& shell =
          effects.minimal_effects_ ? kMinimalShell : kFullShell;
      const glm::vec3 centre = target_in_client();
      for (std::uint32_t delay_ms = kShellStepMs;; delay_ms += kShellStepMs) {
        float across = kShellOffsetUnit * static_cast<float>(draw(kShellOffsetDraws));
        float along = kShellOffsetUnit * static_cast<float>(draw(kShellOffsetDraws));
        const std::uint32_t quarter = draw(kShellQuarterDraws);
        // Jamais dans le coin le plus éloigné : l'un des deux écarts s'y réduit.
        if (across == kShellFarOffset && along == kShellFarOffset) {
          if (draw(2) == 0) {
            across = kShellOffsetUnit;
          } else {
            along = kShellOffsetUnit;
          }
        }
        if (quarter == 0 || quarter == 2) {
          across = -across;
        }
        if (quarter == 1 || quarter == 2) {
          along = -along;
        }
        const glm::vec3 point = client_frame.ToWorld(centre + glm::vec3(across, 0.0F, along));
        for (const std::uint32_t effect_id : shell) {
          effects.per_map_.pending_hits.push_back(EffectMapMemory::PendingHit{
              .effect_id = effect_id, .due_ms = now_ms + delay_ms, .ground_point = point});
        }
        if (delay_ms + kShellStepMs >= kShellLastMs) {
          break;
        }
      }
      on_target({shell[0], shell[1]});
      play_centre_sound(kBlastSound);
      return;
    }
    default:
      break;
  }
  if (const TargetCase* const found = TargetCaseOf(strike.skill_id); found != nullptr) {
    for (const std::uint32_t effect_id : found->effect_ids) {
      if (effect_id != 0) {
        spawn_on(*target, Spawn{.effect_id = effect_id});
      }
    }
  }
  if (strike.skill_id == kRangerElectricShocker) {
    play_centre_sound(kShockwaveTrapSound);
  } else if (strike.skill_id == kRangerFiringTrap || strike.skill_id == kMechanicFlameLauncher) {
    play_centre_sound(kBlastSound);
  }
}

}  // namespace moonclient
