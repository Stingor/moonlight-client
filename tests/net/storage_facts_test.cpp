// CE QUE LE SERVEUR DIT DES OBJETS DU COFFRE, décodé.
//
// 🔴 CE BANC PORTE TROIS CHOSES QU'AUCUNE SESSION NE MONTRERAIT VITE :
//
//   · une ENTRÉE fait quinze octets, dans l'ordre que `clif.cpp` écrit —
//     `[id:4][sell:4][subtype:1][equip:4][slots:2]`. Un seul octet de décalage
//     et tous les prix glissent d'un objet à l'autre, ce qui se lit comme une
//     base de serveur bizarre plutôt que comme un défaut de lecture ;
//   · le même identifiant REVIENT — le serveur envoie une entrée par PLACE du
//     coffre — et la table n'en garde qu'une ;
//   · une trame TRONQUÉE annonce plus qu'elle ne porte : on lit ce qui tient ;
//   · une liste peut tenir en PLUSIEURS trames : seule la première repart de
//     zéro, les suivantes s'ajoutent.

#include "ro/net/storage_facts.h"

#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/map_packets.h"
#include "ro/sim/storage_facts.h"
#include "ro/sim/world.h"

#include "support/wire_bytes.h"

using ro::net::ApplyStorageFactsFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::StorageItemFacts;
using ro::sim::World;
using ro::test_support::PutU16;
using ro::test_support::PutU32;

namespace {

// Le drapeau d'une trame qui S'AJOUTE : la suite d'une liste, ou un objet entré.
constexpr std::uint8_t kFollowingFrame = 0;

// ⚠ SANS DRAPEAUX, la trame est celle d'un serveur qui ne découpe pas : rien
// ne suit les entrées.
[[nodiscard]] Frame FactsFrame(const std::vector<StorageItemFacts>& told, int announced = -1,
                               std::optional<std::uint8_t> flags = std::nullopt) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcMoonlightStoragePrices::kOpcode);
  PutU16(bytes, 0);  // la longueur, réécrite plus bas
  PutU16(bytes, static_cast<std::uint16_t>(announced < 0 ? told.size()
                                                         : static_cast<std::size_t>(announced)));
  for (const StorageItemFacts& facts : told) {
    PutU32(bytes, facts.item_id);
    PutU32(bytes, facts.sell_price);
    bytes.push_back(facts.subtype);
    PutU32(bytes, facts.equip_mask);
    PutU16(bytes, facts.card_slots);
  }
  if (flags.has_value()) {
    bytes.push_back(*flags);
  }
  bytes[2] = static_cast<std::uint8_t>(bytes.size() & 0xFF);
  bytes[3] = static_cast<std::uint8_t>(bytes.size() >> 8);
  return Frame{.opcode = ro::proto::ZcMoonlightStoragePrices::kOpcode, .bytes = bytes};
}

}  // namespace

TEST_CASE("Faits du coffre : les cinq champs d'une entrée tombent à leur place", "[net][coffre]") {
  // 🔴 L'ORDRE EST CELUI DE `ZC_MOONLIGHT_STORAGE_PRICES`, et il n'a rien
  // d'évident : l'octet du sous-type est COINCÉ entre deux champs de quatre,
  // donc rien n'est aligné. Un décalage d'un seul octet donnerait des prix
  // plausibles sur les mauvais objets.
  const std::vector<StorageItemFacts> told = {
      StorageItemFacts{
          .item_id = 1201, .sell_price = 25, .subtype = 1, .equip_mask = 0x000002, .card_slots = 3},
      StorageItemFacts{
          .item_id = 501, .sell_price = 10, .subtype = 0, .equip_mask = 0, .card_slots = 0},
  };
  World world;
  REQUIRE(ApplyStorageFactsFrame(FactsFrame(told), world) == FrameOutcome::kApplied);

  const ro::sim::StorageItemFacts* const knife = world.storage_facts().Of(1201);
  REQUIRE(knife != nullptr);
  CHECK(knife->sell_price == 25);
  CHECK(knife->subtype == 1);
  CHECK(knife->equip_mask == 0x000002);
  CHECK(knife->card_slots == 3);

  const ro::sim::StorageItemFacts* const potion = world.storage_facts().Of(501);
  REQUIRE(potion != nullptr);
  CHECK(potion->sell_price == 10);
  CHECK(potion->equip_mask == 0);

  // Ce dont le serveur n'a rien dit ne se devine pas.
  CHECK(world.storage_facts().Of(909) == nullptr);
}

