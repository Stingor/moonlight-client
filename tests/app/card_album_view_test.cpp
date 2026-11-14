// L'ALBUM DE CARTES, VU DE LA PARTIE — contre un serveur en boucle locale.
//
// 🔴 CE BANC TIENT CE QUE LE SERVEUR NE DIT PAS ET QUE NOUS DEVONS DIRE :
//
//   · un sacrifice réussi range AUSSITÔT le reste de la pile, et le compte rendu
//     attend ce dépôt pour dire les deux ensemble ;
//   · un dépôt enchaîné REFUSÉ dit quand même que la pochette s'est ouverte —
//     la carte, elle, est consommée ;
//   · un état reçu sans commande en vol se formule d'après ce qui a changé ;
//   · le compte rendu s'éteint, à l'heure.

#include "items/card_album_view.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/card_album.h"
#include "ro/sim/world.h"

#include "support/loopback_server.h"
#include "support/temp_directory.h"
#include "support/wire_bytes.h"

using moonclient::CardAlbumPieces;
using moonclient::CardAlbumView;
using ro::net::Frame;
using ro::sim::CardAlbumResult;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::TempDirectory;
using ro::ui::CardAlbumTone;
using ro::ui::CardAlbumVerb;
using ro::ui::CardAlbumWindowAction;
using ro::ui::CardAlbumWindowState;

namespace {

constexpr std::uint32_t kPoring = 4001;
constexpr std::uint32_t kPhreeoni = 4121;
constexpr std::size_t kCommandSize = ro::proto::CzMoonlightCardAlbumCmd::kWireSize;
constexpr std::size_t kCommandByte = 4;
constexpr std::size_t kArgByte = 5;
constexpr std::size_t kAmountByte = 9;

// L'état complet : Poring ouverte avec `poring` copies, Phreeoni ouverte ou non.
[[nodiscard]] Frame State(CardAlbumResult result, std::uint16_t poring, bool phreeoni_open,
                          std::uint16_t phreeoni) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcMoonlightCardAlbum::kOpcode);
  PutU16(bytes, 0);
  bytes.push_back(static_cast<std::uint8_t>(result));
  PutU16(bytes, 2);
  PutU32(bytes, kPoring);
  PutU16(bytes, poring);
  bytes.push_back(1);
  PutU32(bytes, 0x10);
  bytes.push_back(0);
  PutU32(bytes, kPhreeoni);
  PutU16(bytes, phreeoni);
  bytes.push_back(phreeoni_open ? 1 : 0);
  PutU32(bytes, 0x02);
  bytes.push_back(2);
  bytes[2] = static_cast<std::uint8_t>(bytes.size() & 0xFFU);
  bytes[3] = static_cast<std::uint8_t>(bytes.size() >> 8U);
  return Frame{.opcode = ro::proto::ZcMoonlightCardAlbum::kOpcode, .bytes = bytes};
}

struct Bench {
  LoopbackServer server;
  TempDirectory root;
  ro::net::Connection link;
  ro::sim::World world;
  CardAlbumWindowState state;
  CardAlbumView album{root.path()};

  Bench() : link(Open(server)) {}

  static ro::net::Connection Open(LoopbackServer& server) {
    REQUIRE(server.listening());
    auto link = ro::net::Connection::Open("127.0.0.1", server.port());
    REQUIRE(link.has_value());
    REQUIRE(server.Accept());
    return std::move(*link);
  }

  // La commande que le serveur vient de recevoir.
  [[nodiscard]] std::vector<std::uint8_t> Command() { return server.Receive(kCommandSize); }

  const ro::ui::CardAlbumView& Compose() {
    album.Compose(CardAlbumPieces{}, world, state, false);
    return album.view();
  }
};

}  // namespace

