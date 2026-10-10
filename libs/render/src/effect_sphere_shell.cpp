#include "ro/render/effect_sphere_shell.h"

#include <cmath>
#include <cstdlib>
#include <numbers>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

[[nodiscard]] float DegreesToRadians(float degrees) {
  return degrees * std::numbers::pi_v<float> / 180.0F;
}

[[nodiscard]] std::int32_t WithinFullTurn(std::int32_t angle_degrees) {
  const std::int32_t remainder = angle_degrees % kEffectTrackFullTurnDegrees;
  return remainder < 0 ? remainder + kEffectTrackFullTurnDegrees : remainder;
}

[[nodiscard]] float Cosinus(std::int32_t angle_degrees) {
  return std::cos(DegreesToRadians(static_cast<float>(WithinFullTurn(angle_degrees))));
}

[[nodiscard]] float Sinus(std::int32_t angle_degrees) {
  return std::sin(DegreesToRadians(static_cast<float>(WithinFullTurn(angle_degrees))));
}

// LE RAYON DE LA COQUILLE VIT DANS LA CLÉ, et la case zéro la fait ENFLER.
//
// ⚠ SEULE LA TROISIÈME COQUILLE ENFLE. Le mouvement ajoute un dixième par tic à
// la case zéro passé le vingt-sixième, et seule la coquille extérieure le lit :
// le bouclier éclate par sa peau pendant que son cœur reste en place. Faire
// enfler les trois ferait grossir une boule au lieu d'ouvrir une coquille.
constexpr std::size_t kSlotOfOuterGrowth = 0;

// LE QUART DE TOUR QUI REVIENT TROIS FOIS DANS CETTE FIGURE : la longitude du
// centre d'un panneau est son azimut moins un quart de tour, l'axe de ses coins
// cette longitude moins encore un quart, et son méridien la latitude plus un
// quart. Les trois sortent du même azimut.
constexpr std::int32_t kQuarterTurnDegrees = kEffectTrackFullTurnDegrees / 4;

// LES DEUX MODES DE MÉLANGE que cette figure emploie, dans la numérotation du
// client. Voir `EffectParticleBlend` : le zéro ADDITIONNE, le un mélange par
// l'alpha de la source.
constexpr std::int16_t kBlendAdditive = 0;
constexpr std::int16_t kBlendBySourceAlpha = 1;

// LES DEUX IMAGES d'un nœud de cette figure, dans l'ordre où son bâtisseur les
// charge : celle que le site donne, puis `guardK2.tga`, écrite en dur.
constexpr std::size_t kFirstTextureIndex = 0;
constexpr std::size_t kSecondTextureIndex = 1;

// LA PROFONDEUR DU CLIENT, retournée dans la nôtre ; les deux autres axes restent.
constexpr glm::vec3 kClientDepth{1.0F, 1.0F, -1.0F};

}  // namespace

std::array<std::uint8_t, 3> EffectSphereShellColor(std::uint8_t mark, std::size_t layer) {
  switch (mark) {
    case 2:
      return kSphereShellAmber;
    case 3:
      return kSphereShellOrange;
    case 4:
      return kSphereShellPink;
    case 5:
      return kSphereShellBlue;
    default:
      return kSphereShellGreens[layer < kSphereShellLayerCount ? layer : 0];
  }
}

