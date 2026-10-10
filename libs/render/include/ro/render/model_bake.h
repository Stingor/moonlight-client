#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/model_mesh.h"

namespace ro::render {

// LES POSES FIGÉES D'UN MODÈLE, CUITES EN UN SEUL MAILLAGE DU MONDE.
//
// 🔴 C'EST LE COÛT DOMINANT D'UNE VILLE : dessiner chaque pose surface par
// surface demande un appel de dessin, une texture liée et une matrice posée
// par (pose, surface) — des milliers par image à Prontera. Une fois cuites,
// toutes les poses d'un modèle se dessinent en une suite par texture.
//
// ⚠ LA CUISSON REFAIT EXACTEMENT CE QUE FAIT LE NUANCIER : la position passe
// par la matrice entière, la normale par sa partie 3×3 SANS translation, et
// c'est le nuancier qui la renormalise. Toute autre transformation de la
// normale changerait l'éclairage d'un bâtiment tourné ou étiré.

// Une suite de sommets cuits qui partagent une texture.
struct BakedRun {
  // Index dans ModelMesh::textures ; négatif ou hors table, la texture de repli.
  std::int32_t texture_index = 0;
  std::uint32_t first_vertex = 0;
  std::uint32_t vertex_count = 0;
};

// LE BLOC D'UNE SURFACE : ses sommets, pose après pose, chaque pose en occupant
// autant — la pose i commence à `first_vertex + i × vertices_per_pose`. C'est ce
// qui permet d'en sauter une au dessin.
struct BakedSurface {
  std::int32_t texture_index = 0;
  std::uint32_t first_vertex = 0;
  std::uint32_t vertices_per_pose = 0;
};

struct BakedModel {
  std::vector<ModelVertex> vertices;
  // UN PAR SOMMET : le côté que sa face montre dans SA pose — voir `FaceSide`.
  // Une pose en miroir retourne ses faces simples, et une fois les poses
  // cuites ensemble, c'est le sommet qui doit le porter.
  std::vector<FaceSide> sides;
  std::vector<BakedRun> runs;
  std::vector<BakedSurface> surfaces;
};

// Cuit chaque pose de `transforms`. Les surfaces se regroupent par texture —
// l'ordre de dessin d'un opaque ne compte pas — et les suites contiguës d'une
// même texture fusionnent. Une surface qui déborde du maillage est ignorée.
//
// `rsm_version` décide du côté des faces simples, voir `ShowsCounterClockwise`.
// Une face que le maillage ne dit pas simple se tient pour double.
[[nodiscard]] BakedModel BakeInstances(const ModelMesh& mesh, std::span<const glm::mat4> transforms,
                                       std::int32_t rsm_version);

}  // namespace ro::render
