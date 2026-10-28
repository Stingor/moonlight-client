#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/core/error.h"
#include "ro/render/gl_program.h"

namespace ro::render {

// LA CARTE D'OMBRE DU SOLEIL : une texture de profondeur, peinte depuis la
// lumière directionnelle de la carte, puis lue par le sol et le décor.
//
// Elle remplace l'ombre cuite dans l'alpha de la lightmap, pas la lumière posée
// de son RGB : torches et lanternes restent celles que la carte a peintes.

// Le côté de la carte, en texels.
inline constexpr int kSunShadowMapTexels = 2048;
// L'unité de texture où les nuanceurs la lisent : 0 porte la texture, 1 la
// lightmap.
inline constexpr int kSunShadowTextureUnit = 2;
// Celle où un sprite lit, en plus, la couche qui porte les acteurs : voir
// `kSunShadowSpriteFragmentGlsl`.
inline constexpr int kSunActorShadowTextureUnit = 3;

// LES DEUX COUCHES DE LA CARTE.
//
// Le sol et le décor lisent celle qui porte les silhouettes des acteurs : un
// personnage ombre le sol où il se tient. Les acteurs lisent celle qui ne les
// porte pas — un sprite plat lu à ses pieds y verrait sa propre silhouette, et
// s'ombrerait lui-même. Un acteur n'ombre donc pas un autre acteur.
enum class SunShadowLayer : std::uint8_t {
  kWorld,
  kWithActors,
};
inline constexpr std::size_t kSunShadowLayerCount = 2;

class SunShadowMap {
 public:
  SunShadowMap() = default;
  SunShadowMap(const SunShadowMap&) = delete;
  SunShadowMap& operator=(const SunShadowMap&) = delete;
  SunShadowMap(SunShadowMap&& other) noexcept;
  SunShadowMap& operator=(SunShadowMap&& other) noexcept;
  ~SunShadowMap();

  // ⚠ LAISSE LIÉ CE QU'IL A TROUVÉ LIÉ — cible de dessin, cible de lecture,
  // texture de l'unité active : la carte naît à la première image qui la
  // demande, au milieu d'une peinture qui peut viser une cible hors écran.
  [[nodiscard]] static core::Result<SunShadowMap> Create(int side_texels);

  // Dirige le dessin vers la carte, efface sa profondeur et décale celle des
  // faces dessinées, contre l'acné d'une face qui s'ombre elle-même.
  // ⚠ RETIENT les cibles de dessin et de lecture et la fenêtre en vigueur, que
  // `EndDepthPass` rétablit chacune.
  // La passe commence sur la couche du monde : sol et décor.
  void BeginDepthPass();
  // Recopie le monde dans la couche des acteurs, et y dirige le dessin : ce qui
  // suit n'est vu que du sol et du décor. ⚠ À APPELER MÊME SANS ACTEUR.
  void BeginActorLayer() const;
  void EndDepthPass() const;

  // LE RECUL DE PROFONDEUR des faces peintes, contre l'acné d'une face qui
  // s'ombre elle-même : posé par `BeginDepthPass`. Une silhouette de sprite s'en
  // passe — aucun sprite ne lit la couche où elle est peinte —, et l'en
  // dispenser garde son ombre collée aux pieds.
  void SetCasterOffset(bool on) const;

  void Bind(int unit, SunShadowLayer layer) const;

  [[nodiscard]] bool valid() const noexcept { return framebuffers_.front() != 0; }
  [[nodiscard]] int side_texels() const noexcept { return side_texels_; }

 private:
  static constexpr int kViewportValues = 4;

  std::array<unsigned int, kSunShadowLayerCount> framebuffers_{};
  std::array<unsigned int, kSunShadowLayerCount> depths_{};
  int side_texels_ = 0;
  int previous_draw_framebuffer_ = 0;
  int previous_read_framebuffer_ = 0;
  int previous_viewport_[kViewportValues] = {};
};

// L'ombre pleine : seule l'ambiante y reste, comme la lumière de la carte le dit.
inline constexpr float kFullSunShadow = 1.0F;

// Ce qu'une passe de couleur lit de l'ombre. Sans carte, rien ne s'ombre.
struct SunShadowView {
  const SunShadowMap* map = nullptr;
  glm::mat4 view_projection{1.0F};
  // Le côté d'un texel, en unités du monde : le décalage le long de la normale
  // s'y mesure.
  float texel_world = 0.0F;
  // Un : à l'ombre, seule l'ambiante reste. Zéro : aucune ombre.
  float strength = 0.0F;
  // CE QUE L'OMBRE LAISSE À UN SPRITE, canal par canal : les acteurs ne sont
  // pas éclairés par sommet, et prennent le rapport du sol plat — voir
  // `SunShadeOf`.
  glm::vec3 sprite_shade{1.0F};
  // La direction VERS l'astre qui porte l'ombre, normalisée.
  glm::vec3 toward_sun{0.0F, 1.0F, 0.0F};
};

// LE NUANCEUR DE SOMMETS projette le point vers la carte, décalé le long de sa
// normale : une pente rasante ne s'ombre pas elle-même.
inline constexpr std::string_view kSunShadowVertexGlsl = R"GLSL(
uniform mat4 u_sun_view_projection;
uniform float u_sun_normal_offset;
out vec4 v_sun_position;

void SunShadowProject(vec3 world, vec3 normal) {
    v_sun_position = u_sun_view_projection * vec4(world + normal * u_sun_normal_offset, 1.0);
}
)GLSL";

