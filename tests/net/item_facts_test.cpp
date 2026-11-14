// CE QUE LE SERVEUR SEUL SAIT D'UN OBJET, sur le fil : ses sources et son
// script.
//
// 🔴 CE BANC TIENT TROIS CHOSES : la réponse d'une COMPÉTENCE passe par le même
// opcode et n'est pas pour nous ; une liste coupée garde ce qu'elle porte ; et
// un objet inconnu du serveur ne lit rien de plus que son statut.

#include "ro/net/item_facts.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/player_settings.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/bank.h"
#include "ro/sim/item_facts.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyItemFactsFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::ItemFacts;
using ro::sim::ItemSourceScope;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;

namespace {

constexpr std::uint32_t kJellopy = 909;

void PutText8(std::vector<std::uint8_t>& into, const std::string& text) {
  into.push_back(static_cast<std::uint8_t>(text.size()));
  into.insert(into.end(), text.begin(), text.end());
}

void PutText16(std::vector<std::uint8_t>& into, const std::string& text) {
  PutU16(into, static_cast<std::uint16_t>(text.size()));
  into.insert(into.end(), text.begin(), text.end());
}

[[nodiscard]] Frame Framed(std::uint16_t opcode, std::vector<std::uint8_t> body) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, opcode);
  PutU16(bytes, static_cast<std::uint16_t>(body.size() + 4));
  bytes.insert(bytes.end(), body.begin(), body.end());
  return Frame{.opcode = opcode, .bytes = bytes};
}

[[nodiscard]] Frame Sources(std::uint8_t is_skill, std::uint8_t scope) {
  std::vector<std::uint8_t> body;
  PutU32(body, kJellopy);
  body.push_back(is_skill);
  body.push_back(scope);
  body.push_back(2);  // count
  body.push_back(1);  // truncated
  body.push_back(3);  // treasure excluded
  PutU32(body, 1002);
  PutU32(body, 7000);
  body.push_back(0);  // ordinaire
  body.push_back(0);  // butin
  PutText8(body, "Poring");
  PutU32(body, 1038);
  PutU32(body, 100);
  body.push_back(2);  // MVP
  body.push_back(1);  // récompense de MVP
  PutText8(body, "Osiris");
  return Framed(ro::proto::ZcMoonlightTechdata::kOpcode, body);
}

}  // namespace

TEST_CASE("Faits d'objet : les sources d'un objet se lisent", "[net][description]") {
  ItemFacts facts;
  REQUIRE(ApplyItemFactsFrame(Sources(0, 1), facts) == FrameOutcome::kApplied);
  const ro::sim::ItemDropSources* told = facts.SourcesOf(kJellopy, ItemSourceScope::kBoss);
  REQUIRE(told != nullptr);
  CHECK(told->truncated);
  CHECK(told->treasure_excluded == 3);
  REQUIRE(told->sources.size() == 2);
  CHECK(told->sources[0].name == "Poring");
  CHECK(told->sources[0].rate_hundredths == 7000);
  CHECK_FALSE(told->sources[0].mvp_reward);
  CHECK(told->sources[1].boss_rank == 2);
  CHECK(told->sources[1].mvp_reward);
  CHECK(facts.SourcesOf(kJellopy, ItemSourceScope::kOrdinary) == nullptr);
}

TEST_CASE("Faits d'objet : un nom rembourré de NUL s'arrête au premier", "[net][description]") {
  // 🔴 LE SERVEUR ANNONCE LA LONGUEUR DU NOM DE SA BASE, bornée à vingt-quatre
  // octets, et ce nom porte son remplissage : « Poring » arrive suivi de
  // dix-huit NUL, qui se dessinaient en glyphes « caractère manquant ».
  constexpr std::size_t kServerNameBytes = 24;
  std::string padded = "Poring";
  padded.resize(kServerNameBytes, '\0');
  std::vector<std::uint8_t> body;
  PutU32(body, kJellopy);
  body.push_back(0);  // objet
  body.push_back(0);  // monstres ordinaires
  body.push_back(2);  // count
  body.push_back(0);
  body.push_back(0);
  PutU32(body, 1002);
  PutU32(body, 7000);
  body.push_back(0);
  body.push_back(0);
  PutText8(body, padded);
  // ⚠ LA LONGUEUR RESTE CONSOMMÉE ENTIÈRE : la source suivante se lit bien.
  PutU32(body, 1113);
  PutU32(body, 5000);
  body.push_back(0);
  body.push_back(0);
  PutText8(body, "Drops");
  ItemFacts facts;
  REQUIRE(ApplyItemFactsFrame(Framed(ro::proto::ZcMoonlightTechdata::kOpcode, body), facts) ==
          FrameOutcome::kApplied);
  const ro::sim::ItemDropSources* told = facts.SourcesOf(kJellopy, ItemSourceScope::kOrdinary);
  REQUIRE(told != nullptr);
  REQUIRE(told->sources.size() == 2);
  CHECK(told->sources[0].name == "Poring");
  CHECK(told->sources[1].name == "Drops");
  CHECK(told->sources[1].monster_id == 1113);
}

