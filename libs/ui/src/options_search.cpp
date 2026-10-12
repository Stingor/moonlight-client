// LE FILTRE ET L'ORDRE DU PANNEAU DE RÉGLAGES. Voir l'en-tête.

#include "ro/ui/options_search.h"

#include <algorithm>

#include "ro/core/search_key.h"

namespace ro::ui {
namespace {

// Ce qui suit, dans un libellé, l'identité qu'ImGui lit et que l'écran tait.
constexpr std::string_view kImGuiIdMark = "##";

// Une rangée qui RÈGLE quelque chose : une note informe, un cadre montre.
[[nodiscard]] bool IsSetting(const OptionRowView& row) {
  return row.kind != OptionKind::kNote && row.kind != OptionKind::kCanvas;
}

[[nodiscard]] bool RowFound(const OptionRowView& row, const OptionsSearch& search) {
  return IsSetting(row) &&
         (search.Finds(row.label) || search.Finds(row.command) || search.Finds(row.object));
}

}  // namespace

OptionsSearch::OptionsSearch(std::string_view typed) : needle_(core::SearchKey(typed)) {
  // ⚠ DES ESPACES SEULES NE CHERCHENT RIEN : elles videraient la liste d'un
  // coup de pouce sur la barre.
  if (std::ranges::all_of(needle_, [](char letter) { return letter == ' '; })) {
    needle_.clear();
  }
}

bool OptionsSearch::Finds(const char* text) const {
  if (text == nullptr) {
    return false;
  }
  std::string_view shown = text;
  if (const std::size_t mark = shown.find(kImGuiIdMark); mark != std::string_view::npos) {
    shown = shown.substr(0, mark);
  }
  return core::SearchKeyContains(core::SearchKey(shown), needle_);
}

std::vector<bool> KeptRows(std::span<const OptionRowView> rows, const OptionsSearch& search,
                           bool whole) {
  std::vector<bool> kept(rows.size(), whole || !search.active());
  if (whole || !search.active()) {
    return kept;
  }
  // UN TITRE TROUVÉ coiffe tout ce qui le suit jusqu'au titre suivant de même
  // rang : un bloc jusqu'au bloc suivant, un intertitre jusqu'au suivant.
  bool fold_found = false;
  bool heading_found = false;
  for (std::size_t rank = 0; rank < rows.size(); ++rank) {
    const OptionRowView& row = rows[rank];
    if (row.fold != nullptr) {
      fold_found = search.Finds(row.fold);
      heading_found = false;
    }
    if (row.heading != nullptr) {
      heading_found = search.Finds(row.heading);
    }
    kept[rank] = fold_found || heading_found || RowFound(row, search);
  }
  // LA MAÎTRESSE d'une rangée décalée : la plus proche au-dessus qui ne l'est pas.
  for (std::size_t rank = rows.size(); rank-- > 0;) {
    if (!kept[rank] || !rows[rank].indented) {
      continue;
    }
    for (std::size_t above = rank; above-- > 0;) {
      if (!rows[above].indented) {
        kept[above] = true;
        break;
      }
    }
  }
  // LES SŒURS D'UNE MÊME LIGNE se montrent ensemble, ou pas du tout.
  std::size_t first = 0;
  while (first < rows.size()) {
    std::size_t end = first + 1;
    while (end < rows.size() && rows[end].same_line) {
      ++end;
    }
    const bool any = std::any_of(kept.begin() + static_cast<std::ptrdiff_t>(first),
                                 kept.begin() + static_cast<std::ptrdiff_t>(end),
                                 [](bool shown) { return shown; });
    std::fill(kept.begin() + static_cast<std::ptrdiff_t>(first),
              kept.begin() + static_cast<std::ptrdiff_t>(end), any);
    first = end;
  }
  return kept;
}

int CountKeptSettings(std::span<const OptionRowView> rows, const std::vector<bool>& kept) {
  int counted = 0;
  for (std::size_t rank = 0; rank < rows.size() && rank < kept.size(); ++rank) {
    if (kept[rank] && IsSetting(rows[rank])) {
      ++counted;
    }
  }
  return counted;
}

SectionFinding FindInSection(const OptionSectionView& section, const OptionsSearch& search) {
  SectionFinding finding;
  finding.whole =
      !search.active() || search.Finds(section.title) ||
      std::ranges::any_of(section.tabs, [&search](const char* tab) { return search.Finds(tab); });
  finding.settings =
      CountKeptSettings(section.shared_rows, KeptRows(section.shared_rows, search, finding.whole)) +
      CountKeptSettings(section.rows, KeptRows(section.rows, search, finding.whole));
  finding.shown = finding.whole || finding.settings > 0;
  return finding;
}

std::vector<NavEntry> SortedNavEntries(std::span<const OptionSectionView> sections,
                                       const std::vector<bool>& shown) {
  struct Ranked {
    int family = 0;
    std::string key;
    int section = 0;
  };
  std::vector<const char*> family_titles;
  std::vector<Ranked> ranked;
  int family = -1;
  for (std::size_t rank = 0; rank < sections.size(); ++rank) {
    const OptionSectionView& section = sections[rank];
    if (section.group != nullptr || family < 0) {
      family_titles.push_back(section.group);
      ++family;
    }
    if (rank < shown.size() && !shown[rank]) {
      continue;
    }
    ranked.push_back(Ranked{.family = family,
                            .key = core::SearchKey(section.title),
                            .section = static_cast<int>(rank)});
  }
  // ⚠ STABLE : deux titres de même clé gardent l'ordre reçu, d'une image à
  // l'autre — sinon ils permuteraient sous la souris.
  std::ranges::stable_sort(ranked, [](const Ranked& left, const Ranked& right) {
    if (left.family != right.family) {
      return left.family < right.family;
    }
    return left.key < right.key;
  });
  std::vector<NavEntry> entries;
  entries.reserve(ranked.size());
  int titled = -1;
  for (const Ranked& entry : ranked) {
    const char* const group =
        entry.family != titled ? family_titles[static_cast<std::size_t>(entry.family)] : nullptr;
    titled = entry.family;
    entries.push_back(NavEntry{.label = sections[static_cast<std::size_t>(entry.section)].title,
                               .group = group,
                               .target = entry.section});
  }
  return entries;
}

}  // namespace ro::ui
