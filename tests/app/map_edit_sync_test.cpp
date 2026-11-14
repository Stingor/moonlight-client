// LES ÉDITIONS DE CARTE QUE LE SERVEUR TIENT, côté jeu : annonce, demande,
// document servi adopté ou masqué, publication et verdicts (ADR 0124). Le
// serveur est joué par l'essai, sur une liaison en boucle locale.

#include "world/map_edit_sync.h"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/mapedit/map_edit.h"
#include "ro/net/map_edits.h"
#include "ro/proto/map_edit_packets.h"
#include "ro/proto/map_packets.h"

#include "support/client_vfs.h"
#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"
#include "support/temp_directory.h"

using moonclient::MapEditStore;
using moonclient::MapEditSync;
using ro::test_support::FrameOf;

namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr const char* kMap = "prontera";
constexpr std::uint8_t kPutOpcodeLow = 0x45;
constexpr std::uint8_t kGetOpcodeLow = 0x49;
constexpr std::uint8_t kCustomOpcodeHigh = 0x0F;
// Assez pour qu'un envoi parti arrive ; un silence se juge sur ce délai.
constexpr int kSilenceMs = 200;
constexpr std::size_t kHeadBytes = 4;

ro::mapedit::MapEdit Edit(const char* why) {
  ro::mapedit::MapEdit edit;
  ro::mapedit::ModelEdit model;
  model.model_index = 0;
  model.model_file = "pont.rsm";
  model.origin = {1.0F, 2.0F, 3.0F};
  model.hidden = true;
  model.why = why;
  edit.SetModel(model);
  return edit;
}

void Announce(MapEditSync& sync, std::uint32_t revision, bool can_publish) {
  ro::proto::ZcMoonclientMapEditRevision announce;
  announce.map = kMap;
  announce.revision = revision;
  announce.can_publish = can_publish ? 1 : 0;
  REQUIRE(sync.ApplyFrame(FrameOf(Encode(announce))) == ro::net::FrameOutcome::kApplied);
}

void Serve(MapEditSync& sync, const std::string& text, std::uint32_t revision) {
  auto zipped = ro::net::ZipMapEdit(text);
  REQUIRE(zipped.has_value());
  for (Bytes& frame : ro::proto::EncodeMapEditDoc(
           kMap, revision, "Stingor", static_cast<std::uint32_t>(text.size()), *zipped)) {
    REQUIRE(sync.ApplyFrame(FrameOf(std::move(frame))) == ro::net::FrameOutcome::kApplied);
  }
}

void Judge(MapEditSync& sync, ro::net::MapEditVerdictKind kind, std::uint32_t revision) {
  ro::proto::ZcMoonclientMapEditResult verdict;
  verdict.map = kMap;
  verdict.result = static_cast<std::uint8_t>(kind);
  verdict.revision = revision;
  REQUIRE(sync.ApplyFrame(FrameOf(Encode(verdict))) == ro::net::FrameOutcome::kApplied);
}

// La prochaine trame que reçoit le serveur, entière ; vide après un silence.
Bytes NextFrame(ro::test::LoopbackServer& server, int timeout_ms = 2000) {
  Bytes frame = server.Receive(kHeadBytes, timeout_ms);
  if (frame.size() != kHeadBytes) {
    return {};
  }
  const std::size_t length = frame[2] | (static_cast<std::size_t>(frame[3]) << 8);
  const Bytes rest = server.Receive(length - kHeadBytes);
  frame.insert(frame.end(), rest.begin(), rest.end());
  return frame;
}

