#include "ro/sim/shortcut_bar.h"

#include <algorithm>

namespace ro::sim {

void ShortcutBars::Assign(std::size_t index, ShortcutSlot what) {
  // ⚠ L'UN CHASSE L'AUTRE, ici et nulle part ailleurs. Une case qui porterait
  // les deux ferait deux gestes pour une touche, et chaque lecteur trancherait
  // à sa façon.
  if (what.item_id != 0) {
    what.skill_id = 0;
    what.level = 0;
  }
  if (index >= slots_.size()) {
    return;
  }
  // ⚠ UNE ENTRÉE NE TIENT QU'UNE CASE DE SA BARRE. Le jeu retire celle qu'on
  // dépose de son ancienne place, sans quoi un dépôt la DUPLIQUE — et deux cases
  // voisines pour la même compétence ne servent qu'à se tromper de touche.
  //
  // 🔴 DE SA BARRE, ET PAS AU-DELÀ. Balayer les huit interdirait de poser `Heal`
  // sur la barre de soin ET sur celle de mêlée, c'est-à-dire la chose même pour
  // quoi l'on fait huit barres (fiche 0038).
  if (what.filled()) {
    const std::size_t first = ShortcutIndex(ShortcutBarOfIndex(index), 0);
    for (std::size_t rank = 0; rank < kShortcutSlotsPerBar; ++rank) {
      ShortcutSlot& other = slots_[first + rank];
      // 🔴 LA COMPARAISON DOIT PORTER SUR LE MÊME GENRE. Comparer les seuls
      // identifiants de compétence viderait toutes les cases d'OBJET en posant
      // un objet : leur identifiant de compétence vaut zéro, comme celui de la
      // case qu'on pose — et les cases vides y passeraient aussi.
      const bool same = what.holds_item() ? other.item_id == what.item_id
                                          : other.skill_id != 0 && other.skill_id == what.skill_id;
      if (same) {
        other = ShortcutSlot{};
      }
    }
  }
  slots_[index] = what;
  ++revision_;
}

void ShortcutBars::Clear(std::size_t index) {
  if (index >= slots_.size() || !slots_[index].filled()) {
    return;
  }
  slots_[index] = ShortcutSlot{};
  ++revision_;
}

void ShortcutBars::Swap(std::size_t first, std::size_t second) {
  if (first >= slots_.size() || second >= slots_.size() || first == second) {
    return;
  }
  // Deux cases vides qui s'échangent ne changent rien : compter une révision
  // ferait réécrire le fichier pour un glisser qui n'a rien déplacé.
  if (!slots_[first].filled() && !slots_[second].filled()) {
    return;
  }
  std::swap(slots_[first], slots_[second]);
  ++revision_;
}

ShortcutSlot ShortcutBars::At(std::size_t index) const {
  return index < slots_.size() ? slots_[index] : ShortcutSlot{};
}

std::span<const ShortcutSlot> ShortcutBars::BarSlots(std::size_t bar) const {
  if (bar >= kShortcutBarCount) {
    return {};
  }
  return std::span<const ShortcutSlot>(slots_).subspan(ShortcutIndex(bar, 0), kShortcutSlotsPerBar);
}

std::size_t ShortcutBars::used_span(std::size_t bar) const {
  const std::span<const ShortcutSlot> mine = BarSlots(bar);
  for (std::size_t past = mine.size(); past > 0; --past) {
    if (mine[past - 1].filled()) {
      return past;
    }
  }
  return 0;
}

}  // namespace ro::sim
