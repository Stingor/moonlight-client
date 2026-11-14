#include "ro/net/inventory.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/inventory.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyInventoryFrame;
using ro::net::FrameOutcome;
using ro::net::kItemListEquipOpcode;
using ro::net::kItemListNormalOpcode;
using ro::net::kOverweightPercentOpcode;
using ro::sim::InventoryItem;
using ro::sim::ObtainedItem;
using ro::sim::RefusedPickup;
using ro::sim::World;
using ro::test_support::FrameOf;

namespace {

// ── Un bâtisseur d'octets ────────────────────────────────────────────────────
//
// ⚠ ÉCRIT À LA MAIN, ET C'EST VOULU. Le générateur ne produit pas d'encodeur
// pour ces paquets-là : leur opcode ne se rattache à leur structure par aucune
// règle. Les fabriquer octet par octet fait donc mesurer la DISPOSITION DU FIL,
// et non l'accord de notre encodeur avec notre décodeur — qui se tromperaient
// ensemble.
class Wire {
 public:
  void U8(std::uint8_t value) { bytes_.push_back(value); }

  void U16(std::uint16_t value) {
    bytes_.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes_.push_back(static_cast<std::uint8_t>(value >> 8U));
  }

  void U32(std::uint32_t value) {
    U16(static_cast<std::uint16_t>(value & 0xFFFFU));
    U16(static_cast<std::uint16_t>(value >> 16U));
  }

  void Zeros(std::size_t count) { bytes_.insert(bytes_.end(), count, 0U); }

  // Réécrit la longueur annoncée, une fois tout posé.
  void SealLength() {
    const auto length = static_cast<std::uint16_t>(bytes_.size());
    bytes_[2] = static_cast<std::uint8_t>(length & 0xFFU);
    bytes_[3] = static_cast<std::uint8_t>(length >> 8U);
  }

  [[nodiscard]] std::vector<std::uint8_t> take() { return std::move(bytes_); }

 private:
  std::vector<std::uint8_t> bytes_;
};

// L'HORLOGE DE L'APPELANT, la même partout où l'instant n'est pas le sujet.
// `ApplyInventoryFrame` l'exige sans défaut : c'est elle qui date le journal des
// ramassages.
constexpr std::uint32_t kNowMs = 1000;

constexpr std::uint8_t kInventory = 0;
constexpr std::uint8_t kCart = 1;
// ⚠ DEUX VALEURS OUVRENT LE COFFRE, pas une : le stockage personnel et celui de
// la guilde. Seul le nom les distingue, et il vient du serveur.
constexpr std::uint8_t kStorage = 2;

// Une entrée ORDINAIRE, 34 octets.
void PutNormal(Wire& wire, std::uint16_t slot, std::uint32_t item_id, std::int16_t amount,
               std::uint8_t flags) {
  wire.U16(slot);
  wire.U32(item_id);
  wire.U8(0);  // type : soin
  wire.U16(static_cast<std::uint16_t>(amount));
  wire.U32(0);     // emplacement possible
  wire.Zeros(16);  // quatre cartes
  wire.U32(0);     // fin de location
  wire.U8(flags);
}

// Une entrée d'ÉQUIPEMENT, 68 octets. Le raffinement et le grade sont posés
// APRÈS les options, comme sur le fil.
struct EquipWire {
  std::uint16_t slot = 0;
  std::uint32_t item_id = 0;
  std::uint32_t cards[4] = {0, 0, 0, 0};
  std::uint16_t option_id = 0;
  std::int16_t option_value = 0;
  std::uint8_t refine = 0;
  std::uint8_t grade = 0;
  std::uint8_t flags = 0;
  std::uint8_t lying_option_count = 0;
};

void PutEquip(Wire& wire, const EquipWire& what) {
  wire.U16(what.slot);
  wire.U32(what.item_id);
  wire.U8(4);   // type : armure
  wire.U32(2);  // où elle PEUT se porter
  wire.U32(0);  // où elle EST portée
  for (const std::uint32_t card : what.cards) {
    wire.U32(card);
  }
  wire.U32(0);  // fin de location
  wire.U16(0);  // liaison à l'équipement
  wire.U16(7);  // apparence
  wire.U8(what.lying_option_count);
  // ⚠ LA PREMIÈRE CASE D'OPTION EST LAISSÉE VIDE : c'est la SECONDE qui porte le
  // bonus. Un décodeur qui s'arrêterait à la première vide n'en verrait aucune.
  wire.U16(0);
  wire.U16(0);
  wire.U8(0);
  wire.U16(what.option_id);
  wire.U16(static_cast<std::uint16_t>(what.option_value));
  wire.U8(0);
  wire.Zeros(15);  // les trois dernières options
  wire.U8(what.refine);
  wire.U8(what.grade);
  wire.U8(what.flags);
}

[[nodiscard]] std::vector<std::uint8_t> InventoryStart(std::uint8_t inv_type,
                                                       std::string_view name) {
  Wire wire;
  wire.U16(0x0B08);
  wire.U16(0);
  wire.U8(inv_type);
  for (const char letter : name) {
    wire.U8(static_cast<std::uint8_t>(letter));
  }
  wire.U8(0);  // le zéro terminal, compté dans la longueur
  wire.SealLength();
  return wire.take();
}

// UN SAC DÉJÀ REMPLI D'UNE PILE : le contenant s'ouvre, reçoit un objet
// ordinaire identifié dans une case, et la liste passe. C'est le point de
// départ de tout ce qui éprouve un ramassage, et il se refaisait à l'identique
// dans chaque cas.
void GiveBagOneStack(World& world, std::uint16_t slot, std::uint32_t item_id, std::int16_t amount) {
  REQUIRE(ApplyInventoryFrame(FrameOf(InventoryStart(kInventory, "")), world, kNowMs) ==
          FrameOutcome::kApplied);
  Wire normal;
  normal.U16(kItemListNormalOpcode);
  normal.U16(0);
  normal.U8(kInventory);
  PutNormal(normal, slot, item_id, amount, 0x01);
  normal.SealLength();
  REQUIRE(ApplyInventoryFrame(FrameOf(normal.take()), world, kNowMs) == FrameOutcome::kApplied);
}

[[nodiscard]] std::vector<std::uint8_t> InventoryEnd(std::uint8_t inv_type) {
  Wire wire;
  wire.U16(0x0B0B);
  wire.U8(inv_type);
  wire.U8(0);  // le drapeau, que le serveur laisse toujours à zéro
  return wire.take();
}

}  // namespace

