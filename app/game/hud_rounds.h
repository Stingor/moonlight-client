#pragma once

#include <string>

#include "ro/ui/status_portrait.h"
#include "ro/ui/status_window.h"
#include "ro/ui/target_hud.h"

namespace moonclient {

// ── LES HUD DE CADRES LIBRES, ALLER ET RETOUR ───────────────────────────────
//
// Ce que `UiStage` tient pour chacun : sa vue et son allure descendent d'`app`
// à chaque image, sa réponse remonte à la suivante. Vue nulle, rien ne se
// dessine.

// LE HUD DE CIBLE. Sa vue ne descend que sous le mode Ciblage.
struct TargetHudRound {
  const ro::ui::TargetView* view = nullptr;
  ro::ui::TargetHudLook* look = nullptr;
  ro::ui::TargetHudAnswer answer;
};

// LE PORTRAIT DE STATUT DU JOUEUR. Sa vue ne descend qu'en jeu.
struct StatusPortraitRound {
  const ro::ui::StatusPortraitView* view = nullptr;
  ro::ui::StatusPortraitLook* look = nullptr;
  ro::ui::StatusPortraitAnswer answer;
};

// LA PETITE FENÊTRE DE STATUT, à côté de la fiche, même aller-retour : sa vue
// est recopiée par `app` à chaque image, et la flèche cliquée remonte à la
// suivante. Son interrupteur vit ici, retenu d'une session à l'autre comme
// celui de la fiche.
struct StatusWindowRound {
  ro::ui::StatusWindowState state;
  ro::ui::StatusWindowView view;
  ro::ui::StatusWindowAction answer;
  // LE NOM DE GUILDE QUE LA VUE DÉSIGNE, copié à chaque composition : la vue
  // ne pointe jamais dans le livre de guilde, que le réseau réécrit.
  std::string guild_name;
};

}  // namespace moonclient
