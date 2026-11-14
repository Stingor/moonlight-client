// L'ÉTAGE DU CHAMP DE BATAILLE contre un serveur en boucle locale : les
// fenêtres qui suivent la file, l'effectif redemandé, la réponse à
// l'invitation.

#include "game/battleground_stage.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/battleground_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/guild.h"
#include "ro/sim/map_mode.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"
#include "support/temp_directory.h"

using moonclient::BattlegroundContext;
using moonclient::BattlegroundStage;
using moonclient::BattlegroundWindows;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::EntryQueuePhase;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;
using ro::test_support::TempDirectory;
using ro::ui::EntryQueueVerb;

namespace {

using Bytes = std::vector<std::uint8_t>;

// LE NOM QUE LE SERVEUR CONNAÎT, et celui que le joueur lit : ils diffèrent,
// pour qu'on voie lequel part.
constexpr std::string_view kServerName = "Tierra Gorge";
constexpr std::string_view kShownName = "Gorge de Tierra";
constexpr std::string_view kLobby = "Tierra Gorge";

constexpr std::uint32_t kStartMs = 100'000;
constexpr std::uint32_t kSelf = 2'000'001;
constexpr std::uint32_t kSelfChar = 150'001;
constexpr int kSilenceMs = 150;
// Un opcode qu'aucun des deux traducteurs ne réclame.
constexpr std::uint16_t kUnrelatedOpcode = 0x0001;

// Les codes du fil.
constexpr std::uint8_t kWireAccepted = 1;
constexpr std::uint8_t kWireLevel = 6;
constexpr std::uint8_t kWireCancelled = 1;
constexpr std::uint8_t kWireDecline = 2;
constexpr std::int16_t kWireSolo = 1;
constexpr std::int16_t kWireParty = 2;

constexpr std::uint32_t kBadgeItem = 7828;
constexpr std::uint32_t kUnknownItem = 9999999;

constexpr std::string_view kTable = R"(version: 1
fields:
  - server_name: "Tierra Gorge"
    name: { fr: "Gorge de Tierra", en: "Tierra Gorge" }
    objective: { fr: "Détruire une Réserve.", en: "Destroy a Reserve." }
    minimap: "bat_a01.bmp"
    team: { min: 3, max: 3 }
    apply: [solo, party, guild]
    level: { min: 80 }
    rewards:
      win: [{ item: 7828, count: 3 }]
      draw: [{ item: 9999999 }]
      lose: [{ text: { fr: "Rien", en: "Nothing" } }]
  - server_name: "Flavius"
    apply: [solo]
)";

[[nodiscard]] std::uint16_t OpcodeOf(const Bytes& bytes) {
  return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8U));
}

// LE BANC : un serveur, la liaison, le monde, le groupe, la guilde, une
// installation qui porte la table des terrains, l'étage et ses fenêtres.
struct Bench {
  LoopbackServer server;
  ro::net::Connection link;
  ro::sim::World world;
  ro::sim::PartyRoster party;
  ro::sim::GuildBook guild;
  TempDirectory client;
  std::optional<ro::vfs::Vfs> vfs;
  BattlegroundStage stage;
  BattlegroundWindows windows;
  bool link_ready = true;
  // Les chemins d'image que l'étage a demandés.
  std::vector<std::string> images_asked;
  // Les objets de récompense que l'étage a demandés.
  std::vector<std::uint32_t> items_asked;

  Bench() : link(Connect(server)) {
    world.set_player_id(kSelf);
    client.WriteText("data/yaml_files/battleground.yml", std::string(kTable));
    auto mounted = ro::vfs::Vfs::Mount({.root = client.path()});
    REQUIRE(mounted.has_value());
    vfs.emplace(std::move(*mounted));
    stage.StartSession(windows);
  }

  static ro::net::Connection Connect(LoopbackServer& listening) {
    REQUIRE(listening.listening());
    return ro::test::Connect(listening);
  }

