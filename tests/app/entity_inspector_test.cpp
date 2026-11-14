// « PROPRIÉTÉS… » : l'entrée du staff, et ce que l'inspecteur prépare pour sa
// fenêtre — la demande, l'appariement de la réponse, l'attente qui expire, la
// carte qui change.

#include "world/entity_inspector.h"

#include <cstdint>
#include <optional>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/entity_props.h"
#include "ro/ui/entity_inspector_window.h"
#include "ro/ui/entity_menu.h"

#include "world/entity_menu.h"

using moonclient::BuildEntityMenu;
using moonclient::EntityFamily;
using moonclient::EntityInspector;
using moonclient::EntityMenuFacts;
using moonclient::EntityVerb;
using moonclient::kEntityInspectorWaitMs;
using ro::net::EntityPropsReply;
using ro::net::EntityPropsStatus;
using ro::ui::EntityInspectorPhase;

namespace {

constexpr std::uint32_t kPlayer = 2000002;
constexpr std::uint32_t kPoring = 110001;
constexpr std::uint32_t kStartMs = 1000;
constexpr const char* kMap = "prontera";

[[nodiscard]] bool Offers(EntityFamily family, bool staff) {
  EntityMenuFacts facts;
  facts.family = family;
  facts.id = kPlayer;
  facts.name = "Quelqu'un";
  facts.staff = staff;
  for (const ro::ui::EntityMenuItem& item : BuildEntityMenu(facts, {}).items) {
    if (item.verb == static_cast<int>(EntityVerb::kInspect)) {
      return item.staff && item.submenu && !item.disabled;
    }
  }
  return false;
}

[[nodiscard]] EntityPropsReply Found(std::uint32_t gid) {
  EntityPropsReply reply;
  reply.gid = gid;
  reply.status = EntityPropsStatus::kFound;
  reply.props = {{.key = "Serveur", .value = ""}, {.key = "Nom", .value = "Alice"}};
  return reply;
}

struct Inspecting {
  EntityInspector inspector;
  ro::ui::EntityInspectorWindowState state;

  Inspecting() { inspector.Open(state, kPlayer, "Joueur — Alice", kMap, kStartMs); }

  void Frame(std::uint32_t now_ms, bool refresh = false, const char* map = kMap) {
    inspector.Advance(state, ro::ui::EntityInspectorAnswer{.refresh = refresh}, map, now_ms);
  }
};

}  // namespace

TEST_CASE("Inspecteur : « Propriétés… » au staff seul, sur toute famille",
          "[app][entity_inspector]") {
  for (const EntityFamily family :
       {EntityFamily::kSelf, EntityFamily::kPlayer, EntityFamily::kMonster, EntityFamily::kNpc,
        EntityFamily::kGroundItem, EntityFamily::kOther}) {
    CHECK(Offers(family, /*staff=*/true));
    CHECK_FALSE(Offers(family, /*staff=*/false));
  }
}

TEST_CASE("Inspecteur : l'ouverture demande une fois, et attend", "[app][entity_inspector]") {
  Inspecting probe;
  CHECK(probe.state.open);
  CHECK(probe.state.raise);
  CHECK(probe.inspector.view().subject == "Joueur — Alice");
  CHECK(probe.inspector.view().phase == EntityInspectorPhase::kAsking);
  CHECK(probe.inspector.TakeRequest() == std::optional<std::uint32_t>(kPlayer));
  CHECK_FALSE(probe.inspector.TakeRequest().has_value());
  // Pas d'autre demande tant que la première attend, même si on insiste.
  probe.Frame(kStartMs + 1, /*refresh=*/true);
  CHECK_FALSE(probe.inspector.TakeRequest().has_value());
}

TEST_CASE("Inspecteur : la réponse d'une autre entité est jetée", "[app][entity_inspector]") {
  Inspecting probe;
  probe.inspector.Note(Found(kPoring));
  CHECK(probe.inspector.view().phase == EntityInspectorPhase::kAsking);
  CHECK(probe.inspector.view().rows.empty());
  probe.inspector.Note(Found(kPlayer));
  CHECK(probe.inspector.view().phase == EntityInspectorPhase::kReady);
  REQUIRE(probe.inspector.view().rows.size() == 2);
  CHECK(probe.inspector.view().rows[0].value.empty());
  CHECK(probe.inspector.view().rows[1].value == "Alice");
}

TEST_CASE("Inspecteur : le refus et l'entité introuvable se disent", "[app][entity_inspector]") {
  Inspecting probe;
  EntityPropsReply reply;
  reply.gid = kPlayer;
  reply.status = EntityPropsStatus::kDenied;
  probe.inspector.Note(reply);
  CHECK(probe.inspector.view().phase == EntityInspectorPhase::kDenied);
  reply.status = EntityPropsStatus::kMissing;
  probe.inspector.Note(reply);
  CHECK(probe.inspector.view().phase == EntityInspectorPhase::kMissing);
}

TEST_CASE("Inspecteur : une attente sans réponse finit par se dire", "[app][entity_inspector]") {
  Inspecting probe;
  probe.Frame(kStartMs + kEntityInspectorWaitMs - 1);
  CHECK(probe.inspector.view().phase == EntityInspectorPhase::kAsking);
  probe.Frame(kStartMs + kEntityInspectorWaitMs);
  CHECK(probe.inspector.view().phase == EntityInspectorPhase::kSilent);
  // « Redemander » repart d'une attente neuve.
  static_cast<void>(probe.inspector.TakeRequest());
  probe.Frame(kStartMs + kEntityInspectorWaitMs + 1, /*refresh=*/true);
  CHECK(probe.inspector.view().phase == EntityInspectorPhase::kAsking);
  CHECK(probe.inspector.TakeRequest() == std::optional<std::uint32_t>(kPlayer));
}

TEST_CASE("Inspecteur : « Actualiser » vide la fiche et redemande", "[app][entity_inspector]") {
  Inspecting probe;
  static_cast<void>(probe.inspector.TakeRequest());
  probe.inspector.Note(Found(kPlayer));
  probe.Frame(kStartMs + 1, /*refresh=*/true);
  CHECK(probe.inspector.view().phase == EntityInspectorPhase::kAsking);
  CHECK(probe.inspector.view().rows.empty());
  CHECK(probe.inspector.TakeRequest() == std::optional<std::uint32_t>(kPlayer));
}

TEST_CASE("Inspecteur : une autre carte ferme la fenêtre et oublie la cible",
          "[app][entity_inspector]") {
  Inspecting probe;
  static_cast<void>(probe.inspector.TakeRequest());
  probe.Frame(kStartMs + 1, /*refresh=*/false, "geffen");
  CHECK_FALSE(probe.state.open);
  CHECK(probe.inspector.gid() == 0);
  // Une réponse tardive ne rouvre rien.
  probe.inspector.Note(Found(kPlayer));
  CHECK(probe.inspector.view().rows.empty());
}

TEST_CASE("Inspecteur : la fenêtre fermée oublie la cible", "[app][entity_inspector]") {
  Inspecting probe;
  probe.state.open = false;
  probe.Frame(kStartMs + 1);
  CHECK(probe.inspector.gid() == 0);
  CHECK_FALSE(probe.inspector.TakeRequest().has_value());
}
