#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/rsm.h"
#include "ro/formats/rsw.h"

namespace ro::render {

// La pose des nœuds d'un modèle du décor à une image de son animation.
//
// Données pures, sans OpenGL : c'est ici que se fixent les règles du client
// natif, et elles ne se voient autrement qu'en comparant deux images.
//
// ⚠ Convention : glm multiplie en vecteur-COLONNE, le natif en vecteur-LIGNE.
// Le produit natif « échelle · rotation · position » s'écrit donc ici
// « position * rotation * échelle » ; les deux appliquent l'échelle d'abord.

// Un nœud tel que la hiérarchie le PLACE : son maillage et son parent.
//
// Un maillage peut être placé plusieurs fois — sous chaque nœud qui porte le nom
// de son parent — et un placement ne se confond donc pas avec un maillage.
struct NodePlacement {
  // Index dans RsmModel::meshes().
  std::size_t mesh_index = 0;
  // Index dans la même liste, toujours ANTÉRIEUR, ou -1 pour une racine.
  std::int32_t parent = -1;
};

// Parcourt l'arbre depuis ses racines ; un parent précède ses enfants.
//
// Une racine est un nœud sans parent, ou son propre parent. Un nœud dont le
// parent n'existe pas est traité en racine plutôt que de disparaître du décor.
[[nodiscard]] std::vector<NodePlacement> PlaceNodes(const formats::RsmModel& model);

// Ce qu'il faut d'un nœud de la PREMIÈRE famille (avant 2.2) pour le poser.
//
// Le natif n'en anime que la rotation : la position et l'échelle sont celles du
// repos, et les images-clés d'échelle du fichier ne sont jamais lues — le nœud
// ne recopie que celles de rotation et de position, et la première famille n'a
// pas de position animée.
struct FirstFamilyNode {
  std::size_t mesh_index = 0;
  std::int32_t parent = -1;
  glm::vec3 position{0.0F};
  glm::vec3 scale{1.0F};
  // Rotation au repos, en quaternion (x, y, z, w). Elle ne sert que faute
  // d'images-clés : le natif en fait alors une image-clé unique.
  std::array<float, 4> rest_rotation{0.0F, 0.0F, 0.0F, 1.0F};
  std::vector<formats::RsmRotationKey> rotation_keys;
};

// Un nœud par placement, dans l'ordre de PlaceNodes.
[[nodiscard]] std::vector<FirstFamilyNode> FirstFamilyNodes(const formats::RsmModel& model);

// Le quaternion d'un angle autour d'un axe, comme le natif le calcule : l'axe
// n'est PAS normalisé et aucun angle n'est jugé négligeable.
[[nodiscard]] std::array<float, 4> AxisAngleQuaternion(const std::array<float, 3>& axis,
                                                       float angle_radians);

// La matrice d'un quaternion (x, y, z, w), sans le normaliser.
[[nodiscard]] glm::mat4 QuaternionMatrix(const std::array<float, 4>& quaternion);

// L'interpolation sphérique du natif : chemin le plus court, et
// interpolation linéaire quand les deux quaternions sont presque confondus.
[[nodiscard]] std::array<float, 4> SlerpQuaternion(const std::array<float, 4>& from,
                                                   const std::array<float, 4>& to, float amount);

// Rotation d'un nœud à l'image `frame`.
//
// ⚠ AU-DELÀ DE SA DERNIÈRE IMAGE-CLÉ, un nœud qui en a deux ou plus ne tourne
// PLUS DU TOUT : le natif rend alors l'identité, pas la dernière rotation.
[[nodiscard]] glm::mat4 FirstFamilyRotationAt(const FirstFamilyNode& node, std::int32_t frame);

// La matrice de chaque nœud à l'image `frame`, ses parents composés, indexée
// comme `nodes` : la pose du nœud à cette image, puis le produit du dessin.
[[nodiscard]] std::vector<glm::mat4> PoseFirstFamily(std::span<const FirstFamilyNode> nodes,
                                                     std::int32_t frame);

// Un nœud de la première famille bouge-t-il ? Il lui faut deux images-clés.
[[nodiscard]] bool FirstFamilyMoves(std::span<const FirstFamilyNode> nodes);

// ---------------------------------------------------------------------------
// L'horloge d'un objet de la première famille.

// Le pas de la boucle logique du natif : 1000/60 en division ENTIÈRE, soit
// 16 ms. Un objet RSM1 avance d'un cran par pas.
inline constexpr std::uint32_t kModelStepMs = 16;

// Les deux façons de jouer que le natif connaît ; toute autre valeur fige,
// comme `kModelStill`.
inline constexpr std::int32_t kModelStill = formats::kRswModelStill;
inline constexpr std::int32_t kModelPlayOnce = formats::kRswModelPlayOnce;
inline constexpr std::int32_t kModelPlayLooped = formats::kRswModelPlayLooped;
// La plus grande vitesse de jeu qu'un .rsw garde.
inline constexpr float kModelMaxAnimationSpeed = formats::kRswMaxAnimationSpeed;

// L'image d'après, un pas logique plus tard.
//
// Le cran vaut `vitesse / 3 × 100` images, TRONQUÉ après l'addition — 33 pour
// une vitesse de 1. En boucle, l'image repart de ZÉRO dès qu'elle atteint la
// longueur (le reste est perdu) ; jouée une fois, elle s'arrête sur la
// dernière. Sous une vitesse de 0,03 le cran est nul : l'objet ne bouge jamais.
[[nodiscard]] std::int32_t NextFirstFamilyFrame(std::int32_t frame, std::int32_t animation_type,
                                                float animation_speed,
                                                std::int32_t animation_length);

// Le cran reste-t-il nul, depuis l'image zéro ? L'objet est alors figé à jamais.
[[nodiscard]] bool FirstFamilyStaysStill(std::int32_t animation_type, float animation_speed,
                                         std::int32_t animation_length);

}  // namespace ro::render
