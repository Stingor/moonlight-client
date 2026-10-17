#include "ro/render/model_renderer.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <glad/glad.h>

#include "ro/formats/grf.h"
#include "ro/formats/image.h"
#include "ro/render/fog_shader.h"
#include "ro/render/point_light_shader.h"
#include "ro/render/scene_cutout.h"
#include "ro/render/sun_shadow_map.h"
#include "ro/render/veiled_prims.h"

namespace ro::render {
namespace {

// Même formule d'éclairage que le terrain, mais sans lightmap : les modèles
// n'en portent pas. La normale est transformée par la matrice de pose, sinon un
// bâtiment tourné resterait éclairé comme s'il ne l'était pas.
constexpr std::string_view kVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texture_uv;
layout(location = 2) in vec3 a_normal;
// Le côté que la face du sommet montre : voir `FaceSide`.
layout(location = 3) in uint a_side;

uniform mat4 u_view_projection;
// Un pour une pose animée en qui le côté montré s'inverse — voir
// `ShowsCounterClockwise` —, zéro sinon.
uniform int u_flip_sides;
uniform mat4 u_model;
// La pose du nœud dans le modèle, pour un sommet laissé dans son nœud ;
// l'identité pour un maillage déjà posé. Elle ne touche PAS la normale : le
// natif n'éclaire jamais un nœud selon sa pose animée.
uniform mat4 u_node;
// Les grandeurs d'une texture animée. Le natif tourne autour du centre de la
// texture, PUIS étire, PUIS décale. Au repos : 0, 1 et 0.
uniform vec2 u_texture_offset;
uniform vec2 u_texture_scale;
uniform float u_texture_angle;
uniform vec3 u_light_direction;
uniform vec3 u_light_ambient;
uniform vec3 u_light_diffuse;
// La nuit de la carte, en facteur : voir `SceneLight::tint`.
uniform vec3 u_light_tint;
// Ce que l'heure laisse de la lumière de midi : voir `SceneLight::daylight`.
uniform vec3 u_daylight;
// Ce que le .rsm déclare : zéro veut dire « pas d'ombrage ».
uniform int u_shade_type;
// Vaut 1 quand l'éclairage s'applique, 0 quand il est coupé pour mesurer.
uniform float u_light_strength;

out vec2 v_texture_uv;
out vec3 v_light;
// L'éclairement sans le soleil : celui d'une face à l'ombre.
out vec3 v_shaded_light;
out float v_fog_kept;
flat out uint v_side;

void main() {
    v_side = a_side == 0u || u_flip_sides == 0 ? a_side : 3u - a_side;
    vec2 centered = a_texture_uv - 0.5;
    float cosine = cos(u_texture_angle);
    float sine = sin(u_texture_angle);
    v_texture_uv = vec2(cosine * centered.x - sine * centered.y,
                        sine * centered.x + cosine * centered.y) * u_texture_scale +
                   0.5 + u_texture_offset;
    vec4 world = u_model * u_node * vec4(a_position, 1.0);
    v_world_position = world.xyz;
    gl_Position = u_view_projection * world;
    v_fog_kept = FogKeptPart(gl_Position.w);

    // La partie 3 × 3 de la pose, et non la transposée de son inverse : c'est
    // ainsi que le client éclaire un modèle étiré — voir `TransformedNormal`.
    vec3 normal = normalize(mat3(u_model) * a_normal);
    SunShadowProject(world.xyz, normal);
    // Une face qui tourne le dos au soleil ne reçoit que l'ambiante. Prendre
    // la valeur ABSOLUE l'éclairerait autant que celle qui lui fait face, et le
    // modelé disparaîtrait : les deux pans d'un toit sortiraient identiques.
    float incidence = clamp(dot(normal, u_light_direction), 0.0, 1.0);

    vec3 ambient_factor = (1.0 - u_light_ambient) * u_light_ambient;
    vec3 ambient = u_light_ambient - ambient_factor + ambient_factor * u_light_diffuse;
    vec3 diffuse_factor = (1.0 - u_light_diffuse) * u_light_diffuse;
    vec3 diffuse = u_light_diffuse - diffuse_factor + diffuse_factor * u_light_ambient;

    vec3 lit = min(incidence * diffuse + ambient, 1.0);
    vec3 ceiling = min(max(u_light_diffuse, u_light_ambient) +
                       (1.0 - max(u_light_diffuse, u_light_ambient)) *
                           min(u_light_diffuse, u_light_ambient),
                       1.0);
    // Un modèle sans ombrage prend le PLAFOND, pas l'incidence : sa texture
    // porte déjà son relief, et l'éclairer par la géométrie le noircirait.
    vec3 applied = u_shade_type == 0 ? ceiling : min(lit, ceiling);
    // ⚠ LA NUIT S'APPLIQUE MÊME ÉCLAIRAGE COUPÉ : elle teinte la couleur du
    // sommet, quelle qu'en soit l'origine.
    v_light = clamp(max(applied, vec3(1.0 - u_light_strength)), 0.0, 1.0) * u_light_tint * u_daylight;
    v_shaded_light = clamp(max(min(ambient, ceiling), vec3(1.0 - u_light_strength)), 0.0, 1.0) *
                     u_light_tint * u_daylight;
}
)GLSL";

constexpr std::string_view kFragmentShader = R"GLSL(
#version 330 core
in vec2 v_texture_uv;
in vec3 v_light;
in vec3 v_shaded_light;
in float v_fog_kept;
flat in uint v_side;

uniform sampler2D u_texture;
// Un quand les faces simples ne se montrent que d'un côté, zéro quand ce tri
// est coupé pour mesurer ce qu'il retire.
uniform int u_one_sided;
// Le seuil de découpe de la texture liée : il dépend de son fichier — voir
// SceneCutoutOf.
uniform float u_alpha_reference;
// L'opacité de la pose : un, sauf voilée. Elle multiplie l'alpha de la texture
// avant la découpe, comme la couleur de sommet du client.
uniform float u_opacity;
// Un pour la passe translucide, qui mêle ce qu'elle peint ; zéro sinon.
uniform float u_translucent;

out vec4 frag_color;

void main() {
    vec3 surface_normal = PointSurfaceNormal();
    // UNE FACE SIMPLE VUE DE DOS NE SE DESSINE PAS. Le sens trigonométrique à
    // l'écran est celui qu'OpenGL dit « de face ».
    if (u_one_sided != 0 &&
        ((v_side == 1u && !gl_FrontFacing) || (v_side == 2u && gl_FrontFacing))) {
        discard;
    }
    // LA DÉCOUPE SE LIT AU NIVEAU DE BASE, quel que soit le filtrage : les
    // niveaux réduits moyennent l'alpha, et boucheraient de loin les trous d'une
    // grille ou d'un feuillage. Seule la couleur profite du filtrage choisi —
    // sans niveaux réduits, les deux lectures sont la même.
    if (textureLod(u_texture, v_texture_uv, 0.0).a * u_opacity < u_alpha_reference) {
        discard;
    }
    vec4 color = texture(u_texture, v_texture_uv);
    vec3 light = mix(v_shaded_light, v_light, SunVisibility()) +
                 PointSurfaceLighting(surface_normal);
    frag_color = vec4(Fogged(color.rgb * light, v_fog_kept),
                      mix(1.0, color.a * u_opacity, u_translucent));
}
)GLSL";

