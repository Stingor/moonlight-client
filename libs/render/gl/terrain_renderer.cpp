#include "ro/render/terrain_renderer.h"

#include <string_view>
#include <utility>

#include <glad/glad.h>

#include "ro/render/fog_shader.h"
#include "ro/render/point_light_shader.h"
#include "ro/render/scene_cutout.h"
#include "ro/render/sun_shadow_map.h"

namespace ro::render {
namespace {

// Le calcul d'éclairage est fait par SOMMET, pas par fragment.
//
// Ce n'est pas une optimisation : c'est ce que fait le client officiel, et
// l'interpolation linéaire du résultat entre les sommets donne à RO son rendu
// caractéristique. Le passer au fragment changerait l'aspect des pentes.
constexpr std::string_view kVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texture_uv;
layout(location = 2) in vec2 a_lightmap_uv;
layout(location = 3) in vec4 a_color;
layout(location = 4) in vec3 a_normal;

uniform mat4 u_view_projection;
uniform vec3 u_light_direction;
uniform vec3 u_light_ambient;
uniform vec3 u_light_diffuse;

out vec2 v_texture_uv;
out vec2 v_lightmap_uv;
out vec4 v_color;
out vec3 v_light;
// L'éclairement sans le soleil : celui d'un point à l'ombre.
out vec3 v_shaded_light;
out float v_fog_kept;

void main() {
    v_texture_uv = a_texture_uv;
    v_lightmap_uv = a_lightmap_uv;
    v_color = a_color;
    v_world_position = a_position;
    gl_Position = u_view_projection * vec4(a_position, 1.0);
    v_fog_kept = FogKeptPart(gl_Position.w);
    SunShadowProject(a_position, normalize(a_normal));

    // Incidence mesurée contre la direction TELLE QUELLE, sans la négliger.
    //
    // Le shader de référence écrit -lightDirection, mais ses normales de sol
    // pointent vers le BAS ; les nôtres pointent vers le haut (vérifié par
    // test). Reprendre son signe donnerait une incidence négative sur tout
    // terrain plat, donc l'ambiante seule et des cartes uniformément sombres —
    // ce que dément une capture du client, où le sol est clair.
    float incidence = clamp(dot(normalize(a_normal), u_light_direction), 0.0, 1.0);

    // Ambiante et diffuse ne s'additionnent pas simplement : chacune est
    // d'abord mélangée à l'autre. La formule vient du client, et la remplacer
    // par une somme donne des cartes trop claires qui saturent partout.
    vec3 ambient_factor = (1.0 - u_light_ambient) * u_light_ambient;
    vec3 ambient = u_light_ambient - ambient_factor + ambient_factor * u_light_diffuse;
    vec3 diffuse_factor = (1.0 - u_light_diffuse) * u_light_diffuse;
    vec3 diffuse = u_light_diffuse - diffuse_factor + diffuse_factor * u_light_ambient;

    vec3 lit = min(incidence * diffuse + ambient, 1.0);
    // Plafond : au-delà d'une somme de 1, le client change littéralement de
    // formule et borne l'éclairement au mélange des deux composantes.
    vec3 ceiling = min(max(u_light_diffuse, u_light_ambient) +
                       (1.0 - max(u_light_diffuse, u_light_ambient)) *
                           min(u_light_diffuse, u_light_ambient),
                       1.0);
    v_light = clamp(min(lit, ceiling), 0.0, 1.0);
    v_shaded_light = clamp(min(ambient, ceiling), 0.0, 1.0);
}
)GLSL";

constexpr std::string_view kFragmentShader = R"GLSL(
#version 330 core
in vec2 v_texture_uv;
in vec2 v_lightmap_uv;
in vec4 v_color;
in vec3 v_light;
in vec3 v_shaded_light;
in float v_fog_kept;

uniform sampler2D u_texture;
uniform sampler2D u_lightmap;
// Un étage vaut 1 quand il s'applique, 0 quand il est coupé. Voir
// TerrainStages : ce sont eux qui permettent d'attribuer un écart de
// luminosité à un étage plutôt que de le constater sur l'image entière.
uniform float u_vertex_light_strength;
uniform float u_shadowmap_strength;
uniform float u_lightmap_strength;
uniform float u_tile_color_strength;
// La nuit de la carte, en facteur : voir `SceneLight::tint`.
uniform vec3 u_light_tint;
// Ce que l'heure laisse de la lumière de midi : voir `SceneLight::daylight`.
uniform vec3 u_daylight;
// La lumière froide : 1 quand le rouge et le bleu de la lumière posée
// s'échangent, 0 sinon. Voir TerrainStages::cold_light.
uniform float u_cold_light;
// Le sol uni : 1 quand il remplace tout ce qui précède, 0 quand il n'est pas
// demandé. Voir TerrainStages::flat_color.
uniform float u_flat_strength;
uniform vec3 u_flat_color;
uniform float u_alpha_reference;

out vec4 frag_color;

void main() {
    vec3 surface_normal = PointSurfaceNormal();
    vec4 texel = texture(u_texture, v_texture_uv);
    // Les textures RO emploient le magenta comme couleur de transparence ; le
    // rejet franc évite d'avoir à trier les faces en profondeur. Son seuil
    // dépend du fichier de la texture : voir SceneCutoutOf.
    //
    // Il ne décide PAS de la présence d'un carreau : un carreau entièrement
    // transparent n'est jamais émis, parce que le mipmap que ce test lit
    // remonterait son alpha au-dessus du seuil dès que la caméra recule.
    if (texel.a < u_alpha_reference) {
        discard;
    }

    // À L'OMBRE DU SOLEIL, la diffuse s'éteint et l'ambiante reste.
    vec3 light = mix(v_shaded_light, v_light, SunVisibility());
    vec3 result = texel.rgb * max(light, vec3(1.0 - u_vertex_light_strength));
    // La couleur de carreau module la texture, comme l'éclairage du sommet :
    // c'est la passe de base, que l'extinction des ombres laisse en place.
    result *= max(v_color.rgb, vec3(1.0 - u_tile_color_strength));
    // LA NUIT ASSOMBRIT LA COULEUR DU CARREAU, pas la lumière posée par la
    // lightmap, qui s'ajoute ensuite.
    result *= u_light_tint * u_daylight;

    // L'alpha de la lightmap est l'OMBRE, multiplicative ; son RGB est la
    // lumière posée par-dessus, additive. Les deux ne se composent pas de la
    // même façon, et les confondre écrase le relief cuit de la carte.
    vec4 lightmap = texture(u_lightmap, v_lightmap_uv);
    result *= max(lightmap.a, 1.0 - u_shadowmap_strength);
    // LES LUMIÈRES PONCTUELLES S'AJOUTENT À L'ÉCLAIRAGE DU CARREAU, sur sa
    // texture, sans que la nuit ni l'ombre cuite ne les assombrissent.
    result += texel.rgb * PointSurfaceLighting(surface_normal);
    vec3 laid_light = mix(lightmap.rgb, lightmap.bgr, u_cold_light);
    result += clamp(laid_light, 0.0, 1.0) * u_lightmap_strength;

    // LE BROUILLARD EN DERNIER, sur la couleur finie : lumière posée comprise.
    result = Fogged(result, v_fog_kept);

    // LE SOL UNI REMPLACE, IL NE MODULE PAS : une couleur multipliée par
    // l'éclairage sortirait dégradée, donc inutilisable comme fond de capture.
    //
    // ⚠ ET IL ARRIVE APRÈS LE REJET DE TRANSPARENCE, jamais avant : l'alpha de
    // la texture décide seul de la découpe, si bien qu'un carreau troué le
    // reste. C'est ce que fait la référence, qui ne touche pas à l'étage alpha.
    result = mix(result, u_flat_color, u_flat_strength);

    frag_color = vec4(result, 1.0);
}
)GLSL";

}  // namespace

