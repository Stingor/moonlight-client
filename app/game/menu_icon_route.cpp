#include "game/menu_icon_route.h"

#include <optional>

#include "ro/render/camera.h"
#include "ro/render/sprite_catalog.h"
#include "ro/sim/world.h"

#include "game/battleground_stage.h"
#include "input/key_action.h"

namespace moonclient {

using ro::ui::MenuCommand;

MenuIconFacts MenuIconFactsOf(const ro::sim::World& world, const UiStage::Windows& windows,
                              std::string_view cash_shop_name) {
  const ro::sim::Actor* self = world.player();
  MenuIconFacts facts{
      .doram = self != nullptr && ro::render::IsDoramJob(self->look.job),
      .map_name = world.map_name(),
      .mvp_tracker_enabled = windows.mvp_tracker.enabled,
      .status_points_waiting = ro::sim::StatusPointsSpendable(world.player_status()),
      .skill_points_waiting = ro::sim::SkillPointsSpendable(world.player_status()),
      .battleground_queued = windows.battleground.queued,
      .battleground_queued_name = windows.battleground.queued_name,
      .cash_shop_name = cash_shop_name};
  const auto open_when = [&facts](MenuCommand command, bool open) {
    if (open) {
      facts.open.push_back(command);
    }
  };
  open_when(MenuCommand::kCraftAtlas, windows.craft_atlas.state.open);
  open_when(MenuCommand::kNavigation, windows.navigation.state.open);
  open_when(MenuCommand::kWorldMap, windows.navigation.world_map.open);
  open_when(MenuCommand::kMvpTracker, windows.mvp_tracker.window.open);
  open_when(MenuCommand::kCardAlbum, windows.card_album.open);
  open_when(MenuCommand::kPetWindow, windows.pet.open);
  open_when(MenuCommand::kRankingBoard, windows.ranking_board.window.open);
  open_when(MenuCommand::kCashShop, windows.cash_shop.state.open);
  open_when(MenuCommand::kMail, windows.mail.box.open);
  open_when(MenuCommand::kAchievement, windows.achievements.window.open);
  open_when(MenuCommand::kQuest, windows.quests.window.open);
  open_when(MenuCommand::kParty, windows.party.window.open);
  open_when(MenuCommand::kBattleground,
            windows.battleground.window.open || windows.battleground.window.standby_open);
  const auto lit_when = [&facts](MenuCommand command, bool lit) {
    if (lit) {
      facts.lit.push_back(command);
    }
  };
  // LE COURRIER S'ALLUME : la pastille du serveur, ou un non-lu de la liste.
  lit_when(MenuCommand::kMail, windows.mail.news_lit);
  // LES SUCCÈS S'ALLUMENT à l'achèvement d'un succès, jusqu'à la réouverture.
  lit_when(MenuCommand::kAchievement, windows.achievements.news_lit);
  // LES QUÊTES S'ALLUMENT à l'arrivée d'une quête, jusqu'à la réouverture.
  lit_when(MenuCommand::kQuest, windows.quests.news_lit);
  // LE CARNET S'ALLUME quand un favori entre dans sa fenêtre.
  lit_when(MenuCommand::kMvpTracker, windows.mvp_tracker.badge_lit);
  return facts;
}

void ApplyMenuCommand(MenuCommand command, const MenuIconTargets& to) {
  if (to.windows == nullptr) {
    return;
  }
  if (const std::optional<GameAction> action = ActionOfMenuCommand(command); action.has_value()) {
    const KeyPress press{.sheet = &to.windows->character,
                         .pet = &to.windows->pet,
                         .inventory = &to.windows->inventory,
                         .party = &to.windows->party.window,
                         .bank = to.bank,
                         .card_album = to.card_album,
                         .card_album_window = &to.windows->card_album,
                         .cash_shop = to.cash_shop,
                         .mail = to.mail,
                         .achievements = to.achievements,
                         .quests = to.quests,
                         .craft_atlas = &to.windows->craft_atlas.state,
                         .navigation = &to.windows->navigation.state,
                         .world_map = &to.windows->navigation.world_map,
                         .mvp_tracker = to.mvp_tracker,
                         .link = to.link,
                         .world = to.world,
                         .now_ms = to.now_ms};
    // ⚠ LA CAMÉRA N'EST LUE PAR AUCUNE DE CES ACTIONS : elle ne sert qu'à
    // recadrer la carte entière, qui n'a pas d'icône.
    static_cast<void>(TriggerAction(press, *action, /*repeat=*/false, ro::render::Camera{}));
    return;
  }
  // LES FENÊTRES SANS ACTION DE RACCOURCI : une bascule, comme au clavier.
  switch (command) {
    case MenuCommand::kOption:
      to.windows->options.open = !to.windows->options.open;
      break;
    case MenuCommand::kShortcut:
      to.windows->hotkeys.open = !to.windows->hotkeys.open;
      break;
    case MenuCommand::kBattleground:
      if (to.battleground != nullptr) {
        to.battleground->RequestOpen(to.windows->battleground);
      }
      break;
    case MenuCommand::kRankingBoard:
      ToggleRankingBoard(to.windows->ranking_board);
      break;
    default:
      // ⚠ UN CAS PAR DÉFAUT, à rebours du dépôt : les icônes sans fenêtre ne
      // remontent jamais jusqu'ici — la grille ouvre pour elles la fenêtre
      // d'attente. Ce `switch` filtre les quatre qui restent.
      break;
  }
}

}  // namespace moonclient
