// « @item » et « @itemreset » : ce que le staff envoie quand il se fait donner
// des items, et quand il vide son sac.

#include "chat/staff_item_command.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/craft_atlas_window.h"

using moonclient::StaffItemCommand;
using moonclient::StaffItemCommands;
using moonclient::StaffItemResetCommand;
using ro::ui::CraftAtlasGrantKind;
using ro::ui::CraftAtlasRow;
using ro::ui::SessionRights;
using ro::ui::StaffItemGrant;

namespace {

constexpr std::uint32_t kIronOre = 1002;
constexpr std::uint32_t kCoal = 1003;
constexpr std::uint32_t kAnvil = 986;
constexpr SessionRights kStaff{.staff_tools = true};
constexpr SessionRights kPlayer{.staff_tools = false};

// Deux matériaux consommés — le charbon déjà en quantité — et un guide qu'on
// ne possède pas.
[[nodiscard]] std::array<CraftAtlasRow, 3> Recipe() {
  return {CraftAtlasRow{.item_id = kIronOre, .name = "Iron Ore", .owned = 3, .quantity = 2},
          CraftAtlasRow{.item_id = kCoal, .name = "Coal", .owned = 50, .quantity = 1},
          CraftAtlasRow{.item_id = kAnvil, .name = "Anvil", .owned = 0, .kept = true}};
}

}  // namespace

TEST_CASE("@item : l'identifiant, puis la quantité", "[app][chat][staff]") {
  CHECK(StaffItemCommand(StaffItemGrant{.item_id = kIronOre, .amount = 5}) == "@item 1002 5");
  // Une pile pleine, pas davantage : au-delà le serveur refuserait tout.
  CHECK(StaffItemCommand(
            StaffItemGrant{.item_id = kIronOre, .amount = ro::ui::kStaffItemMostAmount + 1}) ==
        "@item 1002 " + std::to_string(ro::ui::kStaffItemMostAmount));
}

TEST_CASE("@item : une demande vide ne se compose pas", "[app][chat][staff]") {
  // Le serveur lirait une quantité nulle comme UN.
  CHECK(StaffItemCommand(StaffItemGrant{.item_id = kIronOre, .amount = 0}).empty());
  CHECK(StaffItemCommand(StaffItemGrant{.item_id = kIronOre, .amount = -3}).empty());
  CHECK(StaffItemCommand(StaffItemGrant{.item_id = 0, .amount = 1}).empty());
}

TEST_CASE("@item : rien ne part sans le droit du staff", "[app][chat][staff]") {
  const std::array<StaffItemGrant, 2> grants = {StaffItemGrant{.item_id = kIronOre, .amount = 2},
                                                StaffItemGrant{.item_id = kCoal, .amount = 1}};
  CHECK(StaffItemCommands(grants, kPlayer).empty());
  CHECK(StaffItemCommands(grants, kStaff) ==
        std::vector<std::string>{"@item 1002 2", "@item 1003 1"});
}

TEST_CASE("@item : l'Atlas demande tout, pour N fabrications", "[app][chat][staff]") {
  const std::array<CraftAtlasRow, 3> recipe = Recipe();
  constexpr int kCrafts = 3;
  CHECK(
      StaffItemCommands(
          ro::ui::CraftAtlasGrants(recipe, kCrafts, CraftAtlasGrantKind::kAll, std::nullopt).grants,
          kStaff) == std::vector<std::string>{"@item 1002 6", "@item 1003 3", "@item 986 1"});
}

TEST_CASE("@item : l'Atlas ne demande que le manque", "[app][chat][staff]") {
  const std::array<CraftAtlasRow, 3> recipe = Recipe();
  // Une fabrication : le minerai et le charbon suffisent, ils ne partent pas.
  CHECK(StaffItemCommands(
            ro::ui::CraftAtlasGrants(recipe, 1, CraftAtlasGrantKind::kMissing, std::nullopt).grants,
            kStaff) == std::vector<std::string>{"@item 986 1"});
  // Cinq : dix minerais exigés, trois tenus ; le charbon suffit encore.
  constexpr int kCrafts = 5;
  CHECK(StaffItemCommands(
            ro::ui::CraftAtlasGrants(recipe, kCrafts, CraftAtlasGrantKind::kMissing, std::nullopt)
                .grants,
            kStaff) == std::vector<std::string>{"@item 1002 7", "@item 986 1"});
  // Tout tenu : rien à envoyer.
  std::array<CraftAtlasRow, 3> stocked = recipe;
  for (CraftAtlasRow& row : stocked) {
    row.owned = ro::ui::kStaffItemMostAmount;
  }
  CHECK(StaffItemCommands(
            ro::ui::CraftAtlasGrants(stocked, kCrafts, CraftAtlasGrantKind::kMissing, std::nullopt)
                .grants,
            kStaff)
            .empty());
}

TEST_CASE("@itemreset : la commande nue, et seulement pour le staff", "[app][chat][staff]") {
  CHECK(StaffItemResetCommand(kStaff) == "@itemreset");
  // Le bouton a pu être dessiné une image avant que le droit ne tombe.
  CHECK(StaffItemResetCommand(kPlayer).empty());
}
