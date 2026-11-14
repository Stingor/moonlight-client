// Les couleurs que les joueurs ont composées, telles que le lot les porte.
//
// Ce qui se vérifie ici, c'est la lecture d'une liste que le générateur ne
// modélise pas : seul l'en-tête est engendré, les entrées se lisent à la main.
// Une disposition fausse poserait des couleurs tirées d'octets mal découpés,
// et rien ne le dirait — le serveur n'attend aucune réponse à ce paquet.

#include "ro/net/player_styles.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/style.h"
#include "ro/sim/world.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyPlayerStylesFrame;
using ro::net::ApplyStyleOpenFrame;
using ro::net::Connection;
using ro::net::FrameOutcome;
using ro::net::SendPlayerStyle;
using ro::net::SendPlayerStyleClear;
using ro::net::SendPlayerStyleClearAll;
using ro::sim::kStyleRampCount;
using ro::sim::StyleAdjust;
using ro::sim::StyleRecipe;
using ro::sim::World;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU32;

namespace {

// La disposition déclarée par le serveur :
//   [packetType:2][packetLength:2][count:2] puis count × 56 octets
//   {gid:4, version:1, drapeaux:1, corps:4, palette:2, cheveux:2, coupe:2,
//    réglages: 8 × {teinte:2, sat:1, lum:1, absolu:1}}
constexpr std::uint16_t kOpcode = 0x0F27;
constexpr std::uint8_t kWireVersion = 7;
constexpr std::uint8_t kFlagClear = 0x01;
constexpr std::uint8_t kFlagFallback = 0x04;
constexpr std::size_t kEntrySize = 56;

// Une entrée, écrite comme le serveur l'écrit.
struct Entry {
  std::uint32_t gid = 0;
  std::uint8_t version = kWireVersion;
  std::uint8_t flags = 0;
  std::uint32_t body_key = 0;
  std::int16_t body_palette = -1;
  std::int16_t hair_palette = -1;
  std::int16_t hair_style = -1;
  // La teinte du PREMIER réglage ; les sept autres restent neutres. Assez pour
  // reconnaître une entrée d'une autre.
  std::int16_t first_hue = 0;
  bool first_absolute = false;
};

void PutEntry(std::vector<std::uint8_t>& into, const Entry& entry) {
  const std::size_t before = into.size();
  PutU32(into, entry.gid);
  into.push_back(entry.version);
  into.push_back(entry.flags);
  PutU32(into, entry.body_key);
  PutU16(into, static_cast<std::uint16_t>(entry.body_palette));
  PutU16(into, static_cast<std::uint16_t>(entry.hair_palette));
  PutU16(into, static_cast<std::uint16_t>(entry.hair_style));
  for (std::size_t ramp = 0; ramp < kStyleRampCount; ++ramp) {
    PutU16(into, static_cast<std::uint16_t>(ramp == 0 ? entry.first_hue : 0));
    into.push_back(0);  // saturation
    into.push_back(0);  // luminosité
    into.push_back(ramp == 0 && entry.first_absolute ? 1 : 0);
  }
  REQUIRE(into.size() - before == kEntrySize);
}

// Le lot entier. `announced` remplace le compte écrit dans l'en-tête, pour
// éprouver une trame qui ment sur ce qu'elle porte.
[[nodiscard]] std::vector<std::uint8_t> Batch(const std::vector<Entry>& entries,
                                              int announced = -1) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, kOpcode);
  PutU16(bytes, 0);  // la longueur, réécrite plus bas
  PutU16(bytes, static_cast<std::uint16_t>(announced < 0 ? entries.size()
                                                         : static_cast<std::size_t>(announced)));
  for (const Entry& entry : entries) {
    PutEntry(bytes, entry);
  }
  bytes[2] = static_cast<std::uint8_t>(bytes.size() & 0xFFU);
  bytes[3] = static_cast<std::uint8_t>(bytes.size() >> 8U);
  return bytes;
}

[[nodiscard]] FrameOutcome Apply(World& world, std::vector<std::uint8_t> bytes) {
  return ApplyPlayerStylesFrame(ro::net::Frame{.opcode = kOpcode, .bytes = std::move(bytes)},
                                world);
}

constexpr std::uint32_t kAlice = 2000001;
constexpr std::uint32_t kBob = 2000002;
constexpr std::uint32_t kOnFoot = 0x11111111;
constexpr std::uint32_t kMounted = 0x22222222;

}  // namespace

