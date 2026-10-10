#include "ro/render/water_plane.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "ro/render/terrain_mesh.h"

namespace ro::render {
namespace {

using formats::GndCube;
using formats::GndMap;
using formats::GndWaterZone;
using formats::RswScene;

// La version de GND à partir de laquelle l'eau vit dans le sol, et celle de RSW
// à partir de laquelle elle a quitté la scène. Entre les deux, les deux
// fichiers la portent.
constexpr std::int32_t kGndWaterVersion = 0x0108;
constexpr std::int32_t kRswWaterMovedVersion = 0x0206;

constexpr float kMillisecondsPerSecond = 1000.0F;
constexpr float kHalfTurnDegrees = 180.0F;
constexpr float kFullTurnDegrees = 360.0F;

// Un cube sans face du dessus n'est pas un trou d'eau : c'est un trou tout
// court, que le mappeur a creusé et que rien ne doit recouvrir.
constexpr std::int32_t kNoTile = -1;

std::string TwoDigits(int value) {
  std::string text = std::to_string(value);
  return text.size() < 2 ? "0" + text : text;
}

// Le nombre de tics écoulés.
//
// Fractionnaire et non entier : une seconde ne tombe pas sur un compte rond de
// tics, et arrondir ferait avancer la houle par paliers.
//
// ⚠ EN DOUBLE PRÉCISION : l'horloge reçue part du lancement du client, donc
// elle compte en dizaines de millions de millisecondes après quelques heures.
// Le repli sur une période se fait sur ce nombre-là, avant de revenir au
// flottant — c'est gratuit ici, et cela évite d'avoir à se demander à partir de
// quelle heure de jeu la mantisse ne suit plus.
[[nodiscard]] double TicksAt(std::uint32_t elapsed_ms) {
  return static_cast<double>(elapsed_ms) / static_cast<double>(kMillisecondsPerSecond) *
         static_cast<double>(kWaterTicksPerSecond);
}

}  // namespace

std::string WaterTexturePath(std::int32_t type, int frame) {
  // Le dossier est écrit en UTF-8 dans cette source, donc ce chemin passe par
  // `Vfs::Read`, qui le transcode — et JAMAIS par `ReadRaw`, qui le prendrait
  // pour du CP949 et ne trouverait rien.
  return "data\\texture\\워터\\water" + std::to_string(type) + TwoDigits(frame) + ".jpg";
}

std::vector<GndWaterZone> WaterZonesOf(const GndMap& gnd, const RswScene& rsw) {
  if (gnd.version() >= kGndWaterVersion && !gnd.water_zones().empty()) {
    return gnd.water_zones();
  }
  if (rsw.water().present && rsw.header().version() < kRswWaterMovedVersion) {
    const formats::RswWater& water = rsw.water();
    return {GndWaterZone{
        .level = water.level,
        .type = water.type,
        .wave_height = water.wave_height,
        .wave_speed = water.wave_speed,
        .wave_pitch = water.wave_pitch,
        .texture_cycling = water.texture_cycling,
    }};
  }
  return {};
}

WaterPlane BuildWaterPlane(const GndMap& gnd, const RswScene& rsw,
                           std::span<const std::uint8_t> invisible_tiles) {
  WaterPlane plane;
  const std::vector<GndWaterZone> zones = WaterZonesOf(gnd, rsw);
  if (zones.empty() || gnd.width() <= 0 || gnd.height() <= 0) {
    return plane;
  }

  const std::int32_t width = gnd.width();
  const std::int32_t height = gnd.height();
  const float depth = static_cast<float>(height) * kWorldUnitsPerCube;

  // La découpe en zones est celle du GND — et elle ne vaut QUE si c'est lui qui
  // a donné les zones. Une nappe venue du RSW n'en a qu'une, qui couvre tout ;
  // lui appliquer une découpe 3 × 3 lue ailleurs chercherait neuf zones dans
  // une liste qui n'en a qu'une.
  const bool zoned = zones.size() > 1;
  const std::int32_t split_x = zoned ? std::max(1, gnd.water_split_width()) : 1;
  const std::int32_t split_y = zoned ? std::max(1, gnd.water_split_height()) : 1;
  // Division entière, comme le client : le reste est absorbé par la dernière
  // zone grâce au plafonnement ci-dessous.
  const std::int32_t cubes_per_zone_x = std::max(1, width / split_x);
  const std::int32_t cubes_per_zone_y = std::max(1, height / split_y);

  // ⚠ LA RANGÉE DE ZONE NE S'INVERSE PAS, alors que le dessin inverse l'axe Z.
  // Le client divise l'indice de cube TEL QUEL, et c'est aussi
  // ce que fait `WaterField::LevelAt` : les deux inversions se compensent.
  const auto zone_of = [&](std::int32_t x, std::int32_t y) -> std::size_t {
    const std::int32_t zone_x = std::clamp(x / cubes_per_zone_x, 0, split_x - 1);
    const std::int32_t zone_y = std::clamp(y / cubes_per_zone_y, 0, split_y - 1);
    const auto index = static_cast<std::size_t>(zone_y) * static_cast<std::size_t>(split_x) +
                       static_cast<std::size_t>(zone_x);
    return std::min(index, zones.size() - 1);
  };

  // Un carreau que sa texture rend tout transparent est un TROU : le terrain ne
  // l'émet pas, et l'eau ne le recouvre pas davantage. Voir l'en-tête.
  const auto invisible = [invisible_tiles](std::int32_t index) {
    const auto tile = static_cast<std::size_t>(index);
    return index >= 0 && tile < invisible_tiles.size() && invisible_tiles[tile] != 0;
  };

  // Les sommets sont rassemblés par zone : chacune a sa texture et sa houle,
  // donc son propre appel de dessin.
  std::vector<std::vector<WaterVertex>> by_zone(zones.size());

  for (std::int32_t y = 0; y < height; ++y) {
    for (std::int32_t x = 0; x < width; ++x) {
      const GndCube& cube = gnd.at(x, y);
      if (cube.tile_up == kNoTile || invisible(cube.tile_up)) {
        continue;
      }
      const std::size_t zone_index = zone_of(x, y);
      const GndWaterZone& zone = zones[zone_index];

      // Le CREUX de la houle : sous lui, l'eau ne descend jamais. Un cube dont
      // les quatre coins sont au-dessus ne sera couvert à aucun instant.
      //
      // ⚠ LES HAUTEURS DU FICHIER COMPTENT VERS LE BAS : un coin est au-dessus
      // du creux quand son nombre lui est INFÉRIEUR. Le sens se retourne ici et
      // nulle part ailleurs.
      const float trough = zone.level - zone.wave_height;
      const bool all_above =
          std::ranges::none_of(cube.corner_height, [trough](float h) { return h > trough; });
      if (all_above) {
        continue;
      }

      const float left = static_cast<float>(x) * kWorldUnitsPerCube;
      const float right = left + kWorldUnitsPerCube;
      const float z_back = depth - static_cast<float>(y) * kWorldUnitsPerCube;
      const float z_front = z_back + kWorldUnitsPerCube;

      // La texture pave par carrés de quatre cubes. Le bord droit prend
      // `(x & 3) + 1` et NON `(x + 1) & 3` : c'est ce qui fait que le dernier
      // cube d'un carré va jusqu'à un, et que le premier du suivant repart de
      // zéro sans couture.
      const float u_low = static_cast<float>(x % kWaterTileCubes) / kWaterTileCubes;
      const float u_high = u_low + 1.0F / kWaterTileCubes;
      const float v_low = static_cast<float>(y % kWaterTileCubes) / kWaterTileCubes;
      const float v_high = v_low + 1.0F / kWaterTileCubes;

      // LA DIAGONALE DE CHAQUE COIN, en indices de cube. Le bord AVANT est une
      // rangée plus près de l'observateur, donc une rangée de MOINS : l'axe Z
      // du monde remonte les rangées du fichier.
      const auto step = [](std::int32_t cube_x, std::int32_t cube_y) {
        return static_cast<float>(cube_x + cube_y);
      };
      // L'ALTITUDE AU REPOS, houle éteinte. Négative, comme tout le terrain :
      // le fichier compte vers le bas, la scène vers le haut. La houle s'y
      // ajoute au dessin, où la phase change à chaque image.
      const float rest = -zone.level;
      const WaterVertex back_left{{left, rest, z_back}, {u_low, v_high}, step(x, y)};
      const WaterVertex back_right{{right, rest, z_back}, {u_high, v_high}, step(x + 1, y)};
      const WaterVertex front_left{{left, rest, z_front}, {u_low, v_low}, step(x, y - 1)};
      const WaterVertex front_right{{right, rest, z_front}, {u_high, v_low}, step(x + 1, y - 1)};

      std::vector<WaterVertex>& out = by_zone[zone_index];
      out.push_back(back_left);
      out.push_back(back_right);
      out.push_back(front_left);
      out.push_back(back_right);
      out.push_back(front_right);
      out.push_back(front_left);
    }
  }

  for (std::size_t index = 0; index < zones.size(); ++index) {
    const std::vector<WaterVertex>& vertices = by_zone[index];
    if (vertices.empty()) {
      continue;
    }
    plane.zones.push_back(WaterZoneMesh{
        .settings = zones[index],
        .first_vertex = static_cast<std::uint32_t>(plane.vertices.size()),
        .vertex_count = static_cast<std::uint32_t>(vertices.size()),
    });
    plane.vertices.insert(plane.vertices.end(), vertices.begin(), vertices.end());
  }
  return plane;
}

int WaterFrameAt(const GndWaterZone& zone, std::uint32_t elapsed_ms) {
  // Une cadence nulle ou négative figerait l'animation sur une division par
  // zéro : le client compte au moins un tic par image.
  const auto cycling = static_cast<double>(std::max(1, zone.texture_cycling));
  const double frames = TicksAt(elapsed_ms) / cycling;
  const auto index = static_cast<int>(std::fmod(frames, static_cast<double>(kWaterFrameCount)));
  return std::clamp(index, 0, kWaterFrameCount - 1);
}

float WaterPhaseAt(const GndWaterZone& zone, std::uint32_t elapsed_ms) {
  const double turned = static_cast<double>(zone.wave_speed) * TicksAt(elapsed_ms);
  const double folded = std::fmod(turned, static_cast<double>(kFullTurnDegrees));
  // Le client garde sa phase dans ]-180, 180] en lui retranchant un tour dès
  // qu'elle dépasse. Le repli donne le même cosinus, et laisse le nombre petit.
  return static_cast<float>(folded > static_cast<double>(kHalfTurnDegrees)
                                ? folded - static_cast<double>(kFullTurnDegrees)
                                : folded);
}

}  // namespace ro::render
