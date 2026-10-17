#include "ro/render/effect_typed_ribbon.h"

#include <algorithm>
#include <cmath>

#include "ro/render/effect_scene.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_trail_palette.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LA VARIANTE DE LA PREMIÈRE PASSE, écrite juste avant l'appel. Ce n'est pas une
// passe SANS teinte : c'est une passe teintée par la variante zéro.
constexpr std::uint8_t kFirstPassVariant = 0;

// CE QUE CHAQUE TYPE DONNE, relevé sur les douze branches du dessinateur.
//
// Les variantes vont de cinq à trente-deux, et ce sont des numéros de palette de
// traînée : cette famille ne demande donc AUCUNE couleur nouvelle. Le
// multiplicateur, lui, s'applique à l'alpha de la piste avant écrêtage.
struct TypedRibbonRow {
  std::uint8_t type;
  std::uint8_t trail_variant;
  std::int16_t alpha_factor;
  bool doubles_when_setting_is_off;
  bool slows_the_image;
};

constexpr std::array<TypedRibbonRow, 12> kTypedRibbonRows = {{
    {0, 17, 3, false, false},
    {1, 17, 3, false, false},
    {2, 17, 3, false, false},
    {3, 18, 3, false, false},
    {4, 19, 2, false, false},
    {5, 20, 2, false, false},
    {6, 5, 2, false, false},
    {7, 21, 2, false, false},
    {8, 22, 2, false, false},
    // ⚠ CES DEUX-LÀ NE MULTIPLIENT QUE SI LE RÉGLAGE EST ÉTEINT, et leur facteur
    // vaut alors deux. Le figer à deux les rendrait deux fois trop vifs pour qui
    // a le réglage allumé.
    {9, 30, 1, true, false},
    {10, 6, 3, false, true},
    {11, 32, 1, true, false},
}};

}  // namespace

EffectTypedRibbonPass EffectTypedRibbonPassFor(std::uint8_t type) {
  const auto found = std::ranges::find(kTypedRibbonRows, type, &TypedRibbonRow::type);
  const TypedRibbonRow& row = found == kTypedRibbonRows.end() ? kTypedRibbonRows.front() : *found;
  return {row.trail_variant, row.alpha_factor, row.doubles_when_setting_is_off,
          row.slows_the_image};
}

