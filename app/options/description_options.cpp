#include "options/description_options.h"

#include <algorithm>
#include <cmath>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier. GRAVÉES : les changer rend à tous les joueurs les
// réglages d'origine, sans que rien ne le dise.
constexpr const char* kAtCursorKey = "description_near_mouse";
constexpr const char* kAnchorKey = "description_anchoring";
constexpr const char* kOffsetXKey = "description_offset_x";
constexpr const char* kOffsetYKey = "description_offset_y";
constexpr const char* kCompareKey = "description_compare";
constexpr const char* kBookFirstPageKey = "book_first_page";
constexpr const char* kTextSelectFreeKey = "description_text_select_free";

// Les bornes du décalage, de part et d'autre du curseur.
constexpr int kOffsetMostPixels = 400;

[[nodiscard]] int ClampedOffset(int pixels) {
  return std::clamp(pixels, -kOffsetMostPixels, kOffsetMostPixels);
}

[[nodiscard]] OptionRowView Slider(DescriptionRow row, const char* label, const char* help,
                                   int value, bool disabled) {
  return OptionRowView{.id = DescriptionRowId(row),
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kSlider,
                       .value = static_cast<float>(ClampedOffset(value)),
                       .lowest = static_cast<float>(-kOffsetMostPixels),
                       .highest = static_cast<float>(kOffsetMostPixels),
                       .format = "%.0f px",
                       .indented = true,
                       .disabled = disabled};
}

}  // namespace

ro::ui::DescriptionPlacement DescriptionOptions::placement() const noexcept {
  const int anchor = anchor_ >= 0 && anchor_ < static_cast<int>(ro::ui::kDescriptionAnchorCount)
                         ? anchor_
                         : static_cast<int>(ro::ui::DescriptionAnchor::kBottomRight);
  return ro::ui::DescriptionPlacement{.at_cursor = at_cursor_,
                                      .anchor = static_cast<ro::ui::DescriptionAnchor>(anchor),
                                      .offset_x = static_cast<float>(ClampedOffset(offset_x_)),
                                      .offset_y = static_cast<float>(ClampedOffset(offset_y_))};
}

void DescriptionOptions::SetCompare(bool on, SettingsFile* file) {
  compare_ = on;
  if (file != nullptr) {
    file->Touch();
  }
}

void DescriptionOptions::Declare(SettingsFile& into) {
  into.Declare(kAtCursorKey, &at_cursor_);
  into.Declare(kAnchorKey, &anchor_);
  into.Declare(kOffsetXKey, &offset_x_);
  into.Declare(kOffsetYKey, &offset_y_);
  into.Declare(kCompareKey, &compare_);
  into.Declare(kBookFirstPageKey, &book_first_page_);
  into.Declare(kTextSelectFreeKey, &text_select_free_);
}

void DescriptionOptions::Compose() {
  rows_.clear();
  // ⚠ DANS L'ORDRE DE `DescriptionAnchor` : le rang choisi EST l'ancre.
  anchor_names_ = {Tr("Haut-gauche"), Tr("Haut-droite"), Tr("Bas-gauche"), Tr("Bas-droite"),
                   Tr("Centre")};
  const bool off = !at_cursor_;
  rows_.push_back(
      OptionRowView{.id = DescriptionRowId(DescriptionRow::kAtCursor),
                    .label = Tr("Ouvrir près de la souris"),
                    .help = Tr("ON : la description apparaît près du curseur à chaque ouverture.\n"
                               "OFF : elle réapparaît à sa dernière position connue."),
                    .kind = OptionKind::kToggle,
                    .on = at_cursor_});
  rows_.push_back(OptionRowView{.id = DescriptionRowId(DescriptionRow::kAnchor),
                                .label = Tr("Ancrage"),
                                .help = Tr("Le côté du curseur où la fenêtre s'ouvre."),
                                .kind = OptionKind::kChoice,
                                .chosen = static_cast<int>(placement().anchor),
                                .choices = anchor_names_,
                                .indented = true,
                                .disabled = off});
  rows_.push_back(Slider(DescriptionRow::kOffsetX, Tr("Offset X"),
                         Tr("Décalage depuis le curseur (molette au survol pour ajuster)."),
                         offset_x_, off));
  rows_.push_back(Slider(DescriptionRow::kOffsetY, Tr("Offset Y"),
                         Tr("Décalage depuis le curseur (molette au survol pour ajuster)."),
                         offset_y_, off));
  rows_.push_back(OptionRowView{
      .id = DescriptionRowId(DescriptionRow::kCompare),
      .label = Tr("Comparer avec l'item porté"),
      .help = Tr("La description d'une pièce montre à côté celle de la pièce portée au même "
                 "emplacement, comme au client. La case « Comparer » de la fenêtre règle la "
                 "même chose."),
      .kind = OptionKind::kToggle,
      .on = compare_});
  rows_.push_back(OptionRowView{.id = DescriptionRowId(DescriptionRow::kBookFirstPage),
                                .label = Tr("Livres : ouvrir à la première page"),
                                .help = Tr("ON : un livre s'ouvre toujours page 1.\n"
                                           "OFF : il reprend à la page où on l'a refermé."),
                                .kind = OptionKind::kToggle,
                                .on = book_first_page_});
  rows_.push_back(OptionRowView{
      .id = DescriptionRowId(DescriptionRow::kTextSelectFree),
      .label = Tr("Texte sélectionnable sans touche"),
      .help = Tr("ON : glisser sur le texte d'une description le sélectionne.\n"
                 "OFF : la sélection demande Maj ou Ctrl tenu, et glisser sur le texte "
                 "déplace la fenêtre."),
      .kind = OptionKind::kToggle,
      .on = text_select_free_});
}

bool DescriptionOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kDescriptionRowBase ||
      asked.changed >= kDescriptionRowBase + static_cast<int>(DescriptionRow::kCount)) {
    return false;
  }
  switch (static_cast<DescriptionRow>(asked.changed - kDescriptionRowBase)) {
    case DescriptionRow::kAtCursor:
      at_cursor_ = asked.on;
      break;
    case DescriptionRow::kAnchor:
      anchor_ = std::clamp(asked.chosen, 0, static_cast<int>(ro::ui::kDescriptionAnchorCount) - 1);
      break;
    case DescriptionRow::kOffsetX:
      offset_x_ = ClampedOffset(static_cast<int>(std::lround(asked.value)));
      break;
    case DescriptionRow::kOffsetY:
      offset_y_ = ClampedOffset(static_cast<int>(std::lround(asked.value)));
      break;
    case DescriptionRow::kCompare:
      compare_ = asked.on;
      break;
    case DescriptionRow::kBookFirstPage:
      book_first_page_ = asked.on;
      break;
    case DescriptionRow::kTextSelectFree:
      text_select_free_ = asked.on;
      break;
    case DescriptionRow::kCount:
      break;
  }
  return true;
}

}  // namespace moonclient
