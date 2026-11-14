// LE CARNET DE CHASSE MVP SUR LE FIL — `ZC 0x0F31`, `ZC 0x0F32` et `CZ 0x0F30`.
//
// 🔴 LES TRAMES SONT ÉCRITES À LA MAIN, champ par champ, aux décalages que le
// serveur emploie : une entrée qui glisserait d'un octet décalerait tous les
// champs suivants sans qu'aucun contrôle de longueur ne le voie.

#include "ro/net/mvp_tracker.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/mvp_tracker.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyMvpTrackerFrame;
using ro::net::EncodeMvpCommand;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::net::MvpCommand;
using ro::net::MvpManualReportText;
using ro::net::RequestMvpCommand;
using ro::sim::MvpTrackerBook;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;

namespace {

constexpr std::uint16_t kStateOpcode = ro::proto::ZcMoonlightMvpState::kOpcode;
constexpr std::uint16_t kGroupOpcode = ro::proto::ZcMoonlightMvpGroup::kOpcode;
constexpr std::int64_t kLocalS = 1699999000;
constexpr std::int64_t kServerS = 1700000000;
constexpr std::uint32_t kNowMs = 123456;
// La plus longue commande sans identifiant : le suffixe en est absent.
constexpr std::size_t kNoIdMostBytes = ro::net::kMvpCommandFixedSize + ro::net::kMvpCommandTextMost;

using Bytes = std::vector<std::uint8_t>;

void PutU8(Bytes& into, std::uint8_t value) { into.push_back(value); }

void PutI64(Bytes& into, std::int64_t value) {
  const auto raw = static_cast<std::uint64_t>(value);
  PutU32(into, static_cast<std::uint32_t>(raw & 0xFFFFFFFFU));
  PutU32(into, static_cast<std::uint32_t>(raw >> 32U));
}

// Un champ texte de `width` octets, complété de zéros — ou plein, sans NUL.
void PutText(Bytes& into, std::string_view text, std::size_t width) {
  for (std::size_t at = 0; at < width; ++at) {
    into.push_back(at < text.size() ? static_cast<std::uint8_t>(text[at]) : std::uint8_t{0});
  }
}

// Écrit la longueur de la trame, une fois la queue posée.
[[nodiscard]] Frame Framed(std::uint16_t opcode, Bytes bytes) {
  bytes[2] = static_cast<std::uint8_t>(bytes.size() & 0xFFU);
  bytes[3] = static_cast<std::uint8_t>(bytes.size() >> 8U);
  return Frame{.opcode = opcode, .bytes = std::move(bytes)};
}

// L'en-tête de `ZC 0x0F31`, quinze octets.
[[nodiscard]] Bytes StateHead(std::uint8_t kind, std::int64_t server_time_s, std::uint16_t count) {
  Bytes bytes;
  PutU16(bytes, kStateOpcode);
  PutU16(bytes, 0);
  PutU8(bytes, kind);
  PutI64(bytes, server_time_s);
  PutU16(bytes, count);
  return bytes;
}

struct CatalogueEntry {
  std::uint16_t slot_id;
  std::uint16_t mob_id;
  std::uint8_t kind;
  std::uint32_t delay1_ms;
  std::uint32_t delay2_ms;
  std::uint16_t map_xs;
  std::uint16_t map_ys;
  std::string map;
  std::string name;
};

void PutCatalogueEntry(Bytes& into, const CatalogueEntry& entry) {
  PutU16(into, entry.slot_id);
  PutU16(into, entry.mob_id);
  PutU8(into, entry.kind);
  PutU32(into, entry.delay1_ms);
  PutU32(into, entry.delay2_ms);
  PutU16(into, entry.map_xs);
  PutU16(into, entry.map_ys);
  PutText(into, entry.map, ro::net::kMvpMapNameBytes);
  PutText(into, entry.name, ro::net::kMvpNameBytes);
}

struct ObservationEntry {
  std::uint16_t slot_id;
  std::uint8_t source;
  std::uint16_t mob_id;
  std::int64_t kill_s;
  std::int64_t exact_s;
  std::int16_t tomb_x;
  std::int16_t tomb_y;
  std::uint32_t by_user;
  std::int64_t reported_s;
  std::string by_name;
};

void PutObservationEntry(Bytes& into, const ObservationEntry& entry) {
  PutU16(into, entry.slot_id);
  PutU8(into, entry.source);
  PutU16(into, entry.mob_id);
  PutI64(into, entry.kill_s);
  PutI64(into, entry.exact_s);
  PutU16(into, static_cast<std::uint16_t>(entry.tomb_x));
  PutU16(into, static_cast<std::uint16_t>(entry.tomb_y));
  PutU32(into, entry.by_user);
  PutI64(into, entry.reported_s);
  PutText(into, entry.by_name, ro::net::kMvpNameBytes);
}

// Un nom en 1252, tel que le serveur le tient : le « é » pèse UN octet.
const std::string kCp1252Name = "Baphom\xE9t";
const std::string kUtf8Name = "Baphom\xC3\xA9t";

const std::vector<CatalogueEntry> kTwoSlots = {
    {4, 1039, 0, 7260000, 540000, 200, 180, "Prt_Maze03", kCp1252Name},
    {9, 0, 1, 3600000, 0, 300, 250, "lhz_dun03", ""},
};

[[nodiscard]] Frame CatalogueFrame(const std::vector<CatalogueEntry>& entries,
                                   std::int64_t server_time_s = kServerS) {
  Bytes bytes = StateHead(0, server_time_s, static_cast<std::uint16_t>(entries.size()));
  for (const CatalogueEntry& entry : entries) {
    PutCatalogueEntry(bytes, entry);
  }
  return Framed(kStateOpcode, std::move(bytes));
}

[[nodiscard]] Frame ObservationFrame(std::uint8_t kind,
                                     const std::vector<ObservationEntry>& entries,
                                     std::int64_t server_time_s = kServerS) {
  Bytes bytes = StateHead(kind, server_time_s, static_cast<std::uint16_t>(entries.size()));
  for (const ObservationEntry& entry : entries) {
    PutObservationEntry(bytes, entry);
  }
  return Framed(kStateOpcode, std::move(bytes));
}

// Coupe les `dropped` derniers octets d'une trame, sans retoucher sa longueur
// écrite : c'est ce qu'une queue tronquée donnerait.
[[nodiscard]] Frame Cut(Frame frame, std::size_t dropped) {
  frame.bytes.resize(frame.bytes.size() - dropped);
  return frame;
}

struct MemberEntry {
  std::uint32_t user_id;
  std::int16_t level;
  bool online;
  std::string name;
};

[[nodiscard]] Frame GroupFrame(std::uint8_t kind, std::uint8_t result, std::uint32_t group_id,
                               std::uint32_t owner, std::string_view name,
                               const std::vector<MemberEntry>& members) {
  Bytes bytes;
  PutU16(bytes, kGroupOpcode);
  PutU16(bytes, 0);
  PutU8(bytes, kind);
  PutU8(bytes, result);
  PutU32(bytes, group_id);
  PutU32(bytes, owner);
  PutText(bytes, name, ro::net::kMvpGroupNameBytes);
  PutU8(bytes, static_cast<std::uint8_t>(members.size()));
  for (const MemberEntry& member : members) {
    PutU32(bytes, member.user_id);
    PutU16(bytes, static_cast<std::uint16_t>(member.level));
    PutU8(bytes, static_cast<std::uint8_t>(member.online ? 1 : 0));
    PutText(bytes, member.name, ro::net::kMvpNameBytes);
  }
  return Framed(kGroupOpcode, std::move(bytes));
}

const std::vector<MemberEntry> kThreeMembers = {
    {2000001, 99, true, "Sting"},
    // Deux têtes d'un même compte, à la suite.
    {2000002, 175, false, "L\xE9vi"},
    {2000002, -1, true, ""},
};

[[nodiscard]] FrameOutcome Apply(const Frame& frame, MvpTrackerBook& book,
                                 std::int64_t local_now_s = kLocalS) {
  return ApplyMvpTrackerFrame(frame, book, local_now_s, kNowMs);
}

[[nodiscard]] Bytes Concat(Bytes head, std::string_view tail) {
  head.insert(head.end(), tail.begin(), tail.end());
  return head;
}

}  // namespace