// L'emplacement du côté de chaque sommet, voir `a_side`.
constexpr GLuint kSideAttribute = 3;

// Les côtés d'un gréement : ceux de ses faces pour une pose qui montre le sens
// trigonométrique ; le nuancier les retourne pour les autres.
[[nodiscard]] std::vector<FaceSide> RigSides(const ModelRig& rig) {
  constexpr std::size_t kVerticesPerFace = 3;
  std::vector<FaceSide> sides(rig.vertices.size(), FaceSide::kBoth);
  for (std::size_t vertex = 0; vertex < sides.size(); ++vertex) {
    const std::size_t face = vertex / kVerticesPerFace;
    if (face < rig.two_sided.size() && rig.two_sided[face] == 0) {
      sides[vertex] = FaceSide::kCounterClockwise;
    }
  }
  return sides;
}

// LA DÉCOUPE DE LA PASSE TRANSLUCIDE : le client n'y écarte que ce qui ne
// porte plus rien, un seul cran d'alpha sur deux cent cinquante-cinq.
constexpr float kVeiledAlphaReference = 1.0F / 255.0F;
constexpr float kOpacityScale = 255.0F;

// Le rang d'un échantillonneur dans `ModelRenderer::samplers_`.
enum SamplerRank : std::size_t {
  kSharpClamped,
  kSharpRepeated,
  kFilteredClamped,
  kFilteredRepeated,
};

[[nodiscard]] std::size_t SamplerRankOf(bool filtered, bool repeats) {
  if (filtered) {
    return repeats ? kFilteredRepeated : kFilteredClamped;
  }
  return repeats ? kSharpRepeated : kSharpClamped;
}

