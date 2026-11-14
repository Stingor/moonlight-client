// LES ONGLETS DE LA CONVERSATION VUS DE LA BOUCLE : l'heure que portent les lignes,
// et l'onglet que chaque fenêtre garde sous les yeux quand une languette bouge.

#include "chat/chat_tab_view.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/chat_line.h"
#include "ro/ui/chat_window.h"

#include "chat/whisper_desk.h"
#include "support/temp_directory.h"

namespace {

using moonclient::ChatTabsView;
using ro::sim::ChatKind;
using ro::sim::ChatLine;
using ro::ui::ChatWindowAction;

constexpr std::uint32_t kMsPerHour = 3'600'000;
// Plus longtemps qu'une journée : le repère du jour est alors négatif, quelle que
// soit l'heure à laquelle l'essai tourne.
constexpr std::uint32_t kTwoDaysMs = 48 * kMsPerHour;
constexpr std::uint32_t kMainGroup = 0;
constexpr std::string_view kPeer = "Alice";

[[nodiscard]] ChatLine Said(std::uint64_t seq, std::uint32_t said_ms) {
  ChatLine line;
  line.seq = seq;
  line.kind = ChatKind::kPublic;
  line.type = ro::sim::ChatType::kPublicChat;
  line.text = "bonjour";
  line.said_ms = said_ms;
  return line;
}

// Une vue sur un dossier à elle, et les fenêtres qu'elle tient à jour.
struct ViewBench {
  ro::test_support::TempDirectory root;
  ChatTabsView view{root.path()};
  ro::ui::ChatWindows windows;
  std::vector<ChatLine> chat;

  void Refresh(std::uint32_t now_ms = 0) {
    view.Refresh(windows, chat, now_ms, ro::ui::ChatAppearance{}, false);
  }
  // La bande principale nommée A, B, C, D.
  void FourTabs() {
    Refresh();
    for (int added = 0; added < 3; ++added) {
      Apply(ChatWindowAction{.add_tab = true});
      Refresh();
    }
    constexpr std::array<const char*, 4> kNames = {"A", "B", "C", "D"};
    for (int rank = 0; rank < static_cast<int>(kNames.size()); ++rank) {
      ChatWindowAction rename;
      rename.renamed_tab = rank;
      rename.new_name = kNames[static_cast<std::size_t>(rank)];
      Apply(rename);
    }
    Refresh();
  }
  void Apply(const ChatWindowAction& asked, std::uint32_t group = kMainGroup) {
    view.Apply(group, asked, windows);
  }
  // Le nom de l'onglet que cette fenêtre montre.
  [[nodiscard]] std::string Shown(std::uint32_t group = kMainGroup) {
    const std::span<const ro::ui::ChatStripTab> tabs = view.tabs(group);
    REQUIRE(windows.For(group).tab >= 0);
    REQUIRE(static_cast<std::size_t>(windows.For(group).tab) < tabs.size());
    return std::string(tabs[static_cast<std::size_t>(windows.For(group).tab)].name);
  }
  // La première fenêtre flottante.
  [[nodiscard]] std::uint32_t Floating() const {
    REQUIRE(view.groups().size() > 1);
    return view.groups()[1];
  }
};

void CheckSameClock(const ro::ui::ChatLineView& shown, const ro::ui::ChatLineView& expected) {
  CHECK(shown.hour == expected.hour);
  CHECK(shown.minute == expected.minute);
  CHECK(shown.second == expected.second);
}

}  // namespace

