#include "game/options_bug_report.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <format>
#include <string>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::ui::OptionKind;
using ro::ui::OptionRowView;
using ro::ui::OptionSectionView;

// Le libellé d'un rapport — « Options : Gameplay, onglet Barre 3 » —, jamais
// plus long.
constexpr std::size_t kLabelBytes = 192;
// LE PLAFOND DE LA LISTE DES VALEURS, en octets : avec la page, l'onglet et la
// révision du client, le contexte reste loin des 1024 octets que le serveur
// garde.
constexpr std::size_t kValuesBytes = 480;

// La valeur d'une rangée en JSON, ou vide pour une rangée qui n'en a pas.
//
// ⚠ UN CHAMP TAPÉ NE PART PAS : il peut porter un chemin du disque du joueur,
// et son contenu ne dit rien qu'un autre réglage ne dise.
[[nodiscard]] std::string ValueOf(const OptionRowView& row) {
  switch (row.kind) {
    case OptionKind::kToggle:
      return row.on ? "1" : "0";
    case OptionKind::kSlider:
      return std::isfinite(row.value) ? std::format("{:g}", row.value) : std::string("null");
    case OptionKind::kChoice:
      return std::to_string(row.chosen);
    case OptionKind::kColor:
      return std::format(R"("{:08X}")", static_cast<unsigned>(row.color));
    case OptionKind::kAction:
    case OptionKind::kNote:
    case OptionKind::kCanvas:
    case OptionKind::kText:
    case OptionKind::kPanel:
      return {};
  }
  return {};
}

// Ajoute `[id, valeur]` pour chaque rangée qui a une valeur, tant que la liste
// tient sous son plafond. Faux quand une valeur n'a pas tenu.
[[nodiscard]] bool AppendValues(std::span<const OptionRowView> rows, std::string& values) {
  for (const OptionRowView& row : rows) {
    if (row.id < 0) {
      continue;
    }
    const std::string value = ValueOf(row);
    if (value.empty()) {
      continue;
    }
    const std::string entry = std::format("{}[{},{}]", values.empty() ? "" : ",", row.id, value);
    if (values.size() + entry.size() > kValuesBytes) {
      return false;
    }
    values += entry;
  }
  return true;
}

[[nodiscard]] const char* Shown(const char* text) {
  return text != nullptr && text[0] != '\0' ? text : "?";
}

}  // namespace

BugReportContext OptionsBugReport(const OptionsBugScene& scene) {
  const ro::ui::OptionsWindowState& window = *scene.window;
  const bool known =
      window.section >= 0 && static_cast<std::size_t>(window.section) < scene.sections.size();
  std::string fields = std::format(R"("options":{{"section":{})", window.section);
  if (!known) {
    fields += "}";
    std::array<char, kLabelBytes> label{};
    std::snprintf(label.data(), label.size(), ro::core::Tr("Options : page %d"), window.section);
    return FieldsBugReport(label.data(), fields);
  }

  const OptionSectionView& section = scene.sections[static_cast<std::size_t>(window.section)];
  fields += std::format(R"(,"section_title":"{}")", JsonEscaped(Shown(section.title)));
  const char* tab_title = nullptr;
  if (!section.tabs.empty()) {
    if (window.section_tab >= 0 &&
        static_cast<std::size_t>(window.section_tab) < section.tabs.size()) {
      tab_title = section.tabs[static_cast<std::size_t>(window.section_tab)];
    }
    fields += std::format(R"(,"tab":{},"tab_title":"{}","tabs":{})", window.section_tab,
                          JsonEscaped(Shown(tab_title)), section.tabs.size());
  }

  // LES COMMUNS AUX ONGLETS EN TÊTE, comme la page les montre ; puis l'onglet,
  // puis le volet.
  std::string values;
  const bool whole = AppendValues(section.pinned_rows, values) &&
                     AppendValues(section.shared_rows, values) &&
                     AppendValues(section.rows, values) && AppendValues(section.side_rows, values);
  fields += std::format(R"(,"values":[{}],"values_cut":{})", values, whole ? "false" : "true");
  if (window.revert_seconds >= 0) {
    fields += std::format(R"(,"revert_seconds":{})", window.revert_seconds);
  }
  fields += '}';

  std::array<char, kLabelBytes> label{};
  if (tab_title != nullptr) {
    std::snprintf(label.data(), label.size(), ro::core::Tr("Options : %s, onglet %s"),
                  Shown(section.title), Shown(tab_title));
  } else {
    std::snprintf(label.data(), label.size(), ro::core::Tr("Options : %s"), Shown(section.title));
  }
  return FieldsBugReport(label.data(), fields);
}

}  // namespace moonclient
