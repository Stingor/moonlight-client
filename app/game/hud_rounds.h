#pragma once

#include <string>

#include "ro/ui/basic_info_window.h"
#include "ro/ui/stat_preset_window.h"
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

// LA FENÊTRE « BASIC INFO » DU JEU : sa vue est recomposée par `app` à chaque
// image, et elle ne rend que le coin où la grille d'icônes s'accroche. Sa taille
// repliée ou dépliée est retenue avec la disposition du personnage.
struct BasicInfoRound {
  ro::ui::BasicInfoWindowState state;
  ro::ui::BasicInfoView view;
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

// LA FENÊTRE DES PRÉRÉGLAGES DE STATS, même aller-retour. ⚠ ELLE N'EST NI AU
// STATUT NI À LA FICHE : chacune y ouvre une porte, et c'est cet état unique
// qu'elles basculent. Fermer l'une ou l'autre la laisse donc ouverte.
struct StatPresetRound {
  ro::ui::StatPresetWindowState state;
  ro::ui::StatPresetWindowView view;
  ro::ui::StatPresetAction answer;
};

}  // namespace moonclient
