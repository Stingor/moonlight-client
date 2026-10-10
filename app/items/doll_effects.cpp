#include "items/doll_effects.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <utility>

#include "ro/core/log.h"
#include "ro/render/hat_effect_table.h"
#include "ro/render/inset_paint.h"
#include "ro/sim/actor.h"
#include "ro/sim/movement.h"

#include "effects/doll_camera.h"
#include "effects/effect_layers.h"
#include "world/actor_view.h"

namespace moonclient {
namespace {

// Le porteur, seul acteur de son monde : son numéro n'y rencontre personne.
constexpr std::uint32_t kBearerId = 1;

// Un rectangle de l'image, arrondi au pixel.
[[nodiscard]] std::int32_t Pixels(float value) {
  return static_cast<std::int32_t>(std::lround(value));
}

// 🔴 LA TOILE DE L'EFFET DÉBORDE LA BOÎTE DU PANTIN, d'une boîte entière de
// chaque côté : une aura, une paire d'ailes s'étendent bien au-delà du
// personnage, et bornées à sa boîte elles se coupaient net. Les pieds restent
// où le pantin les pose, à la même échelle ; seul le cadre grandit.
constexpr float kCanvasMarginBoxes = 1.0F;
constexpr float kCanvasMarginX = ro::ui::kDescriptionDollWidthPixels * kCanvasMarginBoxes;
constexpr float kCanvasMarginY = ro::ui::kDescriptionDollHeightPixels * kCanvasMarginBoxes;
constexpr float kCanvasWidth = ro::ui::kDescriptionDollWidthPixels + 2.0F * kCanvasMarginX;
constexpr float kCanvasHeight = ro::ui::kDescriptionDollHeightPixels + 2.0F * kCanvasMarginY;

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

void DollEffects::Advance(ro::ui::DescriptionDollState& doll, std::int16_t ordinal,
                          const ro::render::VignetteFraming& framing, const ro::sim::World& world,
                          const MapStage& stage, std::uint32_t now_ms) {
  const ro::sim::Actor* player = world.player();
  const bool ready = effect_renderer_.has_value() && sprites_.has_value();
  if (ordinal <= 0 || !doll.image.usable() || !framing.usable() || player == nullptr || !ready) {
    Rest();
    return;
  }
  const auto worn = static_cast<ro::sim::HatEffectId>(ordinal);
  const ro::render::HatEffect* record = effects_.HatEffectFor(worn);
  if (record == nullptr) {
    Rest();
    return;
  }
  // LE PORTEUR NAÎT À LA CASE DU JOUEUR, et y reste : le sol de la carte est
  // là, et les nœuds qui rebondissent le cherchent.
  if (ordinal != worn_ || stage.name != map_name_) {
    Rest();
    ro::sim::Actor bearer;
    bearer.id = kBearerId;
    bearer.kind = ro::sim::ActorKind::kPlayer;
    bearer.position = player->position;
    // LE MÉTIER QUE MONTRE LE JOUEUR : l'ancre d'un chapeau monte avec une
    // monture et se réduit avec un bébé.
    bearer.look.job = static_cast<std::uint16_t>(ShownJobOf(*player));
    bearer.hat_effects.insert(worn);
    stand_.Spawn(std::move(bearer));
    worn_ = ordinal;
    map_name_ = stage.name;
  }
  const ro::sim::Actor* bearer = stand_.Find(kBearerId);
  if (bearer == nullptr) {
    return;
  }
  const ro::sim::FractionalCell cell =
      ro::sim::InterpolatedCell(*bearer, planner_.PathFor(*bearer), now_ms);
  const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
  ro::render::VignetteFraming on_canvas = framing;
  on_canvas.origin += glm::vec2(kCanvasMarginX, kCanvasMarginY);
  camera_ = DollCamera(glm::vec3(ground[0], ground[1], ground[2]), on_canvas, kCanvasWidth,
                       kCanvasHeight);
  stage_ = &stage;
  // LE MÊME BATTEMENT QUE LA PARTIE, réduit à ce qu'un porteur immobile demande.
  nodes_.NoticeWorld(EffectScatterWorldOf(stage));
  effects_.SeeScene(camera_, {});
  effects_.Classes().SeeScene(camera_, kCanvasWidth, kCanvasHeight, effects_.MinimalEffects());
  effects_.NoticeHatEffects(stand_, stage, planner_, now_ms);
  played_ = effects_.Drawn(now_ms, stage, stand_, planner_);
  static_cast<void>(nodes_.Drawn(now_ms, camera_));
  effects_.SowRequestedEffects(stand_, stage, planner_, now_ms);
  Drain();
  // ⚠ CE QUE LA PARTIE PEINT AVANT LES CORPS passe sous le pantin : l'ombre d'un
  // costume, le fichier qu'une fiche range avant le personnage. Voir `PaintInto`.
  doll.overlay = ro::ui::DollOverlay{.paint = &DollEffects::Paint, .painter = this};
}

void DollEffects::Rest() {
  if (worn_ == 0) {
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
  stage_ = nullptr;
  worn_ = 0;
  map_name_.clear();
}

void DollEffects::ForgetTextures() {
  if (effect_renderer_.has_value()) {
    effect_renderer_->ForgetTextures();
  }
  if (ground_quads_.has_value()) {
    ground_quads_->ForgetTextures();
  }
}

void DollEffects::Drain() {
  static_cast<void>(nodes_.TakeSounds());
  static_cast<void>(effects_.TakeLaunchedEffects());
  static_cast<void>(effects_.TakeSownNamedSounds());
  static_cast<void>(effects_.TakeScreenQuake());
  static_cast<void>(effects_.TakeActorEffects());
  static_cast<void>(effects_.TakeChimneys());
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
  if (worn_ == 0 || stage_ == nullptr || !effect_renderer_.has_value() || !sprites_.has_value() ||
      frame == nullptr) {
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
  const float margin_x = kCanvasMarginX * box_width / ro::ui::kDescriptionDollWidthPixels;
  const float margin_y = kCanvasMarginY * box_height / ro::ui::kDescriptionDollHeightPixels;
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
  } else {
    PaintEffectsUnderStr(effects_, nodes_, painters);
    PaintStrEffects(effects_, nodes_, played_, painters);
    PaintEffectsLast(effects_, nodes_, painters);
  }
  ro::render::EndInsetPaint();
}

}  // namespace moonclient