  void Advance(std::uint32_t now_ms = kStartMs) {
    stage.Advance(windows, BattlegroundContext{.link = link,
                                               .world = world,
                                               .party = party,
                                               .guild = guild,
                                               .vfs = &*vfs,
                                               .image_at =
                                                   [this](std::string_view path) {
                                                     images_asked.emplace_back(path);
                                                     return ro::ui::SkinImage{};
                                                   },
                                               .item_at =
                                                   [this](std::uint32_t item_id) {
                                                     items_asked.push_back(item_id);
                                                     return moonclient::BattlegroundItemFace{
                                                         .name = item_id == kBadgeItem
                                                                     ? "Insigne de bravoure"
                                                                     : ""};
                                                   },
                                               .link_ready = link_ready,
                                               .own_char_id = kSelfChar,
                                               .now_ms = now_ms});
  }

  // Ce que le joueur demande, lu à l'avancée qui suit. Une inscription y est
  // confirmée, comme après le « oui » de sa question.
  void Answer(EntryQueueVerb verb, std::size_t field_rank = 0, std::uint32_t now_ms = kStartMs) {
    windows.answer =
        ro::ui::EntryQueueAction{.verb = verb, .field_rank = field_rank, .confirmed = true};
    Advance(now_ms);
  }

  // Le clic sur une inscription, avant toute question.
  void AskApply(EntryQueueVerb verb) {
    windows.answer = ro::ui::EntryQueueAction{.verb = verb, .field_rank = 0};
    Advance();
  }

  void Apply(Bytes bytes, std::uint32_t now_ms = kStartMs) {
    REQUIRE(stage.ApplyFrame(FrameOf(std::move(bytes)), world, now_ms) == FrameOutcome::kApplied);
  }

  // La trame suivante que le serveur reçoit, de la taille attendue.
  [[nodiscard]] Bytes Next(std::size_t size) {
    Bytes bytes = server.Receive(size);
    REQUIRE(bytes.size() == size);
    return bytes;
  }

  [[nodiscard]] bool Silent() { return server.Receive(1, kSilenceMs).empty(); }

  // Inscrit seul au premier terrain, puis accepté en file.
  void Queue(std::uint32_t now_ms = kStartMs) {
    Answer(EntryQueueVerb::kApplySolo, 0, now_ms);
    static_cast<void>(Next(ro::proto::PacketBgqueueRegister::kWireSize));
    Apply(ApplyAck(kWireAccepted), now_ms);
    Advance(now_ms);
  }

  [[nodiscard]] static Bytes ApplyAck(std::uint8_t code) {
    return ro::proto::Encode(
        ro::proto::PacketBgqueueAck{.type = code, .bg_name = std::string(kServerName)});
  }

  [[nodiscard]] std::string notice() const { return std::string(windows.view->notice); }
};

}  // namespace

TEST_CASE("Étage du champ : neuf, il n'est pas en file", "[app][battleground]") {
  moonclient::BattlegroundStage stage;
  CHECK(stage.queue().phase() == EntryQueuePhase::kIdle);
}

TEST_CASE("Étage du champ : la liste montre le nom du joueur et la condition traduite",
          "[app][battleground]") {
  Bench bench;
  bench.windows.window.open = true;
  bench.Advance();
  REQUIRE(bench.windows.view != nullptr);
  REQUIRE(bench.windows.view->fields.size() == 2);
  CHECK(bench.windows.view->fields[0].display_name == kShownName);
  CHECK(bench.windows.view->fields[0].condition_text.find("80") != std::string_view::npos);
  // Sans nom pour le joueur, celui du serveur.
  CHECK(bench.windows.view->fields[1].display_name == "Flavius");
  CHECK(bench.windows.view->fields[1].condition_text.empty());
  CHECK(bench.windows.view->fields[1].team_text.empty());
  // La minimap du terrain, demandée liste ouverte seulement.
  CHECK(bench.images_asked == std::vector<std::string>{"map\\bat_a01.bmp"});
  bench.images_asked.clear();
  bench.items_asked.clear();
  bench.windows.window.open = false;
  bench.Advance();
  CHECK(bench.images_asked.empty());
  CHECK(bench.items_asked.empty());
}