TerrainRenderer::TerrainRenderer(TerrainRenderer&& other) noexcept
    : program_(std::move(other.program_)),
      textures_(std::move(other.textures_)),
      alpha_references_(std::move(other.alpha_references_)),
      placeholder_(std::move(other.placeholder_)),
      lightmap_(std::move(other.lightmap_)),
      surfaces_(std::move(other.surfaces_)),
      has_lightmap_(other.has_lightmap_),
      vertex_array_(other.vertex_array_),
      vertex_buffer_(other.vertex_buffer_) {
  other.vertex_array_ = 0;
  other.vertex_buffer_ = 0;
  other.has_lightmap_ = false;
}

TerrainRenderer& TerrainRenderer::operator=(TerrainRenderer&& other) noexcept {
  if (this != &other) {
    program_ = std::move(other.program_);
    textures_ = std::move(other.textures_);
    alpha_references_ = std::move(other.alpha_references_);
    placeholder_ = std::move(other.placeholder_);
    lightmap_ = std::move(other.lightmap_);
    surfaces_ = std::move(other.surfaces_);
    std::swap(has_lightmap_, other.has_lightmap_);
    std::swap(vertex_array_, other.vertex_array_);
    std::swap(vertex_buffer_, other.vertex_buffer_);
  }
  return *this;
}

TerrainRenderer::~TerrainRenderer() {
  if (vertex_buffer_ != 0) {
    glDeleteBuffers(1, &vertex_buffer_);
  }
  if (vertex_array_ != 0) {
    glDeleteVertexArrays(1, &vertex_array_);
  }
}

core::Result<TerrainRenderer> TerrainRenderer::Create() {
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

  TerrainRenderer renderer;
  renderer.program_ = std::move(*program);
  renderer.placeholder_ = std::move(*placeholder);
  return renderer;
}

