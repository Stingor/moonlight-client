// LES LIGNES DU STAFF QUE LE SERVEUR RÉSERVE À UN RANG : les points de manière,
// au staff ; le retrait de tout l'équipement et la question du client, à
// l'administrateur seul.
//
// 🔴 UNE LIGNE MONTRÉE À UN RANG QUE LE SERVEUR REFUSE NE FAIT RIEN, SANS UN MOT :
// il ignore une commande que le groupe ne détient pas. Le banc tient le seuil
// de chacune.

#include <algorithm>
#include <cstdint>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/entity_menu.h"

#include "world/entity_menu.h"

using moonclient::BuildEntityMenu;
using moonclient::EntityFamily;
using moonclient::EntityMenuFacts;
using moonclient::EntityMenuSettings;
using moonclient::EntityVerb;
using ro::ui::EntityMenuItem;
using ro::ui::EntityMenuQuestion;
using ro::ui::EntityMenuView;

namespace {

constexpr std::uint32_t kSelf = 2000001;
constexpr std::uint32_t kPlayer = 2000002;
constexpr std::uint32_t kPoring = 110001;

[[nodiscard]] const EntityMenuItem* Find(const EntityMenuView& view, EntityVerb verb) {
  const auto found = std::ranges::find_if(view.items, [verb](const EntityMenuItem& item) {
    return item.verb == static_cast<int>(verb);
  });
  return found != view.items.end() ? &*found : nullptr;
}

[[nodiscard]] EntityMenuFacts Facts(EntityFamily family, std::uint32_t id) {
  EntityMenuFacts facts;
  facts.family = family;
  facts.id = id;
  facts.name = "Quelqu'un";
  return facts;
}

[[nodiscard]] bool HasRankedLines(const EntityMenuView& view) {
  return Find(view, EntityVerb::kMannerGood) != nullptr ||
         Find(view, EntityVerb::kMannerBad) != nullptr ||
         Find(view, EntityVerb::kFullStrip) != nullptr ||
         Find(view, EntityVerb::kClientInfo) != nullptr;
}

}  // namespace

TEST_CASE("Menu d'entité : un joueur hors du staff ne voit aucune des deux", "[app][entity_menu]") {
  const EntityMenuFacts facts = Facts(EntityFamily::kPlayer, kPlayer);
  CHECK_FALSE(HasRankedLines(BuildEntityMenu(facts, {})));
}

TEST_CASE("Menu d'entité : les points de manière au staff, sans le retrait de l'équipement",
          "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kPlayer, kPlayer);
  facts.staff = true;
  const EntityMenuView view = BuildEntityMenu(facts, {});
  const EntityMenuItem* good = Find(view, EntityVerb::kMannerGood);
  const EntityMenuItem* bad = Find(view, EntityVerb::kMannerBad);
  REQUIRE(good != nullptr);
  REQUIRE(bad != nullptr);
  // Une SAISIE, comme les points d'event : les minutes se choisissent.
  CHECK(good->question == EntityMenuQuestion::kMannerGood);
  CHECK(bad->question == EntityMenuQuestion::kMannerBad);
  CHECK(good->staff);
  CHECK(bad->submenu);
  // Le bon juste avant le mauvais, dans le bloc de la parole.
  CHECK(bad == good + 1);
  CHECK(Find(view, EntityVerb::kUnmute) + 1 == good);
  CHECK(Find(view, EntityVerb::kFullStrip) == nullptr);
  CHECK(Find(view, EntityVerb::kClientInfo) == nullptr);
}

TEST_CASE("Menu d'entité : le retrait de l'équipement attend le niveau 99", "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kPlayer, kPlayer);
  facts.staff = true;
  facts.admin = true;
  const EntityMenuView view = BuildEntityMenu(facts, {});
  const EntityMenuItem* strip = Find(view, EntityVerb::kFullStrip);
  REQUIRE(strip != nullptr);
  CHECK(strip->danger);
  CHECK(strip->submenu);
  // Le client d'origine ne demande rien : la ligne agit au clic.
  CHECK(strip->question == EntityMenuQuestion::kNone);
  // Elle ouvre le bloc de ce qui punit, devant la foudre.
  CHECK(strip->separator);
  CHECK(strip + 1 == Find(view, EntityVerb::kNuke));
  CHECK(Find(view, EntityVerb::kMannerGood) != nullptr);
}

TEST_CASE("Menu d'entité : la question du client attend le niveau 99", "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kPlayer, kPlayer);
  facts.staff = true;
  facts.admin = true;
  const EntityMenuView view = BuildEntityMenu(facts, {});
  const EntityMenuItem* client = Find(view, EntityVerb::kClientInfo);
  REQUIRE(client != nullptr);
  CHECK(client->staff);
  CHECK(client->submenu);
  // Une question, rien d'une punition, et posée au clic.
  CHECK_FALSE(client->danger);
  CHECK(client->question == EntityMenuQuestion::kNone);
  CHECK_FALSE(client->tip.empty());
  // Elle ouvre son bloc, devant le confort.
  CHECK(client->separator);
  CHECK(client + 1 == Find(view, EntityVerb::kComeHere));
}

TEST_CASE("Menu d'entité : aucune des deux hors d'un autre joueur", "[app][entity_menu]") {
  for (const auto& [family, id] :
       {std::pair{EntityFamily::kSelf, kSelf}, std::pair{EntityFamily::kMonster, kPoring}}) {
    EntityMenuFacts facts = Facts(family, id);
    facts.staff = true;
    facts.admin = true;
    CHECK_FALSE(HasRankedLines(BuildEntityMenu(facts, {})));
  }
}

TEST_CASE("Menu d'entité : le réglage du staff les replie avec le reste", "[app][entity_menu]") {
  EntityMenuFacts facts = Facts(EntityFamily::kPlayer, kPlayer);
  facts.staff = true;
  facts.admin = true;
  EntityMenuSettings settings;
  settings.staff_extras = false;
  CHECK_FALSE(HasRankedLines(BuildEntityMenu(facts, settings)));
}
