// LA CHAÎNE D'AFFINAGE AUTOMATIQUE : elle ne part qu'après un oui donné dans la
// fenêtre ouverte, et une fenêtre rouverte redemande.
//
// 🔴 CHAQUE TENTATIVE PEUT DÉTRUIRE L'ARME. Le réglage coché survit d'une partie
// à l'autre : s'il suffisait, rouvrir la fenêtre relancerait des refines que
// personne n'a demandés ce jour-là.

#include <cstdint>
#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/net/connection.h"
#include "ro/sim/craft_session.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world.h"
#include "ro/ui/weapon_refine_window.h"

#include "items/weapon_refine_flow.h"
#include "support/temp_directory.h"

using moonclient::WeaponRefineFlow;
using moonclient::WeaponRefineSettings;
using ro::ui::RefineChainNote;
using ro::ui::WeaponRefineVerb;
using ro::ui::WeaponRefineWindowAction;

namespace {

constexpr std::uint32_t kPhracon = 1010;
constexpr std::uint16_t kOreSlot = 2;
constexpr std::int16_t kOreStock = 10;
constexpr std::int16_t kKnifeSlot = 3;
constexpr std::uint32_t kKnife = 1201;
constexpr std::uint32_t kStartMs = 1000;

struct ChainBench {
  WeaponRefineFlow flow{nullptr, nullptr, nullptr};
  ro::sim::World world;
  ro::net::Connection link;
  ro::ui::WeaponRefineWindowState state;
  WeaponRefineSettings settings;

  ChainBench() {
    settings.auto_refine = true;
    world.inventory().Put(
        ro::sim::InventoryItem{.client_slot = kOreSlot, .item_id = kPhracon, .amount = kOreStock});
  }

  void ListArrives(std::uint32_t now_ms) {
    world.craft().TellRefineList(
        {ro::sim::RefinableWeapon{.index = kKnifeSlot, .item_id = kKnife}});
    flow.Refresh(world, state, settings, now_ms);
  }

  // Ce que la fenêtre aurait rendu : la liste montrée, l'arme visible.
  void Answer(WeaponRefineVerb verb, std::uint32_t now_ms) {
    WeaponRefineWindowAction asked;
    asked.verb = verb;
    asked.list_drawn = true;
    asked.selection_visible = true;
    asked.first_visible = kKnifeSlot;
    flow.Apply(asked, state, settings, link, world, now_ms);
    flow.Refresh(world, state, settings, now_ms);
  }
};

}  // namespace

TEST_CASE("Chaîne d'affinage : le réglage coché seul ne lance rien", "[app][refine]") {
  ChainBench bench;
  bench.ListArrives(kStartMs);
  CHECK_FALSE(bench.flow.view().chain_armed);
  CHECK(bench.flow.view().chain_note == RefineChainNote::kReady);
  bench.Answer(WeaponRefineVerb::kNothing, kStartMs + 1);
  CHECK_FALSE(bench.flow.view().chain_running);
}

TEST_CASE("Chaîne d'affinage : le oui de la fenêtre l'arme et la programme", "[app][refine]") {
  ChainBench bench;
  bench.ListArrives(kStartMs);
  // ⚠ LE RÉGLAGE N'EST PAS ENCORE COCHÉ : c'est la partie qui l'écrit, une fois
  // le verbe rendu. La chaîne doit partir quand même.
  bench.settings.auto_refine = false;
  bench.Answer(WeaponRefineVerb::kStartChain, kStartMs + 1);
  CHECK(bench.flow.view().chain_note == RefineChainNote::kRefineSoon);

  bench.settings.auto_refine = true;
  bench.flow.Refresh(bench.world, bench.state, bench.settings, kStartMs + 2);
  CHECK(bench.flow.view().chain_armed);
  CHECK(bench.flow.view().chain_running);
}

TEST_CASE("Chaîne d'affinage : une fenêtre rouverte redemande", "[app][refine]") {
  ChainBench bench;
  bench.ListArrives(kStartMs);
  bench.Answer(WeaponRefineVerb::kStartChain, kStartMs + 1);
  REQUIRE(bench.flow.view().chain_armed);

  bench.Answer(WeaponRefineVerb::kClose, kStartMs + 2);
  bench.ListArrives(kStartMs + 3);
  CHECK_FALSE(bench.flow.view().chain_armed);
  CHECK(bench.flow.view().chain_note == RefineChainNote::kReady);
}

TEST_CASE("Chaîne d'affinage : « Arrêter » la désarme", "[app][refine]") {
  ChainBench bench;
  bench.ListArrives(kStartMs);
  bench.Answer(WeaponRefineVerb::kStartChain, kStartMs + 1);
  REQUIRE(bench.flow.view().chain_running);

  bench.Answer(WeaponRefineVerb::kStop, kStartMs + 2);
  CHECK_FALSE(bench.flow.view().chain_armed);
  CHECK_FALSE(bench.flow.view().chain_running);
  CHECK(bench.flow.view().chain_note == RefineChainNote::kPaused);
}

TEST_CASE("Chaîne d'affinage : la raison d'arrêt survit au rechargement du catalogue",
          "[app][refine]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path catalogue = folder.path() / "en.txt";
  {
    std::ofstream file(catalogue, std::ios::binary);
    file << "fr: Plus aucun minerai : chaîne arrêtée.\ntr: Out of ore: chain stopped.\n";
  }
  REQUIRE(ro::core::LoadLanguage(catalogue, "en").has_value());

  ChainBench bench;
  bench.world.inventory().Clear();
  bench.ListArrives(kStartMs);
  bench.Answer(WeaponRefineVerb::kStartChain, kStartMs + 1);
  bench.flow.Refresh(bench.world, bench.state, bench.settings, kStartMs + 2);
  REQUIRE(bench.flow.view().chain_note == RefineChainNote::kStopped);
  CHECK(bench.flow.view().stop_reason == "Out of ore: chain stopped.");

  // Le catalogue d'où vient la raison n'existe plus : elle tient par elle-même.
  ro::core::UseFrench();
  bench.flow.Refresh(bench.world, bench.state, bench.settings, kStartMs + 3);
  CHECK(bench.flow.view().stop_reason == "Out of ore: chain stopped.");
}