TEST_CASE("Carnet MVP : le catalogue, champ par champ", "[net][mvp]") {
  REQUIRE(CatalogueFrame(kTwoSlots).bytes.size() ==
          ro::net::kMvpStateFixedSize + 2 * ro::net::kMvpCatalogueEntrySize);
  MvpTrackerBook book;
  REQUIRE(Apply(CatalogueFrame(kTwoSlots), book) == FrameOutcome::kApplied);
  CHECK(book.catalog_known());
  REQUIRE(book.slots().size() == 2);

  const ro::sim::MvpSlot& first = book.slots()[0];
  CHECK(first.slot_id == 4);
  CHECK(first.mob_id == 1039);
  CHECK(first.kind == 0);
  CHECK(first.delay1_ms == 7260000);
  CHECK(first.delay2_ms == 540000);
  CHECK(first.map_cells_x == 200);
  CHECK(first.map_cells_y == 180);
  // La carte reste telle quelle, casse comprise ; le nom passe en UTF-8.
  CHECK(first.map == "Prt_Maze03");
  CHECK(first.name == kUtf8Name);

  const ro::sim::MvpSlot& second = book.slots()[1];
  CHECK(second.slot_id == 9);
  CHECK(second.mob_id == 0);
  CHECK(second.kind == 1);
  CHECK(second.delay1_ms == 3600000);
  CHECK(second.map_cells_x == 300);
  CHECK(second.map_cells_y == 250);
  CHECK(second.map == "lhz_dun03");
  CHECK(second.name.empty());

  // L'horloge s'est recalée sur ce paquet.
  CHECK(book.clock_known());
  CHECK(book.ServerNow(kLocalS) == kServerS);
}

