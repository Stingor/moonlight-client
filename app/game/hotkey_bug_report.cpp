#include "game/hotkey_bug_report.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <format>
#include <string>
#include <string_view>

#include "ro/core/ascii.h"
#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::ui::HotkeyRowView;
// LE PLAFOND DES CHAMPS, en octets : la liste des lignes s'arrête avant lui. Avec
// la révision du client, le contexte reste loin des 1024 octets que le serveur
// garde.
constexpr std::size_t kFieldsBytes = 760;

// Un tampon de la fenêtre, jusqu'à son zéro et jamais au-delà.
template <std::size_t kBytes>
[[nodiscard]] std::string_view Text(const char (&buffer)[kBytes]) {
  return {buffer, static_cast<std::size_t>(std::find(buffer, buffer + kBytes, '\0') - buffer)};
}

// Le nom d'une ligne dans le rapport : sa clé stable, sinon son libellé.
[[nodiscard]] std::string_view ActionName(const HotkeyBugScene& scene, std::size_t rank) {
  if (rank < scene.action_keys.size()) {
    return scene.action_keys[rank];
  }
  return scene.view.rows[rank].action;
}

// Le titre de l'onglet montré, ou vide pour « Tout ».
[[nodiscard]] std::string_view TabTitle(const HotkeyBugScene& scene) {
  const int tab = scene.window->tab;
  if (tab < 0 || static_cast<std::size_t>(tab) >= scene.view.tabs.size()) {
    return {};
  }
  return scene.view.tabs[static_cast<std::size_t>(tab)];
}

}  // namespace

BugReportContext HotkeyBugReport(const HotkeyBugScene& scene) {
  const ro::ui::HotkeyWindowState& window = *scene.window;
  const std::string_view filter = Text(window.filter);
  const std::string_view robbed = Text(window.robbed);
  const std::string_view tab_title = TabTitle(scene);

  std::string fields =
      std::format(R"("hotkeys":{{"tab":{},"tab_title":"{}","filter":"{}","rows":{})", window.tab,
                  JsonEscaped(tab_title.empty() ? std::string_view("all") : tab_title),
                  JsonEscaped(filter), scene.view.rows.size());
  if (window.capturing >= 0 &&
      static_cast<std::size_t>(window.capturing) < scene.view.rows.size()) {
    const auto waited = static_cast<std::size_t>(window.capturing);
    fields += std::format(R"(,"capturing":{{"action":"{}","key":"{}"}})",
                          JsonEscaped(ActionName(scene, waited)),
                          JsonEscaped(scene.view.rows[waited].key));
  } else {
    fields += R"(,"capturing":null)";
  }
  if (!robbed.empty()) {
    fields += std::format(R"(,"robbed":"{}")", JsonEscaped(robbed));
  }

  // LES LIGNES MONTRÉES, filtrées comme la fenêtre les filtre, tant qu'elles
  // tiennent sous le plafond.
  std::string shown;
  std::size_t shown_total = 0;
  bool cut = false;
  for (std::size_t rank = 0; rank < scene.view.rows.size(); ++rank) {
    const HotkeyRowView& row = scene.view.rows[rank];
    if (row.hidden || (window.tab != ro::ui::kAllHotkeyTabs && row.tab != window.tab)) {
      continue;
    }
    if (!ro::core::AsciiContainsFolded(row.action, filter) &&
        !ro::core::AsciiContainsFolded(row.key, filter)) {
      continue;
    }
    ++shown_total;
    if (cut) {
      continue;
    }
    const std::string entry =
        std::format(R"({}["{}","{}"])", shown.empty() ? "" : ",",
                    JsonEscaped(ActionName(scene, rank)), JsonEscaped(row.key));
    if (fields.size() + shown.size() + entry.size() > kFieldsBytes) {
      cut = true;
      continue;
    }
    shown += entry;
  }
  fields += std::format(R"(,"shown":[{}],"shown_total":{},"shown_cut":{}}})", shown, shown_total,
                        JsonFlag(cut));

  std::array<char, kBugReportLabelBytes> label{};
  if (window.capturing >= 0 &&
      static_cast<std::size_t>(window.capturing) < scene.view.rows.size()) {
    const std::string action(scene.view.rows[static_cast<std::size_t>(window.capturing)].action);
    std::snprintf(label.data(), label.size(), ro::core::Tr("Raccourcis : capture de « %s »"),
                  action.c_str());
  } else if (!tab_title.empty()) {
    std::snprintf(label.data(), label.size(), ro::core::Tr("Raccourcis : onglet %s"),
                  std::string(tab_title).c_str());
  } else {
    std::snprintf(label.data(), label.size(), "%s", ro::core::Tr("Raccourcis : tous les onglets"));
  }
  return FieldsBugReport(label.data(), fields);
}

}  // namespace moonclient