TEST_CASE(
    "Étage du champ : le détail lit l'objectif et l'effectif, et nomme les récompenses par la "
    "base d'objets",
    "[app][battleground]") {
  Bench bench;
  bench.windows.window.open = true;
  bench.Advance();
  REQUIRE(bench.windows.view != nullptr);
  const ro::ui::EntryQueueFieldView& gorge = bench.windows.view->fields[0];
  CHECK(gorge.victory == "Détruire une Réserve.");
  CHECK(gorge.team_text.find('3') != std::string_view::npos);
  REQUIRE(gorge.reward_win.size() == 1);
  CHECK(gorge.reward_win[0].text.find("Insigne de bravoure") != std::string_view::npos);
  CHECK(gorge.reward_win[0].text.find('3') != std::string_view::npos);
  // Un objet absent de la base se montre par son numéro, sans rien casser.
  REQUIRE(gorge.reward_draw.size() == 1);
  CHECK(gorge.reward_draw[0].text == "#9999999");
  REQUIRE(gorge.reward_lose.size() == 1);
  CHECK(gorge.reward_lose[0].text == "Rien");
  CHECK(bench.items_asked == std::vector<std::uint32_t>{kBadgeItem, kUnknownItem});
  CHECK(bench.windows.view->fields[1].reward_win.empty());
  // Un objet porte son numéro, que son lien ouvre ; un texte libre, aucun.
  CHECK(gorge.reward_win[0].item_id == kBadgeItem);
  CHECK(gorge.reward_lose[0].item_id == 0);
}

TEST_CASE("Étage du champ : le geste sur le lien d'une récompense passe à la partie",
          "[app][battleground]") {
  Bench bench;
  bench.windows.window.open = true;
  bench.Advance();
  CHECK(bench.stage.TakeItemGesture().gesture == ro::ui::ChatLinkGesture::kNothing);
  bench.windows.answer = ro::ui::EntryQueueAction{
      .link = ro::ui::ChatLinkAsk{.gesture = ro::ui::ChatLinkGesture::kWhoDrops,
                                  .item_id = kBadgeItem}};
  bench.Advance();
  const ro::ui::ChatLinkAsk asked = bench.stage.TakeItemGesture();
  CHECK(asked.gesture == ro::ui::ChatLinkGesture::kWhoDrops);
  CHECK(asked.item_id == kBadgeItem);
  // Consommé : le geste ne se rejoue pas de lui-même.
  CHECK(bench.stage.TakeItemGesture().gesture == ro::ui::ChatLinkGesture::kNothing);
  // Rien n'est parti au serveur.
  CHECK(bench.Silent());
}

TEST_CASE(
    "Étage du champ : l'inscription part sous le nom du serveur, et l'acceptation ouvre "
    "« en attente »",
    "[app][battleground]") {
  Bench bench;
  bench.windows.window.open = true;
  bench.Answer(EntryQueueVerb::kApplySolo);
  const Bytes sent = bench.Next(ro::proto::PacketBgqueueRegister::kWireSize);
  ro::proto::PacketBgqueueRegister request;
  REQUIRE(ro::proto::Decode(sent, request));
  CHECK(request.type == kWireSolo);
  CHECK(request.bg_name == kServerName);
  CHECK(bench.stage.queue().phase() == EntryQueuePhase::kApplying);
  CHECK(bench.windows.view->applying);
  CHECK_FALSE(bench.windows.queued);

  bench.Apply(Bench::ApplyAck(kWireAccepted));
  bench.Advance();
  CHECK(bench.windows.window.standby_open);
  CHECK_FALSE(bench.windows.window.open);
  CHECK(bench.windows.queued);
  CHECK(bench.windows.queued_name == kShownName);
  CHECK(bench.windows.view->queued_name == kShownName);
  const std::vector<moonclient::InterfaceSoundFile> sounds = bench.stage.TakeSounds();
  REQUIRE(sounds.size() == 1);
  CHECK(sounds[0].path == "se_btg_request.wav");

  // FERMÉE PAR LE JOUEUR, « en attente » ne se rouvre pas d'elle-même.
  bench.windows.window.standby_open = false;
  bench.Advance();
  CHECK_FALSE(bench.windows.window.standby_open);
}