TEST_CASE("Carnet MVP : un champ plein garde son dernier octet pour le NUL", "[net][mvp]") {
  const std::string full_map(ro::net::kMvpMapNameBytes, 'm');
  const std::string full_name(ro::net::kMvpNameBytes, 'n');
  MvpTrackerBook book;
  REQUIRE(Apply(CatalogueFrame({{1, 1039, 0, 0, 0, 0, 0, full_map, full_name}}), book) ==
          FrameOutcome::kApplied);
  REQUIRE(book.slots().size() == 1);
  CHECK(book.slots()[0].map == std::string(ro::net::kMvpMapNameBytes - 1, 'm'));
  CHECK(book.slots()[0].name == std::string(ro::net::kMvpNameBytes - 1, 'n'));
}

TEST_CASE("Carnet MVP : l'instantané et le delta, champ par champ", "[net][mvp]") {
  MvpTrackerBook book;
  const std::vector<ObservationEntry> snapshot = {
      {4, 2, 1039, kServerS - 600, 0, -1, -1, 2000001, kServerS - 590, "Sting"},
      {9, 3, 1646, kServerS - 300, kServerS + 3000, 150, 37, 2000002, kServerS - 200, kCp1252Name},
  };
  const Frame frame = ObservationFrame(1, snapshot);
  REQUIRE(frame.bytes.size() ==
          ro::net::kMvpStateFixedSize + 2 * ro::net::kMvpObservationEntrySize);
  REQUIRE(Apply(frame, book) == FrameOutcome::kApplied);
  REQUIRE(book.observations().size() == 2);

  const ro::sim::MvpObservation* killed = book.FindObservation(4);
  REQUIRE(killed != nullptr);
  CHECK(killed->source == ro::sim::MvpSource::kKill);
  CHECK(killed->source_raw == 2);
  CHECK(killed->mob_id == 1039);
  CHECK(killed->kill_time_s == kServerS - 600);
  CHECK(killed->exact_respawn_s == 0);
  // -1 relu SIGNÉ : « tombe inconnue », pas la cellule 65535.
  CHECK(killed->tomb_x == -1);
  CHECK(killed->tomb_y == -1);
  CHECK(killed->by_user_id == 2000001);
  CHECK(killed->reported_at_s == kServerS - 590);
  CHECK(killed->by_name == "Sting");

  const ro::sim::MvpObservation* mirrored = book.FindObservation(9);
  REQUIRE(mirrored != nullptr);
  CHECK(mirrored->source == ro::sim::MvpSource::kMirror);
  CHECK(mirrored->mob_id == 1646);
  CHECK(mirrored->exact_respawn_s == kServerS + 3000);
  CHECK(mirrored->tomb_x == 150);
  CHECK(mirrored->tomb_y == 37);
  CHECK(mirrored->by_user_id == 2000002);
  CHECK(mirrored->by_name == kUtf8Name);

  // Un delta d'une entrée COMPLÈTE ; une source inconnue se garde en brut.
  REQUIRE(Apply(ObservationFrame(2, {{5, 7, 1059, kServerS, 0, 3, 4, 1, kServerS, ""}}), book) ==
          FrameOutcome::kApplied);
  CHECK(book.observations().size() == 3);
  const ro::sim::MvpObservation* unknown = book.FindObservation(5);
  REQUIRE(unknown != nullptr);
  CHECK(unknown->source_raw == 7);
  CHECK(unknown->source == ro::sim::MvpSource::kManual);

  // Un instantané vide : « sans groupe », tout part.
  REQUIRE(Apply(ObservationFrame(1, {}), book) == FrameOutcome::kApplied);
  CHECK(book.observations().empty());
}

