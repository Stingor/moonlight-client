#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA FAMILLE `Ring_0` : beaucoup de bâtisseurs, UNE figure.
//
// ⚠ CETTE TABLE N'EST PAS CELLE D'UN BÂTISSEUR MAIS CELLE D'UN DESSINATEUR, et
// c'est ce qui la distingue de ses voisines. Les sous-types 23, 24, 31, 41, 50,
// 57, 60, 63, 108, 113 et 139 tombent tous sur `Ring_0` — mesuré dans la table
// des sous-types du client, pas supposé d'après les noms. Ils partagent donc
// leur figure, leur mouvement et leur forme de nœud : ce qui les sépare tient
// ENTIÈREMENT dans les quatre pistes qu'ils sèment.
//
// Une table par bâtisseur en donnerait une vingtaine qui ne diraient que cela.
// Celle-ci porte les pistes telles que le relevé les rend, et rien d'autre.
//
// ⚠ TROIS BÂTISSEURS DE CETTE MÊME FAMILLE ONT LEUR PROPRE TABLE, et ce n'est
// pas une incohérence : ceux des sous-types 30, 23 et 31 sèment par une RÈGLE —
// un style qui choisit, un rang qui se déduit — et non par quatre blocs de
// valeurs. Les aplatir ici perdrait la règle, qui est ce
// qu'on a mesuré de plus précieux chez eux.

// CE QU'UNE PISTE PORTE QUAND ELLE NE SUIT PERSONNE. Un rang valide est
// toujours positif ; moins un ne désigne aucune piste, et rien d'autre.
inline constexpr std::int8_t kSeededRingTrackDrawsAlone = -1;

// UNE PISTE, telle que son bâtisseur la sème.
//
// Les champs sont ceux que le mouvement entretient et que la figure lit ; les
// longueurs de segment n'y sont pas, parce qu'elles naissent nulles — c'est le
// mouvement qui donne au ruban son aire.
struct EffectSeededRingTrack {
  bool active = false;
  std::int16_t alpha = 0;
  std::int16_t style = 0;
  std::int16_t degrees = 0;
  // ⚠ L'AZIMUT SE TIRE AU SORT chez plusieurs, et c'est une MESURE : le
  // bâtisseur écrit « rand() % 360 » pour que ses anneaux ne se superposent pas
  // d'un lancer à l'autre. `azimuth_degrees` en est la borne basse et
  // `azimuth_high` la haute ; quand les deux sont égales, l'azimut est fixe.
  //
  // Le refuser faute d'être un nombre laissait sans anneau la moitié de cette
  // famille — dont EF_HEAL4, dont deux pistes sur quatre le tirent.
  std::int16_t azimuth_degrees = 0;
  std::int16_t azimuth_high = 0;
  std::int16_t tilt_degrees = 0;
  // ⚠ NÉGATIF, ET C'EST VOULU chez plusieurs : une piste qui part en retard
  // attend que son compteur remonte à zéro avant de bouger. C'est ce qui
  // échelonne quatre couronnes qui, sinon, partiraient ensemble.
  std::int32_t ticks = 0;
  // ⚠ CE N'EST PAS UNE HAUTEUR : la figure la compare à des flottants exacts.
  // Voir `effect_track_seed.h`.
  float lift_key = 0.0F;
  float radius_at_anchor = 0.0F;
  // ⚠ ET CES DEUX-LÀ SE TIRENT, chez certains. L'idiome est celui de l'azimut
  // ci-dessus : la borne haute vaut la basse quand la valeur est fixe, et le
  // générateur n'avance alors pas son tirage. `EF_HEAL3` tire la portée de son
  // anneau entre quarante et cinquante-neuf, là où `EF_HEAL2`, sorti du même
  // bâtisseur, la pose à quarante tout court.
  float lift_key_high = 0.0F;
  float radius_at_anchor_high = 0.0F;
  // ⚠ UNE PISTE REPREND PARFOIS LE TIRAGE D'UNE AUTRE, ET CE N'EST PAS TIRER
  // DANS LE MÊME INTERVALLE. Les deux anneaux d'`EF_HEAL3` partagent une seule
  // épaisseur tirée, le second étant le premier plus deux dixièmes : leur écart
  // est donc CONSTANT. Deux tirages indépendants dans [4,5 ; 5,0] et
  // [4,7 ; 5,2] donneraient un écart allant de moins trois dixièmes à plus
  // sept, c'est-à-dire deux anneaux qui se croisent une fois sur deux.
  //
  // Le rang est celui d'une piste DÉJÀ SEMÉE — toujours inférieur au sien — et
  // `kSeededRingTrackDrawsAlone` dit qu'il n'y a personne à suivre.
  std::int8_t follows_track = kSeededRingTrackDrawsAlone;
  float lift_key_plus = 0.0F;
  float radius_at_anchor_plus = 0.0F;
  // 🔴 LE BÂTISSEUR RECOPIE LA CLÉ DANS LA QUEUE, où le mouvement de l'éventail
  // qui s'ouvre relit la base de sa clé à chaque tic. Sans elle la base vaut
  // zéro, et l'éventail ne mesure plus qu'un dixième de sa taille. Voir
  // `kRadialSpreadKeyBaseTailSlot`.
  bool lift_key_seeds_tail = false;
  // LE NOMBRE DE VALEURS de la clé et de la portée tirées : voir
  // `EffectNodeRange::values`. Zéro : fixe. ⚠ EN QUEUE : la table se pose par
  // position.
  std::int32_t lift_key_values = 0;
  std::int32_t radius_at_anchor_values = 0;
  // LA MARQUE DES VINGT ET UNE CASES de la piste, la même pour toutes. Le
  // mouvement des groupes radiaux y lit la vitesse de rotation, la montée de
  // l'alpha et la règle de longueur de chaque segment : un deux fait tourner
  // par moitiés et clignoter quarante-cinq tics, un un tourne le plus vite et
  // tient la colonne pleine. Zéro : l'onde ordinaire. ⚠ EN QUEUE.
  std::uint8_t slot_mark = 0;
};

