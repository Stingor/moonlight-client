#include "world/actor_models.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/ascii.h"
#include "ro/core/log.h"
#include "ro/render/actor_pick.h"
#include "ro/render/effect_frame.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

using ro::render::ActorModelMotion;

constexpr float kMillisecondsPerSecond = 1000.0F;
// Un fichier d'animation n'en porte qu'une, ou il ne sert pas.
constexpr std::size_t kAnimationsPerMotionFile = 1;
// Au-delà, les tics de rotation en retard ne se rejouent plus : trente suffisent
// à fermer le plus grand écart, et une fenêtre restée longtemps en arrière-plan
// n'a pas à en rejouer des milliers.
constexpr std::uint32_t kMaxCatchUpTicks = 64;
// Le tic du jeu : la rotation d'un modèle, sa blessure et son reste le comptent.
constexpr auto kGameTickMs = static_cast<std::uint32_t>(ro::render::kEffectTickMs);

float Seconds(std::uint32_t since_ms, std::uint32_t now_ms) {
  return now_ms > since_ms ? static_cast<float>(now_ms - since_ms) / kMillisecondsPerSecond : 0.0F;
}

glm::vec3 FeetOf(const MapStage& stage, const ro::sim::Actor& actor, ro::sim::WalkPlanner& planner,
                 std::uint32_t now_ms) {
  const ro::sim::FractionalCell cell =
      ro::sim::InterpolatedCell(actor, planner.PathFor(actor), now_ms);
  const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
  return {ground[0], ground[1], ground[2]};
}

// Le repère du client sur cette carte : les lumières des modèles se calculent
// depuis l'oeil, en coordonnées absolues.
ro::render::MapEffectFrame MapFrameOf(const MapStage& stage) {
  return ro::render::MapEffectFrame{stage.scene.gnd.width(), stage.scene.gnd.height()};
}

}  // namespace

const ActorModels::Loaded* ActorModels::Model(std::string_view resource_name) {
  const std::string key = ro::core::AsciiLower(resource_name);
  if (const auto found = models_.find(key); found != models_.end()) {
    return found->second.get();
  }
  auto loaded = std::make_unique<Loaded>();
  loaded->index = ro::render::ActorModelIndex(resource_name);
  const std::string path = std::string(ro::render::kActorModelFolder) + std::string(resource_name);
  auto bytes = vfs_->Read(path);
  if (!bytes) {
    ro::core::LogWarning("modèle d'acteur : {} absent", path);
  } else if (auto scene = ro::formats::ReadGrannyScene(*bytes); !scene) {
    ro::core::LogWarning("modèle d'acteur : {} illisible — {}", path,
                         ro::core::Describe(scene.error()));
  } else {
    loaded->resource = ro::render::MakeActorModelResource(std::move(*scene), resource_name);
  }
  return models_.emplace(key, std::move(loaded)).first->second.get();
}

const ro::formats::GrannyAnimation* ActorModels::Animation(const ro::render::SpriteCatalog& catalog,
                                                           const Loaded& model,
                                                           ActorModelMotion motion) {
  if (!model.index.has_value()) {
    return nullptr;
  }
  // L'ATTENTE vient du fichier que la table des modèles donne à ce numéro ; les
  // autres mouvements, du dossier des animations. Le drapeau de guilde n'en
  // cherche aucune.
  const std::optional<std::string> path =
      motion == ActorModelMotion::kStand ? ro::render::ActorModelStandPath(catalog, *model.index)
                                         : ro::render::ActorModelMotionPath(*model.index, motion);
  if (!path.has_value()) {
    return nullptr;
  }
  const std::string key = ro::core::AsciiLower(*path);
  auto found = motion_files_.find(key);
  if (found == motion_files_.end()) {
    std::optional<ro::formats::GrannyScene> scene;
    if (auto bytes = vfs_->Read(*path); bytes) {
      if (auto read = ro::formats::ReadGrannyScene(*bytes); read) {
        scene = std::move(*read);
      }
    }
    found = motion_files_.emplace(key, std::move(scene)).first;
  }
  // ⚠ UNE SEULE ANIMATION PAR FICHIER, ou aucune n'est retenue.
  if (!found->second.has_value() || found->second->animations.size() != kAnimationsPerMotionFile) {
    return nullptr;
  }
  return &found->second->animations.front();
}