TEST_CASE("Album : l'ouvrir demande l'état au serveur", "[app][album]") {
  Bench bench;
  bench.album.Toggle(bench.state, bench.link, 0);
  CHECK(bench.state.open);
  const std::vector<std::uint8_t> asked = bench.Command();
  REQUIRE(asked.size() == kCommandSize);
  CHECK(asked[kCommandByte] == static_cast<std::uint8_t>(ro::net::CardAlbumCommand::kRefresh));
  CHECK(bench.Compose().loading);

  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kOk, 3, false, 0), bench.link, 0) ==
          ro::net::FrameOutcome::kApplied);
  const ro::ui::CardAlbumView& view = bench.Compose();
  CHECK_FALSE(view.loading);
  REQUIRE(view.pockets.size() == 2);
  CHECK(view.pockets[0].families == ro::ui::kCardFamilyArmor);
  CHECK(view.pockets[0].slot_label == "Armor");
  CHECK(view.pockets[1].families == ro::ui::kCardFamilyWeapon);
  CHECK(view.unlocked_count == 1);
  // Un rafraîchissement demandé n'est pas un événement : rien à dire.
  CHECK(view.notice.empty());
}

TEST_CASE("Album : un sacrifice réussi range le reste de la pile", "[app][album]") {
  Bench bench;
  bench.album.Toggle(bench.state, bench.link, 0);
  (void)bench.Command();
  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kOk, 3, false, 0), bench.link, 0) ==
          ro::net::FrameOutcome::kApplied);
  (void)bench.Compose();

  CardAlbumWindowAction sacrifice;
  sacrifice.verb = CardAlbumVerb::kSacrifice;
  sacrifice.slot = 9;
  sacrifice.card_id = kPhreeoni;
  sacrifice.amount = 1;
  sacrifice.rest = 2;
  bench.album.Apply(sacrifice, bench.link, 0);
  const std::vector<std::uint8_t> unlock = bench.Command();
  CHECK(unlock[kCommandByte] == static_cast<std::uint8_t>(ro::net::CardAlbumCommand::kUnlock));
  CHECK(unlock[kArgByte] == 9);
  // 🔴 UNE copie, toujours.
  CHECK(unlock[kAmountByte] == 1);

  // Le serveur confirme : le dépôt du reste part tout seul, et rien ne se dit
  // encore — le compte rendu attend le dépôt.
  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kOk, 3, true, 0), bench.link, 1) ==
          ro::net::FrameOutcome::kApplied);
  const std::vector<std::uint8_t> put = bench.Command();
  CHECK(put[kCommandByte] == static_cast<std::uint8_t>(ro::net::CardAlbumCommand::kPut));
  CHECK(put[kArgByte] == 9);
  CHECK(put[kAmountByte] == 2);
  CHECK(bench.Compose().notice.empty());

  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kOk, 3, true, 2), bench.link, 2) ==
          ro::net::FrameOutcome::kApplied);
  const ro::ui::CardAlbumView& view = bench.Compose();
  CHECK(view.notice_tone == CardAlbumTone::kDone);
  CHECK(std::string(view.notice).find("DÉBLOQUÉE, et 2") != std::string::npos);
  CHECK(view.highlight_card == kPhreeoni);
}

TEST_CASE("Album : un dépôt enchaîné refusé dit quand même le sacrifice", "[app][album]") {
  Bench bench;
  bench.album.Toggle(bench.state, bench.link, 0);
  (void)bench.Command();
  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kOk, 3, false, 0), bench.link, 0) ==
          ro::net::FrameOutcome::kApplied);
  CardAlbumWindowAction sacrifice;
  sacrifice.verb = CardAlbumVerb::kSacrifice;
  sacrifice.slot = 9;
  sacrifice.card_id = kPhreeoni;
  sacrifice.rest = 4;
  bench.album.Apply(sacrifice, bench.link, 0);
  (void)bench.Command();
  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kOk, 3, true, 0), bench.link, 1) ==
          ro::net::FrameOutcome::kApplied);
  (void)bench.Command();
  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kStackFull, 3, true, 0), bench.link, 2) ==
          ro::net::FrameOutcome::kApplied);
  const ro::ui::CardAlbumView& view = bench.Compose();
  CHECK(view.notice_tone == CardAlbumTone::kWarning);
  CHECK(std::string(view.notice).find("DÉBLOQUÉE, mais") != std::string::npos);
}