TEST_CASE("Étage du champ : l'annulation acceptée ferme « en attente » et sort de la file",
          "[app][battleground]") {
  Bench bench;
  bench.Queue();
  bench.Answer(EntryQueueVerb::kCancel);
  const Bytes sent = bench.Next(ro::proto::PacketBgqueueRevokeReq::kWireSize);
  CHECK(OpcodeOf(sent) == ro::proto::PacketBgqueueRevokeReq::kOpcode);
  CHECK(bench.stage.queue().phase() == EntryQueuePhase::kCancelling);

  bench.Apply(ro::proto::Encode(ro::proto::PacketBgqueueNoticeDelete{
      .type = kWireCancelled, .bg_name = std::string(kServerName)}));
  bench.Advance();
  CHECK_FALSE(bench.windows.window.standby_open);
  CHECK_FALSE(bench.windows.queued);
  CHECK(bench.windows.queued_name.empty());
  const std::vector<std::string> notices = bench.stage.TakeNotices();
  REQUIRE(notices.size() == 1);
  // Le terrain se nomme comme le joueur le lit, et la sortie n'encombre pas la
  // boîte de résultat.
  CHECK(notices[0].find(kShownName) != std::string::npos);
  CHECK(bench.notice().empty());
}

TEST_CASE("Étage du champ : l'effectif se redemande toutes les dix secondes",
          "[app][battleground]") {
  Bench bench;
  bench.Queue();
  bench.Advance(kStartMs + ro::sim::kEntryQueueRankingPeriodMs - 1);
  CHECK(bench.Silent());
  bench.Advance(kStartMs + ro::sim::kEntryQueueRankingPeriodMs);
  const Bytes asked = bench.Next(ro::proto::PacketBgqueueCheckstate::kWireSize);
  ro::proto::PacketBgqueueCheckstate request;
  REQUIRE(ro::proto::Decode(asked, request));
  CHECK(request.bg_name == kServerName);
  bench.Advance(kStartMs + ro::sim::kEntryQueueRankingPeriodMs + 1);
  CHECK(bench.Silent());
  bench.Advance(kStartMs + 2 * ro::sim::kEntryQueueRankingPeriodMs);
  static_cast<void>(bench.Next(ro::proto::PacketBgqueueCheckstate::kWireSize));

  // La réponse remplit « en attente ».
  bench.Apply(ro::proto::Encode(
      ro::proto::PacketBgqueueNotifyEntry{.name = std::string(kServerName), .position = 5}));
  bench.Advance(kStartMs + 2 * ro::sim::kEntryQueueRankingPeriodMs);
  CHECK(bench.windows.view->count == 5);
  CHECK(bench.windows.view->waited_s == 2 * ro::sim::kEntryQueueRankingPeriodMs / 1000);
}

TEST_CASE("Étage du champ : hors liaison, l'effectif ne se demande pas", "[app][battleground]") {
  Bench bench;
  bench.Queue();
  bench.link_ready = false;
  bench.Advance(kStartMs + ro::sim::kEntryQueueRankingPeriodMs);
  CHECK(bench.Silent());
}

