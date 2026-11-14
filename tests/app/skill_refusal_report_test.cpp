#include "world/skill_refusal_report.h"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/skill_use_block.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/world.h"
#include "ro/vfs/item_link.h"
#include "ro/vfs/item_table.h"

using moonclient::SkillRefusalLine;
using moonclient::SkillRefusalReport;
using ro::sim::SkillRefusal;

namespace {

constexpr std::uint16_t kPreparePotion = 228;
constexpr std::uint32_t kMedicineBowl = 7134;

[[nodiscard]] ro::sim::World WorldKnowing(std::uint16_t skill_id, const char* name) {
  ro::sim::World world;
  ro::sim::KnownSkill known;
  known.id = skill_id;
  known.level = 1;
  known.name = name;
  world.skill_book().Learn(known);
  return world;
}

[[nodiscard]] ro::vfs::ItemTable BowlTable() {
  ro::vfs::ItemTable table;
  table.Put(kMedicineBowl, ro::vfs::ItemEntry{.name = "Medicine Bowl"});
  return table;
}

[[nodiscard]] bool Contains(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}

}  // namespace

TEST_CASE("Refus de skill : l'objet manquant se nomme en lien, avec sa quantité", "[app][refus]") {
  // Le serveur porte l'objet ET la quantité : un Medicine Bowl pour Prepare
  // Potion. La ligne les dit, et l'objet se clique comme tout lien d'item.
  const ro::sim::World world = WorldKnowing(kPreparePotion, "AM_PHARMACY");
  const ro::vfs::ItemTable items = BowlTable();
  const std::string line = SkillRefusalLine(
      SkillRefusal{.skill_id = kPreparePotion, .cause = 71, .count = 1, .item_id = kMedicineBowl},
      world, nullptr, &items);
  const std::string link = ro::vfs::BuildItemLink(ro::vfs::ItemLink{.item_id = kMedicineBowl}, 0);
  CHECK(line == "AM_PHARMACY : [" + link + "] requis en quantité 1.");

  // Sans table des objets, le numéro : la ligne dit encore quoi chercher.
  const std::string bare = SkillRefusalLine(
      SkillRefusal{.skill_id = kPreparePotion, .cause = 71, .count = 2, .item_id = kMedicineBowl},
      world, nullptr, nullptr);
  CHECK(Contains(bare, "[#7134] requis en quantité 2."));

  // L'équipement requis, de même.
  const std::string equip = SkillRefusalLine(
      SkillRefusal{.skill_id = kPreparePotion, .cause = 72, .item_id = kMedicineBowl}, world,
      nullptr, &items);
  CHECK(Contains(equip, "Équipez [" + link + "]."));
}

TEST_CASE("Refus de skill : chaque motif a sa phrase, et certains se taisent", "[app][refus]") {
  const ro::sim::World world = WorldKnowing(28, "AL_HEAL");
  const auto said = [&world](std::uint8_t cause, std::int32_t count = 0,
                             std::uint16_t skill_id = 28) {
    return SkillRefusalLine(SkillRefusal{.skill_id = skill_id, .cause = cause, .count = count},
                            world, nullptr, nullptr);
  };
  CHECK(said(1) == "AL_HEAL : SP insuffisants.");
  CHECK(said(7) == "AL_HEAL : Une Red Gemstone est requise.");
  // ⚠ LE MOTIF ZÉRO N'AFFIRME RIEN : il ne dit pas « niveau insuffisant ».
  CHECK(said(0) == "AL_HEAL : Le skill a échoué.");
  // La phrase qui cite déjà le skill n'est pas précédée de son nom.
  CHECK(said(4) == "Impossible d'utiliser [AL_HEAL] : son délai de réutilisation court encore.");
  // Un nombre nul change la phrase plutôt que de dire « 0 ».
  CHECK(said(74, 3) == "AL_HEAL : Demande 3 Spirit Sphere.");
  CHECK(said(74, 0) == "AL_HEAL : Une Spirit Sphere est requise.");
  // La classe d'arme exigée se lit dans le nombre.
  CHECK(said(78, 11) == "AL_HEAL : [Bow] doit être équipé.");
  CHECK(said(78, 99).empty());
  // Les motifs que le client tait — celui qui suit un message déjà envoyé.
  CHECK(said(70).empty());
  CHECK(said(38).empty());
  // Basic Skill dit quel geste lui manque.
  CHECK(said(0, 2, 1) == "Skill n°1 : Vous n'avez pas assez de Basic Skill pour vous asseoir.");
  // Un motif au-delà de la table se dit par son numéro.
  CHECK(said(200) == "AL_HEAL : Le skill a échoué (motif 200).");
}