// LES TEXTURES QU'UN NŒUD ANIME, dans l'ordre de celles du modèle.
//
// Le jeu les échantillonne en répétition — leurs coordonnées glissent — et
// borne toutes les autres à leurs bords. Seule la seconde famille anime ses
// textures.
[[nodiscard]] std::vector<std::uint8_t> AnimatedTextures(const ModelInstances& model) {
  std::vector<std::uint8_t> animated(model.mesh.textures.size(), 0);
  if (!model.rig.second_family()) {
    return animated;
  }
  for (const ModelRigPart& part : model.rig.parts) {
    const SecondFamilyNode& node = model.rig.second_nodes[part.node];
    for (std::size_t lot = 0; lot < part.surfaces.size() && lot < part.node_textures.size();
         ++lot) {
      const std::int32_t slot = part.node_textures[lot];
      const bool moves = std::ranges::any_of(
          node.texture_animations, [slot](const formats::RsmTextureAnimation& animation) {
            return animation.texture_index == slot && !animation.keys.empty();
          });
      const auto texture = static_cast<std::size_t>(part.surfaces[lot].texture_index);
      if (moves && texture < animated.size()) {
        animated[texture] = 1;
      }
    }
  }
  return animated;
}

}  // namespace

ModelRenderer::ModelRenderer(ModelRenderer&& other) noexcept
    : program_(std::move(other.program_)),
      placeholder_(std::move(other.placeholder_)),
      veiled_(std::exchange(other.veiled_, VertexStore())),
      textures_(std::move(other.textures_)),
      batches_(std::move(other.batches_)),
      instance_count_(other.instance_count_),
      uploaded_textures_(other.uploaded_textures_),
      samplers_(std::exchange(other.samplers_, {})),
      filter_(other.filter_),
      max_anisotropy_(other.max_anisotropy_),
      mipmaps_complete_(other.mipmaps_complete_) {
  other.instance_count_ = 0;
  other.uploaded_textures_ = 0;
}

ModelRenderer& ModelRenderer::operator=(ModelRenderer&& other) noexcept {
  if (this != &other) {
    program_ = std::move(other.program_);
    placeholder_ = std::move(other.placeholder_);
    // Les tampons remplacés se rendent au pilote : les écraser sans les
    // libérer les laisserait en mémoire graphique jusqu'à la fin du jeu.
    // ⚠ LES TEXTURES PARTENT AVEC LEURS LOTS, qui pointent dans leurs nœuds.
    std::swap(veiled_, other.veiled_);
    std::swap(textures_, other.textures_);
    std::swap(batches_, other.batches_);
    std::swap(instance_count_, other.instance_count_);
    std::swap(uploaded_textures_, other.uploaded_textures_);
    std::swap(samplers_, other.samplers_);
    std::swap(filter_, other.filter_);
    std::swap(max_anisotropy_, other.max_anisotropy_);
    std::swap(mipmaps_complete_, other.mipmaps_complete_);
  }
  return *this;
}

ModelRenderer::~ModelRenderer() {
  for (Batch& batch : batches_) {
    Release(batch.baked);
    Release(batch.rig);
  }
  Release(veiled_);
  if (samplers_.front() != 0) {
    glDeleteSamplers(static_cast<GLsizei>(samplers_.size()), samplers_.data());
  }
}

ModelRenderer::VertexStore ModelRenderer::UploadVertices(const std::vector<ModelVertex>& vertices,
                                                         std::span<const FaceSide> sides) {
  VertexStore store;
  glGenVertexArrays(1, &store.vertex_array);
  glGenBuffers(1, &store.vertex_buffer);
  glGenBuffers(1, &store.side_buffer);
  glBindVertexArray(store.vertex_array);
  glBindBuffer(GL_ARRAY_BUFFER, store.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(ModelVertex)),
               vertices.data(), GL_STATIC_DRAW);

  const auto stride = static_cast<GLsizei>(sizeof(ModelVertex));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(ModelVertex, position)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(ModelVertex, texture_uv)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(ModelVertex, normal)));

  // LE CÔTÉ DE CHAQUE SOMMET, dans un tampon à part : double faute d'en dire.
  glBindBuffer(GL_ARRAY_BUFFER, store.side_buffer);
  UploadSides(vertices.size(), sides);
  glEnableVertexAttribArray(kSideAttribute);
  glVertexAttribIPointer(kSideAttribute, 1, GL_UNSIGNED_BYTE, sizeof(FaceSide), nullptr);
  glBindVertexArray(0);
  return store;
}