TEST_CASE("Heure des lignes : un repère négatif vaut partout, et ne se relève qu'une fois",
          "[app][onglets]") {
  ViewBench bench;
  bench.chat.push_back(Said(1, kTwoDaysMs));
  bench.Refresh(kTwoDaysMs);
  REQUIRE(bench.view.lines(kMainGroup).size() == 1);
  const ro::ui::ChatLineView in_log = bench.view.lines(kMainGroup)[0];

  SECTION("le salon montre la même heure que le fil") {
    const std::vector<ChatLine> room = {Said(1, kTwoDaysMs)};
    bench.view.RefreshRoom(room, 1);
    REQUIRE(bench.view.room_lines().size() == 1);
    CheckSameClock(bench.view.room_lines()[0], in_log);
  }
  SECTION("une conversation aussi") {
    ChatLine whispered = Said(2, kTwoDaysMs);
    whispered.kind = ChatKind::kWhisper;
    whispered.whisper.side = ro::sim::WhisperSide::kReceived;
    whispered.whisper.peer_name = kPeer;
    const std::vector<ChatLine> chat = {std::move(whispered)};
    const std::vector<moonclient::Conversation> conversations = {
        moonclient::Conversation{.peer = std::string(kPeer)}};
    bench.view.RefreshWhispers(chat, conversations);
    REQUIRE(bench.view.whisper_lines(kPeer).size() == 1);
    CheckSameClock(bench.view.whisper_lines(kPeer)[0], in_log);
  }
  SECTION("le repère ne suit pas l'horloge de la boucle") {
    // 🔴 RELEVÉ À NOUVEAU, il reculerait l'heure de la même ligne d'autant.
    bench.Refresh(kTwoDaysMs + kMsPerHour);
    REQUIRE(bench.view.lines(kMainGroup).size() == 1);
    CheckSameClock(bench.view.lines(kMainGroup)[0], in_log);
  }
}

TEST_CASE("Onglet montré : retirer une languette à sa gauche ne le change pas", "[app][onglets]") {
  ViewBench bench;
  bench.FourTabs();
  bench.windows.main.tab = 2;
  REQUIRE(bench.Shown() == "C");

  ChatWindowAction asked;
  asked.remove_tab = 0;
  bench.Apply(asked);
  bench.Refresh();
  CHECK(bench.Shown() == "C");
}

TEST_CASE("Onglet montré : l'onglet d'à côté n'est pas marqué lu à sa place", "[app][onglets]") {
  ViewBench bench;
  bench.chat.push_back(Said(1, 0));
  bench.FourTabs();
  bench.windows.main.tab = 2;
  bench.chat.push_back(Said(2, 0));
  bench.Refresh();
  REQUIRE(bench.view.tabs(kMainGroup)[3].unread);

  ChatWindowAction asked;
  asked.remove_tab = 0;
  bench.Apply(asked);
  bench.Refresh();
  // A parti, D est au rang 2 — celui que la fenêtre montrait.
  CHECK(bench.view.tabs(kMainGroup)[2].name == "D");
  CHECK(bench.view.tabs(kMainGroup)[2].unread);
}

TEST_CASE("Onglet montré : retirer une languette à sa droite, ou lui-même", "[app][onglets]") {
  ViewBench bench;
  bench.FourTabs();
  bench.windows.main.tab = 1;
  ChatWindowAction asked;
  SECTION("à sa droite, rien ne glisse") {
    asked.remove_tab = 3;
    bench.Apply(asked);
    bench.Refresh();
    CHECK(bench.Shown() == "B");
  }
  SECTION("lui-même, sa voisine prend sa place") {
    asked.remove_tab = 1;
    bench.Apply(asked);
    bench.Refresh();
    CHECK(bench.Shown() == "C");
  }
}

TEST_CASE("Onglet montré : le dernier fil refuse de partir, et rien ne glisse", "[app][onglets]") {
  ViewBench bench;
  bench.Refresh();
  REQUIRE(bench.view.count() == 1);
  ChatWindowAction asked;
  asked.remove_tab = 0;
  bench.Apply(asked);
  CHECK(bench.view.count() == 1);
  CHECK(bench.windows.main.tab == 0);
}

TEST_CASE("Onglet montré : sortir une languette à sa gauche ne le change pas", "[app][onglets]") {
  ViewBench bench;
  bench.FourTabs();
  bench.windows.main.tab = 2;
  ChatWindowAction asked;
  asked.detach_tab = 1;
  bench.Apply(asked);
  bench.Refresh();
  CHECK(bench.Shown() == "C");
}

TEST_CASE("Onglet montré : rattacher une languette de la flottante à sa gauche", "[app][onglets]") {
  ViewBench bench;
  bench.FourTabs();
  // Une flottante de deux onglets : B, puis C rangé dans sa bande.
  ChatWindowAction out;
  out.detach_tab = 1;
  bench.Apply(out);
  bench.Refresh();
  const std::uint32_t floating = bench.Floating();
  ro::ui::ChatDrop into{.asked = true, .from_group = kMainGroup, .from_tab = 1};
  into.to_group = floating;
  into.to_rank = -1;
  bench.view.ApplyDrop(into, bench.windows);
  bench.Refresh();
  REQUIRE(bench.view.tabs(floating).size() == 2);
  bench.windows.For(floating).tab = 1;
  const std::string shown = bench.Shown(floating);

  ChatWindowAction back;
  back.attach_tab = 0;
  bench.Apply(back, floating);
  bench.Refresh();
  CHECK(bench.Shown(floating) == shown);
}

