#include "ro/render/scene_post_process.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <string>
#include <string_view>
#include <utility>

#include <glad/glad.h>

namespace ro::render {
namespace {

// AUCUN SOMMET N'EST TÉLÉVERSÉ : le triangle plein écran se déduit du rang du
// sommet, comme pour la teinte de veille. Le rang 0 donne (-1, -1), le 1
// (3, -1), le 2 (-1, 3), et le découpage rend exactement le rectangle.
//
// ⚠ AUCUN DEMI-TEXEL : Direct3D 9 décale ses sommets d'un demi-pixel pour
// aligner texels et pixels ; OpenGL les aligne déjà.
constexpr std::string_view kVertexShader = R"GLSL(
#version 330 core
out vec2 v_uv;

void main() {
    vec2 corner = vec2(float((gl_VertexID & 1) << 2), float((gl_VertexID & 2) << 1));
    v_uv = corner * 0.5;
    gl_Position = vec4(corner - 1.0, 0.0, 1.0);
}
)GLSL";

// 🔴 LA PASSE DE COULEUR, TRADUITE DE `kFxColorHLSL` INSTRUCTION PAR
// INSTRUCTION, dans le même ordre. Ses nombres viennent de `post_fx.h`, écrits
// dans le texte à la compilation.
//
// ⚠ `smoothstep` DÉPLIÉ À LA MAIN : HLSL accepte une première borne plus
// grande que la seconde, ce qui inverse la rampe ; GLSL le déclare indéfini.
constexpr std::string_view kColorBody = R"GLSL(
in vec2 v_uv;

uniform sampler2D u_source;
uniform vec4 u_grade;
uniform vec4 u_tone;
uniform vec4 u_effects;
uniform vec2 u_texel;

out vec4 frag_color;

void main() {
    vec2 uv = v_uv;
    vec3 c;
    if (u_effects.x > 0.0) {
        vec2 o = (uv - 0.5) * u_effects.x * kAberrationScale;
        c.r = texture(u_source, uv + o).r;
        c.g = texture(u_source, uv).g;
        c.b = texture(u_source, uv - o).b;
    } else {
        c = texture(u_source, uv).rgb;
    }
    if (u_effects.y > 0.0) {
        vec3 b = texture(u_source, uv + vec2(u_texel.x, 0.0)).rgb
               + texture(u_source, uv - vec2(u_texel.x, 0.0)).rgb
               + texture(u_source, uv + vec2(0.0, u_texel.y)).rgb
               + texture(u_source, uv - vec2(0.0, u_texel.y)).rgb;
        b *= 0.25;
        c += (c - b) * u_effects.y * kSharpenGain;
    }
    c = (c - 0.5) * u_grade.y + 0.5;
    c += u_grade.x;
    c = pow(clamp(c, 0.0, 1.0), vec3(u_grade.z));
    float l = dot(c, kLuminance);
    c = mix(vec3(l), c, u_grade.w);
    c.r += u_tone.x * kTemperatureStep;
    c.b -= u_tone.x * kTemperatureStep;
    if (u_tone.y > 0.5 && u_tone.y < 1.5) {
        float g = dot(c, kLuminance);
        c = vec3(g);
    } else if (u_tone.y > 1.5 && u_tone.y < 2.5) {
        float g = dot(c, kLuminance);
        c = clamp(g * kSepiaTint, 0.0, 1.0);
    } else if (u_tone.y > 2.5 && u_tone.y < 3.5) {
        c = 1.0 - c;
    } else if (u_tone.y > 3.5) {
        c = clamp(c * kColorblindTint, 0.0, 1.0);
    }
    if (u_tone.z > 0.0) {
        float d = distance(uv, vec2(0.5));
        float t = clamp((d - kVignetteFull) / (kVignetteClear - kVignetteFull), 0.0, 1.0);
        float v = t * t * (3.0 - 2.0 * t);
        c *= mix(1.0, v, u_tone.z);
    }
    if (u_tone.w > 0.0) {
        float n = fract(sin(dot(uv * (u_effects.z + 1.0), kNoiseAxis)) * kNoiseScale);
        c += (n - 0.5) * u_tone.w * kGrainSwing;
    }
    frag_color = vec4(clamp(c, 0.0, 1.0), 1.0);
}
)GLSL";

