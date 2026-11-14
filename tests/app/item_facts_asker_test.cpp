// LE RYTHME DES DEMANDES à propos d'un objet — contre un serveur en boucle
// locale : trois demandes à l'ouverture, aucune tant qu'elles sont en route,
// une seconde chance au bout de trois secondes, les sources qui vieillissent en
// quinze et le script qui ne vieillit pas.

#include "items/item_facts_asker.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/repair_list.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "items/creator_names.h"
#include "items/repair_piece_name.h"
#include "support/loopback_server.h"

using moonclient::ItemFactsAsker;
using ro::test::LoopbackServer;

namespace {

constexpr std::uint32_t kJellopy = 909;
constexpr std::size_t kSourcesSize = ro::proto::CzMoonlightReqTechdata::kWireSize;
constexpr std::size_t kScriptsSize = ro::proto::CzMoonlightReqItemscript::kWireSize;
// Assez pour les trois demandes d'un coup.
constexpr std::size_t kThreeAsks = 2 * kSourcesSize + kScriptsSize;
// Un silence : rien ne doit arriver dans ce délai.
constexpr int kQuietMs = 100;

struct Bench {
  LoopbackServer server;
  ro::net::Connection link;
  ItemFactsAsker asker;

  Bench() : link(Open(server)) {}

  static ro::net::Connection Open(LoopbackServer& server) {
    REQUIRE(server.listening());
    auto link = ro::net::Connection::Open("127.0.0.1", server.port());
    REQUIRE(link.has_value());
    REQUIRE(server.Accept());
    return std::move(*link);
  }

  // Les réponses du serveur, posées à la main.
  void Answer() {
    asker.facts().TellSources(kJellopy, ro::sim::ItemSourceScope::kOrdinary, {});
    asker.facts().TellSources(kJellopy, ro::sim::ItemSourceScope::kBoss, {});
    asker.facts().TellScripts(kJellopy, {});
  }
};

}  // namespace

TEST_CASE("Faits d'objet : trois demandes à l'ouverture, pas une de plus en route",
          "[app][description]") {
  Bench bench;
  bench.asker.Ask(bench.link, true, kJellopy, 1000);
  CHECK(bench.server.Receive(kThreeAsks).size() == kThreeAsks);
  bench.asker.Ask(bench.link, true, kJellopy, 1000 + 2999);
  CHECK(bench.server.Receive(kSourcesSize, kQuietMs).empty());
  // Sans réponse au bout de trois secondes : la demande se rejoue.
  bench.asker.Ask(bench.link, true, kJellopy, 1000 + 3000);
  CHECK(bench.server.Receive(kThreeAsks).size() == kThreeAsks);
}

TEST_CASE("Faits d'objet : les sources vieillissent, le script non", "[app][description]") {
  Bench bench;
  bench.asker.Ask(bench.link, true, kJellopy, 1000);
  (void)bench.server.Receive(kThreeAsks);
  bench.Answer();
  bench.asker.Ask(bench.link, true, kJellopy, 2000);  // la réponse est notée ici
  bench.asker.Ask(bench.link, true, kJellopy, 2000 + 14999);
  CHECK(bench.server.Receive(kSourcesSize, kQuietMs).empty());
  bench.asker.Ask(bench.link, true, kJellopy, 2000 + 15000);
  // Les deux portées de sources, et elles seules.
  CHECK(bench.server.Receive(2 * kSourcesSize).size() == 2 * kSourcesSize);
  CHECK(bench.server.Receive(kScriptsSize, kQuietMs).empty());
}

TEST_CASE("Faits d'objet : rouvrir la description redemande les sources", "[app][description]") {
  Bench bench;
  bench.asker.Ask(bench.link, true, kJellopy, 1000);
  (void)bench.server.Receive(kThreeAsks);
  bench.Answer();
  bench.asker.Ask(bench.link, true, kJellopy, 1100);
  bench.asker.Ask(bench.link, false, kJellopy, 1200);
  bench.asker.Ask(bench.link, true, kJellopy, 1300);
  CHECK(bench.server.Receive(2 * kSourcesSize).size() == 2 * kSourcesSize);
  CHECK(bench.server.Receive(kScriptsSize, kQuietMs).empty());
}

TEST_CASE("Faits d'objet : « Rafraichir » oublie et redemande", "[app][description]") {
  Bench bench;
  bench.asker.Ask(bench.link, true, kJellopy, 1000);
  (void)bench.server.Receive(kThreeAsks);
  bench.Answer();
  bench.asker.Ask(bench.link, true, kJellopy, 1100);
  bench.asker.Forget(kJellopy, ItemFactsAsker::Kind::kBoss);
  CHECK(bench.asker.facts().SourcesOf(kJellopy, ro::sim::ItemSourceScope::kBoss) == nullptr);
  bench.asker.Ask(bench.link, true, kJellopy, 1200);
  CHECK(bench.server.Receive(kSourcesSize).size() == kSourcesSize);
  CHECK(bench.server.Receive(kSourcesSize, kQuietMs).empty());
}