TEST_CASE("Onglet montré : un glisser garde ce que chaque fenêtre montrait", "[app][onglets]") {
  ViewBench bench;
  bench.FourTabs();
  bench.windows.main.tab = 2;
  SECTION("hors de toute bande, la languette fonde sa fenêtre") {
    const ro::ui::ChatDrop drop{
        .asked = true, .from_group = kMainGroup, .from_tab = 0, .to_new_window = true};
    bench.view.ApplyDrop(drop, bench.windows);
    bench.Refresh();
    CHECK(bench.Shown() == "C");
  }
  SECTION("dans une autre bande, les deux fenêtres gardent le leur") {
    ChatWindowAction out;
    out.detach_tab = 3;
    bench.Apply(out);
    bench.Refresh();
    const std::uint32_t floating = bench.Floating();
    REQUIRE(bench.Shown(floating) == "D");

    // A part devant D.
    ro::ui::ChatDrop drop{.asked = true, .from_group = kMainGroup, .from_tab = 0};
    drop.to_group = floating;
    drop.to_rank = 0;
    bench.view.ApplyDrop(drop, bench.windows);
    bench.Refresh();
    CHECK(bench.Shown() == "C");
    CHECK(bench.Shown(floating) == "D");
  }
}

TEST_CASE("Onglet montré : une languette rattachée ne déplace pas la principale",
          "[app][onglets]") {
  ViewBench bench;
  bench.FourTabs();
  ChatWindowAction out;
  out.detach_tab = 1;
  bench.Apply(out);
  bench.Refresh();
  const std::uint32_t floating = bench.Floating();
  ChatWindowAction back;
  back.attach_tab = 0;

  SECTION("à droite du rang d'arrivée, il glisse avec sa bande") {
    bench.windows.main.tab = 2;
    REQUIRE(bench.Shown() == "D");
    bench.Apply(back, floating);
    bench.Refresh();
    CHECK(bench.Shown() == "D");
  }
  SECTION("à gauche du rang d'arrivée, il ne bouge pas") {
    bench.windows.main.tab = 0;
    bench.Apply(back, floating);
    bench.Refresh();
    CHECK(bench.Shown() == "A");
  }
}

TEST_CASE("Onglet montré : le non-lu de la languette rattachée survit", "[app][onglets]") {
  ViewBench bench;
  bench.chat.push_back(Said(1, 0));
  bench.FourTabs();
  // Une flottante B, C qui montre B ; la principale A, D qui montre D.
  ChatWindowAction out;
  out.detach_tab = 1;
  bench.Apply(out);
  bench.Refresh();
  const std::uint32_t floating = bench.Floating();
  ro::ui::ChatDrop into{.asked = true, .from_group = kMainGroup, .from_tab = 1};
  into.to_group = floating;
  into.to_rank = -1;
  bench.view.ApplyDrop(into, bench.windows);
  bench.Refresh();
  bench.windows.main.tab = 1;
  bench.windows.For(floating).tab = 0;
  REQUIRE(bench.Shown() == "D");
  REQUIRE(bench.Shown(floating) == "B");
  bench.chat.push_back(Said(2, 0));
  bench.Refresh();

  // C rentre au rang 1, celui que la principale montrait : il n'y a pas été lu.
  ChatWindowAction back;
  back.attach_tab = 1;
  bench.Apply(back, floating);
  bench.Refresh();
  CHECK(bench.Shown() == "D");
  REQUIRE(bench.view.tabs(kMainGroup)[1].name == "C");
  CHECK(bench.view.tabs(kMainGroup)[1].unread);
}