TEST_CASE("Album : un sacrifice refusé ne range rien", "[app][album]") {
  Bench bench;
  bench.album.Toggle(bench.state, bench.link, 0);
  (void)bench.Command();
  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kOk, 3, false, 0), bench.link, 0) ==
          ro::net::FrameOutcome::kApplied);
  CardAlbumWindowAction sacrifice;
  sacrifice.verb = CardAlbumVerb::kSacrifice;
  sacrifice.slot = 9;
  sacrifice.card_id = kPhreeoni;
  sacrifice.rest = 4;
  bench.album.Apply(sacrifice, bench.link, 0);
  (void)bench.Command();
  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kBound, 3, false, 0), bench.link, 1) ==
          ro::net::FrameOutcome::kApplied);
  const ro::ui::CardAlbumView& view = bench.Compose();
  CHECK(view.notice_tone == CardAlbumTone::kRefusal);
  CHECK(view.highlight_card == 0);
  // Aucun dépôt n'est parti : le prochain octet reçu serait une attente vaine.
  CHECK(bench.server.Receive(kCommandSize, 100).empty());
}

TEST_CASE("Album : un état reçu sans demande dit ce qui a changé", "[app][album]") {
  Bench bench;
  bench.album.Toggle(bench.state, bench.link, 0);
  (void)bench.Command();
  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kOk, 3, false, 0), bench.link, 0) ==
          ro::net::FrameOutcome::kApplied);
  // `@storealbum` : la réserve grandit sans que rien ne soit parti.
  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kOk, 8, true, 1), bench.link, 5) ==
          ro::net::FrameOutcome::kApplied);
  const ro::ui::CardAlbumView& view = bench.Compose();
  CHECK(std::string(view.notice).find("Album mis à jour : 6 copie(s)") != std::string::npos);
  CHECK(std::string(view.notice).find("1 pochette(s)") != std::string::npos);
}

TEST_CASE("Album : le compte rendu s'éteint à l'heure", "[app][album]") {
  Bench bench;
  bench.album.Apply(CardAlbumWindowAction{.refused_not_a_card = true}, bench.link, 100);
  CHECK_FALSE(bench.Compose().notice.empty());
  bench.album.Apply(CardAlbumWindowAction{}, bench.link, 100 + 5999);
  CHECK_FALSE(bench.Compose().notice.empty());
  bench.album.Apply(CardAlbumWindowAction{}, bench.link, 100 + 6000);
  CHECK(bench.Compose().notice.empty());
}

TEST_CASE("Album : tenu ailleurs, il se vide et le dit", "[app][album]") {
  Bench bench;
  bench.album.Toggle(bench.state, bench.link, 0);
  (void)bench.Command();
  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kInUse, 3, true, 1), bench.link, 0) ==
          ro::net::FrameOutcome::kApplied);
  const ro::ui::CardAlbumView& view = bench.Compose();
  CHECK(view.blocked);
  CHECK(view.pockets.empty());
  CHECK(view.notice_tone == CardAlbumTone::kRefusal);
}

// ── « VOIR DANS L'ALBUM » ───────────────────────────────────────────────────

TEST_CASE("Voir dans l'album : fermé, il s'ouvre, charge, puis saute", "[app][album]") {
  Bench bench;
  bench.album.Show(kPhreeoni, bench.state, bench.link, 0);
  CHECK(bench.state.open);
  CHECK(bench.state.raise);
  // L'ouverture demande l'état, comme le raccourci.
  const std::vector<std::uint8_t> asked = bench.Command();
  REQUIRE(asked.size() == kCommandSize);
  CHECK(asked[kCommandByte] == static_cast<std::uint8_t>(ro::net::CardAlbumCommand::kRefresh));
  // 🔴 RIEN NE TOURNE AVANT LA RÉPONSE : les pages sont encore vides.
  CHECK(bench.Compose().highlight_serial == 0);

  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kOk, 3, false, 0), bench.link, 7) ==
          ro::net::FrameOutcome::kApplied);
  const ro::ui::CardAlbumView& view = bench.Compose();
  // Scellée, la pochette se montre quand même.
  CHECK(view.highlight_card == kPhreeoni);
  CHECK(view.highlight_serial == 1);
  CHECK(view.highlight_reveal);
  CHECK(view.notice.empty());
}