const ro::render::ActorModelAnimation* ActorModels::Bound(const ro::render::SpriteCatalog& catalog,
                                                          const Loaded& model,
                                                          ActorModelMotion motion) {
  const auto key = std::pair{&model, motion};
  auto found = bindings_.find(key);
  if (found == bindings_.end()) {
    std::optional<ro::render::ActorModelAnimation> bound;
    if (const ro::formats::GrannyAnimation* animation = Animation(catalog, model, motion);
        animation != nullptr && model.resource.has_value()) {
      bound = ro::render::BindActorModelAnimation(model.resource->scene, *animation);
    }
    found = bindings_.emplace(key, std::move(bound)).first;
  }
  return found->second.has_value() ? &*found->second : nullptr;
}

void ActorModels::Turn(Tracked& tracked, float body_degrees, std::uint32_t now_ms) {
  if (!tracked.yaw_set) {
    tracked.yaw_degrees = ro::render::InitialActorModelYaw(body_degrees);
    tracked.yaw_ticked_ms = now_ms;
    tracked.yaw_set = true;
    return;
  }
  if (now_ms <= tracked.yaw_ticked_ms) {
    return;
  }
  const std::uint32_t ticks = (now_ms - tracked.yaw_ticked_ms) / kGameTickMs;
  for (std::uint32_t tick = 0; tick < std::min(ticks, kMaxCatchUpTicks); ++tick) {
    tracked.yaw_degrees = ro::render::TurnActorModelYaw(tracked.yaw_degrees, body_degrees);
  }
  tracked.yaw_ticked_ms += ticks * kGameTickMs;
}

void ActorModels::Follow(const Frame& frame, const ro::sim::Actor& actor, Tracked& tracked) {
  const std::uint32_t now_ms = frame.now_ms;
  const ro::sim::WalkPath& path = frame.planner->PathFor(actor);
  tracked.feet = FeetOf(*frame.stage, actor, *frame.planner, now_ms);
  // LA MARCHE COMMENCE ET FINIT : chaque bascule repart de zéro.
  const bool walking = ro::sim::IsWalking(actor, path, now_ms);
  if (walking != tracked.walking) {
    tracked.walking = walking;
    tracked.motion = walking ? ActorModelMotion::kMove : ActorModelMotion::kStand;
    tracked.motion_since_ms = now_ms;
    tracked.attack_body_degrees.reset();
  }
  // UNE ATTAQUE : le mouvement repart de zéro à son premier instant, à la
  // cadence que le coup annonce. ⚠ LE CORPS NE SE TOURNE VERS LA CIBLE QU'EN
  // ENTRANT dans l'attaque : un coup qui en suit un autre garde l'angle du
  // premier, l'acteur restant « en attaque » jusqu'à une autre action.
  if (actor.attack.has_value() && actor.attack->started_ms <= now_ms &&
      (!tracked.attack_seen || actor.attack->started_ms != tracked.attack_seen_ms)) {
    const bool already_attacking = tracked.motion == ActorModelMotion::kAttack;
    tracked.attack_seen = true;
    tracked.attack_seen_ms = actor.attack->started_ms;
    tracked.motion = ActorModelMotion::kAttack;
    tracked.motion_since_ms = actor.attack->started_ms;
    tracked.attack_speed = ro::render::ActorModelAttackSpeed(actor.attack->attack_motion_ms);
    if (!already_attacking) {
      tracked.attack_body_degrees.reset();
    }
    const ro::sim::Actor* target =
        already_attacking ? nullptr : frame.world->Find(actor.attack->target);
    if (target != nullptr) {
      const glm::vec3 from = ro::render::ClientFromWorld(tracked.feet);
      const glm::vec3 to =
          ro::render::ClientFromWorld(FeetOf(*frame.stage, *target, *frame.planner, now_ms));
      tracked.attack_body_degrees = ro::render::ActorBodyDegreesToward(from.x, from.z, to.x, to.z);
    }
  }
  // UN COUP REÇU QUI FAIT BRONCHER : la blessure se montre un tic, puis
  // l'attente repart de zéro.
  if (actor.hit.has_value() && actor.hit->flinches && frame.wardrobe != nullptr) {
    const auto lands_ms = actor.hit->started_ms + static_cast<std::uint32_t>(HitLandsAtMs(
                                                      *frame.wardrobe, *frame.world, *actor.hit));
    if (lands_ms <= now_ms && (!tracked.hurt_seen || lands_ms != tracked.hurt_seen_ms)) {
      tracked.hurt_seen = true;
      tracked.hurt_seen_ms = lands_ms;
      tracked.motion = ActorModelMotion::kDamage;
      tracked.motion_since_ms = lands_ms;
      tracked.attack_body_degrees.reset();
    }
  }
  if (tracked.motion == ActorModelMotion::kDamage &&
      now_ms >= tracked.motion_since_ms + kGameTickMs) {
    tracked.motion = ActorModelMotion::kStand;
    tracked.motion_since_ms += kGameTickMs;
  }
  const float body = tracked.attack_body_degrees.has_value()
                         ? *tracked.attack_body_degrees
                         : ro::render::ActorBodyDegrees(ro::sim::FacingAt(actor, path, now_ms));
  Turn(tracked, body, now_ms);
}