// 🔴 L'ANTICRÉNELAGE, TRADUIT DE `kFxFxaaHLSL` : cinq lectures, une sortie
// anticipée sur les zones plates, une direction le long de l'arête, quatre
// lectures sur elle, puis un mélange par la force.
//
// ⚠ LES DÉCALAGES SONT CEUX DU HLSL, et l'axe vertical opposé n'y change rien :
// la direction est la perpendiculaire du gradient, calculée et parcourue dans le
// même repère de texture.
constexpr std::string_view kFxaaBody = R"GLSL(
in vec2 v_uv;

uniform sampler2D u_source;
uniform vec2 u_texel;
uniform float u_strength;

out vec4 frag_color;

void main() {
    vec2 r = u_texel;
    vec3 nw = texture(u_source, v_uv + vec2(-1.0, -1.0) * r).rgb;
    vec3 ne = texture(u_source, v_uv + vec2(1.0, -1.0) * r).rgb;
    vec3 sw = texture(u_source, v_uv + vec2(-1.0, 1.0) * r).rgb;
    vec3 se = texture(u_source, v_uv + vec2(1.0, 1.0) * r).rgb;
    vec3 m = texture(u_source, v_uv).rgb;
    float lnw = dot(nw, kLuminance);
    float lne = dot(ne, kLuminance);
    float lsw = dot(sw, kLuminance);
    float lse = dot(se, kLuminance);
    float lm = dot(m, kLuminance);
    float lmin = min(lm, min(min(lnw, lne), min(lsw, lse)));
    float lmax = max(lm, max(max(lnw, lne), max(lsw, lse)));
    if (lmax - lmin < max(kFxaaEdgeFloor, lmax * kFxaaEdgeShare)) {
        frag_color = vec4(m, 1.0);
        return;
    }
    vec2 d;
    d.x = -((lnw + lne) - (lsw + lse));
    d.y = ((lnw + lsw) - (lne + lse));
    float dr = max((lnw + lne + lsw + lse) * 0.25 * kFxaaReduceShare, kFxaaReduceFloor);
    float rm = 1.0 / (min(abs(d.x), abs(d.y)) + dr);
    d = clamp(d * rm, -kFxaaSpanMost, kFxaaSpanMost) * r;
    vec3 a = 0.5 * (texture(u_source, v_uv + d * kFxaaNearTap).rgb
                  + texture(u_source, v_uv + d * kFxaaFarTap).rgb);
    vec3 b = a * 0.5 + 0.25 * (texture(u_source, v_uv - d * kFxaaEndTap).rgb
                             + texture(u_source, v_uv + d * kFxaaEndTap).rgb);
    float lb = dot(b, kLuminance);
    vec3 res = (lb < lmin || lb > lmax) ? a : b;
    frag_color = vec4(mix(m, res, u_strength), 1.0);
}
)GLSL";

// 🔴 LE FLOU DE ROTATION : la moyenne de lectures réparties le long d'un ARC
// autour du point visé, de bout en bout du balayage.
//
// L'écart au point visé se compte en PIXELS, pour que l'arc reste un arc sur une
// fenêtre qui n'est pas carrée, et se déplie d'abord de l'écrasement vertical :
// la rotation se fait dans le plan du sol, puis s'y réécrase. L'étirement
// vertical, lui, suit l'inclinaison qui glisse.
constexpr std::string_view kSpinBody = R"GLSL(
in vec2 v_uv;

uniform sampler2D u_source;
uniform vec2 u_size;
uniform vec2 u_pivot;
uniform float u_arc;
uniform float u_stretch;
uniform float u_squash;

out vec4 frag_color;

void main() {
    vec2 d = (v_uv - u_pivot) * u_size;
    d.y /= u_squash;
    vec3 sum = vec3(0.0);
    for (int i = 0; i < kSpinTaps; ++i) {
        float t = float(i) / float(kSpinTaps - 1) - 0.5;
        float a = u_arc * t;
        float s = sin(a);
        float co = cos(a);
        vec2 q = vec2(d.x * co - d.y * s, d.x * s + d.y * co);
        q.y *= u_squash * (1.0 + u_stretch * t);
        sum += texture(u_source, u_pivot + q / u_size).rgb;
    }
    frag_color = vec4(sum / float(kSpinTaps), 1.0);
}
)GLSL";

// Un flottant écrit pour GLSL : neuf chiffres significatifs rendent exactement
// le même flottant, et la forme `#` garde le point décimal.
std::string Glsl(float value) { return std::format("{:#.9g}", value); }

