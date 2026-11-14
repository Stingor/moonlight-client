// LES ÉDITIONS DE CARTE QUE LE SERVEUR TIENT — `ZC 0x0F46` à `0x0F48` reçus, le
// document servi réassemblé et décompressé, et ce qui part (ADR 0124).

#include "ro/net/map_edits.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/map_edit_packets.h"
#include "ro/proto/map_packets.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"

using ro::net::FrameOutcome;
using ro::net::MapEditInbox;
using ro::net::MapEditVerdictKind;
using ro::test_support::FrameOf;

namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr std::uint32_t kRevision = 12;
// UN BRUIT QUI SE COMPRESSE MAL : des nombres qui ne se répètent pas, assez
// nombreux pour remplir plusieurs morceaux.
constexpr std::size_t kNoiseLines = 30000;
constexpr std::size_t kNoiseStep = 7919;
constexpr std::size_t kNoiseModulus = 104729;
// Un document qui se répète, comme un vrai : il se compresse bien.
std::string Document(std::size_t lines) {
  std::string text = "version: 1\nmap: 'prontera'\nmodels:\n";
  for (std::size_t rank = 0; rank < lines; ++rank) {
    text += "  - index: " + std::to_string(rank) + "\n    why: \"une raison\"\n";
  }
  return text;
}

std::vector<Bytes> Served(const std::string& text, std::uint32_t revision) {
  auto zipped = ro::net::ZipMapEdit(text);
  REQUIRE(zipped.has_value());
  return ro::proto::EncodeMapEditDoc("prontera", revision, "Stingor",
                                     static_cast<std::uint32_t>(text.size()), *zipped);
}

}  // namespace

TEST_CASE("Éditions de carte : le texte se compresse et se retrouve à l'octet", "[net][map_edit]") {
  const std::string text = Document(500);
  auto zipped = ro::net::ZipMapEdit(text);
  REQUIRE(zipped.has_value());
  CHECK(zipped->size() < text.size());
  auto unzipped = ro::net::UnzipMapEdit(*zipped, static_cast<std::uint32_t>(text.size()));
  REQUIRE(unzipped.has_value());
  CHECK(*unzipped == text);
  // 🔴 UN FLUX QUI DONNE PLUS QU'IL N'ANNONCE s'arrête au plafond, et se refuse.
  CHECK_FALSE(
      ro::net::UnzipMapEdit(*zipped, static_cast<std::uint32_t>(text.size() - 1)).has_value());
  // Et moins aussi.
  CHECK_FALSE(
      ro::net::UnzipMapEdit(*zipped, static_cast<std::uint32_t>(text.size() + 1)).has_value());
  CHECK_FALSE(ro::net::ZipMapEdit("").has_value());
}

TEST_CASE("Éditions de carte : le document servi en morceaux se reçoit entier", "[net][map_edit]") {
  // Assez de texte pour plusieurs morceaux de 16 Kio, même compressé.
  std::string text = Document(20);
  for (std::size_t rank = 0; rank < kNoiseLines; ++rank) {
    text += "# " + std::to_string(rank * kNoiseStep % kNoiseModulus) + "\n";
  }
  const std::vector<Bytes> frames = Served(text, kRevision);
  REQUIRE(frames.size() > 1);
  MapEditInbox inbox;
  for (const Bytes& frame : frames) {
    CHECK(inbox.Apply(FrameOf(frame)) == FrameOutcome::kApplied);
  }
  const std::vector<ro::net::ServedMapEdit> documents = inbox.TakeDocuments();
  REQUIRE(documents.size() == 1);
  CHECK(documents[0].map == "prontera");
  CHECK(documents[0].revision == kRevision);
  CHECK(documents[0].author == "Stingor");
  CHECK(documents[0].text == text);
  CHECK(inbox.TakeRefusals().empty());
}