TEST_CASE("Étage du champ : l'invitation ouvre la modale et refuser répond 2",
          "[app][battleground]") {
  Bench bench;
  bench.Queue();
  bench.Apply(ro::proto::Encode(ro::proto::PacketBgqueueBattlebegins{
      .bg_name = std::string(kServerName), .game_name = std::string(kLobby)}));
  bench.Advance();
  CHECK(bench.windows.view->invited);
  CHECK(bench.windows.view->invited_name == kShownName);
  CHECK(bench.windows.view->invitation_s_left ==
        static_cast<int>(ro::sim::kEntryQueueInvitationMs / 1000));
  // Le compte à rebours ne montre zéro qu'une fois l'invitation expirée.
  bench.Advance(kStartMs + ro::sim::kEntryQueueInvitationMs - 1);
  CHECK(bench.windows.view->invitation_s_left == 1);

  bench.Answer(EntryQueueVerb::kDecline);
  const Bytes sent = bench.Next(ro::proto::PacketBgqueueBattlebeginAck::kWireSize);
  ro::proto::PacketBgqueueBattlebeginAck answer;
  REQUIRE(ro::proto::Decode(sent, answer));
  CHECK(answer.result == kWireDecline);
  CHECK(answer.bg_name == kServerName);
  CHECK(answer.game_name == kLobby);
  CHECK(bench.stage.queue().phase() == EntryQueuePhase::kIdle);
  CHECK_FALSE(bench.windows.view->invited);
  CHECK_FALSE(bench.windows.window.standby_open);
}

TEST_CASE("Étage du champ : pendant l'invitation, l'annulation ne part pas",
          "[app][battleground]") {
  Bench bench;
  bench.Queue();
  bench.Apply(ro::proto::Encode(ro::proto::PacketBgqueueBattlebegins{
      .bg_name = std::string(kServerName), .game_name = std::string(kLobby)}));
  bench.Advance();
  bench.Answer(EntryQueueVerb::kCancel);
  CHECK(bench.Silent());
  CHECK(bench.stage.queue().phase() == EntryQueuePhase::kInvited);
  CHECK_FALSE(bench.notice().empty());
}

TEST_CASE("Étage du champ : un refus du serveur s'écrit dans le chat et la boîte, la liste reste",
          "[app][battleground]") {
  Bench bench;
  bench.windows.window.open = true;
  bench.Answer(EntryQueueVerb::kApplySolo);
  static_cast<void>(bench.Next(ro::proto::PacketBgqueueRegister::kWireSize));
  bench.Apply(Bench::ApplyAck(kWireLevel));
  bench.Advance();
  CHECK(bench.stage.queue().phase() == EntryQueuePhase::kIdle);
  CHECK(bench.windows.window.open);
  const std::vector<std::string> notices = bench.stage.TakeNotices();
  REQUIRE(notices.size() == 1);
  CHECK(bench.notice() == notices[0]);

  // Lue, la boîte s'oublie.
  bench.Answer(EntryQueueVerb::kNoticeSeen);
  CHECK(bench.notice().empty());
}

TEST_CASE("Étage du champ : le refus « raison à part » ne fait qu'une ligne",
          "[app][battleground]") {
  Bench bench;
  bench.Answer(EntryQueueVerb::kApplySolo);
  static_cast<void>(bench.Next(ro::proto::PacketBgqueueRegister::kWireSize));
  bench.Apply(Bench::ApplyAck(0));
  bench.Advance();
  CHECK(bench.stage.queue().phase() == EntryQueuePhase::kIdle);
  CHECK(bench.stage.TakeNotices().size() == 1);
  CHECK(bench.notice().empty());
}

TEST_CASE("Étage du champ : en party, seul le chef inscrit", "[app][battleground]") {
  Bench bench;
  bench.party.Replace("Les Lions", {ro::sim::PartyMember{.account_id = kSelf, .leader = false},
                                    ro::sim::PartyMember{.account_id = kSelf + 1, .leader = true}});
  bench.Answer(EntryQueueVerb::kApplyParty);
  CHECK(bench.Silent());
  CHECK_FALSE(bench.notice().empty());
  CHECK(bench.stage.queue().phase() == EntryQueuePhase::kIdle);

  bench.Answer(EntryQueueVerb::kNoticeSeen);
  bench.party.Replace("Les Lions", {ro::sim::PartyMember{.account_id = kSelf, .leader = true}});
  bench.Answer(EntryQueueVerb::kApplyParty);
  ro::proto::PacketBgqueueRegister request;
  REQUIRE(ro::proto::Decode(bench.Next(ro::proto::PacketBgqueueRegister::kWireSize), request));
  CHECK(request.type == kWireParty);
  CHECK(bench.notice().empty());
}

