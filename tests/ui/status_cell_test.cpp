// Le placement et l'ordre d'une rangée d'états.
//
// 🔴 C'EST LA SEULE PART DE CETTE CASE QU'UN BANC PEUT PROUVER, et c'est
// justement celle qui se trompe en silence : une rangée qui grandit du mauvais
// côté, un plafond mal réparti, un état permanent rangé en tête des urgences.
// Le dessin, lui, se juge à l'œil — À UNE PART PRÈS. Le balayage horaire déborde
// de sa case par construction, et c'est un clip qui l'y ramène : ce défaut-là ne
// se voit pas sur la case qu'on regarde, mais sur sa VOISINE.

#include "ro/ui/status_cell.h"

#include <cstdint>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

using ro::ui::OrderStatusCells;
using ro::ui::PaintStatusSweep;
using ro::ui::PlaceStatusRow;
using ro::ui::StatusCellView;
using ro::ui::StatusRowLayout;
using ro::ui::StatusSort;

namespace {

// Une disposition nette : des cases de dix, un jour de deux.
[[nodiscard]] StatusRowLayout Grid(int most, int rows) {
  StatusRowLayout layout;
  layout.side = 10.0F;
  layout.gap = 2.0F;
  layout.most = most;
  layout.rows = rows;
  return layout;
}

[[nodiscard]] StatusCellView Lasting(std::uint32_t remaining_ms) {
  StatusCellView cell;
  cell.remaining_ms = remaining_ms;
  cell.total_ms = remaining_ms;
  return cell;
}

}  // namespace

TEST_CASE("Rangée d'états : le plafond se répartit sur les lignes", "[ui][etats]") {
  // Huit cases sur trois lignes font trois, trois, deux : la répartition
  // s'arrondit vers le haut.
  const std::vector<ImVec2> places = PlaceStatusRow(8, ImVec2(100.0F, 50.0F), Grid(8, 3));
  REQUIRE(places.size() == 8);

  // La première ligne : trois cases, de gauche à droite.
  CHECK(places[0].x == 100.0F);
  CHECK(places[1].x == 112.0F);
  CHECK(places[2].x == 124.0F);
  CHECK(places[0].y == 50.0F);
  CHECK(places[2].y == 50.0F);

  // La quatrième repart à gauche, une ligne plus bas.
  CHECK(places[3].x == 100.0F);
  CHECK(places[3].y == 62.0F);

  // Et la dernière ligne n'en porte que deux.
  CHECK(places[6].y == 74.0F);
  CHECK(places[7].y == 74.0F);
}

TEST_CASE("Rangée d'états : le plafond coupe, il ne déborde pas", "[ui][etats]") {
  const std::vector<ImVec2> places = PlaceStatusRow(12, ImVec2(0.0F, 0.0F), Grid(5, 1));
  CHECK(places.size() == 5);

  // Et zéro case ne place rien, sans se plaindre.
  CHECK(PlaceStatusRow(0, ImVec2(0.0F, 0.0F), Grid(8, 1)).empty());
}

TEST_CASE("Rangée d'états : les deux sens partent de l'origine", "[ui][etats]") {
  // 🔴 LE SENS N'EST PAS COSMÉTIQUE : une rangée qui grandit vers la gauche
  // garde ses icônes en place quand une nouvelle tombe. Empiler par la gauche
  // ferait glisser toute la rangée à chaque changement.
  StatusRowLayout leftward = Grid(4, 1);
  leftward.right_to_left = true;
  const std::vector<ImVec2> places = PlaceStatusRow(3, ImVec2(200.0F, 0.0F), leftward);
  REQUIRE(places.size() == 3);
  // La première case touche l'origine par son bord DROIT.
  CHECK(places[0].x == 190.0F);
  CHECK(places[1].x == 178.0F);
  CHECK(places[2].x == 166.0F);

  StatusRowLayout upward = Grid(4, 2);
  upward.upward = true;
  const std::vector<ImVec2> stacked = PlaceStatusRow(3, ImVec2(0.0F, 300.0F), upward);
  REQUIRE(stacked.size() == 3);
  // La première ligne colle au bord bas, les suivantes montent.
  CHECK(stacked[0].y == 290.0F);
  CHECK(stacked[2].y == 278.0F);
}

