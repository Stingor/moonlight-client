#pragma once

#include "ro/ui/skin_art.h"

namespace ro::ui {

// L'ÉCRAN DE MISE À JOUR : le client se patche avant d'ouvrir ses archives.
//
// ⚠ IL S'AFFICHE SANS HABILLAGE : l'habillage vit dans les archives que le
// patch est justement en train de réécrire. C'est le style d'ImGui par défaut,
// sur un fond noir, et rien d'autre.
//
// ⚠ IL NE SAIT RIEN DU PATCHER : `app` lui passe des textes déjà traduits et
// un avancement, et il rend les intentions du joueur.

struct PatchScreenView {
  // L'illustration de fond, étirée plein cadre comme sur l'écran de connexion.
  // Inutilisable : un fond noir.
  SkinImage backdrop;
  // Ce qui se passe, en une ligne : « Mise à jour 3 sur 7 ».
  const char* headline = "";
  // Le détail : le nom de l'archive, ou le motif d'un échec.
  const char* detail = "";
  // L'avancement de 0 à 1, ou négatif quand il ne se mesure pas encore.
  float fraction = -1.0F;
  // Vrai : le patch s'est arrêté en échec, et le joueur choisit la suite.
  bool failed = false;
  // Libellés des trois choix, montrés seulement sur un échec.
  const char* retry_label = "";
  const char* play_label = "";
  const char* quit_label = "";
};

struct PatchScreenAnswers {
  bool retry = false;
  bool play_anyway = false;
  bool quit = false;
};

// Peint l'écran dans l'image d'ImGui en cours.
[[nodiscard]] PatchScreenAnswers DrawPatchScreen(const PatchScreenView& view);

}  // namespace ro::ui
