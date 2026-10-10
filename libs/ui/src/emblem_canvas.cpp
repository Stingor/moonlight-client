#include "ro/ui/emblem_canvas.h"

#include <algorithm>
#include <cmath>

namespace ro::ui {
namespace {

constexpr float kChannelMost = 255.0F;
constexpr float kRoundHalf = 0.5F;
constexpr unsigned kRedShift = 16;
constexpr unsigned kGreenShift = 8;
// Le demi-axe le plus court d'une ellipse, pour qu'une ellipse tirée sur une
// seule ligne reste une ligne et non une division par zéro.
constexpr float kLeastRadius = 0.5F;

[[nodiscard]] std::size_t CellIndex(int column, int row) {
  return static_cast<std::size_t>(row) * kEmblemCanvasSide + static_cast<std::size_t>(column);
}

[[nodiscard]] bool Inside(int column, int row) {
  return column >= 0 && column < kEmblemCanvasSide && row >= 0 && row < kEmblemCanvasSide;
}

// L'ÉTENDUE DU PINCEAU autour de la case pointée, pour que « N px » dessine un
// carré de N pixels de côté. ⚠ UN RAYON SYMÉTRIQUE DONNERAIT 2N − 1 : pour une
// épaisseur paire, le carré déborde d'un pixel vers la droite et le bas.
void BrushExtent(const EmblemCanvas& canvas, int& low, int& high) {
  const int size = std::clamp(canvas.brush, 1, kEmblemBrushMost);
  low = -((size - 1) / 2);
  high = size / 2;
}

// L'EMPREINTE DU PINCEAU, une fois : le carré, ses bornes et son miroir. Seule
// l'action à chaque case change — peindre, ou marquer un masque.
template <typename Plot>
void BrushCells(const EmblemCanvas& canvas, int column, int row, Plot plot) {
  int low = 0;
  int high = 0;
  BrushExtent(canvas, low, high);
  for (int dy = low; dy <= high; ++dy) {
    for (int dx = low; dx <= high; ++dx) {
      const int x = column + dx;
      const int y = row + dy;
      if (!Inside(x, y)) {
        continue;
      }
      plot(x, y);
      if (canvas.mirror) {
        plot(kEmblemCanvasSide - 1 - x, y);
      }
    }
  }
}

// BRESENHAM, une fois.
template <typename Plot>
void BresenhamLine(int x0, int y0, int x1, int y1, Plot plot) {
  const int dx = std::abs(x1 - x0);
  const int dy = -std::abs(y1 - y0);
  const int sx = x0 < x1 ? 1 : -1;
  const int sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;
  while (true) {
    plot(x0, y0);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    const int twice = 2 * error;
    if (twice >= dy) {
      error += dy;
      x0 += sx;
    }
    if (twice <= dx) {
      error += dx;
      y0 += sy;
    }
  }
}

void MaskCell(const EmblemCanvas& canvas, EmblemMask& mask, int column, int row) {
  BrushCells(canvas, column, row, [&mask](int x, int y) { mask[CellIndex(x, y)] = true; });
}

void MaskLine(const EmblemCanvas& canvas, EmblemMask& mask, int x0, int y0, int x1, int y1) {
  BresenhamLine(x0, y0, x1, y1, [&](int x, int y) { MaskCell(canvas, mask, x, y); });
}

}  // namespace

std::uint32_t PackEmblemColor(const std::array<float, 3>& rgb) {
  const auto to_byte = [](float value) {
    return static_cast<std::uint32_t>(std::clamp(value, 0.0F, 1.0F) * kChannelMost + kRoundHalf);
  };
  return (to_byte(rgb[0]) << kRedShift) | (to_byte(rgb[1]) << kGreenShift) | to_byte(rgb[2]);
}

std::uint32_t EmblemInk(const EmblemCanvas& canvas) {
  return canvas.color_clear ? kEmblemCanvasClear : PackEmblemColor(canvas.color);
}

void ClearEmblemCanvas(EmblemCanvas& canvas) {
  canvas.pixel.fill(kEmblemCanvasClear);
  canvas.started = true;
  ++canvas.revision;
}

void PushEmblemUndo(EmblemCanvas& canvas) {
  if (!canvas.started) {
    return;
  }
  canvas.undo.push_back(canvas.pixel);
  if (canvas.undo.size() > kEmblemUndoMost) {
    canvas.undo.erase(canvas.undo.begin());
  }
}

bool UndoEmblemCanvas(EmblemCanvas& canvas) {
  if (canvas.undo.empty()) {
    return false;
  }
  canvas.pixel = canvas.undo.back();
  canvas.undo.pop_back();
  ++canvas.revision;
  return true;
}

void PaintEmblemCell(EmblemCanvas& canvas, int column, int row, std::uint32_t color) {
  // ⚠ LA RÉVISION MONTE UNE FOIS PAR GESTE, pas par case du pinceau.
  ++canvas.revision;
  BrushCells(canvas, column, row,
             [&canvas, color](int x, int y) { canvas.pixel[CellIndex(x, y)] = color; });
}

void StrokeEmblemCanvas(EmblemCanvas& canvas, int from_column, int from_row, int to_column,
                        int to_row, std::uint32_t color) {
  BresenhamLine(from_column, from_row, to_column, to_row,
                [&canvas, color](int x, int y) { PaintEmblemCell(canvas, x, y, color); });
}

void FillEmblemCanvas(EmblemCanvas& canvas, int column, int row, std::uint32_t color) {
  if (!Inside(column, row)) {
    return;
  }
  const std::uint32_t target = canvas.pixel[CellIndex(column, row)];
  if (target == color) {
    return;
  }
  ++canvas.revision;
  std::vector<std::size_t> stack{CellIndex(column, row)};
  while (!stack.empty()) {
    const std::size_t index = stack.back();
    stack.pop_back();
    if (canvas.pixel[index] != target) {
      continue;
    }
    canvas.pixel[index] = color;
    const int x = static_cast<int>(index % kEmblemCanvasSide);
    const int y = static_cast<int>(index / kEmblemCanvasSide);
    if (x > 0) {
      stack.push_back(index - 1);
    }
    if (x < kEmblemCanvasSide - 1) {
      stack.push_back(index + 1);
    }
    if (y > 0) {
      stack.push_back(index - kEmblemCanvasSide);
    }
    if (y < kEmblemCanvasSide - 1) {
      stack.push_back(index + kEmblemCanvasSide);
    }
  }
}

EmblemMask EmblemShapeMask(const EmblemCanvas& canvas, int to_column, int to_row) {
  EmblemMask mask = {};
  const int x0 = canvas.shape_x0;
  const int y0 = canvas.shape_y0;
  if (canvas.tool == EmblemTool::kLine) {
    MaskLine(canvas, mask, x0, y0, to_column, to_row);
    return mask;
  }
  const int left = std::min(x0, to_column);
  const int right = std::max(x0, to_column);
  const int top = std::min(y0, to_row);
  const int bottom = std::max(y0, to_row);
  if (canvas.tool == EmblemTool::kRect) {
    if (canvas.filled) {
      for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
          MaskCell(canvas, mask, x, y);
        }
      }
    } else {
      MaskLine(canvas, mask, left, top, right, top);
      MaskLine(canvas, mask, left, bottom, right, bottom);
      MaskLine(canvas, mask, left, top, left, bottom);
      MaskLine(canvas, mask, right, top, right, bottom);
    }
    return mask;
  }
  // L'ELLIPSE INSCRITE dans le rectangle tiré. Le contour = les cases DANS
  // l'ellipse dont un voisin est dehors : sur 24 × 24 c'est plus net qu'un
  // tracé paramétrique.
  const float center_x = static_cast<float>(left + right) * kRoundHalf;
  const float center_y = static_cast<float>(top + bottom) * kRoundHalf;
  const float radius_x = std::max(static_cast<float>(right - left) * kRoundHalf, kLeastRadius);
  const float radius_y = std::max(static_cast<float>(bottom - top) * kRoundHalf, kLeastRadius);
  const auto inside = [&](int x, int y) {
    const float nx = (static_cast<float>(x) - center_x) / radius_x;
    const float ny = (static_cast<float>(y) - center_y) / radius_y;
    return nx * nx + ny * ny <= 1.0F;
  };
  for (int y = top; y <= bottom; ++y) {
    for (int x = left; x <= right; ++x) {
      if (!inside(x, y)) {
        continue;
      }
      if (canvas.filled || !inside(x - 1, y) || !inside(x + 1, y) || !inside(x, y - 1) ||
          !inside(x, y + 1)) {
        MaskCell(canvas, mask, x, y);
      }
    }
  }
  return mask;
}

void ApplyEmblemMask(EmblemCanvas& canvas, const EmblemMask& mask, std::uint32_t color) {
  ++canvas.revision;
  for (std::size_t index = 0; index < kEmblemCanvasPixels; ++index) {
    if (mask[index]) {
      canvas.pixel[index] = color;
    }
  }
}

void LoadEmblemCanvas(EmblemCanvas& canvas, const EmblemCanvasPixels& pixels) {
  PushEmblemUndo(canvas);
  canvas.pixel = pixels;
  canvas.started = true;
  ++canvas.revision;
}

bool EmblemCanvasHasInk(const EmblemCanvas& canvas) {
  return canvas.started && std::ranges::any_of(canvas.pixel, [](std::uint32_t color) {
           return color != kEmblemCanvasClear;
         });
}

}  // namespace ro::ui
