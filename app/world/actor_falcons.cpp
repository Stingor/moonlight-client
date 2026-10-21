#include "world/actor_falcons.h"

#include <algorithm>

#include "ro/core/log.h"
#include "ro/render/effect_frame.h"
#include "ro/render/ground_skill_visual.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/terrain_mesh.h"
#include "ro/render/tick_blend.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "world/actor_view.h"
#include "world/hit_popup_rules.h"

namespace moonclient {
namespace {

// Au-delà, le temps écoulé depuis la dernière image ne se rattrape plus : une
// fenêtre restée figée une minute rejouerait sinon trois mille tics d'un coup.
// Le faucon reprend alors où il en était.
constexpr std::uint32_t kMostTicksPerFrame = 60;

// Un point de NOTRE monde, ramené au repère du client : l'ordonnée et la
// profondeur s'y retournent.
ro::render::FalconPoint ClientPoint(const std::array<float, 3>& ours) noexcept {
  return ro::render::FalconPoint{ours[0], -ours[1], -ours[2]};
}

glm::vec3 OurPoint(const ro::render::FalconPoint& client) noexcept {
  return glm::vec3(client.x, -client.y, -client.z);
}

// Les pieds d'un acteur à cet instant, dans le repère du client.
ro::render::FalconPoint FeetAt(const FalconFrame& frame, const ro::sim::Actor& actor,
                               std::uint32_t at_ms) {
  const ro::sim::FractionalCell cell =
      ro::sim::InterpolatedCell(actor, frame.planner->PathFor(actor), at_ms);
  return ClientPoint(GroundPositionAt(*frame.stage, cell.x, cell.y));
}

ro::render::FalconMaster MasterAt(const FalconFrame& frame, const ro::sim::Actor& actor,
                                  std::uint32_t at_ms) {
  return ro::render::FalconMaster{
      FeetAt(frame, actor, at_ms),
      ClientFacingDegrees(ro::sim::FacingAt(actor, frame.planner->PathFor(actor), at_ms))};
}

}  // namespace

const ActorFalcons::Art* ActorFalcons::ArtFor(ro::vfs::Vfs& vfs,
                                              const ro::render::FalconArt& wanted) {
  std::string key = std::string(wanted.spr_path) + "|" + std::string(wanted.act_path);
  if (const auto found = arts_.find(key); found != arts_.end()) {
    return found->second.get();
  }
  auto art = std::make_unique<Art>();
  auto spr_bytes = vfs.Read(std::string(wanted.spr_path));
  auto act_bytes = vfs.Read(std::string(wanted.act_path));
  if (spr_bytes && act_bytes) {
    auto sprite = ro::formats::SprSprite::Parse(*spr_bytes);
    auto animation = ro::formats::ActAnimation::Parse(*act_bytes);
    if (sprite && animation) {
      art->sprite = std::move(*sprite);
      art->animation = std::move(*animation);
    }
  }
  if (!art->sprite.has_value()) {
    ro::core::LogWarning("faucon : {} ou {} illisible — cet oiseau ne se dessinera pas",
                         wanted.spr_path, wanted.act_path);
  }
  return arts_.emplace(std::move(key), std::move(art)).first->second.get();
}

void ActorFalcons::Census(const FalconFrame& frame) {
  std::erase_if(falcons_, [&frame](const auto& kept) {
    const ro::sim::Actor* master = frame.world->Find(kept.first);
    return master == nullptr || !ro::render::CarriesFalcon(master->option);
  });
  for (const auto& [id, actor] : frame.world->actors()) {
    if (!ro::render::CarriesFalcon(actor.option) || falcons_.contains(id)) {
      continue;
    }
    // L'OISEAU SE CHOISIT À LA NAISSANCE, et ne change plus tant qu'il vit :
    // sur le style de corps pour un joueur, sur le métier montré sinon.
    Kept born;
    born.falcon = ro::render::BornFalcon(MasterAt(frame, actor, frame.now_ms));
    born.art = ArtFor(*frame.vfs, ComposedAsPlayer(actor)
                                      ? ro::render::PlayerFalconArtFor(actor.look.body_style)
                                      : ro::render::FalconArtFor(actor.look.job, actor.option));
    born.launched_ms = frame.now_ms;
    falcons_.emplace(id, std::move(born));
  }
}

// ⚠ LE LANCER SE LIT SUR LE PIÈGE, et non sur le lanceur : le serveur annonce
// la compétence sans dégâts sur l'unité qu'elle vise, et c'est là que le monde
// range « qui l'a lancée ». Le client ne lance rien sur un piège qu'il ne
// connaît pas.
void ActorFalcons::Launch(const FalconFrame& frame) {
  for (const auto& [id, unit] : frame.world->actors()) {
    if (unit.kind != ro::sim::ActorKind::kSkillUnit || !unit.support.has_value() ||
        unit.support->skill_id != kSpringTrapSkill) {
      continue;
    }
    const auto kept = falcons_.find(unit.support->source_id);
    if (kept == falcons_.end() || unit.support->started_ms <= kept->second.launched_ms) {
      continue;
    }
    kept->second.launched_ms = unit.support->started_ms;
    ro::render::LaunchFalcon(kept->second.falcon, FeetAt(frame, unit, frame.now_ms));
  }
  // ET UNE COMPÉTENCE POSÉE SUR UNE CASE : son répartiteur envoie le faucon du
  // lanceur vers le sol de la case visée, quand ce lanceur en a un.
  for (const auto& [id, actor] : frame.world->actors()) {
    if (!actor.ground_skill.has_value() ||
        !ro::render::GroundSkillSendsFalcon(actor.ground_skill->skill_id)) {
      continue;
    }
    const ro::sim::GroundSkillUse& use = *actor.ground_skill;
    const auto kept = falcons_.find(id);
    if (kept == falcons_.end() || use.started_ms <= kept->second.launched_ms) {
      continue;
    }
    kept->second.launched_ms = use.started_ms;
    ro::render::LaunchFalcon(kept->second.falcon, ClientPoint(GroundPositionAt(
                                                      *frame.stage, static_cast<float>(use.cell.x),
                                                      static_cast<float>(use.cell.y))));
  }
  // ET UN COUP DE COMPÉTENCE qui appelle le faucon — Blitz Beat, Falcon
  // Assault : l'oiseau de l'auteur pique vers les pieds de sa cible, là où
  // elle se tenait à l'annonce du coup. Une salve qui touche plusieurs cibles
  // dans la même image l'envoie vers la dernière : chaque annonce recalcule le
  // vol depuis la même place.
  //
  // ⚠ LE JOURNAL DES COUPS, ET NON LE DERNIER COUP DE L'AUTEUR : l'attaque
  // ordinaire qui suit un Blitz Beat automatique dans la même image
  // l'effacerait, et un coup qu'on se porte à soi-même n'y est pas rangé.
  for (const ro::sim::RecordedStrike& recorded : strike_cursor_.Take(frame.world->strikes())) {
    const ro::sim::Strike& strike = recorded.strike;
    if (!ro::render::SkillHitSendsFalcon(strike.skill_id)) {
      continue;
    }
    // Un coup plus ancien que l'oiseau — le journal lu pour la première fois —
    // ne le lance pas.
    const auto kept = falcons_.find(strike.attacker);
    if (kept == falcons_.end() || strike.started_ms < kept->second.launched_ms) {
      continue;
    }
    const ro::sim::Actor* target = frame.world->Find(strike.target);
    if (target == nullptr || target->kind == ro::sim::ActorKind::kSkillUnit ||
        target->kind == ro::sim::ActorKind::kItem) {
      continue;
    }
    kept->second.launched_ms = strike.started_ms;
    ro::render::LaunchFalcon(kept->second.falcon, FeetAt(frame, *target, strike.started_ms));
  }
}

void ActorFalcons::Tick(const FalconFrame& frame, std::uint32_t tick_ms) {
  const MapStage& stage = *frame.stage;
  const ro::render::FalconGround ground = [&stage](float x, float z) {
    const std::array<float, 2> cell = ro::render::FractionalCellAt(stage.scene.gnd, x, -z);
    return -GroundPositionAt(stage, cell[0], cell[1])[1];
  };
  for (auto& [id, kept] : falcons_) {
    const ro::sim::Actor* master = frame.world->Find(id);
    if (master == nullptr) {
      continue;
    }
    const ro::formats::ActAnimation* animation =
        kept.art != nullptr && kept.art->animation.has_value() ? &*kept.art->animation : nullptr;
    const ro::render::FalconFrameCount frame_count = [animation](std::size_t action) {
      if (animation == nullptr || action >= animation->actions().size()) {
        return std::size_t{0};
      }
      return animation->actions()[action].frames.size();
    };
    ro::render::StepFalcon(kept.falcon, MasterAt(frame, *master, tick_ms), ground, frame_count,
                           frame.camera_yaw_degrees);
  }
}

std::span<const ro::render::DrawnActor> ActorFalcons::Update(const FalconFrame& frame) {
  drawn_.clear();
  if (frame.world == nullptr || frame.planner == nullptr || frame.stage == nullptr ||
      frame.vfs == nullptr) {
    return drawn_;
  }
  Census(frame);
  Launch(frame);

  const auto tick_ms = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  if (!last_tick_ms_.has_value() || frame.now_ms - *last_tick_ms_ > kMostTicksPerFrame * tick_ms) {
    last_tick_ms_ = frame.now_ms;
    // LE TEMPS NE SE RATTRAPE PAS : la position d'avant le saut ne vaut plus
    // rien, et l'image se peindrait encore à mi-chemin d'elle.
    for (auto& [id, kept] : falcons_) {
      kept.falcon.previous_position = kept.falcon.position;
    }
  }
  while (frame.now_ms - *last_tick_ms_ >= tick_ms) {
    *last_tick_ms_ += tick_ms;
    Tick(frame, *last_tick_ms_);
  }
  const float weight =
      ro::render::TickBlendWeight(frame.now_ms - *last_tick_ms_, tick_ms, frame.blended);

  const std::uint8_t alpha =
      frame.translucent ? ro::render::kFalconTranslucentAlpha : ro::render::kFalconOpaqueAlpha;
  for (auto& [id, kept] : falcons_) {
    kept.frame = ro::render::ComposedActor();
    if (kept.art == nullptr || !kept.art->sprite.has_value()) {
      continue;
    }
    const ro::formats::ActAnimation& animation = *kept.art->animation;
    const std::size_t action = ro::render::FalconAction(kept.falcon, frame.camera_yaw_degrees);
    if (action >= animation.actions().size() ||
        kept.falcon.frame >= animation.actions()[action].frames.size()) {
      continue;
    }
    for (const ro::render::SpriteQuad& quad :
         ro::render::BuildFrameQuads(*kept.art->sprite, animation, action, kept.falcon.frame)) {
      kept.frame.sources.push_back(
          ro::render::SpriteImageKey{&*kept.art->sprite, nullptr, quad.section, quad.image_index});
      kept.frame.quads.push_back(quad);
    }
    if (kept.frame.quads.empty()) {
      continue;
    }
    const glm::vec3 placed = OurPoint(ro::render::FalconDrawnPosition(kept.falcon, weight));
    std::array<std::uint8_t, 4> tint = {255, 255, 255, alpha};
    if (frame.light != nullptr) {
      const std::array<float, 2> cell =
          ro::render::FractionalCellAt(frame.stage->scene.gnd, placed.x, placed.z);
      tint = ro::render::LitTint(tint, frame.light->Under(cell[0], cell[1]), frame.cold_light);
    }
    drawn_.push_back(ro::render::DrawnActor{&kept.frame, nullptr, placed, tint});
  }
  return drawn_;
}

}  // namespace moonclient
