#pragma once

#include <span>
#include <string>
#include <vector>

#include "ro/ui/card_album_window.h"
#include "ro/ui/options_window.h"

#include "game/settings_file.h"
#include "options/quick_cast_options.h"

namespace moonclient {

// ── LA PAGE « ALBUM DE CARTES » ─────────────────────────────────────────────
//
// Le bouton qui ouvre l'album — rien d'autre ne l'ouvre que lui et le
// raccourci —, puis quatre cases.
//
// ⚠ LES LIGNES PORTENT UN IDENTIFIANT COMPOSÉ, hors de `GameOption`, comme
// celles du quick cast, dont la plage précède la sienne.
//
// 🔴 DEUX PAGES SUR LA MÊME PLAGE SE VOLENT LEURS LIGNES : la première page
// interrogée prend l'identifiant pour le sien, et la case cochée ici réglerait
// l'Atlas. D'où l'assertion, qui chaîne chaque plage à la précédente.
inline constexpr int kCardAlbumRowBase = 5900;

enum class CardAlbumRow : int {
  kOpen = 0,
  kNameIcon,
  kSources,
  kDropperRim,
  kAutoSacrifice,
  kCount,
};

static_assert(kQuickCastRowBase + static_cast<int>(QuickCastRow::kCount) <= kCardAlbumRowBase,
              "la plage de l'album de cartes chevauche celle du quick cast");

[[nodiscard]] constexpr int CardAlbumRowId(CardAlbumRow row) {
  return kCardAlbumRowBase + static_cast<int>(row);
}

class CardAlbumOptions {
 public:
  [[nodiscard]] const ro::ui::CardAlbumLook& look() const noexcept { return look_; }

  // Le libellé de la touche qui ouvre l'album, ou vide sans touche. ⚠ POSÉ PAR
  // L'APPELANT avant la composition : cette page ne connaît pas les raccourcis.
  void NoteOpenKey(std::string label) { open_key_ = std::move(label); }

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Refait les lignes, à chaque image où le panneau est ouvert.
  void Compose();
  [[nodiscard]] std::span<const ro::ui::OptionRowView> rows() const noexcept { return rows_; }

  // Rend vrai quand la ligne était l'une des siennes. ⚠ LE BOUTON « OUVRIR »
  // EN EST, mais ne change aucun réglage : c'est la partie qui l'ouvre.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

 private:
  ro::ui::CardAlbumLook look_;
  std::string open_key_;
  std::vector<ro::ui::OptionRowView> rows_;
};

}  // namespace moonclient