TEST_CASE("Faits du coffre : deux piles du même objet ne font qu'une entrée", "[net][coffre]") {
  // ⚠ LE SERVEUR ENVOIE UNE ENTRÉE PAR PLACE, pas par objet : deux piles de
  // potions en font deux, et elles portent la même chose. Les garder toutes
  // ferait répondre l'une ou l'autre selon l'humeur du tri.
  const std::vector<StorageItemFacts> told = {
      StorageItemFacts{.item_id = 501, .sell_price = 10},
      StorageItemFacts{.item_id = 501, .sell_price = 10},
      StorageItemFacts{.item_id = 502, .sell_price = 20},
  };
  World world;
  REQUIRE(ApplyStorageFactsFrame(FactsFrame(told), world) == FrameOutcome::kApplied);
  CHECK(world.storage_facts().size() == 2);
  REQUIRE(world.storage_facts().Of(501) != nullptr);
  CHECK(world.storage_facts().Of(501)->sell_price == 10);
}

TEST_CASE("Faits du coffre : une trame tronquée ne lit que ce qui tient", "[net][coffre]") {
  // Le compte est cru, mais pas sur parole : une trame qui annonce dix entrées
  // et n'en porte qu'une ne doit pas lire neuf fois au-delà de sa fin.
  const std::vector<StorageItemFacts> told = {StorageItemFacts{.item_id = 501, .sell_price = 10}};
  World world;
  REQUIRE(ApplyStorageFactsFrame(FactsFrame(told, /*announced=*/10), world) ==
          FrameOutcome::kApplied);
  CHECK(world.storage_facts().size() == 1);
}

TEST_CASE("Faits du coffre : une table vide efface celle d'avant", "[net][coffre]") {
  // ⚠ UN COFFRE VIDE EN ENVOIE UNE, et les prix du coffre précédent ne doivent
  // pas lui survivre : ce sont deux contenus différents.
  World world;
  REQUIRE(ApplyStorageFactsFrame(FactsFrame({StorageItemFacts{.item_id = 501, .sell_price = 10}}),
                                 world) == FrameOutcome::kApplied);
  REQUIRE_FALSE(world.storage_facts().empty());

  REQUIRE(ApplyStorageFactsFrame(FactsFrame({}), world) == FrameOutcome::kApplied);
  CHECK(world.storage_facts().empty());
  CHECK(world.storage_facts().Of(501) == nullptr);
}

TEST_CASE("Faits du coffre : une liste en plusieurs trames se cumule", "[net][coffre]") {
  // 🔴 AU-DELÀ DE 2 183 OBJETS, LA LISTE NE TIENT PLUS DANS UNE TRAME : le
  // serveur la coupe, et seule la première trame dit « repartir de zéro ». Une
  // trame qui remplacerait la table n'en laisserait que la dernière tranche.
  World world;
  REQUIRE(ApplyStorageFactsFrame(FactsFrame({StorageItemFacts{.item_id = 501, .sell_price = 10},
                                             StorageItemFacts{.item_id = 1201, .sell_price = 25}},
                                            -1, ro::net::kStorageFactsFirstFrame),
                                 world) == FrameOutcome::kApplied);
  // La suite reprend un identifiant déjà dit : une autre pile du même objet.
  REQUIRE(ApplyStorageFactsFrame(FactsFrame({StorageItemFacts{.item_id = 909, .sell_price = 3},
                                             StorageItemFacts{.item_id = 501, .sell_price = 10}},
                                            -1, kFollowingFrame),
                                 world) == FrameOutcome::kApplied);

  CHECK(world.storage_facts().size() == 3);
  REQUIRE(world.storage_facts().Of(501) != nullptr);
  CHECK(world.storage_facts().Of(501)->sell_price == 10);
  REQUIRE(world.storage_facts().Of(1201) != nullptr);
  CHECK(world.storage_facts().Of(1201)->sell_price == 25);
  REQUIRE(world.storage_facts().Of(909) != nullptr);
  CHECK(world.storage_facts().Of(909)->sell_price == 3);
}