TEST_CASE("Sac : les deux listes remplissent le MÊME contenant", "[net][inventaire]") {
  World world;
  REQUIRE(ApplyInventoryFrame(FrameOf(InventoryStart(kInventory, "")), world, kNowMs) ==
          FrameOutcome::kApplied);

  Wire normal;
  normal.U16(kItemListNormalOpcode);
  normal.U16(0);
  normal.U8(kInventory);
  PutNormal(normal, 2, 501, 30, 0x01);
  PutNormal(normal, 3, 502, 5, 0x01);
  normal.SealLength();
  CHECK(ApplyInventoryFrame(FrameOf(normal.take()), world, kNowMs) == FrameOutcome::kApplied);

  Wire equip;
  equip.U16(kItemListEquipOpcode);
  equip.U16(0);
  equip.U8(kInventory);
  PutEquip(equip, EquipWire{.slot = 4, .item_id = 2301, .refine = 7, .flags = 0x01});
  equip.SealLength();
  CHECK(ApplyInventoryFrame(FrameOf(equip.take()), world, kNowMs) == FrameOutcome::kApplied);

  REQUIRE(ApplyInventoryFrame(FrameOf(InventoryEnd(kInventory)), world, kNowMs) ==
          FrameOutcome::kApplied);

  // La seconde liste n'a PAS effacé la première : c'est tout l'objet du couple
  // ouverture/fermeture.
  CHECK(world.inventory().size() == 3);
  CHECK_FALSE(world.inventory().filling());
}

TEST_CASE("Sac : le raffinement se lit APRÈS les options", "[net][inventaire]") {
  // 🔴 LE PIÈGE DE CETTE VERSION. La structure du serveur déclare le raffinement
  // à DEUX endroits, sous deux conditions complémentaires ; celle qui vit en
  // 20250716 le place en fin d'entrée, derrière les vingt-cinq octets d'options.
  // Le lire à l'ancienne place rendrait une valeur d'option.
  World world;
  REQUIRE(ApplyInventoryFrame(FrameOf(InventoryStart(kInventory, "")), world, kNowMs) ==
          FrameOutcome::kApplied);

  Wire equip;
  equip.U16(kItemListEquipOpcode);
  equip.U16(0);
  equip.U8(kInventory);
  PutEquip(equip, EquipWire{.slot = 9,
                            .item_id = 1201,
                            .option_id = 42,
                            .option_value = 13,
                            .refine = 11,
                            .grade = 3,
                            .flags = 0x07});
  equip.SealLength();
  REQUIRE(ApplyInventoryFrame(FrameOf(equip.take()), world, kNowMs) == FrameOutcome::kApplied);

  const InventoryItem* item = world.inventory().Find(9);
  REQUIRE(item != nullptr);
  CHECK(item->refine_level == 11);
  CHECK(item->grade == 3);
  CHECK(item->identified);
  CHECK(item->damaged);
  CHECK(item->favorite);
  // La seconde case d'option porte le bonus, la première est vide : le compteur
  // annoncé par le serveur vaut zéro et ne doit pas servir à s'arrêter.
  CHECK_FALSE(item->options[0].filled());
  CHECK(item->options[1].filled());
  CHECK(item->options[1].id == 42);
  CHECK(item->options[1].value == 13);
}