TEST_CASE("Voir dans l'album : ouvert et chargé, chaque demande tourne une fois", "[app][album]") {
  Bench bench;
  bench.album.Toggle(bench.state, bench.link, 0);
  (void)bench.Command();
  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kOk, 3, false, 0), bench.link, 0) ==
          ro::net::FrameOutcome::kApplied);
  CHECK(bench.Compose().highlight_serial == 0);
  bench.album.Show(kPoring, bench.state, bench.link, 1);
  CHECK(bench.Compose().highlight_card == kPoring);
  CHECK(bench.Compose().highlight_serial == 1);
  // La même carte redemandée : un NOUVEAU numéro, les pages retournent à elle.
  bench.album.Show(kPoring, bench.state, bench.link, 2);
  CHECK(bench.Compose().highlight_serial == 2);
  // Rien n'est reparti au serveur : le catalogue était en main.
  CHECK(bench.server.Receive(kCommandSize, 100).empty());
}

TEST_CASE("Voir dans l'album : demandé pendant un ordre, il l'emporte à l'arrivée",
          "[app][album]") {
  Bench bench;
  bench.album.Toggle(bench.state, bench.link, 0);
  (void)bench.Command();
  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kOk, 3, true, 0), bench.link, 0) ==
          ro::net::FrameOutcome::kApplied);
  // Un dépôt de Phreeoni part…
  CardAlbumWindowAction deposit;
  deposit.verb = CardAlbumVerb::kDeposit;
  deposit.slot = 9;
  deposit.card_id = kPhreeoni;
  deposit.amount = 2;
  bench.album.Apply(deposit, bench.link, 1);
  (void)bench.Command();
  // … et le joueur demande Poring avant la réponse.
  bench.album.Show(kPoring, bench.state, bench.link, 2);
  CHECK(bench.Compose().highlight_serial == 0);
  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kOk, 3, true, 2), bench.link, 3) ==
          ro::net::FrameOutcome::kApplied);
  const ro::ui::CardAlbumView& view = bench.Compose();
  CHECK(view.highlight_card == kPoring);
  CHECK(view.highlight_reveal);
  // L'ordre garde son compte rendu.
  CHECK(view.notice_tone == CardAlbumTone::kDone);
  CHECK(std::string(view.notice).find("rangée(s) dans l'album") != std::string::npos);
}

TEST_CASE("Voir dans l'album : sans pochette pendant un ordre, les deux se disent",
          "[app][album]") {
  constexpr std::uint32_t kUnlistedCard = 4500;
  Bench bench;
  bench.album.Toggle(bench.state, bench.link, 0);
  (void)bench.Command();
  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kOk, 3, true, 0), bench.link, 0) ==
          ro::net::FrameOutcome::kApplied);
  CardAlbumWindowAction deposit;
  deposit.verb = CardAlbumVerb::kDeposit;
  deposit.slot = 9;
  deposit.card_id = kPhreeoni;
  deposit.amount = 2;
  bench.album.Apply(deposit, bench.link, 1);
  (void)bench.Command();
  bench.album.Show(kUnlistedCard, bench.state, bench.link, 2);
  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kOk, 3, true, 2), bench.link, 3) ==
          ro::net::FrameOutcome::kApplied);
  const ro::ui::CardAlbumView& view = bench.Compose();
  const std::string notice(view.notice);
  // Le dépôt, PUIS l'avertissement, sur deux lignes.
  const std::size_t deposit_at = notice.find("rangée(s) dans l'album");
  const std::size_t line_break_at = notice.find('\n');
  const std::size_t warning_at = notice.find("pas de pochette");
  REQUIRE(deposit_at != std::string::npos);
  REQUIRE(line_break_at != std::string::npos);
  REQUIRE(warning_at != std::string::npos);
  CHECK(deposit_at < line_break_at);
  CHECK(line_break_at < warning_at);
  CHECK(view.notice_tone == CardAlbumTone::kWarning);
  // Le surlignage reste celui du dépôt : la carte demandée n'a nulle part où mener.
  CHECK(view.highlight_card == kPhreeoni);
}

