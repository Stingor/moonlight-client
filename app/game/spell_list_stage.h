#pragma once

// LA LISTE DES SORTS D'AUTOSPELL, DANS LA BOUCLE DE LA PARTIE : ce que la
// fenêtre montre de l'offre du serveur, et ce que sa réponse devient sur le fil.

#include <vector>

#include "ro/net/connection.h"
#include "ro/render/skill_icons.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/spell_list.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/spell_list_window.h"

namespace moonclient {

class SpellListStage {
 public:
  [[nodiscard]] ro::sim::SpellListOffer& offer() noexcept { return offer_; }

  // Recopie l'offre dans l'état et la vue de la fenêtre. Le nom et l'icône de
  // chaque sort viennent de l'arbre des compétences : le serveur ne propose que
  // des sorts que le joueur connaît. Le niveau armé se déduit du grimoire et du
  // lien d'âme du Sage.
  void Stage(const ro::sim::SkillTree* tree, ro::render::SkillIcons* icons,
             const ro::sim::World& world, ro::ui::SpellListWindowState& state,
             ro::ui::SpellListView& view);

  // Envoie la réponse — le sort, ou zéro pour le refus —, et ferme la liste.
  void Apply(const ro::ui::SpellListWindowAction& asked, ro::net::Connection& link);

 private:
  ro::sim::SpellListOffer offer_;
  std::vector<ro::ui::SpellListRow> rows_;
};

}  // namespace moonclient
