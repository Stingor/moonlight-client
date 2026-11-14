// « JOUEURS EN LIGNE » DU STAFF : ce qui ouvre la fenêtre, le rythme de ses
// demandes, l'attente qui expire, et ce que la liste dit du client de chacun.

#include "world/staff_roster.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/staff_roster.h"
#include "ro/proto/map_packets.h"
#include "ro/ui/staff_roster_window.h"
#include "ro/vfs/job_names.h"

#include "support/net_frames.h"

using moonclient::IsStaffRosterCommand;
using moonclient::kStaffRosterLeastGapMs;
using moonclient::kStaffRosterRefreshMs;
using moonclient::kStaffRosterWaitMs;
using moonclient::OwnClient;
using moonclient::StaffRoster;
using moonclient::StaffRosterContext;
using moonclient::StaffRosterRowsOf;
using ro::net::FrameOutcome;
using ro::net::StaffRosterClient;
using ro::net::StaffRosterPlayer;
using ro::net::StaffRosterStatus;
using ro::ui::StaffRosterPhase;

namespace {

constexpr std::uint32_t kStartMs = 1000;
constexpr std::int32_t kOwnPatch = 41;
constexpr const char* kOwnCommit = "8cfd8b234";

[[nodiscard]] StaffRosterPlayer Player(std::string name, std::uint32_t account_id,
                                       StaffRosterClient client, std::int32_t patch_index) {
  StaffRosterPlayer player;
  player.name = std::move(name);
  player.account_id = account_id;
  player.client = client;
  player.patch_index = patch_index;
  return player;
}

[[nodiscard]] ro::net::StaffRosterAnswer Served(std::vector<StaffRosterPlayer> players) {
  return ro::net::StaffRosterAnswer{.status = StaffRosterStatus::kServed,
                                    .players = std::move(players)};
}

// Une trame de la réponse à la demande `request_id`, telle que le serveur
// l'écrit.
[[nodiscard]] ro::net::Frame PageFrame(std::uint16_t request_id, std::uint8_t page,
                                       std::uint8_t pages, std::string name) {
  ro::proto::ZcMoonlightStaffRoster packet;
  packet.request_id = request_id;
  packet.status = 0;
  packet.page = page;
  packet.pages = pages;
  packet.entries.emplace_back().name = std::move(name);
  return ro::test_support::FrameOf(ro::proto::Encode(packet));
}

// La fenêtre du staff, image par image.
struct Watching {
  StaffRoster roster;
  ro::ui::StaffRosterWindowState state;
  bool allowed = true;

  void Frame(std::uint32_t now_ms, bool refresh = false) {
    roster.Advance(state, ro::ui::StaffRosterAnswer{.refresh = refresh},
                   StaffRosterContext{.allowed = allowed,
                                      .own = {.patch_index = kOwnPatch, .commit = kOwnCommit}},
                   now_ms);
  }

  // Ouvre la fenêtre, et rend le numéro de sa première demande.
  [[nodiscard]] std::uint16_t Open(std::uint32_t now_ms = kStartMs) {
    state.open = true;
    Frame(now_ms);
    return roster.TakeRequest().value();
  }
};

}  // namespace

TEST_CASE("Joueurs en ligne : `@users` se reconnaît seule, à la casse près",
          "[app][staff_roster]") {
  CHECK(IsStaffRosterCommand("@users"));
  CHECK(IsStaffRosterCommand("@USERS"));
  CHECK(IsStaffRosterCommand("  @users "));
  CHECK_FALSE(IsStaffRosterCommand("@users2"));
  CHECK_FALSE(IsStaffRosterCommand("@users prontera"));
  CHECK_FALSE(IsStaffRosterCommand("#users Alice"));
  CHECK_FALSE(IsStaffRosterCommand("users"));
  CHECK_FALSE(IsStaffRosterCommand(""));
}

TEST_CASE("Joueurs en ligne : sans le droit du staff, la demande n'ouvre rien",
          "[app][staff_roster]") {
  Watching probe;
  probe.allowed = false;
  probe.Frame(kStartMs);
  CHECK_FALSE(probe.roster.Ask());
  probe.Frame(kStartMs + 1);
  CHECK_FALSE(probe.state.open);
  CHECK_FALSE(probe.roster.TakeRequest().has_value());
}

