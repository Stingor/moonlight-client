#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_scrolling_band.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA BANDE QUI DÉFILE, et les treize effets qui l'emploient par le
// sous-type 79 : les cascades du client.
//
// DEUX bâtisseurs et UN seul semis : même nœud, mêmes
// quatre pistes, mêmes vingt jointures espacées de quarante, même échelonnement
// en profondeur. Elles ne diffèrent que par les constantes que chacune pose.
//
// ⚠ CHACUNE SE DÉCIDE SUR DEUX OCTETS que son site d'appel lui passe, et les
// treize identifiants en épuisent le produit. Ne lier qu'un seul des deux
// rendrait la moitié des cascades identiques à l'autre moitié.
//
// ⚠ LES QUATRE PISTES NE PARTAGENT NI LEUR VITESSE, NI LEUR LARGEUR, NI LEUR
// PLAN DE PROFONDEUR. C'est là toute la cascade : quatre rideaux qui glissent à
// des allures différentes, légèrement décalés en profondeur, font une eau qui
// bouge. Les semer identiques donnerait un seul rideau plat, et quatre fois le
// même.

// LEQUEL DES DEUX BÂTISSEURS pose ce nœud. Le nom ne sert qu'à désigner : c'est
// le semis ci-dessous qui dit ce que chacun fait.
enum class EffectScrollingBandBuilder : std::uint8_t {
  kInitWaterfall,
  kBuildWaterfall,
};

// UNE CASCADE QU'UN EFFET POSE : ce que son site d'appel passe au bâtisseur.
struct EffectScrollingBandPattern {
  std::uint32_t effect_id = 0;
  // LA FONCTION DU CLIENT qui pose cette cascade.
  std::string_view client_function;
  // LES TROIS IMAGES DU JEU, dans l'ordre où le bâtisseur les charge.
  std::array<std::string_view, kScrollingBandTextures> textures;
  EffectScrollingBandBuilder builder = EffectScrollingBandBuilder::kInitWaterfall;
  // Le premier octet : la marque de plan, posée telle quelle dans les quatre
  // pistes. Un pour la bande basculée d'un quart de tour.
  std::uint8_t plane = 0;
  // ⚠ LE SECOND OCTET, ET SON SENS APPARTIENT AU BÂTISSEUR QUI LE REÇOIT
  // (ADR 0019) : `kInitWaterfall` y lit quelle LARGEUR donner à ses rideaux,
  // `kBuildWaterfall` à quelle VITESSE les faire couler. Lui
  // donner ici un nom unique en donnerait un faux à l'un des deux.
  std::uint8_t builder_argument = 0;
};

// Toutes les cascades connues, par identifiant d'effet puis par ordre de semis.
[[nodiscard]] std::span<const EffectScrollingBandPattern> EffectScrollingBandPatternTable();

// TOUTES les cascades d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectScrollingBandPattern> EffectScrollingBandPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES D'UNE CASCADE, garnies comme le bâtisseur les garnit.
//
// ⚠ AUCUN TIRAGE ICI : les deux bâtisseurs ne consultent aucun
// générateur. Une cascade rejouée deux fois est la même, et c'est ce que le
// client montre.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectScrollingBandTracks(
    const EffectScrollingBandPattern& pattern);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
//
// La vie vient de l'instance : les deux bâtisseurs la recopient sans jamais la
// poser. L'appelant doit donc la donner.
[[nodiscard]] PooledEffectNode SowEffectScrollingBandNode(const EffectScrollingBandPattern& pattern,
                                                          glm::vec3 anchor,
                                                          std::int32_t effect_life_ticks);

}  // namespace ro::render
