#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "ro/core/error.h"
#include "ro/render/camera.h"
#include "ro/render/gl_program.h"
#include "ro/render/gl_texture.h"
#include "ro/render/model_bake.h"
#include "ro/render/model_clock.h"
#include "ro/render/model_mesh.h"
#include "ro/render/model_mesh_identity.h"
#include "ro/render/model_pose_boxes.h"
#include "ro/render/model_veil.h"
#include "ro/render/point_lights.h"
#include "ro/render/scene_fog.h"
#include "ro/render/scene_light.h"
#include "ro/render/sun_shadow_map.h"
#include "ro/render/world_frustum.h"

namespace ro::render {

// Les étages du dessin des modèles, que l'on peut couper pour mesurer.
struct ModelStages {
  // L'ÉCLAIRAGE. Coupé, la texture sort telle qu'elle est dans le fichier. C'est
  // la référence à laquelle on compare une capture du client pour savoir ce
  // qu'il applique, lui, au même endroit — sans quoi on ne mesure qu'un rendu
  // contre un autre.
  bool light = true;
  // LE TRI DES FACES SIMPLES : une face que le fichier ne déclare pas double ne
  // se dessine que du côté que sa pose montre, voir `ShowsCounterClockwise`.
  // Coupé, tout se dessine des deux côtés : c'est ce qu'il retire qui se mesure.
  bool one_sided = true;
  // LES FACES À TEXTURE MÊLÉE LAISSÉES À LA PASSE TRANSLUCIDE : la passe opaque
  // les saute, et c'est `DrawVeiled` qui les peint, triées et voilant ce qui est
  // derrière. ⚠ UN APPELANT QUI LE POSE DOIT APPELER `DrawVeiled`, sans quoi ces
  // faces disparaissent ; un autre les garde opaques.
  bool blended_apart = false;
};

// Dessine les modèles du décor.
//
// Séparé du renderer de terrain : leurs géométries, leurs textures et leur
// éclairage n'ont rien de commun, et les mêler obligerait chacun à porter les
// cas de l'autre.
// LE FILTRAGE DES TEXTURES DU DÉCOR.
//
// ⚠ ÉTEINT PAR DÉFAUT, comme le client d'origine (fiche 0008) : aucune texture
// n'a de mipmap, et un décor lointain scintille. Allumé, les niveaux réduits se
// génèrent à la demande et le filtrage passe en trilinéaire — anisotrope si on
// le demande, et si la carte graphique le sait.
struct ModelTextureFilter {
  bool mipmaps = false;
  // Les échantillons pris le long d'une face vue de biais. Un : aucun.
  int anisotropy = 1;
};

class ModelRenderer {
 public:
  ModelRenderer() = default;
  ModelRenderer(const ModelRenderer&) = delete;
  ModelRenderer& operator=(const ModelRenderer&) = delete;
  ModelRenderer(ModelRenderer&& other) noexcept;
  ModelRenderer& operator=(ModelRenderer&& other) noexcept;
  ~ModelRenderer();

  [[nodiscard]] static core::Result<ModelRenderer> Create();

  // Remplace les modèles téléversés par ceux-ci.
  //
  // 🔴 UN MODÈLE QUI N'A PAS CHANGÉ GARDE SES TAMPONS : même fichier, même
  // variante, mêmes sommets déplacés et mêmes images, il a le même maillage ; et
  // mêmes poses figées, la même cuisson. Retéléverser un décor dont une seule
  // pose bouge ne recuit donc que son lot.
  //
  // 🔴 LES TEXTURES SE PARTAGENT PAR NOM, ET SURVIVENT D'UN TÉLÉVERSEMENT À
  // L'AUTRE : un nom désigne toujours le même fichier de l'archive, donc la même
  // image. Une texture que plusieurs modèles citent ne part qu'une fois au
  // pilote, et une texture déjà là ne repart pas — les unités au sol se
  // retéléversent à chaque piège posé ou retiré, et renvoyaient toutes leurs
  // textures à chaque fois. Celles que le nouveau téléversement ne cite plus se
  // rendent au pilote.
  [[nodiscard]] core::Result<void> Upload(const std::vector<ModelInstances>& models);

  // Combien de textures le dernier téléversement a dû envoyer au pilote : celles
  // qu'aucun précédent n'avait déjà envoyées.
  [[nodiscard]] std::size_t uploaded_textures() const noexcept { return uploaded_textures_; }
  // Combien de lots le dernier téléversement a dû cuire : ceux qu'aucun
  // précédent ne portait déjà tels quels.
  [[nodiscard]] std::size_t baked_batches() const noexcept { return baked_batches_; }

