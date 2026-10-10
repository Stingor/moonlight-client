#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/rsm.h"

namespace ro::render {

// La pose des nœuds d'un modèle de la SECONDE famille (2.2 et au-delà) à une
// image de son animation, comme le client natif la calcule.
//
// Contrairement à la première famille, un nœud y anime son échelle, sa rotation
// ET sa position, l'image est FLOTTANTE, et chaque texture peut faire glisser,
// étirer ou tourner ses coordonnées.
//
// ⚠ LES SOMMETS DU FICHIER SONT DANS LE REPÈRE DE LEUR NŒUD. Au repos, le natif
// les pose par la matrice du nœud telle que le fichier la donne — la hiérarchie
// ne fait que s'y ramener. Les prendre tels quels laissait au pied de son tronc
// le houppier d'un arbre de wolfvill, décalé de 57 unités dans son fichier.

struct SecondFamilyNode {
  std::size_t mesh_index = 0;
  // Index dans la même liste, toujours ANTÉRIEUR, ou -1 pour une racine.
  std::int32_t parent = -1;
  // La pose au repos RELATIVE au parent : la matrice du fichier du nœud, suivie
  // de l'inverse de celle de son parent.
  // Elle tient lieu de rotation faute d'image-clé de rotation, et de position
  // faute d'image-clé de position.
  glm::mat4 rest{1.0F};
  std::vector<formats::RsmScaleKey> scale_keys;
  std::vector<formats::RsmRotationKey> rotation_keys;
  std::vector<formats::RsmPositionKey> position_keys;
  std::vector<formats::RsmTextureAnimation> texture_animations;
};

// Un nœud par placement, dans l'ordre de PlaceNodes.
[[nodiscard]] std::vector<SecondFamilyNode> SecondFamilyNodes(const formats::RsmModel& model);

// La matrice d'un nœud tel que le fichier la range — sa 3 × 3 suivie de son
// décalage —, en convention colonne.
[[nodiscard]] glm::mat4 FileNodeMatrix(const formats::RsmMesh& mesh);

// La transformation propre d'un nœud à l'image `frame` : échelle, puis
// rotation, puis position.
[[nodiscard]] glm::mat4 SecondFamilyNodePose(const SecondFamilyNode& node, float frame);

// La matrice de chaque nœud, parents composés, indexée comme `nodes`.
[[nodiscard]] std::vector<glm::mat4> PoseSecondFamily(std::span<const SecondFamilyNode> nodes,
                                                      float frame);

// Un nœud bouge-t-il ? Il lui faut deux images-clés d'échelle, de rotation ou
// de position, ou deux d'une grandeur de texture.
[[nodiscard]] bool SecondFamilyMoves(std::span<const SecondFamilyNode> nodes);

// Les cinq grandeurs d'une texture à une image. Numérotation du fichier : 0 et
// 1 décalent, 2 et 3 étirent, 4 tourne.
struct TextureMotion {
  float offset_u = 0.0F;
  float offset_v = 0.0F;
  float scale_u = 1.0F;
  float scale_v = 1.0F;
  // En radians.
  float angle = 0.0F;
};

// Les grandeurs de la texture `texture_index` d'un nœud à l'image `frame` :
// interpolées entre deux images-clés, figées sur la dernière.
[[nodiscard]] TextureMotion TextureMotionAt(
    std::span<const formats::RsmTextureAnimation> animations, std::int32_t texture_index,
    float frame);

// Applique ces grandeurs à une coordonnée, comme le dessin du natif :
// rotation autour du centre de la texture, PUIS étirement, PUIS décalage.
[[nodiscard]] std::array<float, 2> MoveTextureCoordinate(const TextureMotion& motion,
                                                         const std::array<float, 2>& uv);

// ---------------------------------------------------------------------------
// L'horloge d'un objet de la seconde famille : elle compte en MILLISECONDES,
// et non en pas logiques comme la première.

// La durée d'une boucle : 1000 × longueur / cadence, en simple précision.
// Infinie pour une cadence nulle.
[[nodiscard]] double SecondFamilyDurationMs(std::int32_t animation_length, float frame_rate);

// Le temps joué `elapsed_ms` après le départ, et s'il faut repartir.
struct SecondFamilyPlayed {
  std::uint32_t played_ms = 0;
  // En boucle, la fin atteinte : le départ se reporte à MAINTENANT, et le
  // dépassement est perdu.
  bool restart = false;
};
[[nodiscard]] SecondFamilyPlayed SecondFamilyPlayedAt(std::uint32_t elapsed_ms,
                                                      std::int32_t animation_type,
                                                      float animation_speed, double duration_ms);

// L'image atteinte après `played_ms` : cadence × temps / 1000.
[[nodiscard]] float SecondFamilyFrameAt(std::uint32_t played_ms, float frame_rate);

}  // namespace ro::render
