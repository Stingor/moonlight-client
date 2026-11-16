// LA GRILLE D'ICÔNES DE MENU : ce qu'elle montre selon ce que les fenêtres en
// disent — sans art, chaque icône garde sa place.

#include "game/menu_icon_compose.h"

#include <algorithm>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using moonclient::ComposeMenuIcons;
using moonclient::Hotkeys;
using moonclient::MenuIconFacts;
using ro::ui::MenuCommand;
using ro::ui::MenuIconView;

namespace {

std::vector<MenuIconView> Compose(const MenuIconFacts& facts) {
  std::vector<MenuIconView> icons;
  ComposeMenuIcons(icons, /*images=*/nullptr, Hotkeys(), facts);
  return icons;
}

const MenuIconView* Find(const std::vector<MenuIconView>& icons, MenuCommand command) {
  const auto found = std::ranges::find(icons, command, &MenuIconView::command);
  return found == icons.end() ? nullptr : &*found;
}

}  // namespace

TEST_CASE("Icônes de menu : le carnet MVP éteint n'a pas d'icône", "[app][menu]") {
  MenuIconFacts facts;
  CHECK(Find(Compose(facts), MenuCommand::kMvpTracker) == nullptr);
  facts.mvp_tracker_enabled = true;
  CHECK(Find(Compose(facts), MenuCommand::kMvpTracker) != nullptr);
}

TEST_CASE("Icônes de menu : pas de bouton du vote shop sur la carte de son évènement",
          "[app][menu]") {
  MenuIconFacts facts;
  facts.map_name = "prontera";
  CHECK(Find(Compose(facts), MenuCommand::kCashShopButton) != nullptr);
  facts.map_name = "new_event";
  CHECK(Find(Compose(facts), MenuCommand::kCashShopButton) == nullptr);
}

TEST_CASE("Icônes de menu : enfoncée fenêtre ouverte, allumée par du nouveau", "[app][menu]") {
  MenuIconFacts facts;
  facts.open = {MenuCommand::kQuest};
  facts.lit = {MenuCommand::kMail};
  const std::vector<MenuIconView> icons = Compose(facts);
  const MenuIconView* quest = Find(icons, MenuCommand::kQuest);
  const MenuIconView* mail = Find(icons, MenuCommand::kMail);
  REQUIRE(quest != nullptr);
  REQUIRE(mail != nullptr);
  CHECK(quest->shows_open);
  CHECK_FALSE(quest->news_lit);
  CHECK(mail->news_lit);
  CHECK_FALSE(mail->shows_open);
}

TEST_CASE("Icônes de menu : en file, le champ de bataille porte une pastille et son terrain",
          "[app][menu]") {
  MenuIconFacts facts;
  const MenuIconView* idle = Find(Compose(facts), MenuCommand::kBattleground);
  REQUIRE(idle != nullptr);
  CHECK_FALSE(idle->dot_lit);

  facts.battleground_queued = true;
  facts.battleground_queued_name = "Tierra";
  const std::vector<MenuIconView> icons = Compose(facts);
  const MenuIconView* queued = Find(icons, MenuCommand::kBattleground);
  REQUIRE(queued != nullptr);
  CHECK(queued->dot_lit);
  CHECK(queued->label.find("Tierra") != std::string::npos);
}

TEST_CASE("Icônes de menu : des points à placer allument le Statut et les Skills", "[app][menu]") {
  MenuIconFacts facts;
  {
    const std::vector<MenuIconView> icons = Compose(facts);
    REQUIRE(Find(icons, MenuCommand::kStatus) != nullptr);
    REQUIRE(Find(icons, MenuCommand::kSkill) != nullptr);
    CHECK_FALSE(Find(icons, MenuCommand::kStatus)->dot_lit);
    CHECK_FALSE(Find(icons, MenuCommand::kSkill)->dot_lit);
  }
  facts.status_points_waiting = true;
  {
    const std::vector<MenuIconView> icons = Compose(facts);
    CHECK(Find(icons, MenuCommand::kStatus)->dot_lit);
    CHECK_FALSE(Find(icons, MenuCommand::kSkill)->dot_lit);
  }
  facts.status_points_waiting = false;
  facts.skill_points_waiting = true;
  {
    const std::vector<MenuIconView> icons = Compose(facts);
    CHECK_FALSE(Find(icons, MenuCommand::kStatus)->dot_lit);
    CHECK(Find(icons, MenuCommand::kSkill)->dot_lit);
  }
}

