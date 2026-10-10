#include "options/setting_links.h"

#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/character_window.h"
#include "ro/ui/hotkey_window.h"
#include "ro/ui/party_friend_window.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::core::TrText;

// Ce qui sépare deux étages d'un chemin. ⚠ U+203A, dans les plages que l'atlas
// charge.
constexpr std::string_view kPathStep = " › ";
// Le séparateur de la balise, et les chevrons qui la délimitent : une clé qui
// en porterait couperait la balise chez qui la relit.
constexpr char kTagFieldSeparator = ':';
constexpr char kTagOpen = '<';
constexpr char kTagClose = '>';

// LE TITRE DE LA FENÊTRE d'un genre d'onglet, en français marqué ; nul pour une
// page du panneau.
[[nodiscard]] const char* WindowTitleOf(SettingDestinationKind kind) {
  switch (kind) {
    case SettingDestinationKind::kSheet:
      return ro::ui::kCharacterWindowTitle;
    // ⚠ LA FENÊTRE DES SUCCÈS TIRE SON TITRE DES TEXTES DU CLIENT : le nôtre est
    // celui de son repli.
    case SettingDestinationKind::kAchievements:
      return TrText("Succès");
    case SettingDestinationKind::kPartyFriend:
      return ro::ui::kPartyFriendWindowTitle;
    case SettingDestinationKind::kHotkeys:
      return ro::ui::kHotkeyWindowTitle;
    case SettingDestinationKind::kOptions:
      break;
  }
  return nullptr;
}

// LA FAMILLE D'UNE PAGE, telle que la navigation la titre : voir l'ordre de
// `OptionSection`, où `kDisplay` à `kCapture` forment « Core ».
[[nodiscard]] const char* FamilyOf(OptionSection section) {
  if (Rank(section) <= Rank(OptionSection::kCapture)) {
    return TrText("Core");
  }
  if (section == OptionSection::kCommands) {
    return TrText("Server");
  }
  return TrText("UI");
}

// LA PAGE ENTIÈRE qui porte un onglet, pour la nommer devant lui.
[[nodiscard]] const SettingDestination* PageOf(OptionSection section) {
  return SettingDestinationOf(SettingDestinationKind::kOptions, section, kNoSettingTab);
}

// Le libellé d'un étage, numéro compris, dans la langue que `word` choisit.
template <typename Word>
[[nodiscard]] std::string Leaf(const SettingDestination& destination, Word word) {
  std::string leaf(word(destination.label));
  if (destination.ordinal != 0) {
    leaf.push_back(' ');
    leaf += std::to_string(destination.ordinal);
  }
  return leaf;
}

// LE CHEMIN, chaque étage passé par `word` : la traduction, ou le français nu.
template <typename Word>
[[nodiscard]] std::string PathOf(const SettingDestination& destination, Word word) {
  std::string path;
  if (const char* const window = WindowTitleOf(destination.kind); window != nullptr) {
    path = word(window);
  } else if (destination.tab != kNoSettingTab) {
    if (const SettingDestination* const page = PageOf(destination.section);
        page != nullptr && page != &destination) {
      path = word(page->label);
    }
  }
  if (!path.empty()) {
    path += kPathStep;
  }
  path += Leaf(destination, word);
  return path;
}

[[nodiscard]] std::string Translated(const char* text) { return std::string(Tr(text)); }
[[nodiscard]] std::string Bare(const char* text) { return std::string(text); }

}  // namespace

const SettingDestination* FindSettingDestination(std::string_view key) {
  if (key.empty()) {
    return nullptr;
  }
  for (const SettingDestination& destination : SettingDestinations()) {
    if (destination.key == key) {
      return &destination;
    }
  }
  return nullptr;
}

const SettingDestination* SettingDestinationOf(SettingDestinationKind kind, OptionSection section,
                                               int tab) {
  for (const SettingDestination& destination : SettingDestinations()) {
    // ⚠ LA PAGE NE DÉPARTAGE QUE LES PAGES : un onglet de fenêtre porte `kCount`.
    if (destination.kind == kind && destination.tab == tab &&
        (kind != SettingDestinationKind::kOptions || destination.section == section)) {
      return &destination;
    }
  }
  return nullptr;
}

bool SettingDestinationReachable(const SettingDestination& destination, const SettingReach& reach) {
  switch (destination.kind) {
    case SettingDestinationKind::kSheet:
      if (destination.tab == static_cast<int>(ro::ui::CharacterTab::kHomunculus)) {
        return reach.homunculus;
      }
      if (destination.tab == static_cast<int>(ro::ui::CharacterTab::kMercenary)) {
        return reach.mercenary;
      }
      return true;
    case SettingDestinationKind::kAchievements:
      return reach.achievements;
    case SettingDestinationKind::kOptions:
    case SettingDestinationKind::kPartyFriend:
    case SettingDestinationKind::kHotkeys:
      return true;
  }
  return false;
}

std::string SettingDestinationPath(const SettingDestination& destination) {
  // ⚠ TRADUIT ICI, CHEZ CELUI QUI REGARDE : la table porte le français, et c'est
  // le catalogue du lecteur qui décide de la langue affichée.
  return PathOf(destination, Translated);
}

std::string SettingDestinationFallback(const SettingDestination& destination) {
  return PathOf(destination, Bare);
}

std::string SettingLinkLabel(const SettingDestination& destination) {
  std::string label = "[";
  label += WindowTitleOf(destination.kind) != nullptr ? Tr("Fenêtre") : Tr("Réglage");
  label += ": ";
  label += SettingDestinationPath(destination);
  label.push_back(']');
  return label;
}

std::string SettingDestinationTip(const SettingDestination& destination) {
  if (WindowTitleOf(destination.kind) != nullptr) {
    return SettingDestinationPath(destination);
  }
  std::string tip(Tr("Réglages"));
  tip += kPathStep;
  tip += Tr(FamilyOf(destination.section));
  tip += kPathStep;
  tip += SettingDestinationPath(destination);
  return tip;
}

std::string SettingDestinationLabel(std::string_view key, const SettingReach& reach) {
  const SettingDestination* const destination = FindSettingDestination(key);
  if (destination == nullptr || !SettingDestinationReachable(*destination, reach)) {
    return {};
  }
  return SettingLinkLabel(*destination);
}

bool SettingKeyTravels(std::string_view key) {
  return !key.empty() && key.find(kTagFieldSeparator) == std::string_view::npos &&
         key.find(kTagOpen) == std::string_view::npos &&
         key.find(kTagClose) == std::string_view::npos;
}

}  // namespace moonclient