std::string Glsl(const std::array<float, 3>& value) {
  return std::format("vec3({}, {}, {})", Glsl(value[0]), Glsl(value[1]), Glsl(value[2]));
}

std::string Glsl(const std::array<float, 2>& value) {
  return std::format("vec2({}, {})", Glsl(value[0]), Glsl(value[1]));
}

// Les constantes des deux passes, telles que `post_fx.h` les tient.
std::string SharedConstants() {
  std::string text = "#version 330 core\n";
  const auto line = [&text](std::string_view type, std::string_view name,
                            const std::string& value) {
    text += std::format("const {} {} = {};\n", type, name, value);
  };
  line("vec3", "kLuminance", Glsl(post_fx::kLuminance));
  line("float", "kAberrationScale", Glsl(post_fx::kAberrationScale));
  line("float", "kSharpenGain", Glsl(post_fx::kSharpenGain));
  line("float", "kTemperatureStep", Glsl(post_fx::kTemperatureStep));
  line("vec3", "kSepiaTint", Glsl(post_fx::kSepiaTint));
  line("vec3", "kColorblindTint", Glsl(post_fx::kColorblindTint));
  line("float", "kVignetteClear", Glsl(post_fx::kVignetteClear));
  line("float", "kVignetteFull", Glsl(post_fx::kVignetteFull));
  line("float", "kGrainSwing", Glsl(post_fx::kGrainSwing));
  line("vec2", "kNoiseAxis", Glsl(post_fx::kNoiseAxis));
  line("float", "kNoiseScale", Glsl(post_fx::kNoiseScale));
  line("float", "kFxaaEdgeFloor", Glsl(post_fx::kFxaaEdgeFloor));
  line("float", "kFxaaEdgeShare", Glsl(post_fx::kFxaaEdgeShare));
  line("float", "kFxaaReduceShare", Glsl(post_fx::kFxaaReduceShare));
  line("float", "kFxaaReduceFloor", Glsl(post_fx::kFxaaReduceFloor));
  line("float", "kFxaaSpanMost", Glsl(post_fx::kFxaaSpanMost));
  line("float", "kFxaaNearTap", Glsl(post_fx::kFxaaNearTap));
  line("float", "kFxaaFarTap", Glsl(post_fx::kFxaaFarTap));
  line("float", "kFxaaEndTap", Glsl(post_fx::kFxaaEndTap));
  line("int", "kSpinTaps", std::to_string(spin_blur::kTaps));
  return text;
}

constexpr int kFullScreenVertices = 3;

// Une texture de couleur : huit bits par canal, filtrée LINÉAIREMENT et bornée
// aux bords — c'est l'échantillonneur des deux passes, et ce qui agrandit ou
// réduit la scène quand son échelle n'est pas un.
unsigned int CreateColorTexture(int width, int height) {
  unsigned int texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);
  return texture;
}

void DeleteTexture(unsigned int& texture) {
  if (texture != 0) {
    glDeleteTextures(1, &texture);
    texture = 0;
  }
}

void DeleteRenderbuffer(unsigned int& buffer) {
  if (buffer != 0) {
    glDeleteRenderbuffers(1, &buffer);
    buffer = 0;
  }
}

void DeleteFramebuffer(unsigned int& framebuffer) {
  if (framebuffer != 0) {
    glDeleteFramebuffers(1, &framebuffer);
    framebuffer = 0;
  }
}

bool Complete() { return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE; }

// Une image intermédiaire entre deux passes, préparée à cette taille : gardée
// si elle l'a déjà, refaite sinon. Faux quand la carte graphique la refuse.
bool EnsurePassTarget(unsigned int& framebuffer, unsigned int& color, int& target_width,
                      int& target_height, int width, int height) {
  if (framebuffer != 0 && width == target_width && height == target_height) {
    return true;
  }
  DeleteFramebuffer(framebuffer);
  DeleteTexture(color);
  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  color = CreateColorTexture(width, height);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
  const bool complete = Complete();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (!complete) {
    DeleteFramebuffer(framebuffer);
    DeleteTexture(color);
    target_width = 0;
    target_height = 0;
    return false;
  }
  target_width = width;
  target_height = height;
  return true;
}

// La zone d'affichage en place : origine, puis taille.
struct Viewport {
  std::array<GLint, 4> box{};
  [[nodiscard]] int width() const noexcept { return box[2]; }
  [[nodiscard]] int height() const noexcept { return box[3]; }
};

}  // namespace

