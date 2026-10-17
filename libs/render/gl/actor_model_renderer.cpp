#include "ro/render/actor_model_renderer.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include <glad/glad.h>

#include "ro/render/fog_shader.h"
#include "ro/render/point_light_shader.h"
#include "ro/render/sun_shadow_map.h"

namespace ro::render {
namespace {

constexpr std::string_view kVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texture_uv;
layout(location = 2) in vec4 a_color;

uniform mat4 u_view_projection;

out vec2 v_texture_uv;
out vec4 v_color;
out float v_fog_kept;

void main() {
    v_texture_uv = a_texture_uv;
    v_color = a_color;
    v_world_position = a_position;
    gl_Position = u_view_projection * vec4(a_position, 1.0);
    v_fog_kept = FogKeptPart(gl_Position.w);
    // Les sommets ne portent pas de normale : le décalage se fait vers le ciel.
    SunShadowProject(a_position, vec3(0.0, 1.0, 0.0));
}
)GLSL";

// Le rejet des texels vides est celui des autres primitives du client : un test
// d'alpha de seuil un. La carte d'ombre coupe plus haut, comme les sprites.
constexpr std::string_view kFragmentShader = R"GLSL(
#version 330 core
in vec2 v_texture_uv;
in vec4 v_color;
in float v_fog_kept;

uniform sampler2D u_texture;
uniform float u_alpha_reference;
// Ce que l'ombre du soleil laisse au modèle : voir `SunShadowView::sprite_shade`.
uniform vec3 u_sun_shade;
// Ce que l'heure laisse de la lumière de midi : voir `SceneLight::daylight`.
uniform vec3 u_daylight;

out vec4 frag_color;

void main() {
    vec4 color = texture(u_texture, v_texture_uv) * v_color;
    if (color.a < u_alpha_reference) {
        discard;
    }
    // LA LUMIÈRE PONCTUELLE S'AJOUTE AU FACTEUR DU JOUR : ni la nuit ni l'ombre
    // du soleil ne l'assombrissent.
    color.rgb *= mix(u_sun_shade, vec3(1.0), SunVisibility()) * u_daylight +
                 PointLighting(v_world_position);
    frag_color = vec4(Fogged(color.rgb, v_fog_kept), color.a);
}
)GLSL";

constexpr float kColorAlphaReference = 1.0F / 255.0F;
// LA SILHOUETTE DANS LA CARTE D'OMBRE coupe au demi : un bord adouci ne porte
// pas d'ombre.
constexpr float kDepthAlphaReference = 0.5F;

formats::Image ImageOf(const formats::GrannyTextureImage& texture) {
  formats::Image image;
  image.width = texture.width;
  image.height = texture.height;
  image.pixels = texture.rgba;
  return image;
}

}  // namespace

ActorModelRenderer::ActorModelRenderer(ActorModelRenderer&& other) noexcept
    : program_(std::move(other.program_)),
      textures_(std::move(other.textures_)),
      vertex_array_(other.vertex_array_),
      vertex_buffer_(other.vertex_buffer_) {
  other.vertex_array_ = 0;
  other.vertex_buffer_ = 0;
}

ActorModelRenderer& ActorModelRenderer::operator=(ActorModelRenderer&& other) noexcept {
  if (this != &other) {
    Release();
    program_ = std::move(other.program_);
    textures_ = std::move(other.textures_);
    vertex_array_ = other.vertex_array_;
    vertex_buffer_ = other.vertex_buffer_;
    other.vertex_array_ = 0;
    other.vertex_buffer_ = 0;
  }
  return *this;
}

ActorModelRenderer::~ActorModelRenderer() { Release(); }

void ActorModelRenderer::Release() noexcept {
  if (vertex_buffer_ != 0) {
    glDeleteBuffers(1, &vertex_buffer_);
    vertex_buffer_ = 0;
  }
  if (vertex_array_ != 0) {
    glDeleteVertexArrays(1, &vertex_array_);
    vertex_array_ = 0;
  }
}

