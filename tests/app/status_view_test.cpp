// Ce que la barre d'états montre, une fois les trois sources croisées.
//
// 🔴 TROIS PANNES MUETTES EN JEU. Une altération qui arrive en BITS et qu'on
// oublie de traduire ne montre rien ; la même comptée DEUX fois montre deux
// icônes pour un seul poison ; et un état sans image à qui l'on garde sa place
// creuse un trou dans la rangée. Aucune des trois ne fait échouer quoi que ce
// soit.

#include "character/status_view.h"

#include <cstdint>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/equip_location.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/world.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/status_table.h"

using moonclient::StatusAbbreviation;
using moonclient::StatusView;
using ro::sim::World;

namespace {

constexpr std::uint32_t kPlayer = 2'000'001;
constexpr ro::sim::StatusEffectId kBlessing = 37;
constexpr std::uint32_t kNow = 100'000;

// ⚠ LES VALEURS DU SERVEUR, ET ELLES NE SE DEVINENT PAS : `OPT1_SLEEP` vaut
// QUATRE, pas trois — le serveur saute le rang cinq, et trois est l'étourdissement.
// Voir `e_opt1`/`e_opt2` dans `src/map/status.hpp`.
constexpr std::uint16_t kOpt2Poison = 0x0001;
constexpr std::uint16_t kOpt1Sleep = 4;

[[nodiscard]] World WithPlayer() {
  World world;
  world.set_player_id(kPlayer);
  ro::sim::Actor self;
  self.id = kPlayer;
  world.Spawn(std::move(self));
  return world;
}

[[nodiscard]] ro::vfs::StatusTable Named() {
  ro::vfs::StatusTable table;
  table.by_id[kBlessing] = ro::vfs::StatusInfo{.name = "Blessing"};
  table.by_id[ro::sim::kEfstPoison] = ro::vfs::StatusInfo{.name = "Poison"};
  table.by_id[ro::sim::kEfstSleep] = ro::vfs::StatusInfo{.name = "Sleep"};
  return table;
}

}  // namespace

TEST_CASE("Abréviation d'un état : trois lettres tirées du nom, et rien d'autre", "[app][etats]") {
  CHECK(StatusAbbreviation("Blessing") == "BLE");
  // La ponctuation et les espaces ne comptent pas pour des lettres.
  CHECK(StatusAbbreviation("Two Hand Quicken") == "TWO");
  CHECK(StatusAbbreviation("_Ab") == "AB");

  // ⚠ UN NOM CORÉEN NE DONNE RIEN, et c'est voulu : couper trois OCTETS au
  // milieu d'une syllabe rendrait un carré, pas une abréviation.
  CHECK(StatusAbbreviation("\xEC\x88\x98\xEB\xA9\xB4").empty());

  // 🔴 SANS NOM, PAS DE LETTRES : aucun texte de repli écrit à la main.
  CHECK(StatusAbbreviation("").empty());
}

TEST_CASE("Barre d'états : l'aveuglement ne paraît que si la table le nomme", "[app][etats]") {
  // 🔴 LA TABLE EST LA SEULE SOURCE. L'aveuglement arrive en bit, et aucune table
  // de l'installation ne nomme son état : la barre ne lui invente pas de texte.
  constexpr std::uint16_t kOpt2Blind = 0x0010;
  World world = WithPlayer();
  world.SetStates(kPlayer, 0, kOpt2Blind, 0, kNow);

  const ro::vfs::StatusTable table = Named();
  StatusView silent(&table, nullptr);
  silent.Refresh(world, kNow, /*preview=*/0);
  CHECK(silent.cells().empty());

  // Le jour où la table le nomme, sa case paraît d'elle-même, sous ce nom.
  ro::vfs::StatusTable told = Named();
  told.by_id[ro::sim::kEfstBlind] = ro::vfs::StatusInfo{.name = "Blind"};
  StatusView named(&told, nullptr);
  named.Refresh(world, kNow, /*preview=*/0);
  REQUIRE(named.cells().size() == 1);
  CHECK(named.cells()[0].abbreviation == "BLI");
  CHECK(named.cells()[0].name == "Blind");
}

