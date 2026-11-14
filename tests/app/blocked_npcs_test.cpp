// LES PNJ RENDUS SOURDS AU CLIC GAUCHE, et la fenêtre de cible qu'on masque.
//
// 🔴 CE BANC GARDE DEUX PROMESSES QUE LE JOUEUR NE PEUT PAS VÉRIFIER SEUL. Un
// blocage survit au fichier : relu de travers, il laisserait une Kafra muette
// sans que rien ne dise pourquoi — ou bloquerait au hasard un GID recyclé. Et
// une fenêtre masquée doit revenir dès qu'on redésigne, la même entité
// comprise.

#include "world/blocked_npcs.h"

#include <cstdint>
#include <filesystem>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/world.h"
#include "ro/ui/options_window.h"

#include "game/save_folder.h"
#include "options/context_menu_options.h"
#include "support/temp_directory.h"
#include "world/entity_menu.h"
#include "world/targeting.h"

using moonclient::BlockedNpcs;
using moonclient::ContextMenuOptions;
using moonclient::ContextMenuRow;
using moonclient::ContextMenuRowId;
using moonclient::kBlockedNpcRowBase;
using moonclient::SwallowsLeftClick;
using moonclient::Targeting;
using moonclient::TargetingSettings;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::World;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint32_t kKafra = 3000001;
constexpr std::uint32_t kStylist = 3000042;
constexpr std::uint32_t kDynamicNpc = 110500;
constexpr std::uint32_t kPoring = 2000123;
constexpr std::uint32_t kNow = 1000;

[[nodiscard]] Actor Npc(std::uint32_t id) {
  Actor actor;
  actor.id = id;
  actor.kind = ActorKind::kNonPlayer;
  actor.name = "Kafra Employee";
  return actor;
}

[[nodiscard]] std::size_t ChatCount(const World& world) { return world.chat_lines().size(); }

}  // namespace

TEST_CASE("PNJ bloqués : la bascule va dans les deux sens, dans la plage seulement",
          "[app][entity_menu]") {
  BlockedNpcs blocked;
  CHECK(blocked.Toggle(kKafra, "Kafra Employee"));
  CHECK(blocked.Has(kKafra));
  CHECK_FALSE(blocked.Toggle(kKafra, "Kafra Employee"));
  CHECK_FALSE(blocked.Has(kKafra));
  // ⚠ UN GID DYNAMIQUE DÉSIGNERAIT N'IMPORTE QUI LE LENDEMAIN.
  CHECK_FALSE(blocked.Toggle(kDynamicNpc, "Passant"));
  CHECK(blocked.empty());
}

TEST_CASE("PNJ bloqués : le premier clic avalé de la session dit pourquoi, une fois",
          "[app][entity_menu]") {
  BlockedNpcs blocked;
  blocked.Put(kKafra, "Kafra Employee");
  World world;
  const Actor kafra = Npc(kKafra);

  CHECK(SwallowsLeftClick(blocked, true, kafra, world, kNow));
  CHECK(ChatCount(world) == 1);
  CHECK(SwallowsLeftClick(blocked, true, kafra, world, kNow));
  CHECK(ChatCount(world) == 1);

  // Une nouvelle session le redit.
  blocked.ForgetExplanations();
  CHECK(SwallowsLeftClick(blocked, true, kafra, world, kNow));
  CHECK(ChatCount(world) == 2);
}

TEST_CASE("PNJ bloqués : l'interrupteur éteint rend le clic, la liste reste",
          "[app][entity_menu]") {
  BlockedNpcs blocked;
  blocked.Put(kKafra, "Kafra Employee");
  World world;
  CHECK_FALSE(SwallowsLeftClick(blocked, false, Npc(kKafra), world, kNow));
  CHECK(blocked.Has(kKafra));
  CHECK_FALSE(SwallowsLeftClick(blocked, true, Npc(kStylist), world, kNow));
  CHECK(ChatCount(world) == 0);
}

TEST_CASE("PNJ bloqués : débloquer puis rebloquer réexplique le silence", "[app][entity_menu]") {
  BlockedNpcs blocked;
  blocked.Put(kKafra, "Kafra Employee");
  World world;
  CHECK(SwallowsLeftClick(blocked, true, Npc(kKafra), world, kNow));
  CHECK(blocked.Remove(kKafra));
  blocked.Put(kKafra, "Kafra Employee");
  CHECK(SwallowsLeftClick(blocked, true, Npc(kKafra), world, kNow));
  CHECK(ChatCount(world) == 2);
}

