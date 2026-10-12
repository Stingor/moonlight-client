#pragma once

#include "ro/ui/status_portrait.h"
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

}  // namespace moonclient
