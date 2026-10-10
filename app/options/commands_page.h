#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ro/net/connection.h"
#include "ro/sim/command_settings.h"
#include "ro/sim/item_facts.h"
#include "ro/ui/commands_page.h"
#include "ro/ui/options_window.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

namespace moonclient {

// LA PAGE « COMMANDS SETTINGS », vue de la partie : le miroir des réglages que le
// serveur tient pour le personnage, et le chemin de ce que le joueur y change.
//
// ⚠ AUCUN FICHIER : ces réglages vivent dans les registres du personnage, côté
// serveur. Les écrire ici aussi ferait deux sources, dont l'une mentirait dès
// qu'une commande tapée dans le chat change l'autre.
//
// Chaque image où la page est sous les yeux : `Compose`, puis le dessin — par le
// corps que `Body` rend au panneau —, puis `Apply`.
class CommandsPage {
 public:
  // Ce que les trames 0x0F05 et 0x0F07 remplissent.
  [[nodiscard]] ro::sim::CommandSettings& settings() noexcept { return settings_; }

  // Refait la vue. `items` peut être nul : les items de la liste se montrent
  // alors par leur seul identifiant.
  void Compose(const ro::sim::ItemFacts* facts, const ro::vfs::ItemTable* items,
               const ro::vfs::ItemWords& words);

  // L'ITEM DONT LA DESCRIPTION EST OUVERTE, zéro pour aucun. ⚠ À CHAQUE IMAGE,
  // page ouverte ou non : la page propose le DERNIER item lu, fenêtre refermée
  // comprise — c'est en lisant une description qu'on décide de le ramasser.
  void NoteDescribed(std::uint32_t item_id);

  // Le corps que le panneau appelle. ⚠ IL RETIENT `this` : la page doit
  // survivre au panneau qui le garde.
  [[nodiscard]] ro::ui::OptionSectionBody Body();

  // Envoie ce que le joueur vient de demander, et note d'avance ce que le
  // serveur ne renverra pas. Muet sans liaison ouverte : un geste qui part dans
  // le vide ne doit rien changer à l'écran.
  void Apply(ro::net::Connection& link, ro::sim::ItemFacts* facts);

 private:
  // Ce que le serveur vient de redire des presets : la liste choisit alors le
  // preset chargé, et le champ du nom reprend le sien.
  void FollowPresetAnnounce();
  void ApplySetting(ro::net::Connection& link, ro::ui::CommandsField field, int value);

  ro::sim::CommandSettings settings_;
  ro::ui::CommandsPageState state_;
  ro::ui::CommandsPageAction asked_;
  ro::ui::CommandsPageView view_;
  std::uint32_t presets_seen_ = 0;

  // Les tampons derrière la vue. ⚠ REMPLIS AVANT QU'UNE VUE NE S'Y PRENNE.
  std::vector<std::string> names_;
  std::vector<ro::ui::CommandsAutolootEntry> entries_;
  std::vector<ro::ui::CommandsPresetView> presets_;
  std::uint32_t last_viewed_item_ = 0;
  // L'item dont `last_viewed_name_` porte le nom : il ne se recompose qu'au
  // changement.
  std::uint32_t last_viewed_named_ = 0;
  std::string last_viewed_name_;
};

}  // namespace moonclient