TEST_CASE("PNJ bloqués : le fichier se relit, et jette ce qui sort de la plage",
          "[app][entity_menu]") {
  TempDirectory client;
  std::filesystem::create_directories(client.path() / moonclient::kSaveFolder);
  BlockedNpcs written;
  written.Put(kKafra, "Kafra Employee");
  written.Put(kStylist, "");
  REQUIRE(moonclient::SaveBlockedNpcs(client.path(), written));

  BlockedNpcs read;
  moonclient::LoadBlockedNpcs(client.path(), read);
  CHECK(read.all() == written.all());

  // Une entrée écrite à la main hors de la plage ne bloque rien.
  client.WriteText(std::filesystem::path(moonclient::kSaveFolder) /
                       std::string(moonclient::kBlockedNpcsFileName),
                   "pnj_bloques:\n  - {id: 110500, nom: Passant}\n  - {id: 3000001, nom: Kafra}\n");
  BlockedNpcs hand;
  moonclient::LoadBlockedNpcs(client.path(), hand);
  CHECK_FALSE(hand.Has(kDynamicNpc));
  CHECK(hand.Has(kKafra));
}

TEST_CASE("PNJ bloqués : un fichier abîmé ne bloque rien, et n'empêche pas de jouer",
          "[app][entity_menu]") {
  TempDirectory client;
  client.WriteText(std::filesystem::path(moonclient::kSaveFolder) /
                       std::string(moonclient::kBlockedNpcsFileName),
                   "pnj_bloques: [ {id: \n");
  BlockedNpcs read;
  moonclient::LoadBlockedNpcs(client.path(), read);
  CHECK(read.empty());
}

TEST_CASE("Page du menu : une ligne par PNJ bloqué, et « Débloquer » le retire",
          "[app][entity_menu]") {
  ContextMenuOptions page;
  page.blocked().Put(kKafra, "Kafra Employee");
  page.blocked().Put(kStylist, "");
  page.Compose();
  int listed = 0;
  for (const ro::ui::OptionRowView& row : page.rows()) {
    if (row.id >= kBlockedNpcRowBase) {
      ++listed;
      CHECK(row.kind == ro::ui::OptionKind::kAction);
      CHECK(row.object != nullptr);
    }
  }
  CHECK(listed == 2);

  REQUIRE(page.Change(ro::ui::OptionsWindowAction{.changed = kBlockedNpcRowBase}));
  CHECK_FALSE(page.blocked().Has(kKafra));
  CHECK(page.blocked().Has(kStylist));
  CHECK(page.TakeBlockedChanged());
  CHECK_FALSE(page.TakeBlockedChanged());
}

TEST_CASE("Page du menu : les cases changent les réglages, le staff seul voit la sienne",
          "[app][entity_menu]") {
  ContextMenuOptions page;
  REQUIRE(page.Change(ro::ui::OptionsWindowAction{
      .changed = ContextMenuRowId(ContextMenuRow::kAllEntities), .on = true}));
  CHECK(page.settings().all_entities);
  CHECK_FALSE(page.Change(ro::ui::OptionsWindowAction{.changed = 1}));

  const auto has_staff_row = [&page]() {
    page.Compose();
    for (const ro::ui::OptionRowView& row : page.rows()) {
      if (row.id == ContextMenuRowId(ContextMenuRow::kStaffExtras)) {
        return true;
      }
    }
    return false;
  };
  CHECK_FALSE(has_staff_row());
  page.NoteStaff(true);
  CHECK(has_staff_row());
}

TEST_CASE("Fenêtre de cible : masquée pour une entité, rallumée par toute désignation",
          "[app][entity_menu][targeting]") {
  TargetingSettings settings;
  settings.enabled = true;
  Targeting targeting(settings);
  World world;
  Actor poring;
  poring.id = kPoring;
  poring.kind = ActorKind::kMonster;
  poring.name = "Poring";
  poring.card = ro::sim::NameCard{};
  world.Spawn(poring);

  REQUIRE(targeting.Designate(world, kPoring));
  CHECK(targeting.ShownFor(world, kPoring));
  targeting.HideFor(world, kPoring);
  CHECK(targeting.Hidden(world));
  CHECK_FALSE(targeting.ShownFor(world, kPoring));
  CHECK(targeting.MarkedActor(world) == nullptr);

  // La même entité, redésignée : elle revient.
  REQUIRE(targeting.Designate(world, kPoring));
  CHECK(targeting.ShownFor(world, kPoring));

  // Masquer une autre entité que la cible ne fait rien.
  targeting.HideFor(world, kKafra);
  CHECK_FALSE(targeting.Hidden(world));
}