core::Result<ActorModelRenderer> ActorModelRenderer::Create() {
  auto program = GlProgram::Link(
      WithPointLights(WithFog(WithSunShadow(kVertexShader, kSunShadowVertexGlsl), kFogVertexGlsl),
                      kPointLightVertexGlsl),
      WithPointLights(
          WithFog(WithSunShadow(kFragmentShader, kSunShadowFragmentGlsl), kFogFragmentGlsl),
          kPointLightFragmentGlsl, kPointLightGlsl));
  if (!program) {
    return std::unexpected(program.error());
  }
  ActorModelRenderer renderer;
  renderer.program_ = std::move(*program);
  glGenVertexArrays(1, &renderer.vertex_array_);
  glGenBuffers(1, &renderer.vertex_buffer_);
  glBindVertexArray(renderer.vertex_array_);
  glBindBuffer(GL_ARRAY_BUFFER, renderer.vertex_buffer_);
  const auto stride = static_cast<GLsizei>(sizeof(ActorModelVertex));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(ActorModelVertex, position)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(ActorModelVertex, uv)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
                        reinterpret_cast<void*>(offsetof(ActorModelVertex, color)));
  glBindVertexArray(0);
  return renderer;
}

void ActorModelRenderer::Forget(const void* owner) {
  for (auto entry = textures_.begin(); entry != textures_.end();) {
    entry = entry->first.first == owner ? textures_.erase(entry) : std::next(entry);
  }
}

const GlTexture* ActorModelRenderer::TextureFor(const ActorModelTexture& texture) {
  const std::pair<const void*, std::uint64_t> key{texture.owner, texture.key};
  if (const auto known = textures_.find(key); known != textures_.end()) {
    return known->second.valid() ? &known->second : nullptr;
  }
  core::Result<GlTexture> uploaded =
      texture.emblem != nullptr ? GlTexture::Upload(*texture.emblem, GlTexture::Filtering::kSprite)
      : texture.image != nullptr
          ? GlTexture::Upload(ImageOf(*texture.image), GlTexture::Filtering::kSharp)
          : core::Result<GlTexture>(GlTexture{});
  // ⚠ L'ÉCHEC SE RETIENT AUSSI : une image illisible ne se retente pas à chaque
  // image.
  const auto stored = textures_.emplace(key, uploaded ? std::move(*uploaded) : GlTexture{}).first;
  return stored->second.valid() ? &stored->second : nullptr;
}

void ActorModelRenderer::Draw(const Camera& camera, std::span<const DrawnActorModel> models,
                              float aspect_ratio, const SceneFog& fog, const SunShadowView& sun,
                              const glm::vec3& daylight, const PointLightSet& points) {
  if (models.empty()) {
    return;
  }
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  program_.Use();
  program_.SetMatrix4("u_view_projection",
                      camera.ProjectionMatrix(aspect_ratio) * camera.ViewMatrix());
  program_.SetInt("u_texture", 0);
  program_.SetFloat("u_alpha_reference", kColorAlphaReference);
  SetFogUniforms(program_, fog);
  SetSunShadowUniforms(program_, sun, SunShadowLayer::kWorld);
  program_.SetVector3("u_sun_shade", sun.sprite_shade);
  program_.SetVector3("u_daylight", daylight);
  SetPointLightUniforms(program_, points);
  DrawMeshes(models);
  glDisable(GL_BLEND);
}

void ActorModelRenderer::DrawDepth(const glm::mat4& light_view_projection,
                                   std::span<const DrawnActorModel> models) {
  if (models.empty()) {
    return;
  }
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  program_.Use();
  program_.SetMatrix4("u_view_projection", light_view_projection);
  program_.SetInt("u_texture", 0);
  program_.SetFloat("u_alpha_reference", kDepthAlphaReference);
  SetFogUniforms(program_, SceneFog());
  SetSunShadowUniforms(program_, SunShadowView());
  program_.SetVector3("u_sun_shade", glm::vec3(1.0F));
  program_.SetVector3("u_daylight", glm::vec3(1.0F));
  SetPointLightUniforms(program_, PointLightSet());
  DrawMeshes(models);
}

void ActorModelRenderer::DrawMeshes(std::span<const DrawnActorModel> models) {
  glBindVertexArray(vertex_array_);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  for (const DrawnActorModel& model : models) {
    for (const DrawnActorModelMesh& mesh : model.meshes) {
      if (mesh.vertices.empty()) {
        continue;
      }
      const GlTexture* texture = TextureFor(mesh.texture);
      if (texture == nullptr) {
        continue;
      }
      glBufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(ActorModelVertex)),
                   mesh.vertices.data(), GL_STREAM_DRAW);
      texture->Bind(0);
      glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh.vertices.size()));
    }
  }
  glBindVertexArray(0);
}

}  // namespace ro::render