TEST_CASE("Carnet MVP : les heures du fil se bornent à la lecture", "[net][mvp]") {
  constexpr std::int64_t kMost = std::numeric_limits<std::int64_t>::max();
  constexpr std::int64_t kLeast = std::numeric_limits<std::int64_t>::min();
  MvpTrackerBook book;
  // Une heure du serveur aberrante : la plus tardive qu'on garde.
  REQUIRE(Apply(ObservationFrame(1,
                                 {{4, 2, 1039, kMost, kLeast, -1, -1, 1, kMost, ""},
                                  {9, 3, 1646, kLeast, kMost, -1, -1, 1, kLeast, ""}},
                                 kMost),
                book) == FrameOutcome::kApplied);
  const ro::sim::MvpObservation* late = book.FindObservation(4);
  REQUIRE(late != nullptr);
  CHECK(late->kill_time_s == ro::net::kMvpLatestWireSeconds);
  CHECK(late->exact_respawn_s == 0);
  CHECK(late->reported_at_s == ro::net::kMvpLatestWireSeconds);
  const ro::sim::MvpObservation* early = book.FindObservation(9);
  REQUIRE(early != nullptr);
  CHECK(early->kill_time_s == 0);
  CHECK(early->exact_respawn_s == ro::net::kMvpLatestWireSeconds);
  CHECK(early->reported_at_s == 0);
  CHECK(book.ServerNow(kLocalS) == ro::net::kMvpLatestWireSeconds);
  // 🔴 UNE ÉCHÉANCE TIRÉE DE CES HEURES laisse encore de la place pour une
  // durée : rien en aval ne déborde.
  const std::optional<ro::sim::MvpWindow> window = book.WindowOf(9);
  REQUIRE(window.has_value());
  CHECK(window->to_s <= kMost - ro::net::kMvpLatestWireSeconds);
}

TEST_CASE("Carnet MVP : les favoris", "[net][mvp]") {
  MvpTrackerBook book;
  Bytes bytes = StateHead(3, kServerS, 3);
  PutU16(bytes, 4);
  PutU16(bytes, 9);
  PutU16(bytes, 513);
  REQUIRE(Apply(Framed(kStateOpcode, bytes), book) == FrameOutcome::kApplied);
  REQUIRE(book.favorites().size() == 3);
  CHECK(book.favorites()[0] == 4);
  CHECK(book.favorites()[1] == 9);
  CHECK(book.favorites()[2] == 513);
  CHECK(book.IsFavorite(513));
}

TEST_CASE("Carnet MVP : le groupe, ses membres, l'invitation, le résultat", "[net][mvp]") {
  MvpTrackerBook book;
  REQUIRE(Apply(ObservationFrame(1, {{4, 2, 1039, kServerS, 0, -1, -1, 1, kServerS, "A"}}), book) ==
          FrameOutcome::kApplied);

  const Frame group = GroupFrame(0, 0, 77, 2000001, "Chasse \xE0 l'aube", kThreeMembers);
  REQUIRE(group.bytes.size() == ro::net::kMvpGroupFixedSize + 3 * ro::net::kMvpMemberEntrySize);
  REQUIRE(Apply(group, book) == FrameOutcome::kApplied);
  CHECK(book.group().group_id == 77);
  CHECK(book.group().owner_user_id == 2000001);
  CHECK(book.group().name == "Chasse \xC3\xA0 l'aube");
  REQUIRE(book.group().members.size() == 3);
  CHECK(book.group().members[0].user_id == 2000001);
  CHECK(book.group().members[0].level == 99);
  CHECK(book.group().members[0].online);
  CHECK(book.group().members[0].name == "Sting");
  CHECK(book.group().members[1].user_id == 2000002);
  CHECK(book.group().members[1].level == 175);
  CHECK_FALSE(book.group().members[1].online);
  CHECK(book.group().members[1].name == "L\xC3\xA9vi");
  CHECK(book.group().members[2].user_id == 2000002);
  CHECK(book.group().members[2].level == -1);
  CHECK(book.group().members[2].name.empty());
  // Un groupe non nul ne vide pas ce que l'on sait.
  CHECK(book.observations().size() == 1);

  // L'INVITATION : son nom est au même décalage que celui du groupe.
  REQUIRE(Apply(GroupFrame(1, 0, 12, 0, "Les Chasseurs", {}), book) == FrameOutcome::kApplied);
  CHECK(book.pending_invite() == 12);
  CHECK(book.pending_invite_name() == "Les Chasseurs");
  // Le groupe n'a pas bougé.
  CHECK(book.group().group_id == 77);

  // LE RÉSULTAT : ni le groupe ni l'invitation ne bougent.
  REQUIRE(Apply(GroupFrame(2, 7, 0, 0, "", {}), book) == FrameOutcome::kApplied);
  CHECK(book.last_result() == 7);
  CHECK(book.last_result_ms() == kNowMs);
  CHECK(book.group().group_id == 77);
  CHECK(book.pending_invite() == 12);

  // « DANS AUCUN GROUPE » vide les observations, et garde l'invitation.
  REQUIRE(Apply(GroupFrame(0, 0, 0, 0, "", {}), book) == FrameOutcome::kApplied);
  CHECK(book.group().group_id == 0);
  CHECK(book.group().members.empty());
  CHECK(book.observations().empty());
  CHECK(book.pending_invite() == 12);
}

