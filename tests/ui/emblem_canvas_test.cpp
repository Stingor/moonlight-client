// LE CANEVAS DE L'ÉDITEUR D'EMBLÈME, sans rien dessiner.
//
// 🔴 CE BANC PORTE CE QUE L'ŒIL NE VÉRIFIE PAS SUR 24 × 24 :
//
//   · « N px » peint un carré de N pixels de côté, et non de 2N − 1 ;
//   · le miroir repasse sur la colonne symétrique ;
//   · un trait rapide se RELIE, sans pointillés ;
//   · le remplissage s'arrête aux bords de sa couleur, en quatre-connexité ;
//   · « Annuler » remonte coup par coup, et la pile est bornée.

#include "ro/ui/emblem_canvas.h"

#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using ro::ui::ApplyEmblemMask;
using ro::ui::ClearEmblemCanvas;
using ro::ui::EmblemCanvas;
using ro::ui::EmblemCanvasHasInk;
using ro::ui::EmblemCanvasPixels;
using ro::ui::EmblemMask;
using ro::ui::EmblemShapeMask;
using ro::ui::EmblemTool;
using ro::ui::FillEmblemCanvas;
using ro::ui::kEmblemCanvasClear;
using ro::ui::kEmblemCanvasSide;
using ro::ui::kEmblemUndoMost;
using ro::ui::LoadEmblemCanvas;
using ro::ui::PaintEmblemCell;
using ro::ui::PushEmblemUndo;
using ro::ui::StrokeEmblemCanvas;
using ro::ui::UndoEmblemCanvas;

namespace {

constexpr std::uint32_t kInk = 0x123456U;

[[nodiscard]] std::size_t At(int column, int row) {
  return static_cast<std::size_t>(row) * kEmblemCanvasSide + static_cast<std::size_t>(column);
}

[[nodiscard]] int Painted(const EmblemCanvas& canvas) {
  int count = 0;
  for (const std::uint32_t color : canvas.pixel) {
    count += color != kEmblemCanvasClear ? 1 : 0;
  }
  return count;
}

[[nodiscard]] int Marked(const EmblemMask& mask) {
  int count = 0;
  for (const bool cell : mask) {
    count += cell ? 1 : 0;
  }
  return count;
}

[[nodiscard]] EmblemCanvas Blank() {
  EmblemCanvas canvas;
  ClearEmblemCanvas(canvas);
  return canvas;
}

}  // namespace

TEST_CASE("Canevas : vide, il n'a pas d'encre ; un tableau de zéros n'est pas vide",
          "[ui][emblem_canvas]") {
  EmblemCanvas fresh;
  CHECK_FALSE(EmblemCanvasHasInk(fresh));
  const EmblemCanvas blank = Blank();
  CHECK_FALSE(EmblemCanvasHasInk(blank));
  CHECK(blank.pixel[0] == kEmblemCanvasClear);
}

TEST_CASE("Canevas : le pinceau peint un carré de N pixels de côté", "[ui][emblem_canvas]") {
  EmblemCanvas canvas = Blank();
  SECTION("un pixel") {
    PaintEmblemCell(canvas, 5, 5, kInk);
    CHECK(Painted(canvas) == 1);
  }
  SECTION("deux pixels : le carré déborde vers la droite et le bas") {
    canvas.brush = 2;
    PaintEmblemCell(canvas, 5, 5, kInk);
    CHECK(Painted(canvas) == 4);
    CHECK(canvas.pixel[At(6, 6)] == kInk);
    CHECK(canvas.pixel[At(4, 4)] == kEmblemCanvasClear);
  }
  SECTION("trois pixels, centrés") {
    canvas.brush = 3;
    PaintEmblemCell(canvas, 5, 5, kInk);
    CHECK(Painted(canvas) == 9);
    CHECK(canvas.pixel[At(4, 4)] == kInk);
  }
  SECTION("au bord, le carré est rogné") {
    canvas.brush = 3;
    PaintEmblemCell(canvas, 0, 0, kInk);
    CHECK(Painted(canvas) == 4);
  }
}

TEST_CASE("Canevas : le miroir repasse sur la colonne symétrique", "[ui][emblem_canvas]") {
  EmblemCanvas canvas = Blank();
  canvas.mirror = true;
  PaintEmblemCell(canvas, 2, 7, kInk);
  CHECK(canvas.pixel[At(2, 7)] == kInk);
  CHECK(canvas.pixel[At(kEmblemCanvasSide - 1 - 2, 7)] == kInk);
  CHECK(Painted(canvas) == 2);
}