TEST_CASE("Joueurs en ligne : la demande du staff ouvre la fenêtre et part une fois",
          "[app][staff_roster]") {
  Watching probe;
  probe.Frame(kStartMs);
  CHECK_FALSE(probe.state.open);
  REQUIRE(probe.roster.Ask());
  probe.Frame(kStartMs + 1);
  CHECK(probe.state.open);
  CHECK(probe.state.raise);
  CHECK(probe.roster.view().phase == StaffRosterPhase::kAsking);
  CHECK(probe.roster.TakeRequest().has_value());
  // La même demande ne repart pas à l'image suivante.
  probe.Frame(kStartMs + 2);
  CHECK_FALSE(probe.roster.TakeRequest().has_value());
}

TEST_CASE("Joueurs en ligne : la réponse montre la liste, et se redemande à la cadence",
          "[app][staff_roster]") {
  Watching probe;
  static_cast<void>(probe.Open());
  constexpr std::uint32_t kAnsweredMs = kStartMs + 200;
  probe.roster.Note(Served({Player("Aldric", 2000001, StaffRosterClient::kMoonclient, kOwnPatch)}),
                    kAnsweredMs);
  probe.Frame(kAnsweredMs);
  CHECK(probe.roster.view().phase == StaffRosterPhase::kReady);
  REQUIRE(probe.roster.view().rows.size() == 1);
  CHECK(probe.roster.view().rows.front().name == "Aldric");
  CHECK(probe.roster.view().refresh_period_s == kStaffRosterRefreshMs / 1000);

  probe.Frame(kAnsweredMs + kStaffRosterRefreshMs - 1);
  CHECK_FALSE(probe.roster.TakeRequest().has_value());
  CHECK(probe.roster.view().age_s == (kStaffRosterRefreshMs - 1) / 1000);
  probe.Frame(kAnsweredMs + kStaffRosterRefreshMs);
  CHECK(probe.roster.TakeRequest().has_value());
  // La liste précédente reste montrée pendant que la suivante est en route.
  CHECK(probe.roster.view().phase == StaffRosterPhase::kReady);
  CHECK(probe.roster.view().refreshing);
  CHECK(probe.roster.view().rows.size() == 1);
}

TEST_CASE("Joueurs en ligne : la case décochée, seule la main redemande", "[app][staff_roster]") {
  Watching probe;
  static_cast<void>(probe.Open());
  probe.roster.Note(Served({}), kStartMs + 200);
  probe.state.auto_refresh = false;
  probe.Frame(kStartMs + 200 + (2 * kStaffRosterRefreshMs));
  CHECK_FALSE(probe.roster.TakeRequest().has_value());
  probe.Frame(kStartMs + 201 + (2 * kStaffRosterRefreshMs), /*refresh=*/true);
  CHECK(probe.roster.TakeRequest().has_value());
}

TEST_CASE("Joueurs en ligne : deux demandes gardent leur écart", "[app][staff_roster]") {
  Watching probe;
  static_cast<void>(probe.Open());
  probe.roster.Note(Served({}), kStartMs + 100);
  // Trop tôt : le bouton est grisé, et un clic forgé ne part pas.
  probe.Frame(kStartMs + kStaffRosterLeastGapMs - 1, /*refresh=*/true);
  CHECK_FALSE(probe.roster.view().can_refresh);
  CHECK_FALSE(probe.roster.TakeRequest().has_value());
  probe.Frame(kStartMs + kStaffRosterLeastGapMs);
  CHECK(probe.roster.view().can_refresh);
  probe.Frame(kStartMs + kStaffRosterLeastGapMs + 1, /*refresh=*/true);
  CHECK(probe.roster.TakeRequest().has_value());
}

TEST_CASE("Joueurs en ligne : une attente a une fin, et se relance à la main",
          "[app][staff_roster]") {
  Watching probe;
  static_cast<void>(probe.Open());
  probe.Frame(kStartMs + kStaffRosterWaitMs - 1);
  CHECK(probe.roster.view().phase == StaffRosterPhase::kAsking);
  probe.Frame(kStartMs + kStaffRosterWaitMs);
  CHECK(probe.roster.view().phase == StaffRosterPhase::kSilent);
  // Le silence ne se redemande pas tout seul.
  probe.Frame(kStartMs + kStaffRosterWaitMs + kStaffRosterRefreshMs);
  CHECK_FALSE(probe.roster.TakeRequest().has_value());
  probe.Frame(kStartMs + kStaffRosterWaitMs + kStaffRosterRefreshMs + 1, /*refresh=*/true);
  CHECK(probe.roster.view().phase == StaffRosterPhase::kAsking);
  CHECK(probe.roster.TakeRequest().has_value());
}

