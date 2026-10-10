#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU DISQUE AU SOL, et les effets qui l'emploient par le sous-type 26.
//
// Le bâtisseur reçoit une image, un octet, et UNE seule piste vive — le
// bâtisseur éteint les trois autres.
//
// C'est le cercle magique des cartes. Un effet en pose plusieurs, l'un sur
// l'autre : `EF_MAP_MAGICZONE3` en pose trois, et celui du milieu porte l'octet
// quatorze, le seul que le mouvement fasse tourner à REBOURS. Le cercle entier
// tient dans ce contraste.
//
// ⚠ L'IMAGE VIENT DU SITE D'APPEL, PAS DU BÂTISSEUR (ADR 0019). Celui-ci ne
// connaît aucun nom de fichier : il charge ce qu'on lui passe. C'est pourquoi la
// table porte un chemin par appel et non un par identifiant.
//
// ⚠ ET L'OCTET DÉCIDE DE CINQ VALEURS D'UN COUP — le style du nœud, le rayon,
// l'azimut, l'alpha et la hauteur —, sans règle commune : le client les écrit
// branche par branche, et deux octets voisins n'ont rien à voir. Les déduire
// l'un de l'autre donnerait six disques de la même taille.

// LA CASE DE VARIANTE, où le semis range l'octet du site pour que le mouvement
// et la figure l'y relisent. Voir `AdvanceGroundDiscTracks`.
inline constexpr std::size_t kSlotOfGroundDiscVariant = 4;

// LA CASE DE HAUTEUR, que la figure lit comme le décalage vertical du disque.
inline constexpr std::size_t kSlotOfGroundDiscHeight = 0;

// UN DISQUE QU'UN EFFET POSE : ce que son site d'appel passe au bâtisseur.
struct EffectGroundDiscPattern {
  std::uint32_t effect_id = 0;
  // L'étiquette du bâtisseur du client qui pose ce disque.
  std::string_view client_function;
  std::string_view texture;
  std::uint8_t builder_argument = 0;
};

// Tous les disques connus, par identifiant d'effet.
[[nodiscard]] std::span<const EffectGroundDiscPattern> EffectGroundDiscPatternTable();

// TOUS les disques d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectGroundDiscPattern> EffectGroundDiscPatternsFor(
    std::uint32_t effect_id);

// CE QU'UN OCTET POSE. Rendu d'un bloc parce que le client le décide d'un bloc.
struct EffectGroundDiscSeed {
  float radius_at_anchor = 0.0F;
  std::int16_t azimuth_degrees = 0;
  std::int16_t alpha = 0;
  // La hauteur du disque au-dessus de son ancre, NÉGATIVE dans le repère du
  // client, qui descend.
  float height = 0.0F;
  // Le champ de style du NŒUD, que la figure relit. Cent dix, cent onze ou cent
  // douze : trois valeurs voisines et trois teintes distinctes.
  std::int32_t node_style = 0;
};

// LE RAYON QUE L'INSTANCE IMPOSE, quand elle en porte un.
//
// ⚠ DEUX OCTETS SUR SEPT LE LISENT, et c'est le seul endroit où le semis d'un
// disque dépend d'autre chose que de son octet : les octets quatre et quatorze
// prennent le rayon rangé dans l'instance d'effet s'il est STRICTEMENT positif,
// et quarante sinon. Aucun des six identifiants connus ne l'y range — leurs cas
// n'écrivent pas ce champ —, si bien que quarante est ce qu'ils obtiennent tous
// ; le paramètre reste ouvert plutôt que d'être effacé, parce que le client le
// lit.
[[nodiscard]] EffectGroundDiscSeed EffectGroundDiscSeedFor(std::uint8_t builder_argument,
                                                           float radius_from_instance = 0.0F);

// LES QUATRE PISTES D'UN DISQUE : une seule vive, et rien de tiré au sort.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectGroundDiscTracks(
    const EffectGroundDiscPattern& pattern, float radius_from_instance = 0.0F);

// LE NŒUD qu'un patron décrit, piste semée, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectGroundDiscNode(const EffectGroundDiscPattern& pattern,
                                                       glm::vec3 anchor,
                                                       std::int32_t effect_life_ticks,
                                                       float radius_from_instance = 0.0F);

}  // namespace ro::render