  // Dessine par-dessus une scène déjà rendue : ni effacement, ni changement de
  // cible. Le test de profondeur du terrain reste en vigueur.
  //
  // `clock` pose les modèles animés, et doit avoir démarré sur les MÊMES modèles
  // que ceux téléversés. Sans elle, ils restent à l'image zéro.
  //
  // `opacities` écarte les poses qui ne sont plus pleines : c'est `DrawVeiled`
  // qui les peint, plus tard.
  //
  // `points` sont les lumières ponctuelles que le décor reçoit, en plus de
  // celles de la carte.
  void Draw(const Camera& camera, const SceneLight& light, float aspect_ratio,
            const ModelStages& stages = ModelStages(), const ModelClock* clock = nullptr,
            const SceneFog& fog = SceneFog(), const ModelOpacities* opacities = nullptr,
            const SunShadowView& sun = SunShadowView(),
            const PointLightSet& points = PointLightSet()) const;

  // LE DÉCOR DANS LA CARTE D'OMBRE : sa seule profondeur, vue du soleil, toutes
  // poses pleines, posées par `clock` comme dans `Draw`. `casting` écarte les
  // poses qui n'y sont pas pleines : voir `ShadowCastingOpacities`.
  void DrawDepth(const glm::mat4& light_view_projection, const ModelClock* clock,
                 const ModelOpacities* casting = nullptr) const;

  // LES POSES QU'UNE FACE D'OMBRE PONCTUELLE VOIT, à passer à `DrawDepth` comme
  // `casting` : voir `SightedCasting`. Rend vrai quand l'une d'elles s'anime.
  bool Sighted(const ShadowSight& sight, const ModelOpacities* casting,
               ModelOpacities& into) const {
    return SightedCasting(pose_boxes_, casting, sight, into);
  }

  // LA PASSE TRANSLUCIDE DES POSES VOILÉES, par-dessus tout ce qui est peint :
  // leurs suites de faces triées de la plus lointaine à la plus proche, mêlées
  // à ce qui est derrière, sans écrire la profondeur. `models` sont ceux
  // téléversés, dans le même ordre. Voir `veiled_prims.h`.
  //
  // Elle peint aussi, triées avec elles, les faces à texture mêlée de toutes
  // les poses pleines : voir `ModelStages::blended_apart`. Elle se dessine donc
  // même sans aucune pose voilée.
  void DrawVeiled(const Camera& camera, const SceneLight& light, float aspect_ratio,
                  const std::vector<ModelInstances>& models, std::span<const VeiledPose> veiled,
                  const ModelClock* clock = nullptr, const SceneFog& fog = SceneFog(),
                  const PointLightSet& points = PointLightSet()) const;

  [[nodiscard]] std::size_t instance_count() const noexcept { return instance_count_; }
  // Combien de fois le décor a été téléversé : ce qui dit qu'il a changé.
  [[nodiscard]] std::uint64_t uploads() const noexcept { return uploads_; }

  // Pose le filtrage des textures. ⚠ À CHAQUE IMAGE SI L'ON VEUT : rien ne
  // se fait tant qu'il ne change pas. Les niveaux réduits se génèrent la
  // première fois qu'on les demande, puis pour chaque texture qui arrive.
  void SetTextureFilter(const ModelTextureFilter& filter);

  // Le plus d'échantillons anisotropes que la carte graphique accepte ; un
  // quand elle ne le sait pas.
  [[nodiscard]] int max_anisotropy() const noexcept { return max_anisotropy_; }

 private:
  // Un tampon de sommets et sa description.
  struct VertexStore {
    unsigned int vertex_array = 0;
    unsigned int vertex_buffer = 0;
    // Le côté de chaque sommet, voir `FaceSide`.
    unsigned int side_buffer = 0;
  };

  // Un lot de dessin : les textures d'un modèle, ses poses figées cuites en un
  // maillage du monde, et ses poses animées.
  struct Batch {
    // Index du modèle dans ce qu'Upload a reçu : c'est lui que l'horloge connaît.
    std::size_t model_index = 0;
    // Dans l'ordre de ModelMesh::textures : des entrées de `textures_`, ou nulles
    // — le repli se dessine alors.
    std::vector<const GlTexture*> textures;
    // Le seuil de découpe de chacune, dans le même ordre : voir `SceneCutoutOf`.
    std::vector<float> alpha_references;
    // Vrai pour une texture que le jeu mêle au fond, dans le même ordre : voir
    // `BlendedTextures`.
    std::vector<std::uint8_t> blended;
    // Vrai pour une texture qu'un nœud du modèle anime, dans le même ordre :
    // elle se répète, là où toutes les autres se bornent à leurs bords.
    std::vector<std::uint8_t> repeats;
    // Les poses figées, cuites : une suite par texture. Voir `BakeInstances`.
    VertexStore baked;
    std::vector<BakedRun> baked_runs;
    // Et le bloc de chaque surface, pour sauter une pose voilée.
    std::vector<BakedSurface> baked_surfaces;
    std::size_t still_poses = 0;
    // Voir ModelInstances::rsm_version : il décide du côté des faces simples.
    std::int32_t rsm_version = 0;
    // Voir ModelInstances::shade_type.
    std::int32_t shade_type = 0;
    // Le gréement, pour les poses qui s'animent.
    VertexStore rig;
    std::vector<ModelRigPart> rig_parts;
    std::vector<std::vector<glm::mat4>> animated_transforms;
    // La pose et les grandeurs de texture de l'image zéro, faute d'horloge.
    std::vector<glm::mat4> rest_pose;
    std::vector<TextureMotion> rest_texture_motions;
    // Ce qui fait le maillage, pour reconnaître un modèle inchangé au
    // téléversement suivant.
    ModelMeshIdentity identity;
    // Et ce qui fait la cuisson : les poses figées.
    std::vector<glm::mat4> baked_transforms;
    // Vrai une fois qu'un modèle a repris ses tampons : un lot ne sert qu'une
    // fois.
    bool taken = false;
  };