TEST_CASE("Joueurs en ligne : un refus et un statut inconnu se disent, sans liste",
          "[app][staff_roster]") {
  Watching probe;
  static_cast<void>(probe.Open());
  probe.roster.Note(ro::net::StaffRosterAnswer{.status = StaffRosterStatus::kDenied},
                    kStartMs + 100);
  CHECK(probe.roster.view().phase == StaffRosterPhase::kDenied);
  CHECK(probe.roster.view().rows.empty());
  // Un refus ne se redemande pas à la cadence.
  probe.Frame(kStartMs + 100 + kStaffRosterRefreshMs);
  CHECK_FALSE(probe.roster.TakeRequest().has_value());
  probe.roster.Note(ro::net::StaffRosterAnswer{.status = StaffRosterStatus::kUnknown},
                    kStartMs + 200);
  CHECK(probe.roster.view().phase == StaffRosterPhase::kUnreadable);
}

TEST_CASE("Joueurs en ligne : fermée ou sans le droit, la pièce oublie tout",
          "[app][staff_roster]") {
  Watching probe;
  static_cast<void>(probe.Open());
  probe.roster.Note(Served({Player("Aldric", 2000001, StaffRosterClient::kMoonclient, kOwnPatch)}),
                    kStartMs + 100);
  REQUIRE(probe.roster.Find(2000001) != nullptr);
  CHECK(probe.roster.Find(2000002) == nullptr);

  probe.state.open = false;
  probe.Frame(kStartMs + 200);
  CHECK(probe.roster.view().rows.empty());
  CHECK(probe.roster.Find(2000001) == nullptr);

  // Le droit perdu ne referme pas la fenêtre : il peut revenir.
  probe.state.open = true;
  probe.allowed = false;
  probe.Frame(kStartMs + kStaffRosterRefreshMs);
  CHECK(probe.state.open);
  CHECK_FALSE(probe.roster.TakeRequest().has_value());
  CHECK(probe.roster.view().rows.empty());
  // Revenu, la fenêtre restée ouverte redemande.
  probe.allowed = true;
  probe.Frame(kStartMs + kStaffRosterRefreshMs + 1);
  CHECK(probe.roster.TakeRequest().has_value());
}

TEST_CASE("Joueurs en ligne : les trames s'assemblent, celles d'une autre demande sont jetées",
          "[app][staff_roster]") {
  Watching probe;
  const std::uint16_t request_id = probe.Open();
  const auto older = static_cast<std::uint16_t>(request_id - 1);
  CHECK(probe.roster.ApplyFrame(PageFrame(older, 0, 1, "Intrus"), kStartMs + 10) ==
        FrameOutcome::kIgnored);
  CHECK(probe.roster.ApplyFrame(PageFrame(request_id, 0, 2, "Aldric"), kStartMs + 20) ==
        FrameOutcome::kIgnored);
  CHECK(probe.roster.view().phase == StaffRosterPhase::kAsking);
  CHECK(probe.roster.ApplyFrame(PageFrame(request_id, 1, 2, "Mirelle"), kStartMs + 30) ==
        FrameOutcome::kApplied);
  CHECK(probe.roster.view().phase == StaffRosterPhase::kReady);
  REQUIRE(probe.roster.view().rows.size() == 2);
  CHECK(probe.roster.view().rows[1].name == "Mirelle");
}

TEST_CASE("Joueurs en ligne : une trame abîmée ou étrangère se dit pour ce qu'elle est",
          "[app][staff_roster]") {
  Watching probe;
  static_cast<void>(probe.Open());
  ro::net::Frame torn = PageFrame(1, 0, 1, "Aldric");
  torn.bytes.pop_back();
  CHECK(probe.roster.ApplyFrame(torn, kStartMs) == FrameOutcome::kUndecodable);
  CHECK(probe.roster.ApplyFrame(ro::net::Frame{.opcode = 0x0F3F, .bytes = {}}, kStartMs) ==
        FrameOutcome::kUnhandled);
}

