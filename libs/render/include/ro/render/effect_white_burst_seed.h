#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA BOUFFÉE BLANCHE, second bâtisseur du sous-type 43.
//
// Quatre carrés naissent au point de l'effet, attendent un retard tiré au sort,
// puis montent sur une sphère qui grandit en s'allumant puis en s'éteignant.
//
// ⚠ NI SA FIGURE NI SON MOUVEMENT NE SONT ICI, et c'est tout l'intérêt : ce
// sont ceux de `EffectRisingParticlePattern`, portés avec le premier bâtisseur
// du sous-type. Un nœud de cette famille prend donc `PooledNodeShape::
// kRisingParticles`, et rien du côté de la composition ne change.
//
// ⚠ LE STYLE DU SITE D'APPEL CHOISIT QUATRE CHOSES À LA FOIS — la texture, la
// demi-diagonale, la marque et le retard —, posées dans quatre branches d'un
// même `switch`. Aucune ne se déduit d'une autre. Voir
// `scripts/releve_pieces_de_la_bouffee_blanche.py`.

// CE QU'UN EFFET SÈME.
struct EffectWhiteBurstPattern {
  std::uint32_t effect_id = 0;
  // L'étiquette du bâtisseur du client qui pose ce nœud.
  std::string_view client_function;
  std::string_view texture;
  // La demi-diagonale du carré, que le bâtisseur range en deuxième case.
  float half_diagonal = 0.0F;

  // LA MARQUE, qui décide de la couleur, de l'orientation, de la vitesse de
  // montée de l'alpha et de la rotation de l'azimut.
  //
  // ⚠ ELLE NE SE TIRE QUE QUAND LE TIRAGE A PLUS D'UNE ISSUE. Deux des quatre
  // styles écrivent une constante sans consulter le générateur ; en tirer un
  // quand même, fût-ce pour le jeter, décalerait les quatre tirages d'angle qui
  // suivent — et ce sont eux qui donnent sa forme à la bouffée.
  std::uint8_t lowest_mark = 0;
  std::uint8_t mark_draw_bound = 1;

  // LE RETARD, posé en NÉGATIF : tant que le compteur l'est, la piste attend.
  //
  // ⚠ LE PAS N'EST PAS TOUJOURS UN. Un style tire sur vingt-six et DOUBLE le
  // résultat : ses retards sont pairs, de zéro à cinquante, et jamais impairs.
  // Le rendre par une fourchette continue aurait étalé la bouffée deux fois
  // plus finement que le client ne le fait.
  std::int32_t shortest_delay_ticks = 0;
  std::int32_t delay_step_ticks = 1;
  std::int32_t delay_draw_bound = 1;

  // COMBIEN DE TICS CE NŒUD VIT.
  //
  // ⚠ DÉDUIT DU MOUVEMENT, faute de mieux, et dit ici pour qu'on ne le prenne
  // pas pour une valeur du client : le bâtisseur ne pose aucune vie. C'est le
  // retard le plus long que le style puisse tirer, plus le temps qu'il faut à
  // l'alpha pour monter, tenir et retomber à zéro. Il DIFFÈRE d'un style à
  // l'autre — une marque nulle monte moins haut, donc s'éteint plus tôt —, et
  // c'est pourquoi il vit dans le patron et non dans une constante de famille.
  std::int32_t life_ticks = 0;
  // COMBIEN DE BOUFFÉES L'EFFET SÈME, d'après la boucle qui entoure le
  // bâtisseur.
  //
  // 🔴 À LA NAISSANCE SEULE. Le répartiteur rappelle la boucle à chaque tic,
  // mais le bâtisseur ne bâtit qu'au tic zéro et laisse passer les suivants :
  // l'aura d'Intimidate est UNE volée de vingt bouffées. La resemer à chaque
  // tic en fait un nuage qui ne retombe jamais.
  std::int32_t sowings_at_birth = 1;
};

// Tous les nœuds de bouffée connus, par identifiant d'effet puis par ordre de
// semis.
[[nodiscard]] std::span<const EffectWhiteBurstPattern> EffectWhiteBurstPatternTable();

// TOUTES les bouffées d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectWhiteBurstPattern> EffectWhiteBurstPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES D'UNE BOUFFÉE.
//
// Le générateur avance à chaque tirage : deux nœuds semés à la suite diffèrent,
// et repartir de la même graine rejoue exactement la même figure.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectWhiteBurstTracks(
    const EffectWhiteBurstPattern& pattern, glm::vec3 anchor, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectWhiteBurstNode(const EffectWhiteBurstPattern& pattern,
                                                       glm::vec3 anchor, std::uint32_t& generator);

}  // namespace ro::render