TEST_CASE("Rangée d'états : l'ordre d'arrivée est celui qu'on donne", "[ui][etats]") {
  const std::vector<StatusCellView> cells = {Lasting(5'000), Lasting(90'000), Lasting(1'000)};
  const std::vector<std::size_t> order = OrderStatusCells(cells, StatusSort::kArrival, 8);
  CHECK(order == std::vector<std::size_t>{0, 1, 2});
}

TEST_CASE("Rangée d'états : un permanent n'est pas une urgence", "[ui][etats]") {
  // 🔴 ZÉRO VEUT DIRE « SANS ÉCHÉANCE », pas « fini dans zéro milliseconde ».
  // Rangé tel quel, un buff permanent aurait pris la tête des états à relancer —
  // exactement la place de ce qui va tomber.
  const std::vector<StatusCellView> cells = {Lasting(0), Lasting(90'000), Lasting(1'000)};

  const std::vector<std::size_t> urgent = OrderStatusCells(cells, StatusSort::kEndingSoon, 8);
  CHECK(urgent == std::vector<std::size_t>{2, 1, 0});

  // Et sous « plus long d'abord », il passe devant : rien ne dure plus qu'un
  // état qui ne finit pas.
  const std::vector<std::size_t> stable = OrderStatusCells(cells, StatusSort::kLongest, 8);
  CHECK(stable == std::vector<std::size_t>{0, 1, 2});
}

TEST_CASE("Rangée d'états : deux durées égales gardent leur ordre d'arrivée", "[ui][etats]") {
  // ⚠ LE TRI EST STABLE, et c'est ce qui empêche deux icônes d'échanger leur
  // place à chaque image — le pire des défauts pour l'œil qui suit une durée.
  const std::vector<StatusCellView> cells = {Lasting(5'000), Lasting(5'000), Lasting(5'000)};
  CHECK(OrderStatusCells(cells, StatusSort::kEndingSoon, 8) == std::vector<std::size_t>{0, 1, 2});
}

TEST_CASE("Rangée d'états : le tri coupe APRÈS avoir choisi", "[ui][etats]") {
  // 🔴 L'ORDRE DÉCIDE DE CE QUI RESTE. Couper d'abord garderait les trois
  // premiers arrivés, et le plus urgent des états tomberait sans qu'on l'ait vu.
  const std::vector<StatusCellView> cells = {Lasting(90'000), Lasting(80'000), Lasting(70'000),
                                             Lasting(1'000)};
  CHECK(OrderStatusCells(cells, StatusSort::kEndingSoon, 2) == std::vector<std::size_t>{3, 2});
}

// ── LE BALAYAGE ────────────────────────────────────────────────────────────
//
// ⚠ UNE LISTE DE DESSIN NUE, SANS CONTEXTE IMGUI NI ATLAS : le balayage ne pose
// que des triangles pleins, et `ImDrawList` sait vivre seul. C'est ce qui rend
// cette part du dessin essayable sans fenêtre ni GPU.
namespace {

// La case d'essai : cent pixels de côté, posée loin de l'origine pour qu'un
// rectangle oublié à zéro ne passe pas pour le bon.
constexpr float kCellSide = 100.0F;
const ImVec2 kTopLeft(200.0F, 300.0F);
const ImVec2 kBottomRight(200.0F + kCellSide, 300.0F + kCellSide);

struct LoneDrawList {
  ImDrawListSharedData shared;
  ImDrawList into;

  LoneDrawList() : into(&shared) {
    shared.ClipRectFullscreen = ImVec4(0.0F, 0.0F, 4096.0F, 4096.0F);
    into._ResetForNewFrame();
    into.PushClipRect(ImVec2(0.0F, 0.0F), ImVec2(4096.0F, 4096.0F), false);
    into.PushTextureID(ImTextureRef());
  }
};

}  // namespace

TEST_CASE("Balayage : le clip le retient dans sa case", "[ui][etats]") {
  LoneDrawList sheet;
  PaintStatusSweep(sheet.into, kTopLeft, kBottomRight, 0.75F, IM_COL32(0, 0, 0, 160));

  // 🔴 LE VOILE EST BORNÉ À LA CASE, ET C'EST LE CLIP QUI LE BORNE. Sans lui, le
  // disque mord sur les cases voisines et sur la carte — c'est exactement ce qui
  // s'est vu en jeu.
  bool clipped = false;
  for (const ImDrawCmd& order : sheet.into.CmdBuffer) {
    if (order.ElemCount == 0) {
      continue;
    }
    CHECK(order.ClipRect.x >= kTopLeft.x);
    CHECK(order.ClipRect.y >= kTopLeft.y);
    CHECK(order.ClipRect.z <= kBottomRight.x);
    CHECK(order.ClipRect.w <= kBottomRight.y);
    clipped = true;
  }
  CHECK(clipped);

  // ⚠ ET LA GÉOMÉTRIE, ELLE, DÉBORDE — c'est voulu : un rayon qui s'arrêterait au
  // bord laisserait les quatre coins clairs sur un état presque fini. Si ce
  // débordement disparaît, le clip ci-dessus ne prouve plus rien.
  bool beyond = false;
  for (const ImDrawVert& corner : sheet.into.VtxBuffer) {
    beyond = beyond || corner.pos.x > kBottomRight.x || corner.pos.y > kBottomRight.y ||
             corner.pos.x < kTopLeft.x || corner.pos.y < kTopLeft.y;
  }
  CHECK(beyond);
}

TEST_CASE("Balayage : rien à peindre au premier instant", "[ui][etats]") {
  // Un état qui vient de commencer n'a pas de part écoulée : pas un triangle.
  LoneDrawList fresh;
  PaintStatusSweep(fresh.into, kTopLeft, kBottomRight, 0.0F, IM_COL32(0, 0, 0, 160));
  CHECK(fresh.into.VtxBuffer.Size == 0);

  // 🔴 ET LE SECTEUR PASSE LE DEMI-TOUR SANS SE TRANCHER. Un remplissage convexe
  // rendrait au-delà de 0,5 une forme FAUSSE mais
  // plausible : trois quarts de tour doivent poser plus de triangles que la
  // moitié, et la moitié plus que le quart.
  LoneDrawList quarter;
  LoneDrawList half;
  LoneDrawList most;
  PaintStatusSweep(quarter.into, kTopLeft, kBottomRight, 0.25F, IM_COL32(0, 0, 0, 160));
  PaintStatusSweep(half.into, kTopLeft, kBottomRight, 0.5F, IM_COL32(0, 0, 0, 160));
  PaintStatusSweep(most.into, kTopLeft, kBottomRight, 0.75F, IM_COL32(0, 0, 0, 160));
  CHECK(quarter.into.VtxBuffer.Size < half.into.VtxBuffer.Size);
  CHECK(half.into.VtxBuffer.Size < most.into.VtxBuffer.Size);
}