void ModelRenderer::UploadSides(std::size_t vertex_count, std::span<const FaceSide> sides) {
  if (sides.size() == vertex_count) {
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sides.size_bytes()), sides.data(),
                 GL_STATIC_DRAW);
    return;
  }
  const std::vector<FaceSide> both(vertex_count, FaceSide::kBoth);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(both.size() * sizeof(FaceSide)),
               both.data(), GL_STATIC_DRAW);
}

void ModelRenderer::Release(VertexStore& store) {
  if (store.vertex_buffer != 0) {
    glDeleteBuffers(1, &store.vertex_buffer);
  }
  if (store.side_buffer != 0) {
    glDeleteBuffers(1, &store.side_buffer);
  }
  if (store.vertex_array != 0) {
    glDeleteVertexArrays(1, &store.vertex_array);
  }
  store = VertexStore();
}

core::Result<ModelRenderer> ModelRenderer::Create() {
  auto program = GlProgram::Link(
      WithPointLights(WithFog(WithSunShadow(kVertexShader, kSunShadowVertexGlsl), kFogVertexGlsl),
                      kPointLightVertexGlsl),
      WithPointLights(
          WithFog(WithSunShadow(kFragmentShader, kSunShadowFragmentGlsl), kFogFragmentGlsl),
          kPointLightFragmentGlsl, kPointLightGlsl));
  if (!program) {
    return std::unexpected(program.error());
  }
  auto placeholder = GlTexture::Placeholder();
  if (!placeholder) {
    return std::unexpected(placeholder.error());
  }

  ModelRenderer renderer;
  renderer.program_ = std::move(*program);
  renderer.placeholder_ = std::move(*placeholder);
  renderer.veiled_ = UploadVertices({}, {});
  glGenSamplers(static_cast<GLsizei>(renderer.samplers_.size()), renderer.samplers_.data());
  for (std::size_t rank = 0; rank < renderer.samplers_.size(); ++rank) {
    const unsigned int sampler = renderer.samplers_[rank];
    const bool filtered = rank == kFilteredClamped || rank == kFilteredRepeated;
    const bool repeats = rank == kSharpRepeated || rank == kFilteredRepeated;
    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER,
                        filtered ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, repeats ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, repeats ? GL_REPEAT : GL_CLAMP_TO_EDGE);
  }
  renderer.max_anisotropy_ = GlTexture::MaxAnisotropy();
  return renderer;
}

ModelRenderer::Batch* ModelRenderer::TakeSameMesh(std::vector<Batch>& previous,
                                                  const ModelMeshIdentity& identity) {
  for (Batch& batch : previous) {
    if (!batch.taken && batch.identity == identity) {
      batch.taken = true;
      return &batch;
    }
  }
  return nullptr;
}