TEST_CASE("Refus de skill : chaque refus s'écrit une fois, daté de sa réception", "[app][refus]") {
  ro::sim::World world = WorldKnowing(28, "AL_HEAL");
  SkillRefusalReport report;
  world.skill_book().RecordRefusal(SkillRefusal{.skill_id = 28, .cause = 1, .refused_ms = 500});
  report.Notice(world, nullptr, nullptr, true);
  REQUIRE(world.chat_lines().size() == 1);
  const ro::sim::ChatLine& line = world.chat_lines()[0];
  CHECK(line.text == "AL_HEAL : SP insuffisants.");
  CHECK(line.type == ro::sim::ChatType::kSkillFailure);
  CHECK(line.said_ms == 500);
  CHECK(line.color == moonclient::kSkillRefusalColor);
  // Un second passage sans refus neuf n'écrit rien.
  report.Notice(world, nullptr, nullptr, true);
  CHECK(world.chat_lines().size() == 1);
}

TEST_CASE("Refus de skill : /skillfail éteint tait l'échec vague et le délai, pas le reste",
          "[app][refus]") {
  ro::sim::World world = WorldKnowing(28, "AL_HEAL");
  SkillRefusalReport report;
  constexpr std::uint16_t kSteal = 50;
  // Tus : l'échec sans motif, le délai de réutilisation, l'échec générique.
  world.skill_book().RecordRefusal(SkillRefusal{.skill_id = 28, .cause = 0});
  world.skill_book().RecordRefusal(SkillRefusal{.skill_id = 28, .cause = 4});
  world.skill_book().RecordRefusal(SkillRefusal{.skill_id = 28, .cause = 10});
  // Dits : ce qui manque, et l'échec que le motif zéro nomme pour Steal.
  world.skill_book().RecordRefusal(SkillRefusal{.skill_id = 28, .cause = 1});
  world.skill_book().RecordRefusal(SkillRefusal{.skill_id = kSteal, .cause = 0});
  report.Notice(world, nullptr, nullptr, false);
  REQUIRE(world.chat_lines().size() == 2);
  CHECK(world.chat_lines()[0].text == "AL_HEAL : SP insuffisants.");
  CHECK(world.chat_lines()[1].text.ends_with("Steal a échoué."));
  // ⚠ UN REFUS TU NE REVIENT PAS quand on rallume : il est passé.
  report.Notice(world, nullptr, nullptr, true);
  CHECK(world.chat_lines().size() == 2);
}

TEST_CASE("Refus de skill : une Twilight qui échoue dit ce qui manque au sac", "[app][refus]") {
  // Le serveur ne dit qu'« échec », sans objet. Le sac porte le manuel et
  // cinquante White Herb, aucune Empty Potion Bottle : il manque les herbes et
  // les bouteilles, pas le manuel.
  constexpr std::uint16_t kTwilight1 = 496;
  constexpr std::uint32_t kNormalPotionGuide = 7144;
  constexpr std::uint32_t kWhiteHerb = 509;
  constexpr std::uint32_t kEmptyPotionBottle = 1093;
  constexpr std::int16_t kHerbsOwned = 50;
  ro::sim::World world = WorldKnowing(kTwilight1, "AM_TWILIGHT1");
  world.inventory().Put(
      ro::sim::InventoryItem{.client_slot = 2, .item_id = kNormalPotionGuide, .amount = 1});
  world.inventory().Put(
      ro::sim::InventoryItem{.client_slot = 3, .item_id = kWhiteHerb, .amount = kHerbsOwned});
  ro::vfs::ItemTable table;
  table.Put(kNormalPotionGuide, ro::vfs::ItemEntry{.name = "Normal Potion Creation Guide"});
  table.Put(kWhiteHerb, ro::vfs::ItemEntry{.name = "White Herb"});
  table.Put(kEmptyPotionBottle, ro::vfs::ItemEntry{.name = "Empty Potion Bottle"});
  const std::string line =
      SkillRefusalLine(SkillRefusal{.skill_id = kTwilight1, .cause = 0}, world, nullptr, &table);
  CHECK(line.starts_with("AM_TWILIGHT1 : il manque "));
  CHECK(Contains(line,
                 ro::vfs::BuildItemLink(ro::vfs::ItemLink{.item_id = kWhiteHerb}, 0) + " 50/200"));
  CHECK(Contains(line, ro::vfs::BuildItemLink(ro::vfs::ItemLink{.item_id = kEmptyPotionBottle}, 0) +
                           " 0/200"));
  CHECK_FALSE(
      Contains(line, ro::vfs::BuildItemLink(ro::vfs::ItemLink{.item_id = kNormalPotionGuide}, 0)));

  // ⚠ ET ELLE PARLE MÊME SOUS `/skillfail` ÉTEINT : elle ne dit plus « échec ».
  SkillRefusalReport report;
  world.skill_book().RecordRefusal(SkillRefusal{.skill_id = kTwilight1, .cause = 0});
  report.Notice(world, nullptr, &table, false);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines()[0].text.starts_with("AM_TWILIGHT1 : il manque "));
}