TEST_CASE("Étage du champ : on ne s'inscrit pas depuis un champ de bataille, ni hors liaison",
          "[app][battleground]") {
  Bench bench;
  ro::sim::MapMode battlefield;
  battlefield.battlefield = true;
  bench.world.TellMapMode(battlefield);
  bench.Answer(EntryQueueVerb::kApplySolo);
  CHECK(bench.Silent());
  const std::string on_field = bench.notice();
  CHECK_FALSE(on_field.empty());

  bench.Answer(EntryQueueVerb::kNoticeSeen);
  bench.world.TellMapMode(ro::sim::MapMode{});
  bench.link_ready = false;
  bench.Answer(EntryQueueVerb::kApplySolo);
  CHECK(bench.Silent());
  CHECK_FALSE(bench.notice().empty());
  CHECK(bench.notice() != on_field);
}

TEST_CASE("Étage du champ : la vue porte le refus de chaque inscription avant sa question",
          "[app][battleground]") {
  Bench bench;
  bench.windows.window.open = true;
  bench.Advance();
  REQUIRE(bench.windows.view != nullptr);
  const ro::ui::EntryQueueFieldView* gorge = &bench.windows.view->fields[0];
  CHECK(gorge->solo_refusal.empty());
  // Hors party et hors guilde : les deux inscriptions de groupe sont refusées.
  CHECK_FALSE(gorge->party_refusal.empty());
  CHECK_FALSE(gorge->guild_refusal.empty());

  ro::sim::MapMode battlefield;
  battlefield.battlefield = true;
  bench.world.TellMapMode(battlefield);
  bench.Advance();
  gorge = &bench.windows.view->fields[0];
  CHECK_FALSE(gorge->solo_refusal.empty());
}

TEST_CASE("Étage du champ : un clic refusé ouvre la boîte du refus, un clic accepté attend le oui",
          "[app][battleground]") {
  Bench bench;
  bench.windows.window.open = true;
  // Refusée : la boîte dit pourquoi, rien ne part.
  bench.AskApply(EntryQueueVerb::kApplyParty);
  CHECK(bench.Silent());
  CHECK(bench.notice() == std::string(bench.windows.view->fields[0].party_refusal));

  // Acceptée mais pas encore confirmée : ni boîte ni envoi.
  bench.Answer(EntryQueueVerb::kNoticeSeen);
  bench.AskApply(EntryQueueVerb::kApplySolo);
  CHECK(bench.Silent());
  CHECK(bench.notice().empty());
  CHECK(bench.stage.queue().phase() == EntryQueuePhase::kIdle);

  // Le oui fait partir la demande.
  bench.Answer(EntryQueueVerb::kApplySolo);
  static_cast<void>(bench.Next(ro::proto::PacketBgqueueRegister::kWireSize));
  CHECK(bench.stage.queue().phase() == EntryQueuePhase::kApplying);
}

TEST_CASE("Étage du champ : l'icône ouvre la liste hors file, « en attente » en file",
          "[app][battleground]") {
  Bench bench;
  bench.stage.RequestOpen(bench.windows);
  CHECK(bench.windows.window.open);
  CHECK_FALSE(bench.windows.window.standby_open);
  bench.stage.RequestOpen(bench.windows);
  CHECK_FALSE(bench.windows.window.open);

  bench.Queue();
  bench.windows.window.standby_open = false;
  bench.stage.RequestOpen(bench.windows);
  CHECK(bench.windows.window.standby_open);
  CHECK_FALSE(bench.windows.window.open);
  // Enfoncée, l'icône referme « en attente ».
  bench.stage.RequestOpen(bench.windows);
  CHECK_FALSE(bench.windows.window.standby_open);
  CHECK_FALSE(bench.windows.window.open);
}

