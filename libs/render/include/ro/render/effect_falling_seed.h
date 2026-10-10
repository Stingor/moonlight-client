#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE L'OBJET QUI TOMBE, et les six effets qui l'emploient par le
// sous-type 87 — les potions et les lances qu'un joueur lance.
//
// ⚠ LE SPRITE DESCEND. Il part au-dessus de son
// porteur et s'en rapproche de trois par tic. C'est l'ADR 0017 à la lettre.
//
// ⚠ ET SES QUATRE PISTES NAISSENT VIVES MAIS NUES : seule la première porte un
// style, un rayon, une hauteur. Les trois autres sont allumées et vides, parce
// que c'est le MOUVEMENT qui les remplit, une par tic, en y faisant descendre
// l'état de la précédente. Les semer garnies donnerait quatre objets au lieu
// d'un objet et de sa traînée.

// CE QU'UN STYLE SÈME dans la première piste.
struct EffectFallingStyleSeed {
  std::uint8_t style = 0;
  // La marque que la FIGURE lit, et la sous-marque que la seule marque neuf
  // consulte. ZÉRO veut dire ABSENTE — voir `effect_tinted_quad.h`.
  std::uint8_t draw_mark = 0;
  std::uint8_t sub_mark = 0;
  float radius_at_anchor = 0.0F;
  // ⚠ DE COMBIEN L'OBJET PART AU-DESSUS de son porteur, et sur quelle étendue il
  // TIRE cette hauteur. Le client l'écrit en négatif, dans un repère qui
  // descend ; ce champ dit le SENS et non le signe.
  //
  // L'étendue vaut zéro pour quatre styles sur cinq. Le cinquième tire, et c'est
  // ce qui échelonne les trois lances que l'effet 930 lance ensemble : une
  // hauteur fixe les superposerait.
  float lift_above_owner = 0.0F;
  float lift_spread = 0.0F;
};

// Tous les styles connus, par valeur croissante.
[[nodiscard]] std::span<const EffectFallingStyleSeed> EffectFallingStyleSeedTable();

// Ce qu'un style sème, ou `nullptr` quand aucun site d'appel ne l'emploie.
[[nodiscard]] const EffectFallingStyleSeed* EffectFallingStyleSeedFor(std::uint8_t style);

// UN OBJET QU'UN EFFET LANCE : ce que son site d'appel passe au bâtisseur.
struct EffectFallingNodePattern {
  std::uint32_t effect_id = 0;
  // LA FONCTION DU CLIENT qui pose cet objet.
  std::string_view client_function;
  std::string_view texture;
  std::uint8_t style = 0;
};

// Tous les objets connus, par identifiant d'effet puis par ordre de semis.
[[nodiscard]] std::span<const EffectFallingNodePattern> EffectFallingNodePatternTable();

// TOUS les objets d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectFallingNodePattern> EffectFallingNodePatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES D'UN OBJET : une tête garnie, trois crans vides et vifs.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectFallingTracks(
    const EffectFallingStyleSeed& seed, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectFallingNode(const EffectFallingNodePattern& pattern,
                                                    glm::vec3 anchor,
                                                    std::int32_t effect_life_ticks,
                                                    std::uint32_t& generator);

}  // namespace ro::render