TEST_CASE("Faits d'objet : un forgeron se demande une fois, fenêtre fermée comprise",
          "[app][description]") {
  constexpr std::size_t kCreatorAskSize = 6;
  constexpr std::uint32_t kSmith = 150001;
  Bench bench;
  bench.asker.WantCreator(kSmith);
  bench.asker.Ask(bench.link, false, 0, 1000);
  CHECK(bench.server.Receive(kCreatorAskSize).size() == kCreatorAskSize);
  // Voulu encore, mais en route : rien ne repart.
  bench.asker.WantCreator(kSmith);
  bench.asker.Ask(bench.link, false, 0, 1000 + 2999);
  CHECK(bench.server.Receive(kCreatorAskSize, kQuietMs).empty());
  // Sans réponse au bout de trois secondes : la demande se rejoue.
  bench.asker.WantCreator(kSmith);
  bench.asker.Ask(bench.link, false, 0, 1000 + 3000);
  CHECK(bench.server.Receive(kCreatorAskSize).size() == kCreatorAskSize);
  // Nommé : on ne le redemande plus, même voulu.
  bench.asker.facts().TellCreator(kSmith, "Forgeron");
  bench.asker.WantCreator(kSmith);
  bench.asker.Ask(bench.link, false, 0, 1000 + 9000);
  CHECK(bench.server.Receive(kCreatorAskSize, kQuietMs).empty());
}

TEST_CASE("Forgeron : la poignée pose le nom, ou le demande", "[app][description]") {
  constexpr std::size_t kCreatorAskSize = 6;
  constexpr std::uint32_t kSmith = 150001;
  constexpr std::uint32_t kForgeMark = 0xFF;
  Bench bench;
  const moonclient::CreatorNames creators(&bench.asker);
  ro::vfs::ItemNameFacts facts;
  facts.cards = {kForgeMark, 0, kSmith & 0xFFFFU, kSmith >> 16U};
  // Inconnu : la demande part au prochain passage, et le nom reste vide.
  CHECK(creators.Fill(facts));
  CHECK(facts.creator_name.empty());
  bench.asker.Ask(bench.link, false, 0, 1000);
  CHECK(bench.server.Receive(kCreatorAskSize).size() == kCreatorAskSize);
  // Nommé : il se pose, et plus rien ne se demande.
  bench.asker.facts().TellCreator(kSmith, "Forgeron");
  CHECK_FALSE(creators.Fill(facts));
  CHECK(facts.creator_name == "Forgeron");
  // Des cartes, pas un forgeron : rien à faire.
  ro::vfs::ItemNameFacts carded;
  carded.cards = {4001, 0, 0, 0};
  CHECK_FALSE(creators.Fill(carded));
}

TEST_CASE("Forgeron : le journal attend sa réponse trois secondes au plus", "[app][description]") {
  constexpr std::uint32_t kSmith = 150001;
  constexpr std::uint32_t kForgeMark = 0xFF;
  constexpr std::uint32_t kPickedAt = 50000;
  moonclient::ItemFactsAsker asker;
  const moonclient::CreatorNames creators(&asker);
  ro::sim::Inventory bag;
  ro::sim::InventoryItem forged;
  forged.item_id = 1201;
  forged.amount = 1;
  forged.cards = {kForgeMark, 0, kSmith & 0xFFFFU, kSmith >> 16U};
  bag.RecordObtained(forged, kPickedAt);
  CHECK(moonclient::AwaitsCreator(bag, 0, creators, kPickedAt + 100));
  // Passé le délai, on écrit « Sans nom » plutôt que de se taire.
  CHECK_FALSE(moonclient::AwaitsCreator(bag, 0, creators, kPickedAt + 3000));
  // Déjà vu : rien n'attend.
  CHECK_FALSE(moonclient::AwaitsCreator(bag, bag.last_obtained_seq(), creators, kPickedAt + 100));
  // Nommé : rien n'attend non plus.
  asker.facts().TellCreator(kSmith, "Forgeron");
  CHECK_FALSE(moonclient::AwaitsCreator(bag, 0, creators, kPickedAt + 100));
}

TEST_CASE("Forgeron : la liste de réparation nomme l'arme forgée", "[app][description]") {
  constexpr std::size_t kCreatorAskSize = 6;
  constexpr std::uint32_t kSmith = 150001;
  constexpr std::uint32_t kForgeMark = 0xFF;
  constexpr std::uint32_t kKnife = 1201;
  constexpr int kWeaponLevel = 1;
  Bench bench;
  const moonclient::CreatorNames creators(&bench.asker);
  ro::vfs::ItemTable items;
  items.Put(kKnife, ro::vfs::ItemEntry{.name = "Knife", .card_slots = 3});
  ro::vfs::ItemWords words;
  words.nameless_creator = "Sans nom";
  ro::sim::RepairableItem forged;
  forged.item_id = kKnife;
  forged.cards = {kForgeMark, 0, kSmith & 0xFFFFU, kSmith >> 16U};

  // Inconnu : « Sans nom », et la demande part au prochain passage.
  const std::string waiting =
      moonclient::RepairPieceName(forged, kWeaponLevel, items, words, creators);
  CHECK(waiting.find("Sans nom") != std::string::npos);
  CHECK(waiting.find("Knife") != std::string::npos);
  bench.asker.Ask(bench.link, false, 0, 1000);
  CHECK(bench.server.Receive(kCreatorAskSize).size() == kCreatorAskSize);

  // La réponse arrivée : le forgeron à sa place.
  bench.asker.facts().TellCreator(kSmith, "Forgeron");
  const std::string named =
      moonclient::RepairPieceName(forged, kWeaponLevel, items, words, creators);
  CHECK(named.find("Forgeron") != std::string::npos);
  CHECK(named.find("Sans nom") == std::string::npos);

  // Une pièce ordinaire ne porte pas de forgeron.
  ro::sim::RepairableItem plain;
  plain.item_id = kKnife;
  CHECK(moonclient::RepairPieceName(plain, kWeaponLevel, items, words, creators).find("Sans nom") ==
        std::string::npos);
}