TEST_CASE("Canevas : un trait rapide se relie", "[ui][emblem_canvas]") {
  EmblemCanvas canvas = Blank();
  StrokeEmblemCanvas(canvas, 0, 0, 10, 0, kInk);
  CHECK(Painted(canvas) == 11);
  StrokeEmblemCanvas(canvas, 0, 5, 5, 10, kInk);
  for (int step = 0; step <= 5; ++step) {
    CHECK(canvas.pixel[At(step, 5 + step)] == kInk);
  }
}

TEST_CASE("Canevas : le remplissage s'arrête au bord de sa couleur", "[ui][emblem_canvas]") {
  EmblemCanvas canvas = Blank();
  // Un cadre fermé de 5 × 5, dont l'intérieur fait 3 × 3.
  StrokeEmblemCanvas(canvas, 2, 2, 6, 2, kInk);
  StrokeEmblemCanvas(canvas, 2, 6, 6, 6, kInk);
  StrokeEmblemCanvas(canvas, 2, 2, 2, 6, kInk);
  StrokeEmblemCanvas(canvas, 6, 2, 6, 6, kInk);
  const int frame = Painted(canvas);
  constexpr std::uint32_t kFill = 0xABCDEFU;
  FillEmblemCanvas(canvas, 4, 4, kFill);
  CHECK(Painted(canvas) == frame + 9);
  CHECK(canvas.pixel[At(0, 0)] == kEmblemCanvasClear);
  // Remplir de la couleur déjà en place ne fait rien.
  const int revision = canvas.revision;
  FillEmblemCanvas(canvas, 4, 4, kFill);
  CHECK(canvas.revision == revision);
}

TEST_CASE("Canevas : les formes tirées", "[ui][emblem_canvas]") {
  EmblemCanvas canvas = Blank();
  canvas.shape_x0 = 2;
  canvas.shape_y0 = 2;
  SECTION("une ligne") {
    canvas.tool = EmblemTool::kLine;
    CHECK(Marked(EmblemShapeMask(canvas, 9, 2)) == 8);
  }
  SECTION("un rectangle en contour, puis plein") {
    canvas.tool = EmblemTool::kRect;
    CHECK(Marked(EmblemShapeMask(canvas, 6, 6)) == 16);
    canvas.filled = true;
    CHECK(Marked(EmblemShapeMask(canvas, 6, 6)) == 25);
  }
  SECTION("une ellipse pleine couvre plus que son contour") {
    canvas.tool = EmblemTool::kEllipse;
    const int outline = Marked(EmblemShapeMask(canvas, 12, 12));
    canvas.filled = true;
    const int filled = Marked(EmblemShapeMask(canvas, 12, 12));
    CHECK(outline > 0);
    CHECK(filled > outline);
  }
  SECTION("le masque s'applique tel qu'il s'est montré") {
    canvas.tool = EmblemTool::kRect;
    const EmblemMask mask = EmblemShapeMask(canvas, 4, 4);
    ApplyEmblemMask(canvas, mask, kInk);
    CHECK(Painted(canvas) == Marked(mask));
  }
}

TEST_CASE("Canevas : « Annuler » remonte coup par coup, et la pile est bornée",
          "[ui][emblem_canvas]") {
  EmblemCanvas canvas = Blank();
  PushEmblemUndo(canvas);
  PaintEmblemCell(canvas, 1, 1, kInk);
  REQUIRE(UndoEmblemCanvas(canvas));
  CHECK(Painted(canvas) == 0);
  CHECK_FALSE(UndoEmblemCanvas(canvas));

  for (std::size_t stroke = 0; stroke < kEmblemUndoMost + 10; ++stroke) {
    PushEmblemUndo(canvas);
  }
  CHECK(canvas.undo.size() == kEmblemUndoMost);

  // Charger un dessin venu d'ailleurs ne perd pas celui en cours.
  canvas.undo.clear();
  EmblemCanvasPixels loaded;
  loaded.fill(kInk);
  LoadEmblemCanvas(canvas, loaded);
  CHECK(Painted(canvas) == static_cast<int>(loaded.size()));
  REQUIRE(UndoEmblemCanvas(canvas));
  CHECK(Painted(canvas) == 0);
}
