#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
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
#include "ro/render/terrain_depth_chunks.h"
#include "ro/render/terrain_mesh.h"
#include "ro/render/world_frustum.h"

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

  // LE MÊME TÉLÉVERSEMENT, EN TRANCHES, pour qu'aucune image ne le paie en
  // entier : `BeginUpload` pose les textures et la lightmap et réserve les
  // tampons ; chaque `UploadSlice` y copie au plus `most_bytes` des grandes
  // textures (au-delà de kSlicedTextureBytes, par rangées), puis des sommets,
  // puis des indices, et rend vrai quand tout y est. Le compte de
  // téléversements passe au-delà de `later_than` : un renderer qui en remplace
  // un autre se dit changé à qui gardait le compte de l'autre.
  //
  // ⚠ IL NE SE DESSINE PAS ENTRE LES DEUX, et `mesh` comme `textures` doivent
  // vivre jusqu'à la dernière tranche.
  // Au-delà de 256 Kio, une planche de 256 × 256 texels : les planches du sol
  // partent entières avec `BeginUpload`.
  static constexpr std::size_t kSlicedTextureBytes = std::size_t{1} << 18;
  [[nodiscard]] core::Result<void> BeginUpload(const TerrainMesh& mesh,
                                               const std::vector<formats::Image>& textures,
                                               const formats::Image& lightmap,
                                               std::uint64_t later_than);
  [[nodiscard]] bool UploadSlice(std::size_t most_bytes);
  // ABANDONNE LE TÉLÉVERSEMENT EN COURS sans copier ce qui reste : `mesh` peut
  // mourir. Les tampons gardent une copie partielle ; seul un nouveau
  // `BeginUpload` rend le renderer dessinable.
  void CancelUpload() noexcept;

  // `points` sont les lumières ponctuelles que le sol reçoit, en plus de
  // celles de la carte.
  void Draw(const Camera& camera, const SceneLight& light, float aspect_ratio,
            const std::array<float, 4>& clear_color, const TerrainStages& stages = TerrainStages(),
            const SceneFog& fog = SceneFog(), const SunShadowView& sun = SunShadowView(),
            const PointLightSet& points = PointLightSet()) const;

  // LE SOL DANS LA CARTE D'OMBRE : sa seule profondeur, vue du soleil, sans
  // effacer. Les carreaux transparents n'y portent pas d'ombre. Avec `sight`,
  // seuls les blocs qu'il voit : voir `TerrainDepthChunks`.
  // Combien de fois le sol a été téléversé : ce qui dit qu'il a changé.
  [[nodiscard]] std::uint64_t uploads() const noexcept { return uploads_; }
  // LA TEXTURE DE RANG `rank` (celui de `textures` au téléversement), ou rien
  // au-delà : ce qu'on relit pour vérifier une copie par rangées.
  [[nodiscard]] const GlTexture* texture(std::size_t rank) const noexcept {
    return rank < textures_.size() ? &textures_[rank] : nullptr;
  }

  void DrawDepth(const glm::mat4& light_view_projection, const ShadowSight* sight = nullptr) const;

 private:
  void DrawSurfaces(const glm::mat4& view_projection, const SceneLight& light,
                    const TerrainStages& stages, const SceneFog& fog, const SunShadowView& sun,
                    const PointLightSet& points, const ShadowSight* sight = nullptr) const;
  // Lie la texture de la surface `surface` et pose son seuil de découpe.
  void BindSurface(std::size_t surface) const;
  // Décrit les sommets du tampon de sommets au tableau de sommets lié.
  void DescribeVertices() const;

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
  // LE SOL EN BLOCS, pour les cartes d'ombre ponctuelles : ses propres indices,
  // sur les mêmes sommets.
  std::vector<TerrainDepthChunk> depth_chunks_;
  unsigned int depth_vertex_array_ = 0;
  unsigned int depth_index_buffer_ = 0;
  std::uint64_t uploads_ = 0;
  // LE TÉLÉVERSEMENT EN TRANCHES en cours : le maillage, ses blocs, et ce qui
  // en est déjà copié, sommets puis indices puis indices des blocs.
  const TerrainMesh* staged_mesh_ = nullptr;
  std::shared_ptr<const TerrainDepthChunks> staged_chunks_;
  std::size_t staged_bytes_ = 0;
  // Les grandes textures réservées dont il reste des rangées à copier : leur
  // rang dans `textures_` et leur image ; la prochaine rangée de la première.
  struct StagedTexture {
    std::size_t rank = 0;
    const formats::Image* image = nullptr;
  };
  std::vector<StagedTexture> staged_textures_;
  int staged_row_ = 0;
};

}  // namespace ro::render
