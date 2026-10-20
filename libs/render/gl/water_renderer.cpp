#include "ro/render/water_renderer.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include <glad/glad.h>

#include "ro/render/fog_shader.h"

namespace ro::render {
namespace {

// LA HOULE EST CALCULÉE PAR SOMMET, comme dans le client : elle déplace la
// nappe, elle ne la peint pas. Le faire au fragment ne changerait rien à
// l'image et coûterait un cosinus par pixel.
//
// L'argument du cosinus est la DIAGONALE du sommet multipliée par le pas de la
// vague, plus la phase — le tout en degrés. C'est ce qui fait courir les
// vagues en biais sur la carte : à pas égal, deux sommets d'une même diagonale
// montent ensemble.
constexpr std::string_view kVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texture_uv;
layout(location = 2) in float a_wave_step;

uniform mat4 u_view_projection;
uniform float u_wave_height;
uniform float u_wave_pitch;
uniform float u_phase_degrees;

out vec2 v_texture_uv;
out float v_fog_kept;

void main() {
    v_texture_uv = a_texture_uv;
    // RETRANCHÉE, ET NON AJOUTÉE : l'axe vertical du fichier descend, celui de
    // la scène monte. Le signe ne se voit pas à l'œil — il vaut une demi-période
    // de phase — mais l'inverser rendrait fausse la comparaison avec le client.
    float wave = u_wave_height * cos(radians(a_wave_step * u_wave_pitch + u_phase_degrees));
    gl_Position = u_view_projection * vec4(a_position.x, a_position.y - wave, a_position.z, 1.0);
    v_fog_kept = FogKeptPart(gl_Position.w);
}
)GLSL";

// L'eau ne reçoit PAS la lumière de la carte : le client peint ses quatre
// sommets d'un blanc fixe et ne module que l'alpha. Lui appliquer l'éclairage
// la teindrait d'une couleur que le jeu ne lui donne jamais.
//
// LA NUIT, ELLE, LA TOUCHE : l'heure et la nuit de la carte l'assombrissent du
// même facteur que la couleur du sol, un en plein jour.
constexpr std::string_view kFragmentShader = R"GLSL(
#version 330 core
in vec2 v_texture_uv;
in float v_fog_kept;

uniform sampler2D u_texture;
uniform float u_opacity;
// Ce que l'heure et la nuit laissent de la lumière : voir `WaterRenderer::Draw`.
uniform vec3 u_sky_light;

out vec4 frag_color;

void main() {
    // L'alpha vient du réglage, pas de la texture : les images d'eau sont des
    // JPEG, donc opaques. Sans lui, la nappe cacherait le fond au lieu de le
    // voiler.
    vec3 color = texture(u_texture, v_texture_uv).rgb * u_sky_light;
    frag_color = vec4(Fogged(color, v_fog_kept), u_opacity);
}
)GLSL";

}  // namespace

WaterRenderer::WaterRenderer(WaterRenderer&& other) noexcept
    : program_(std::move(other.program_)),
      animations_(std::move(other.animations_)),
      zones_(std::move(other.zones_)),
      vertex_array_(other.vertex_array_),
      vertex_buffer_(other.vertex_buffer_) {
  other.vertex_array_ = 0;
  other.vertex_buffer_ = 0;
}

WaterRenderer& WaterRenderer::operator=(WaterRenderer&& other) noexcept {
  if (this != &other) {
    program_ = std::move(other.program_);
    animations_ = std::move(other.animations_);
    zones_ = std::move(other.zones_);
    std::swap(vertex_array_, other.vertex_array_);
    std::swap(vertex_buffer_, other.vertex_buffer_);
  }
  return *this;
}

WaterRenderer::~WaterRenderer() {
  if (vertex_buffer_ != 0) {
    glDeleteBuffers(1, &vertex_buffer_);
  }
  if (vertex_array_ != 0) {
    glDeleteVertexArrays(1, &vertex_array_);
  }
}

core::Result<WaterRenderer> WaterRenderer::Create() {
  auto program = GlProgram::Link(WithFog(kVertexShader, kFogVertexGlsl),
                                 WithFog(kFragmentShader, kFogFragmentGlsl));
  if (!program) {
    return std::unexpected(program.error());
  }
  WaterRenderer renderer;
  renderer.program_ = std::move(*program);
  return renderer;
}