ScenePostProcess::ScenePostProcess(ScenePostProcess&& other) noexcept { *this = std::move(other); }

ScenePostProcess& ScenePostProcess::operator=(ScenePostProcess&& other) noexcept {
  if (this != &other) {
    Release();
    color_program_ = std::move(other.color_program_);
    fxaa_program_ = std::move(other.fxaa_program_);
    spin_program_ = std::move(other.spin_program_);
    vertex_array_ = std::exchange(other.vertex_array_, 0);
    scene_framebuffer_ = std::exchange(other.scene_framebuffer_, 0);
    scene_color_texture_ = std::exchange(other.scene_color_texture_, 0);
    scene_color_buffer_ = std::exchange(other.scene_color_buffer_, 0);
    scene_depth_buffer_ = std::exchange(other.scene_depth_buffer_, 0);
    resolve_framebuffer_ = std::exchange(other.resolve_framebuffer_, 0);
    resolved_color_ = std::exchange(other.resolved_color_, 0);
    scene_width_ = std::exchange(other.scene_width_, 0);
    scene_height_ = std::exchange(other.scene_height_, 0);
    scene_samples_ = std::exchange(other.scene_samples_, 0);
    chain_framebuffer_ = std::exchange(other.chain_framebuffer_, 0);
    chain_color_ = std::exchange(other.chain_color_, 0);
    chain_width_ = std::exchange(other.chain_width_, 0);
    chain_height_ = std::exchange(other.chain_height_, 0);
    spin_framebuffer_ = std::exchange(other.spin_framebuffer_, 0);
    spin_color_ = std::exchange(other.spin_color_, 0);
    spin_width_ = std::exchange(other.spin_width_, 0);
    spin_height_ = std::exchange(other.spin_height_, 0);
    max_samples_ = std::exchange(other.max_samples_, 0);
    max_size_ = std::exchange(other.max_size_, 0);
    active_ = std::exchange(other.active_, false);
  }
  return *this;
}

ScenePostProcess::~ScenePostProcess() { Release(); }

void ScenePostProcess::ReleaseScene() noexcept {
  DeleteFramebuffer(scene_framebuffer_);
  DeleteTexture(scene_color_texture_);
  DeleteRenderbuffer(scene_color_buffer_);
  DeleteRenderbuffer(scene_depth_buffer_);
  DeleteFramebuffer(resolve_framebuffer_);
  DeleteTexture(resolved_color_);
  scene_width_ = 0;
  scene_height_ = 0;
  scene_samples_ = 0;
}

void ScenePostProcess::Release() noexcept {
  ReleaseScene();
  DeleteFramebuffer(chain_framebuffer_);
  DeleteTexture(chain_color_);
  chain_width_ = 0;
  chain_height_ = 0;
  DeleteFramebuffer(spin_framebuffer_);
  DeleteTexture(spin_color_);
  spin_width_ = 0;
  spin_height_ = 0;
  if (vertex_array_ != 0) {
    glDeleteVertexArrays(1, &vertex_array_);
    vertex_array_ = 0;
  }
  active_ = false;
}

core::Result<ScenePostProcess> ScenePostProcess::Create() {
  const std::string constants = SharedConstants();
  auto color = GlProgram::Link(kVertexShader, constants + std::string(kColorBody));
  if (!color) {
    return std::unexpected(color.error());
  }
  auto fxaa = GlProgram::Link(kVertexShader, constants + std::string(kFxaaBody));
  if (!fxaa) {
    return std::unexpected(fxaa.error());
  }
  auto spin = GlProgram::Link(kVertexShader, constants + std::string(kSpinBody));
  if (!spin) {
    return std::unexpected(spin.error());
  }
  ScenePostProcess post;
  post.color_program_ = std::move(*color);
  post.fxaa_program_ = std::move(*fxaa);
  post.spin_program_ = std::move(*spin);
  // ⚠ UN TABLEAU DE SOMMETS VIDE, MAIS IL EN FAUT UN : le profil « core »
  // refuse de dessiner sans, même quand le nuanceur ne lit aucun attribut.
  glGenVertexArrays(1, &post.vertex_array_);
  glGetIntegerv(GL_MAX_SAMPLES, &post.max_samples_);
  GLint texture_size = 0;
  GLint renderbuffer_size = 0;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &texture_size);
  glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &renderbuffer_size);
  post.max_size_ = std::min(texture_size, renderbuffer_size);
  return post;
}

