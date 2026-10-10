#pragma once

#include <cstdint>
#include <vector>

#include "ro/render/skill_icons.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/description_window.h"
#include "ro/vfs/skill_words.h"

namespace moonclient {

// LA DESCRIPTION D'UNE COMPÉTENCE, VUE DE LA PARTIE.
//
// ⚠ ELLE CROISE TROIS SOURCES, comme celle d'un objet : l'arbre du client pour
// le nom affiché, la table de descriptions pour le texte, et les icônes. `ui`
// n'en connaît aucune.
//
// 🔴 ELLE NE RECOPIE RIEN. Le nom vit dans l'arbre et les lignes dans la table,
// tous deux montés une fois au démarrage et immobiles ensuite : la vue pointe
// dessus. C'est ce qui la sépare de celle d'un objet, dont le nom se COMPOSE à
// chaque image — raffinement, cartes, forge — et doit donc être gardé quelque
// part.
//
// ⚠ ET SA CLÉ EST UN IDENTIFIANT, PAS UNE CASE DE GRILLE : on décrit aussi bien
// une compétence apprise qu'une qu'on n'aura jamais, et une description ouverte
// survit à un changement de métier qui retire la case.
class SkillDescriptionView {
 public:
  // Les trois peuvent être nuls : une installation sans tables de compétences
  // garde un jeu entier, moins ces descriptions.
  SkillDescriptionView(const ro::sim::SkillTree* tree, const ro::vfs::SkillWords* words,
                       ro::render::SkillIcons* icons);

  // Refait la vue pour la compétence que la fenêtre montre. À appeler une fois
  // par image, avant le dessin. Ne coûte rien tant que rien n'est montré.
  void Refresh(std::uint16_t skill_id, const ro::sim::World& world);

  [[nodiscard]] const ro::ui::SkillDescriptionView& view() const noexcept { return view_; }

 private:
  const ro::sim::SkillTree* tree_ = nullptr;
  const ro::vfs::SkillWords* words_ = nullptr;
  ro::render::SkillIcons* icons_ = nullptr;

  // ⚠ LA TABLE DE CAST SE RECOPIE, et il le faut : celle du monde est faite de
  // types de `sim`, que `ui` ne connaît pas. La recopie ne coûte qu'une dizaine
  // d'entiers, et seulement quand la fenêtre est ouverte.
  std::vector<ro::ui::SkillCastLevelView> cast_;
  ro::ui::SkillDescriptionView view_;
};

}  // namespace moonclient
