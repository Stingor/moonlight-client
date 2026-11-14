// LE TABLEAU DES CLASSEMENTS côté jeu : un classement reçu ne l'ouvre pas ;
// `/ranking` et l'icône l'ouvrent, un onglet sans classement le demande, la vue
// attend ses noms comme la conversation, et la réponse à une demande du tableau
// ne s'écrit pas dans la conversation.

#include "world/ranking_board.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/rank_notices.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"
#include "ro/ui/ranking_window.h"

#include "chat/rank_command.h"
#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "world/rank_notices.h"

using moonclient::RankingBoard;
using moonclient::RankingBoardWindows;
using moonclient::RankNotices;
using ro::net::RankKind;
using ro::test::Connect;
using ro::ui::RankingTab;

namespace {

constexpr std::uint32_t kNowMs = 10'000;
constexpr std::uint32_t kOneSecondMs = 1000;
constexpr std::uint32_t kAnaId = 77;
constexpr std::uint32_t kBobId = 78;
constexpr std::uint32_t kAnaPoints = 120;
constexpr std::uint32_t kOwnPoints = 7;
// CZ_REQ_RANKING : l'opcode et le genre.
constexpr std::size_t kRankingRequestBytes = 4;
constexpr std::size_t kNameRequestBytes = 6;
constexpr int kSilenceMs = 150;

ro::net::Frame RankingFrame(RankKind kind, std::array<std::uint32_t, ro::net::kRankingSize> ids) {
  ro::proto::ZcAckRanking2 packet;
  packet.type = static_cast<std::uint16_t>(kind);
  packet.CIDs = ids;
  packet.points[0] = kAnaPoints;
  packet.mypoints = kOwnPoints;
  return ro::net::Frame{.opcode = ro::proto::ZcAckRanking2::kOpcode,
                        .bytes = ro::proto::Encode(packet)};
}

ro::net::Frame NameFrame(std::uint32_t character_id, std::string name) {
  ro::proto::ZcAckReqnameBygid packet;
  packet.CID = character_id;
  packet.name = std::move(name);
  return ro::net::Frame{.opcode = ro::proto::ZcAckReqnameBygid::kOpcode,
                        .bytes = ro::proto::Encode(packet)};
}

bool ChatSays(const ro::sim::World& world, std::string_view part) {
  for (const ro::sim::ChatLine& line : world.chat_lines()) {
    if (line.text.find(part) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("un classement reçu n'ouvre pas le tableau, qui le montre ouvert",
          "[app][rank][ranking]") {
  ro::sim::World world;
  ro::net::Connection link;
  RankNotices notices;
  RankingBoard board;
  RankingBoardWindows windows;

  std::array<std::uint32_t, ro::net::kRankingSize> ids = {};
  ids[0] = kAnaId;
  ids[1] = kBobId;
  static_cast<void>(
      notices.ApplyFrame(RankingFrame(RankKind::kAlchemist, ids), world, link, kNowMs));
  board.Advance(windows, notices, link, kNowMs);
  // 🔴 LA COMMANDE ÉCRIT DANS LA CONVERSATION, ET C'EST TOUT : le tableau reste
  // fermé.
  CHECK_FALSE(windows.window.open);

  // Ouvert sur cet onglet, il montre le classement reçu, qui attend ses noms.
  windows.window.tab = RankingTab::kAlchemist;
  windows.open_asked = true;
  board.Advance(windows, notices, link, kNowMs);
  REQUIRE(windows.view != nullptr);
  CHECK(windows.window.open);
  CHECK_FALSE(windows.open_asked);
  CHECK(windows.view->tab == RankingTab::kAlchemist);
  CHECK_FALSE(windows.view->awaiting_list);
  CHECK(windows.view->waiting_names);
  REQUIRE(windows.view->rows.size() == ro::net::kRankingSize);
  CHECK(windows.view->rows[0].points == kAnaPoints);
  CHECK(windows.view->own_points == kOwnPoints);

  // Un nom sur deux : la vue attend toujours.
  static_cast<void>(notices.ApplyFrame(NameFrame(kAnaId, "Ana"), world, link, kNowMs));
  notices.Advance(world, kNowMs + kOneSecondMs);
  board.Advance(windows, notices, link, kNowMs + kOneSecondMs);
  CHECK(windows.view->waiting_names);

  // Le second arrive : les lignes se montrent, nommées.
  static_cast<void>(notices.ApplyFrame(NameFrame(kBobId, "Bob"), world, link, kNowMs));
  notices.Advance(world, kNowMs + 2 * kOneSecondMs);
  board.Advance(windows, notices, link, kNowMs + 2 * kOneSecondMs);
  CHECK_FALSE(windows.view->waiting_names);
  CHECK(windows.view->rows[0].name == "Ana");
  CHECK(windows.view->rows[1].name == "Bob");
  // Une place vide reste sans nom : la fenêtre la dit comme la conversation.
  CHECK(windows.view->rows[2].name.empty());
  CHECK(ChatSays(world, "ALCHEMIST RANK"));
}

TEST_CASE("/ranking ouvre le tableau, et demande un classement jamais reçu",
          "[app][rank][ranking]") {
  CHECK(moonclient::IsRankingBoardCommand("/ranking"));
  CHECK(moonclient::IsRankingBoardCommand("/Ranking"));
  CHECK_FALSE(moonclient::IsRankingBoardCommand("/ranking 2"));
  CHECK_FALSE(moonclient::ReadRankCommand("/ranking").has_value());

  ro::test::LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  ro::sim::World world;
  RankNotices notices;
  RankingBoard board;
  RankingBoardWindows windows;

  // Jamais ouvert : l'onglet des forgerons, dont le classement se demande.
  windows.open_asked = true;
  board.Advance(windows, notices, link, kNowMs);
  CHECK(windows.window.open);
  REQUIRE(windows.view != nullptr);
  CHECK(windows.view->tab == RankingTab::kBlacksmith);
  CHECK(windows.view->awaiting_list);
  const std::vector<std::uint8_t> sent = server.Receive(kRankingRequestBytes);
  REQUIRE(sent.size() == kRankingRequestBytes);
  CHECK(sent[2] == static_cast<std::uint8_t>(RankKind::kBlacksmith));

  // La réponse se montre, et ne s'écrit pas dans la conversation.
  const std::array<std::uint32_t, ro::net::kRankingSize> nobody = {};
  static_cast<void>(
      notices.ApplyFrame(RankingFrame(RankKind::kBlacksmith, nobody), world, link, kNowMs));
  board.Advance(windows, notices, link, kNowMs);
  CHECK_FALSE(windows.view->awaiting_list);
  CHECK(windows.view->own_points == kOwnPoints);
  CHECK_FALSE(ChatSays(world, "BLACKSMITH RANK"));

  // Refermé puis rouvert : le classement est là, rien ne se redemande.
  windows.window.open = false;
  windows.open_asked = true;
  board.Advance(windows, notices, link, kNowMs);
  CHECK(windows.window.open);
  CHECK_FALSE(windows.view->awaiting_list);
  CHECK(server.Receive(kRankingRequestBytes, kSilenceMs).empty());
}

TEST_CASE("l'icône du menu ouvre le tableau, et le referme ouvert", "[app][rank][ranking]") {
  RankingBoardWindows windows;
  moonclient::ToggleRankingBoard(windows);
  CHECK(windows.open_asked);
  CHECK_FALSE(windows.window.open);
  windows.open_asked = false;
  windows.window.open = true;
  moonclient::ToggleRankingBoard(windows);
  CHECK_FALSE(windows.window.open);
  CHECK_FALSE(windows.open_asked);
}

TEST_CASE("l'onglet changé demande son classement, que la conversation tait",
          "[app][rank][ranking]") {
  ro::test::LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  ro::sim::World world;
  RankNotices notices;
  RankingBoard board;
  RankingBoardWindows windows;

  windows.window.open = true;
  windows.window.tab = RankingTab::kTaekwon;
  windows.answer =
      ro::ui::RankingWindowAction{.requested = true, .requested_tab = RankingTab::kTaekwon};
  board.Advance(windows, notices, link, kNowMs);
  const std::vector<std::uint8_t> sent = server.Receive(kRankingRequestBytes);
  REQUIRE(sent.size() == kRankingRequestBytes);
  CHECK(sent[2] == static_cast<std::uint8_t>(RankKind::kTaekwon));
  // La demande n'est envoyée qu'une fois.
  board.Advance(windows, notices, link, kNowMs);
  CHECK(server.Receive(kRankingRequestBytes, kSilenceMs).empty());

  // La réponse, sans personne à nommer, se montre et ne s'écrit pas.
  const std::array<std::uint32_t, ro::net::kRankingSize> nobody = {};
  static_cast<void>(
      notices.ApplyFrame(RankingFrame(RankKind::kTaekwon, nobody), world, link, kNowMs));
  board.Advance(windows, notices, link, kNowMs);
  REQUIRE(windows.view != nullptr);
  CHECK(windows.view->tab == RankingTab::kTaekwon);
  CHECK_FALSE(windows.view->waiting_names);
  CHECK_FALSE(ChatSays(world, "TAEKWON RANK"));

  // LA RÉPONSE SUIVANTE, celle d'une commande, s'écrit de nouveau.
  static_cast<void>(
      notices.ApplyFrame(RankingFrame(RankKind::kTaekwon, nobody), world, link, kNowMs));
  CHECK(ChatSays(world, "TAEKWON RANK"));
  CHECK(server.Receive(kNameRequestBytes, kSilenceMs).empty());
}

TEST_CASE("le classement des tueurs n'a pas d'onglet, il s'écrit seulement",
          "[app][rank][ranking]") {
  CHECK_FALSE(moonclient::RankingTabOf(RankKind::kKiller).has_value());

  ro::net::Connection link;
  ro::sim::World world;
  RankNotices notices;
  const std::array<std::uint32_t, ro::net::kRankingSize> nobody = {};
  static_cast<void>(
      notices.ApplyFrame(RankingFrame(RankKind::kKiller, nobody), world, link, kNowMs));
  CHECK(ChatSays(world, "KILLER RANK"));
}
