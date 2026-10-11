#include "game/setting_destination_open.h"

#include <algorithm>
#include <optional>

#include "ro/ui/achievement_window.h"
#include "ro/ui/character_window.h"
#include "ro/ui/party_friend_window.h"

namespace moonclient {

void OpenSettingDestination(const SettingDestination& destination, UiStage::Windows& windows) {
  switch (destination.kind) {
    case SettingDestinationKind::kOptions: {
      ro::ui::OptionsWindowState& options = windows.options;
      options.open = true;
      options.section = static_cast<int>(Rank(destination.section));
      // ⚠ L'ONGLET SE DEMANDE, ET UNE PAGE ENTIÈRE REPART DU PREMIER — comme un
      // clic dans la navigation : c'est ImGui qui tient l'onglet ouvert, et il
      // garderait sinon celui qu'on avait laissé sur cette page.
      const int tab = std::max(destination.tab, 0);
      options.section_tab = tab;
      options.section_tab_wanted = tab;
      return;
    }
    case SettingDestinationKind::kSheet: {
      // ⚠ LES DEUX, ET PAS SEULEMENT LA DEMANDE : `tab` décide des crans de
      // largeur AVANT la barre, la demande la commande PENDANT.
      const auto tab = static_cast<ro::ui::CharacterTab>(destination.tab);
      windows.character.open = true;
      windows.character.tab = tab;
      windows.character.tab_request = tab;
      return;
    }
    case SettingDestinationKind::kAchievements: {
      // ⚠ UN ONGLET CHOISI QUITTE LA RECHERCHE, comme un clic dans l'arbre, et la
      // sélection revient sur sa première rangée : c'est le changement d'onglet
      // que l'étage lit à l'image suivante.
      ro::ui::OpenAchievementTab(
          windows.achievements.window,
          ro::ui::AchievementTabHit{.tab = destination.tab,
                                    .subtab = destination.subtab == kNoSettingTab
                                                  ? ro::ui::kAchievementWholeTab
                                                  : destination.subtab});
      windows.achievements.news_lit = false;
      windows.achievements.answer.tab_changed = true;
      return;
    }
    case SettingDestinationKind::kPartyFriend:
      windows.party.window.open = true;
      windows.party.window.tab = static_cast<ro::ui::PartyFriendTab>(destination.tab);
      return;
    case SettingDestinationKind::kHotkeys:
      // ⚠ UNE CAPTURE NE SURVIT PAS AU CHANGEMENT D'ONGLET : sa ligne peut n'être
      // plus à l'écran, et la prochaine touche lui irait sans qu'on le voie.
      windows.hotkeys.capturing = -1;
      windows.hotkeys.open = true;
      windows.hotkeys.tab = destination.tab;
      windows.hotkeys.tab_request = destination.tab;
      return;
  }
}

const SettingDestination* AskedSettingLink(const UiStage::Answers& answers,
                                           const UiStage::Windows& windows) {
  using Kind = SettingDestinationKind;
  if (const ro::ui::OptionsWindowAction& options = answers.options;
      options.link_section >= 0 &&
      options.link_section < static_cast<int>(Rank(OptionSection::kCount))) {
    const auto section = static_cast<OptionSection>(options.link_section);
    // ⚠ UN ONGLET SANS CLÉ RETOMBE SUR SA PAGE : mieux vaut un lien un cran trop
    // large que pas de lien du tout.
    const SettingDestination* const tab =
        SettingDestinationOf(Kind::kOptions, section, options.link_tab);
    return tab != nullptr ? tab : SettingDestinationOf(Kind::kOptions, section, kNoSettingTab);
  }
  if (answers.character.link_tab.has_value()) {
    return SettingDestinationOf(Kind::kSheet, OptionSection::kCount,
                                static_cast<int>(*answers.character.link_tab));
  }
  if (const std::optional<ro::ui::AchievementTabHit>& place = windows.achievements.answer.link_tab;
      place.has_value()) {
    // ⚠ UN SOUS-ONGLET SANS CLÉ RETOMBE SUR SON ONGLET, comme un onglet de page
    // sur sa page.
    const SettingDestination* const subtab =
        place->subtab == ro::ui::kAchievementWholeTab
            ? nullptr
            : SettingDestinationOf(Kind::kAchievements, OptionSection::kCount, place->tab,
                                   place->subtab);
    return subtab != nullptr
               ? subtab
               : SettingDestinationOf(Kind::kAchievements, OptionSection::kCount, place->tab);
  }
  if (windows.party.answer.link_tab.has_value()) {
    return SettingDestinationOf(Kind::kPartyFriend, OptionSection::kCount,
                                static_cast<int>(*windows.party.answer.link_tab));
  }
  if (answers.hotkeys.link_tab.has_value()) {
    return SettingDestinationOf(Kind::kHotkeys, OptionSection::kCount, *answers.hotkeys.link_tab);
  }
  return nullptr;
}

SettingReach SettingReachOf(const GamePieces& pieces) {
  return SettingReach{.homunculus = pieces.homunculus.view().present,
                      .mercenary = pieces.mercenary.view().present,
                      .achievements = pieces.achievement_catalog.has_value()};
}

}  // namespace moonclient