void ActorModels::Paint(const Frame& frame, const ro::sim::Actor& actor, Tracked& tracked) {
  const ro::render::ActorModelResource& resource = *tracked.model->resource;
  const ro::render::ActorModelAnimation* bound =
      Bound(*frame.catalog, *tracked.model, tracked.motion);
  float clock = ro::render::kActorModelClockRate * Seconds(tracked.motion_since_ms, frame.now_ms);
  float local = 0.0F;
  if (tracked.motion == ActorModelMotion::kAttack && bound != nullptr) {
    // L'attaque se joue une fois, plus vite si sa cadence est brève ; l'attente
    // prend ensuite la suite de la même horloge.
    const float duration = bound->animation->duration_seconds;
    const float quick = tracked.attack_speed < 1.0F ? clock / tracked.attack_speed : clock;
    if (quick < duration) {
      local = quick;
    } else {
      clock = duration + (clock - duration * tracked.attack_speed);
      bound = Bound(*frame.catalog, *tracked.model, ActorModelMotion::kStand);
      local = bound != nullptr ? ro::render::AnimationLocalSeconds(
                                     clock, bound->animation->duration_seconds, false)
                               : 0.0F;
    }
  } else if (bound != nullptr) {
    const bool once = tracked.motion == ActorModelMotion::kDamage;
    local = ro::render::AnimationLocalSeconds(clock, bound->animation->duration_seconds, once);
  }
  ro::render::ActorModelPlacement placement;
  placement.ground_position = tracked.feet;
  placement.yaw_degrees = tracked.yaw_degrees;
  placement.stance = ro::render::ActorModelStanceOf(tracked.job);
  placement.job = tracked.job;
  const ro::formats::Image* emblem =
      placement.stance == ro::render::ActorModelStance::kGrounded && frame.guild != nullptr &&
              frame.plates != nullptr
          ? EmblemOnPlate(actor, *frame.world, *frame.guild, *frame.plates)
          : nullptr;
  drawn_.push_back(ro::render::ComposeActorModel(resource, bound, local, placement, *frame.camera,
                                                 MapFrameOf(*frame.stage), emblem));
  // LES EFFETS QU'UN MAILLAGE FAIT NAÎTRE : une instance neuve à chaque tic du
  // jeu, qui s'empile sur les précédentes.
  const std::vector<ro::render::ActorModelMeshEvent>& events = drawn_.back().mesh_events;
  if (!events.empty() && frame.now_ms >= tracked.events_ticked_ms + kGameTickMs) {
    tracked.events_ticked_ms = frame.now_ms;
    mesh_events_.insert(mesh_events_.end(), events.begin(), events.end());
  }
}

