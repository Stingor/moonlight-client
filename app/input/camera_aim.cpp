#include "input/camera_aim.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

#include "ro/render/actor_effect_table.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_launch.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/sprite_billboard.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "input/camera_chase.h"

namespace moonclient {
namespace {

// LA COMMANDE QUE LA SECOUSSE ENVOIE au mode de jeu.
//
// Le mode de jeu aiguille ses commandes par numéro : la commande 0x7A est celle
// qui remue la caméra.
constexpr std::uint16_t kScreenQuakeCommand = 0x7A;

// COMBIEN DE TEMPS UNE SECOUSSE DURE, en millisecondes. Le client la pose dans
// le champ de durée de sa caméra quand le deuxième paramètre vaut zéro, ce qui
// est le cas d'`EF_SCREEN_QUAKE`.
constexpr std::uint32_t kShakeMs = 650;

// ⚠ ET L'EFFET LA RELANCE UN TIC SUR CINQ, jusqu'à sa borne de vie. Chaque
// relance dure six cent cinquante millisecondes et la suivante arrive
// quatre-vingts millisecondes plus tard : elles se recouvrent toutes, la
// secousse est donc CONTINUE et s'arrête une durée entière après la dernière.
// La compter relance par relance donnerait la même image, en plus compliqué.
constexpr std::uint32_t kRetriggerTicks = 5;

// DE COMBIEN LA CAMÉRA BOUGE. Les trois nombres sont ceux que le client pousse
// à sa secousse de caméra : un pour le point visé, vingt-quatre centièmes
// pour le recul, deux dixièmes pour l'élévation.
constexpr float kLookAtUnits = 1.0F;
constexpr float kDistanceUnits = 0.24F;
constexpr float kPitchDegrees = 0.2F;

// LE BRUIT QUI S'AJOUTE À CHACUN. Le client tire un entier de zéro à quatre,
// lui retranche deux et divise — par dix pour le point visé et le recul, par
// quinze pour l'élévation.
constexpr std::uint32_t kNoiseValues = 5;
constexpr std::int32_t kNoiseCenter = 2;
constexpr float kNoiseDivisor = 10.0F;
constexpr float kPitchNoiseDivisor = 15.0F;

// Quand la secousse que cet effet demande COMMENCE et FINIT, en tics depuis
// son départ. Rien s'il n'en demande aucune.
//
// ⚠ DEUX FAÇONS DE LA DEMANDER, et les confondre fait trembler l'écran au
// mauvais moment. `EF_SCREEN_QUAKE` la relance UN TIC SUR CINQ depuis son
// premier tic et jusqu'à sa borne de vie ; `EF_MAGICCRASHER` et
// `EF_FALCONASSAULT` la demandent UNE SEULE FOIS, à leur trentième tic — une
// demi-seconde après leur départ, quand le coup touche. Le tic est celui du
// client, et la table le porte.
[[nodiscard]] std::optional<std::pair<std::uint32_t, std::uint32_t>> Window(
    const ro::render::ActorEffectChange& change) {
  if (change.command != kScreenQuakeCommand) {
    return std::nullopt;
  }
  const auto to_ms = [](std::uint32_t ticks) {
    return ticks * static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  };
  if (change.command_tick != ro::render::kCommandRepeats) {
    const std::uint32_t start = to_ms(static_cast<std::uint32_t>(change.command_tick));
    return std::make_pair(start, start + kShakeMs);
  }
  if (change.life_ticks == 0) {
    return std::nullopt;
  }
  const std::uint32_t last_retrigger = (change.life_ticks - 1) / kRetriggerTicks * kRetriggerTicks;
  return std::make_pair(0U, to_ms(last_retrigger) + kShakeMs);
}

// Vrai tant qu'un effet en cours réclame la secousse.
[[nodiscard]] bool Requested(const ro::sim::World& world, std::uint32_t now_ms) {
  for (const auto& [id, actor] : world.actors()) {
    for (const ro::sim::PlayedEffect& effect : actor.effects) {
      const ro::render::ActorEffectChange* change = ro::render::ActorEffectChangeFor(effect.id);
      if (change == nullptr || now_ms < effect.since_ms) {
        continue;
      }
      const auto window = Window(*change);
      if (!window.has_value()) {
        continue;
      }
      const std::uint32_t age_ms = now_ms - effect.since_ms;
      if (age_ms >= window->first && age_ms < window->second) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

void CameraTracking::Follow(ro::render::Camera& camera, const glm::vec3& target,
                            std::uint32_t now_ms) {
  const std::uint32_t elapsed_ms = started_ && now_ms > last_ms_ ? now_ms - last_ms_ : 0;
  last_ms_ = now_ms;
  if (fixed || !started_ || glm::length(target - look_at_) > TeleportUnits()) {
    look_at_ = target;
    started_ = true;
  } else {
    const float remaining = RemainingLagAfter(static_cast<float>(elapsed_ms));
    look_at_ += (target - look_at_) * (1.0F - remaining);
  }
  camera.look_at = look_at_;
}

void ScreenShake::Undo(ro::render::Camera& camera) {
  camera.distance -= distance_offset_;
  camera.pitch_degrees -= pitch_offset_;
  distance_offset_ = 0.0F;
  pitch_offset_ = 0.0F;
}

void ScreenShake::Apply(ro::render::Camera& camera, const ro::sim::World& world,
                        std::uint32_t now_ms) {
  look_at_offset_ = glm::vec3(0.0F, 0.0F, 0.0F);
  if (!enabled) {
    requested_until_ms_ = 0;
    return;
  }
  if (!Requested(world, now_ms) && now_ms >= requested_until_ms_) {
    return;
  }
  // Un mélange bon marché : on ne lui demande que de ne pas se répéter.
  const auto draw = [this](float divisor) {
    seed_ ^= seed_ << 13U;
    seed_ ^= seed_ >> 17U;
    seed_ ^= seed_ << 5U;
    return static_cast<float>(static_cast<std::int32_t>(seed_ % kNoiseValues) - kNoiseCenter) /
           divisor;
  };
  // 🔴 LE POINT VISÉ BOUGE LE LONG DE `(cos, 0, −sin)` DE LA LONGITUDE DU CLIENT,
  // dans son repère : l'abscisse d'un premier tirage, la profondeur d'un
  // SECOND. Le client lit ces deux valeurs dans des tables dont les noms sont
  // échangés, sur sa longitude tronquée — l'opposée de la nôtre, un degré de
  // travers quand elle est négative. Puis viennent le recul et l'élévation, dans
  // cet ordre ; son élévation monte quand notre inclinaison descend.
  const float longitude =
      ro::render::NativeTableRadians(static_cast<std::int32_t>(-camera.yaw_degrees));
  const float le_long_x = sign_ * (kLookAtUnits + draw(kNoiseDivisor));
  const float le_long_z = sign_ * (kLookAtUnits + draw(kNoiseDivisor));
  look_at_offset_ = ro::render::EffectClientOffsetInWorld(std::cos(longitude) * le_long_x, 0.0F,
                                                          -std::sin(longitude) * le_long_z);
  distance_offset_ = sign_ * (kDistanceUnits + draw(kNoiseDivisor));
  pitch_offset_ = -sign_ * (kPitchDegrees + draw(kPitchNoiseDivisor));
  camera.look_at += look_at_offset_;
  camera.distance += distance_offset_;
  camera.pitch_degrees = std::clamp(camera.pitch_degrees + pitch_offset_,
                                    -ro::render::kMaxPitchDegrees, ro::render::kMaxPitchDegrees);
  sign_ = -sign_;
}

void AimCamera(ro::render::Camera& camera, const MapStage& stage, const ro::sim::World& world,
               ro::sim::WalkPlanner& planner, std::uint32_t now_ms, CameraAim& aim) {
  aim.shake.Undo(camera);
  const ro::sim::Actor* walking = world.player();
  if (walking != nullptr) {
    // Le suivi réécrit le point visé EN ENTIER : ce que la secousse y avait posé
    // s'en va de lui-même, et le retirer une seconde fois déplacerait la caméra
    // pour de bon. ⚠ Et il le réécrit depuis SA propre mémoire, non depuis ce
    // que la caméra porte : sans cela le tremblement entrerait dans le
    // rattrapage et la caméra dériverait avec lui.
    const ro::sim::FractionalCell cell =
        ro::sim::InterpolatedCell(*walking, planner.PathFor(*walking), now_ms);
    const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
    aim.tracking.Follow(camera, glm::vec3(ground[0], ground[1], ground[2]), now_ms);
  } else {
    camera.look_at -= aim.shake.look_at_offset();
  }
  aim.shake.Apply(camera, world, now_ms);
}

}  // namespace moonclient