TEST_CASE("Carnet MVP : un nom de groupe plein garde 31 octets", "[net][mvp]") {
  MvpTrackerBook book;
  const std::string full(ro::net::kMvpGroupNameBytes, 'g');
  REQUIRE(Apply(GroupFrame(1, 0, 3, 0, full, {}), book) == FrameOutcome::kApplied);
  CHECK(book.pending_invite_name() == std::string(ro::net::kMvpGroupNameBytes - 1, 'g'));
}

TEST_CASE("Carnet MVP : une queue tronquée ne touche à rien, sauf l'horloge", "[net][mvp]") {
  MvpTrackerBook book;
  REQUIRE(Apply(CatalogueFrame(kTwoSlots), book) == FrameOutcome::kApplied);
  REQUIRE(Apply(ObservationFrame(1, {{4, 2, 1039, kServerS, 0, 5, 6, 1, kServerS, "A"}}), book) ==
          FrameOutcome::kApplied);
  Bytes favorites = StateHead(3, kServerS, 1);
  PutU16(favorites, 4);
  REQUIRE(Apply(Framed(kStateOpcode, favorites), book) == FrameOutcome::kApplied);
  const std::uint32_t serial = book.catalog_serial();
  // Chaque trame tronquée porte une heure neuve : elle doit être retenue.
  const std::int64_t later_s = kServerS + 500;

  SECTION("le catalogue") {
    const Frame cut = Cut(CatalogueFrame({kTwoSlots[0]}, later_s), 1);
    CHECK(Apply(cut, book) == FrameOutcome::kUndecodable);
    CHECK(book.slots().size() == 2);
    CHECK(book.catalog_serial() == serial);
  }
  SECTION("l'instantané") {
    const Frame cut = Cut(ObservationFrame(1, {{8, 1, 1, 1, 0, 1, 1, 1, 1, "B"}}, later_s), 1);
    CHECK(Apply(cut, book) == FrameOutcome::kUndecodable);
    REQUIRE(book.observations().size() == 1);
    CHECK(book.FindObservation(4) != nullptr);
  }
  SECTION("un compte qui ment") {
    Bytes lying = StateHead(2, later_s, 2);
    PutObservationEntry(lying, {8, 1, 1, 1, 0, 1, 1, 1, 1, "B"});
    CHECK(Apply(Framed(kStateOpcode, lying), book) == FrameOutcome::kUndecodable);
    CHECK(book.FindObservation(8) == nullptr);
  }
  SECTION("les favoris") {
    Bytes cut = StateHead(3, later_s, 2);
    PutU16(cut, 9);
    CHECK(Apply(Framed(kStateOpcode, cut), book) == FrameOutcome::kUndecodable);
    REQUIRE(book.favorites().size() == 1);
    CHECK(book.favorites()[0] == 4);
  }
  SECTION("un kind inconnu est ignoré") {
    CHECK(Apply(Framed(kStateOpcode, StateHead(9, later_s, 0)), book) == FrameOutcome::kIgnored);
    CHECK(book.slots().size() == 2);
  }
  CHECK(book.ServerNow(kLocalS) == later_s);
}

TEST_CASE("Carnet MVP : un corps plus long que son compte passe", "[net][mvp]") {
  MvpTrackerBook book;
  Bytes bytes = StateHead(3, kServerS, 1);
  PutU16(bytes, 4);
  PutU16(bytes, 9);
  REQUIRE(Apply(Framed(kStateOpcode, bytes), book) == FrameOutcome::kApplied);
  REQUIRE(book.favorites().size() == 1);
  CHECK(book.favorites()[0] == 4);
}

TEST_CASE("Carnet MVP : un en-tête trop court ne recale pas l'horloge", "[net][mvp]") {
  MvpTrackerBook book;
  const Frame head_cut = Cut(Framed(kStateOpcode, StateHead(0, kServerS, 0)), 1);
  CHECK(Apply(head_cut, book) == FrameOutcome::kUndecodable);
  CHECK_FALSE(book.clock_known());
  CHECK(book.ServerNow(kLocalS) == kLocalS);
  CHECK_FALSE(book.catalog_known());

  // Une heure nulle passe, sans rien recaler.
  REQUIRE(Apply(CatalogueFrame({}, 0), book) == FrameOutcome::kApplied);
  CHECK_FALSE(book.clock_known());
  CHECK(book.catalog_known());
}