TEST_CASE("Faits du coffre : un objet entré dans le coffre ouvert s'ajoute", "[net][coffre]") {
  // ⚠ LE SERVEUR EN ENVOIE UNE TRAME À LUI SEUL, sans le drapeau de tête : elle
  // complète la table de l'ouverture au lieu de l'écraser.
  World world;
  REQUIRE(ApplyStorageFactsFrame(FactsFrame({StorageItemFacts{.item_id = 501, .sell_price = 10}},
                                            -1, ro::net::kStorageFactsFirstFrame),
                                 world) == FrameOutcome::kApplied);
  REQUIRE(ApplyStorageFactsFrame(FactsFrame({StorageItemFacts{.item_id = 607, .sell_price = 1250}},
                                            -1, kFollowingFrame),
                                 world) == FrameOutcome::kApplied);
  CHECK(world.storage_facts().size() == 2);
  CHECK(world.storage_facts().Of(501) != nullptr);
  CHECK(world.storage_facts().Of(607) != nullptr);
}

TEST_CASE("Faits du coffre : une nouvelle ouverture repart de zéro", "[net][coffre]") {
  // ⚠ LE DRAPEAU DE TÊTE EFFACE TOUT, cumul compris : les prix d'un coffre ne
  // survivent pas à l'ouverture d'un autre. Et une ouverture sur un coffre vide
  // envoie quand même sa trame, vide.
  World world;
  REQUIRE(ApplyStorageFactsFrame(FactsFrame({StorageItemFacts{.item_id = 501, .sell_price = 10}},
                                            -1, ro::net::kStorageFactsFirstFrame),
                                 world) == FrameOutcome::kApplied);
  REQUIRE(ApplyStorageFactsFrame(
              FactsFrame({StorageItemFacts{.item_id = 909, .sell_price = 3}}, -1, kFollowingFrame),
              world) == FrameOutcome::kApplied);
  REQUIRE(world.storage_facts().size() == 2);

  REQUIRE(ApplyStorageFactsFrame(FactsFrame({StorageItemFacts{.item_id = 1201, .sell_price = 25}},
                                            -1, ro::net::kStorageFactsFirstFrame),
                                 world) == FrameOutcome::kApplied);
  CHECK(world.storage_facts().size() == 1);
  CHECK(world.storage_facts().Of(501) == nullptr);
  CHECK(world.storage_facts().Of(909) == nullptr);
  CHECK(world.storage_facts().Of(1201) != nullptr);

  REQUIRE(ApplyStorageFactsFrame(FactsFrame({}, -1, ro::net::kStorageFactsFirstFrame), world) ==
          FrameOutcome::kApplied);
  CHECK(world.storage_facts().empty());
}

TEST_CASE("Faits du coffre : chaque trame change la génération", "[net][coffre]") {
  // C'est elle que la vue du coffre compare pour savoir si ses cases sont à
  // refaire : une suite qui ne la changerait pas laisserait des prix manquants.
  World world;
  const std::uint64_t before = world.storage_facts().generation();
  REQUIRE(ApplyStorageFactsFrame(FactsFrame({StorageItemFacts{.item_id = 501, .sell_price = 10}},
                                            -1, ro::net::kStorageFactsFirstFrame),
                                 world) == FrameOutcome::kApplied);
  const std::uint64_t first = world.storage_facts().generation();
  CHECK(first != before);
  REQUIRE(ApplyStorageFactsFrame(
              FactsFrame({StorageItemFacts{.item_id = 909, .sell_price = 3}}, -1, kFollowingFrame),
              world) == FrameOutcome::kApplied);
  CHECK(world.storage_facts().generation() != first);
}

TEST_CASE("Faits du coffre : une trame tronquée ne prend pas un reste pour un drapeau",
          "[net][coffre]") {
  // Une trame qui annonce deux entrées et n'en porte qu'une, plus un octet : cet
  // octet est le début de l'entrée manquante, pas la fin de trame. Elle se lit
  // comme une trame sans drapeau — elle remplace.
  World world;
  REQUIRE(ApplyStorageFactsFrame(FactsFrame({StorageItemFacts{.item_id = 501, .sell_price = 10}},
                                            -1, ro::net::kStorageFactsFirstFrame),
                                 world) == FrameOutcome::kApplied);
  REQUIRE(ApplyStorageFactsFrame(FactsFrame({StorageItemFacts{.item_id = 909, .sell_price = 3}},
                                            /*announced=*/2, kFollowingFrame),
                                 world) == FrameOutcome::kApplied);
  CHECK(world.storage_facts().size() == 1);
  CHECK(world.storage_facts().Of(909) != nullptr);
}
