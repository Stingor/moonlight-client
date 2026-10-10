#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU CARRÉ TEINTÉ, et les douze effets qui l'emploient par le
// sous-type 47.
//
// Un seul bâtisseur, décliné sur les dix valeurs de style que ses sites
// d'appel emploient. Comme
// celui de l'arc, il n'allume QU'UNE piste sur quatre.
//
// ⚠ LE SEMIS RECOUPE LA FIGURE SANS QU'ON LE LUI DEMANDE, et c'est la meilleure
// preuve qu'on ait des deux. Le seul style qui pose une sous-marque est
// exactement celui dont la marque de dessin est neuf — la seule marque qui
// délègue à une sous-marque. Et la clé de décalage ne prend sa valeur
// déclenchante que pour un style, celui dont le rayon est le plus large. Ni le
// semis ni la figure ne savent l'un de l'autre.

// CE QU'UN STYLE SÈME dans l'unique piste vive du nœud. Tout le reste est
// commun aux dix : l'angle de départ, l'alpha, le creux et la texture.
struct EffectTintedQuadStyleSeed {
  std::uint8_t style = 0;
  // Le rayon du cercle où les quatre coins se posent. C'est le seul champ que
  // tous les styles écrivent, et le seul qui les sépare vraiment.
  float radius_at_anchor = 0.0F;
  // La marque que la FIGURE lit pour choisir sa teinte et l'ordre de ses coins.
  std::uint8_t draw_mark = 0;
  // La sous-marque, que la seule marque neuf consulte. ZÉRO veut dire ABSENTE,
  // et le client n'a pas de branche pour elle : voir `effect_tinted_quad.h`.
  std::uint8_t sub_mark = 0;
  // La clé qui pousse le carré de côté quand elle vaut dix. Un seul style la
  // pose ; les autres laissent la case à zéro, ce qui ne pousse rien.
  float nudge_key = 0.0F;

  // LA TEXTURE QUE LE STYLE CHOISIT LUI-MÊME, tirée au sort dans cette tranche.
  //
  // ⚠ TROIS STYLES SUR DIX NE LISENT PAS L'ARGUMENT DE LEUR SITE D'APPEL. Le
  // bâtisseur choisit alors sa texture dans un `switch` sur le style — deux
  // mélodies, trois gemmes, ou huit sceaux — et l'argument du site devient du
  // bruit. C'est pourquoi les trois sites concernés y passent une table et non
  // un chemin : ils n'avaient rien à passer.
  //
  // Un compte NUL veut dire que le style lit bien son site d'appel, et c'est le
  // cas des sept autres. Voir `EffectTintedQuadStyleTextures`.
  std::uint16_t first_texture = 0;
  std::uint16_t texture_count = 0;

  // ⚠ UN STYLE ALLUME LES QUATRE PISTES au lieu d'une. Le bâtisseur les éteint
  // toutes, rallume la première — ce que tous font —, puis rallume aussi les
  // trois autres pour ce style-là, en posant leur alpha à zéro et RIEN de plus :
  // ni ancre, ni rayon, ni angle. C'est leur mouvement qui les remplit.
  bool lights_every_track = false;
};

// LES TEXTURES ENTRE LESQUELLES UN STYLE TIRE, ou un span VIDE quand il prend
// celle de son site d'appel.
[[nodiscard]] std::span<const std::string_view> EffectTintedQuadStyleTextures(
    const EffectTintedQuadStyleSeed& seed);

// Tous les styles connus, par valeur croissante.
[[nodiscard]] std::span<const EffectTintedQuadStyleSeed> EffectTintedQuadStyleSeedTable();

// Ce qu'un style sème, ou `nullptr` quand aucun site d'appel ne l'emploie.
[[nodiscard]] const EffectTintedQuadStyleSeed* EffectTintedQuadStyleSeedFor(std::uint8_t style);

// UN CARRÉ QU'UN EFFET SÈME : ce que son site d'appel passe au bâtisseur.
struct EffectTintedQuadNodePattern {
  std::uint32_t effect_id = 0;
  // LE NOM DU BÂTISSEUR qui pose ce carré chez le client.
  std::string_view client_function;
  std::string_view texture;
  std::uint8_t style = 0;
};

// Tous les carrés connus, par identifiant d'effet puis par ordre de semis.
[[nodiscard]] std::span<const EffectTintedQuadNodePattern> EffectTintedQuadNodePatternTable();

// TOUS les carrés d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectTintedQuadNodePattern> EffectTintedQuadNodePatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES D'UN NŒUD DE CARRÉ, dont une seule est vive.
//
// ⚠ L'ALPHA NAÎT PLEIN, à deux cents, et c'est ce qui distingue ce bâtisseur de
// celui de l'arc. Le carré se montre donc dès le premier tic, sans attendre
// qu'un mouvement le fasse monter.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectTintedQuadTracks(
    const EffectTintedQuadStyleSeed& seed, glm::vec3 anchor, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, piste semée, prêt à rejoindre la troupe.
//
// La vie vient de l'instance, comme pour l'arc : ce bâtisseur la recopie sans
// jamais la poser. L'appelant doit donc la donner.
[[nodiscard]] PooledEffectNode SowEffectTintedQuadNode(const EffectTintedQuadNodePattern& pattern,
                                                       glm::vec3 anchor,
                                                       std::int32_t effect_life_ticks,
                                                       std::uint32_t& generator);

}  // namespace ro::render