TEST_CASE("Étage du champ : pendant la demande, l'icône referme la liste sans rien ouvrir",
          "[app][battleground]") {
  Bench bench;
  bench.windows.window.open = true;
  bench.Answer(EntryQueueVerb::kApplySolo);
  static_cast<void>(bench.Next(ro::proto::PacketBgqueueRegister::kWireSize));
  REQUIRE(bench.stage.queue().phase() == EntryQueuePhase::kApplying);
  REQUIRE(bench.windows.window.open);

  bench.stage.RequestOpen(bench.windows);
  CHECK_FALSE(bench.windows.window.open);
  CHECK_FALSE(bench.windows.window.standby_open);
  // Pas encore en file : le clic suivant rouvre la liste.
  bench.stage.RequestOpen(bench.windows);
  CHECK(bench.windows.window.open);
  CHECK_FALSE(bench.windows.window.standby_open);
}

TEST_CASE("Étage du champ : en file, l'icône ferme aussi la liste rouverte à côté",
          "[app][battleground]") {
  Bench bench;
  bench.Queue();
  REQUIRE(bench.windows.window.standby_open);
  bench.windows.window.open = true;
  bench.stage.RequestOpen(bench.windows);
  CHECK_FALSE(bench.windows.window.open);
  CHECK_FALSE(bench.windows.window.standby_open);
}

TEST_CASE("Étage du champ : un chef qui nous inscrit nous met en file", "[app][battleground]") {
  Bench bench;
  bench.Apply(Bench::ApplyAck(kWireAccepted));
  bench.Advance();
  CHECK(bench.windows.queued);
  CHECK(bench.windows.window.standby_open);
}

TEST_CASE("Étage du champ : une nouvelle partie repart hors file", "[app][battleground]") {
  Bench bench;
  bench.Queue();
  bench.stage.StartSession(bench.windows);
  CHECK(bench.stage.queue().phase() == EntryQueuePhase::kIdle);
  CHECK_FALSE(bench.windows.queued);
  CHECK_FALSE(bench.windows.window.standby_open);
  CHECK(bench.windows.view == nullptr);
  // La table reste : la liste se recompose sans relire l'installation.
  bench.Advance();
  CHECK(bench.windows.view->fields.size() == 2);
}

TEST_CASE("Étage du champ : les trames du combat passent aussi par l'étage",
          "[app][battleground]") {
  Bench bench;
  CHECK(bench.stage.ApplyFrame(FrameOf(ro::proto::Encode(ro::proto::ZcEntryQueueInit{})),
                               bench.world, kStartMs) == FrameOutcome::kApplied);
  bench.Apply(ro::proto::Encode(ro::proto::BattlefieldScoreNotice{.lion = 2, .eagle = 1}));
  CHECK(bench.world.battlefield().score().lion == 2);
  CHECK(bench.world.battlefield().score().eagle == 1);
  const Frame unrelated{.opcode = kUnrelatedOpcode, .bytes = {}};
  CHECK(bench.stage.ApplyFrame(unrelated, bench.world, kStartMs) == FrameOutcome::kUnhandled);
}

TEST_CASE("Étage du champ : le bouton de rapport donne un rapport, une seule fois",
          "[app][battleground]") {
  Bench bench;
  bench.Advance();
  CHECK_FALSE(bench.stage.TakeBugReport().has_value());

  // Le rapport accompagne un geste sans le remplacer : l'inscription part aussi.
  bench.windows.answer = ro::ui::EntryQueueAction{
      .verb = EntryQueueVerb::kApplySolo, .field_rank = 0, .confirmed = true, .report_bug = true};
  bench.Advance();
  static_cast<void>(bench.Next(ro::proto::PacketBgqueueRegister::kWireSize));
  std::optional<moonclient::BugReportContext> report = bench.stage.TakeBugReport();
  REQUIRE(report.has_value());
  CHECK(report->json.find(std::string(R"("server_name":")") + std::string(kServerName)) !=
        std::string::npos);
  CHECK_FALSE(bench.stage.TakeBugReport().has_value());

  // Une nouvelle partie oublie un rapport que personne n'a pris.
  bench.windows.answer = ro::ui::EntryQueueAction{.report_bug = true};
  bench.Advance();
  bench.stage.StartSession(bench.windows);
  CHECK_FALSE(bench.stage.TakeBugReport().has_value());
}
