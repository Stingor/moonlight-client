#include "ro/render/skin_islands.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <utility>
#include <vector>

namespace ro::render {
namespace {

using IndexFlags = std::array<bool, formats::kPaletteEntryCount>;

// LA CLARTÉ d'une entrée : sa plus forte composante, de 0 à 255.
[[nodiscard]] int ValueOf(const formats::Palette& palette, std::size_t index) {
  const auto& entry = palette.entries[index];
  return std::max({entry[0], entry[1], entry[2]});
}

// Pour chaque entrée, l'entrée de peau de clarté la plus proche.
[[nodiscard]] std::array<std::uint8_t, formats::kPaletteEntryCount> NearestSkin(
    const formats::Palette& palette, std::span<const std::uint8_t> skin) {
  std::array<std::uint8_t, formats::kPaletteEntryCount> nearest{};
  for (std::size_t index = 0; index < nearest.size(); ++index) {
    const int value = ValueOf(palette, index);
    nearest[index] = *std::ranges::min_element(skin, {}, [&](std::uint8_t candidate) {
      return std::abs(ValueOf(palette, candidate) - value);
    });
  }
  return nearest;
}

// Les îlots d'une image, repris ; rend le nombre de pixels repris.
int FoldFrame(formats::SprIndexedFrame& frame, const IndexFlags& on_skin, const IndexFlags& dark,
              const std::array<std::uint8_t, formats::kPaletteEntryCount>& nearest) {
  const std::size_t size =
      static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height);
  if (frame.indices.size() < size) {
    return 0;
  }
  const auto flat = [&frame](int x, int y) {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) +
           static_cast<std::size_t>(x);
  };
  constexpr std::array<std::pair<int, int>, 4> kSteps = {std::pair{-1, 0}, std::pair{1, 0},
                                                         std::pair{0, -1}, std::pair{0, 1}};
  std::vector<bool> seen(size, false);
  std::vector<std::size_t> group;
  std::vector<std::pair<int, int>> todo;
  int folded = 0;
  for (int y = 0; y < frame.height; ++y) {
    for (int x = 0; x < frame.width; ++x) {
      const std::uint8_t start = frame.indices[flat(x, y)];
      if (start == 0 || on_skin[start] || seen[flat(x, y)]) {
        continue;
      }
      // LE GROUPE D'UN SEUL TENANT, hors peau : tout voisin hors peau le
      // rejoint, si bien que son bord n'est que peau, transparence ou bord.
      group.clear();
      todo.assign(1, {x, y});
      seen[flat(x, y)] = true;
      bool open = false;
      bool shadowed = false;
      while (!todo.empty()) {
        const auto [cx, cy] = todo.back();
        todo.pop_back();
        group.push_back(flat(cx, cy));
        shadowed = shadowed || dark[frame.indices[flat(cx, cy)]];
        for (const auto& [dx, dy] : kSteps) {
          const int nx = cx + dx;
          const int ny = cy + dy;
          if (nx < 0 || ny < 0 || nx >= frame.width || ny >= frame.height) {
            open = true;
            continue;
          }
          const std::uint8_t next = frame.indices[flat(nx, ny)];
          if (next == 0) {
            open = true;
          } else if (!on_skin[next] && !seen[flat(nx, ny)]) {
            seen[flat(nx, ny)] = true;
            todo.emplace_back(nx, ny);
          }
        }
      }
      if (open || shadowed || group.size() > static_cast<std::size_t>(kSkinIslandMostPixels)) {
        continue;
      }
      for (const std::size_t at : group) {
        frame.indices[at] = nearest[frame.indices[at]];
      }
      folded += static_cast<int>(group.size());
    }
  }
  return folded;
}

}  // namespace

int FoldSkinIslands(formats::SprSprite& sprite, std::span<const std::uint8_t> skin) {
  if (skin.empty() || !sprite.palette().has_value()) {
    return 0;
  }
  const formats::Palette& palette = *sprite.palette();
  IndexFlags on_skin{};
  int darkest_skin = ValueOf(palette, skin.front());
  for (const std::uint8_t index : skin) {
    on_skin[index] = true;
    darkest_skin = std::min(darkest_skin, ValueOf(palette, index));
  }
  IndexFlags dark{};
  for (std::size_t index = 1; index < dark.size(); ++index) {
    dark[index] = ValueOf(palette, index) < darkest_skin;
  }
  const std::array<std::uint8_t, formats::kPaletteEntryCount> nearest = NearestSkin(palette, skin);
  int folded = 0;
  for (formats::SprIndexedFrame& frame : sprite.edited_indexed_frames()) {
    folded += FoldFrame(frame, on_skin, dark, nearest);
  }
  return folded;
}

}  // namespace ro::render
