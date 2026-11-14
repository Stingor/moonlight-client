// LES LIGNES DE COMBAT : quels messages de dégâts un coup fait rejouer, et
// laquelle des quatre phrases chacun écrit.

#include "world/battle_log_rules.h"

#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/floating_number.h"
#include "ro/render/second_hand.h"
#include "ro/render/skill_hit_extra.h"
#include "ro/sim/party_roster.h"
#include "ro/vfs/item_words.h"

using moonclient::BattleLogLine;
using moonclient::BattleLogLineFor;
using moonclient::BattleMessage;
using moonclient::BattleMessagesOf;
using moonclient::StrikeSides;
using ro::sim::ChatKind;

namespace {

constexpr std::uint32_t kSelf = 2'000'001;
constexpr std::uint32_t kFriend = 2'000'002;
constexpr std::uint32_t kPoring = 110'000'001;
constexpr std::uint32_t kLandsMs = 1000;
constexpr std::uint32_t kStepMs = 200;
constexpr std::uint32_t kAssassin = 12;

ro::sim::Strike Blow(std::uint32_t attacker, std::uint32_t target, std::int32_t damage,
                     std::uint8_t type = 0, std::uint16_t hits = 1) {
  ro::sim::Strike strike;
  strike.attacker = attacker;
  strike.target = target;
  strike.damage = damage;
  strike.type = type;
  strike.hits = hits;
  return strike;
}

ro::sim::PartyRoster WithFriend(bool offline = false) {
  ro::sim::PartyRoster party;
  party.Upsert(ro::sim::PartyMember{.account_id = kFriend, .name = "Ami", .offline = offline});
  return party;
}

constexpr StrikeSides kKnown{.attacker_is_known = true, .attacker_is_player = true};

}  // namespace

TEST_CASE("Combat : un message par frappe montrée, à l'instant de la frappe", "[app][battle]") {
  const std::vector<BattleMessage> triple = BattleMessagesOf(
      Blow(kSelf, kPoring, 300, ro::render::kMultipleBlows, 3), kKnown, kLandsMs, false, true);
  REQUIRE(triple.size() == 3);
  CHECK(triple[0].damage == 100);
  CHECK(triple[2].due_ms == kLandsMs + 2 * kStepMs);
  CHECK(triple[1].receiver_id == kPoring);
  // Un manqué n'en rejoue aucun.
  CHECK(BattleMessagesOf(Blow(kSelf, kPoring, 0), kKnown, kLandsMs, false, true).empty());
}

TEST_CASE("Combat : la seconde main d'un assassin est un message de plus", "[app][battle]") {
  ro::sim::Strike strike = Blow(kSelf, kPoring, 200, ro::render::kMultipleBlows, 2);
  strike.second_damage = 50;
  StrikeSides sides = kKnown;
  sides.attacker_job = kAssassin;
  const std::vector<BattleMessage> messages =
      BattleMessagesOf(strike, sides, kLandsMs, false, true);
  REQUIRE(messages.size() == 3);
  // Les frappes suivantes partent ensemble, un demi-pas après la première.
  CHECK(messages[1].due_ms == kLandsMs + kStepMs / 2);
  CHECK(messages[2].damage == 50);
  CHECK(messages[2].due_ms == kLandsMs + ro::render::SecondHandOffsetMs(kStepMs));
}

TEST_CASE("Combat : l'enchaînement qui rejoue son coup écrit aussi au lanceur", "[app][battle]") {
  ro::sim::Strike strike = Blow(kSelf, kPoring, 300, ro::render::kMultipleBlows, 3);
  strike.skill_id = ro::render::kSwingReplayingSkill;
  const std::vector<BattleMessage> messages =
      BattleMessagesOf(strike, kKnown, kLandsMs, false, true);
  // Trois à la cible, deux reprises au lanceur.
  REQUIRE(messages.size() == 5);
  CHECK(messages[2].receiver_id == kSelf);
  CHECK(messages[2].receiver_is_player);
}

namespace {

// Les phrases de la table de messages, telles que l'installation les porte.
ro::vfs::ItemWords BattleWordsOfInstallation() {
  ro::vfs::ItemWords words;
  words.battle.someone = "Someone";
  words.battle.i_received = " [%s] inflicts '%d' damage points on you.";
  words.battle.party_received = " [%s] attacked by [%s] with '%d' damage points.";
  words.battle.i_gave = " [%s] receives '%d' damage points.";
  words.battle.party_gave = " [%s] attacks [%s] with '%d' damage points.";
  return words;
}

}  // namespace

TEST_CASE("Combat : sans phrase dans la table, aucune ligne n'est écrite", "[app][battle]") {
  // La table de messages est la seule source : pas de texte anglais en dur.
  const ro::vfs::ItemWords words;
  const ro::sim::PartyRoster party = WithFriend();
  const BattleMessage on_monster{.target_id = kPoring, .attacker_id = kSelf, .damage = 42};
  CHECK_FALSE(BattleLogLineFor(words, on_monster, kSelf, party, "Poring", "Moi"));
  const BattleMessage on_me{
      .target_id = kSelf, .attacker_id = kPoring, .damage = 7, .receiver_is_player = true};
  CHECK_FALSE(BattleLogLineFor(words, on_me, kSelf, party, "Moi", "Poring"));
  CHECK(moonclient::BattleName(words, "").empty());
}

TEST_CASE("Combat : quatre phrases, selon qui reçoit le message", "[app][battle]") {
  const ro::vfs::ItemWords words = BattleWordsOfInstallation();
  const ro::sim::PartyRoster party = WithFriend();
  // Un monstre reçoit : on se demande qui frappe.
  BattleMessage on_monster{.target_id = kPoring, .attacker_id = kSelf, .damage = 42};
  std::optional<BattleLogLine> line =
      BattleLogLineFor(words, on_monster, kSelf, party, "Poring", "Moi");
  REQUIRE(line.has_value());
  CHECK(line->kind == ChatKind::kBattle);
  CHECK(line->text == " [Poring] receives '42' damage points.");
  on_monster.attacker_id = kFriend;
  line = BattleLogLineFor(words, on_monster, kSelf, party, "Poring", "Ami");
  REQUIRE(line.has_value());
  CHECK(line->kind == ChatKind::kPartyBattle);
  CHECK(line->text == " [Ami] attacks [Poring] with '42' damage points.");
  // Un joueur reçoit : on se demande qui encaisse.
  BattleMessage on_me{
      .target_id = kSelf, .attacker_id = kPoring, .damage = 7, .receiver_is_player = true};
  line = BattleLogLineFor(words, on_me, kSelf, party, "Moi", "Poring");
  REQUIRE(line.has_value());
  CHECK(line->text == " [Poring] inflicts '7' damage points on you.");
  on_me.target_id = kFriend;
  line = BattleLogLineFor(words, on_me, kSelf, party, "Ami", "Poring");
  REQUIRE(line.has_value());
  CHECK(line->text == " [Ami] attacked by [Poring] with '7' damage points.");
  // Un membre déconnecté, un inconnu : rien.
  CHECK_FALSE(BattleLogLineFor(words, on_me, kSelf, WithFriend(true), "Ami", "Poring"));
  on_me.target_id = kPoring + 1;
  CHECK_FALSE(BattleLogLineFor(words, on_me, kSelf, party, "X", "Poring"));
  // Le nom se coupe au dièse, et « Someone » remplace un nom vide.
  CHECK(moonclient::BattleName(words, "Garde#12") == "Garde");
  CHECK(moonclient::BattleName(words, "") == "Someone");
}