TEST_CASE("Styles : un lot range une variante par corps", "[net][style]") {
  World world;
  CHECK(
      Apply(
          world,
          Batch({
              Entry{.gid = kAlice, .body_key = kOnFoot, .body_palette = 12, .first_hue = 120},
              Entry{.gid = kAlice, .body_key = kMounted, .first_hue = -40, .first_absolute = true},
              Entry{.gid = kBob, .body_key = kOnFoot, .first_hue = 7},
          })) == FrameOutcome::kApplied);

  const ro::sim::StyleVariants* alice = world.styles().Find(kAlice);
  REQUIRE(alice != nullptr);
  REQUIRE(alice->by_body.size() == 2);
  const StyleRecipe* on_foot = alice->For(kOnFoot);
  REQUIRE(on_foot != nullptr);
  CHECK(on_foot->body_palette == 12);
  CHECK(on_foot->ramps[0].hue == 120);
  CHECK_FALSE(on_foot->ramps[0].absolute);
  const StyleRecipe* mounted = alice->For(kMounted);
  REQUIRE(mounted != nullptr);
  CHECK(mounted->ramps[0].hue == -40);
  CHECK(mounted->ramps[0].absolute);
  // Les sept autres rampes sont restées neutres : la lecture ne décale pas.
  CHECK(mounted->ramps[kStyleRampCount - 1].hue == 0);

  REQUIRE(world.styles().Find(kBob) != nullptr);
  CHECK(world.styles().Find(kBob)->by_body.size() == 1);
}

TEST_CASE("Styles : un corps sans variante reprend celle de repli", "[net][style]") {
  World world;
  CHECK(Apply(world, Batch({Entry{.gid = kAlice, .body_key = kOnFoot, .first_hue = 90}})) ==
        FrameOutcome::kApplied);

  const ro::sim::StyleVariants* alice = world.styles().Find(kAlice);
  REQUIRE(alice != nullptr);
  // 🔴 SANS CE REPLI, MONTER SUR UNE MONTURE RENDRAIT L'APPARENCE NATIVE : un
  // corps jamais teint n'a pas de variante.
  const StyleRecipe* mounted = alice->For(kMounted);
  REQUIRE(mounted != nullptr);
  CHECK(mounted->ramps[0].hue == 90);
}

TEST_CASE("Styles : le drapeau de repli désigne la variante à reprendre", "[net][style]") {
  World world;
  CHECK(
      Apply(
          world,
          Batch({
              Entry{.gid = kAlice, .body_key = kOnFoot, .first_hue = 90},
              Entry{.gid = kAlice, .flags = kFlagFallback, .body_key = kMounted, .first_hue = 200},
          })) == FrameOutcome::kApplied);

  const ro::sim::StyleVariants* alice = world.styles().Find(kAlice);
  REQUIRE(alice != nullptr);
  const StyleRecipe* other = alice->For(0x33333333);
  REQUIRE(other != nullptr);
  CHECK(other->ramps[0].hue == 200);
}

TEST_CASE("Styles : un lot REDÉFINIT les variantes des joueurs qu'il mentionne", "[net][style]") {
  World world;
  CHECK(Apply(world, Batch({
                         Entry{.gid = kAlice, .body_key = kOnFoot, .first_hue = 90},
                         Entry{.gid = kAlice, .body_key = kMounted, .first_hue = 200},
                         Entry{.gid = kBob, .body_key = kOnFoot, .first_hue = 10},
                     })) == FrameOutcome::kApplied);

  // Le serveur renvoie l'ensemble complet d'Alice, réduit à un corps.
  CHECK(Apply(world, Batch({Entry{.gid = kAlice, .body_key = kMounted, .first_hue = 333}})) ==
        FrameOutcome::kApplied);

  const ro::sim::StyleVariants* alice = world.styles().Find(kAlice);
  REQUIRE(alice != nullptr);
  CHECK(alice->by_body.size() == 1);
  REQUIRE(alice->For(kMounted) != nullptr);
  CHECK(alice->For(kMounted)->ramps[0].hue == 333);
  // ⚠ ET BOB N'EST PAS TOUCHÉ : un lot ne redéfinit que ce qu'il mentionne.
  REQUIRE(world.styles().Find(kBob) != nullptr);
  CHECK(world.styles().Find(kBob)->by_body.size() == 1);
}

TEST_CASE("Styles : une entrée d'effacement retire le joueur", "[net][style]") {
  World world;
  CHECK(Apply(world, Batch({Entry{.gid = kAlice, .body_key = kOnFoot, .first_hue = 90}})) ==
        FrameOutcome::kApplied);
  REQUIRE(world.styles().Find(kAlice) != nullptr);

  CHECK(Apply(world, Batch({Entry{.gid = kAlice, .flags = kFlagClear, .body_key = kOnFoot}})) ==
        FrameOutcome::kApplied);
  // ⚠ RETIRÉ, ET NON GARDÉ VIDE : une entrée vide se lirait « on sait qu'il n'a
  // pas de couleurs » alors qu'elle dit « il n'en a plus ».
  CHECK(world.styles().Find(kAlice) == nullptr);
}

