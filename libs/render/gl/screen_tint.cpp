#include "ro/render/screen_tint.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include <glad/glad.h>

namespace ro::render {
namespace {

// AUCUN SOMMET N'EST TÉLÉVERSÉ : le triangle se déduit du rang du sommet.
//
// Trois sommets couvrant tout l'écran coûtent moins qu'un quad de deux
// triangles — pas de couture en diagonale, où le matériel échantillonne deux
// fois la même ligne de pixels — et surtout rien à garder en mémoire. Le rang 0
// donne (-1, -1), le 1 donne (3, -1), le 2 donne (-1, 3) : le triangle déborde,
// et le découpage rend exactement le rectangle de l'écran.
constexpr std::string_view kVertexShader = R"GLSL(
#version 330 core
out vec2 v_uv;

void main() {
    vec2 corner = vec2(float((gl_VertexID & 1) << 2), float((gl_VertexID & 2) << 1));
    v_uv = corner * 0.5;
    gl_Position = vec4(corner - 1.0, 0.0, 1.0);
}
)GLSL";

// ⚠ `smoothstep` DÉPLIÉ À LA MAIN, et ce n'est pas du zèle : la vignette va de
// 0,8 à 0,35 — première borne PLUS GRANDE que la seconde, ce qui inverse la
// rampe —, et GLSL déclare indéfini un `smoothstep` ainsi borné. Le pilote aurait pu
// rendre n'importe quoi, y compris le résultat attendu sur une machine et un
// écran noir sur une autre.
constexpr std::string_view kFragmentShader = R"GLSL(
#version 330 core
in vec2 v_uv;

uniform sampler2D u_scene;
uniform float u_vignette;
uniform float u_desaturate;
uniform float u_grain;
uniform float u_seconds;

out vec4 frag_color;

const vec3 kLuminance = vec3(0.299, 0.587, 0.114);
// Où la vignette commence à mordre, et où elle est au plus sombre, en distance
// au centre DANS LE CARRÉ DES COORDONNÉES DE TEXTURE. Le coin y est à 0,707 :
// la vignette n'atteint donc jamais son maximum, et c'est l'allure voulue.
const float kVignetteClear = 0.35;
const float kVignetteFull = 0.8;
// De combien le grain écarte un pixel, au réglage plein.
const float kGrainSwing = 0.15;
// Les deux nombres de la fonction de hachage, et son facteur.
const vec2 kNoiseAxis = vec2(12.9898, 78.233);
const float kNoiseScale = 43758.5453;

void main() {
    vec3 color = texture(u_scene, v_uv).rgb;

    float luma = dot(color, kLuminance);
    color = mix(color, vec3(luma), u_desaturate);

    if (u_vignette > 0.0) {
        float away = distance(v_uv, vec2(0.5));
        float edge = clamp((kVignetteFull - away) / (kVignetteFull - kVignetteClear), 0.0, 1.0);
        float shade = edge * edge * (3.0 - 2.0 * edge);
        color *= mix(1.0, shade, u_vignette);
    }

    if (u_grain > 0.0) {
        float noise = fract(sin(dot(v_uv * (u_seconds + 1.0), kNoiseAxis)) * kNoiseScale);
        color += (noise - 0.5) * u_grain * kGrainSwing;
    }

    frag_color = vec4(clamp(color, 0.0, 1.0), 1.0);
}
)GLSL";

constexpr int kFullScreenVertices = 3;

// LE GRAIN AVANCE AVEC L'HORLOGE, et il la replie à soixante-cinq secondes.
//
// ⚠ LE REPLI NE SE SUPPRIME PAS : ce compteur entre dans un `sin`, où un nombre qui grandit sans
// fin perd d'abord ses décimales puis rend un grain FIGÉ. Un écran de veille tient des heures ;
// c'est exactement le cas qui l'aurait montré.
constexpr std::uint32_t kGrainClockWrapMs = 0xFFFF;
constexpr float kMillisecondsPerSecond = 1000.0F;

}  // namespace

