#pragma once

#include <array>
#include <span>
#include <vector>

#include "ro/ui/description_window.h"
#include "ro/ui/options_window.h"

#include "game/settings_file.h"
#include "options/card_album_options.h"

namespace moonclient {

// ── LA PAGE « DESCRIPTIONS » ────────────────────────────────────────────────
//
// Où s'ouvrent la description d'un item et celle d'un skill. Par défaut près de
// la souris, la fenêtre descendant à droite du curseur, décalée de douze
// pixels.
//
// ⚠ LES LIGNES PORTENT UN IDENTIFIANT COMPOSÉ, hors de `GameOption`, comme
// celles de l'album de cartes, dont la plage précède la sienne.
inline constexpr int kDescriptionRowBase = 6000;

enum class DescriptionRow : int {
  kAtCursor = 0,
  kAnchor,
  kOffsetX,
  kOffsetY,
  kCompare,
  kBookFirstPage,
  kTextSelectFree,
  kCount,
};

static_assert(kCardAlbumRowBase + static_cast<int>(CardAlbumRow::kCount) <= kDescriptionRowBase,
              "la plage des descriptions chevauche celle de l'album de cartes");

[[nodiscard]] constexpr int DescriptionRowId(DescriptionRow row) {
  return kDescriptionRowBase + static_cast<int>(row);
}

class DescriptionOptions {
 public:
  // ⚠ BORNÉ : le fichier s'édite à la main, et une ancre hors table ou un
  // décalage de dix mille pixels ne doivent pas atteindre la fenêtre.
  [[nodiscard]] ro::ui::DescriptionPlacement placement() const noexcept;
  // Comparer une pièce à celle qu'on porte au même emplacement. ALLUMÉ par
  // défaut.
  [[nodiscard]] bool compare() const noexcept { return compare_; }
  // La case « Comparer » de la fenêtre, qui est ce même réglage.
  void SetCompare(bool on, SettingsFile* file);
  // Un livre s'ouvre toujours à sa première page. ALLUMÉ par défaut ; éteint,
  // il reprend à la page où on l'a fermé.
  [[nodiscard]] bool book_first_page() const noexcept { return book_first_page_; }
  // Le texte des descriptions se sélectionne au simple glisser. ÉTEINT par
  // défaut : il demande alors Maj ou Ctrl, et le glisser déplace la fenêtre.
  [[nodiscard]] ro::ui::RichTextSelect text_select() const noexcept {
    return text_select_free_ ? ro::ui::RichTextSelect::kFree
                             : ro::ui::RichTextSelect::kWithModifier;
  }

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Refait les lignes, à chaque image où le panneau est ouvert.
  void Compose();
  [[nodiscard]] std::span<const ro::ui::OptionRowView> rows() const noexcept { return rows_; }

  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

 private:
  bool at_cursor_ = true;
  int anchor_ = static_cast<int>(ro::ui::DescriptionAnchor::kBottomRight);
  int offset_x_ = ro::ui::kDescriptionCursorGapPixels;
  int offset_y_ = ro::ui::kDescriptionCursorGapPixels;
  bool compare_ = true;
  bool book_first_page_ = true;
  bool text_select_free_ = false;
  std::array<const char*, ro::ui::kDescriptionAnchorCount> anchor_names_{};
  std::vector<ro::ui::OptionRowView> rows_;
};

}  // namespace moonclient