TEST_CASE("Faits d'objet : la réponse d'une compétence n'est pas la sienne", "[net][description]") {
  ItemFacts facts;
  CHECK(ApplyItemFactsFrame(Sources(1, 0), facts) == FrameOutcome::kUnhandled);
  CHECK(facts.SourcesOf(kJellopy, ItemSourceScope::kOrdinary) == nullptr);
}

TEST_CASE("Faits d'objet : les scripts et les combos se lisent", "[net][description]") {
  std::vector<std::uint8_t> body;
  PutU32(body, kJellopy);
  body.push_back(0);
  PutText16(body, "bonus bStr,1;");
  PutText16(body, "");
  PutText16(body, "");
  body.push_back(1);  // un combo
  body.push_back(2);  // deux membres
  PutU32(body, kJellopy);
  PutText8(body, "Jellopy");
  PutU32(body, 4001);
  PutText8(body, "Poring Card");
  PutText16(body, "bonus bLuk,5;");
  ItemFacts facts;
  REQUIRE(ApplyItemFactsFrame(Framed(ro::proto::ZcMoonlightItemscript::kOpcode, body), facts) ==
          FrameOutcome::kApplied);
  const ro::sim::ItemScripts* told = facts.ScriptsOf(kJellopy);
  REQUIRE(told != nullptr);
  CHECK(told->status == 0);
  CHECK(told->main == "bonus bStr,1;");
  CHECK(told->equip.empty());
  REQUIRE(told->combos.size() == 1);
  REQUIRE(told->combos[0].members.size() == 2);
  CHECK(told->combos[0].members[1].name == "Poring Card");
  CHECK(told->combos[0].script == "bonus bLuk,5;");
}

TEST_CASE("Faits d'objet : un objet inconnu du serveur ne porte que son statut",
          "[net][description]") {
  std::vector<std::uint8_t> body;
  PutU32(body, kJellopy);
  body.push_back(1);
  ItemFacts facts;
  REQUIRE(ApplyItemFactsFrame(Framed(ro::proto::ZcMoonlightItemscript::kOpcode, body), facts) ==
          FrameOutcome::kApplied);
  REQUIRE(facts.ScriptsOf(kJellopy) != nullptr);
  CHECK(facts.ScriptsOf(kJellopy)->status == 1);
}

TEST_CASE("Faits d'objet : les deux demandes, octet pour octet", "[net][description]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  REQUIRE(ro::net::RequestItemSources(link, kJellopy, ItemSourceScope::kBoss).has_value());
  const std::vector<std::uint8_t> sources = server.Receive(10);
  const std::vector<std::uint8_t> expected_sources = {0x0B, 0x0F, 10, 0, 0x8D, 0x03, 0, 0, 0, 1};
  CHECK(sources == expected_sources);
  REQUIRE(ro::net::RequestItemScripts(link, kJellopy).has_value());
  const std::vector<std::uint8_t> scripts = server.Receive(8);
  const std::vector<std::uint8_t> expected_scripts = {0x11, 0x0F, 8, 0, 0x8D, 0x03, 0, 0};
  CHECK(scripts == expected_scripts);
}

