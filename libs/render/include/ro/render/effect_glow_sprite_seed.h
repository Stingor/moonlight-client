#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_track_fwd.h"
#include "ro/render/effect_track_motion.h"

namespace ro::render {

// LE SEMIS DU SPRITE ÉCLATANT, et les quinze effets qui l'emploient par le
// sous-type 153 — des MONSTRES qui paraissent et s'effacent.
//
// Son site d'appel ne lui
// donne qu'un style ; le style choisit le monstre, et le bâtisseur charge son
// couple .spr/.act lui-même.
//
// ⚠ CE N'EST PAS UN QUAD, C'EST UN SPRITE, et c'est la première figure de la
// famille dans ce cas. Elle emprunte le chemin des acteurs — le même que les
// nuées de sprites — et non celui des quads du monde.
//
// ⚠ ET SA « TRAÎNÉE » NE FAIT QU'UN CRAN. Le dessinateur boucle sur autant de
// crans que le nœud en déclare ; le constructeur en pose UN et ce bâtisseur ne
// le change pas. Le sprite se dessine donc une fois, et la mécanique de l'image
// rémanente — portée à côté — attend les sous-types qui en déclarent plusieurs.
// Lui en donner quatre d'autorité montrerait une rémanence que le client n'a pas.

// CE QU'UN STYLE SÈME.
struct EffectGlowSpriteStyleSeed {
  std::uint8_t style = 0;
  // La marque que la figure lit. Neuf valeurs pour seize styles : plusieurs
  // monstres partagent la leur.
  std::uint8_t draw_mark = 0;
  // Le couple .spr/.act SANS extension — les deux portent le même nom, et les
  // séparer inviterait à en désaccorder un.
  std::string_view sprite_base;
};

// Tous les styles connus, par valeur croissante.
[[nodiscard]] std::span<const EffectGlowSpriteStyleSeed> EffectGlowSpriteStyleSeedTable();

// Ce qu'un style sème, ou `nullptr` quand aucun site d'appel ne l'emploie.
[[nodiscard]] const EffectGlowSpriteStyleSeed* EffectGlowSpriteStyleSeedFor(std::uint8_t style);

// UN SPRITE QU'UN EFFET FAIT PARAÎTRE : son site d'appel ne passe que le style.
struct EffectGlowSpriteNodePattern {
  std::uint32_t effect_id = 0;
  // LA FONCTION DU CLIENT qui pose ce sprite.
  std::string_view client_function;
  std::uint8_t style = 0;
};

// Tous les sprites connus, par identifiant d'effet.
[[nodiscard]] std::span<const EffectGlowSpriteNodePattern> EffectGlowSpriteNodePatternTable();

// TOUS les sprites d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectGlowSpriteNodePattern> EffectGlowSpriteNodePatternsFor(
    std::uint32_t effect_id);

// 🔴 UN MONSTRE PAR TIC, ET NON UN PAR EFFET. Le bâtisseur est rappelé à chaque
// tic et sème tant que l'âge de l'effet reste en deçà d'un seuil de son style :
// cinquante tics pour les styles zéro et un, dix pour deux à huit, trente pour
// treize et quatorze, vingt pour les autres. C'est une RONDE de dix à cinquante
// monstres, chacun posé à son écart du centre et sur le sol à cet endroit.
[[nodiscard]] std::int32_t EffectGlowSpriteSowingTicks(std::uint8_t style);

// CE QU'UN SEMIS TIRE OU LIT DE L'ÂGE, dans l'ordre du client.
//
// ⚠ L'ÉCART EST DANS LE REPÈRE DU CLIENT : `rayon · sin(cap)` sur l'abscisse,
// `rayon · cos(cap)` sur la profondeur. Le cap AVANCE avec l'âge pour la
// plupart des styles — c'est la succession des naissances qui fait le tour —,
// et se tire pour les styles zéro et un.
struct EffectGlowSpriteDraws {
  float sprite_scale = 1.0F;
  float radius = 0.0F;
  std::int32_t heading_degrees = 0;
  // TOUS LES COMBIEN DE TICS L'IMAGE CHANGE : de six à huit, quatre ou trois
  // selon le style.
  std::int32_t frame_hold_ticks = 0;
  // L'ACTION DU `.act`, direction comprise : une base par style plus un
  // tirage de huit, ou zéro sans tirage.
  std::int32_t action = 0;
};

// Les tirages d'un semis de ce style, à cet âge de l'effet. Le générateur
// avance comme celui du client : échelle ou écart d'abord, puis un tirage que
// personne ne relit, puis la cadence et l'action.
[[nodiscard]] EffectGlowSpriteDraws DrawEffectGlowSprite(std::uint8_t style,
                                                         std::int32_t effect_tick,
                                                         std::uint32_t& generator);

// LE MOT DE RENDU DU SPRITE : le style quinze se peint en ADDITION, les autres
// au mélange ordinaire.
[[nodiscard]] std::uint32_t EffectGlowSpriteRenderWord(std::uint8_t style);

// 🔴 LE HALO : chaque calque se peint une SECONDE fois, élargi d'un pixel
// d'écran sur chacun de ses côtés, même teinte, et toujours ajouté — quel que
// soit le mot de la première passe. C'est ce qui fait briller les monstres.
inline constexpr float kEffectGlowSpriteHaloScreenPixels = 1.0F;
inline constexpr std::uint32_t kEffectGlowSpriteHaloRenderWord = kEffectRenderWordOfAdditiveSprite;

// LE NŒUD qu'un patron sème à cet âge de l'effet, prêt à rejoindre la troupe.
//
// `instance_position` est la place de l'instance — celle du porteur — à ce tic ;
// le nœud naît à son écart, posé sur le sol que `world` lui donne, et n'en bouge
// plus. `effect_life_ticks` est la vie de l'instance : le nœud vit ce qu'il en
// reste.
[[nodiscard]] PooledEffectNode SowEffectGlowSpriteNode(const EffectGlowSpriteNodePattern& pattern,
                                                       glm::vec3 instance_position,
                                                       std::int32_t effect_tick,
                                                       std::int32_t effect_life_ticks,
                                                       const EffectScatterWorld& world,
                                                       std::uint32_t& generator);

}  // namespace ro::render