TEST_CASE("Styles : une version inconnue fait sauter l'entrée, pas le lot", "[net][style]") {
  World world;
  CHECK(Apply(world, Batch({
                         Entry{.gid = kAlice, .version = 6, .body_key = kOnFoot, .first_hue = 90},
                         Entry{.gid = kBob, .body_key = kOnFoot, .first_hue = 200},
                     })) == FrameOutcome::kApplied);

  // 🔴 LA VERSION SIX A CHANGÉ LE CLASSEMENT DES RAMPES : la relire poserait
  // des couleurs sur d'autres pièces du costume.
  CHECK(world.styles().Find(kAlice) == nullptr);
  REQUIRE(world.styles().Find(kBob) != nullptr);
  CHECK(world.styles().Find(kBob)->For(kOnFoot)->ramps[0].hue == 200);
}

TEST_CASE("Styles : une version refusée n'efface pas ce qu'on savait", "[net][style]") {
  World world;
  CHECK(Apply(world, Batch({Entry{.gid = kAlice, .body_key = kOnFoot, .first_hue = 90}})) ==
        FrameOutcome::kApplied);

  // 🔴 LE VIDAGE A LIEU À LA PREMIÈRE ENTRÉE RETENUE, jamais d'entrée de jeu :
  // sinon un lot dont toutes les entrées sont refusées effacerait des
  // variantes valides sans rien mettre à la place.
  CHECK(Apply(world, Batch({Entry{.gid = kAlice, .version = 3, .body_key = kOnFoot}})) ==
        FrameOutcome::kApplied);
  REQUIRE(world.styles().Find(kAlice) != nullptr);
  CHECK(world.styles().Find(kAlice)->For(kOnFoot)->ramps[0].hue == 90);
}

TEST_CASE("Styles : une trame qui annonce plus qu'elle ne porte ne lit pas au-delà",
          "[net][style]") {
  World world;
  CHECK(Apply(world, Batch({Entry{.gid = kAlice, .body_key = kOnFoot, .first_hue = 90}},
                           /*announced=*/9)) == FrameOutcome::kApplied);
  REQUIRE(world.styles().Find(kAlice) != nullptr);
  CHECK(world.styles().Find(kAlice)->by_body.size() == 1);
}

TEST_CASE("Styles : une clé de corps nulle ne désigne aucune variante", "[net][style]") {
  World world;
  CHECK(Apply(world, Batch({Entry{.gid = kAlice, .body_key = 0, .first_hue = 90}})) ==
        FrameOutcome::kApplied);
  // La sentinelle « corps inconnu » ne se range pas : personne ne la
  // retrouverait.
  CHECK(world.styles().Find(kAlice) == nullptr);
}

TEST_CASE("Styles : un autre opcode ne passe pas par là", "[net][style]") {
  World world;
  CHECK(ApplyPlayerStylesFrame(ro::net::Frame{.opcode = 0x0F26, .bytes = {}}, world) ==
        FrameOutcome::kUnhandled);
}

// ── CE QUE NOUS ÉMETTONS ────────────────────────────────────────────────────

namespace {

[[nodiscard]] std::int16_t ReadI16(const std::vector<std::uint8_t>& bytes, std::size_t at) {
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(bytes[at]) |
                                   (static_cast<std::uint16_t>(bytes[at + 1]) << 8));
}

// Les décalages de la demande, tels que le serveur la lit.
constexpr std::size_t kVersionAt = 4;
constexpr std::size_t kFlagsAt = 5;
constexpr std::size_t kBodyAt = 6;
constexpr std::size_t kPaletteAt = 10;
constexpr std::size_t kHairPaletteAt = 12;
constexpr std::size_t kHairStyleAt = 14;
constexpr std::size_t kAdjustsAt = 16;
constexpr std::size_t kRampBytes = 5;

}  // namespace

