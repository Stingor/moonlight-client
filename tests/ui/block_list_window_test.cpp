// LA FENÊTRE DES JOUEURS BLOQUÉS, dessinée hors jeu.
//
// Ce qui se vérifie sans écran : fermée elle ne dessine rien, ouverte elle ne
// rend aucun geste de son propre chef, en ligne ou non, vide ou pleine ; et le
// filtre ne garde que les noms qui le contiennent, à la casse près.

#include "ro/ui/block_list_window.h"

#include <cstddef>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::BlockedPlayerView;
using ro::ui::BlockListAnswer;
using ro::ui::BlockListVerb;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr const char* kWindowName = "###joueurs_bloques";
constexpr std::size_t kManyPlayers = 12;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

struct BlockProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  ro::ui::BlockListWindowState state;
  ro::ui::BlockListView view;

  BlockProbe() {
    state.open = true;
    view.connected = true;
    view.voice_online = true;
    view.whispers_kept = true;
    view.players = {{.name = "Alice", .whispers = true, .voice = true, .voice_reachable = true},
                    {.name = "Bob", .whispers = true, .voice = false, .voice_reachable = false}};
  }

  BlockListAnswer Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    BlockListAnswer answer = ro::ui::DrawBlockListWindow(state, view, art);
    ui.EndFrame();
    return answer;
  }
};

[[nodiscard]] std::vector<BlockedPlayerView> Named(std::vector<std::string> names) {
  std::vector<BlockedPlayerView> players;
  for (std::string& name : names) {
    players.push_back(BlockedPlayerView{.name = std::move(name), .whispers = true});
  }
  return players;
}

}  // namespace

TEST_CASE("Joueurs bloqués : fermée, rien ne se dessine", "[ui][block_list]") {
  BlockProbe probe;
  probe.state.open = false;
  for (int frame = 0; frame < 3; ++frame) {
    CHECK(probe.Frame().verb == BlockListVerb::kNothing);
  }
  CHECK(ImGui::FindWindowByName(kWindowName) == nullptr);
}

TEST_CASE("Joueurs bloqués : ouverte, elle ne rend aucun geste d'elle-même", "[ui][block_list]") {
  BlockProbe probe;
  SECTION("en ligne") {}
  SECTION("hors ligne, voix fermée, liste non gardée") {
    probe.view.connected = false;
    probe.view.voice_online = false;
    probe.view.whispers_kept = false;
  }
  SECTION("vide") { probe.view.players.clear(); }
  SECTION("longue, avec le filtre") {
    for (std::size_t rank = 0; rank < kManyPlayers; ++rank) {
      probe.view.players.push_back(BlockedPlayerView{.name = "Joueur" + std::to_string(rank)});
    }
  }
  SECTION("tout le monde bloqué") { probe.view.whispers_from_everyone = true; }
  for (int frame = 0; frame < 3; ++frame) {
    CHECK(probe.Frame().verb == BlockListVerb::kNothing);
  }
  CHECK(ImGui::FindWindowByName(kWindowName) != nullptr);
  CHECK(probe.state.open);
}

TEST_CASE("Joueurs bloqués : remontée, elle s'ouvre devant", "[ui][block_list]") {
  BlockProbe probe;
  probe.state.open = false;
  ro::ui::RaiseBlockListWindow(probe.state);
  CHECK(probe.state.open);
  static_cast<void>(probe.Frame());
  CHECK_FALSE(probe.state.raise);
}

TEST_CASE("Joueurs bloqués : le filtre garde les noms qui le contiennent", "[ui][block_list]") {
  const std::vector<BlockedPlayerView> players = Named({"Alice", "Bob", "Élodie", "malice"});
  CHECK(ro::ui::BlockListShown(players, "") == std::vector<std::size_t>{0, 1, 2, 3});
  CHECK(ro::ui::BlockListShown(players, "LIC") == std::vector<std::size_t>{0, 3});
  CHECK(ro::ui::BlockListShown(players, "élo") == std::vector<std::size_t>{2});
  CHECK(ro::ui::BlockListShown(players, "zed").empty());
}