// CELUI DES FRAGMENTS rend la part du soleil que le fragment reçoit — un en
// pleine lumière —, moyennée sur un disque de lectures pour adoucir le bord.
// Hors de la carte, tout est éclairé : sa bordure vaut la profondeur la plus
// lointaine.
//
// 🔴 LE DISQUE TOURNE D'UN PIXEL À L'AUTRE. Un carré de lectures aligné sur la
// grille de la carte adoucit le bord sans effacer cette grille : un bord
// d'ombre oblique y montrait ses marches, d'autant plus longues que le soleil
// bas étire chaque texel au sol. Tourné selon un bruit propre à chaque pixel,
// le disque fond les marches en un grain fin.
inline constexpr std::string_view kSunShadowFragmentGlsl = R"GLSL(
uniform sampler2DShadow u_sun_shadow;
uniform float u_sun_shadow_strength;
uniform float u_sun_texel;
in vec4 v_sun_position;

// Les lectures, réparties dans le disque unité, et son rayon en texels.
const int kSunDiskTaps = 12;
const vec2 kSunDisk[kSunDiskTaps] = vec2[](
    vec2(-0.326, -0.406), vec2(-0.840, -0.074), vec2(-0.696, 0.457), vec2(-0.203, 0.621),
    vec2(0.962, -0.195), vec2(0.473, -0.480), vec2(0.519, 0.767), vec2(0.185, -0.893),
    vec2(0.507, 0.064), vec2(0.896, 0.412), vec2(-0.322, -0.933), vec2(-0.792, -0.598));
const float kSunDiskRadiusTexels = 2.0;
// Le bruit à gradients entrelacés : un angle par pixel, sans motif visible.
const vec3 kSunNoiseWeights = vec3(0.06711056, 0.00583715, 52.9829189);
const float kSunFullTurn = 6.2831853;

float SunVisibility() {
    if (u_sun_shadow_strength <= 0.0) {
        return 1.0;
    }
    vec3 projected = v_sun_position.xyz / v_sun_position.w * 0.5 + 0.5;
    projected.z = min(projected.z, 1.0);
    float angle = kSunFullTurn *
                  fract(kSunNoiseWeights.z * fract(dot(gl_FragCoord.xy, kSunNoiseWeights.xy)));
    mat2 spin = mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
    float reach = kSunDiskRadiusTexels * u_sun_texel;
    float lit = 0.0;
    for (int tap = 0; tap < kSunDiskTaps; ++tap) {
        vec2 offset = spin * kSunDisk[tap] * reach;
        lit += texture(u_sun_shadow, vec3(projected.xy + offset, projected.z));
    }
    return mix(1.0, lit / float(kSunDiskTaps), u_sun_shadow_strength);
}
)GLSL";

// L'OMBRE D'UN SPRITE, lue dans le nuanceur de SOMMETS : un acteur s'assombrit
// d'un seul tenant, et le même point vaut pour ses quatre coins.
//
// 🔴 MOYENNÉE SUR L'EMPRISE DE SES PIEDS, et non lue en un point. Sous un
// feuillage, l'ombre est trouée par les feuilles découpées : lue en un seul
// point, elle basculait d'un trou à une feuille au moindre pas ou au moindre
// recalage de la carte, et l'acteur entier clignotait du clair au sombre.
// Moyennée, il entre dans l'ombre tachetée par degrés.
//
// 🔴 CHAQUE LECTURE A SON POINT : une grille posée À PLAT autour des pieds,
// dans le monde, chaque point projeté et comparé à SA profondeur. Comparer
// toutes les lectures à la profondeur des seuls pieds assombrissait l'acteur en
// plein soleil : sous un soleil bas, le sol voisin côté soleil est plus proche
// de lui que les pieds, et la moitié des lectures le prenait pour une ombre.
//
// `u_sun_sprite_step` est l'écart entre deux points, en unités du monde : voir
// `kSunSpriteShadeRadius`.
inline constexpr std::string_view kSunShadowSpriteVertexGlsl = R"GLSL(
uniform mat4 u_sun_view_projection;
uniform sampler2DShadow u_sun_shadow;
uniform float u_sun_shadow_strength;
uniform float u_sun_sprite_step;