std::vector<EffectWorldQuad> ComposeSphereShellQuads(const EffectTrack& track, glm::vec3 anchor) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active) {
    return quads;
  }
  quads.reserve(kSphereShellQuadsPerTrack);

  const float key = track.lift_key;
  for (std::size_t row = 0; row < kSphereShellRowCount; ++row) {
    // La rangée du milieu est l'ÉQUATEUR ; les deux autres sont à un huitième
    // de tour de part et d'autre, et leurs panneaux sont plus petits d'autant.
    const auto rang = static_cast<std::int32_t>(row) - 1;
    const std::int32_t latitude = kSphereShellStepDegrees * rang;
    const std::int32_t meridien = latitude + kQuarterTurnDegrees;
    const float half = kSphereShellEquatorHalfSize - static_cast<float>(std::abs(rang));
    // L'AXE DU MÉRIDIEN se décompose en une part verticale et une part qui
    // s'éloigne de l'axe de la sphère : c'est lui qui couche les panneaux du
    // haut et du bas contre la courbure.
    const float part_verticale = Sinus(meridien);
    const float part_radiale = Cosinus(meridien);

    for (std::size_t column = 0; column < kSphereShellColumnCount; ++column) {
      const std::int32_t offset = kSphereShellStepDegrees * static_cast<std::int32_t>(column);
      const std::int32_t longitude = track.azimuth_degrees + offset - kQuarterTurnDegrees;
      const std::int32_t axe = longitude - kQuarterTurnDegrees;
      const float vers_x = Cosinus(axe);
      const float vers_z = Sinus(axe);

      // LES DEUX AXES DU PANNEAU : celui du parallèle, à plat, et celui du
      // méridien, qui monte en s'inclinant. C'est leur couple qui rend le
      // panneau TANGENT à la sphère au lieu de faire face à la caméra.
      //
      // ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : la part verticale du
      // méridien change donc de signe, et la hauteur du centre aussi. Recopier
      // les signes retournerait le bouclier sous le sol.
      const glm::vec3 parallele{vers_x, 0.0F, vers_z};
      const glm::vec3 meridienne{-part_radiale * vers_z, -part_verticale, part_radiale * vers_x};

      for (std::size_t layer_index = 0; layer_index < kSphereShellLayerCount; ++layer_index) {
        const float enflure =
            layer_index == kSphereShellOuterLayer ? track.value[kSlotOfOuterGrowth] : 0.0F;
        const float radius = key + kSphereShellRadiusGrowth[layer_index] + enflure;
        // ⚠ LA PREMIÈRE COQUILLE EST CENTRÉE DEUX UNITÉS PLUS HAUT que les deux
        // autres, et le centre des trois suit la CLÉ, jamais leur propre rayon.
        const float center_height = layer_index == 0 ? key + kSphereShellInnerCenterLift : key;

        const glm::vec3 centre{radius * Cosinus(latitude) * Cosinus(longitude),
                               center_height - radius * Sinus(latitude),
                               radius * Cosinus(latitude) * Sinus(longitude)};
        const glm::vec3 width = half * parallele;
        const glm::vec3 height = half * meridienne;

        // ⚠ LA TROISIÈME COQUILLE PORTE LA SECONDE IMAGE ET L'AUTRE MÉLANGE,
        // sauf pour la marque cinq, qui la ramène à la première image et à
        // l'addition. C'est le seul des cinq identifiants qui ne montre jamais
        // la seconde image.
        const bool uses_second_frame =
            layer_index == kSphereShellOuterLayer && track.mark[0] != kSphereShellPlainOuterMark;

        // 🔴 ET LA PROFONDEUR AUSSI, que l'on retourne ici d'un coup. L'azimut
        // est un angle du client, où la profondeur est l'opposée de la nôtre :
        // sans ce retournement le bouclier se tiendrait dans le MIROIR du regard
        // de son porteur — juste de face et de dos, faux de trois quarts.
        EffectParticleQuadRequest request;
        request.corners = {
            kClientDepth * (centre + width + height), kClientDepth * (centre - width + height),
            kClientDepth * (centre - width - height), kClientDepth * (centre + width - height)};
        request.alpha = static_cast<std::uint8_t>(track.alpha);
        request.color = EffectSphereShellColor(track.mark[0], layer_index);
        // ⚠ L'IMAGE EST RETOURNÉE : le client donne un aux deux premiers coins
        // et zéro aux deux derniers, là où le défaut fait l'inverse.
        request.v_at_a_and_b = 1.0F;
        request.v_at_c_and_d = 0.0F;
        request.blend_mode = uses_second_frame ? kBlendBySourceAlpha : kBlendAdditive;
        request.texture = uses_second_frame ? kSecondTextureIndex : kFirstTextureIndex;
        quads.push_back(ComposeEffectParticleQuad(request, anchor));
      }
    }
  }
  return quads;
}

}  // namespace ro::render