TEST_CASE("Onglet montré : un compteur caché ne compte pas au rang d'arrivée", "[app][onglets]") {
  ViewBench bench;
  bench.FourTabs();
  // A, compteur, B, C, D, le compteur caché : la bande lit A, B, C, D.
  bench.view.DockMeter(kMainGroup, 1, bench.windows);
  bench.view.HideMeter(false, bench.windows);
  bench.view.HideMeter(true, bench.windows);
  bench.Refresh();
  ChatWindowAction out;
  out.detach_tab = 1;
  bench.Apply(out);
  bench.Refresh();
  const std::uint32_t floating = bench.Floating();
  bench.windows.main.tab = 1;
  REQUIRE(bench.Shown() == "C");

  ChatWindowAction back;
  back.attach_tab = 0;
  bench.Apply(back, floating);
  bench.Refresh();
  CHECK(bench.Shown() == "C");
}

TEST_CASE("Onglet montré : le compteur rangé dans la bande ne le change pas", "[app][onglets]") {
  ViewBench bench;
  bench.FourTabs();
  bench.windows.main.tab = 2;
  SECTION("à sa gauche") {
    bench.view.DockMeter(kMainGroup, 1, bench.windows);
    bench.Refresh();
    CHECK(bench.windows.main.tab == 3);
    CHECK(bench.Shown() == "C");
  }
  SECTION("au bout, rien ne glisse") {
    bench.view.DockMeter(kMainGroup, -1, bench.windows);
    bench.Refresh();
    CHECK(bench.windows.main.tab == 2);
    CHECK(bench.Shown() == "C");
  }
  SECTION("montré, déplacé dans sa bande, il reste montré") {
    bench.view.DockMeter(kMainGroup, 3, bench.windows);
    bench.Refresh();
    bench.windows.main.tab = 3;
    bench.view.LabelMeter("DPS");
    bench.Refresh();
    REQUIRE(bench.Shown() == "DPS");
    bench.view.DockMeter(kMainGroup, 0, bench.windows);
    bench.Refresh();
    CHECK(bench.Shown() == "DPS");
  }
}

TEST_CASE("Onglet montré : le compteur sorti de sa bande ne le change pas", "[app][onglets]") {
  ViewBench bench;
  bench.FourTabs();
  bench.view.DockMeter(kMainGroup, 0, bench.windows);
  bench.Refresh();
  bench.windows.main.tab = 3;
  REQUIRE(bench.Shown() == "C");
  bench.view.UndockMeter(bench.windows);
  bench.Refresh();
  CHECK(bench.Shown() == "C");
}

TEST_CASE("Onglet montré : le profil range et sort le compteur sans le changer", "[app][onglets]") {
  ViewBench bench;
  bench.FourTabs();
  bench.view.DockMeter(kMainGroup, 0, bench.windows);
  bool wanted = true;
  bench.view.FollowMeterDock(wanted, bench.windows);
  bench.Refresh();
  bench.windows.main.tab = 3;
  REQUIRE(bench.Shown() == "C");

  wanted = false;
  bench.view.FollowMeterDock(wanted, bench.windows);
  bench.Refresh();
  CHECK_FALSE(bench.view.meter_docked());
  CHECK(bench.Shown() == "C");

  wanted = true;
  bench.view.FollowMeterDock(wanted, bench.windows);
  bench.Refresh();
  REQUIRE(bench.view.meter_docked());
  CHECK(bench.windows.main.tab == 3);
  CHECK(bench.Shown() == "C");
}

TEST_CASE("Onglet montré : éteindre et rallumer le compteur ne le change pas", "[app][onglets]") {
  ViewBench bench;
  bench.FourTabs();
  bench.view.DockMeter(kMainGroup, 0, bench.windows);
  bench.Refresh();
  bench.windows.main.tab = 3;

  SECTION("une bascule recale, une fois") {
    bench.view.HideMeter(false, bench.windows);
    REQUIRE(bench.Shown() == "C");
    // ⚠ POSÉ À CHAQUE IMAGE : seul le changement compte.
    for (int frame = 0; frame < 3; ++frame) {
      bench.view.HideMeter(true, bench.windows);
      bench.Refresh();
      CHECK(bench.windows.main.tab == 2);
      CHECK(bench.Shown() == "C");
    }
    bench.view.HideMeter(false, bench.windows);
    bench.Refresh();
    CHECK(bench.windows.main.tab == 3);
    CHECK(bench.Shown() == "C");
  }
  SECTION("la première image pose l'état sans rien faire glisser") {
    bench.view.HideMeter(true, bench.windows);
    CHECK(bench.windows.main.tab == 3);
  }
}