TEST_CASE("Éditions de carte : un flux illisible ne donne aucun document", "[net][map_edit]") {
  const Bytes garbage(64, 0x5A);
  const std::vector<Bytes> frames =
      ro::proto::EncodeMapEditDoc("prontera", kRevision, "Stingor", 1000, garbage);
  REQUIRE(frames.size() == 1);
  MapEditInbox inbox;
  CHECK(inbox.Apply(FrameOf(frames[0])) == FrameOutcome::kIgnored);
  CHECK(inbox.TakeDocuments().empty());
  CHECK(inbox.TakeRefusals().size() == 1);
}

TEST_CASE("Éditions de carte : l'annonce et le verdict se lisent", "[net][map_edit]") {
  MapEditInbox inbox;
  ro::proto::ZcMoonclientMapEditRevision announce;
  announce.map = "prontera";
  announce.revision = kRevision;
  announce.can_publish = 1;
  CHECK(inbox.Apply(FrameOf(Encode(announce))) == FrameOutcome::kApplied);
  ro::proto::ZcMoonclientMapEditResult verdict;
  verdict.map = "prontera";
  verdict.result = static_cast<std::uint8_t>(MapEditVerdictKind::kStale);
  verdict.revision = kRevision;
  CHECK(inbox.Apply(FrameOf(Encode(verdict))) == FrameOutcome::kApplied);
  // UN VERDICT INCONNU ne passe pas pour un autre.
  verdict.result = 9;
  CHECK(inbox.Apply(FrameOf(Encode(verdict))) == FrameOutcome::kIgnored);

  const auto announces = inbox.TakeAnnounces();
  REQUIRE(announces.size() == 1);
  CHECK(announces[0].revision == kRevision);
  CHECK(announces[0].can_publish);
  const auto verdicts = inbox.TakeVerdicts();
  REQUIRE(verdicts.size() == 1);
  CHECK(verdicts[0].kind == MapEditVerdictKind::kStale);

  ro::proto::ZcMoonlightGmOutfit other;
  CHECK(inbox.Apply(FrameOf(Encode(other))) == FrameOutcome::kUnhandled);
}

TEST_CASE("Éditions de carte : l'envoi part en morceaux que le serveur relit", "[net][map_edit]") {
  ro::test::LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = ro::test::Connect(server);
  const std::string text = Document(50);
  REQUIRE(ro::net::PublishMapEdit(link, "prontera", kRevision, text).has_value());

  // Le serveur lit les morceaux, et les réassemble comme le client.
  ro::proto::MapEditAssembler assembler;
  ro::proto::MapEditAssembler::Step step = ro::proto::MapEditAssembler::Step::kPending;
  while (step == ro::proto::MapEditAssembler::Step::kPending) {
    const Bytes head = server.Receive(4);
    REQUIRE(head.size() == 4);
    const std::size_t length = head[2] | (static_cast<std::size_t>(head[3]) << 8);
    Bytes frame = head;
    const Bytes rest = server.Receive(length - head.size());
    frame.insert(frame.end(), rest.begin(), rest.end());
    auto chunk = ro::proto::DecodeMapEditPut(frame);
    REQUIRE(chunk.has_value());
    CHECK(chunk->revision == kRevision);
    step = assembler.Feed(std::move(*chunk));
  }
  REQUIRE(step == ro::proto::MapEditAssembler::Step::kComplete);
  const ro::proto::AssembledMapEdit sent = assembler.Take();
  auto unzipped = ro::net::UnzipMapEdit(sent.zipped, sent.text_size);
  REQUIRE(unzipped.has_value());
  CHECK(*unzipped == text);
}

TEST_CASE("Éditions de carte : la demande nomme la carte", "[net][map_edit]") {
  ro::test::LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = ro::test::Connect(server);
  REQUIRE(ro::net::RequestMapEdit(link, "prontera").has_value());
  const Bytes request = server.Receive(ro::proto::CzMoonclientMapEditGet::kWireSize);
  REQUIRE(request.size() == ro::proto::CzMoonclientMapEditGet::kWireSize);
  ro::proto::CzMoonclientMapEditGet decoded;
  REQUIRE(Decode(request, decoded));
  CHECK(decoded.map == "prontera");
  CHECK_FALSE(ro::net::RequestMapEdit(link, "../prontera").has_value());
}