TEST_CASE("Carnet MVP : un groupe tronqué reste sans membres", "[net][mvp]") {
  MvpTrackerBook book;
  REQUIRE(Apply(ObservationFrame(1, {{4, 2, 1039, kServerS, 0, -1, -1, 1, kServerS, "A"}}), book) ==
          FrameOutcome::kApplied);
  REQUIRE(Apply(GroupFrame(0, 0, 77, 1, "Chasse", kThreeMembers), book) == FrameOutcome::kApplied);
  REQUIRE(book.group().members.size() == 3);

  SECTION("un groupe zéro tronqué ne vide pas les observations") {
    const Frame cut = Cut(GroupFrame(0, 0, 0, 0, "", kThreeMembers), 1);
    CHECK(Apply(cut, book) == FrameOutcome::kUndecodable);
    CHECK(book.group().group_id == 0);
    CHECK(book.group().members.empty());
    CHECK(book.observations().size() == 1);
  }
  SECTION("un en-tête de groupe trop court ne touche à rien") {
    const Frame cut = Cut(GroupFrame(0, 0, 0, 0, "", {}), 1);
    CHECK(Apply(cut, book) == FrameOutcome::kUndecodable);
    CHECK(book.group().group_id == 77);
    CHECK(book.group().members.size() == 3);
    const Frame result_cut = Cut(GroupFrame(2, 5, 0, 0, "", {}), 1);
    CHECK(Apply(result_cut, book) == FrameOutcome::kUndecodable);
    CHECK(book.last_result() == 0);
  }
}

TEST_CASE("Carnet MVP : une autre trame n'est pas la sienne", "[net][mvp]") {
  MvpTrackerBook book;
  const Frame other{.opcode = ro::proto::ZcMoonlightCardAlbum::kOpcode,
                    .bytes = CatalogueFrame(kTwoSlots).bytes};
  CHECK(Apply(other, book) == FrameOutcome::kUnhandled);
  CHECK_FALSE(book.catalog_known());
  CHECK_FALSE(book.clock_known());
}

TEST_CASE("Carnet MVP : le texte d'une saisie manuelle", "[net][mvp]") {
  CHECK(MvpManualReportText(123, 45, "Sting") == "123,45|Sting");
  CHECK(MvpManualReportText(0, 0, "") == "0,0");
  CHECK(MvpManualReportText(-1, -1, "Sting") == "|Sting");
  CHECK(MvpManualReportText(12, -1, "Sting") == "|Sting");
  CHECK(MvpManualReportText(-1, -1, "").empty());
  // Le nom reste en UTF-8 : la conversion se fait à l'envoi, une seule fois.
  CHECK(MvpManualReportText(1, 2, "L\xC3\xA9vi") == "1,2|L\xC3\xA9vi");
}

TEST_CASE("Carnet MVP : une commande se code octet pour octet", "[net][mvp]") {
  const Bytes snapshot = EncodeMvpCommand(MvpCommand::kSnapshot, 0, 0, {});
  CHECK(snapshot == Bytes{0x30, 0x0F, 0x0D, 0x00, 0x01, 0, 0, 0, 0, 0, 0, 0, 0});

  const Bytes favorite = EncodeMvpCommand(MvpCommand::kFavorite, 513, 1, {});
  CHECK(favorite == Bytes{0x30, 0x0F, 0x0D, 0x00, 0x09, 0x01, 0x02, 0, 0, 0x01, 0, 0, 0});

  // Le texte se coupe à 64 octets, et la longueur suit.
  const std::string seventy(70, 'x');
  const Bytes capped = EncodeMvpCommand(MvpCommand::kCreateGroup, 0, 0, seventy);
  REQUIRE(capped.size() == kNoIdMostBytes);
  CHECK(capped[2] == static_cast<std::uint8_t>(kNoIdMostBytes));
  CHECK(capped[3] == 0);
  CHECK(capped.back() == 'x');
}