core::Result<void> TerrainRenderer::Upload(const TerrainMesh& mesh,
                                           const std::vector<formats::Image>& textures,
                                           const formats::Image& lightmap) {
  surfaces_ = mesh.surfaces;

  textures_.clear();
  textures_.reserve(textures.size());
  alpha_references_.clear();
  alpha_references_.reserve(textures.size());
  for (const formats::Image& image : textures) {
    const SceneCutout cutout = SceneCutoutOf(image.file);
    auto uploaded = GlTexture::Upload(image, cutout.filtering);
    // Une texture illisible ne doit pas emporter la carte entière : on la
    // remplace, et le magenta la signale à l'écran.
    textures_.push_back(uploaded ? std::move(*uploaded) : GlTexture());
    alpha_references_.push_back(cutout.alpha_reference);
  }

  has_lightmap_ = false;
  if (!lightmap.empty()) {
    if (auto uploaded = GlTexture::Upload(lightmap, GlTexture::Filtering::kAtlas); uploaded) {
      lightmap_ = std::move(*uploaded);
      has_lightmap_ = true;
    }
  }

  if (vertex_array_ == 0) {
    glGenVertexArrays(1, &vertex_array_);
    glGenBuffers(1, &vertex_buffer_);
  }

  glBindVertexArray(vertex_array_);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(TerrainVertex)),
               mesh.vertices.data(), GL_STATIC_DRAW);

  const auto stride = static_cast<GLsizei>(sizeof(TerrainVertex));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(TerrainVertex, position)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(TerrainVertex, texture_uv)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(TerrainVertex, lightmap_uv)));
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(TerrainVertex, color)));
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(TerrainVertex, normal)));
  glBindVertexArray(0);

  return {};
}

void TerrainRenderer::Draw(const Camera& camera, const SceneLight& light, float aspect_ratio,
                           const std::array<float, 4>& clear_color, const TerrainStages& stages,
                           const SceneFog& fog, const SunShadowView& sun,
                           const PointLightSet& points) const {
  glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
  // Le stencil avec la profondeur : la silhouette des corps ne vaut que pour
  // l'image qui la pose.
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  DrawSurfaces(camera.ProjectionMatrix(aspect_ratio) * camera.ViewMatrix(), light, stages, fog, sun,
               points);
}

void TerrainRenderer::DrawDepth(const glm::mat4& light_view_projection) const {
  DrawSurfaces(light_view_projection, SceneLight(), TerrainStages(), SceneFog(), SunShadowView(),
               PointLightSet());
}

void TerrainRenderer::DrawSurfaces(const glm::mat4& view_projection, const SceneLight& light,
                                   const TerrainStages& stages, const SceneFog& fog,
                                   const SunShadowView& sun, const PointLightSet& points) const {
  glEnable(GL_DEPTH_TEST);
  // Pas d'élimination des faces arrière : les faces latérales du terrain ne
  // sont pas orientées de façon cohérente d'un dénivelé à l'autre, et en
  // éliminer ferait apparaître des trous.
  glDisable(GL_CULL_FACE);

  program_.Use();
  program_.SetMatrix4("u_view_projection", view_projection);
  program_.SetInt("u_texture", 0);
  program_.SetInt("u_lightmap", 1);
  const float lightmap_present = has_lightmap_ ? 1.0F : 0.0F;
  program_.SetFloat("u_vertex_light_strength", stages.vertex_light ? 1.0F : 0.0F);
  program_.SetFloat("u_shadowmap_strength", stages.shadowmap ? lightmap_present : 0.0F);
  program_.SetFloat("u_lightmap_strength", stages.lightmap ? lightmap_present : 0.0F);
  program_.SetFloat("u_cold_light", stages.cold_light ? 1.0F : 0.0F);
  program_.SetFloat("u_tile_color_strength", stages.tile_color ? 1.0F : 0.0F);
  const std::array<float, 3> flat = stages.flat_color.value_or(std::array<float, 3>{});
  program_.SetFloat("u_flat_strength", stages.flat_color.has_value() ? 1.0F : 0.0F);
  program_.SetVector3("u_flat_color", glm::vec3(flat[0], flat[1], flat[2]));
  program_.SetVector3("u_light_direction", light.direction);
  program_.SetVector3("u_light_ambient", light.ambient);
  program_.SetVector3("u_light_diffuse", light.diffuse);
  program_.SetVector3("u_light_tint", light.tint);
  program_.SetVector3("u_daylight", light.daylight);
  SetFogUniforms(program_, fog);
  SetSunShadowUniforms(program_, sun);
  SetPointLightUniforms(program_, points);

  (has_lightmap_ ? lightmap_ : placeholder_).Bind(1);

  glBindVertexArray(vertex_array_);
  for (const TerrainSurface& surface : surfaces_) {
    const auto index = static_cast<std::size_t>(surface.texture_index);
    const bool usable = index < textures_.size() && textures_[index].valid();
    (usable ? textures_[index] : placeholder_).Bind(0);
    program_.SetFloat("u_alpha_reference",
                      usable ? alpha_references_[index] : kBlendedCutoutAlphaReference);
    glDrawArrays(GL_TRIANGLES, static_cast<GLint>(surface.first_vertex),
                 static_cast<GLsizei>(surface.vertex_count));
  }
  glBindVertexArray(0);
}

}  // namespace ro::render
