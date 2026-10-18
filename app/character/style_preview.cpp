#include "character/style_preview.h"

#include <cstddef>
#include <utility>

#include "ro/render/sprite_frame.h"
#include "ro/sim/style_code.h"
#include "ro/sim/world.h"
#include "ro/ui/character_window.h"

#include "character/style_recipe.h"
#include "world/actor_view.h"

namespace moonclient {
namespace {

// L'image que le pantin TIENT : la première, comme pour l'éditeur. Un aperçu
// de couleur n'a pas à s'agiter.
constexpr std::size_t kHeldFrame = 0;

}  // namespace

StylePreview::StylePreview(ro::render::ActorWardrobe* wardrobe) : wardrobe_(wardrobe) {
  if (wardrobe_ != nullptr) {
    doll_.emplace(*wardrobe_);
  }
}

bool StylePreview::Open(std::string_view code, std::string_view owner,
                        ro::ui::StylePreviewState& state) {
  const std::optional<ro::sim::StyleCode> read = ro::sim::DecodeStyleCode(code);
  if (!read.has_value()) {
    return false;
  }
  recipe_ = FromWire(read->recipe);
  code_.assign(code);
  state.code = code_;
  state.owner.assign(owner);
  state.open = true;
  // 🔴 LE PANTIN SE REFAIT À LA PREMIÈRE IMAGE, ET NON ICI. Cette fenêtre
  // s'ouvre depuis le FIL, donc à un moment où rien ne garantit que le corps
  // du joueur est chargé — et il peut changer de tenue pendant qu'elle est
  // ouverte. Les deux cas tombent par le même chemin si l'on ne mesure rien
  // d'avance.
  measured_valid_ = false;
  painted_ = false;
  // ⚠ LA POSE REPART DE FACE : deux styles regardés l'un après l'autre se
  // comparent sous le même angle, et c'est celui qui montre le plus de pièces.
  state.doll.direction = 0;
  return true;
}

void StylePreview::Refresh(const ro::sim::World& world, ro::ui::StylePreviewState& state) {
  view_ = ro::ui::StylePreviewView{};
  state.doll.image = ro::ui::SkinImage{};
  if (wardrobe_ == nullptr || !doll_.has_value()) {
    // Une installation sans sprites : on le sait, et on le dit.
    view_.bodyless = true;
    return;
  }
  const ro::sim::Actor* player = world.player();
  if (player == nullptr) {
    // ⚠ TRANSITOIRE, DONC MUET : entre deux cartes, le monde n'a pas de
    // joueur. Annoncer un échec ici ferait clignoter un message pour une
    // absence d'une fraction de seconde.
    return;
  }

  ro::render::ActorLook look = BodyOnlyLookOf(*player);
  // 🔴 CE QUE LA RECETTE IMPOSE, AVANT LA MESURE : la teinte de base décide de
  // la palette sur laquelle les rampes se détectent. La poser après donnerait
  // un pantin peint d'après le découpage d'un autre corps.
  Impose(recipe_, look);

  // ⚠ DEUX TÉMOINS, comme dans l'éditeur : le corps décide du sprite, la
  // teinte de base décide de la fusion. N'en surveiller qu'un laisserait
  // l'aperçu peint sur une base périmée.
  const std::uint32_t body_key = wardrobe_->BodyKeyFor(look);
  if (!measured_valid_ || body_key != body_key_ || look.body_palette != measured_palette_) {
    body_ = wardrobe_->BodyRampsFor(look);
    body_key_ = body_key;
    measured_palette_ = look.body_palette;
    measured_valid_ = true;
    // La palette peinte vaut pour l'ancienne base : elle est périmée avec
    // elle.
    painted_ = false;
  }
  if (body_.ramps.empty()) {
    view_.bodyless = true;
    return;
  }

  if (!painted_) {
    // 🔴 UN IDENTIFIANT NEUF, ET L'ANCIEN RENDU : la garde-robe refuse de
    // réécrire une palette en place, parce qu'une tenue déjà composée en
    // retient l'ADRESSE — la repeindre ne se verrait qu'à la prochaine
    // éviction.
    if (preview_style_ != 0) {
      wardrobe_->ForgetStyle(preview_style_);
    }
    preview_style_ = wardrobe_->NewStyleId();
    if (!wardrobe_->SetStylePalette(preview_style_,
                                    ro::render::ApplyRecipe(body_.base, body_.ramps, recipe_))) {
      preview_style_ = 0;
      return;
    }
    painted_ = true;
  }

  const auto width = static_cast<std::int32_t>(state.doll.box_width);
  const auto height = static_cast<std::int32_t>(state.doll.box_height);
  if (width <= 0 || height <= 0) {
    // La boîte est mesurée au dessin : la première image n'en connaît pas
    // encore la taille, et la suivante l'aura.
    return;
  }
  ro::render::ActorVignette::Pose pose;
  pose.look = look;
  pose.look.style = preview_style_;
  pose.animation = ro::render::kAnimationIdle;
  pose.direction = state.doll.direction;
  pose.animated = false;
  pose.box.width = width;
  pose.box.height = height;
  pose.box.min_body_height = state.doll.box_height * ro::ui::kCharacterAvatarBodyFloor;

  const ro::render::IconTexture& drawn = doll_->Get(pose);
  if (!drawn.usable()) {
    return;
  }
  state.doll.image = ro::ui::SkinImage{.id = static_cast<ImTextureID>(drawn.texture.name()),
                                       .width = state.doll.box_width,
                                       .height = state.doll.box_height};
}

void StylePreview::Close() {
  if (wardrobe_ != nullptr && preview_style_ != 0) {
    wardrobe_->ForgetStyle(preview_style_);
  }
  preview_style_ = 0;
  painted_ = false;
}

}  // namespace moonclient