TEST_CASE("Carnet MVP : les commandes partent en 1252 sur le fil", "[net][mvp]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  // L'INSTANTANÉ : treize octets, aucun texte.
  RequestMvpCommand(link, MvpCommand::kSnapshot, 0, 0);
  CHECK(server.Receive(ro::net::kMvpCommandFixedSize) ==
        Bytes{0x30, 0x0F, 0x0D, 0x00, 0x01, 0, 0, 0, 0, 0, 0, 0, 0});

  // CRÉER UN GROUPE : le nom, sans NUL.
  RequestMvpCommand(link, MvpCommand::kCreateGroup, 0, 0, "Chasse");
  CHECK(server.Receive(ro::net::kMvpCommandFixedSize + 6) ==
        Concat({0x30, 0x0F, 0x13, 0x00, 0x02, 0, 0, 0, 0, 0, 0, 0, 0}, "Chasse"));

  // UN NOM ACCENTUÉ : « é » part en UN octet 0xE9, jamais en UTF-8.
  RequestMvpCommand(link, MvpCommand::kInvite, 0, 0, "L\xC3\xA9vi");
  CHECK(server.Receive(ro::net::kMvpCommandFixedSize + 4) ==
        Bytes{0x30, 0x0F, 0x11, 0x00, 0x04, 0, 0, 0, 0, 0, 0, 0, 0, 0x4C, 0xE9, 0x76, 0x69});

  // LA SAISIE : créneau 5, mort 1700000000, tombe 123,45, partagée par Sting.
  RequestMvpCommand(link, MvpCommand::kReportManual, 5, 1700000000,
                    MvpManualReportText(123, 45, "Sting"));
  CHECK(server.Receive(ro::net::kMvpCommandFixedSize + 12) ==
        Concat({0x30, 0x0F, 0x19, 0x00, 0x0A, 0x05, 0x00, 0x00, 0x00, 0x00, 0xF1, 0x53, 0x65},
               "123,45|Sting"));

  // UN TEXTE TROP LONG est coupé APRÈS conversion : 70 « é » font 70 octets en
  // 1252 — 140 en UTF-8 —, et 64 partent.
  std::string accents;
  for (int letter = 0; letter < 70; ++letter) {
    accents += "\xC3\xA9";
  }
  RequestMvpCommand(link, MvpCommand::kKick, 0, 0, accents);
  const Bytes kicked = server.Receive(kNoIdMostBytes);
  REQUIRE(kicked.size() == kNoIdMostBytes);
  CHECK(kicked[2] == static_cast<std::uint8_t>(kNoIdMostBytes));
  CHECK(kicked[4] == static_cast<std::uint8_t>(MvpCommand::kKick));
  CHECK(kicked[ro::net::kMvpCommandFixedSize] == 0xE9);
  CHECK(kicked.back() == 0xE9);

  // Et rien de plus n'est parti.
  RequestMvpCommand(link, MvpCommand::kLeaveGroup, 0, 0);
  const Bytes leave = server.Receive(ro::net::kMvpCommandFixedSize);
  CHECK(leave == Bytes{0x30, 0x0F, 0x0D, 0x00, 0x07, 0, 0, 0, 0, 0, 0, 0, 0});
}

TEST_CASE("Carnet MVP : une commande porte son identifiant en suffixe", "[net][mvp]") {
  // [0x00][request_id:4] derrière le texte, et la longueur le compte.
  const Bytes leave = EncodeMvpCommand(MvpCommand::kLeaveGroup, 0, 0, {}, 0x01020304U);
  CHECK(leave ==
        Bytes{0x30, 0x0F, 0x12, 0x00, 0x07, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, 0x04, 0x03, 0x02, 0x01});
  const Bytes kick = EncodeMvpCommand(MvpCommand::kKick, 0, 0, "Zed", 7);
  CHECK(kick == Bytes{0x30, 0x0F, 0x15, 0x00, 0x08, 0,    0,    0, 0, 0, 0,
                      0,    0,    'Z',  'e',  'd',  0x00, 0x07, 0, 0, 0});

  // 🔴 UN TEXTE QUI PORTE UN NUL S'Y COUPE : sinon le serveur lirait sa suite
  // comme un identifiant.
  const Bytes cut = EncodeMvpCommand(MvpCommand::kKick, 0, 0, std::string_view("Ze\0dxxxx", 8));
  CHECK(cut == Bytes{0x30, 0x0F, 0x0F, 0x00, 0x08, 0, 0, 0, 0, 0, 0, 0, 0, 'Z', 'e'});

  // LE PLAFOND : 64 octets de texte, puis les cinq du suffixe.
  const std::string seventy(70, 'x');
  const Bytes capped = EncodeMvpCommand(MvpCommand::kCreateGroup, 0, 0, seventy, 9);
  REQUIRE(capped.size() == ro::net::kMvpCommandMostBytes);
  CHECK(capped[2] == static_cast<std::uint8_t>(ro::net::kMvpCommandMostBytes));
  CHECK(capped[kNoIdMostBytes - 1] == 'x');
  CHECK(capped[kNoIdMostBytes] == 0x00);
  CHECK(capped[kNoIdMostBytes + 1] == 9);
  CHECK(capped.back() == 0x00);
}