const int kSunSpriteTapRadius = 2;

float SunVisibilityAround(vec3 world) {
    if (u_sun_shadow_strength <= 0.0) {
        return 1.0;
    }
    float lit = 0.0;
    float taps = 0.0;
    for (int row = -kSunSpriteTapRadius; row <= kSunSpriteTapRadius; ++row) {
        for (int column = -kSunSpriteTapRadius; column <= kSunSpriteTapRadius; ++column) {
            vec3 point = world + vec3(float(column), 0.0, float(row)) * u_sun_sprite_step;
            vec4 projected4 = u_sun_view_projection * vec4(point, 1.0);
            vec3 projected = projected4.xyz / projected4.w * 0.5 + 0.5;
            projected.z = min(projected.z, 1.0);
            lit += textureLod(u_sun_shadow, projected, 0.0);
            taps += 1.0;
        }
    }
    return mix(1.0, lit / taps, u_sun_shadow_strength);
}
)GLSL";

// L'OMBRE DES AUTRES ACTEURS SUR UN SPRITE, lue par fragment.
//
// Chaque fragment la lit au point de son PROPRE panneau d'ombre — celui que
// le sprite dresse face au soleil dans la carte, voir `ShadowPanelBasis` —, et
// dans la couche qui porte les silhouettes des acteurs. Un panneau plat ne
// peut pas s'ombrer lui-même : seul un autre acteur, posé entre lui et le
// soleil, l'assombrit. Les jambes d'un joueur peuvent ainsi passer dans
// l'ombre d'un autre pendant que sa tête reste au soleil.
//
// Le point avance de `u_sun_actor_bias` vers l'astre : cela ne le déplace pas
// dans la carte, seulement sa profondeur, et le panneau lu sur une case de
// texels ne s'y prend pas pour un obstacle.
inline constexpr std::string_view kSunShadowSpriteFragmentGlsl = R"GLSL(
uniform mat4 u_sun_view_projection;
uniform sampler2DShadow u_sun_actor_shadow;
uniform float u_sun_shadow_strength;
uniform float u_sun_texel;
uniform vec3 u_sun_toward;
uniform float u_sun_actor_bias;

const int kSunActorTapRadius = 1;

float ActorVisibilityAt(vec3 panel_world) {
    if (u_sun_shadow_strength <= 0.0) {
        return 1.0;
    }
    vec4 projected4 = u_sun_view_projection * vec4(panel_world + u_sun_toward * u_sun_actor_bias, 1.0);
    vec3 projected = projected4.xyz / projected4.w * 0.5 + 0.5;
    projected.z = min(projected.z, 1.0);
    float lit = 0.0;
    float taps = 0.0;
    for (int row = -kSunActorTapRadius; row <= kSunActorTapRadius; ++row) {
        for (int column = -kSunActorTapRadius; column <= kSunActorTapRadius; ++column) {
            vec2 offset = vec2(float(column), float(row)) * u_sun_texel;
            lit += texture(u_sun_actor_shadow, vec3(projected.xy + offset, projected.z));
            taps += 1.0;
        }
    }
    return lit / taps;
}
)GLSL";

// L'AVANCE DU POINT LU VERS L'ASTRE, en texels de la carte, sous un astre au
// ras du sol. Elle couvre la pente du panneau sur la case lue ; plus l'astre
// monte, plus le panneau debout lui fait face par la tranche, et plus elle
// grandit, comme l'inverse du cosinus de sa hauteur.
inline constexpr float kSunActorBiasTexels = 3.0F;
// Le cosinus le plus faible dont on divise : un astre au zénith ne rendrait
// pas l'avance infinie.
inline constexpr float kSunActorBiasLowestCosine = 0.1F;

// LE RAYON DE L'EMPRISE où un sprite lit son ombre, en unités du monde : un peu
// moins d'une case, qui en vaut cinq.
inline constexpr float kSunSpriteShadeRadius = 4.0F;

// `shader` avec `sun` inséré juste après sa ligne `#version`, comme le
// brouillard : voir `WithFog`.
[[nodiscard]] std::string WithSunShadow(std::string_view shader, std::string_view sun);

// Pose les uniformes de l'ombre sur le programme EN SERVICE, et lie la couche
// `layer` de la carte à son unité — ou rien, quand la vue n'en a pas.
void SetSunShadowUniforms(const GlProgram& program, const SunShadowView& sun,
                          SunShadowLayer layer = SunShadowLayer::kWithActors);

// Les mêmes, pour un programme qui lit l'ombre par `kSunShadowSpriteVertexGlsl`.
void SetSunSpriteShadowUniforms(const GlProgram& program, const SunShadowView& sun);

}  // namespace ro::render
