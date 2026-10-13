#pragma once

#include <cstdint>

#include <imgui.h>

namespace ro::ui {

// L'EFFET QUE `app` PEINT AVEC UN PANTIN — un effet de chapeau —, au moment
// même où l'interface se dessine : celui de l'aperçu d'une pièce, celui du
// portrait de statut.
//
// 🔴 UN RAPPEL DE DESSIN, ET NON UNE IMAGE : un effet se mélange à ce qu'il a
// derrière lui, et derrière le pantin il y a le jeu. Une image transparente
// perdrait tout ce qui s'ajoute ; le rappel peint dans l'image elle-même, à la
// place du pantin. Voir `ro::render::BeginInsetPaint`.
//
// 🔴 DEUX PASSES, COMME LA PARTIE : ce que la partie peint AVANT les corps — la
// file du sol, les fichiers qu'une fiche range avant le personnage — passe
// derrière l'image du pantin, le reste devant. Un seul rappel placé d'un côté
// ou de l'autre ne peut pas montrer un effet dont une moitié passe derrière.
struct DollOverlay {
  // Nul : rien à peindre.
  ImDrawCallback paint = nullptr;
  void* painter = nullptr;
};

// La passe qu'un rappel peint.
enum class DollOverlayPass : std::uint8_t {
  kBehind,  // avant l'image du pantin : ce que les corps recouvrent
  kFront,   // après elle
};

// CE QUE LE RAPPEL REÇOIT, copié dans la liste de dessin : son peintre, le
// rectangle du pantin en coordonnées d'écran de l'interface, et sa passe.
struct DollOverlayCall {
  void* painter = nullptr;
  ImVec2 min{0.0F, 0.0F};
  ImVec2 max{0.0F, 0.0F};
  DollOverlayPass pass = DollOverlayPass::kFront;
};

// Glisse le rappel dans la liste de dessin de la fenêtre courante, à la place
// où il se peindra, puis rend à l'interface l'état qu'il a changé. La liste
// COPIE l'appel : le même peut resservir, passe changée, pour le rappel
// suivant. Rien sans rappel.
void PaintDollOverlay(const DollOverlay& overlay, const DollOverlayCall& call);

}  // namespace ro::ui