// Toutes les pistes semées, par blocs de quatre.
[[nodiscard]] std::span<const EffectSeededRingTrack> EffectSeededRingTrackTable();

// UN ANNEAU QU'UN EFFET SÈME.
struct EffectSeededRingPattern {
  std::uint32_t effect_id = 0;
  // LA FONCTION DU CLIENT qui pose cet anneau, par son nom.
  std::string_view client_function;
  std::string_view texture;
  // Le rang de la PREMIÈRE de ses quatre pistes dans la table ci-dessus.
  std::uint16_t first_track = 0;
  // ⚠ LA FORME EST PAR PATRON, PAS PAR TABLE. Ces bâtisseurs partagent leur
  // dessinateur mais PAS leur mouvement — le répartiteur de mouvement donne à
  // chaque sous-type sa propre cible. Une forme commune ferait bouger les uns
  // comme les autres, ce qui est la faute que cette table a déjà commise une
  // fois. Le générateur la pose depuis le relevé des mouvements.
  PooledNodeShape shape = PooledNodeShape::kRingTracks;
  // Le numéro que la primitive de ruban lit pour y prendre sa couleur.
  std::uint8_t trail_variant = 0;
  // ⚠ QUAND cet effet pose CET anneau-ci. Un effet en pose souvent deux, à deux
  // tics différents et sous deux textures — EF_HEAL4 ouvre le sien au tic zéro
  // et un second au tic un —, et n'en garder qu'un laissait le soin à moitié.
  EffectNodeSchedule schedule;
  // LE PLANCHER QUE LE BÂTISSEUR IMPOSE À LA VIE DE SON INSTANCE, zéro quand il
  // n'y touche pas. Non nul, il RELÈVE une vie plus courte, et la vie relevée
  // DÉCIDE : si elle ne dépasse pas `kSeededRingLitLifeTicks`, les quatre
  // pistes naissent éteintes et partent de cinq tics en cinq tics — elles
  // renaissent alors et montent en alpha au lieu de ne faire que pâlir. ⚠ EN
  // QUEUE : la table se pose par position.
  std::int32_t instance_life_floor_ticks = 0;
};

// LA VIE D'INSTANCE au-delà de laquelle un bâtisseur à plancher sème ses pistes
// allumées, et le retard que chaque piste prend sur la précédente en deçà.
inline constexpr std::int32_t kSeededRingLitLifeTicks = 60;
inline constexpr std::int32_t kSeededRingDelayTicksPerRank = -5;

// Tous les anneaux connus, par identifiant d'effet puis par ordre de semis.
[[nodiscard]] std::span<const EffectSeededRingPattern> EffectSeededRingPatternTable();

// TOUS les anneaux de cette famille pour un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectSeededRingPattern> EffectSeededRingPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un patron, recopiées de la table — et TIRÉES quand la
// table donne des bornes. Le générateur avance d'un tirage par piste tirée.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSeededRingTracks(
    const EffectSeededRingPattern& pattern, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
//
// 🔴 `effect_life_ticks` EST CELLE DE L'INSTANCE, ET LE NŒUD LA PREND ENTIÈRE.
// Les onze bâtisseurs de cette famille donnent tous au nœud la durée de vie de
// l'instance, et c'est bien une DURÉE : le mouvement des murs de verre tient
// l'instance pour « encore en vie » tant que son compteur diffère de cette
// valeur.
//
// ⚠ LE DÉFAUT DU CONSTRUCTEUR NE TIENT PAS LIEU DE RÉPONSE. Cent tics font une
// seconde et demie : la colonne du portail s'éteignait là, sous les yeux du
// joueur, pendant que les anneaux qui pulsent — qui, eux, reçoivent déjà la
// durée de leur effet — tenaient les vingt-trois secondes du mur. Les deux
// figures paraissaient interverties, et ce n'était que cette ligne.
[[nodiscard]] PooledEffectNode SowEffectSeededRingNode(const EffectSeededRingPattern& pattern,
                                                       glm::vec3 ground_position,
                                                       std::uint32_t& generator,
                                                       std::int32_t effect_life_ticks);

}  // namespace ro::render
