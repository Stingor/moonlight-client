#pragma once

// CE QUE LES DESSINS DE `UiStage` REÇOIVENT D'UN SEUL TENANT : le dialogue, la
// fiche du personnage et la scène de sélection.
//
// ⚠ À PART DE `game/ui_stage.h` : seuls ceux qui COMPOSENT ces vues, et l'étage
// qui les dessine, ont besoin de leurs fenêtres.

#include <optional>
#include <span>
#include <string_view>

#include "ro/ui/character_create_window.h"
#include "ro/ui/character_delete_window.h"
#include "ro/ui/character_select_window.h"
#include "ro/ui/character_window.h"
#include "ro/ui/guild_tab.h"
#include "ro/ui/npc_dialog_window.h"
#include "ro/ui/npc_illustration_window.h"
#include "ro/ui/preset_tab.h"
#include "ro/ui/status_preview.h"

namespace moonclient {

// LE DIALOGUE À DESSINER : ce qu'il montre, et de quoi peindre ce qu'il porte.
//
// ⚠ LES DEUX ENSEMBLE, parce qu'ils ne se lisent pas séparément : une page qui
// appelle des icônes sans savoir les charger montrerait des trous. `view` nul
// veut dire qu'aucune conversation n'est ouverte.
struct UiNpcTalk {
  const ro::ui::NpcDialogView* view = nullptr;
  ro::ui::NpcDialogArt art;
  // L'ÉTAT QUE LE JOUEUR SURVOLAIT À L'IMAGE PRÉCÉDENTE, composé par `app`.
  //
  // ⚠ IL RETARDE D'UNE IMAGE, et c'est sans conséquence : la fenêtre apprend
  // ce qu'elle survole par la RÉPONSE qu'elle vient de rendre, et une bulle
  // n'apparaît pas avant que la souris se soit arrêtée. Même mécanique que
  // l'aperçu d'un objet.
  ro::ui::StatusPreviewView status_preview;
  // L'ILLUSTRATION QUE LE PNJ MONTRE, s'il en montre une. Elle voyage avec le
  // dialogue parce qu'elle se dessine avec lui, juste avant : il passe devant.
  std::optional<ro::ui::NpcIllustrationView> illustration;
};

// CE QUE LA FICHE MONTRE, groupé : trois vues qui ne se lisent qu'ensemble,
// et qui viendraient sinon s'ajouter une à une à une signature déjà longue.
struct UiCharacterSheetView {
  ro::ui::CharacterHeaderView header;
  std::span<const ro::ui::EquipSlotView> slots;
  ro::ui::CharacterStatsView stats;
  ro::ui::PresetTabView presets;
  ro::ui::CharacterTitlesView titles;
  ro::ui::GuildView guild;
  // Les onglets de l'homoncule et du mercenaire.
  ro::ui::CompanionTabsView companions;
  // L'INVITATION DANS UNE GUILDE, et LA GUILDE qui propose une alliance à la
  // nôtre, en attente de réponse, ou vides. ⚠ HORS DE LA FICHE : elles
  // arrivent fenêtre fermée, et doivent se voir quand même.
  std::string_view guild_invitation;
  std::string_view guild_alliance;
};

// CE QUE LA SCÈNE DE SÉLECTION A RENDU : la tablée, et la création qui se
// pose dessus.
//
// ⚠ LES DEUX ENSEMBLE, contrairement au formulaire : la création n'est pas
// un autre ÉCRAN mais une fenêtre POSÉE sur la salle, qui reste visible
// derrière elle. Les séparer reviendrait à effacer la tablée le temps de
// choisir un nom.
struct UiLobbySeatsAnswers {
  ro::ui::CharacterSelectAction seats;
  ro::ui::CharacterCreateAction create;
  ro::ui::CharacterDeleteAction remove;
};

// CE QUE LA SCÈNE DE SÉLECTION MONTRE, d'un seul tenant.
//
// ⚠ UNE STRUCTURE PLUTÔT QU'UNE FILE DE SIX PARAMÈTRES, dont trois vues et
// deux états : un appelant qui en intervertit deux ne s'en aperçoit qu'à
// l'écran.
struct UiLobbySeatsFrame {
  const ro::ui::CharacterSelectView* seats = nullptr;
  const ro::ui::CharacterCreateView* create = nullptr;
  const ro::ui::CharacterDeleteView* remove = nullptr;
};

}  // namespace moonclient