TEST_CASE("Joueurs en ligne : un client est en retard sous le patch le plus récent du sien",
          "[app][staff_roster]") {
  const std::vector<StaffRosterPlayer> players = {
      Player("A jour", 1, StaffRosterClient::kMoonclient, kOwnPatch),
      Player("Ancien", 2, StaffRosterClient::kMoonclient, kOwnPatch - 1),
      Player("Origine neuve", 3, StaffRosterClient::kOriginal, 118),
      Player("Origine ancienne", 4, StaffRosterClient::kOriginal, 117),
      Player("Sans rien", 5, StaffRosterClient::kNone, -1)};
  const std::vector<ro::ui::StaffRosterRow> rows =
      StaffRosterRowsOf(players, nullptr, OwnClient{.patch_index = kOwnPatch});
  REQUIRE(rows.size() == players.size());
  CHECK_FALSE(rows[0].client_outdated);
  CHECK(rows[1].client_outdated);
  // Chaque client compte sur sa propre chaîne : 118 n'accuse pas 41.
  CHECK_FALSE(rows[2].client_outdated);
  CHECK(rows[3].client_outdated);
  CHECK_FALSE(rows[4].client_outdated);
}

TEST_CASE("Joueurs en ligne : notre propre patch compte, même seul à le porter",
          "[app][staff_roster]") {
  const std::vector<StaffRosterPlayer> players = {
      Player("Ancien", 2, StaffRosterClient::kMoonclient, kOwnPatch - 1)};
  CHECK(StaffRosterRowsOf(players, nullptr, OwnClient{.patch_index = kOwnPatch})
            .front()
            .client_outdated);
  CHECK_FALSE(StaffRosterRowsOf(players, nullptr, OwnClient{.patch_index = kOwnPatch - 1})
                  .front()
                  .client_outdated);
}

TEST_CASE("Joueurs en ligne : une autre révision ne se dit qu'à patch égal au nôtre",
          "[app][staff_roster]") {
  StaffRosterPlayer same = Player("Pareil", 1, StaffRosterClient::kMoonclient, kOwnPatch);
  same.commit = kOwnCommit;
  StaffRosterPlayer other = Player("Autre", 2, StaffRosterClient::kMoonclient, kOwnPatch);
  other.commit = "906828f0f";
  StaffRosterPlayer silent = Player("Muet", 3, StaffRosterClient::kMoonclient, kOwnPatch);
  StaffRosterPlayer late = Player("Ancien", 4, StaffRosterClient::kMoonclient, kOwnPatch - 1);
  late.commit = "0f83233db";
  const std::vector<StaffRosterPlayer> players = {same, other, silent, late};
  const std::vector<ro::ui::StaffRosterRow> rows = StaffRosterRowsOf(
      players, nullptr, OwnClient{.patch_index = kOwnPatch, .commit = kOwnCommit});
  CHECK_FALSE(rows[0].other_revision);
  CHECK(rows[1].other_revision);
  // Une révision non déclarée ne se compare pas, et un retard suffit à lui seul.
  CHECK_FALSE(rows[2].other_revision);
  CHECK_FALSE(rows[3].other_revision);
  CHECK(rows[3].client_outdated);
}

TEST_CASE("Joueurs en ligne : le job se nomme par sa table, le staff par son niveau",
          "[app][staff_roster]") {
  ro::vfs::JobNames names;
  names.by_id[4008] = "Lord Knight";
  StaffRosterPlayer knight = Player("Aldric", 1, StaffRosterClient::kMoonclient, kOwnPatch);
  knight.job = 4008;
  knight.group_level = 99;
  StaffRosterPlayer unknown = Player("Corvin", 2, StaffRosterClient::kMoonclient, kOwnPatch);
  unknown.job = 4999;
  unknown.group_level = 79;
  const std::vector<StaffRosterPlayer> players = {knight, unknown};
  const std::vector<ro::ui::StaffRosterRow> rows =
      StaffRosterRowsOf(players, &names, OwnClient{.patch_index = kOwnPatch});
  CHECK(rows[0].job_name == "Lord Knight");
  CHECK(rows[0].staff);
  CHECK(rows[1].job_name.empty());
  CHECK_FALSE(rows[1].staff);
}
