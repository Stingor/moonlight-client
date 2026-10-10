#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"

namespace ro::formats {

// UN MODÈLE GRANNY 2 (.gr2), tel qu'un acteur du jeu s'en sert : son squelette,
// ses maillages, ses textures décodées et ses animations.
//
// Le fichier est l'image mémoire d'une arborescence d'objets typés, dont il
// porte lui-même la description. On n'en garde que ce que le client consomme :
// le PREMIER modèle, et les maillages dans l'ordre où CE MODÈLE les lie.
//
// Tout est dans le repère du fichier, où Z monte.

// Une transformée d'os : translation, rotation (quaternion x, y, z, w), puis
// échelle et cisaillement (matrice 3 × 3, rangée par rangée). Par défaut,
// l'identité.
struct GrannyTransform {
  std::uint32_t flags = 0;
  std::array<float, 3> position{0.0F, 0.0F, 0.0F};
  std::array<float, 4> orientation{0.0F, 0.0F, 0.0F, 1.0F};
  std::array<float, 9> scale_shear{1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F};
};

struct GrannyBone {
  std::string name;
  // Le rang du parent dans le squelette, ou -1 pour une racine.
  std::int32_t parent_index = -1;
  GrannyTransform local;
  // L'inverse de la pose de repos dans le monde, en colonnes.
  std::array<float, 16> inverse_world{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                                      0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
};

struct GrannyVertex {
  std::array<float, 3> position{0.0F, 0.0F, 0.0F};
  std::array<float, 3> normal{0.0F, 0.0F, 0.0F};
  std::array<float, 2> uv{0.0F, 0.0F};
  // Les os du sommet, en rangs dans `GrannyMesh::bone_names`, et leurs poids
  // sur un octet — 255 vaut l'unité.
  std::array<std::uint8_t, 4> bone_indices{0, 0, 0, 0};
  std::array<std::uint8_t, 4> bone_weights{255, 0, 0, 0};
};

struct GrannyMesh {
  std::string name;
  std::vector<GrannyVertex> vertices;
  // Trois sommets par triangle.
  std::vector<std::uint16_t> indices;
  // Les os que le maillage cite, par leur nom.
  std::vector<std::string> bone_names;
  // Le rang dans `GrannyScene::textures` de sa texture diffuse, ou -1.
  std::int32_t texture = -1;
  // UN MAILLAGE RIGIDE ne cite qu'un os, et le suit d'un bloc.
  bool rigid = false;
};

// Une texture décodée : RGBA8, rangée 0 en haut.
struct GrannyTextureImage {
  std::string file_name;
  std::int32_t width = 0;
  std::int32_t height = 0;
  // Vrai quand le fichier porte une couche alpha ; sinon, l'alpha vaut 255.
  bool has_alpha = false;
  std::vector<std::uint8_t> rgba;
};

// Une courbe : une B-spline de degré `degree`, de `knots.size()` nœuds, chacun
// portant `dimension` valeurs dans `controls`. Sans nœud, la courbe est vide.
struct GrannyCurve {
  std::int32_t degree = 0;
  std::int32_t dimension = 0;
  std::vector<float> knots;
  std::vector<float> controls;
};

struct GrannyTrack {
  std::string name;
  GrannyCurve position;
  GrannyCurve orientation;
  GrannyCurve scale_shear;
};

struct GrannyTrackGroup {
  std::string name;
  std::vector<GrannyTrack> tracks;
};

struct GrannyAnimation {
  std::string name;
  float duration_seconds = 0.0F;
  float time_step_seconds = 0.0F;
  std::vector<GrannyTrackGroup> track_groups;
};

struct GrannyScene {
  // Le nom du modèle : les groupes de pistes qui l'animent portent le même.
  std::string model_name;
  std::vector<GrannyBone> bones;
  std::vector<GrannyMesh> meshes;
  std::vector<GrannyTextureImage> textures;
  std::vector<GrannyAnimation> animations;
};

// Lit un fichier .gr2 entier. Un fichier sans modèle rend une scène sans os ni
// maillage : c'est la forme des fichiers qui ne portent qu'une animation.
[[nodiscard]] core::Result<GrannyScene> ReadGrannyScene(std::span<const std::uint8_t> bytes);

}  // namespace ro::formats
