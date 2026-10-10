#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE L'ÉVENTAIL DE SPRITES, sous-type 167.
//
// UN SEUL bâtisseur et TROIS sites, un par identifiant. Chacun lui passe un
// octet, et cet octet choisit le sprite et la teinte.
//
// ⚠ LE BÂTISSEUR SÈME DEUX FOIS, aux âges ZÉRO et UN de l'effet : sa garde est
// « âge inférieur ou égal à un », pas « âge nul ». Deux nœuds de quatre pistes
// font donc HUIT sprites, et le second éventail est tourné d'un demi-tour. Ne
// semer qu'au premier tic en montrerait la moitié.
inline constexpr std::int32_t kSpriteFanLastSowTick = 1;

// LE PAS ANGULAIRE entre deux pistes, et de combien l'âge de l'effet décale
// l'éventail entier.
//
// ⚠ QUARANTE-CINQ FOIS QUATRE FONT CENT QUATRE-VINGTS : le second éventail
// tombe exactement entre les branches du premier, ce qui donne les huit
// directions. Un autre décalage les superposerait deux à deux.
inline constexpr std::int16_t kSpriteFanDegreesPerTrack = 45;
inline constexpr std::int16_t kSpriteFanTracksPerTurn = 4;

// SUR QUEL CERCLE une piste naît, et à quelle hauteur, DANS LE REPÈRE DU CLIENT
// où l'ancre d'une piste est tenue.
//
// ⚠ LE REPÈRE DU CLIENT DESCEND : son moins cinq est un CINQ AU-DESSUS du nœud.
// L'ancre ne passe dans notre repère, par `S`, qu'à la composition des poses ;
// la poser telle quelle dans le nôtre mettrait l'éventail sous le nœud et
// tournerait ses branches en miroir.
inline constexpr float kSpriteFanSpawnRadius = 5.0F;
inline constexpr float kSpriteFanSpawnHeight = -5.0F;

// DE COMBIEN UNE PISTE S'ÉCARTE par tic.
inline constexpr float kSpriteFanSpreadStep = 0.3F;

// TOUS LES COMBIEN DE TICS l'image avance. Le bâtisseur pose deux, puis quatre
// sur la même case : seul le quatre compte.
inline constexpr std::int32_t kSpriteFanFrameHoldTicks = 4;

// UN ÉVENTAIL qu'un effet pose.
struct EffectSpriteFanPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  // Le couple .spr/.act SANS extension — les deux portent le même nom, et les
  // séparer inviterait à en désaccorder un.
  std::string_view sprite_base;
  // LA MARQUE DE TEINTE que l'octet du site choisit.
  std::uint8_t color_mark = 0;
};

[[nodiscard]] std::span<const EffectSpriteFanPattern> EffectSpriteFanPatternTable();

[[nodiscard]] std::span<const EffectSpriteFanPattern> EffectSpriteFanPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, toutes vives, tournées selon l'âge de l'effet.
// Leurs ancres sont dans le repère du client.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSpriteFanTracks(
    const EffectSpriteFanPattern& pattern, std::int32_t effect_age_ticks);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectSpriteFanNode(const EffectSpriteFanPattern& pattern,
                                                      glm::vec3 anchor,
                                                      std::int32_t effect_life_ticks,
                                                      std::int32_t effect_age_ticks);

}  // namespace ro::render
