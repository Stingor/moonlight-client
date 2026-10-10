#pragma once

#include <imgui.h>

#include "ro/render/skin_pieces.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LES TROIS VISAGES D'UN BOUTON-IMAGE : au repos, survolé, enfoncé.
struct ButtonFaces {
  render::SkinPiece out;
  render::SkinPiece over;
  render::SkinPiece down;
};
inline constexpr ButtonFaces kOkFaces{.out = render::SkinPiece::kOkButtonOut,
                                      .over = render::SkinPiece::kOkButtonOver,
                                      .down = render::SkinPiece::kOkButtonDown};
inline constexpr ButtonFaces kCancelFaces{.out = render::SkinPiece::kCancelButtonOut,
                                          .over = render::SkinPiece::kCancelButtonOver,
                                          .down = render::SkinPiece::kCancelButtonDown};

// UN BOUTON DONT LE MOT EST DANS L'IMAGE, posé au curseur.
//
// Il montre son image enfoncée tant que le bouton de la souris, pressé sur lui,
// reste tenu AU-DESSUS de lui ; survolée quand le pointeur passe sans le tenir ;
// au repos sinon — tenu mais sorti, il revient au repos. Le pointeur prend la
// main au survol.
//
// ⚠ SANS SON IMAGE, il redevient un bouton habillé à libellé : une installation
// qui ne la porte pas garde un bouton lisible.
[[nodiscard]] bool BitmapButton(const char* id, const SkinArt& art, const ButtonFaces& faces,
                                const char* fallback_label, ImVec2 size);

}  // namespace ro::ui