TEST_CASE("Voir dans l'album : sans pochette, l'album le dit et ne tourne pas", "[app][album]") {
  constexpr std::uint32_t kUnlistedCard = 4500;
  constexpr std::uint32_t kRedPotion = 501;
  Bench bench;
  bench.album.NoteCardItems({kPhreeoni, kUnlistedCard, kPoring});
  // Avant la réponse, rien ne prouve l'absence : l'entrée reste offerte.
  CHECK(ro::ui::CardAlbumEntryOf(bench.album.LinkFacts(true), kUnlistedCard) ==
        ro::ui::CardAlbumEntry::kOffered);
  bench.album.Show(kUnlistedCard, bench.state, bench.link, 0);
  (void)bench.Command();
  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kOk, 3, false, 0), bench.link, 0) ==
          ro::net::FrameOutcome::kApplied);
  const ro::ui::CardAlbumView& view = bench.Compose();
  CHECK(view.highlight_serial == 0);
  CHECK(view.notice_tone == CardAlbumTone::kWarning);
  CHECK(std::string(view.notice).find("pas de pochette") != std::string::npos);
  // Le catalogue arrivé, le menu la grise.
  const ro::ui::CardAlbumLinkFacts facts = bench.album.LinkFacts(true);
  CHECK(ro::ui::CardAlbumEntryOf(facts, kUnlistedCard) == ro::ui::CardAlbumEntry::kNoPocket);
  CHECK(ro::ui::CardAlbumEntryOf(facts, kPhreeoni) == ro::ui::CardAlbumEntry::kOffered);
  CHECK(ro::ui::CardAlbumEntryOf(facts, kRedPotion) == ro::ui::CardAlbumEntry::kAbsent);
}

// ── LA PATIENCE ─────────────────────────────────────────────────────────────

TEST_CASE("Album : sans réponse à temps, la demande est abandonnée", "[app][album]") {
  constexpr std::uint32_t kPatienceMs = 3000;
  constexpr std::uint32_t kSentMs = 100;
  Bench bench;
  bench.album.Toggle(bench.state, bench.link, 0);
  (void)bench.Command();
  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kOk, 3, true, 0), bench.link, 0) ==
          ro::net::FrameOutcome::kApplied);
  CardAlbumWindowAction deposit;
  deposit.verb = CardAlbumVerb::kDeposit;
  deposit.slot = 9;
  deposit.card_id = kPhreeoni;
  deposit.amount = 2;
  bench.album.Apply(deposit, bench.link, kSentMs);
  (void)bench.Command();
  bench.album.Show(kPoring, bench.state, bench.link, kSentMs + 1);

  // Juste avant le délai, on attend encore.
  bench.album.Apply(CardAlbumWindowAction{}, bench.link, kSentMs + kPatienceMs - 1);
  CHECK(bench.Compose().notice.empty());

  bench.album.Apply(CardAlbumWindowAction{}, bench.link, kSentMs + kPatienceMs);
  const ro::ui::CardAlbumView& view = bench.Compose();
  CHECK_FALSE(view.loading);
  CHECK(view.notice_tone == CardAlbumTone::kWarning);
  CHECK(std::string(view.notice) == "Le serveur n'a pas répondu.");
  // « Voir dans l'album » tombe avec la demande : pas de saut fantôme plus tard.
  CHECK(view.highlight_serial == 0);

  // 🔴 LA RÉPONSE TARDIVE N'EST QU'UN ÉTAT REÇU : ni compte rendu du dépôt, ni
  // saut vers la carte demandée.
  REQUIRE(bench.album.ApplyFrame(State(CardAlbumResult::kOk, 3, true, 2), bench.link,
                                 kSentMs + 2 * kPatienceMs) == ro::net::FrameOutcome::kApplied);
  const ro::ui::CardAlbumView& late = bench.Compose();
  const std::string notice(late.notice);
  CHECK(notice.find("rangée(s) dans l'album") == std::string::npos);
  CHECK(notice.find("Album mis à jour : 2 copie(s)") != std::string::npos);
  CHECK(late.highlight_serial == 0);
  CHECK(late.highlight_card == 0);
}

TEST_CASE("Album : l'ouverture sans réponse cesse de charger", "[app][album]") {
  constexpr std::uint32_t kPatienceMs = 3000;
  Bench bench;
  bench.album.Toggle(bench.state, bench.link, 0);
  (void)bench.Command();
  CHECK(bench.Compose().loading);
  bench.album.Apply(CardAlbumWindowAction{}, bench.link, kPatienceMs);
  const ro::ui::CardAlbumView& view = bench.Compose();
  CHECK_FALSE(view.loading);
  CHECK(std::string(view.notice) == "Le serveur n'a pas répondu.");
}