TEST_CASE("Icônes de menu : la boutique porte le nom du serveur, et chaque icône sa touche",
          "[app][menu]") {
  MenuIconFacts facts;
  facts.cash_shop_name = "Moon Shop";
  const std::vector<MenuIconView> icons = Compose(facts);
  const MenuIconView* shop = Find(icons, MenuCommand::kCashShop);
  REQUIRE(shop != nullptr);
  CHECK(shop->label == "Moon Shop");
  // L'inventaire a une touche d'origine : son étiquette n'est pas vide.
  const MenuIconView* item = Find(icons, MenuCommand::kItem);
  REQUIRE(item != nullptr);
  CHECK_FALSE(item->shortcut.empty());
  // La carte du monde a sa fenêtre : son clic l'ouvre. Une commande sans
  // fenêtre dans ce client — le replay — ouvre la fenêtre d'attente. Les icônes
  // masquées ne viennent pas du tout.
  const MenuIconView* world_map = Find(icons, MenuCommand::kWorldMap);
  REQUIRE(world_map != nullptr);
  CHECK(world_map->ported);
  CHECK_FALSE(moonclient::MenuCommandPorted(MenuCommand::kReplay));
  CHECK(Find(icons, MenuCommand::kReplay) == nullptr);
}

TEST_CASE("Icônes de menu : la recherche de groupe a son icône, enfoncée fenêtre ouverte",
          "[app][menu]") {
  MenuIconFacts facts;
  const std::vector<MenuIconView> idle = Compose(facts);
  const MenuIconView* closed = Find(idle, MenuCommand::kBooking);
  REQUIRE(closed != nullptr);
  // Sa fenêtre existe : le clic l'ouvre, pas la fenêtre d'attente.
  CHECK(closed->ported);
  CHECK_FALSE(closed->shows_open);
  facts.open = {MenuCommand::kBooking};
  const std::vector<MenuIconView> icons = Compose(facts);
  const MenuIconView* open = Find(icons, MenuCommand::kBooking);
  REQUIRE(open != nullptr);
  CHECK(open->shows_open);
}

TEST_CASE("Icônes de menu : le tableau des classements a son icône, sans art livré",
          "[app][menu]") {
  MenuIconFacts facts;
  const std::vector<MenuIconView> icons = Compose(facts);
  const MenuIconView* ranking = Find(icons, MenuCommand::kRankingBoard);
  REQUIRE(ranking != nullptr);
  // Sa fenêtre existe : le clic l'ouvre, pas la fenêtre d'attente.
  CHECK(ranking->ported);
  CHECK(moonclient::MenuCommandPorted(MenuCommand::kRankingBoard));
  // Sans art, le carré de secours tient sa place.
  CHECK_FALSE(ranking->normal.usable());
  facts.open = {MenuCommand::kRankingBoard};
  CHECK(Find(Compose(facts), MenuCommand::kRankingBoard)->shows_open);
}

TEST_CASE("Icônes de menu : la réputation ouvre sa fenêtre", "[app][menu]") {
  MenuIconFacts facts;
  const std::vector<MenuIconView> icons = Compose(facts);
  const MenuIconView* reputation = Find(icons, MenuCommand::kReputation);
  REQUIRE(reputation != nullptr);
  CHECK(reputation->ported);
  CHECK(moonclient::MenuCommandPorted(MenuCommand::kReputation));
  facts.open = {MenuCommand::kReputation};
  CHECK(Find(Compose(facts), MenuCommand::kReputation)->shows_open);
}

TEST_CASE("Icônes de menu : le Marché a son icône, sans art livré", "[app][menu]") {
  MenuIconFacts facts;
  const std::vector<MenuIconView> icons = Compose(facts);
  const MenuIconView* market = Find(icons, MenuCommand::kMarket);
  REQUIRE(market != nullptr);
  // Sa fenêtre existe : le clic l'ouvre, pas la fenêtre d'attente.
  CHECK(market->ported);
  CHECK(moonclient::MenuCommandPorted(MenuCommand::kMarket));
  CHECK_FALSE(moonclient::ActionOfMenuCommand(MenuCommand::kMarket).has_value());
  CHECK_FALSE(market->normal.usable());
  facts.open = {MenuCommand::kMarket};
  CHECK(Find(Compose(facts), MenuCommand::kMarket)->shows_open);
}

