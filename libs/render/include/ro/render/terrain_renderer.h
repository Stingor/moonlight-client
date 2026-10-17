#pragma once

#include <array>
#include <optional>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/bmp.h"
#include "ro/render/camera.h"
#include "ro/render/gl_program.h"
#include "ro/render/gl_texture.h"
#include "ro/render/point_lights.h"
#include "ro/render/scene_fog.h"
#include "ro/render/scene_light.h"
#include "ro/render/sun_shadow_map.h"
#include "ro/render/terrain_mesh.h"

namespace ro::render {

// Étages d'éclairage du sol, que l'on peut couper un par un.
//
// Le sol est le produit de plusieurs facteurs indépendants : l'éclairage
// calculé par sommet, la couleur du carreau, l'ombre cuite dans la lightmap, et
// la lumière cuite qui s'y ajoute. Quand un rendu sort trop sombre, la moyenne
// de l'image ne dit pas LEQUEL est en cause.
//
// Les couper un par un l'attribue à un étage, et c'est la seule façon de
// comparer avec un client dont on n'a que des captures. L'implémentation de
// référence expose les mêmes interrupteurs, pour la même raison.
struct TerrainStages {
  // Ambiante et diffuse, calculées par sommet.
  bool vertex_light = true;
  // Alpha de la lightmap : l'ombre, multiplicative.
  bool shadowmap = true;
  // RGB de la lightmap : la lumière posée, additive.
  bool lightmap = true;
  // LA COULEUR DE CARREAU, qui module la texture sommet par sommet.
  //
  // ⚠ UN ÉTAGE À PART, ET `/lightmap` NE LE TOUCHE PAS : le client ne saute
  // que sa passe de lightmap, et la couleur de carreau vit dans la passe de
  // base, avec l'éclairage du sommet. Niflheim garde ainsi ses carreaux
  // violets et noirs quand les ombres sont éteintes.
  bool tile_color = true;
  // LA LUMIÈRE FROIDE : le rouge et le bleu de la lumière posée échangés. Le
  // jaune d'une lanterne sort cyan, et la carte vire au bleu. Un GOÛT de joueur,
  // pas un étage : éteinte, la lumière est celle que la carte a peinte.
  bool cold_light = false;

  // LE SOL UNI, et ce n'est PAS un étage de plus : c'est une SUBSTITUTION.
  //
  // Posée, cette couleur devient celle du sol telle quelle — sans texture, sans
  // éclairage par sommet, sans couleur de carreau, sans ombre ni lumière cuite.
  // Les trois interrupteurs ci-dessus ne la regardent même pas.
  //
  // 🔴 C'EST CE QUI EN FAIT UN FOND DE CAPTURE. Une couleur seulement MODULÉE
  // par l'éclairage sortirait dégradée, et ne servirait plus à détourer quoi que
  // ce soit. L'implémentation de référence remplace pour la même raison
  // (`D3DTOP_SELECTARG1` sur la texture ET sur la lightmap).
  //
  // ⚠ ELLE NE DÉCIDE PAS DE LA DÉCOUPE. Un carreau que sa texture rend
  // transparent le reste : le sol uni ne bouche pas les trous d'une carte, parce
  // que la référence ne touche pas non plus à l'étage alpha.
  //
  // Trois composantes, et pas quatre : la passe du sol est opaque.
  std::optional<std::array<float, 3>> flat_color;
};

// Dessine un TerrainMesh.
//
// Il reçoit un maillage déjà construit et des images déjà décodées : il ne lit
// aucun fichier et ne connaît ni le VFS ni les formats de carte. C'est ce qui
// permet de le nourrir d'une scène fabriquée à la main dans un test.
class TerrainRenderer {
 public:
  TerrainRenderer() = default;
  TerrainRenderer(const TerrainRenderer&) = delete;
  TerrainRenderer& operator=(const TerrainRenderer&) = delete;
  TerrainRenderer(TerrainRenderer&& other) noexcept;
  TerrainRenderer& operator=(TerrainRenderer&& other) noexcept;
  ~TerrainRenderer();

  [[nodiscard]] static core::Result<TerrainRenderer> Create();

  // `textures` est indexé comme GndMap::texture_paths(). Une image vide — parce
  // que le fichier manquait ou n'a pas pu être décodé — reçoit la texture de
  // remplacement, et le reste de la carte s'affiche quand même.
  //
  // `lightmap` peut être vide : le terrain sort alors sans ses ombres cuites,
  // ce qui reste préférable à un refus de rendre.
  [[nodiscard]] core::Result<void> Upload(const TerrainMesh& mesh,
                                          const std::vector<formats::Image>& textures,
                                          const formats::Image& lightmap);

  // `points` sont les lumières ponctuelles que le sol reçoit, en plus de
  // celles de la carte.
  void Draw(const Camera& camera, const SceneLight& light, float aspect_ratio,
            const std::array<float, 4>& clear_color, const TerrainStages& stages = TerrainStages(),
            const SceneFog& fog = SceneFog(), const SunShadowView& sun = SunShadowView(),
            const PointLightSet& points = PointLightSet()) const;

  // LE SOL DANS LA CARTE D'OMBRE : sa seule profondeur, vue du soleil, sans
  // effacer. Les carreaux transparents n'y portent pas d'ombre.
  void DrawDepth(const glm::mat4& light_view_projection) const;

 private:
  void DrawSurfaces(const glm::mat4& view_projection, const SceneLight& light,
                    const TerrainStages& stages, const SceneFog& fog, const SunShadowView& sun,
                    const PointLightSet& points) const;

  GlProgram program_;
  std::vector<GlTexture> textures_;
  // LE SEUIL DE DÉCOUPE de chaque texture, dans l'ordre de `textures_` : il
  // dépend de son fichier — voir `SceneCutoutOf`.
  std::vector<float> alpha_references_;
  GlTexture placeholder_;
  GlTexture lightmap_;
  std::vector<TerrainSurface> surfaces_;
  bool has_lightmap_ = false;
  // Le maillage téléversé a des indices : le sol arrondi. Voir
  // `TerrainMesh::indices`.
  bool indexed_ = false;
  unsigned int vertex_array_ = 0;
  unsigned int vertex_buffer_ = 0;
  unsigned int index_buffer_ = 0;
};

}  // namespace ro::render