TEST_CASE("Styles : la demande part en cinquante-six octets, réglage par réglage", "[net][style]") {
  // Ce que le serveur reçoit, octet par octet : c'est la seule preuve que la
  // recette traverse le fil telle qu'il la lit.
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  ro::sim::StyleRecipe recipe;
  recipe.body_palette = 42;
  recipe.hair_palette = 7;
  recipe.hair_style = 13;
  recipe.ramps[0] = StyleAdjust{.hue = -359, .saturation = -100, .value = 100};
  recipe.ramps[1] = StyleAdjust{.hue = 300, .saturation = 100, .value = -20, .absolute = true};
  recipe.ramps[7] = StyleAdjust{.hue = 1};

  REQUIRE(SendPlayerStyle(link, recipe, kOnFoot, /*make_main=*/true).has_value());
  const std::vector<std::uint8_t> bytes = server.Receive(ro::proto::CzMoonlightStyle::kWireSize);
  REQUIRE(bytes.size() == ro::proto::CzMoonlightStyle::kWireSize);

  CHECK((bytes[0] | (bytes[1] << 8)) == ro::proto::CzMoonlightStyle::kOpcode);
  // La longueur s'écrit, pour une trame juste — le serveur, lui, la découpe à
  // la taille fixe que sa table déclare.
  CHECK(ReadI16(bytes, 2) == static_cast<std::int16_t>(ro::proto::CzMoonlightStyle::kWireSize));
  CHECK(bytes[kVersionAt] == kWireVersion);
  CHECK(bytes[kFlagsAt] == kFlagFallback);
  CHECK(ReadU32(bytes, kBodyAt) == kOnFoot);
  CHECK(ReadI16(bytes, kPaletteAt) == 42);
  CHECK(ReadI16(bytes, kHairPaletteAt) == 7);
  CHECK(ReadI16(bytes, kHairStyleAt) == 13);

  // 🔴 CINQ OCTETS PAR RAMPE, ÉCRITS CHAMP PAR CHAMP : la structure en alloue
  // six, et recopier son bourrage décalerait toutes les rampes suivantes.
  CHECK(ReadI16(bytes, kAdjustsAt) == -359);
  CHECK(static_cast<std::int8_t>(bytes[kAdjustsAt + 2]) == -100);
  CHECK(static_cast<std::int8_t>(bytes[kAdjustsAt + 3]) == 100);
  CHECK(bytes[kAdjustsAt + 4] == 0);
  CHECK(ReadI16(bytes, kAdjustsAt + kRampBytes) == 300);
  CHECK(bytes[kAdjustsAt + kRampBytes + 4] == 1);
  // La huitième rampe tombe bien à sa place, et non une case plus loin.
  CHECK(ReadI16(bytes, kAdjustsAt + 7 * kRampBytes) == 1);
}

TEST_CASE("Styles : effacer un corps ne porte aucune couleur", "[net][style]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  REQUIRE(SendPlayerStyleClear(link, kOnFoot).has_value());
  const std::vector<std::uint8_t> bytes = server.Receive(ro::proto::CzMoonlightStyle::kWireSize);
  REQUIRE(bytes.size() == ro::proto::CzMoonlightStyle::kWireSize);
  CHECK(bytes[kFlagsAt] == kFlagClear);
  CHECK(ReadU32(bytes, kBodyAt) == kOnFoot);
  // ⚠ MOINS UN, ET NON ZÉRO : zéro serait la première palette officielle, et
  // un effacement repeindrait le personnage en couleur numéro zéro.
  CHECK(ReadI16(bytes, kPaletteAt) == -1);
  CHECK(ReadI16(bytes, kHairStyleAt) == -1);
}

TEST_CASE("Styles : tout effacer ne vise aucun corps", "[net][style]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  REQUIRE(SendPlayerStyleClearAll(link).has_value());
  const std::vector<std::uint8_t> bytes = server.Receive(ro::proto::CzMoonlightStyle::kWireSize);
  REQUIRE(bytes.size() == ro::proto::CzMoonlightStyle::kWireSize);
  // 🔴 DEUX GESTES DIFFÉRENTS, DEUX DRAPEAUX : « ce corps-ci » et « plus aucune
  // couleur » ne se confondent pas.
  CHECK(bytes[kFlagsAt] == 0x02);
  CHECK(ReadU32(bytes, kBodyAt) == 0);
}

TEST_CASE("Styles : valider sans en faire le style principal ne porte aucun drapeau",
          "[net][style]") {
  // ⚠ LE REPLI NE CHANGE QUE SUR DEMANDE : une validation ordinaire le laisse au
  // serveur, qui garde la première variante rangée.
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  REQUIRE(SendPlayerStyle(link, StyleRecipe{}, kMounted, /*make_main=*/false).has_value());
  const std::vector<std::uint8_t> bytes = server.Receive(ro::proto::CzMoonlightStyle::kWireSize);
  REQUIRE(bytes.size() == ro::proto::CzMoonlightStyle::kWireSize);
  CHECK(bytes[kFlagsAt] == 0);
  CHECK(ReadU32(bytes, kBodyAt) == kMounted);
}

