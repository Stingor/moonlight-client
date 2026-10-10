#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/gr2_model.h"

namespace ro::render {

// LA POSE D'UN ACTEUR EN MODÈLE, à un instant de son animation.
//
// Trois étages, dans l'ordre du client :
//   1. chaque os prend la transformée que sa piste donne à cet instant — sa pose
//      de repos s'il n'en a pas ;
//   2. les os se composent de la racine vers les feuilles, et chacun multiplie
//      l'inverse de sa pose de repos : c'est la matrice COMPOSITE, qui déplace
//      un sommet de sa place de repos à sa place animée ;
//   3. les sommets d'un maillage déformable se mélangent entre leurs os selon
//      leurs poids ; ceux d'un maillage rigide restent tels quels, et le
//      maillage entier suit la composite de son premier os.
//
// ⚠ UNE COMPOSANTE SANS COURBE VAUT SA VALEUR NEUTRE, pas celle du repos : une
// piste qui n'anime que la position laisse l'os sans rotation.
//
// Tout se calcule dans le repère du FICHIER, où Z monte.

// La valeur d'une courbe à `seconds`, ou rien quand elle n'a pas de nœud. Un
// quaternion sort renormalisé.
[[nodiscard]] std::optional<std::vector<float>> EvaluateGrannyCurve(
    const formats::GrannyCurve& curve, float seconds);

// L'animation liée à un squelette : pour chaque os, la piste qui l'anime.
struct ActorModelAnimation {
  const formats::GrannyAnimation* animation = nullptr;
  std::vector<const formats::GrannyTrack*> track_of_bone;
};

// Lie une animation au modèle : le groupe de pistes qui porte le NOM du
// modèle, puis chaque piste à l'os de même nom. Rien quand aucun groupe ne
// vise ce modèle — l'animation ne le concerne pas.
[[nodiscard]] std::optional<ActorModelAnimation> BindActorModelAnimation(
    const formats::GrannyScene& model, const formats::GrannyAnimation& animation);

// La pose locale de chaque os ; celle de repos sans animation.
[[nodiscard]] std::vector<formats::GrannyTransform> SampleLocalPose(
    const formats::GrannyScene& model, const ActorModelAnimation* animation, float local_seconds);

// La matrice d'une transformée : translation × rotation × échelle, pour des
// vecteurs colonnes.
[[nodiscard]] glm::mat4 TransformMatrix(const formats::GrannyTransform& transform);

// Les matrices composites de chaque os.
[[nodiscard]] std::vector<glm::mat4> CompositeMatrices(
    const formats::GrannyScene& model, std::span<const formats::GrannyTransform> local_pose);

// Pour chaque os qu'un maillage cite, son rang dans le squelette, ou -1.
[[nodiscard]] std::vector<std::int32_t> MeshBoneMap(const formats::GrannyScene& model,
                                                    const formats::GrannyMesh& mesh);

struct PosedVertex {
  glm::vec3 position{0.0F};
  glm::vec3 normal{0.0F};
  glm::vec2 uv{0.0F};
};

struct PosedMesh {
  std::vector<PosedVertex> vertices;
  // La matrice qui porte le maillage entier : l'identité pour un maillage
  // déformable, la composite de son premier os pour un maillage rigide.
  glm::mat4 mesh_matrix{1.0F};
};

// Déforme un maillage à la pose donnée.
//
// ⚠ LES NORMALES D'UN MAILLAGE RIGIDE RESTENT CELLES DU REPOS : c'est la
// matrice du maillage qui le porte, et elle ne s'applique qu'aux positions.
[[nodiscard]] PosedMesh PoseMesh(const formats::GrannyMesh& mesh,
                                 std::span<const std::int32_t> bone_map,
                                 std::span<const glm::mat4> composites);

// LE TEMPS LOCAL D'UNE ANIMATION jouée depuis `clock_seconds` : il boucle, ou
// s'arrête sur sa dernière image quand elle ne se joue qu'une fois.
[[nodiscard]] float AnimationLocalSeconds(float clock_seconds, float duration_seconds,
                                          bool play_once);

}  // namespace ro::render
