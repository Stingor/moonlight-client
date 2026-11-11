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
//  - les sons propres de RA_ELECTRICSHOCKER, RA_FIRINGTRAP et NC_FLAMELAUNCHER ;
//  - les cas qui calculent : le souffle des dragons par quart de vue, les
//    éclats éparpillés de WL_JACKFROST et NC_ARMSCANNON, la rotation de
//    MH_GLANZEN_SPIES, le délai tiré de la cadence du lanceur des rayons de
//    genèse, et le canon de GN_CARTCANNON.

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>

#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "effects/effect_view.h"
#include "world/actor_view.h"

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

}  // namespace

void EffectsInPlay::PlayNotifySkillCases(const ro::sim::World& world, const MapStage& stage,
                                         ro::sim::WalkPlanner& planner,
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
      per_map_.pending_hits.push_back(PendingHit{
          .effect_id = spawn.effect_id, .target_id = bearer.id, .due_ms = now_ms + spawn.delay_ms});
      return;
    }
    const ro::sim::FractionalCell cell =
        ro::sim::InterpolatedCell(bearer, planner.PathFor(bearer), now_ms);
    const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
    static_cast<void>(
        PlayById(spawn.effect_id, glm::vec3(ground[0], ground[1], ground[2]), now_ms, bearer.id));
  };
  const auto on_target = [&](std::initializer_list<std::uint32_t> effect_ids) {
    for (const std::uint32_t effect_id : effect_ids) {
      spawn_on(*target, Spawn{.effect_id = effect_id});
    }
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
        if (caster->id == player_id_) {
          spawn_on(*caster, Spawn{.effect_id = 1121});
        }
        if (target->id == player_id_) {
          on_target({1121});
        }
        return;
      case kRuneKnightDeathBound:
        on_target({183});
        return;
      case kWarlockComet:
        if (minimal_effects_) {
          on_target({1534, 1535});
        } else {
          on_target({1413, 1414});
        }
        return;
      case kSorcererVaretyrSpear:
        on_target({1561, 1562});
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
      if (minimal_effects_) {
        on_target({1463, 1464});
      } else {
        on_target({1312, 1313});
      }
      return;
    case kSuraFallenEmpire:
      spawn_on(*target, Spawn{.effect_id = 1572, .delay_ms = kFallenEmpireDelayMs});
      return;
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
}

}  // namespace moonclient