TEST_CASE("Carnet MVP : les queues du groupe, de l'invitation et du résultat", "[net][mvp]") {
  MvpTrackerBook book;

  SECTION("le groupe dit notre compte, derrière le dernier membre") {
    Frame group = GroupFrame(0, 0, 77, 2000001, "Chasse", kThreeMembers);
    PutU32(group.bytes, 2000002);
    REQUIRE(Apply(Framed(kGroupOpcode, std::move(group.bytes)), book) == FrameOutcome::kApplied);
    CHECK(book.group().members.size() == 3);
    CHECK(book.group().self_user_id == std::optional<std::uint32_t>(2000002));
    CHECK(book.extensions_seen());
  }
  SECTION("dans aucun groupe, il le dit aussi") {
    Frame none = GroupFrame(0, 0, 0, 0, "", {});
    PutU32(none.bytes, 0);
    REQUIRE(Apply(Framed(kGroupOpcode, std::move(none.bytes)), book) == FrameOutcome::kApplied);
    CHECK(book.group().self_user_id == std::optional<std::uint32_t>(0));
  }
  SECTION("l'invitation dit son origine, à l'octet 47") {
    Frame invite = GroupFrame(1, 0, 12, 0, "Les Chasseurs", {});
    PutU8(invite.bytes, 3);
    REQUIRE(Apply(Framed(kGroupOpcode, std::move(invite.bytes)), book) == FrameOutcome::kApplied);
    CHECK(book.pending_invite() == 12);
    CHECK(book.invite_origin() == ro::sim::MvpInviteOrigin::kPushed);
    CHECK(book.extensions_seen());
  }
  SECTION("une origine inconnue de cette version se lit « inconnue »") {
    Frame invite = GroupFrame(1, 0, 12, 0, "Les Chasseurs", {});
    PutU8(invite.bytes, 200);
    REQUIRE(Apply(Framed(kGroupOpcode, std::move(invite.bytes)), book) == FrameOutcome::kApplied);
    CHECK(book.invite_origin() == ro::sim::MvpInviteOrigin::kUnknown);
  }
  SECTION("le résultat dit sa commande, à l'octet 47") {
    Frame result = GroupFrame(2, 14, 0, 0, "", {});
    PutU32(result.bytes, 0x0A0B0C0DU);
    PutU8(result.bytes, 10);
    REQUIRE(Apply(Framed(kGroupOpcode, std::move(result.bytes)), book) == FrameOutcome::kApplied);
    CHECK(book.last_result() == 14);
    REQUIRE(book.last_result_tag().has_value());
    CHECK(book.last_result_tag()->request_id == 0x0A0B0C0DU);
    CHECK(book.last_result_tag()->command == 10);
  }
  SECTION("un octet au-delà d'une queue connue s'ignore") {
    Frame group = GroupFrame(0, 0, 77, 1, "Chasse", kThreeMembers);
    PutU32(group.bytes, 1);
    PutU8(group.bytes, 0xEE);
    REQUIRE(Apply(Framed(kGroupOpcode, std::move(group.bytes)), book) == FrameOutcome::kApplied);
    CHECK(book.group().self_user_id == std::optional<std::uint32_t>(1));
    Frame result = GroupFrame(2, 0, 0, 0, "", {});
    PutU32(result.bytes, 5);
    PutU8(result.bytes, 4);
    PutU8(result.bytes, 0xEE);
    REQUIRE(Apply(Framed(kGroupOpcode, std::move(result.bytes)), book) == FrameOutcome::kApplied);
    REQUIRE(book.last_result_tag().has_value());
    CHECK(book.last_result_tag()->request_id == 5);
  }
}

TEST_CASE("Carnet MVP : sans queue, les trames d'avant se lisent comme avant", "[net][mvp]") {
  MvpTrackerBook book;
  REQUIRE(Apply(GroupFrame(0, 0, 77, 1, "Chasse", kThreeMembers), book) == FrameOutcome::kApplied);
  CHECK_FALSE(book.group().self_user_id.has_value());
  // L'invitation et le résultat de 47 octets.
  REQUIRE(Apply(GroupFrame(1, 0, 12, 0, "Les Chasseurs", {}), book) == FrameOutcome::kApplied);
  CHECK(book.invite_origin() == ro::sim::MvpInviteOrigin::kUnknown);
  REQUIRE(Apply(GroupFrame(2, 7, 0, 0, "", {}), book) == FrameOutcome::kApplied);
  CHECK_FALSE(book.last_result_tag().has_value());
  CHECK_FALSE(book.extensions_seen());

  // UNE QUEUE DE RÉSULTAT INCOMPLÈTE n'en est pas une.
  Frame short_tag = GroupFrame(2, 7, 0, 0, "", {});
  PutU32(short_tag.bytes, 5);
  REQUIRE(Apply(Framed(kGroupOpcode, std::move(short_tag.bytes)), book) == FrameOutcome::kApplied);
  CHECK_FALSE(book.last_result_tag().has_value());
  CHECK_FALSE(book.extensions_seen());
}

TEST_CASE("Carnet MVP : un groupe tronqué ne dit pas notre compte", "[net][mvp]") {
  MvpTrackerBook book;
  // Le dernier membre perd six octets ; quatre octets de queue n'y suppléent pas.
  Frame group = Cut(GroupFrame(0, 0, 77, 1, "Chasse", kThreeMembers), 6);
  PutU32(group.bytes, 1);
  CHECK(Apply(Framed(kGroupOpcode, std::move(group.bytes)), book) == FrameOutcome::kUndecodable);
  CHECK(book.group().members.empty());
  CHECK_FALSE(book.group().self_user_id.has_value());
  CHECK_FALSE(book.extensions_seen());
}