ScreenTint::ScreenTint(ScreenTint&& other) noexcept
    : program_(std::move(other.program_)),
      vertex_array_(other.vertex_array_),
      framebuffer_(other.framebuffer_),
      copy_(other.copy_),
      width_(other.width_),
      height_(other.height_) {
  other.vertex_array_ = 0;
  other.framebuffer_ = 0;
  other.copy_ = 0;
  other.width_ = 0;
  other.height_ = 0;
}

ScreenTint& ScreenTint::operator=(ScreenTint&& other) noexcept {
  if (this != &other) {
    Release();
    program_ = std::move(other.program_);
    vertex_array_ = other.vertex_array_;
    framebuffer_ = other.framebuffer_;
    copy_ = other.copy_;
    width_ = other.width_;
    height_ = other.height_;
    other.vertex_array_ = 0;
    other.framebuffer_ = 0;
    other.copy_ = 0;
    other.width_ = 0;
    other.height_ = 0;
  }
  return *this;
}

ScreenTint::~ScreenTint() { Release(); }

void ScreenTint::Release() noexcept {
  if (copy_ != 0) {
    glDeleteTextures(1, &copy_);
    copy_ = 0;
  }
  if (framebuffer_ != 0) {
    glDeleteFramebuffers(1, &framebuffer_);
    framebuffer_ = 0;
  }
  if (vertex_array_ != 0) {
    glDeleteVertexArrays(1, &vertex_array_);
    vertex_array_ = 0;
  }
  width_ = 0;
  height_ = 0;
}

core::Result<ScreenTint> ScreenTint::Create() {
  auto program = GlProgram::Link(kVertexShader, kFragmentShader);
  if (!program) {
    return std::unexpected(program.error());
  }

  ScreenTint tint;
  tint.program_ = std::move(*program);
  // ⚠ UN TABLEAU DE SOMMETS VIDE, MAIS IL EN FAUT UN : le profil « core »
  // refuse de dessiner sans, même quand le nuanceur ne lit aucun attribut.
  glGenVertexArrays(1, &tint.vertex_array_);
  glGenFramebuffers(1, &tint.framebuffer_);
  return tint;
}

bool ScreenTint::EnsureCopy(int width, int height) {
  if (width <= 0 || height <= 0) {
    return false;
  }
  if (copy_ != 0 && width == width_ && height == height_) {
    return true;
  }
  if (copy_ == 0) {
    glGenTextures(1, &copy_);
  }
  glBindTexture(GL_TEXTURE_2D, copy_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  // ⚠ AU PLUS PROCHE, ET SANS RÉPÉTITION : la copie se redessine à l'échelle
  // un pour un, donc rien n'est à interpoler — et au bord, la répétition irait
  // chercher la ligne d'en face.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, copy_, 0);
  const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (!complete) {
    return false;
  }
  width_ = width;
  height_ = height;
  return true;
}

void ScreenTint::Draw(const ScreenTintLook& look, int drawable_width, int drawable_height,
                      std::uint32_t now_ms) {
  if (!look.visible() || !EnsureCopy(drawable_width, drawable_height)) {
    return;
  }

  // ⚠ LA SOURCE DE LECTURE RESTE LA FENÊTRE, la cible devient la copie : les
  // deux bouts se lient séparément, et les confondre ferait transférer la copie
  // sur elle-même. C'est le même geste que `ScreenGrab::Take`.
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_);
  glBlitFramebuffer(0, 0, drawable_width, drawable_height, 0, 0, drawable_width, drawable_height,
                    GL_COLOR_BUFFER_BIT, GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  // ⚠ SANS MÉLANGE : cette passe REMPLACE l'image, elle ne se pose pas dessus.
  glDisable(GL_BLEND);

  program_.Use();
  program_.SetInt("u_scene", 0);
  program_.SetFloat("u_vignette", std::clamp(look.vignette, 0.0F, 1.0F));
  program_.SetFloat("u_desaturate", std::clamp(look.desaturate, 0.0F, 1.0F));
  program_.SetFloat("u_grain", std::clamp(look.grain, 0.0F, 1.0F));
  program_.SetFloat("u_seconds",
                    static_cast<float>(now_ms & kGrainClockWrapMs) / kMillisecondsPerSecond);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, copy_);
  glBindVertexArray(vertex_array_);
  glDrawArrays(GL_TRIANGLES, 0, kFullScreenVertices);
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);

  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);
}

}  // namespace ro::render