std::vector<EffectWorldQuad> ComposeTypedRibbonQuads(const EffectTrack& track, glm::vec3 anchor,
                                                     std::size_t image_count, bool setting_is_on) {
  std::vector<EffectWorldQuad> quads;
  // Un balayage négatif n'est pas un arc à l'envers : le client abandonne la
  // piste. Même règle que les deux autres rubans.
  if (!track.active || track.degrees < 0 || track.alpha <= 0) {
    return quads;
  }

  const EffectTypedRibbonPass passe =
      EffectTypedRibbonPassFor(static_cast<std::uint8_t>(track.style));
  const std::int16_t facteur =
      passe.doubles_when_setting_is_off && !setting_is_on ? 2 : passe.alpha_factor;
  const std::int16_t second_alpha = std::min<std::int16_t>(
      static_cast<std::int16_t>(track.alpha * facteur), kTypedRibbonAlphaCeiling);

  // L'IMAGE QUI TOURNE : les tics modulo le nombre d'images. Un nœud sans image
  // ne peut pas en choisir une, et le client diviserait par zéro.
  const auto count = static_cast<std::int32_t>(std::max<std::size_t>(image_count, 1));
  const std::int32_t ticks = std::max(track.ticks, 0);
  const std::size_t first_frame = static_cast<std::size_t>((ticks % count + count) % count);
  const std::int32_t second_ticks = passe.slows_the_image ? ticks / kTypedRibbonSlowFactor : ticks;
  const std::size_t second_frame = static_cast<std::size_t>((second_ticks % count + count) % count);

  // ⚠ LE RAYON DE BASE PORTE LE SINUS DE L'AZIMUT. C'est ce qui échelonne les
  // quatre rubans d'un même nœud : leurs azimuts sont à quatre-vingt-dix degrés
  // d'écart, donc leurs sinus valent zéro, moins un, zéro et un. Lire le rayon
  // seul les rendrait concentriques.
  const float azimuth = DegreesToRadians(static_cast<float>(track.azimuth_degrees));
  const float base = std::sin(azimuth) + track.radius_at_anchor;

  const std::int32_t sweep_degrees = track.degrees;
  const std::int32_t step = sweep_degrees / kTypedRibbonLastSegment;
  const float tilt = DegreesToRadians(static_cast<float>(track.tilt_degrees));
  const float upward = std::sin(tilt);
  const float vers_le_large = std::cos(tilt);

  const EffectTrailVariant first_tint = EffectTrailVariantFor(kFirstPassVariant);
  const EffectTrailVariant second_tint = EffectTrailVariantFor(passe.trail_variant);

  glm::vec3 previous_inner{0.0F, 0.0F, 0.0F};
  glm::vec3 previous_outer{0.0F, 0.0F, 0.0F};
  bool has_previous = false;
  std::int32_t travelled = 0;
  for (std::int32_t segment = 0; segment <= kTypedRibbonLastSegment; ++segment) {
    // LE DERNIER SEGMENT REPREND L'ANGLE DE DÉPART quand le tour est entier :
    // accumuler le pas y laisserait une fente.
    const bool referme =
        sweep_degrees == kEffectTrackFullTurnDegrees && segment == kTypedRibbonLastSegment;
    const std::int32_t angle =
        referme ? track.azimuth_degrees
                : (track.azimuth_degrees + travelled) % kEffectTrackFullTurnDegrees;
    const float theta = DegreesToRadians(static_cast<float>(angle));
    const float vers_x = std::cos(theta);
    // LA PROFONDEUR SE RETOURNE AVEC L'ORDONNÉE : le passage au nôtre est un
    // demi-tour autour de X. N'en retourner qu'une ferait de l'anneau le
    // miroir du sien, qui tournerait à l'envers vu d'en haut.
    const float vers_z = -std::sin(theta);

    const float length = track.value[static_cast<std::size_t>(segment)];
    const glm::vec3 inner = anchor + glm::vec3{base * vers_x, 0.0F, base * vers_z};
    // ⚠ LA JUPE MONTE : le client écrit son bord à une ordonnée NÉGATIVE, qui
    // élève dans son repère tourné vers le bas. Dans le nôtre, qui monte, le
    // signe s'inverse ; le recopier enterrait la jupe sous le sol.
    const float offset = length * vers_le_large + base;
    const glm::vec3 outer = anchor + glm::vec3{offset * vers_x, length * upward, offset * vers_z};

    if (has_previous) {
      const float u_before = static_cast<float>(segment - 1) / kTypedRibbonTextureSpan;
      const float u_after = static_cast<float>(segment) / kTypedRibbonTextureSpan;
      const auto place = [&](const EffectTrailVariant& tint, std::int16_t alpha,
                             std::size_t image) {
        const std::int16_t retenu = tint.forced_alpha != 0 ? tint.forced_alpha : alpha;
        if (retenu <= 0) {
          return;
        }
        EffectWorldQuad quad;
        quad.corners = {previous_inner, inner, outer, previous_outer};
        quad.texture_uv = {glm::vec2{u_before, 1.0F}, glm::vec2{u_after, 1.0F},
                           glm::vec2{u_after, 0.0F}, glm::vec2{u_before, 0.0F}};
        quad.tint = {tint.color[0], tint.color[1], tint.color[2],
                     static_cast<std::uint8_t>(retenu)};
        quad.blend_source = kEffectNodeBlendSource;
        quad.blend_destination = tint.blend_destination;
        quad.texture = image;
        quads.push_back(quad);
      };
      // ⚠ LE TYPE QUI RALENTIT SON IMAGE SAUTE LA PREMIÈRE PASSE : le client ne
      // dessine de lui que la seconde, la variante et l'alpha de son type.
      if (!passe.slows_the_image) {
        place(first_tint, track.alpha, first_frame);
      }
      place(second_tint, second_alpha, second_frame);
    }

    previous_inner = inner;
    previous_outer = outer;
    has_previous = true;
    travelled += step;
    if (travelled > sweep_degrees) {
      break;
    }
  }
  return quads;
}

}  // namespace ro::render