TEST_CASE("Sac : c'est le contenant annoncé qui range, pas l'opcode", "[net][inventaire]") {
  World world;
  REQUIRE(ApplyInventoryFrame(FrameOf(InventoryStart(kCart, "")), world, kNowMs) ==
          FrameOutcome::kApplied);

  Wire normal;
  normal.U16(kItemListNormalOpcode);
  normal.U16(0);
  normal.U8(kCart);
  PutNormal(normal, 2, 501, 99, 0x01);
  normal.SealLength();
  REQUIRE(ApplyInventoryFrame(FrameOf(normal.take()), world, kNowMs) == FrameOutcome::kApplied);

  CHECK(world.cart().size() == 1);
  CHECK(world.inventory().size() == 0);
}

TEST_CASE("Sac : le nom du contenant est de longueur variable", "[net][inventaire]") {
  // ⚠ CE NOM EST LA SEULE CHOSE qui sépare le coffre personnel de celui de la
  // guilde : le serveur les annonce tous deux sous le même numéro de contenant.
  World world;
  REQUIRE(ApplyInventoryFrame(FrameOf(InventoryStart(kStorage, "Guild Storage")), world, kNowMs) ==
          FrameOutcome::kApplied);
  CHECK(world.storage().name() == "Guild Storage");

  REQUIRE(ApplyInventoryFrame(FrameOf(InventoryStart(2, "")), world, kNowMs) ==
          FrameOutcome::kApplied);
  CHECK(world.storage().name().empty());
}

TEST_CASE("Sac : un objet forgé n'a ni cartes ni emplacements", "[net][inventaire]") {
  World world;
  REQUIRE(ApplyInventoryFrame(FrameOf(InventoryStart(kInventory, "")), world, kNowMs) ==
          FrameOutcome::kApplied);

  Wire equip;
  equip.U16(kItemListEquipOpcode);
  equip.U16(0);
  equip.U8(kInventory);
  // Les quatre « cartes » portent alors l'identité du forgeron. La première est
  // petite, et c'est ce qui le dit.
  EquipWire forged{.slot = 5, .item_id = 1101};
  forged.cards[0] = 254;
  forged.cards[1] = 0;
  forged.cards[2] = 0x1234;
  forged.cards[3] = 0x0056;
  PutEquip(equip, forged);
  equip.SealLength();
  REQUIRE(ApplyInventoryFrame(FrameOf(equip.take()), world, kNowMs) == FrameOutcome::kApplied);

  const InventoryItem* item = world.inventory().Find(5);
  REQUIRE(item != nullptr);
  CHECK(ro::sim::ItemIsForged(*item));
  CHECK(ro::sim::ItemCardCount(*item) == 0);
}

TEST_CASE("Sac : un ramassage s'ajoute à la pile déjà là", "[net][inventaire]") {
  World world;
  GiveBagOneStack(world, 2, 501, 10);

  ro::proto::ZcItemPickupAck picked;
  picked.Index = 2;
  picked.count = 5;
  picked.nameid = 501;
  picked.IsIdentified = 1;
  picked.result = 0;
  REQUIRE(ApplyInventoryFrame(FrameOf(Encode(picked)), world, kNowMs) == FrameOutcome::kApplied);

  const InventoryItem* item = world.inventory().Find(2);
  REQUIRE(item != nullptr);
  CHECK(item->amount == 15);
}