TEST_CASE("Faits d'objet : le nom d'un forgeron, demandé puis appris", "[net][description]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  REQUIRE(ro::net::RequestCreatorName(link, 150001).has_value());
  const std::vector<std::uint8_t> asked = server.Receive(6);
  const std::vector<std::uint8_t> expected = {0x69, 0x03, 0xF1, 0x49, 0x02, 0x00};
  CHECK(asked == expected);

  // La réponse : drapeau, personnage, et le nom sur vingt-quatre octets.
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcAckReqnameBygid::kOpcode);
  PutU16(bytes, 3);
  PutU32(bytes, 150001);
  const std::string name = "Forgeron";
  bytes.insert(bytes.end(), name.begin(), name.end());
  bytes.resize(ro::proto::ZcAckReqnameBygid::kWireSize, 0);
  ItemFacts facts;
  CHECK(facts.CreatorOf(150001) == nullptr);
  REQUIRE(ApplyItemFactsFrame(
              Frame{.opcode = ro::proto::ZcAckReqnameBygid::kOpcode, .bytes = bytes}, facts) ==
          FrameOutcome::kApplied);
  REQUIRE(facts.CreatorOf(150001) != nullptr);
  CHECK(*facts.CreatorOf(150001) == "Forgeron");
}

namespace {

// Une trame 0x0F05 : personnage, compte, puis les couples {id, valeur}.
[[nodiscard]] Frame Settings(const std::vector<std::pair<std::uint16_t, std::uint32_t>>& pairs) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcMoonlightSettings::kOpcode);
  PutU16(bytes, static_cast<std::uint16_t>(10 + pairs.size() * 6));
  PutU32(bytes, 150001);
  PutU16(bytes, static_cast<std::uint16_t>(pairs.size()));
  for (const auto& [id, value] : pairs) {
    PutU16(bytes, id);
    PutU32(bytes, value);
  }
  return Frame{.opcode = ro::proto::ZcMoonlightSettings::kOpcode, .bytes = bytes};
}

}  // namespace

TEST_CASE("Faits d'objet : chaque trame de réglages porte la liste entière de ramassage",
          "[net][description]") {
  constexpr std::uint16_t kGroupLevel = 26;
  ItemFacts facts;
  CHECK_FALSE(facts.autoloot_told());
  // L'annonce complète : des réglages, puis un ajout par item.
  CHECK(ApplyItemFactsFrame(Settings({{kGroupLevel, 0},
                                      {ro::net::kSettingAutolootAdd, 909},
                                      {ro::net::kSettingAutolootAdd, 512}}),
                            facts) == FrameOutcome::kIgnored);
  CHECK(facts.autoloot_told());
  CHECK(facts.Autoloots(909));
  CHECK(facts.Autoloots(512));
  // La resynchronisation : « efface », puis ce qui reste.
  CHECK(ApplyItemFactsFrame(
            Settings({{ro::net::kSettingAutolootAdd, 0}, {ro::net::kSettingAutolootAdd, 512}}),
            facts) == FrameOutcome::kIgnored);
  CHECK_FALSE(facts.Autoloots(909));
  CHECK(facts.Autoloots(512));
  // Une annonce sans aucun item vide la liste.
  CHECK(ApplyItemFactsFrame(Settings({{kGroupLevel, 0}}), facts) == FrameOutcome::kIgnored);
  CHECK_FALSE(facts.Autoloots(512));
}

TEST_CASE("Faits d'objet : ajout et retrait de la liste, octet pour octet", "[net][description]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  ro::net::RequestAutoloot(link, 909, true);
  const std::vector<std::uint8_t> added = {0x04, 0x0F, 10, 0, 23, 0, 0x8D, 0x03, 0, 0};
  CHECK(server.Receive(10) == added);
  ro::net::RequestAutoloot(link, 909, false);
  const std::vector<std::uint8_t> removed = {0x04, 0x0F, 10, 0, 24, 0, 0x8D, 0x03, 0, 0};
  CHECK(server.Receive(10) == removed);
}

TEST_CASE("Faits d'objet : un geste corrige la liste d'avance", "[net][description]") {
  ItemFacts facts;
  facts.TellAutoloot({512});
  facts.NoteAutoloot(909, true);
  facts.NoteAutoloot(909, true);
  CHECK(facts.Autoloots(909));
  facts.NoteAutoloot(512, false);
  CHECK_FALSE(facts.Autoloots(512));
  CHECK(facts.Autoloots(909));
}
