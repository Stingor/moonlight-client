#pragma once

#include <cstdint>

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// L'ILLUSTRATION D'UN PNJ : un portrait, une carte, un décor, qu'un script montre
// pendant qu'il parle.
//
// Trois places EN BAS de l'écran, où l'image se tient sans prendre la souris —
// on continue de cliquer le monde à travers —, et deux AU MILIEU, où elle se
// déplace et se ferme : dans une fenêtre titrée, ou sans cadre.
enum class IllustrationAnchor : std::uint8_t {
  kBottomLeft,
  kBottomMiddle,
  kBottomRight,
  kCenteredWindow,
  kCenteredFloating,
};

// DE COMBIEN LE CENTRE D'UNE ILLUSTRATION DE CÔTÉ S'ÉCARTE DU BORD, en pixels
// d'interface : à gauche, son centre est à cette distance du bord gauche ; à
// droite, du bord droit. C'est l'écart du client d'origine.
inline constexpr float kIllustrationSideCenterPixels = 200.0F;

struct NpcIllustrationView {
  SkinImage image;
  IllustrationAnchor anchor = IllustrationAnchor::kBottomMiddle;
};

struct NpcIllustrationAction {
  // Le joueur a fermé l'illustration : elle ne revient qu'au prochain `cutin`.
  bool closed = false;
};

// LE COIN HAUT-GAUCHE D'UNE ILLUSTRATION DU BAS, en pixels d'écran.
//
// L'image, déjà à l'échelle de l'interface, a son BORD BAS sur celui de l'écran,
// et son centre horizontal à l'écart du bord (gauche, droite) ou au milieu. Les
// deux ancrages du milieu n'ont pas de place imposée : ils rendent le centre de
// l'écran, moins la moitié de l'image.
[[nodiscard]] ImVec2 IllustrationTopLeft(IllustrationAnchor anchor, ImVec2 image_size,
                                         ImVec2 screen_origin, ImVec2 screen_size, float scale);

// Dessine l'illustration. Rien, et aucune action, si l'image n'est pas utilisable.
[[nodiscard]] NpcIllustrationAction DrawNpcIllustration(const NpcIllustrationView& view,
                                                        const SkinArt& art);

}  // namespace ro::ui