core::Result<void> ModelRenderer::Upload(const std::vector<ModelInstances>& models) {
  // Les lots précédents qu'un modèle inchangé ne reprend pas se rendent au
  // pilote à la fin : sinon, ils resteraient en mémoire graphique.
  std::vector<Batch> previous = std::exchange(batches_, {});
  instance_count_ = 0;
  uploaded_textures_ = 0;
  baked_batches_ = 0;
  batches_.reserve(models.size());
  // Les noms que ce téléversement cite : les autres textures se rendront au
  // pilote une fois tous les lots posés.
  std::set<std::string> cited;

  for (std::size_t model_index = 0; model_index < models.size(); ++model_index) {
    const ModelInstances& model = models[model_index];
    const bool animated = !model.rig.empty() && !model.animated.empty();
    if (model.mesh.empty() || (model.transforms.empty() && !animated)) {
      continue;
    }

    Batch batch;
    batch.model_index = model_index;
    batch.shade_type = model.shade_type;
    batch.textures.reserve(model.textures.size());
    batch.alpha_references.reserve(model.textures.size());
    for (std::size_t rank = 0; rank < model.textures.size(); ++rank) {
      batch.textures.push_back(SharedTexture(model, rank, cited));
      const formats::Image* image = model.textures[rank].get();
      batch.alpha_references.push_back(
          SceneCutoutOf(image != nullptr ? image->file : formats::ImageFile::kOther)
              .alpha_reference);
    }
    batch.blended = BlendedTextures(model);
    batch.repeats = AnimatedTextures(model);
    batch.identity = MeshIdentityOf(model);
    batch.rsm_version = model.rsm_version;
    Batch* kept = TakeSameMesh(previous, batch.identity);
    if (kept != nullptr && kept->baked_transforms == model.transforms) {
      batch.baked = std::exchange(kept->baked, VertexStore());
      batch.baked_runs = std::move(kept->baked_runs);
      batch.baked_surfaces = std::move(kept->baked_surfaces);
    } else {
      const BakedModel baked = BakeInstances(model.mesh, model.transforms, model.rsm_version);
      if (!baked.vertices.empty()) {
        batch.baked = UploadVertices(baked.vertices, baked.sides);
        batch.baked_runs = baked.runs;
        batch.baked_surfaces = baked.surfaces;
      }
      ++baked_batches_;
    }
    batch.baked_transforms = model.transforms;
    batch.still_poses = model.transforms.size();
    instance_count_ += model.transforms.size();

    if (animated) {
      if (kept != nullptr && kept->rig.vertex_array != 0) {
        batch.rig = std::exchange(kept->rig, VertexStore());
        batch.rig_parts = std::move(kept->rig_parts);
        batch.rest_pose = std::move(kept->rest_pose);
        batch.rest_texture_motions = std::move(kept->rest_texture_motions);
      } else {
        batch.rig = UploadVertices(model.rig.vertices, RigSides(model.rig));
        batch.rig_parts = model.rig.parts;
        batch.rest_pose = PoseRig(model.rig, 0.0F);
        batch.rest_texture_motions = RigTextureMotions(model.rig, 0.0F);
      }
      for (const ModelPlaybackGroup& group : model.animated) {
        batch.animated_transforms.push_back(group.transforms);
        instance_count_ += group.transforms.size();
      }
    }
    batches_.push_back(std::move(batch));
  }
  for (Batch& batch : previous) {
    Release(batch.baked);
    Release(batch.rig);
  }
  std::erase_if(textures_, [&cited](const auto& kept) { return !cited.contains(kept.first); });
  return {};
}

const GlTexture* ModelRenderer::SharedTexture(const ModelInstances& model, std::size_t rank,
                                              std::set<std::string>& cited) {
  const std::shared_ptr<const formats::Image>& image = model.textures[rank];
  if (image == nullptr || rank >= model.mesh.textures.size()) {
    return nullptr;
  }
  std::string name = formats::NormalizeGrfPath(model.mesh.textures[rank]);
  auto [kept, fresh] = textures_.try_emplace(name);
  if (fresh) {
    // ⚠ UN ÉCHEC SE RETIENT AUSSI : la même image échouerait de même au
    // téléversement suivant.
    if (auto uploaded = GlTexture::Upload(*image, SceneCutoutOf(image->file).filtering); uploaded) {
      kept->second = std::move(*uploaded);
      // Une texture qui arrive quand les niveaux réduits sont demandés les
      // reçoit tout de suite ; sinon, ils manqueront à la prochaine demande.
      if (filter_.mipmaps) {
        kept->second.GenerateMipmaps();
      } else {
        mipmaps_complete_ = false;
      }
    }
    ++uploaded_textures_;
  }
  cited.insert(std::move(name));
  return kept->second.valid() ? &kept->second : nullptr;
}

void ModelRenderer::SetTextureFilter(const ModelTextureFilter& filter) {
  const int anisotropy = std::clamp(filter.anisotropy, 1, max_anisotropy_);
  if (filter.mipmaps && !mipmaps_complete_) {
    for (auto& [name, texture] : textures_) {
      texture.GenerateMipmaps();
    }
    mipmaps_complete_ = true;
  }
  if (samplers_.front() != 0 && max_anisotropy_ > 1 && anisotropy != filter_.anisotropy) {
    for (const std::size_t rank : {kFilteredClamped, kFilteredRepeated}) {
      glSamplerParameterf(samplers_[rank], GL_TEXTURE_MAX_ANISOTROPY,
                          static_cast<float>(anisotropy));
    }
  }
  filter_ = ModelTextureFilter{.mipmaps = filter.mipmaps, .anisotropy = anisotropy};
}

const GlTexture* ModelRenderer::BindTexture(const Batch& batch, std::int32_t texture_index,
                                            bool filtered) const {
  const auto index = static_cast<std::size_t>(texture_index);
  const GlTexture* texture =
      texture_index >= 0 && index < batch.textures.size() ? batch.textures[index] : nullptr;
  (texture != nullptr ? *texture : placeholder_).Bind(0);
  const bool repeats =
      texture != nullptr && index < batch.repeats.size() && batch.repeats[index] != 0;
  glBindSampler(0, samplers_[SamplerRankOf(filtered, repeats)]);
  return texture;
}

