// La grille d'icônes de menu. Voir l'en-tête.

#include "game/menu_icon_compose.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"

namespace moonclient {
namespace {

using ro::ui::MenuCommand;

// LA CARTE OÙ LE JEU NE POSE PAS LE BOUTON DU VOTE SHOP, quel que soit
// `/cashshop` : celle de son évènement.
constexpr std::string_view kMapWithoutCashShopButton = "new_event";

[[nodiscard]] ro::ui::SkinImage ImageAt(ro::render::GameImages* images, const std::string& path) {
  if (images == nullptr) {
    return {};
  }
  // ⚠ `GetOurs` ET NON `Get` : le chemin est écrit dans NOS sources, en UTF-8,
  // et le dossier d'interface est en coréen.
  const ro::render::IconTexture& icon = images->GetOurs(path);
  if (!icon.usable()) {
    return {};
  }
  return ro::ui::SkinImage{.id = static_cast<ImTextureID>(icon.texture.name()),
                           .width = static_cast<float>(icon.width_px),
                           .height = static_cast<float>(icon.height_px)};
}

[[nodiscard]] ro::ui::SkinImage ImageOf(ro::render::GameImages* images, MenuCommand command,
                                        bool doram, ro::ui::MenuIconArt art) {
  return ImageAt(images, ro::ui::MenuIconPath(ro::ui::MenuIconStem(command, doram), art));
}

[[nodiscard]] bool Holds(const std::vector<MenuCommand>& commands, MenuCommand command) {
  return std::ranges::find(commands, command) != commands.end();
}

}  // namespace

std::optional<GameAction> ActionOfMenuCommand(MenuCommand command) {
  switch (command) {
    case MenuCommand::kStatus:
      return GameAction::kOpenStatus;
    case MenuCommand::kEquipment:
      return GameAction::kOpenCharacterEquipment;
    case MenuCommand::kItem:
      return GameAction::kOpenInventory;
    case MenuCommand::kSkill:
      return GameAction::kOpenCharacterGrimoire;
    case MenuCommand::kGuild:
      return GameAction::kOpenCharacterGuild;
    case MenuCommand::kBank:
      return GameAction::kOpenBank;
    case MenuCommand::kParty:
      return GameAction::kOpenParty;
    case MenuCommand::kOption:
    case MenuCommand::kShortcut:
    case MenuCommand::kBooking:
    case MenuCommand::kBattleground:
    case MenuCommand::kReplay:
    case MenuCommand::kTip:
    case MenuCommand::kSocialNetwork:
    case MenuCommand::kAttendance:
    case MenuCommand::kAdventurerAgency:
    case MenuCommand::kReputation:
    case MenuCommand::kAdventureGuide:
    case MenuCommand::kProbability:
    case MenuCommand::kRankingBoard:
    case MenuCommand::kMarket:
    case MenuCommand::kDatabase:
      break;
    case MenuCommand::kCardAlbum:
      return GameAction::kOpenCardAlbum;
    case MenuCommand::kCraftAtlas:
      return GameAction::kOpenCraftAtlas;
    case MenuCommand::kMvpTracker:
      return GameAction::kOpenMvpTracker;
    case MenuCommand::kPetWindow:
      return GameAction::kOpenPetWindow;
    case MenuCommand::kNavigation:
      return GameAction::kOpenNavigation;
    case MenuCommand::kWorldMap:
      return GameAction::kOpenWorldMap;
    case MenuCommand::kCashShop:
    case MenuCommand::kCashShopButton:
      return GameAction::kOpenCashShop;
    case MenuCommand::kMail:
      return GameAction::kOpenMail;
    case MenuCommand::kAchievement:
      return GameAction::kOpenAchievements;
    case MenuCommand::kQuest:
      return GameAction::kOpenQuests;
  }
  return std::nullopt;
}

bool MenuCommandPorted(MenuCommand command) {
  // ⚠ LES OPTIONS ET LES RACCOURCIS N'ONT PAS D'ACTION DE RACCOURCI, mais bien
  // une fenêtre : le menu d'Échap les ouvre déjà.
  // LE CHAMP DE BATAILLE NON PLUS : son icône rouvre la fenêtre de la file, ou
  // la liste, selon qu'on est en file.
  // LE TABLEAU DES CLASSEMENTS NON PLUS : il n'a pas de touche, `/ranking` et
  // son icône l'ouvrent.
  // LA RECHERCHE DE GROUPE NON PLUS : son icône, et le bouton de la fenêtre de
  // groupe.
  // LE MARCHÉ NON PLUS : sa touche passe par son étage, que l'icône n'atteint
  // pas ; elle dépose sa bascule dans ses fenêtres.
  return ActionOfMenuCommand(command).has_value() || command == MenuCommand::kOption ||
         command == MenuCommand::kShortcut || command == MenuCommand::kBattleground ||
         command == MenuCommand::kRankingBoard || command == MenuCommand::kBooking ||
         command == MenuCommand::kMarket || command == MenuCommand::kDatabase;
}

void ComposeMenuIcons(std::vector<ro::ui::MenuIconView>& into, ro::render::GameImages* images,
                      const Hotkeys& bindings, const MenuIconFacts& facts) {
  into.clear();
  for (const ro::ui::MenuIconEntry& entry : ro::ui::MenuIconCatalogue()) {
    if (!entry.shown) {
      continue;
    }
    if (entry.command == MenuCommand::kCashShopButton &&
        facts.map_name == kMapWithoutCashShopButton) {
      continue;
    }
    // LE CARNET ÉTEINT N'A PAS D'ICÔNE : seule sa case, dans les options, le
    // rallume.
    if (entry.command == MenuCommand::kMvpTracker && !facts.mvp_tracker_enabled) {
      continue;
    }
    // ⚠ LE BOUTON HORS GRILLE N'A QU'UN BITMAP, qui sert aussi d'enfoncé : ses
    // états n'existent pas sous `menu_icon\`, et les y chercher ne donnerait
    // qu'une absence.
    ro::ui::MenuIconView icon{
        .command = entry.command,
        .normal = entry.off_grid
                      ? ImageAt(images, ro::ui::MenuButtonPath(entry.stem))
                      : ImageOf(images, entry.command, facts.doram, ro::ui::MenuIconArt::kNormal),
        .pressed = entry.off_grid
                       ? ro::ui::SkinImage{}
                       : ImageOf(images, entry.command, facts.doram, ro::ui::MenuIconArt::kPressed),
        .ported = MenuCommandPorted(entry.command),
        .news_lit = Holds(facts.lit, entry.command),
        // Les bascules de ce client restent enfoncées fenêtre ouverte.
        .shows_open = Holds(facts.open, entry.command)};
    // LE CHAMP DE BATAILLE PORTE UNE PASTILLE TANT QU'ON EST EN FILE, et son
    // infobulle nomme le terrain.
    if (entry.command == MenuCommand::kBattleground && facts.battleground_queued) {
      icon.dot_lit = true;
      icon.label = ro::core::Printed(ro::core::Tr("En file : %s"),
                                     std::string(facts.battleground_queued_name).c_str());
    }
    // DES POINTS À PLACER : la même pastille, sans infobulle à elle.
    if ((entry.command == MenuCommand::kStatus && facts.status_points_waiting) ||
        (entry.command == MenuCommand::kSkill && facts.skill_points_waiting)) {
      icon.dot_lit = true;
    }
    if (entry.command == MenuCommand::kCashShop || entry.command == MenuCommand::kCashShopButton) {
      icon.label = std::string(facts.cash_shop_name);
    }
    // ⚠ LE « NOUVEAU » N'EST DEMANDÉ QU'OÙ LE JEU EN DESSINE UN : ailleurs le
    // fichier n'existe pas, et la demande ne produirait qu'une absence.
    if (entry.has_news_art) {
      icon.news = ImageOf(images, entry.command, facts.doram, ro::ui::MenuIconArt::kNews);
      icon.news_pressed =
          ImageOf(images, entry.command, facts.doram, ro::ui::MenuIconArt::kNewsPressed);
    }
    if (const std::optional<GameAction> action = ActionOfMenuCommand(entry.command);
        action.has_value()) {
      icon.shortcut = LabelOfBinding(bindings.Of(*action));
    }
    into.push_back(std::move(icon));
  }
}

}  // namespace moonclient
