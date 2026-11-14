// LE MENU D'UN JOUEUR DE LA LISTE DU STAFF : il ne garde du menu d'entité que
// ce qui vaut hors de vue, y ajoute le voyage, et tient ses traits.

#include "world/staff_roster_menu.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/entity_menu.h"

#include "world/entity_menu.h"

using moonclient::BuildStaffRosterMenu;
using moonclient::EntityFamily;
using moonclient::EntityMenuFacts;
using moonclient::EntityVerb;
using moonclient::StaffTravelCommand;
using ro::ui::EntityMenuItem;
using ro::ui::EntityMenuView;

namespace {

constexpr std::uint32_t kPlayer = 2000002;

[[nodiscard]] const EntityMenuItem* Find(const EntityMenuView& view, EntityVerb verb) {
  const auto found = std::ranges::find_if(view.items, [verb](const EntityMenuItem& item) {
    return item.verb == static_cast<int>(verb);
  });
  return found != view.items.end() ? &*found : nullptr;
}

[[nodiscard]] EntityMenuFacts Listed(EntityFamily family, bool admin = false) {
  EntityMenuFacts facts;
  facts.family = family;
  facts.id = kPlayer;
  facts.name = "Petit Lot";
  facts.staff = true;
  facts.admin = admin;
  return facts;
}

[[nodiscard]] std::vector<EntityVerb> VerbsOf(const EntityMenuView& view) {
  std::vector<EntityVerb> verbs;
  for (const EntityMenuItem& item : view.items) {
    verbs.push_back(static_cast<EntityVerb>(item.verb));
  }
  return verbs;
}

}  // namespace

TEST_CASE("Menu de la liste : ce qui vaut par le nom ou l'AID, et le voyage",
          "[app][staff_roster]") {
  const EntityMenuView view = BuildStaffRosterMenu(Listed(EntityFamily::kPlayer));
  CHECK(VerbsOf(view) == std::vector<EntityVerb>{
                             EntityVerb::kWhisper, EntityVerb::kWhisperWindow,
                             EntityVerb::kMailSend, EntityVerb::kCopyName, EntityVerb::kInspect,
                             EntityVerb::kCopyId, EntityVerb::kStaffGoTo, EntityVerb::kStaffRecall,
                             EntityVerb::kEventPoints, EntityVerb::kMute, EntityVerb::kUnmute,
                             EntityVerb::kMannerGood, EntityVerb::kMannerBad,
                             EntityVerb::kChatBlockLog, EntityVerb::kNuke, EntityVerb::kKick,
                             EntityVerb::kJail, EntityVerb::kBlockAccount});
  CHECK(view.subject == kPlayer);
  CHECK(view.name == "Petit Lot");
}

TEST_CASE("Menu de la liste : rien de ce qui vise un acteur à l'écran", "[app][staff_roster]") {
  const EntityMenuView view = BuildStaffRosterMenu(Listed(EntityFamily::kPlayer, /*admin=*/true));
  for (const EntityVerb verb :
       {EntityVerb::kViewEquip, EntityVerb::kDealPropose, EntityVerb::kFollow,
        EntityVerb::kFriendAdd, EntityVerb::kReportPlayer, EntityVerb::kComeHere,
        EntityVerb::kSitStand, EntityVerb::kCopyPickIdentity, EntityVerb::kClientInfo}) {
    CHECK(Find(view, verb) == nullptr);
  }
  // Le retrait de l'équipement reste à l'administrateur seul.
  CHECK(Find(view, EntityVerb::kFullStrip) != nullptr);
  CHECK(Find(BuildStaffRosterMenu(Listed(EntityFamily::kPlayer)), EntityVerb::kFullStrip) ==
        nullptr);
}

TEST_CASE("Menu de la liste : la section du staff s'y montre même éteinte dans le menu du monde",
          "[app][staff_roster]") {
  // Le réglage du menu du monde n'est pas lu : la fenêtre est déjà au staff.
  const EntityMenuView view = BuildStaffRosterMenu(Listed(EntityFamily::kPlayer));
  const EntityMenuItem* inspect = Find(view, EntityVerb::kInspect);
  REQUIRE(inspect != nullptr);
  CHECK(inspect->staff);
  CHECK(inspect->submenu);
}

TEST_CASE("Menu de la liste : un trait par bloc, aucun en tête du menu ni du staff",
          "[app][staff_roster]") {
  const EntityMenuView view = BuildStaffRosterMenu(Listed(EntityFamily::kPlayer));
  REQUIRE_FALSE(view.items.empty());
  CHECK_FALSE(view.items.front().separator);
  // « Copier le nom » ouvrait déjà son bloc.
  CHECK(Find(view, EntityVerb::kCopyName)->separator);
  CHECK_FALSE(Find(view, EntityVerb::kInspect)->separator);
  CHECK_FALSE(Find(view, EntityVerb::kCopyId)->separator);
  // Le voyage est un bloc, et celui dont la tête est partie garde son trait.
  const EntityMenuItem* go = Find(view, EntityVerb::kStaffGoTo);
  const EntityMenuItem* recall = Find(view, EntityVerb::kStaffRecall);
  REQUIRE(go != nullptr);
  REQUIRE(recall != nullptr);
  CHECK(go->separator);
  CHECK(go->staff);
  CHECK(go->submenu);
  CHECK_FALSE(go->tip.empty());
  CHECK_FALSE(recall->separator);
  CHECK(Find(view, EntityVerb::kEventPoints)->separator);
  CHECK(Find(view, EntityVerb::kMute)->separator);
  CHECK_FALSE(Find(view, EntityVerb::kUnmute)->separator);
  CHECK(Find(view, EntityVerb::kNuke)->separator);
}

TEST_CASE("Menu de la liste : sur notre propre ligne, ni voyage ni sanction",
          "[app][staff_roster]") {
  const EntityMenuView view = BuildStaffRosterMenu(Listed(EntityFamily::kSelf, /*admin=*/true));
  CHECK(VerbsOf(view) ==
        std::vector<EntityVerb>{EntityVerb::kCopyName, EntityVerb::kInspect, EntityVerb::kCopyId});
}

TEST_CASE("Menu de la liste : le voyage se tape par le nom, espaces compris",
          "[app][staff_roster]") {
  CHECK(StaffTravelCommand(EntityVerb::kStaffGoTo, "Petit Lot") == "@jumpto Petit Lot");
  CHECK(StaffTravelCommand(EntityVerb::kStaffRecall, "Petit Lot") == "@recall Petit Lot");
  CHECK(StaffTravelCommand(EntityVerb::kStaffGoTo, "").empty());
  CHECK(StaffTravelCommand(EntityVerb::kKick, "Petit Lot").empty());
}
