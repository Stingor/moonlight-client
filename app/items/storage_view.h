#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/net/connection.h"
#include "ro/render/item_icons.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/storage_window.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "items/creator_names.h"
#include "items/item_subcategory.h"
#include "items/storage_cells_cache.h"
#include "items/storage_favorites.h"
#include "items/storage_tab_names.h"

namespace moonclient {

// LE COFFRE, VU DE LA PARTIE.
//
// ⚠ IL CROISE LES MÊMES QUATRE SOURCES QUE SES DEUX SŒURS, et de la même façon :
// c'est `BuildItemCells` qui le fait pour les trois. Ce qui lui est propre tient
// à ce qu'il est une SESSION — son ouverture, son nom et sa fermeture viennent
// du serveur, et rien ici ne les décide.
class StorageView {
 public:
  // LES NOMS DES FORGERONS, confiés une fois la description ouverte.
  void SeeCreators(CreatorNames creators) noexcept {
    creators_ = creators;
    cells_cache_.Forget();
  }

  StorageView(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
              ro::render::ItemIcons* icons);

  // ⚠ `favorites` EST LA LISTE DU JOUEUR, et elle vit dans les réglages : c'est
  // là qu'on sait l'écrire. La vue ne fait que la LIRE — un onglet qui rangerait
  // lui-même aurait deux vérités à tenir d'accord avec le fichier.
  // ⚠ `sub_category` VIENT DE LA FENÊTRE, qui l'a écrit à l'image précédente :
  // c'est elle qui porte le combo, comme elle porte l'onglet.
  void Refresh(const ro::sim::World& world, ro::ui::ItemTab tab, std::string_view filter,
               const StorageFavorites* favorites, int sub_category,
               const StorageTabNames* tab_names);

  [[nodiscard]] std::span<const ro::ui::InventoryCellView> cells() const noexcept { return cells_; }
  // LES COFFRES QU'ON PEUT OUVRIR, traduits pour la fenêtre. ⚠ REFAITS À CHAQUE
  // IMAGE : ce sont des vues sur ce que le monde porte, et la liste du monde
  // change à chaque ouverture.
  [[nodiscard]] std::span<const ro::ui::StorageShelfView> shelves() const noexcept {
    return shelves_;
  }
  [[nodiscard]] ro::ui::StorageTally tally() const noexcept { return tally_; }
  // LES SOUS-TYPES PRÉSENTS sous l'onglet courant, triés par clé. ⚠ REFAITS À
  // CHAQUE CHANGEMENT DE LANGUE : leurs libellés sont traduits, et un texte
  // traduit ne survit pas au catalogue qui l'a donné.
  [[nodiscard]] std::span<const ro::ui::StorageSubCategoryView> sub_categories() const noexcept {
    return sub_views_;
  }
  [[nodiscard]] std::string_view name() const noexcept { return name_; }
  // Ce que la fenêtre a demandé de basculer, zéro si rien : `app` bascule et
  // écrit, parce que c'est lui qui possède le fichier.

  // ⚠ ÉMET ET S'ARRÊTE LÀ, comme les deux autres. Et la fermeture en fait
  // partie : c'est un verbe du serveur, pas un geste d'affichage.
  void Apply(const ro::ui::StorageWindowAction& asked, ro::net::Connection& link,
             const ro::sim::World& world);

  // OUVRIR OU REFERMER LA SESSION, selon celle en cours.
  //
  // 🔴 DEUX CHEMINS D'OUVERTURE, ET LE SECOND EST UN REPLI. rAthena n'expose
  // l'ouverture d'un coffre par AUCUN paquet — un script de PNJ, ou « @storage »
  // tapé dans la conversation —, et le client d'origine ne sait pas l'ouvrir
  // non plus : il attend qu'on le lui ouvre. Moonlight a ajouté `CZ 0x0F1D`, et
  // c'est par lui qu'on passe DÈS QUE LE SERVEUR A DIT SES COFFRES : il
  // BASCULE, là où les commandes referment quand un coffre est déjà ouvert.
  //
  // ⚠ LA COMMANDE RESTE POUR QUI NE LES A PAS DITS — un serveur sans cette
  // extension, ou une liste qui n'est pas encore arrivée. La fermeture, elle,
  // a son paquet depuis toujours.
  void ToggleSession(ro::net::Connection& link, const ro::sim::World& world);

 private:
  // Les cases, leurs sous-types, leurs valeurs et le compte : tout ce que la
  // porte `cells_cache_` garde d'une image à l'autre.
  void ComposeCells(const ro::sim::World& world, ro::ui::ItemTab tab, std::string_view filter,
                    int sub_category);

  // ⚠ UN POINTEUR ET NON UNE COPIE : la liste est écrite ailleurs — au clic
  // suivant — et une copie montrerait l'étoile d'avant jusqu'au prochain
  // rafraîchissement.
  const StorageFavorites* favorites_ = nullptr;
  std::vector<ro::ui::StorageSubCategoryView> sub_views_;
  const ro::vfs::ItemTable* items_;
  const ro::vfs::ItemWords* words_;
  CreatorNames creators_;
  ro::render::ItemIcons* icons_;

  // 🔴 LES CASES, LEURS NOMS, LES SOUS-TYPES ET LE COMPTE NE SE REFONT QUE
  // QUAND CE QU'ILS LISENT A CHANGÉ : un coffre de milliers d'objets coûterait
  // sinon des millisecondes par image. `cells_cache_` en tient la porte.
  StorageCellsCache cells_cache_;
  // Les forgerons que la dernière composition n'a pas su nommer : leur demande
  // se répète à chaque image, comme quand tout se recomposait.
  std::vector<std::uint32_t> creators_pending_;
  std::vector<std::string> names_;
  std::vector<ro::ui::StorageShelfView> shelves_;
  std::vector<ro::ui::InventoryCellView> cells_;
  ro::ui::StorageTally tally_;
  std::string name_;
};

}  // namespace moonclient
