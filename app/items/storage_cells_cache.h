#pragma once

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>

#include "ro/ui/inventory_window.h"

namespace moonclient {

// CE DONT LES CASES DU COFFRE SONT FAITES, lu à chaque image.
//
// ⚠ DES VUES ET DES COMPTES, rien de possédé : la porte copie ce qu'elle doit
// retenir, et seulement quand les cases se refont.
struct StorageCellsInputs {
  // `Inventory::revision` du coffre : son contenu, son nom, sa capacité.
  std::uint32_t storage_revision = 0;
  ro::ui::ItemTab tab = ro::ui::ItemTab::kAll;
  std::string_view filter;
  int sub_category = 0;
  // Les favoris du joueur. Nul : aucun.
  const std::set<std::uint32_t>* favorites = nullptr;
  // `StorageFacts::generation` : les prix, sous-types et emplacements.
  std::uint64_t facts_generation = 0;
  // `ItemIcons::generation` : un cache vidé rend ses textures caduques.
  std::uint64_t icons_generation = 0;
  // `CreatorNames::revision` : un nom de forgeron arrivé change un nom composé.
  std::uint64_t creators_revision = 0;
  // `core::LanguageSerial` : les libellés des sous-types sont traduits.
  std::size_t language_serial = 0;
};

// LA PORTE DES CASES DU COFFRE : dit quand elles sont à refaire.
//
// 🔴 UN COFFRE PEUT COMPTER DES MILLIERS D'OBJETS, et composer leurs noms à
// chaque image coûterait des millisecondes pour un résultat identique. Les
// cases se refont quand ce qu'elles lisent a changé, et alors seulement.
//
// ⚠ CE QU'ELLE COMPARE EST TOUT CE QUI ENTRE DANS UNE CASE. Une entrée oubliée
// ici se verrait comme une case figée : un prix qui n'arrive pas, une étoile
// qui ne s'allume pas.
class StorageCellsCache {
 public:
  // Vrai quand les cases sont à refaire ; la porte retient alors ces entrées.
  [[nodiscard]] bool NeedsRebuild(const StorageCellsInputs& now);

  // Oublie tout : la prochaine demande refera les cases. Pour ce qui change sans
  // compte — les forgerons qu'on confie à la vue.
  void Forget() noexcept { built_ = false; }

 private:
  bool built_ = false;
  std::uint32_t storage_revision_ = 0;
  ro::ui::ItemTab tab_ = ro::ui::ItemTab::kAll;
  std::string filter_;
  int sub_category_ = 0;
  std::set<std::uint32_t> favorites_;
  std::uint64_t facts_generation_ = 0;
  std::uint64_t icons_generation_ = 0;
  std::uint64_t creators_revision_ = 0;
  std::size_t language_serial_ = 0;
};

}  // namespace moonclient