void ModelRenderer::DrawRange(const Batch& batch, std::int32_t texture_index,
                              std::uint32_t first_vertex, std::uint32_t vertex_count, bool filtered,
                              bool blended_apart) const {
  const auto rank = static_cast<std::size_t>(texture_index);
  if (blended_apart && texture_index >= 0 && rank < batch.blended.size() &&
      batch.blended[rank] != 0) {
    return;
  }
  const GlTexture* texture = BindTexture(batch, texture_index, filtered);
  program_.SetFloat("u_alpha_reference",
                    texture != nullptr
                        ? batch.alpha_references[static_cast<std::size_t>(texture_index)]
                        : kBlendedCutoutAlphaReference);
  glDrawArrays(GL_TRIANGLES, static_cast<GLint>(first_vertex), static_cast<GLsizei>(vertex_count));
}

void ModelRenderer::DrawStillExcept(const Batch& batch, std::span<const std::uint8_t> opacity,
                                    bool filtered, bool blended_apart) const {
  const std::size_t poses = batch.still_poses;
  const auto kept = [&opacity](std::size_t pose) {
    return pose >= opacity.size() || opacity[pose] == kVeilOpaque;
  };
  for (const BakedSurface& surface : batch.baked_surfaces) {
    // Les poses pleines contiguës partent d'un seul tracé.
    std::size_t pose = 0;
    while (pose < poses) {
      if (!kept(pose)) {
        ++pose;
        continue;
      }
      const std::size_t start = pose;
      while (pose < poses && kept(pose)) {
        ++pose;
      }
      DrawRange(
          batch, surface.texture_index,
          surface.first_vertex + static_cast<std::uint32_t>(start) * surface.vertices_per_pose,
          static_cast<std::uint32_t>(pose - start) * surface.vertices_per_pose, filtered,
          blended_apart);
    }
  }
}

// ⚠ PAR DICHOTOMIE : `Upload` range les lots dans l'ordre croissant des
// modèles, et la passe voilée en cherche un par face émise et par face peinte,
// à chaque image — une recherche linéaire y coûtait faces × lots.
const ModelRenderer::Batch* ModelRenderer::BatchOf(std::size_t model_index) const {
  const auto found = std::ranges::lower_bound(batches_, model_index, {}, &Batch::model_index);
  return found != batches_.end() && found->model_index == model_index ? &*found : nullptr;
}

void ModelRenderer::SetTextureMotion(const TextureMotion& motion) const {
  program_.SetVector2("u_texture_offset", {motion.offset_u, motion.offset_v});
  program_.SetVector2("u_texture_scale", {motion.scale_u, motion.scale_v});
  program_.SetFloat("u_texture_angle", motion.angle);
}

void ModelRenderer::Use(const glm::mat4& view_projection, const SceneLight& light,
                        const ModelStages& stages, const SceneFog& fog, const SunShadowView& sun,
                        const PointLightSet& points) const {
  program_.Use();
  program_.SetMatrix4("u_view_projection", view_projection);
  program_.SetInt("u_texture", 0);
  program_.SetVector3("u_light_direction", light.direction);
  program_.SetVector3("u_light_ambient", light.ambient);
  program_.SetVector3("u_light_diffuse", light.diffuse);
  program_.SetVector3("u_light_tint", light.tint);
  program_.SetVector3("u_daylight", light.daylight);
  program_.SetFloat("u_light_strength", stages.light ? 1.0F : 0.0F);
  program_.SetInt("u_one_sided", stages.one_sided ? 1 : 0);
  program_.SetInt("u_flip_sides", 0);
  SetFogUniforms(program_, fog);
  SetSunShadowUniforms(program_, sun);
  SetPointLightUniforms(program_, points);
}

ModelRenderer::GroupPose ModelRenderer::PoseOf(const Batch& batch, const ModelClock* clock,
                                               std::size_t group) const {
  GroupPose posed;
  if (clock != nullptr) {
    posed.nodes = clock->Pose(batch.model_index, group);
    posed.motions = clock->TextureMotions(batch.model_index, group);
  }
  if (posed.nodes.size() != batch.rest_pose.size() ||
      posed.motions.size() != batch.rest_texture_motions.size()) {
    posed.nodes = batch.rest_pose;
    posed.motions = batch.rest_texture_motions;
  }
  return posed;
}

