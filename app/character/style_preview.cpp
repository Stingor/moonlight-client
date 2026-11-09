#include "character/style_preview.h"

#include "ro/sim/style_code.h"
#include "ro/sim/world.h"
#include "ro/ui/character_window.h"

#include "character/style_recipe.h"
#include "world/actor_view.h"

namespace moonclient {

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
  ++revision_;
  code_.assign(code);
  state.code = code_;
  state.owner.assign(owner);
  state.open = true;
  // 🔴 LE PANTIN SE REFAIT À LA PREMIÈRE IMAGE, ET NON ICI. Cette fenêtre
  // s'ouvre depuis le FIL, donc à un moment où rien ne garantit que le corps
  // du joueur est chargé — et il peut changer de tenue pendant qu'elle est
  // ouverte.
  //
  // ⚠ LA POSE REPART DE FACE : deux styles regardés l'un après l'autre se
  // comparent sous le même angle, et c'est celui qui montre le plus de pièces.
  state.doll.direction = 0;
  return true;
}

void StylePreview::Refresh(const ro::sim::World& world, ro::ui::StylePreviewState& state) {
  view_ = ro::ui::StylePreviewView{};
  state.doll.image = ro::ui::SkinImage{};
  // ⚠ UNE FENÊTRE RESTÉE OUVERTE D'UNE AUTRE PARTIE N'A RIEN À MONTRER : son
  // état appartient à la coquille, et ce code-là n'a jamais été ouvert ici.
  if (code_.empty()) {
    state.open = false;
    return;
  }
  if (!doll_.has_value()) {
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
  const std::uint32_t body_key = wardrobe_->BodyKeyFor(look);
  // 🔴 CE QUE LA RECETTE IMPOSE, AVANT LA MESURE : la teinte de base du style
  // reçu décide de la palette sur laquelle les rampes se détectent.
  Impose(recipe_, look);
  if (doll_->Measure(look, body_key).ramps.empty()) {
    view_.bodyless = true;
    return;
  }

  // La boîte est mesurée au dessin : la première image n'en connaît pas encore
  // la taille, et la suivante l'aura.
  const StyleDoll::Shot shot{
      .look = look,
      .recipe = &recipe_,
      .revision = revision_,
      .direction = state.doll.direction,
      .box = ro::render::VignetteBox{
          .width = static_cast<std::int32_t>(state.doll.box_width),
          .height = static_cast<std::int32_t>(state.doll.box_height),
          .min_body_height = state.doll.box_height * ro::ui::kCharacterAvatarBodyFloor}};
  const ro::render::IconTexture* drawn = doll_->Draw(shot);
  if (drawn == nullptr) {
    return;
  }
  state.doll.image = ro::ui::SkinImage{.id = static_cast<ImTextureID>(drawn->texture.name()),
                                       .width = state.doll.box_width,
                                       .height = state.doll.box_height};
}

void StylePreview::Close() {
  if (doll_.has_value()) {
    doll_->Close();
  }
}

}  // namespace moonclient