bool ScenePostProcess::EnsureScene(int width, int height, int samples) {
  if (scene_framebuffer_ != 0 && width == scene_width_ && height == scene_height_ &&
      samples == scene_samples_) {
    return true;
  }
  ReleaseScene();
  glGenFramebuffers(1, &scene_framebuffer_);
  glBindFramebuffer(GL_FRAMEBUFFER, scene_framebuffer_);
  glGenRenderbuffers(1, &scene_depth_buffer_);
  glBindRenderbuffer(GL_RENDERBUFFER, scene_depth_buffer_);
  if (samples > 1) {
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width, height);
    glGenRenderbuffers(1, &scene_color_buffer_);
    glBindRenderbuffer(GL_RENDERBUFFER, scene_color_buffer_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                              scene_color_buffer_);
  } else {
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    scene_color_texture_ = CreateColorTexture(width, height);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           scene_color_texture_, 0);
  }
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  // La profondeur ET le stencil, qui porte la silhouette des corps : voir
  // `SpriteRenderer::BodySilhouette`.
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                            scene_depth_buffer_);
  bool complete = Complete();
  if (complete && samples > 1) {
    // La résolution : les échantillons ramenés à un pixel, dans une texture que
    // les passes savent lire.
    glGenFramebuffers(1, &resolve_framebuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, resolve_framebuffer_);
    resolved_color_ = CreateColorTexture(width, height);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resolved_color_, 0);
    complete = Complete();
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (!complete) {
    ReleaseScene();
    return false;
  }
  scene_width_ = width;
  scene_height_ = height;
  scene_samples_ = samples;
  return true;
}

bool ScenePostProcess::EnsureChain(int width, int height) {
  return EnsurePassTarget(chain_framebuffer_, chain_color_, chain_width_, chain_height_, width,
                          height);
}

bool ScenePostProcess::EnsureSpin(int width, int height) {
  return EnsurePassTarget(spin_framebuffer_, spin_color_, spin_width_, spin_height_, width, height);
}

bool ScenePostProcess::Begin(int output_width, int output_height, const SceneTargetLook& look,
                             bool passes_wanted) {
  active_ = false;
  const int samples = look.samples > 1 ? std::min(look.samples, max_samples_) : 0;
  const bool scaled = look.render_scale != 1.0F;
  if (output_width <= 0 || output_height <= 0 || vertex_array_ == 0 ||
      (!passes_wanted && samples <= 1 && !scaled)) {
    return false;
  }
  const auto scaled_side = [this, &look](int side) {
    const auto wanted = static_cast<int>(std::lround(static_cast<float>(side) * look.render_scale));
    return std::clamp(wanted, 1, std::max(1, max_size_));
  };
  const int width = scaled_side(output_width);
  const int height = scaled_side(output_height);
  if (!EnsureScene(width, height, samples)) {
    return false;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, scene_framebuffer_);
  glViewport(0, 0, width, height);
  active_ = true;
  return true;
}

void ScenePostProcess::DrawFullScreen(unsigned int source) const {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, source);
  glBindVertexArray(vertex_array_);
  glDrawArrays(GL_TRIANGLES, 0, kFullScreenVertices);
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void ScenePostProcess::DrawSpin(unsigned int source, const SpinBlurFrame& spin) const {
  spin_program_.Use();
  spin_program_.SetInt("u_source", 0);
  spin_program_.SetVector2(
      "u_size", glm::vec2(static_cast<float>(scene_width_), static_cast<float>(scene_height_)));
  spin_program_.SetVector2("u_pivot", glm::vec2(spin.pivot_uv[0], spin.pivot_uv[1]));
  spin_program_.SetFloat("u_arc", spin.arc_radians);
  spin_program_.SetFloat("u_stretch", spin.stretch);
  spin_program_.SetFloat("u_squash", std::max(spin.squash, spin_blur::kSquashFloor));
  DrawFullScreen(source);
}