TEST_CASE("Sac : un ramassage REFUSÉ ne range rien, mais entre au journal", "[net][inventaire]") {
  // ⚠ UN REFUS VIDE LE RESTE DU PAQUET : le serveur remet tout à zéro sauf la
  // case, la quantité et le motif. Le lire quand même rangerait un objet dont
  // l'identifiant est zéro — et le taire laisserait le joueur sans savoir
  // pourquoi son ramassage a échoué.
  //
  // Les codes sont ceux que CE serveur retourne (`e_additem_result`,
  // src/map/pc.hpp) : la valeur 3 n'y est rendue nulle part, et rien ici ne
  // l'attend.
  constexpr std::array<std::uint8_t, 6> kRefusalCodes = {1, 2, 4, 5, 6, 7};
  constexpr std::uint32_t kFirstRefusedMs = 4000;
  constexpr std::uint32_t kBetweenRefusalsMs = 10;
  constexpr std::uint16_t kEmptySlot = 8;
  constexpr std::uint16_t kFilledSlot = 2;
  constexpr std::int16_t kHeldAmount = 10;

  World world;
  GiveBagOneStack(world, kFilledSlot, 501, kHeldAmount);

  for (std::size_t rank = 0; rank < kRefusalCodes.size(); ++rank) {
    ro::proto::ZcItemPickupAck refused;
    refused.Index = kEmptySlot;
    refused.count = 1;
    refused.result = kRefusalCodes[rank];
    const auto refused_ms = static_cast<std::uint32_t>(kFirstRefusedMs + rank * kBetweenRefusalsMs);
    REQUIRE(ApplyInventoryFrame(FrameOf(Encode(refused)), world, refused_ms) ==
            FrameOutcome::kApplied);
  }

  // Chaque motif entre avec SON code et SON instant, dans l'ordre reçu.
  const std::span<const RefusedPickup> journal = world.inventory().refused();
  REQUIRE(journal.size() == kRefusalCodes.size());
  for (std::size_t rank = 0; rank < kRefusalCodes.size(); ++rank) {
    CHECK(journal[rank].result == kRefusalCodes[rank]);
    CHECK(journal[rank].refused_ms == kFirstRefusedMs + rank * kBetweenRefusalsMs);
    CHECK(journal[rank].seq == rank + 1);
  }
  CHECK(world.inventory().last_refused_seq() == kRefusalCodes.size());

  // Le sac n'a pas bougé : aucune case créée pour celle que le refus nomme,
  // aucune quantité changée sur celle qui existait.
  CHECK(world.inventory().size() == 1);
  CHECK(world.inventory().Find(kEmptySlot) == nullptr);
  REQUIRE(world.inventory().Find(kFilledSlot) != nullptr);
  CHECK(world.inventory().Find(kFilledSlot)->amount == kHeldAmount);

  // Et rien au journal des GAINS : un bandeau naîtrait sinon pour un objet
  // numéro zéro que le joueur n'a jamais eu.
  CHECK(world.inventory().obtained().empty());
  CHECK(world.inventory().last_obtained_seq() == 0);
}

TEST_CASE("Journal : la quantité RAMASSÉE, et non le total de la pile", "[net][inventaire]") {
  // 🔴 LE FAIT QUI DISPARAISSAIT AU RANGEMENT. Le serveur annonce un AJOUT, que
  // le rangement additionne à ce que la case portait déjà : après coup la pile
  // dit « quinze », et plus rien ne dit que cinq viennent d'entrer. Le bandeau
  // « <nom> - N obtained. » n'a que ce nombre-là à montrer.
  //
  // ⚠ ET L'INSTANT EST CELUI DE L'APPELANT : `sim` ne lit aucune horloge.
  constexpr std::uint32_t kPickedMs = 4242;
  World world;
  GiveBagOneStack(world, 2, 501, 10);

  ro::proto::ZcItemPickupAck picked;
  picked.Index = 2;
  picked.count = 5;
  picked.nameid = 501;
  picked.IsIdentified = 1;
  picked.result = 0;
  REQUIRE(ApplyInventoryFrame(FrameOf(Encode(picked)), world, kPickedMs) == FrameOutcome::kApplied);

  REQUIRE(world.inventory().obtained().size() == 1);
  const ObtainedItem& logged = world.inventory().obtained().back();
  CHECK(logged.picked.amount == 5);
  CHECK(logged.picked.item_id == 501);
  CHECK(logged.picked.client_slot == 2);
  CHECK(logged.obtained_ms == kPickedMs);
  CHECK(logged.seq == 1);
  CHECK(world.inventory().last_obtained_seq() == 1);

  // Et un succès n'entre PAS au journal des refus : la ligne rouge s'écrirait
  // par-dessus le bandeau de l'objet qui vient d'arriver.
  CHECK(world.inventory().refused().empty());
  CHECK(world.inventory().last_refused_seq() == 0);

  // La pile, elle, porte bien le total : le journal ne l'a pas détourné.
  REQUIRE(world.inventory().Find(2) != nullptr);
  CHECK(world.inventory().Find(2)->amount == 15);
}