TEST_CASE("Refus de skill : ce que le serveur tait, le client le dit par sa raison",
          "[app][refus]") {
  const ro::sim::World world = WorldKnowing(19, "MG_FIREBOLT");
  const auto said = [&world](ro::sim::LocalRefusal local, std::uint16_t status = 0) {
    return SkillRefusalLine(SkillRefusal{.skill_id = 19, .local = local, .blocking_status = status},
                            world, nullptr, nullptr);
  };
  CHECK(said(ro::sim::LocalRefusal::kSitting) ==
        "MG_FIREBOLT : Impossible d'utiliser un skill en étant assis.");
  CHECK(said(ro::sim::LocalRefusal::kInterrupted) == "MG_FIREBOLT : Incantation interrompue.");
  CHECK(said(ro::sim::LocalRefusal::kStatus, ro::sim::kEfstSilence) ==
        "MG_FIREBOLT : Impossible d'utiliser un skill sous Silence.");
  CHECK(said(ro::sim::LocalRefusal::kStatus, ro::sim::kEfstBerserk) ==
        "MG_FIREBOLT : Impossible d'utiliser un skill sous Berserk.");
  CHECK(said(ro::sim::LocalRefusal::kStatus, ro::sim::kEfstStun) ==
        "MG_FIREBOLT : Impossible d'utiliser un skill sous Stun.");
  CHECK(said(ro::sim::LocalRefusal::kStatus, ro::sim::kEfstTrickDead) ==
        "MG_FIREBOLT : Impossible d'utiliser un skill sous Play Dead.");
  CHECK(said(ro::sim::LocalRefusal::kChaseWalk) ==
        "MG_FIREBOLT : Impossible d'utiliser un skill sous Chase Walk.");
  CHECK(said(ro::sim::LocalRefusal::kStatus, ro::sim::kStatusAllRiding) ==
        "MG_FIREBOLT : Impossible d'utiliser un skill sur cette monture.");
  CHECK(said(ro::sim::LocalRefusal::kMuted) ==
        "MG_FIREBOLT : Impossible d'utiliser un skill : vos bonnes manières sont négatives.");
  // Un état que la table ne nomme pas dit encore que c'est un état.
  constexpr std::uint16_t kUnnamedStatus = 1;
  CHECK(said(ro::sim::LocalRefusal::kStatus, kUnnamedStatus) ==
        "MG_FIREBOLT : Ce skill ne peut pas être utilisé dans cet état.");
}

TEST_CASE("Refus de skill : /skillfail éteint tait l'interruption, pas les états", "[app][refus]") {
  ro::sim::World world = WorldKnowing(19, "MG_FIREBOLT");
  SkillRefusalReport report;
  world.skill_book().RecordRefusal(
      SkillRefusal{.skill_id = 19, .local = ro::sim::LocalRefusal::kInterrupted});
  world.skill_book().RecordRefusal(
      SkillRefusal{.skill_id = 19, .local = ro::sim::LocalRefusal::kSitting});
  report.Notice(world, nullptr, nullptr, false);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines()[0].text.ends_with("en étant assis."));
  CHECK(world.chat_lines()[0].type == ro::sim::ChatType::kSkillFailure);
  CHECK(world.chat_lines()[0].color == moonclient::kSkillRefusalColor);
}
