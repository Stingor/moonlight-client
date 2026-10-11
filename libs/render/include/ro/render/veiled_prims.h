#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/model_mesh.h"
#include "ro/render/model_veil.h"

namespace ro::render {

// LA PASSE DES POSES VOILÉES, préparée comme le client prépare la sienne. Elle
// porte aussi les faces à texture mêlée des poses pleines : voir
// `BlendedTextures`.
//
// Il ne dessine pas une pose voilée d'un bloc : il émet, suite de faces par
// suite de faces, les triangles qui passent son tri — une face simple tournée
// vers l'œil, ou double —, puis range chaque suite dans sa file translucide
// selon l'INVERSE DE LA PROFONDEUR de son premier sommet émis, la plus lointaine
// d'abord. La file se dessine sans écrire la profondeur.
//
// ⚠ ÉCART : chez lui, cette file mêle les suites voilées aux sprites et aux
// effets translucides, triés ensemble. Ici elles ne se trient qu'entre elles, et
// passent après les sprites et les effets : voir la fiche 0084.

// UNE SUITE ÉMISE.
struct VeiledPrim {
  std::size_t model_index = 0;
  std::int32_t texture_index = 0;
  // Pour une pose animée, le rang de la grandeur de texture de son lot dans
  // celles de son groupe ; vide pour une pose figée, dont les coordonnées
  // portent déjà l'image zéro.
  std::optional<std::size_t> motion;
  std::optional<std::size_t> group;
  std::uint8_t opacity = kVeilOpaque;
  // L'inverse de la profondeur de son premier sommet émis.
  float sort_key = 0.0F;
  // Dans le tampon émis.
  std::uint32_t first_vertex = 0;
  std::uint32_t vertex_count = 0;
};

// Ajoute les suites d'une pose voilée : leurs sommets portés au monde — la
// normale par la seule pose, comme au dessin opaque — dans `vertices`.
// `node_poses` porte les matrices des nœuds d'une pose animée, et rien pour une
// figée.
//
// `only_textures`, une entrée par texture du modèle, restreint l'émission aux
// suites dont la texture y est marquée ; vide, toutes s'émettent.
void EmitVeiledPose(const VeiledPose& pose, const ModelInstances& model,
                    std::span<const glm::mat4> node_poses, const glm::mat4& view_projection,
                    std::vector<ModelVertex>& vertices, std::vector<VeiledPrim>& prims,
                    std::span<const std::uint8_t> only_textures = {});

// LES TEXTURES QUE LE JEU MÊLE AU FOND, une entrée par texture du modèle.
//
// Le jeu range une face du décor dans sa file translucide dès que sa texture
// est une TGA, même sur une pose pleine : elle s'y peint SOURCE ALPHA sur
// INVERSE DE LA SOURCE ALPHA, triée avec les poses voilées, et son alpha voile
// ce qui est derrière. Une BMP, elle, reste dans la file opaque et se découpe.
// Peinte opaque, une plaque de verre à texture TGA cacherait ce qu'elle doit
// laisser voir.
[[nodiscard]] std::vector<std::uint8_t> BlendedTextures(const ModelInstances& model);

// Les poses PLEINES d'un modèle dont des faces passent par la file translucide :
// toutes celles qui ne sont pas dans `veiled` — une pose voilée y passe déjà
// tout entière. Vide pour un modèle sans texture mêlée.
//
// ⚠ À CHEVAL SUR LA VUE, toutes : rien ne dit ici lesquelles la vue coupe, et
// écarter les faces entièrement hors de l'écran ne retire rien de visible.
[[nodiscard]] std::vector<VeiledPose> BlendedPoses(const ModelInstances& model,
                                                   std::size_t model_index,
                                                   std::span<const VeiledPose> veiled);

// La plus lointaine d'abord ; à égalité, dans l'ordre d'émission.
void SortVeiledPrims(std::vector<VeiledPrim>& prims);

}  // namespace ro::render
