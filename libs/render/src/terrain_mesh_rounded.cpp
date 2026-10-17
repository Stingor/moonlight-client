#include <cmath>
#include <functional>
#include <map>
#include <optional>
#include <utility>

#include "ro/render/terrain_relief.h"

#include "terrain_faces.h"

namespace ro::render {
namespace {

template <std::size_t kSize>
std::array<float, kSize> Mix(const std::array<float, kSize>& from,
                             const std::array<float, kSize>& to, float t) {
  std::array<float, kSize> mixed{};
  for (std::size_t component = 0; component < kSize; ++component) {
    mixed[component] = std::lerp(from[component], to[component], t);
  }
  return mixed;
}

// Tout ce qu'un sommet porte, interpolé entre deux autres.
TerrainVertex Mix(const TerrainVertex& from, const TerrainVertex& to, float t) {
  return TerrainVertex{.position = Mix(from.position, to.position, t),
                       .texture_uv = Mix(from.texture_uv, to.texture_uv, t),
                       .lightmap_uv = Mix(from.lightmap_uv, to.lightmap_uv, t),
                       .color = Mix(from.color, to.color, t),
                       .normal = Mix(from.normal, to.normal, t),
                       .shape_normal = Mix(from.shape_normal, to.shape_normal, t)};
}

float StepFraction(std::int32_t step, std::int32_t steps) {
  return static_cast<float>(step) / static_cast<float>(steps);
}

// Le dessus d'un cube, en grille de `steps` sous-carreaux par côté.
//
// TEXTURE, LIGHTMAP ET COULEUR S'INTERPOLENT ENTRE LES QUATRE COINS, comme la
// carte graphique le fait sur les deux triangles d'origine : le cube garde sa
// région de texture et sa case de l'atlas. Seule la hauteur suit le carreau
// arrondi.
void EmitTop(TerrainBatch& out, const TerrainFace& face, const CubeRelief& relief,
             std::int32_t steps) {
  const auto base = static_cast<std::uint32_t>(out.vertices.size());
  const auto& corner = face.corners;
  for (std::int32_t row = 0; row <= steps; ++row) {
    const float along = StepFraction(row, steps);
    for (std::int32_t column = 0; column <= steps; ++column) {
      const float across = StepFraction(column, steps);
      TerrainVertex vertex =
          Mix(Mix(corner[0], corner[1], across), Mix(corner[2], corner[3], across), along);
      // L'axe vertical du monde monte, celui du fichier descend.
      vertex.position[1] = -ReliefHeightIn(relief, across, along);
      vertex.shape_normal = ReliefNormalIn(relief, across, along);
      out.vertices.push_back(vertex);
    }
  }
  // Chaque sous-carreau se plie selon la même diagonale que le cube entier :
  // du fond à gauche à l'avant à droite.
  const auto stride = static_cast<std::uint32_t>(steps + 1);
  for (std::int32_t row = 0; row < steps; ++row) {
    for (std::int32_t column = 0; column < steps; ++column) {
      const std::uint32_t front_left =
          base + static_cast<std::uint32_t>(row) * stride + static_cast<std::uint32_t>(column);
      const std::uint32_t front_right = front_left + 1;
      const std::uint32_t back_left = front_left + stride;
      const std::uint32_t back_right = back_left + 1;
      out.indices.insert(out.indices.end(),
                         {back_left, back_right, front_right, back_left, front_right, front_left});
    }
  }
}

// Une paroi, en `steps` bandes le long de son arête : son bord haut suit le
// carreau arrondi du cube, son bord bas celui du voisin, pour qu'elle ferme le
// dénivelé sans jour. `edge_height(côté, p)` rend la hauteur MONDE du bord du
// cube (`côté` faux) ou du voisin, au paramètre `p` du cube le long de l'arête.
//
// ⚠ LA BANDE SE PARCOURT DANS LE SENS DU CUBE, pas dans celui de la face : ses
// sommets se calculent alors par les mêmes opérations que ceux du dessus, et
// tombent sur eux au bit près. `against_face` dit que la face, elle, va dans
// l'autre sens : ses coins 0 et 1 s'échangent, et l'enroulement avec eux.
void EmitWall(TerrainBatch& out, const TerrainFace& face, std::int32_t steps, bool against_face,
              const std::function<float(bool, float)>& edge_height) {
  const auto base = static_cast<std::uint32_t>(out.vertices.size());
  const auto& corner = face.corners;
  const std::size_t start = against_face ? 1 : 0;
  const std::size_t end = 1 - start;
  for (std::int32_t step = 0; step <= steps; ++step) {
    const float fraction = StepFraction(step, steps);
    TerrainVertex top = Mix(corner[start], corner[end], fraction);
    top.position[1] = edge_height(false, fraction);
    top.shape_normal = top.normal;
    TerrainVertex bottom = Mix(corner[2 + start], corner[2 + end], fraction);
    bottom.position[1] = edge_height(true, fraction);
    bottom.shape_normal = bottom.normal;
    out.vertices.push_back(top);
    out.vertices.push_back(bottom);
  }
  for (std::int32_t step = 0; step < steps; ++step) {
    std::uint32_t top = base + 2 * static_cast<std::uint32_t>(step);
    std::uint32_t next_top = top + 2;
    if (against_face) {
      std::swap(top, next_top);
    }
    const std::uint32_t bottom = top + 1;
    const std::uint32_t next_bottom = next_top + 1;
    out.indices.insert(out.indices.end(), {top, next_top, bottom, next_top, next_bottom, bottom});
  }
}

}  // namespace

TerrainMesh BuildRoundedTerrainMesh(const formats::GndMap& gnd, const TerrainFaces& faces,
                                    std::span<const std::uint8_t> invisible_tiles,
                                    std::int32_t subdivisions) {
  const std::int32_t width = gnd.width();
  const std::int32_t height = gnd.height();
  // Le carreau de chaque cube une fois : le dessus et les deux parois qui le
  // bordent le relisent.
  std::vector<CubeRelief> reliefs;
  reliefs.reserve(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
  for (std::int32_t y = 0; y < height; ++y) {
    for (std::int32_t x = 0; x < width; ++x) {
      reliefs.push_back(CubeReliefOf(gnd, x, y, invisible_tiles));
    }
  }
  const auto relief_at = [&reliefs, width](std::int32_t x, std::int32_t y) -> const CubeRelief& {
    return reliefs[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                   static_cast<std::size_t>(x)];
  };

  std::map<std::int16_t, TerrainBatch> by_texture;
  for (std::int32_t y = 0; y < height; ++y) {
    for (std::int32_t x = 0; x < width; ++x) {
      const CubeRelief& relief = relief_at(x, y);
      if (const std::optional<TerrainFace> top = faces.Top(x, y); top.has_value()) {
        EmitTop(by_texture[top->texture_index], *top, relief, subdivisions);
      }
      // La paroi de droite longe le bord droit du cube et le bord gauche de son
      // voisin. Sa face va du fond vers l'avant, à rebours du cube.
      if (const std::optional<TerrainFace> wall = faces.EastWall(x, y); wall.has_value()) {
        const CubeRelief& neighbour = relief_at(x + 1, y);
        EmitWall(by_texture[wall->texture_index], *wall, subdivisions, true,
                 [&relief, &neighbour](bool on_neighbour, float along) {
                   return on_neighbour ? -ReliefHeightIn(neighbour, 0.0F, along)
                                       : -ReliefHeightIn(relief, 1.0F, along);
                 });
      }
      // Celle du fond longe le bord du fond du cube et le bord proche de son
      // voisin, de gauche à droite comme lui.
      if (const std::optional<TerrainFace> wall = faces.NorthWall(x, y); wall.has_value()) {
        const CubeRelief& neighbour = relief_at(x, y + 1);
        EmitWall(by_texture[wall->texture_index], *wall, subdivisions, false,
                 [&relief, &neighbour](bool on_neighbour, float across) {
                   return on_neighbour ? -ReliefHeightIn(neighbour, across, 0.0F)
                                       : -ReliefHeightIn(relief, across, 1.0F);
                 });
      }
    }
  }
  return AssembleTerrainMesh(by_texture);
}

}  // namespace ro::render
