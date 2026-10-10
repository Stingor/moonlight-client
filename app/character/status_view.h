#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/render/status_icons.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/status_cell.h"
#include "ro/ui/status_preview.h"
#include "ro/vfs/status_table.h"

namespace moonclient {

// CE QUE LA BARRE D'ÉTATS MONTRE : le croisement de trois sources qui ne se
// connaissent pas.
//
// 🔴 ET C'EST `app` QUI LES CROISE, parce qu'il est le seul à les voir toutes.
// `sim` tient les états et leurs durées, `vfs` les noms et les images, `render`
// les textures, `ui` ne possède rien. Aucune des quatre ne peut faire ce travail
// sans franchir le graphe des couches.
//
// 🔴 DEUX SOURCES D'ÉTATS, ET C'EST TOUT LE CHANTIER. Les buffs arrivent en EFST,
// avec leur durée. Le sommeil, le silence, le gel, l'aveuglement n'arrivent PAS :
// le serveur les transmet en bits d'`opt1`/`opt2`, et c'est au client de les
// traduire. Le client natif le fait ; nous n'en avons pas sous nous.

// L'ABRÉVIATION QU'UNE CASE PORTE FAUTE D'IMAGE.
//
// ⚠ CE REPLI N'EST PAS DÉCORATIF : soixante-sept des images nommées manquent à
// cette installation. Il se tire du NOM que la table donne, et de rien d'autre.
//
// Vide quand le nom ne donne aucune lettre latine — ou que la table ne nomme pas
// l'état : la case ne se dessine alors pas, et l'appelant ne lui réserve pas de
// place.
[[nodiscard]] std::string StatusAbbreviation(std::string_view name);

// Les cases de la barre, refaites à chaque image.
//
// ⚠ ELLE GARDE SES TAMPONS pour ne pas allouer soixante fois par seconde ; les
// vues qu'elle rend pointent dedans, et ne survivent donc pas au `Refresh`
// suivant.
class StatusView {
 public:
  // Les deux vivent plus longtemps que cette vue. Les icônes peuvent manquer —
  // une installation sans elles garde ses abréviations.
  StatusView(const ro::vfs::StatusTable* table, ro::render::StatusIcons* icons)
      : table_(table), icons_(icons) {}

  // `preview` : combien de FAUX états ajouter, zéro pour aucun.
  //
  // 🔴 RÉGLER UN AFFICHAGE D'ÉTATS SUPPOSE D'EN AVOIR. Sans buff il n'y a rien à
  // dimensionner, et attendre d'être en combat pour choisir une taille d'icône
  // n'est pas un réglage, c'est une devinette.
  //
  // ⚠ ET CE SONT DE VRAIS ÉTATS DE L'INSTALLATION, pris parmi ceux dont l'image
  // s'ouvre vraiment. Des numéros au hasard rempliraient la barre d'abréviations
  // et donneraient à croire que les icônes sont en panne — ou l'inverse.
  void Refresh(const ro::sim::World& world, std::uint32_t now_ms, int preview);

  // Les cases d'une liste d'états quelconque — celle d'une cible. Ni les
  // altérations du joueur, ni son métier : les icônes gardent celui qu'elles ont.
  void RefreshList(std::span<const ro::sim::ActiveStatus> statuses, std::uint32_t now_ms,
                   int preview);

  [[nodiscard]] std::span<const ro::ui::StatusCellView> cells() const noexcept { return cells_; }
  // Combien des premières cases sont de VRAIS états : l'aperçu se pose après
  // eux. Deux surfaces qui partagent ces cases en tirent chacune la leur.
  [[nodiscard]] std::size_t genuine() const noexcept { return genuine_cells_; }