core::Result<void> WaterRenderer::Upload(const WaterPlane& plane,
                                         const std::vector<WaterAnimation>& animations) {
  zones_ = plane.zones;

  animations_.clear();
  animations_.reserve(animations.size());
  for (const WaterAnimation& animation : animations) {
    UploadedAnimation uploaded;
    uploaded.type = animation.type;
    uploaded.frames.reserve(animation.frames.size());
    for (const formats::Image& frame : animation.frames) {
      // GL_REPEAT, et il n'est pas décoratif : la nappe pave par carrés de
      // quatre cubes, et le sommet du bord droit porte u = 1 quand celui du
      // carré suivant porte u = 0. En mode bordé, le filtrage y mélangerait le
      // dernier texel avec lui-même et poserait une couture tous les quatre
      // cubes.
      auto texture = GlTexture::Upload(frame, GlTexture::Filtering::kSharp);
      if (texture) {
        uploaded.frames.push_back(std::move(*texture));
      }
    }
    animations_.push_back(std::move(uploaded));
  }

  if (plane.vertices.empty()) {
    return {};
  }

  if (vertex_array_ == 0) {
    glGenVertexArrays(1, &vertex_array_);
    glGenBuffers(1, &vertex_buffer_);
  }

  glBindVertexArray(vertex_array_);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(plane.vertices.size() * sizeof(WaterVertex)),
               plane.vertices.data(), GL_STATIC_DRAW);

  const auto stride = static_cast<GLsizei>(sizeof(WaterVertex));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(WaterVertex, position)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(WaterVertex, texture_uv)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(WaterVertex, wave_step)));
  glBindVertexArray(0);

  return {};
}

const GlTexture* WaterRenderer::FrameOf(std::int32_t type, int frame) const {
  const auto found = std::ranges::find(animations_, type, &UploadedAnimation::type);
  if (found == animations_.end() || found->frames.empty()) {
    return nullptr;
  }
  const auto index = static_cast<std::size_t>(frame) % found->frames.size();
  return &found->frames[index];
}

void WaterRenderer::Draw(const Camera& camera, float aspect_ratio, std::uint32_t elapsed_ms,
                         const SceneFog& fog, const glm::vec3& sky_light) const {
  if (zones_.empty() || vertex_array_ == 0) {
    return;
  }

  glEnable(GL_DEPTH_TEST);
  // 🔴 LA PROFONDEUR SE LIT MAIS NE S'ÉCRIT PAS. La nappe est transparente :
  // en écrivant sa profondeur, la moitié arrière d'une même nappe se ferait
  // rejeter par sa moitié avant, et l'eau se couvrirait de plaques manquantes
  // dont la forme dépendrait de l'ordre des sommets.
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  // Même raison que le terrain : les faces ne sont pas orientées de façon
  // cohérente, et en éliminer ferait disparaître des morceaux de nappe.
  glDisable(GL_CULL_FACE);

  program_.Use();
  program_.SetMatrix4("u_view_projection",
                      camera.ProjectionMatrix(aspect_ratio) * camera.ViewMatrix());
  program_.SetInt("u_texture", 0);
  program_.SetFloat("u_opacity", kWaterOpacity);
  program_.SetVector3("u_sky_light", sky_light);
  SetFogUniforms(program_, fog);

  glBindVertexArray(vertex_array_);
  for (const WaterZoneMesh& zone : zones_) {
    const GlTexture* frame = FrameOf(zone.settings.type, WaterFrameAt(zone.settings, elapsed_ms));
    if (frame == nullptr || !frame->valid()) {
      continue;
    }
    frame->Bind(0);
    program_.SetFloat("u_wave_height", zone.settings.wave_height);
    program_.SetFloat("u_wave_pitch", zone.settings.wave_pitch);
    program_.SetFloat("u_phase_degrees", WaterPhaseAt(zone.settings, elapsed_ms));
    glDrawArrays(GL_TRIANGLES, static_cast<GLint>(zone.first_vertex),
                 static_cast<GLsizei>(zone.vertex_count));
  }
  glBindVertexArray(0);

  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

}  // namespace ro::render
