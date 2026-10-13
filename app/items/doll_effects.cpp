#include "items/doll_effects.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <utility>

#include "ro/core/log.h"
#include "ro/render/hat_effect_table.h"
#include "ro/render/inset_paint.h"
#include "ro/render/sprite_frame.h"
#include "ro/sim/actor.h"
#include "ro/sim/movement.h"

#include "effects/doll_camera.h"
#include "effects/effect_actor_view.h"
#include "effects/effect_layers.h"
#include "items/doll_body.h"
#include "world/actor_effect_finish.h"
#include "world/actor_tint.h"
#include "world/actor_view.h"

namespace moonclient {
namespace {

// Le porteur, seul acteur de son monde : son numéro n'y rencontre personne.
constexpr std::uint32_t kBearerId = 1;

// Un rectangle de l'image, arrondi au pixel.
[[nodiscard]] std::int32_t Pixels(float value) {
  return static_cast<std::int32_t>(std::lround(value));
}

// 🔴 LA TOILE DE L'EFFET D'UN APERÇU DÉBORDE LA BOÎTE DU PANTIN, d'une boîte
// entière de chaque côté : une aura, une paire d'ailes s'étendent bien au-delà
// du personnage, et doivent s'y montrer entières. Les pieds restent où le
// pantin les pose, à la même échelle ; seul le cadre grandit.
constexpr DollCanvas kPreviewCanvas{
    .box_px = glm::vec2(ro::ui::kDescriptionDollWidthPixels, ro::ui::kDescriptionDollHeightPixels),
    .margin_boxes = 1.0F};

}  // namespace

DollEffects::DollEffects(ro::vfs::Vfs& vfs)
    : catalog_(vfs), nodes_(vfs), effects_(catalog_, nodes_, vfs) {
  auto effect_renderer = ro::render::EffectRenderer::Create();
  auto sprites = ro::render::SpriteRenderer::Create();
  auto ground_quads = ro::render::GroundQuadRenderer::Create();
  if (!effect_renderer || !sprites || !ground_quads) {
    ro::core::LogWarning("pantin : les effets de chapeau ne se peignent pas");
    return;
  }
  effect_renderer_ = std::move(*effect_renderer);
  sprites_ = std::move(*sprites);
  ground_quads_ = std::move(*ground_quads);
}

DollEffects::Shown DollEffects::Advance(std::span<const ro::sim::HatEffectId> worn,
                                        const DollPose& doll, const ro::sim::World& world,
                                        const MapStage& stage, std::uint32_t now_ms) {
  const ro::sim::Actor* player = world.player();
  const bool ready = effect_renderer_.has_value() && sprites_.has_value();
  if (worn.empty() || !doll.framing.usable() || !doll.canvas.usable() || player == nullptr ||
      !ready) {
    Rest();
    return {};
  }
  // ⚠ UN NUMÉRO SANS FICHE NE PORTE RIEN, comme en partie : l'ensemble ne
  // garde que ce qui se montre, et rien de montrable revient au repos.
  std::vector<ro::sim::HatEffectId> shown_effects;
  for (const ro::sim::HatEffectId effect : worn) {
    if (effects_.HatEffectFor(effect) != nullptr) {
      shown_effects.push_back(effect);
    }
  }
  if (shown_effects.empty()) {
    Rest();
    return {};
  }
  // LE PORTEUR NAÎT À LA CASE DU JOUEUR, et y reste : le sol de la carte est
  // là, et les nœuds qui rebondissent le cherchent. IL MONTRE LE MÊME CORPS :
  // métier montré et monture du shop, que l'ancre d'un chapeau lit.
  DollWearing wanted = WearingOf(*player, std::move(shown_effects), stage.name);
  if (wanted != wearing_) {
    Rest();
    ro::sim::Actor bearer;
    bearer.id = kBearerId;
    bearer.kind = ro::sim::ActorKind::kPlayer;
    bearer.position = player->position;
    bearer.look.job = static_cast<std::uint16_t>(wanted.shown_job);
    bearer.hat_effects.insert(wanted.worn.begin(), wanted.worn.end());
    if (wanted.mounted) {
      bearer.status_effects.insert(ro::sim::kStatusAllRiding);
    }
    stand_.Spawn(std::move(bearer));
    wearing_ = std::move(wanted);
  }
  // IL REGARDE OÙ LE PANTIN REGARDE, à chaque image : la molette d'un aperçu,
  // la direction choisie pour le portrait. La caméra du pantin n'a pas pivoté.
  static_cast<void>(stand_.Turn(
      kBearerId,
      static_cast<std::uint8_t>(ro::render::CompassOfSpriteDirection(doll.sprite_direction)), 0));
  const ro::sim::Actor* bearer = stand_.Find(kBearerId);
  if (bearer == nullptr) {
    return {};
  }
  const ro::sim::FractionalCell cell =
      ro::sim::InterpolatedCell(*bearer, planner_.PathFor(*bearer), now_ms);
  const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
  canvas_ = doll.canvas;
  const glm::vec2 canvas_px = doll.canvas.size_px();
  camera_ = DollCamera(glm::vec3(ground[0], ground[1], ground[2]),
                       OnDollCanvas(doll.framing, doll.canvas), canvas_px.x, canvas_px.y);
  stage_ = &stage;
  // SA TÊTE BOUGE COMME CELLE DU PANTIN — assise, balancée par la marche,
  // relevée par une monture — et ce qui s'y pose la suit.
  head_shifts_.clear();
  if (doll.body.head_shift.has_value()) {
    head_shifts_[kBearerId] = *doll.body.head_shift;
  }
  const std::size_t sprite_direction = doll.sprite_direction;
  // LE MÊME BATTEMENT QUE LA PARTIE, réduit à ce qu'un porteur immobile demande.
  nodes_.NoticeWorld(EffectScatterWorldOf(stage));
  effects_.SeeScene(camera_,
                    EffectActorView{.sprite_direction_of =
                                        [sprite_direction](const ro::sim::Actor& /*actor*/,
                                                           std::uint32_t /*now_ms*/) {
                                          return std::optional<std::size_t>(sprite_direction);
                                        },
                                    .head_shifts = &head_shifts_});
  effects_.Classes().SeeScene(camera_, canvas_px.x, canvas_px.y, effects_.MinimalEffects());
  effects_.NoticeHatEffects(stand_, stage, planner_, now_ms);
  played_ = effects_.Drawn(now_ms, stage, stand_, planner_);
  static_cast<void>(nodes_.Drawn(now_ms, camera_));
  effects_.SowRequestedEffects(stand_, stage, planner_, now_ms);
  Drain(now_ms);
  // LE CORPS QUE SES EFFETS CHANGENT se repeint ici, et l'image composée se
  // retire : voir `ComposeChangedDollBody`. ⚠ LA SEULE COPIE DU CORPS : il doit
  // vivre jusqu'à la peinture.
  Shown shown;
  body_draws_.clear();
  if (doll.body.composed != nullptr) {
    if (const ro::sim::Actor* settled = stand_.Find(kBearerId)) {
      body_ = DollBody{.composed = *doll.body.composed,
                       .animation_duration_ms = doll.body.animation_duration_ms,
                       .head_shift = doll.body.head_shift};
      PlayedEffectsOf(*settled, now_ms, bearer_effects_);
      shown.body_overlaid = ComposeChangedDollBody(bearer_effects_, body_, kBearerId,
                                                   glm::vec3(ground[0], ground[1], ground[2]),
                                                   ghosts_, now_ms, body_draws_);
    }
  }
  // ⚠ CE QUE LA PARTIE PEINT AVANT LES CORPS passe sous le pantin : l'ombre d'un
  // costume, le fichier qu'une fiche range avant le personnage. Voir `PaintInto`.
  shown.overlay = ro::ui::DollOverlay{.paint = &DollEffects::Paint, .painter = this};
  return shown;
}

void DollEffects::Advance(ro::ui::DescriptionDollState& doll, std::int16_t ordinal,
                          const ro::render::VignetteFraming& framing, const DollBody* body,
                          const ro::sim::World& world, const MapStage& stage,
                          std::uint32_t now_ms) {
  if (ordinal <= 0 || !doll.image.usable()) {
    Rest();
    return;
  }
  const std::array<ro::sim::HatEffectId, 1> worn = {static_cast<ro::sim::HatEffectId>(ordinal)};
  const Shown shown = Advance(worn,
                              DollPose{.framing = framing,
                                       .canvas = kPreviewCanvas,
                                       .body = SourceOf(body),
                                       .sprite_direction = doll.direction},
                              world, stage, now_ms);
  doll.body_overlaid = shown.body_overlaid;
  if (shown.overlay.paint != nullptr) {
    doll.overlay = shown.overlay;
  }
}

void DollEffects::Rest() {
  if (wearing_.worn.empty()) {
    return;
  }
  effects_.Forget();
  nodes_.Forget();
  stand_.Clear();
  // LA MÉMOIRE VIDÉO, comme la partie au changement de carte : l'oubli garde ce
  // qui a servi depuis le précédent, donc l'effet qu'on vient de quitter —
  // revenir sur la même carte ne retéléverse rien.
  if (effect_renderer_.has_value()) {
    effect_renderer_->ForgetUnusedTextures();
  }
  if (sprites_.has_value()) {
    sprites_->ForgetUnusedTextures();
  }
  played_ = {};
  body_draws_.clear();
  ghosts_.Forget();
  stage_ = nullptr;
  wearing_ = DollWearing();
}

void DollEffects::ForgetTextures() {
  if (effect_renderer_.has_value()) {
    effect_renderer_->ForgetTextures();
  }
  if (ground_quads_.has_value()) {
    ground_quads_->ForgetTextures();
  }
}

void DollEffects::Drain(std::uint32_t now_ms) {
  static_cast<void>(nodes_.TakeSounds());
  static_cast<void>(effects_.TakeLaunchedEffects());
  static_cast<void>(effects_.TakeSownNamedSounds());
  static_cast<void>(effects_.TakeScreenQuake());
  static_cast<void>(effects_.TakeChimneys());
  SettleActorEffects(stand_, effects_.TakeActorEffects());
  // CE QUI A FINI DE SOI-MÊME S'EN VA, comme en partie : un blanchiment au
  // bout de sa vie ne se relit plus.
  static_cast<void>(ForgetFinishedEffects(stand_, now_ms));
}

void DollEffects::Paint(const ImDrawList* list, const ImDrawCmd* command) {
  static_cast<void>(list);
  // ⚠ UNE COPIE, ET NON UN POINTEUR REPRIS : la liste range l'appel à sa guise,
  // sans promettre l'alignement du type.
  ro::ui::DollOverlayCall call;
  std::memcpy(&call, command->UserCallbackData, sizeof(call));
  if (call.painter != nullptr) {
    static_cast<DollEffects*>(call.painter)->PaintInto(call);
  }
}

void DollEffects::PaintInto(const ro::ui::DollOverlayCall& call) {
  const ImDrawData* frame = ImGui::GetDrawData();
  if (wearing_.worn.empty() || stage_ == nullptr || !effect_renderer_.has_value() ||
      !sprites_.has_value() || frame == nullptr) {
    return;
  }
  // Des coordonnées de l'interface aux pixels de l'image, dont OpenGL compte les
  // lignes depuis le BAS. La boîte du pantin grandit de ses marges, à l'échelle
  // où l'interface l'a posée.
  const ImVec2 scale = frame->FramebufferScale;
  const float box_width = (call.max.x - call.min.x) * scale.x;
  const float box_height = (call.max.y - call.min.y) * scale.y;
  if (box_width <= 0.0F || box_height <= 0.0F) {
    return;
  }
  const float margin_x = canvas_.margin_boxes * box_width;
  const float margin_y = canvas_.margin_boxes * box_height;
  const float left = (call.min.x - frame->DisplayPos.x) * scale.x - margin_x;
  const float top = (call.min.y - frame->DisplayPos.y) * scale.y - margin_y;
  const float width = box_width + 2.0F * margin_x;
  const float height = box_height + 2.0F * margin_y;
  const float image_width = frame->DisplaySize.x * scale.x;
  const float image_height = frame->DisplaySize.y * scale.y;
  const float bottom = image_height - top - height;
  // ⚠ LA VUE PEUT DÉBORDER DE L'IMAGE, la découpe non : elle s'y borne.
  const float clip_left = std::max(left, 0.0F);
  const float clip_bottom = std::max(bottom, 0.0F);
  const float clip_right = std::min(left + width, image_width);
  const float clip_top = std::min(bottom + height, image_height);
  if (clip_right <= clip_left || clip_top <= clip_bottom) {
    return;
  }
  ro::render::BeginInsetPaint(ro::render::InsetRect{.left = Pixels(left),
                                                    .bottom = Pixels(bottom),
                                                    .width = Pixels(width),
                                                    .height = Pixels(height)},
                              ro::render::InsetRect{.left = Pixels(clip_left),
                                                    .bottom = Pixels(clip_bottom),
                                                    .width = Pixels(clip_right - clip_left),
                                                    .height = Pixels(clip_top - clip_bottom)});
  const EffectPainters painters{
      .sprites = &*sprites_,
      .effects = &*effect_renderer_,
      .ground_quads = ground_quads_.has_value() ? &*ground_quads_ : nullptr,
      .stage = stage_,
      .camera = &camera_,
      .aspect_ratio = width / height,
      .viewport_width = width,
      .viewport_height = height,
      .fog = {}};
  // LES DEUX PASSES DE LA PARTIE : derrière le pantin, ce qu'elle peint avant
  // les corps et dans le même ordre ; devant, tout le reste.
  if (call.pass == ro::ui::DollOverlayPass::kBehind) {
    if (const std::span<const ro::render::DrawnEffect> behind = effects_.DrawnBehindActors();
        !behind.empty()) {
      effect_renderer_->Draw(camera_, behind, width / height);
    }
    PaintClassSpritesBehindActors(effects_, *sprites_, camera_, width / height, {});
    // LE CORPS QUE SES EFFETS CHANGENT, à la place de l'image, rémanences en
    // tête : il recouvre ce qui passe derrière lui et passe sous ce qui le suit.
    if (!body_draws_.empty()) {
      sprites_->Draw(
          camera_, body_draws_, width / height, ro::render::SpriteRenderer::Blending::kOver,
          ro::render::SpriteRenderer::DepthWrite::kOff, ro::render::SpriteRenderer::DepthTest::kOff,
          {}, ro::render::SpriteRenderer::DrawOrder::kAsGiven);
    }
  } else {
    PaintEffectsUnderStr(effects_, nodes_, painters);
    PaintStrEffects(effects_, nodes_, played_, painters);
    PaintEffectsLast(effects_, nodes_, painters);
  }
  ro::render::EndInsetPaint();
}

}  // namespace moonclient