void ModelRenderer::Draw(const Camera& camera, const SceneLight& light, float aspect_ratio,
                         const ModelStages& stages, const ModelClock* clock, const SceneFog& fog,
                         const ModelOpacities* opacities, const SunShadowView& sun,
                         const PointLightSet& points) const {
  DrawOpaque(camera.ProjectionMatrix(aspect_ratio) * camera.ViewMatrix(), light, stages, clock, fog,
             opacities, sun, points);
}

void ModelRenderer::DrawDepth(const glm::mat4& light_view_projection,
                              const ModelClock* clock) const {
  // LES DEUX CÔTÉS D'UNE FACE PORTENT OMBRE, et une face mêlée aussi, découpée
  // à son seuil : un feuillage ombre le sol par ses seules feuilles.
  DrawOpaque(light_view_projection, SceneLight(),
             ModelStages{.light = true, .one_sided = false, .blended_apart = false}, clock,
             SceneFog(), nullptr, SunShadowView(), PointLightSet());
}

void ModelRenderer::DrawOpaque(const glm::mat4& view_projection, const SceneLight& light,
                               const ModelStages& stages, const ModelClock* clock,
                               const SceneFog& fog, const ModelOpacities* opacities,
                               const SunShadowView& sun, const PointLightSet& points) const {
  glEnable(GL_DEPTH_TEST);
  // LE TRI DES FACES SIMPLES SE FAIT DANS LE NUANCIER, sommet par sommet : les
  // poses cuites ensemble n'ont pas toutes le même côté.
  glDisable(GL_CULL_FACE);
  glFrontFace(GL_CCW);

  Use(view_projection, light, stages, fog, sun, points);
  program_.SetFloat("u_opacity", 1.0F);
  program_.SetFloat("u_translucent", 0.0F);
  // ⚠ LE PLACEHOLDER, d'un seul pixel, est complet à tout filtrage : il n'a
  // qu'un niveau, et c'est déjà le plus petit.
  const bool filtered = filter_.mipmaps;

  const glm::mat4 identity{1.0F};
  const TextureMotion still;
  for (const Batch& batch : batches_) {
    const std::span<const std::uint8_t> opacity =
        opacities != nullptr && batch.model_index < opacities->size()
            ? std::span<const std::uint8_t>((*opacities)[batch.model_index])
            : std::span<const std::uint8_t>();
    const auto full = [&opacity](std::size_t slot) {
      return slot >= opacity.size() || opacity[slot] == kVeilOpaque;
    };
    program_.SetInt("u_shade_type", batch.shade_type);
    program_.SetMatrix4("u_node", identity);
    program_.SetInt("u_flip_sides", 0);
    // Les coordonnées du maillage cuit portent déjà l'image zéro.
    SetTextureMotion(still);
    if (!batch.baked_runs.empty()) {
      // Les poses sont DANS les sommets : voir `BakeInstances`.
      program_.SetMatrix4("u_model", identity);
      glBindVertexArray(batch.baked.vertex_array);
      bool all_full = true;
      for (std::size_t pose = 0; pose < batch.still_poses; ++pose) {
        all_full = all_full && full(pose);
      }
      if (all_full) {
        for (const BakedRun& run : batch.baked_runs) {
          DrawRange(batch, run.texture_index, run.first_vertex, run.vertex_count, filtered,
                    stages.blended_apart);
        }
      } else {
        DrawStillExcept(batch, opacity, filtered, stages.blended_apart);
      }
    }

    if (batch.animated_transforms.empty()) {
      continue;
    }
    glBindVertexArray(batch.rig.vertex_array);
    std::size_t slot = batch.still_poses;
    for (std::size_t group = 0; group < batch.animated_transforms.size(); ++group) {
      const GroupPose posed = PoseOf(batch, clock, group);
      const std::span<const glm::mat4> pose = posed.nodes;
      const std::span<const TextureMotion> motions = posed.motions;
      for (const glm::mat4& transform : batch.animated_transforms[group]) {
        if (!full(slot++)) {
          continue;
        }
        program_.SetMatrix4("u_model", transform);
        program_.SetInt("u_flip_sides",
                        ShowsCounterClockwise(transform, batch.rsm_version) ? 0 : 1);
        std::size_t lot = 0;
        for (const ModelRigPart& part : batch.rig_parts) {
          program_.SetMatrix4("u_node", pose[part.node]);
          for (const ModelSurface& surface : part.surfaces) {
            SetTextureMotion(motions[lot++]);
            DrawRange(batch, surface.texture_index, surface.first_vertex, surface.vertex_count,
                      filtered, stages.blended_apart);
          }
        }
      }
    }
  }
  glBindVertexArray(0);
  glBindSampler(0, 0);
}