TEST_CASE("Icônes de menu : la base de données a son icône, sans art livré", "[app][menu]") {
  MenuIconFacts facts;
  const std::vector<MenuIconView> icons = Compose(facts);
  const MenuIconView* database = Find(icons, MenuCommand::kDatabase);
  REQUIRE(database != nullptr);
  // Sa fenêtre existe : le clic l'ouvre, pas la fenêtre d'attente.
  CHECK(database->ported);
  CHECK(moonclient::MenuCommandPorted(MenuCommand::kDatabase));
  CHECK_FALSE(moonclient::ActionOfMenuCommand(MenuCommand::kDatabase).has_value());
  CHECK_FALSE(database->normal.usable());
  facts.open = {MenuCommand::kDatabase};
  CHECK(Find(Compose(facts), MenuCommand::kDatabase)->shows_open);
}

TEST_CASE("Icônes de menu : chaque mini-jeu a son icône, enfoncée partie ouverte", "[app][menu]") {
  for (const MenuCommand command :
       {MenuCommand::kRojeweled, MenuCommand::kRoggle, MenuCommand::kSurvivors}) {
    INFO("icône " << ro::ui::MenuIconOf(command).stem);
    MenuIconFacts facts;
    const MenuIconView* game = Find(Compose(facts), command);
    REQUIRE(game != nullptr);
    // Le jeu existe : le clic l'ouvre, pas la fenêtre d'attente.
    CHECK(game->ported);
    CHECK_FALSE(moonclient::ActionOfMenuCommand(command).has_value());
    facts.open = {command};
    CHECK(Find(Compose(facts), command)->shows_open);
  }
}

TEST_CASE("Icônes de menu : le Statut montre la tête du personnage quand elle est rendue",
          "[app][menu]") {
  // Deux textures factices, à double densité : 64 sur 68 pour une case de 32
  // sur 34.
  constexpr ImTextureID kHeadTexture = 11;
  constexpr ImTextureID kPressedHeadTexture = 12;
  const ro::ui::SkinImage head{.id = kHeadTexture, .width = 64.0F, .height = 68.0F};
  const ro::ui::SkinImage pressed_head{.id = kPressedHeadTexture, .width = 64.0F, .height = 68.0F};

  MenuIconFacts facts;
  facts.status_head = head;
  facts.status_head_pressed = pressed_head;
  const std::vector<MenuIconView> icons = Compose(facts);
  const MenuIconView* status = Find(icons, MenuCommand::kStatus);
  REQUIRE(status != nullptr);
  // 🔴 À LA PLACE EXACTE DU BITMAP DU JEU : la densité se divise.
  CHECK(status->normal.id == kHeadTexture);
  CHECK(status->normal.width == 32.0F);
  CHECK(status->normal.height == 34.0F);
  CHECK(status->pressed.id == kPressedHeadTexture);
  CHECK(status->pressed.width == 32.0F);
  // Les autres icônes n'y touchent pas.
  CHECK_FALSE(Find(icons, MenuCommand::kEquipment)->normal.usable());

  // UN SEUL DES DEUX ÉTATS NE SUFFIT PAS : le Statut garde son art — ici aucun,
  // et le carré de secours.
  facts.status_head_pressed = {};
  const std::vector<MenuIconView> half_icons = Compose(facts);
  const MenuIconView* half = Find(half_icons, MenuCommand::kStatus);
  REQUIRE(half != nullptr);
  CHECK_FALSE(half->normal.usable());
  CHECK_FALSE(half->pressed.usable());

  // SANS TÊTE — aucun personnage chargé —, de même.
  const std::vector<MenuIconView> bare_icons = Compose(MenuIconFacts{});
  const MenuIconView* none = Find(bare_icons, MenuCommand::kStatus);
  REQUIRE(none != nullptr);
  CHECK_FALSE(none->normal.usable());
}

TEST_CASE("Icônes de menu : le Statut et l'Équipement ouvrent la fiche ou la fenêtre du jeu",
          "[app][menu]") {
  using moonclient::GameAction;
  using moonclient::MenuIconOpeners;
  // D'usine : la petite fenêtre de statut, et la fiche.
  CHECK(moonclient::ActionOfMenuCommand(MenuCommand::kStatus) == GameAction::kOpenStatus);
  CHECK(moonclient::ActionOfMenuCommand(MenuCommand::kEquipment) ==
        GameAction::kOpenCharacterEquipment);
  const MenuIconOpeners swapped{.status_game_window = false, .equipment_game_window = true};
  CHECK(moonclient::ActionOfMenuCommand(MenuCommand::kStatus, swapped) ==
        GameAction::kOpenCharacterEquipment);
  CHECK(moonclient::ActionOfMenuCommand(MenuCommand::kEquipment, swapped) ==
        GameAction::kToggleEquipWindow);
}