  // CE QU'UN ÉTAT EST, pour l'aperçu d'un lien. Zéro rend une vue vide, que
  // l'affichage ne dessine pas.
  //
  // ⚠ SES PROPRES TAMPONS, ET NON CEUX DE LA BARRE : la barre se refait à chaque
  // image, et un aperçu qui pointerait dedans changerait d'état sous le curseur.
  //
  // ⚠ ET AUCUNE DURÉE : ce qu'un lien désigne est un ÉTAT, pas une occurrence en
  // cours. Voir `ui/status_preview.h`.
  [[nodiscard]] const ro::ui::StatusPreviewView& Preview(ro::sim::StatusEffectId effect);

 private:
  // Un état retenu, avant qu'on sache s'il se dessine.
  struct Pending {
    ro::sim::StatusEffectId effect = 0;
    std::uint32_t remaining_ms = 0;
    std::uint32_t total_ms = 0;
  };

  // Le nom de la table, ou rien. ⚠ LA VUE SURVIT À UNE TABLE ABSENTE : une
  // installation dont les Lua manquent garde ses icônes et ses durées, et perd
  // seulement les noms de ses infobulles.
  [[nodiscard]] std::string_view NameOf(ro::sim::StatusEffectId effect) const {
    return table_ == nullptr ? std::string_view() : table_->NameOf(effect);
  }

  // Les lignes que la table donne à cet état, ou aucune.
  [[nodiscard]] const std::vector<std::string>& DescriptionOf(
      ro::sim::StatusEffectId effect) const {
    static const std::vector<std::string> kNone;
    const ro::vfs::StatusInfo* info = table_ == nullptr ? nullptr : table_->Find(effect);
    return info == nullptr ? kNone : info->description;
  }

  [[nodiscard]] int TimeLineOf(ro::sim::StatusEffectId effect) const {
    const ro::vfs::StatusInfo* info = table_ == nullptr ? nullptr : table_->Find(effect);
    return info == nullptr ? 0 : info->time_line;
  }

  const ro::vfs::StatusTable* table_;
  ro::render::StatusIcons* icons_;

  // ── L'APERÇU D'UN LIEN, à part de la barre ────────────────────────────────
  std::vector<std::string_view> preview_lines_;
  ro::ui::StatusPreviewView preview_;

  // Les états dont l'image s'ouvre, cherchés UNE FOIS. ⚠ Le balayage téléverse
  // des textures : le refaire à chaque image coûterait des centaines de
  // lectures d'archive par seconde.
  void ScanDrawable(int want);

  // Les trois temps d'une composition : ce que le fil a dit, l'aperçu, puis les
  // cases.
  void Collect(std::span<const ro::sim::ActiveStatus> statuses, std::uint32_t now_ms);
  void AppendPreview(int preview);
  void BuildCells();

  std::vector<Pending> pending_;
  std::vector<ro::sim::StatusEffectId> ailments_;
  std::vector<ro::sim::StatusEffectId> drawable_;
  int scanned_for_ = 0;
  // ⚠ REMPLI EN ENTIER AVANT QU'UNE SEULE VUE NE LE POINTE : une vue prise en
  // cours de remplissage pendrait à la première réallocation du vecteur.
  std::vector<std::string> abbreviations_;

  // LES LIGNES DE DESCRIPTION DE TOUTES LES CASES, bout à bout.
  //
  // 🔴 UN SEUL TAMPON, ET REMPLI AVANT LA PREMIÈRE VUE. Chaque case reçoit une
  // TRANCHE de celui-ci ; un vecteur par case coûterait une allocation par état
  // et par image, et des tranches prises en cours de remplissage pendraient à la
  // première réallocation.
  //
  // ⚠ LES TEXTES, EUX, VIVENT DANS LA TABLE : ce tampon ne porte que des vues.
  std::vector<std::string_view> description_lines_;
  std::vector<ro::ui::StatusCellView> cells_;
  // Les vrais états, avant l'aperçu : parmi les retenus, puis parmi les cases.
  std::size_t genuine_pending_ = 0;
  std::size_t genuine_cells_ = 0;
};

}  // namespace moonclient