void ModelRenderer::DrawVeiled(const Camera& camera, const SceneLight& light, float aspect_ratio,
                               const std::vector<ModelInstances>& models,
                               std::span<const VeiledPose> veiled, const ModelClock* clock,
                               const SceneFog& fog, const PointLightSet& points) const {
  if (veiled_.vertex_buffer == 0) {
    return;
  }
  const glm::mat4 view_projection = camera.ProjectionMatrix(aspect_ratio) * camera.ViewMatrix();
  std::vector<ModelVertex> vertices;
  std::vector<VeiledPrim> prims;
  const auto emit = [&](const VeiledPose& pose, std::span<const std::uint8_t> only_textures) {
    const Batch* batch = BatchOf(pose.model_index);
    if (batch == nullptr || pose.model_index >= models.size()) {
      return;
    }
    const std::span<const glm::mat4> nodes = pose.group.has_value()
                                                 ? PoseOf(*batch, clock, *pose.group).nodes
                                                 : std::span<const glm::mat4>();
    EmitVeiledPose(pose, models[pose.model_index], nodes, view_projection, vertices, prims,
                   only_textures);
  };
  for (const VeiledPose& pose : veiled) {
    emit(pose, {});
  }
  // Les faces mêlées des poses pleines, que la passe opaque a sautées.
  for (const Batch& batch : batches_) {
    // ⚠ LE MARQUAGE DES TEXTURES MÊLÉES EST DÉJÀ CUIT DANS LE LOT : sans ce
    // raccourci, `BlendedPoses` le refaisait pour chaque modèle à chaque image —
    // une allocation et une lecture de nom par texture, pour la plupart des
    // modèles qui n'en ont aucune.
    if (batch.model_index >= models.size() ||
        std::ranges::none_of(batch.blended, [](std::uint8_t mark) { return mark != 0; })) {
      continue;
    }
    for (const VeiledPose& pose :
         BlendedPoses(models[batch.model_index], batch.model_index, veiled)) {
      emit(pose, batch.blended);
    }
  }
  if (prims.empty()) {
    return;
  }
  SortVeiledPrims(prims);

  glBindVertexArray(veiled_.vertex_array);
  glBindBuffer(GL_ARRAY_BUFFER, veiled_.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(ModelVertex)),
               vertices.data(), GL_STREAM_DRAW);
  // Les faces émises ont déjà passé le tri du client : toutes se dessinent.
  glBindBuffer(GL_ARRAY_BUFFER, veiled_.side_buffer);
  UploadSides(vertices.size(), {});
  // SANS ÉCRIRE LA PROFONDEUR, mais en la lisant : ce qui est devant un voile le
  // couvre, et le voile ne cache rien de ce qu'on peindra après lui.
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  Use(view_projection, light, ModelStages(), fog, SunShadowView(), points);
  const glm::mat4 identity{1.0F};
  program_.SetMatrix4("u_model", identity);
  program_.SetMatrix4("u_node", identity);
  program_.SetFloat("u_translucent", 1.0F);
  program_.SetFloat("u_alpha_reference", kVeiledAlphaReference);
  const bool filtered = filter_.mipmaps;
  const TextureMotion still;
  for (const VeiledPrim& prim : prims) {
    const Batch* batch = BatchOf(prim.model_index);
    if (batch == nullptr) {
      continue;
    }
    program_.SetInt("u_shade_type", batch->shade_type);
    program_.SetFloat("u_opacity", static_cast<float>(prim.opacity) / kOpacityScale);
    const std::span<const TextureMotion> motions = prim.group.has_value()
                                                       ? PoseOf(*batch, clock, *prim.group).motions
                                                       : std::span<const TextureMotion>();
    SetTextureMotion(
        prim.motion.has_value() && *prim.motion < motions.size() ? motions[*prim.motion] : still);
    static_cast<void>(BindTexture(*batch, prim.texture_index, filtered));
    glDrawArrays(GL_TRIANGLES, static_cast<GLint>(prim.first_vertex),
                 static_cast<GLsizei>(prim.vertex_count));
  }
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  glBindVertexArray(0);
  glBindSampler(0, 0);
}

}  // namespace ro::render