TEST_CASE("Sac : ce qui sort retire la quantité dite", "[net][inventaire]") {
  World world;
  GiveBagOneStack(world, 2, 501, 10);

  ro::proto::ZcDeleteItemFromBody used;
  used.deleteType = 1;  // consommé par une compétence
  used.index = 2;
  used.count = 3;
  REQUIRE(ApplyInventoryFrame(FrameOf(Encode(used)), world, kNowMs) == FrameOutcome::kApplied);
  REQUIRE(world.inventory().Find(2) != nullptr);
  CHECK(world.inventory().Find(2)->amount == 7);

  ro::proto::ZcItemThrowAck thrown;
  thrown.index = 2;
  thrown.count = 7;
  REQUIRE(ApplyInventoryFrame(FrameOf(Encode(thrown)), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(world.inventory().Find(2) == nullptr);
}

TEST_CASE("Sac : le seuil de surcharge n'est pas le poids maximum", "[net][inventaire]") {
  World world;
  Wire wire;
  wire.U16(kOverweightPercentOpcode);
  wire.U32(70);
  REQUIRE(ApplyInventoryFrame(FrameOf(wire.take()), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(world.player_status().overweight_percent == 70);
  // Le poids, lui, n'a pas bougé : il vient d'un autre paquet.
  CHECK(world.player_status().max_weight == ro::sim::kUnknownStat);
}

namespace {

// Un sac d'un seul objet ordinaire, non favori.
[[nodiscard]] World SacDUnObjet(std::uint16_t slot) {
  World world;
  REQUIRE(ApplyInventoryFrame(FrameOf(InventoryStart(kInventory, "")), world, kNowMs) ==
          FrameOutcome::kApplied);
  Wire normal;
  normal.U16(kItemListNormalOpcode);
  normal.U16(0);
  normal.U8(kInventory);
  PutNormal(normal, slot, 501, 3, 0x01);
  normal.SealLength();
  REQUIRE(ApplyInventoryFrame(FrameOf(normal.take()), world, kNowMs) == FrameOutcome::kApplied);
  REQUIRE(ApplyInventoryFrame(FrameOf(InventoryEnd(kInventory)), world, kNowMs) ==
          FrameOutcome::kApplied);
  return world;
}

// L'onglet d'un objet, octet par octet : opcode, place, et le drapeau.
[[nodiscard]] std::vector<std::uint8_t> TabNotice(std::uint16_t slot, bool wire_flag) {
  Wire wire;
  wire.U16(ro::proto::ZcInventoryTab::kOpcode);
  wire.U16(slot);
  wire.U8(wire_flag ? 1U : 0U);
  return wire.take();
}

}  // namespace

TEST_CASE("Favori : le champ du paquet dit le CONTRAIRE de son nom", "[net][inventaire]") {
  // 🔴 CET ESSAI EXISTE POUR QU'ON NE << CORRIGE >> PAS L'INVERSION.
  //
  // Le serveur ecrit, pour 0x0908 :
  //     p.favorite = ( item.favorite == 1 ) ? false : true;
  // Un objet qui EST favori part donc avec le champ a FAUX. Et la
  // documentation de la requete le dit dans l'autre sens : << 0 = move item to
  // personal tab, 1 = move item to normal tab >>. Le champ nomme l'ONGLET VISE,
  // pas l'etat de favori.
  //
  // ⚠ ET CE N'EST PAS LA CONVENTION DU RESTE. Les listes d'objets portent le
  // meme fait EN DIRECT (PlaceETCTab), l'accuse de ramassage aussi, et la piece
  // jointe d'un courrier encore. Sur les trois emetteurs du champ dans le
  // serveur, celui-ci est le seul inverse : c'est l'exception, pas la regle.
  World world = SacDUnObjet(5);
  REQUIRE(world.inventory().Find(5) != nullptr);
  CHECK_FALSE(world.inventory().Find(5)->favorite);

  // FAUX sur le fil = il est dans l'onglet des favoris.
  CHECK(ApplyInventoryFrame(FrameOf(TabNotice(5, false)), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(world.inventory().Find(5)->favorite);

  // VRAI = il est revenu dans l'onglet normal.
  CHECK(ApplyInventoryFrame(FrameOf(TabNotice(5, true)), world, kNowMs) == FrameOutcome::kApplied);
  CHECK_FALSE(world.inventory().Find(5)->favorite);
}

TEST_CASE("Favori : une place vide n'invente pas d'objet", "[net][inventaire]") {
  // Le serveur ne devrait pas l'envoyer, mais une trame en retard sur un objet
  // vendu arriverait ainsi. Elle est IGNOREE et non APPLIQUEE : le releve doit
  // distinguer << rien a faire >> de << fait >>.
  World world = SacDUnObjet(5);
  CHECK(ApplyInventoryFrame(FrameOf(TabNotice(99, false)), world, kNowMs) ==
        FrameOutcome::kIgnored);
}

namespace {

// Une trame courte, octet par octet : opcode puis les champs annonces.
[[nodiscard]] std::vector<std::uint8_t> EquipAck(std::uint16_t slot, std::uint32_t location,
                                                 std::uint16_t sprite, std::uint8_t result) {
  Wire wire;
  wire.U16(ro::proto::ZcReqWearEquipAck::kOpcode);
  wire.U16(slot);
  wire.U32(location);
  wire.U16(sprite);
  wire.U8(result);
  return wire.take();
}

[[nodiscard]] std::vector<std::uint8_t> TakeOffAck(std::uint16_t slot, std::uint32_t location,
                                                   std::uint8_t result) {
  Wire wire;
  wire.U16(ro::proto::PacketUnequipitemAck::kOpcode);
  wire.U16(slot);
  wire.U32(location);
  wire.U8(result);
  return wire.take();
}

[[nodiscard]] std::vector<std::uint8_t> UseAck(std::uint16_t slot, std::uint32_t item_id,
                                               std::int16_t remaining, std::uint8_t result) {
  Wire wire;
  wire.U16(ro::proto::ZcUseItemAck::kOpcode);
  wire.U16(slot);
  wire.U32(item_id);
  wire.U32(0);  // l'identifiant du joueur, que nous ne lisons pas
  wire.U16(static_cast<std::uint16_t>(remaining));
  wire.U8(result);
  return wire.take();
}

}  // namespace

TEST_CASE("Accuses : equiper et retirer disent OUI par ZERO", "[net][inventaire]") {
  // 🔴 DEUX ACCUSES, DEUX INVERSIONS, ET AUCUN NOM DE CHAMP NE LE DIT. Les deux
  // s'appellent `result`. Pour equiper, l'enumeration du serveur pose OK = 0 a
  // cette version (ITEM_EQUIP_ACK_OK, clif.hpp). Pour retirer, le serveur
  // INVERSE lui-meme le booleen avant de l'ecrire, et sa note le dit :
  // << inversed for v2 v5 >>.
  //
  // Lire ces deux-la comme on lit l'accuse d'USAGE -- ou 1 veut dire oui --
  // ferait exactement l'inverse de ce que le serveur annonce.
  World world;
  REQUIRE(ApplyInventoryFrame(FrameOf(InventoryStart(kInventory, "")), world, kNowMs) ==
          FrameOutcome::kApplied);
  Wire equip;
  equip.U16(kItemListEquipOpcode);
  equip.U16(0);
  equip.U8(kInventory);
  PutEquip(equip, EquipWire{.slot = 4, .item_id = 2301, .flags = 0x01});
  equip.SealLength();
  REQUIRE(ApplyInventoryFrame(FrameOf(equip.take()), world, kNowMs) == FrameOutcome::kApplied);
  REQUIRE(ApplyInventoryFrame(FrameOf(InventoryEnd(kInventory)), world, kNowMs) ==
          FrameOutcome::kApplied);
  REQUIRE(world.inventory().Find(4) != nullptr);
  CHECK_FALSE(world.inventory().Find(4)->equipped());

  // Un REFUS ne touche a rien, et ne compte pas comme applique.
  CHECK(ApplyInventoryFrame(FrameOf(EquipAck(4, 0x0100, 0, 2)), world, kNowMs) ==
        FrameOutcome::kIgnored);
  CHECK_FALSE(world.inventory().Find(4)->equipped());

  CHECK(ApplyInventoryFrame(FrameOf(EquipAck(4, 0x0100, 42, 0)), world, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(world.inventory().Find(4)->worn_at == 0x0100);
  CHECK(world.inventory().Find(4)->view_id == 42);

  // Retirer : zero vaut oui la aussi.
  CHECK(ApplyInventoryFrame(FrameOf(TakeOffAck(4, 0x0100, 1)), world, kNowMs) ==
        FrameOutcome::kIgnored);
  CHECK(world.inventory().Find(4)->equipped());
  CHECK(ApplyInventoryFrame(FrameOf(TakeOffAck(4, 0x0100, 0)), world, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK_FALSE(world.inventory().Find(4)->equipped());
}

TEST_CASE("Accuses : l'usage annonce CE QUI RESTE, pas ce qui est parti", "[net][inventaire]") {
  // 🔴 LE PIEGE. Retrancher le nombre recu viderait la pile deux fois trop vite,
  // et l'ecart ne se verrait qu'au bout de plusieurs usages -- jamais au
  // premier, ou << reste 29 >> et << moins 29 >> donnent tous deux quelque chose
  // de plausible sur une pile de trente.
  World world;
  REQUIRE(ApplyInventoryFrame(FrameOf(InventoryStart(kInventory, "")), world, kNowMs) ==
          FrameOutcome::kApplied);
  Wire normal;
  normal.U16(kItemListNormalOpcode);
  normal.U16(0);
  normal.U8(kInventory);
  PutNormal(normal, 2, 501, 30, 0x01);
  normal.SealLength();
  REQUIRE(ApplyInventoryFrame(FrameOf(normal.take()), world, kNowMs) == FrameOutcome::kApplied);
  REQUIRE(ApplyInventoryFrame(FrameOf(InventoryEnd(kInventory)), world, kNowMs) ==
          FrameOutcome::kApplied);

  // Un refus : rien ne bouge. ⚠ ET CELUI-CI EST DIRECT, zero veut dire non.
  CHECK(ApplyInventoryFrame(FrameOf(UseAck(2, 501, 29, 0)), world, kNowMs) ==
        FrameOutcome::kIgnored);
  CHECK(world.inventory().Find(2)->amount == 30);

  CHECK(ApplyInventoryFrame(FrameOf(UseAck(2, 501, 29, 1)), world, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(world.inventory().Find(2)->amount == 29);

  // La derniere potion fait disparaitre la ligne.
  CHECK(ApplyInventoryFrame(FrameOf(UseAck(2, 501, 0, 1)), world, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(world.inventory().Find(2) == nullptr);
}

TEST_CASE("Sac : la capacite s'annonce en EXTENSION, pas en total", "[net][inventaire]") {
  // ⚠ Le serveur ecrit `expansionSize = inventory_slots - INVENTORY_BASE_SIZE`.
  // Prendre le nombre recu pour la capacite afficherait << 20/0 >> sur un
  // personnage sans extension.
  World world;
  CHECK(world.inventory().capacity() == 0);

  Wire etendu;
  etendu.U16(ro::proto::ZcExtendBodyitemSize::kOpcode);
  etendu.U16(0);  // aucune extension
  CHECK(ApplyInventoryFrame(FrameOf(etendu.take()), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(world.inventory().capacity() == 200);

  Wire plus;
  plus.U16(ro::proto::ZcExtendBodyitemSize::kOpcode);
  plus.U16(100);
  CHECK(ApplyInventoryFrame(FrameOf(plus.take()), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(world.inventory().capacity() == 300);
}

TEST_CASE("Charrette : elle s'en va avec son contenu ET sa capacite", "[net][inventaire]") {
  // ⚠ UNE CHARRETTE QU'ON REND N'EST PAS UN COFFRE QU'ON REFERME. Le coffre
  // existe encore ; la charrette, non. Garder sa capacite montrerait un
  // contenant que le personnage n'a plus.
  World world;
  Wire count_wire;
  count_wire.U16(ro::proto::ZcNotifyCartitemCountinfo::kOpcode);
  count_wire.U16(3);     // objets portes
  count_wire.U16(100);   // places
  count_wire.U32(450);   // charge
  count_wire.U32(8000);  // charge maximale
  REQUIRE(ApplyInventoryFrame(FrameOf(count_wire.take()), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(world.cart().capacity() == 100);
  CHECK(world.cart().weight() == 450);
  CHECK(world.cart().max_weight() == 8000);

  Wire rendue;
  rendue.U16(ro::proto::ZcCartoff::kOpcode);
  CHECK(ApplyInventoryFrame(FrameOf(rendue.take()), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(world.cart().capacity() == 0);
  CHECK(world.cart().max_weight() == 0);
}

TEST_CASE("Coffre : une SESSION, et son ouverture ne se devine pas du contenu",
          "[net][inventaire][coffre]") {
  // 🔴 CET ETAT DEBORDE DE SA PROPRE FENETRE : tant qu'un coffre est ouvert, le
  // serveur refuse SANS RIEN DIRE les echanges entre le sac et la charrette. Le
  // contenu ne suffit pas a le deviner -- un coffre ouvert et vide et un coffre
  // ferme ont tous deux zero objet.
  World world;
  CHECK_FALSE(world.storage_open());

  // Le sac se reliste sans rien ouvrir.
  REQUIRE(ApplyInventoryFrame(FrameOf(InventoryStart(kInventory, "")), world, kNowMs) ==
          FrameOutcome::kApplied);
  CHECK_FALSE(world.storage_open());

  REQUIRE(ApplyInventoryFrame(FrameOf(InventoryStart(kStorage, "Guild Storage")), world, kNowMs) ==
          FrameOutcome::kApplied);
  CHECK(world.storage_open());
  CHECK(world.storage().name() == "Guild Storage");
}

TEST_CASE("Coffre : la fermeture emporte le contenu, la charrette non",
          "[net][inventaire][coffre]") {
  World world;
  REQUIRE(ApplyInventoryFrame(FrameOf(InventoryStart(kStorage, "Storage")), world, kNowMs) ==
          FrameOutcome::kApplied);

  Wire list_wire;
  list_wire.U16(kItemListNormalOpcode);
  list_wire.U16(0);
  list_wire.U8(kStorage);
  PutNormal(list_wire, 1, 501, 5, 0);
  list_wire.SealLength();
  REQUIRE(ApplyInventoryFrame(FrameOf(list_wire.take()), world, kNowMs) == FrameOutcome::kApplied);
  REQUIRE(world.storage().size() == 1);

  Wire fermeture;
  fermeture.U16(ro::proto::ZcCloseStore::kOpcode);
  CHECK(ApplyInventoryFrame(FrameOf(fermeture.take()), world, kNowMs) == FrameOutcome::kApplied);
  CHECK_FALSE(world.storage_open());
  // ⚠ ON VIDE : le coffre existe toujours, mais son contenu ne nous est plus
  // annonce. Le garder afficherait une liste qui ne bouge plus et que rien ne
  // corrige.
  CHECK(world.storage().size() == 0);
}

TEST_CASE("Coffre : le changement de carte le ferme, et le serveur ne le dit pas",
          "[net][inventaire][coffre]") {
  // 🔴 unit_remove_map_ appelle storage_storage_quit, qui sauvegarde et remet
  // storage_flag a zero SANS envoyer sa trame : il suppose que le client ferme
  // de lui-meme. Sans cela, la fenetre resterait ouverte sur un coffre qui
  // n'existe plus.
  World world;
  REQUIRE(ApplyInventoryFrame(FrameOf(InventoryStart(kStorage, "Storage")), world, kNowMs) ==
          FrameOutcome::kApplied);
  REQUIRE(world.storage_open());

  world.EnterMap("prontera", 150, 150);
  CHECK_FALSE(world.storage_open());
  CHECK(world.storage().size() == 0);
}

TEST_CASE("Sac : LA VRAIE PORTE mene bien aux contenants", "[net][inventaire]") {
  // 🔴 L'ESSAI QUI MANQUAIT, ET QUI EXPLIQUE TOUT LE RESTE DE CE FICHIER.
  //
  // Tous les autres appellent `ApplyInventoryFrame` DIRECTEMENT. C'est commode
  // et c'est faux : le jeu, lui, passe par `ApplyFrame`, qui aiguille. Pendant
  // longtemps cet aiguillage n'a passe que HUIT des VINGT-QUATRE opcodes que le
  // fichier traite -- les seize autres etaient ecrits, eprouves ici meme, et
  // jamais atteints.
  //
  // Un opcode qui n'arrive nulle part ne casse rien : il ne fait rien. C'est la
  // panne la plus silencieuse qui soit, et seule la vraie porte la voit.
  //
  // ⚠ ON EPROUVE LES DEUX QUI ONT CASSE UNE FENETRE : la capacite de la
  // charrette, sans laquelle elle refuse de s'ouvrir, et la fermeture du coffre,
  // sans laquelle il ne se referme jamais.
  World world;

  Wire count_wire;
  count_wire.U16(ro::proto::ZcNotifyCartitemCountinfo::kOpcode);
  count_wire.U16(3);
  count_wire.U16(100);
  count_wire.U32(450);
  count_wire.U32(8000);
  CHECK(ro::net::ApplyFrame(FrameOf(count_wire.take()), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(world.cart().capacity() == 100);

  REQUIRE(ro::net::ApplyFrame(FrameOf(InventoryStart(kStorage, "Storage")), world, kNowMs) ==
          FrameOutcome::kApplied);
  REQUIRE(world.storage_open());

  Wire fermeture;
  fermeture.U16(ro::proto::ZcCloseStore::kOpcode);
  CHECK(ro::net::ApplyFrame(FrameOf(fermeture.take()), world, kNowMs) == FrameOutcome::kApplied);
  CHECK_FALSE(world.storage_open());
}

TEST_CASE("Sac : la porte passe AUSSI les accuses d'equipement", "[net][inventaire]") {
  // Les deux que le journal du jeu a fait remonter -- 0x0908 et 0x099A --, et
  // qui sortaient en << trame non traitee >> alors que le code pour les lire
  // etait la depuis le debut.
  World world;
  REQUIRE(ro::net::ApplyFrame(FrameOf(InventoryStart(kInventory, "")), world, kNowMs) ==
          FrameOutcome::kApplied);

  Wire list_wire;
  list_wire.U16(kItemListNormalOpcode);
  list_wire.U16(0);
  list_wire.U8(kInventory);
  PutNormal(list_wire, 5, 501, 1, 0);
  list_wire.SealLength();
  REQUIRE(ro::net::ApplyFrame(FrameOf(list_wire.take()), world, kNowMs) == FrameOutcome::kApplied);

  Wire favori;
  favori.U16(ro::proto::ZcInventoryTab::kOpcode);
  favori.U16(5);
  favori.U8(0);  // ⚠ ZERO VEUT DIRE FAVORI : le champ dit le contraire de son nom.
  CHECK(ro::net::ApplyFrame(FrameOf(favori.take()), world, kNowMs) == FrameOutcome::kApplied);

  // ⚠ CE QU'ON EPROUVE ICI EST LE PASSAGE, PAS L'EFFET. Le retrait porte sur une
  // piece qui n'est pas portee, donc le monde le RANGE en « ignore » -- et c'est
  // juste. Ce qu'il ne doit surtout pas etre, c'est « non traite » : cela voudrait
  // dire que la porte ne l'a pas passe, ce qui etait precisement le defaut.
  Wire retire;
  retire.U16(ro::proto::PacketUnequipitemAck::kOpcode);
  retire.U16(5);
  retire.U32(0);
  retire.U8(0);  // ⚠ ZERO VEUT DIRE OUI, sur celui-la comme sur son voisin.
  CHECK(ro::net::ApplyFrame(FrameOf(retire.take()), world, kNowMs) != FrameOutcome::kUnhandled);
}