void ScenePostProcess::Present(const PostFxSettings& settings, std::uint32_t now_ms,
                               const SpinBlurFrame& spin) {
  if (!active_) {
    return;
  }
  active_ = false;

  // LA SORTIE : le tampon et la zone d'affichage que l'appelant a liés.
  GLint output_framebuffer = 0;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &output_framebuffer);
  Viewport output;
  glGetIntegerv(GL_VIEWPORT, output.box.data());
  const auto output_id = static_cast<GLuint>(output_framebuffer);

  // LA SOURCE : la scène, ses échantillons d'abord ramenés à un par pixel.
  unsigned int source_framebuffer = scene_framebuffer_;
  unsigned int source_texture = scene_color_texture_;
  if (scene_samples_ > 1) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, scene_framebuffer_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolve_framebuffer_);
    glBlitFramebuffer(0, 0, scene_width_, scene_height_, 0, 0, scene_width_, scene_height_,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    source_framebuffer = resolve_framebuffer_;
    source_texture = resolved_color_;
  }

  const bool same_size = scene_width_ == output.width() && scene_height_ == output.height();
  // LE FLOU DE ROTATION PASSE EN PREMIER. Seul, il peint droit dans la sortie ;
  // suivi du post-traitement, dans une image à la taille de la scène que la
  // passe de couleur relit. ⚠ CETTE IMAGE NE SE CRÉE QU'AU PREMIER FLOU : sans
  // rotation, rien ne change du chemin ni du coût.
  const bool spinning =
      spin.active() && (!settings.enabled || EnsureSpin(scene_width_, scene_height_));
  if (!settings.enabled && !spinning) {
    // SANS POST-TRAITEMENT, UNE COPIE : au plus proche à la même taille,
    // linéaire quand l'échelle agrandit ou réduit.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, source_framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, output_id);
    glBlitFramebuffer(0, 0, scene_width_, scene_height_, output.box[0], output.box[1],
                      output.box[0] + output.width(), output.box[1] + output.height(),
                      GL_COLOR_BUFFER_BIT, same_size ? GL_NEAREST : GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, output_id);
    return;
  }

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  // ⚠ SANS MÉLANGE : chaque passe REMPLACE l'image, elle ne se pose pas dessus.
  glDisable(GL_BLEND);
  const auto restore = [&output]() {
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glViewport(output.box[0], output.box[1], output.width(), output.height());
  };

  if (spinning) {
    if (!settings.enabled) {
      glBindFramebuffer(GL_FRAMEBUFFER, output_id);
      DrawSpin(source_texture, spin);
      restore();
      return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, spin_framebuffer_);
    glViewport(0, 0, scene_width_, scene_height_);
    DrawSpin(source_texture, spin);
    source_texture = spin_color_;
  }

  // L'anticrénelage lit l'image étalonnée : la passe de couleur écrit alors dans
  // l'image intermédiaire, à la taille de la sortie.
  const bool chained = settings.fxaa && EnsureChain(output.width(), output.height());
  if (chained) {
    glBindFramebuffer(GL_FRAMEBUFFER, chain_framebuffer_);
    glViewport(0, 0, output.width(), output.height());
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER, output_id);
    glViewport(output.box[0], output.box[1], output.width(), output.height());
  }
  const ColorPassConstants constants =
      ColorPassConstantsOf(settings, now_ms, scene_width_, scene_height_);
  color_program_.Use();
  color_program_.SetInt("u_source", 0);
  color_program_.SetVector4("u_grade", glm::vec4(constants.grade[0], constants.grade[1],
                                                 constants.grade[2], constants.grade[3]));
  color_program_.SetVector4("u_tone", glm::vec4(constants.tone[0], constants.tone[1],
                                                constants.tone[2], constants.tone[3]));
  color_program_.SetVector4("u_effects", glm::vec4(constants.effects[0], constants.effects[1],
                                                   constants.effects[2], constants.effects[3]));
  color_program_.SetVector2("u_texel", glm::vec2(constants.texel[0], constants.texel[1]));
  DrawFullScreen(source_texture);

  if (chained) {
    glBindFramebuffer(GL_FRAMEBUFFER, output_id);
    glViewport(output.box[0], output.box[1], output.width(), output.height());
    fxaa_program_.Use();
    fxaa_program_.SetInt("u_source", 0);
    fxaa_program_.SetVector2("u_texel", glm::vec2(1.0F / static_cast<float>(output.width()),
                                                  1.0F / static_cast<float>(output.height())));
    fxaa_program_.SetFloat("u_strength", std::clamp(settings.fxaa_strength, kEffectRange.lowest,
                                                    kEffectRange.highest));
    DrawFullScreen(chain_color_);
  }
  restore();
}

}  // namespace ro::render