TEST_CASE("Barre d'états : un état sans image ni lettre ne prend pas de place", "[app][etats]") {
  // Sans table ni icônes, il ne reste rien à peindre — et surtout pas une case
  // vide, qui creuserait un trou dans la rangée.
  World world = WithPlayer();
  world.NotePlayerStatus(kBlessing, true, 60'000, kNow);

  StatusView blind(nullptr, nullptr);
  blind.Refresh(world, kNow, /*preview=*/0);
  CHECK(blind.cells().empty());

  // Nommé, il se replie sur ses trois lettres.
  const ro::vfs::StatusTable table = Named();
  StatusView named(&table, nullptr);
  named.Refresh(world, kNow, /*preview=*/0);
  REQUIRE(named.cells().size() == 1);
  CHECK(named.cells()[0].abbreviation == "BLE");
  CHECK(named.cells()[0].name == "Blessing");
}

TEST_CASE("Barre d'états : le restant se compte à partir de l'échéance", "[app][etats]") {
  World world = WithPlayer();
  world.NotePlayerStatus(kBlessing, true, 60'000, kNow);

  const ro::vfs::StatusTable table = Named();
  StatusView view(&table, nullptr);
  view.Refresh(world, kNow + 20'000, /*preview=*/0);
  REQUIRE(view.cells().size() == 1);
  CHECK(view.cells()[0].remaining_ms == 40'000);
  CHECK(view.cells()[0].total_ms == 60'000);

  // ⚠ UNE ÉCHÉANCE PASSÉE NE DEVIENT PAS NÉGATIVE : elle compte pour zéro, et
  // zéro veut dire « sans échéance ». C'est le balayage de la boucle qui retire
  // l'état, pas cette vue.
  view.Refresh(world, kNow + 90'000, /*preview=*/0);
  REQUIRE(view.cells().size() == 1);
  CHECK(view.cells()[0].remaining_ms == 0);
}

TEST_CASE("Barre d'états : les altérations arrivent en bits, et sans durée", "[app][etats]") {
  // 🔴 LE SOMMEIL N'A PAS D'EFST SUR LE FIL. Le serveur le transmet dans
  // `opt1` ; sans la conversion, le joueur dormirait sans rien voir.
  World world = WithPlayer();
  world.SetStates(kPlayer, kOpt1Sleep, 0, 0, kNow);

  const ro::vfs::StatusTable table = Named();
  StatusView view(&table, nullptr);
  view.Refresh(world, kNow, /*preview=*/0);
  REQUIRE(view.cells().size() == 1);
  CHECK(view.cells()[0].name == "Sleep");
  CHECK(view.cells()[0].remaining_ms == 0);
  CHECK(view.cells()[0].total_ms == 0);
}

TEST_CASE("Barre d'états : la même altération par les deux voies ne se compte qu'une fois",
          "[app][etats]") {
  // 🔴 CERTAINES VOYAGENT PAR LES DEUX CHEMINS — un bit d'`opt2` et un EFST de
  // son cru. Additionnées, elles donneraient deux icônes pour un seul poison, et
  // c'est la voie qui porte la DURÉE qu'il faut garder.
  World world = WithPlayer();
  world.NotePlayerStatus(ro::sim::kEfstPoison, true, 30'000, kNow);
  world.SetStates(kPlayer, 0, kOpt2Poison, 0, kNow);

  const ro::vfs::StatusTable table = Named();
  StatusView view(&table, nullptr);
  view.Refresh(world, kNow, /*preview=*/0);
  REQUIRE(view.cells().size() == 1);
  CHECK(view.cells()[0].remaining_ms == 30'000);
}

TEST_CASE("Barre d'états : les munitions portées disent leur nom et leur élément", "[app][etats]") {
  // 🔴 LA TABLE DES ÉTATS NE NOMME PAS CET ÉTAT : sans habillage, l'infobulle
  // écrit « État sans nom », et la case ne dit rien de la flèche.
  constexpr std::uint32_t kFireArrow = 1752;
  constexpr std::uint16_t kArrowSlot = 7;
  constexpr std::int32_t kArrowCount = 500;
  World world = WithPlayer();
  ro::sim::InventoryItem arrows;
  arrows.client_slot = kArrowSlot;
  arrows.item_id = kFireArrow;
  arrows.amount = kArrowCount;
  arrows.worn_at = ro::sim::kEquipAmmo;
  world.inventory().Put(arrows);
  world.NotePlayerStatus(ro::sim::kEfstEquipArrow, true, 0, kNow);

  ro::vfs::ItemTable items;
  items.Put(kFireArrow, ro::vfs::ItemEntry{.name = "Fire Arrow"});
  const ro::vfs::StatusTable table = Named();
  StatusView view(&table, nullptr);
  view.UseAmmo(&items, nullptr);

  // L'élément pas encore dit : le nom seul, sans ligne ni cadre.
  view.Refresh(world, kNow, /*preview=*/0);
  REQUIRE(view.cells().size() == 1);
  CHECK(view.cells()[0].name == "Fire Arrow");
  CHECK(view.cells()[0].abbreviation == "FIR");
  CHECK(view.cells()[0].description.empty());
  CHECK(view.cells()[0].accent == 0);

  // Le serveur dit le feu : une ligne, et un cadre.
  REQUIRE(world.TellAttackElement(kPlayer, ro::sim::Element::kFire));
  view.Refresh(world, kNow, /*preview=*/0);
  REQUIRE(view.cells().size() == 1);
  REQUIRE(view.cells()[0].description.size() == 1);
  CHECK(view.cells()[0].description[0].find("Feu") != std::string_view::npos);
  CHECK(view.cells()[0].accent != 0);

  // ⚠ NEUTRE A AUSSI SON CADRE : son absence se confondrait avec un élément
  // que le serveur n'a pas dit.
  REQUIRE(world.TellAttackElement(kPlayer, ro::sim::Element::kNeutral));
  view.Refresh(world, kNow, /*preview=*/0);
  REQUIRE(view.cells().size() == 1);
  CHECK(view.cells()[0].accent != 0);

  // 🔴 LE SERVEUR NE RETIRE PAS L'ÉTAT quand on ôte les flèches : la case
  // part quand même, parce que le sac ne porte plus de munition.
  REQUIRE(world.inventory().TakeOff(kArrowSlot));
  view.Refresh(world, kNow, /*preview=*/0);
  CHECK(view.cells().empty());
}
