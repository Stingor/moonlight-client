// LE PERSONNAGE QU'ON CRÉE : son pantin d'aperçu, et les coupes qu'on essaie.

#include "shell/newcomer_preview.h"

#include <cstddef>

#include "ro/render/sprite_frame.h"

namespace moonclient {
namespace {

// COMBIEN DE VIGNETTES NEUVES PAR IMAGE. La même valeur que l'éditeur de
// style : composer une case ouvre un sprite, et deux cent cinquante d'un coup
// se verraient.
constexpr int kThumbnailBudget = 12;

}  // namespace

ro::render::ActorLook NewcomerPreview::LookOf(const ro::ui::CharacterCreateState& state) {
  ro::render::ActorLook look;
  look.player = true;
  // ⚠ LE NOVICE, ET RIEN D'AUTRE : c'est la seule classe que la création
  // envoie. Voir `CharacterPhase::Create`.
  look.job = 0;
  look.sex = state.sex;
  look.hair = static_cast<std::uint16_t>(state.hair_style);
  look.hair_palette = static_cast<std::int16_t>(state.hair_palette);
  return look;
}

ro::ui::CharacterCreateView NewcomerPreview::Compose(const ro::ui::CharacterCreateState& state,
                                                     const ro::ui::CharacterCreateAction& asked,
                                                     std::uint32_t elapsed_ms) {
  ro::ui::CharacterCreateView view;
  if (!state.open()) {
    return view;
  }
  const ro::render::ActorLook look = LookOf(state);

  // ── LE PANTIN ─────────────────────────────────────────────────────────────
  if (asked.doll_width > 0.0F && asked.doll_height > 0.0F) {
    if (!doll_.has_value()) {
      doll_.emplace(*wardrobe_);
    }
    ro::render::ActorVignette::Pose pose;
    pose.look = look;
    // ⚠ DEBOUT, contrairement aux convives : on regarde un personnage qu'on
    // compose, pas un invité attablé. C'est la pose de l'écran de création
    // d'origine.
    pose.animation = ro::render::kAnimationIdle;
    pose.direction = static_cast<std::size_t>(state.direction);
    pose.animated = false;
    pose.elapsed_ms = static_cast<float>(elapsed_ms);
    pose.box.width = static_cast<std::int32_t>(asked.doll_width);
    pose.box.height = static_cast<std::int32_t>(asked.doll_height);
    // ⚠ AJUSTÉ À SA BOÎTE, ET NON À UNE ÉCHELLE IMPOSÉE : il est SEUL. Il n'y
    // a personne à côté de qui il devrait être comparable, et l'ajustement lui
    // donne toute la place. Voir `VignetteBox::scale`, qui dit pourquoi la
    // tablée fait l'inverse.
    const ro::render::IconTexture& drawn = doll_->Get(pose);
    if (drawn.usable()) {
      view.doll = ro::ui::SkinImage{.id = static_cast<ImTextureID>(drawn.texture.name()),
                                    .width = asked.doll_width,
                                    .height = asked.doll_height};
    }
  }

  // ── LA GRILLE OUVERTE ─────────────────────────────────────────────────────
  thumbs_.clear();
  if (asked.picker.open != shown_) {
    // ⚠ LES TEXTURES DE LA GRILLE PRÉCÉDENTE SE RENDENT ICI, au changement, et
    // pas à chaque image : deux grilles gardées ensemble tiendraient trois
    // cent trente images, une grille refermée n'en vaut aucune.
    if (thumbnails_.has_value()) {
      thumbnails_->Clear();
    }
    shown_ = asked.picker.open;
  }
  if (asked.picker.open == ro::ui::CreatePicker::kNone || asked.picker.count <= 0 ||
      asked.picker.cell <= 0.0F) {
    return view;
  }
  if (!thumbnails_.has_value()) {
    thumbnails_.emplace(*wardrobe_);
  }
  thumbnails_->BeginFrame(kThumbnailBudget);

  const auto side = static_cast<std::int32_t>(asked.picker.cell);
  ro::render::ActorThumbnails::Cell cell;
  // ⚠ LA TÊTE SEULE : une case de trente pixels réduirait la tête d'un
  // personnage entier à cinq, où l'on ne distinguerait aucune coupe.
  cell.subject = ro::render::ActorThumbnails::Subject::kHead;
  cell.box.width = side;
  cell.box.height = side;
  thumbs_.reserve(static_cast<std::size_t>(asked.picker.count));
  for (int rank = 0; rank < asked.picker.count; ++rank) {
    const int value = asked.picker.first + rank;
    cell.look = look;
    if (asked.picker.open == ro::ui::CreatePicker::kHairStyle) {
      cell.look.hair = static_cast<std::uint16_t>(value);
    } else {
      // ⚠ CHAQUE CASE PORTE LA COUPE CHOISIE : on compare des teintes sur la
      // coiffure qu'on portera, pas sur une qu'on vient d'abandonner.
      cell.look.hair_palette = static_cast<std::int16_t>(value);
    }
    const ro::render::IconTexture& drawn = thumbnails_->Get(cell);
    thumbs_.push_back(drawn.usable()
                          ? ro::ui::SkinImage{.id = static_cast<ImTextureID>(drawn.texture.name()),
                                              .width = asked.picker.cell,
                                              .height = asked.picker.cell}
                          : ro::ui::SkinImage{});
  }
  view.thumbnails = thumbs_;
  view.thumbnails_first = asked.picker.first;
  return view;
}

void NewcomerPreview::Close() {
  if (thumbnails_.has_value()) {
    thumbnails_->Clear();
  }
  thumbs_.clear();
  shown_ = ro::ui::CreatePicker::kNone;
}

}  // namespace moonclient
