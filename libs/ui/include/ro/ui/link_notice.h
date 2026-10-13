#pragma once

#include <string>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// CE QUE LE JOUEUR VOIT DE LA LIAISON AU SERVEUR : rien tant qu'elle tient, un
// bandeau quand elle se tait, une boîte quand elle est perdue.
//
// ⚠ L'INTERFACE NE DÉCIDE RIEN : `app` juge la liaison, compose le texte, et
// lit la réponse. Cette couche dessine et rend le geste.
struct LinkNoticeState {
  // LE TEXTE DE LA BOÎTE, déjà traduit. Vide : aucune boîte.
  //
  // ⚠ TANT QU'IL EST LÀ ET SANS RÉPONSE, LA BOÎTE SE REDEMANDE à chaque image :
  // une autre boîte modale ouverte après elle la remplacerait, et il n'y a plus
  // de serveur à qui la seconde pourrait servir.
  std::string text;
  // Posé ici quand le joueur a pris acte. `app` le lit.
  bool acknowledged = false;
  // Le bandeau « le serveur ne répond plus », discret, en haut de l'écran.
  bool unsteady = false;
};

// L'identifiant de la boîte de perte, unique dans l'image.
inline constexpr const char* kLinkLossBoxId = "##liaison_perdue";

// Où se pose le bandeau, en pixels d'interface depuis le haut de l'écran, et
// ce qui sépare son texte de son fond.
inline constexpr float kLinkBannerTopPixels = 50.0F;
inline constexpr float kLinkBannerPaddingPixels = 6.0F;

// Dessine le bandeau puis la boîte, selon l'état. ⚠ APRÈS LES FENÊTRES : le
// bandeau passe par-dessus, et la boîte est modale.
void DrawLinkNotice(LinkNoticeState& state, const SkinArt& art);

}  // namespace ro::ui
