#include "ro/render/actor_vignette.h"

#include <algorithm>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "ro/formats/image.h"
#include "ro/render/actor_sprite.h"

namespace ro::render {
namespace {

// Deux apparences sont les mêmes quand aucune ne précède l'autre.
//
// ⚠ ON COMPARE LES CHAMPS, on ne hache pas : l'ordre total de la garde-robe est
// déjà écrit, et deux apparences que l'on croirait égales par empreinte
// montreraient un autre personnage.
[[nodiscard]] bool SameLook(const ActorLook& left, const ActorLook& right) {
  const ActorLookOrder before;
  return !before(left, right) && !before(right, left);
}

[[nodiscard]] bool SameBox(const VignetteBox& left, const VignetteBox& right) {
  return left.width == right.width && left.height == right.height &&
         left.min_body_height == right.min_body_height && left.scale == right.scale &&
         left.fit == right.fit;
}

// Deux compositions peignent la même chose quand chaque quad tombe au même
// endroit, dans le même sens et la même teinte.
//
// ⚠ LES IMAGES NE SUFFISENT PAS : une animation qui réemploie une image du
// .spr peut la déplacer (un bond d'attaque), la retourner ou la pâlir.
[[nodiscard]] bool SameQuads(std::span<const SpriteQuad> left, std::span<const SpriteQuad> right) {
  return std::ranges::equal(left, right, [](const SpriteQuad& one, const SpriteQuad& other) {
    return one.image_index == other.image_index && one.section == other.section &&
           one.corners == other.corners && one.mirrored == other.mirrored && one.tint == other.tint;
  });
}

}  // namespace

const IconTexture& ActorVignette::Get(const Pose& pose) {
  const bool drawable = wardrobe_ != nullptr && pose.box.width > 0 && pose.box.height > 0;
  ActorOutfit* outfit = drawable ? wardrobe_->OutfitFor(pose.look) : nullptr;
  composed_ = nullptr;
  animation_duration_ms_ = 0.0F;
  if (outfit == nullptr) {
    // ⚠ ON OUBLIE CE QUI ÉTAIT DESSINÉ : sans cela, la vignette d'un
    // personnage reparaîtrait telle quelle à la première image du suivant.
    texture_ = IconTexture{};
    drawn_.clear();
    return texture_;
  }

  // L'ENVELOPPE NE SE REMESURE QUE QUAND ELLE PEUT AVOIR CHANGÉ : elle coûte
  // une composition par image du corps et par direction, ce qui n'est rien une
  // fois par changement d'apparence et tout à chaque image de jeu.
  //
  // ⚠ LE CADRAGE CENTRÉ MESURE UNE SEULE DIRECTION : en tourner change donc
  // l'enveloppe, et changer de cadrage change ce qu'on mesure.
  const bool same_outfit = SameLook(pose.look, look_) && pose.animation == animation_;
  const bool centered = pose.box.fit == VignetteFit::kCenteredShrinkOnly;
  const bool same_span =
      same_outfit && pose.box.fit == box_.fit && (!centered || pose.direction == direction_);
  if (!same_span) {
    span_ = centered ? MeasureDirectionSpan(*outfit, pose.animation, pose.direction)
                     : MeasureOutfitSpan(*outfit, pose.animation);
  }
  if (!same_span || !SameBox(pose.box, box_)) {
    framing_ = FrameVignette(span_, pose.box);
  }

  // ⚠ LE CORPS TIENT SON IMAGE ZÉRO quand la pose est figée, et le temps écoulé
  // continue de courir pour ce qu'il porte : c'est ce qui fait vivre une coiffe
  // animée sur un personnage à l'arrêt, comme en jeu.
  std::optional<std::size_t> held = pose.animated ? std::nullopt : std::optional<std::size_t>(0);
  if (pose.held_frame.has_value()) {
    held = pose.held_frame;
  }
  const ComposedActor& composed =
      outfit->Composed(pose.animation, pose.direction, pose.elapsed_ms, /*loop=*/true, held);
  composed_ = &composed;
  animation_duration_ms_ = outfit->AnimationDurationMs(pose.animation);
  // LES BORNES DE CE QUI EST PEINT, relevées à chaque image : elles bougent
  // avec l'animation, même quand la texture n'a pas à être refaite.
  std::vector<SpriteQuad> painted;
  for (std::size_t index = 0; index < composed.quads.size(); ++index) {
    if (index < composed.sources.size() && HasImage(composed.sources[index])) {
      painted.push_back(composed.quads[index]);
    }
  }
  drawn_bounds_ = ComputeBounds(painted);

  const bool unchanged = same_outfit && SameBox(pose.box, box_) && pose.direction == direction_ &&
                         !drawn_.empty() && composed.sources == drawn_ &&
                         SameQuads(composed.quads, drawn_quads_);
  if (unchanged) {
    return texture_;
  }

  look_ = pose.look;
  animation_ = pose.animation;
  direction_ = pose.direction;
  box_ = pose.box;
  drawn_ = composed.sources;
  drawn_quads_ = composed.quads;

  const formats::Image image = DrawVignette(composed, pose.box, framing_);
  if (image.empty()) {
    texture_ = IconTexture{};
    drawn_.clear();
    return texture_;
  }
  // 🔴 COMME UN SPRITE, ET NON COMME UNE PIÈCE D'INTERFACE : la vignette est
  // composée en pixels de SPRITE et l'interface l'étire sur sa boîte, donc la
  // carte l'agrandit. Au plus proche, elle rendrait des marches d'escalier —
  // l'art d'interface, lui, se pose un pour un, ce qui est justement ce que ce
  // mode-là suppose.
  //
  // ⚠ ET C'EST CE MODE QUI ÉTALE LA COULEUR SOUS LES PIXELS TRANSPARENTS.
  // Sans cela le filtrage mélangerait le bord de la silhouette avec du noir
  // transparent, et un liseré sombre cernerait le personnage.
  auto texture = GlTexture::Upload(image, GlTexture::Filtering::kSprite);
  if (!texture) {
    texture_ = IconTexture{};
    drawn_.clear();
    return texture_;
  }
  texture_.texture = std::move(*texture);
  texture_.width_px = image.width;
  texture_.height_px = image.height;
  return texture_;
}

}  // namespace ro::render
