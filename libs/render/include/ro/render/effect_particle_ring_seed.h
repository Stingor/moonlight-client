#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// L'ANNEAU DE PARTICULES, et les treize effets qui l'emploient par le
// sous-type 31.
//
// ⚠ SES QUATRE PISTES SE DÉDUISENT DE LEUR RANG. Le client les sème dans une
// boucle : l'azimut de la piste `i` s'écrit `90 × i` et son rayon `1 + i`. Une
// seule règle, quatre valeurs.
//
// Les quatre pistes forment donc une CROIX qui s'élargit : quatre couronnes aux
// quatre points cardinaux, de plus en plus larges. Leur donner le même azimut
// les superposerait, et le même rayon les confondrait.
//
// ⚠ ET LEUR ALPHA NAÎT NUL. L'anneau ne se voit pas au tic où il est semé ;
// c'est le mouvement qui l'allume.

// CE QU'UN STYLE CHOISIT. Tout le reste du semis vaut pour tous les styles.
//
// ⚠ ET UN STYLE APPARTIENT À SON BÂTISSEUR, JAMAIS À SON NUMÉRO SEUL. Deux
// fonctions sèment ce sous-type, et le style deux ne veut pas dire la même chose
// pour l'une et pour l'autre : l'anneau de taille réduite s'y couche à
// soixante-dix degrés et se resserre, l'anneau ordinaire garde quatre-vingts
// pour tous ses styles. Chercher une graine par son seul numéro
// rendait celle du voisin — c'est l'ADR 0019, et c'est pourquoi un patron
// désigne sa graine par son RANG dans la table et non par son style.
struct EffectParticleRingStyleSeed {
  // LE BÂTISSEUR auquel ces valeurs appartiennent, par son nom de référence.
  // Il trace, et il sépare : deux graines de même style s'y distinguent.
  std::string_view client_function;
  std::uint8_t style = 0;
  // Le numéro que la primitive de ruban lit dans le nœud pour y prendre sa
  // couleur — voir `effect_trail_palette.h`. Un seul style le change.
  std::uint8_t trail_variant = 0;
  // ⚠ UN SEUL STYLE SE COUCHE PLUS BAS ET SE RESSERRE : soixante-dix degrés au
  // lieu de quatre-vingts, et une clé de quinze au lieu de vingt-cinq. C'est la
  // « taille réduite » que le client nomme lui-même, et l'aplatir sur les
  // autres lui rendrait la taille pleine.
  std::int16_t tilt_degrees = 0;
  // ⚠ CE N'EST PAS UNE HAUTEUR : la figure la compare à des flottants exacts.
  // Voir `effect_track_seed.h`.
  float lift_key = 0.0F;
  // LE MODE DE LA GERBE, que le mouvement compare à quatre-vingt-dix-neuf :
  // égal, l'anneau tourne de trois degrés par tic au lieu de cinq, s'ouvre d'un
  // dixième au lieu de trois et ne se couche pas sous trente degrés au lieu de
  // quarante. Deux bâtisseurs sur trois l'y posent ; le troisième le laisse à
  // zéro. Voir `AdvanceDualModeSprayTracks`.
  float spray_mode = 0.0F;
};

// Tous les styles connus, par valeur croissante.
[[nodiscard]] std::span<const EffectParticleRingStyleSeed> EffectParticleRingStyleSeedTable();

// La graine de ce RANG, ou `nullptr` s'il sort de la table. C'est par là qu'un
// patron trouve la sienne — voir `EffectParticleRingNodePattern::style_index`.
[[nodiscard]] const EffectParticleRingStyleSeed* EffectParticleRingStyleSeedAt(std::size_t rang);

// Ce qu'un style sème CHEZ UNE FONCTION DONNÉE, ou `nullptr`.
//
// ⚠ LA FONCTION N'EST PAS FACULTATIVE. Sans elle, deux bâtisseurs qui emploient
// le même numéro rendraient la première graine trouvée, c'est-à-dire celle de
// l'autre une fois sur deux.
[[nodiscard]] const EffectParticleRingStyleSeed* EffectParticleRingStyleSeedFor(
    std::string_view client_function, std::uint8_t style);

// UN ANNEAU QU'UN EFFET SÈME : ce que son site d'appel passe au bâtisseur.
struct EffectParticleRingNodePattern {
  std::uint32_t effect_id = 0;
  // Le nom de référence du bâtisseur qui pose cet anneau.
  std::string_view client_function;
  std::string_view texture;
  std::uint8_t style = 0;
  // LE RANG DE SA GRAINE dans la table des styles. Voir
  // `EffectParticleRingStyleSeed` : le numéro de style seul ne suffit pas à la
  // désigner dès que deux bâtisseurs sèment ce sous-type.
  std::uint8_t style_index = 0;
};

// Tous les anneaux connus, par identifiant d'effet puis par ordre de semis.
[[nodiscard]] std::span<const EffectParticleRingNodePattern> EffectParticleRingNodePatternTable();

// TOUS les anneaux de particules d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectParticleRingNodePattern> EffectParticleRingNodePatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES, semées : une croix qui s'élargit.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectParticleRingTracks(
    const EffectParticleRingStyleSeed& seed);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectParticleRingNode(
    const EffectParticleRingNodePattern& pattern, glm::vec3 ground_position);

}  // namespace ro::render