  // Reprend le lot de `previous`, pas encore repris, qui porte le maillage
  // d'identité `identity` ; ou nul.
  [[nodiscard]] static Batch* TakeSameMesh(std::vector<Batch>& previous,
                                           const ModelMeshIdentity& identity);

  // La pose des nœuds d'un groupe animé et les grandeurs de ses lots, à l'image
  // de l'horloge — celles de l'image zéro faute d'horloge.
  struct GroupPose {
    std::span<const glm::mat4> nodes;
    std::span<const TextureMotion> motions;
  };

  // `sides` porte un côté par sommet ; vide, toutes les faces sont doubles.
  [[nodiscard]] static VertexStore UploadVertices(const std::vector<ModelVertex>& vertices,
                                                  std::span<const FaceSide> sides);
  // Remplit le tampon lié à GL_ARRAY_BUFFER, doubles faute de côtés.
  static void UploadSides(std::size_t vertex_count, std::span<const FaceSide> sides);
  static void Release(VertexStore& store);
  void Use(const glm::mat4& view_projection, const SceneLight& light, const ModelStages& stages,
           const SceneFog& fog, const SunShadowView& sun, const PointLightSet& points) const;
  // La passe opaque, vue par `view_projection` : la caméra ou le soleil.
  void DrawOpaque(const glm::mat4& view_projection, const SceneLight& light,
                  const ModelStages& stages, const ModelClock* clock, const SceneFog& fog,
                  const ModelOpacities* opacities, const SunShadowView& sun,
                  const PointLightSet& points) const;
  // Lie la texture et son échantillonneur ; rend la texture liée, nulle pour le
  // repli.
  const GlTexture* BindTexture(const Batch& batch, std::int32_t texture_index, bool filtered) const;
  // `blended_apart` saute une texture mêlée : voir `ModelStages::blended_apart`.
  void DrawRange(const Batch& batch, std::int32_t texture_index, std::uint32_t first_vertex,
                 std::uint32_t vertex_count, bool filtered, bool blended_apart) const;
  // Les poses figées d'un lot, moins celles qui ne sont plus pleines.
  void DrawStillExcept(const Batch& batch, std::span<const std::uint8_t> opacity, bool filtered,
                       bool blended_apart) const;
  [[nodiscard]] const Batch* BatchOf(std::size_t model_index) const;
  [[nodiscard]] GroupPose PoseOf(const Batch& batch, const ModelClock* clock,
                                 std::size_t group) const;
  void SetTextureMotion(const TextureMotion& motion) const;
  // La texture partagée de la `rank`-ième texture d'un modèle, téléversée à sa
  // première citation, ou nulle. Note son nom parmi ceux que ce téléversement
  // cite.
  [[nodiscard]] const GlTexture* SharedTexture(const ModelInstances& model, std::size_t rank,
                                               std::set<std::string>& cited);

  GlProgram program_;
  GlTexture placeholder_;
  // Le tampon où la passe translucide verse, à chaque image, ce qu'elle émet.
  VertexStore veiled_;
  // Les textures, une par nom normalisé. ⚠ UNE TABLE ORDONNÉE : les lots
  // pointent dans ses nœuds, qu'un ajout ne déplace pas.
  std::map<std::string, GlTexture> textures_;
  std::vector<Batch> batches_;
  // L'emprise de chaque pose, par modèle téléversé.
  std::vector<ModelPoseBoxes> pose_boxes_;
  std::uint64_t uploads_ = 0;
  std::size_t instance_count_ = 0;
  std::size_t uploaded_textures_ = 0;
  std::size_t baked_batches_ = 0;
  // LES ÉCHANTILLONNEURS, un lié à chaque tracé : borné à ses bords ou répété
  // selon que la texture s'anime — voir `Batch::repeats` —, net ou avec les
  // niveaux réduits du filtrage choisi. Ils écrasent ceux des textures. Voir
  // `SamplerRank` pour leur ordre.
  static constexpr std::size_t kSamplerCount = 4;
  std::array<unsigned int, kSamplerCount> samplers_{};
  ModelTextureFilter filter_;
  int max_anisotropy_ = 1;
  // Vrai quand chaque texture téléversée porte ses niveaux réduits.
  bool mipmaps_complete_ = true;
};

}  // namespace ro::render