void ActorModels::PaintRemnants(const Frame& frame) {
  const auto tick_of = [&](const Remnant& remnant) {
    return static_cast<std::int32_t>((frame.now_ms - remnant.since_ms) / kGameTickMs);
  };
  std::erase_if(remnants_, [&](const Remnant& remnant) {
    return tick_of(remnant) > ro::render::kActorModelRemnantLastTick;
  });
  for (const Remnant& remnant : remnants_) {
    const ro::render::ActorModelAnimation* bound =
        Bound(*frame.catalog, *remnant.model, ActorModelMotion::kDead);
    const float clock = ro::render::kActorModelClockRate * Seconds(remnant.since_ms, frame.now_ms);
    const float local = bound != nullptr ? ro::render::AnimationLocalSeconds(
                                               clock, bound->animation->duration_seconds, true)
                                         : 0.0F;
    ro::render::ActorModelPlacement placement;
    placement.ground_position = remnant.feet;
    placement.yaw_degrees = remnant.yaw_degrees;
    placement.stance = ro::render::ActorModelStance::kRemnant;
    placement.alpha = ro::render::ActorModelRemnantAlpha(tick_of(remnant));
    drawn_.push_back(ro::render::ComposeActorModel(*remnant.model->resource, bound, local,
                                                   placement, *frame.camera,
                                                   MapFrameOf(*frame.stage), nullptr));
  }
}

void ActorModels::Compose(const Frame& frame, ComposedScene& scene, SceneCensus& census) {
  drawn_.clear();
  mesh_events_.clear();
  if (frame.catalog == nullptr || frame.world == nullptr || frame.planner == nullptr ||
      frame.stage == nullptr || frame.camera == nullptr) {
    return;
  }
  // UNE AUTRE CARTE : les acteurs y sont d'autres, et les restes de la
  // précédente n'y ont pas leur place.
  if (frame.stage->name != stage_name_) {
    stage_name_ = frame.stage->name;
    tracked_.clear();
    remnants_.clear();
  }
  // Ceux qui ne sont plus dans le monde, ou plus des modèles, s'oublient.
  std::erase_if(tracked_, [&](const auto& entry) {
    const ro::sim::Actor* actor = frame.world->Find(entry.first);
    return actor == nullptr || actor->look.job != entry.second.job;
  });
  for (const auto& [id, actor] : frame.world->actors()) {
    if (!HasSprite(actor.kind) || actor.kind == ro::sim::ActorKind::kItem) {
      continue;
    }
    const ro::render::ActorLook look = LookOf(actor);
    if (!ro::render::DrawnAsModel(*frame.catalog, look)) {
      continue;
    }
    const auto known = tracked_.find(id);
    // LA MORT : l'acteur s'en va aussitôt, et son reste prend sa place.
    if (actor.posture == ro::sim::Posture::kDead) {
      if (known != tracked_.end() && known->second.model->resource.has_value()) {
        remnants_.push_back(Remnant{known->second.model, known->second.feet,
                                    known->second.yaw_degrees, frame.now_ms});
      }
      if (known != tracked_.end()) {
        tracked_.erase(known);
      }
      scene.vanished.push_back(id);
      continue;
    }
    Tracked& tracked = tracked_[id];
    if (tracked.model == nullptr) {
      tracked.job = look.job;
      tracked.resource_name = ro::render::NonPlayerResourceName(*frame.catalog, look.job);
      tracked.model = Model(tracked.resource_name);
      tracked.motion_since_ms = frame.now_ms;
      tracked.walking = false;
    }
    Follow(frame, actor, tracked);
    if (!tracked.model->resource.has_value() || IsHidden(actor, *frame.world)) {
      continue;
    }
    Paint(frame, actor, tracked);
    ro::render::DrawnActor entry{&no_quads_, nullptr, tracked.feet};
    entry.pick_bias = PickBiasOf(actor);
    entry.model_pick_side = ro::render::ActorModelPickBoxSide(tracked.job);
    ro::render::ActorModelPlacement placement;
    placement.ground_position = tracked.feet;
    placement.stance = ro::render::ActorModelStanceOf(tracked.job);
    entry.model_pick_center = ro::render::ActorModelPickCenter(placement);
    scene.drawn.push_back(entry);
    scene.ids.push_back(id);
    // Le recensement le croyait sans corps : il n'avait pas de sprite.
    if (census.without_body > 0) {
      --census.without_body;
    }
    ++census.drawn;
  }
  PaintRemnants(frame);
}

}  // namespace moonclient
