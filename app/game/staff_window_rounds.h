#pragma once

#include "ro/ui/chat_block_log_window.h"
#include "ro/ui/entity_inspector_window.h"

namespace moonclient {

// ── LES FICHES DU STAFF, ALLER ET RETOUR ────────────────────────────────────
//
// Ce que `UiStage` tient pour chacune : la vue descend d'`app` à chaque image,
// la réponse remonte à la suivante. Vue nulle, rien ne se dessine.

// « PROPRIÉTÉS… » : ce que le serveur sait d'une entité.
struct EntityInspectorRound {
  ro::ui::EntityInspectorWindowState state;
  const ro::ui::EntityInspectorView* view = nullptr;
  ro::ui::EntityInspectorAnswer answer;
};

// « JOURNAL DES BLOCAGES DE CHAT » : les @mute et @unmute qui ont visé un joueur.
struct ChatBlockLogRound {
  ro::ui::ChatBlockLogWindowState state;
  const ro::ui::ChatBlockLogView* view = nullptr;
  ro::ui::ChatBlockLogAnswer answer;
};

}  // namespace moonclient
