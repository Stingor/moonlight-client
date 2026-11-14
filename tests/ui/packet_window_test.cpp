// L'inspecteur de trames, sans serveur et sans carte graphique.

#include "ro/ui/packet_window.h"

#include <array>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

// Trois opcodes : un entièrement traité, un dont tout se perd, un mixte.
constexpr std::array<ro::ui::PacketRow, 3> kTally = {
    ro::ui::PacketRow{.opcode = 0x0080, .applied = 40, .ignored = 2},
    ro::ui::PacketRow{.opcode = 0x0284, .unhandled = 7},
    ro::ui::PacketRow{.opcode = 0x0B0D, .applied = 1, .undecodable = 3},
};

}  // namespace

TEST_CASE("le relevé ne montre que ce qui s'est perdu, le plus fréquent d'abord", "[ui]") {
  ro::ui::PacketWindowState state;
  REQUIRE(state.missing_only);

  const std::vector<ro::ui::PacketRow> kept = ro::ui::SelectPacketRows(kTally, state);
  REQUIRE(kept.size() == 2);
  CHECK(kept[0].opcode == 0x0284);
  CHECK(kept[1].opcode == 0x0B0D);
  // L'opcode entièrement traité ne décrit aucun travail : il sort.
  for (const ro::ui::PacketRow& row : kept) {
    CHECK(row.missing() > 0);
  }
}

TEST_CASE("le relevé complet garde tout, par opcode croissant à égalité", "[ui]") {
  ro::ui::PacketWindowState state;
  state.missing_only = false;

  const std::vector<ro::ui::PacketRow> kept = ro::ui::SelectPacketRows(kTally, state);
  REQUIRE(kept.size() == 3);
  // Sept trames perdues, puis trois, puis aucune : l'ordre est celui du
  // TRAVAIL, et l'opcode entièrement traité descend en dernier.
  CHECK(kept[0].opcode == 0x0284);
  CHECK(kept[1].opcode == 0x0B0D);
  CHECK(kept[2].opcode == 0x0080);
}

TEST_CASE("à égalité de pertes, l'opcode tranche et l'ordre ne bouge plus", "[ui]") {
  // Sans ce second critère, deux opcodes à égalité s'échangeraient d'une image
  // à l'autre au gré du tri, et une liste qui se réordonne sous le curseur est
  // illisible. Il faut une ÉGALITÉ pour le vérifier : le relevé commun n'en a
  // aucune.
  const std::array<ro::ui::PacketRow, 3> tied = {
      ro::ui::PacketRow{.opcode = 0x0B0D, .unhandled = 5},
      ro::ui::PacketRow{.opcode = 0x0080, .unhandled = 5},
      ro::ui::PacketRow{.opcode = 0x0284, .undecodable = 5},
  };
  ro::ui::PacketWindowState state;

  const std::vector<ro::ui::PacketRow> kept = ro::ui::SelectPacketRows(tied, state);
  REQUIRE(kept.size() == 3);
  CHECK(kept[0].opcode == 0x0080);
  CHECK(kept[1].opcode == 0x0284);
  CHECK(kept[2].opcode == 0x0B0D);
}

TEST_CASE("les quatre compteurs partitionnent les trames d'un opcode", "[ui]") {
  const ro::ui::PacketRow row{
      .opcode = 0x0B0D, .applied = 1, .ignored = 2, .unhandled = 4, .undecodable = 8};
  CHECK(row.missing() == 12);
  CHECK(row.total() == 15);
}

TEST_CASE("la fenêtre fermée ne montre rien et ne dessine rien", "[ui]") {
  ro::ui::UiContext ui;
  ro::ui::PacketWindowState state;
  REQUIRE_FALSE(state.open);

  ui.BeginFrame(kViewport, kFrameSeconds);
  const std::size_t shown = ro::ui::DrawPacketWindow(state, kTally, 50);
  ui.EndFrame();

  CHECK(shown == 0);
}

TEST_CASE("la fenêtre ouverte montre exactement les lignes retenues", "[ui]") {
  // Le compte rendu est ce qui fait la différence entre « ça n'a pas planté » et
  // « elle a montré les deux opcodes perdus ».
  ro::ui::UiContext ui;
  ro::ui::PacketWindowState state;
  state.open = true;

  ui.BeginFrame(kViewport, kFrameSeconds);
  const std::size_t shown = ro::ui::DrawPacketWindow(state, kTally, 50);
  ui.EndFrame();
  CHECK(shown == 2);

  state.missing_only = false;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const std::size_t all = ro::ui::DrawPacketWindow(state, kTally, 50);
  ui.EndFrame();
  CHECK(all == 3);
}
