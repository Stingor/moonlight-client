#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU CHAMP DISPERSÉ, et les douze effets qui l'emploient par le
// sous-type 33 — nuages, brouillard et champs d'étoiles.
//
// Un seul bâtisseur, décliné sur les quatorze valeurs de style que ses sites
// d'appel emploient.
//
// ⚠ SON SITE D'APPEL NE LUI DONNE QU'UN NOMBRE : un style. Ni texture ni taille
// — le bâtisseur choisit tout, par branche. C'est le premier de la famille dans
// ce cas, et c'est pourquoi sa table porte des textures que personne ne lui a
// passées.
//
// ⚠ ET SES QUATRE PISTES NAISSENT À ALPHA NUL. Rien ne se montre tant que le
// mouvement n'a pas tourné : c'est lui qui monte l'alpha, fait mourir chaque
// particule et la fait RENAÎTRE ailleurs. Voir `AdvanceScatteringParticleTracks`.

// CE QU'UN STYLE SÈME. Tout le reste est commun aux quatorze.
struct EffectScatterStyleSeed {
  std::uint8_t style = 0;
  // La marque que la FIGURE lit pour choisir sa teinte et l'ordre de ses coins.
  std::uint8_t draw_mark = 0;
  // La tranche de textures de ce style, dans la liste à plat.
  //
  // ⚠ PLUSIEURS, ET LE CLIENT TIRE AU SORT ENTRE ELLES : trois nuages, trois
  // brouillards, six étoiles. N'en garder qu'une ferait un champ uniforme là où
  // le client en mêle trois.
  std::uint16_t first_texture = 0;
  std::uint16_t texture_count = 0;
};

// Les textures d'un style, dans l'ordre où le bâtisseur les charge.
[[nodiscard]] std::span<const std::string_view> EffectScatterStyleTextures(
    const EffectScatterStyleSeed& seed);

// Tous les styles connus, par valeur croissante.
[[nodiscard]] std::span<const EffectScatterStyleSeed> EffectScatterStyleSeedTable();

// Ce qu'un style sème, ou `nullptr` quand aucun site d'appel ne l'emploie.
[[nodiscard]] const EffectScatterStyleSeed* EffectScatterStyleSeedFor(std::uint8_t style);

// UN CHAMP QU'UN EFFET SÈME : son site d'appel ne passe que le style.
struct EffectScatterNodePattern {
  std::uint32_t effect_id = 0;
  // LE NOM DU BÂTISSEUR qui pose ce champ chez le client.
  std::string_view client_function;
  std::uint8_t style = 0;
  // COMBIEN DE FOIS LE SITE D'APPEL SÈME CE CHAMP.
  //
  // 🔴 CE N'EST PAS UN DÉTAIL DE VOLUME. Un nœud porte QUATRE particules ; le
  // client en sème de quarante à quatre-vingts, donc de cent soixante à trois
  // cent vingt particules. Un seul nœud ne fait pas une nuée clairsemée : il ne
  // fait rien de visible du tout, et une brume de carte reste invisible.
  //
  // ⚠ IL APPARTIENT AU SITE, PAS AU STYLE : le style cinq est semé quatre-vingts
  // fois par un effet et trente par un autre.
  std::uint16_t node_count = 1;
};

// Tous les champs connus, par identifiant d'effet puis par ordre de semis.
[[nodiscard]] std::span<const EffectScatterNodePattern> EffectScatterNodePatternTable();

// TOUS les champs d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectScatterNodePattern> EffectScatterNodePatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES D'UN CHAMP, toutes vives.
//
// ⚠ CHACUNE TIRE SA PROPRE DURÉE DE VIE, et c'est ce qui échelonne la nuée : le
// compteur de tics naît au-dessus de la plus courte des vies, si bien qu'une
// particule reste invisible jusqu'à ce qu'il dépasse la sienne. Quatre durées
// tirées, quatre apparitions décalées. Les semer identiques les ferait toutes
// paraître et disparaître ensemble, ce qui se voit.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectScatterTracks(
    const EffectScatterStyleSeed& seed, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectScatterNode(const EffectScatterNodePattern& pattern,
                                                    glm::vec3 anchor,
                                                    std::int32_t effect_life_ticks,
                                                    std::uint32_t& generator);

}  // namespace ro::render