std::string FileText(const MapEditStore& store) {
  std::ifstream stream(store.FileOf(kMap), std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

// Le banc : un client vide, son magasin, et une liaison au serveur de l'essai.
struct Bench {
  ro::test_support::TempDirectory client;
  ro::vfs::Vfs vfs = ro::test_support::EmptyClient(client);
  MapEditStore store;
  ro::test::LoopbackServer server;
  ro::net::Connection link = ro::test::Connect(server);
  MapEditSync sync;

  Bench() {
    store.Open(client.path());
    store.Load(kMap, vfs);
  }
  MapEditSync::Report Step() { return sync.Serve(kMap, store, vfs, link); }
};

}  // namespace

TEST_CASE("Éditions servies : une autre révision se demande, puis s'adopte et s'écrit",
          "[app][map_edit_sync]") {
  Bench bench;
  Announce(bench.sync, 3, false);
  CHECK_FALSE(bench.Step().replaced);
  const Bytes asked = NextFrame(bench.server);
  REQUIRE(asked.size() == ro::proto::CzMoonclientMapEditGet::kWireSize);
  CHECK(asked[0] == kGetOpcodeLow);
  CHECK(asked[1] == kCustomOpcodeHigh);

  Serve(bench.sync, ro::mapedit::MapEditToYaml(Edit("servi"), kMap), 3);
  const MapEditSync::Report report = bench.Step();
  CHECK(report.replaced);
  CHECK_FALSE(report.status.empty());
  CHECK(bench.store.Revision(kMap) == 3);
  CHECK(bench.store.Document(kMap) == Edit("servi"));
  CHECK_FALSE(bench.store.Unsaved(kMap));
  CHECK(FileText(bench.store).find("revision: 3\n") != std::string::npos);
}

TEST_CASE("Éditions servies : la même révision, ou aucune, ne se demande pas",
          "[app][map_edit_sync]") {
  Bench bench;
  Announce(bench.sync, 0, true);
  static_cast<void>(bench.Step());
  CHECK(NextFrame(bench.server, kSilenceMs).empty());
}

TEST_CASE("Éditions servies : un document invalide ne s'applique ni ne s'écrit",
          "[app][map_edit_sync]") {
  Bench bench;
  Serve(bench.sync, ro::mapedit::MapEditToYaml(Edit("autre carte"), "geffen"), 5);
  const MapEditSync::Report report = bench.Step();
  CHECK_FALSE(report.replaced);
  CHECK(bench.store.Document(kMap).empty());
  CHECK(bench.store.Revision(kMap) == 0);
  CHECK(FileText(bench.store).empty());
}

TEST_CASE(
    "Éditions servies : des retouches non enregistrées masquent le document, jusqu'au "
    "rechargement",
    "[app][map_edit_sync]") {
  Bench bench;
  bench.store.Set(kMap, Edit("à moi"));
  Serve(bench.sync, ro::mapedit::MapEditToYaml(Edit("servi"), kMap), 4);
  const MapEditSync::Report report = bench.Step();
  CHECK_FALSE(report.replaced);
  CHECK_FALSE(report.status.empty());
  CHECK(bench.store.Document(kMap) == Edit("à moi"));
  CHECK(bench.store.Unsaved(kMap));
  // Le fichier, lui, est la copie du serveur.
  CHECK(FileText(bench.store).find("revision: 4\n") != std::string::npos);

  const MapEditSync::Report reloaded = bench.sync.Reload(kMap, kMap, bench.store, bench.link);
  CHECK(reloaded.replaced);
  CHECK(bench.store.Document(kMap) == Edit("servi"));
  CHECK(bench.store.Revision(kMap) == 4);
  CHECK_FALSE(bench.store.Unsaved(kMap));
}

TEST_CASE("Éditions servies : rien ne part sans annonce ni droit de publier",
          "[app][map_edit_sync]") {
  Bench bench;
  bench.store.Set(kMap, Edit("à publier"));
  CHECK_FALSE(bench.sync.Publish(kMap, bench.store, bench.link).empty());
  CHECK(NextFrame(bench.server, kSilenceMs).empty());

  Announce(bench.sync, 0, false);
  static_cast<void>(bench.Step());
  CHECK_FALSE(bench.sync.Publish(kMap, bench.store, bench.link).empty());
  CHECK(NextFrame(bench.server, kSilenceMs).empty());
}

TEST_CASE("Éditions servies : publié, accepté, puis reçu en retour sans rien défaire",
          "[app][map_edit_sync]") {
  Bench bench;
  Announce(bench.sync, 0, true);
  static_cast<void>(bench.Step());
  bench.store.Set(kMap, Edit("à publier"));
  REQUIRE(bench.store.Save(kMap).has_value());
  static_cast<void>(bench.sync.Publish(kMap, bench.store, bench.link));
  const Bytes sent = NextFrame(bench.server);
  REQUIRE(sent.size() > kHeadBytes);
  CHECK(sent[0] == kPutOpcodeLow);
  const auto chunk = ro::proto::DecodeMapEditPut(sent);
  REQUIRE(chunk.has_value());
  CHECK(chunk->revision == 0);

  // LE VERDICT AVANT LE DOCUMENT, comme le serveur les envoie.
  Judge(bench.sync, ro::net::MapEditVerdictKind::kAccepted, 1);
  Serve(bench.sync, ro::mapedit::MapEditToYaml(Edit("à publier"), kMap), 1);
  const MapEditSync::Report report = bench.Step();
  // Le document est le sien : le décor ne se repose pas, l'historique reste.
  CHECK_FALSE(report.replaced);
  CHECK(bench.store.Revision(kMap) == 1);
  CHECK(FileText(bench.store).find("revision: 1\n") != std::string::npos);
}

TEST_CASE("Éditions servies : un envoi périmé attend qu'on écrase", "[app][map_edit_sync]") {
  Bench bench;
  Announce(bench.sync, 0, true);
  static_cast<void>(bench.Step());
  bench.store.Set(kMap, Edit("à moi"));
  Judge(bench.sync, ro::net::MapEditVerdictKind::kStale, 6);
  const MapEditSync::Report report = bench.Step();
  CHECK_FALSE(report.status.empty());
  CHECK(bench.sync.Conflicted(kMap));
  // Le document qui a devancé est demandé, pour pouvoir le recharger.
  const Bytes asked = NextFrame(bench.server);
  REQUIRE_FALSE(asked.empty());
  CHECK(asked[0] == kGetOpcodeLow);

  static_cast<void>(bench.sync.Overwrite(kMap, bench.store, bench.link));
  CHECK_FALSE(bench.sync.Conflicted(kMap));
  const auto chunk = ro::proto::DecodeMapEditPut(NextFrame(bench.server));
  REQUIRE(chunk.has_value());
  CHECK(chunk->revision == 6);
}