TEST_CASE("Styles : sur une liaison fermée, rien ne se dit parti", "[net][style]") {
  // 🔴 LA LIAISON AVALE UN ENVOI SUR UNE CONNEXION FERMÉE. Sans ce refus,
  // l'éditeur attendrait l'écho d'une demande qui n'est jamais partie.
  Connection link;
  REQUIRE_FALSE(link.is_open());
  CHECK_FALSE(SendPlayerStyle(link, StyleRecipe{}, kOnFoot, /*make_main=*/false).has_value());
  CHECK_FALSE(SendPlayerStyleClear(link, kOnFoot).has_value());
  CHECK_FALSE(SendPlayerStyleClearAll(link).has_value());
}

// ── LE STYLISTE OUVRE LA FENÊTRE (0x0F28) ───────────────────────────────────

namespace {

[[nodiscard]] FrameOutcome OpenWith(World& world, std::uint8_t mode) {
  ro::proto::ZcMoonlightStyleOpen packet;
  packet.mode = mode;
  return ApplyStyleOpenFrame(ro::net::Frame{.opcode = ro::proto::ZcMoonlightStyleOpen::kOpcode,
                                            .bytes = ro::proto::Encode(packet)},
                             world);
}

constexpr std::uint8_t kModeClose = 0;
constexpr std::uint8_t kModeOpen = 1;
constexpr std::uint8_t kModeToggle = 2;
// Un mode qu'un serveur plus récent pourrait ajouter.
constexpr std::uint8_t kModeUnknown = 9;

}  // namespace

TEST_CASE("Styliste : les trois modes se distinguent", "[net][style]") {
  // ⚠ « BASCULER » N'EST PAS « OUVRIR » : un PNJ veut pouvoir dire « ouvre »
  // sans risquer de refermer ce que le joueur venait d'ouvrir lui-même.
  World world;
  REQUIRE(OpenWith(world, kModeClose) == FrameOutcome::kApplied);
  CHECK(world.styles().TakeRequest() == ro::sim::StyleEditorRequest::kClose);
  REQUIRE(OpenWith(world, kModeOpen) == FrameOutcome::kApplied);
  CHECK(world.styles().TakeRequest() == ro::sim::StyleEditorRequest::kOpen);
  REQUIRE(OpenWith(world, kModeToggle) == FrameOutcome::kApplied);
  CHECK(world.styles().TakeRequest() == ro::sim::StyleEditorRequest::kToggle);
}

TEST_CASE("Styliste : un mode inconnu bascule, et la demande se consomme", "[net][style]") {
  World world;
  REQUIRE(OpenWith(world, kModeUnknown) == FrameOutcome::kApplied);
  CHECK(world.styles().TakeRequest() == ro::sim::StyleEditorRequest::kToggle);
  // 🔴 CONSOMMÉE : laissée en place, elle rouvrirait la fenêtre à chaque image.
  CHECK_FALSE(world.styles().TakeRequest().has_value());
}

TEST_CASE("Styliste : le quartet haut du mode désigne l'onglet", "[net][style]") {
  // Ouvrir sur le troisième onglet, celui des cheveux.
  constexpr std::uint8_t kOpenOnHair = 0x31;
  World world;
  REQUIRE(OpenWith(world, kOpenOnHair) == FrameOutcome::kApplied);
  CHECK(world.styles().TakeRequest() == ro::sim::StyleEditorRequest::kOpen);
  CHECK(world.styles().TakeTab() == 3);
  // L'onglet se consomme ; un mode simple n'en demande aucun.
  CHECK(world.styles().TakeTab() == 0);
  REQUIRE(OpenWith(world, kModeOpen) == FrameOutcome::kApplied);
  CHECK(world.styles().TakeTab() == 0);
}

TEST_CASE("Styliste : une trame tronquée ou d'un autre opcode ne demande rien", "[net][style]") {
  World world;
  CHECK(ApplyStyleOpenFrame(
            ro::net::Frame{.opcode = ro::proto::ZcMoonlightStyles::kOpcode, .bytes = {}}, world) ==
        FrameOutcome::kUnhandled);
  std::vector<std::uint8_t> truncated = ro::proto::Encode(ro::proto::ZcMoonlightStyleOpen{});
  truncated.pop_back();
  CHECK(ApplyStyleOpenFrame(ro::net::Frame{.opcode = ro::proto::ZcMoonlightStyleOpen::kOpcode,
                                           .bytes = std::move(truncated)},
                            world) == FrameOutcome::kUndecodable);
  CHECK_FALSE(world.styles().TakeRequest().has_value());
}
