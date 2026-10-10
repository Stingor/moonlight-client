#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU TOURBILLON DE TETRA VORTEX, sous-type 140.
//
// Un seul cas appelle ce bâtisseur — partagé par les effets 804 et 970 à 973 —
// en lui passant toujours QUATRE. Le bâtisseur a une seconde branche pour un octet
// qui ne vaudrait pas quatre ; aucun site ne la prend, et le générateur refuse un
// site qui la prendrait plutôt que de la porter à l'aveugle.
//
// ⚠ CE N'EST PAS LE SEUL NŒUD DU CAS : le même cas appelle ensuite sept fois
// un autre bâtisseur, dont le sous-type 137 est porté à part — voir
// `effect_spiral_column_seed.h`.
//
// ⚠ QUATRE NŒUDS, UN TOUS LES VINGT-CINQ TICS. Le client rappelle le bâtisseur
// à chaque tic et ne sème que si l'âge de l'instance est au plus quatre-vingts ET
// multiple de vingt-cinq : aux tics 0, 25, 50 et 75. Chaque nœud rejoue son
// animation depuis la première
// image : le tourbillon PULSE quatre fois.
//
// ⚠ ET IL NAÎT AU POINT VISÉ, pas sur le lanceur. Ses coordonnées sont ABSOLUES
// (le dessinateur passe `1` au traceur) et le bâtisseur les recopie du vecteur
// que le gestionnaire de coup envoie par la commande 14 juste après avoir fait
// naître l'effet — la place de l'acteur touché. Sans ce point, le client
// dessinerait au vecteur de création, et nous ne semons rien.

// QUAND LE BÂTISSEUR SÈME : de zéro à quatre-vingts, tous les vingt-cinq tics.
inline constexpr EffectNodeSchedule kTetraVortexSchedule = {0, 80, 25, 0};

// LE MOT DE RENDU écrit en dur : le mélange propre de
// chaque quad. Voir `effect_render_word.h`.
inline constexpr std::uint32_t kTetraVortexRenderWord = 5;

// LES HUIT IMAGES, dans l'ordre où le bâtisseur les charge. La marque zéro de
// la piste est leur index.
inline constexpr std::array<std::string_view, 8> kTetraVortexFrames = {
    "data\\texture\\effect\\tv-1.bmp", "data\\texture\\effect\\tv-2.bmp",
    "data\\texture\\effect\\tv-3.bmp", "data\\texture\\effect\\tv-4.bmp",
    "data\\texture\\effect\\tv-5.bmp", "data\\texture\\effect\\tv-6.bmp",
    "data\\texture\\effect\\tv-7.bmp", "data\\texture\\effect\\tv-8.bmp",
};

// LE RAYON DU CERCLE où les quatre coins se posent. Le client prend un rayon
// porté par l'instance s'il est positif, trente sinon. Ce rayon d'instance ne
// s'écrit que par une commande que le gestionnaire de coup n'envoie pas, et
// l'instance ne le pose pas à sa création. Trente est donc la seule valeur
// qu'on puisse écrire sans supposer — voir aussi `effect_scrolling_band_seed.cpp`.
inline constexpr float kTetraVortexRadius = 30.0F;

// DE COMBIEN LE TOURBILLON SE TIENT AU-DESSUS DU POINT VISÉ. Le client écrit
// l'ordonnée visée moins 35 ; son ordonnée DESCEND, si bien qu'une
// soustraction ÉLÈVE — chez nous, il faut donc AJOUTER.
inline constexpr float kTetraVortexLiftAboveTarget = 35.0F;

// L'ALPHA DE NAISSANCE : le tourbillon paraît d'un
// coup, et c'est le mouvement qui l'éteint.
inline constexpr std::int16_t kTetraVortexBirthAlpha = 200;

// L'ANGLE DU PREMIER COIN et le jeu d'écarts « cerf-volant », ceux de la
// branche à quatre.
inline constexpr std::int16_t kTetraVortexFirstCornerDegrees = 120;
inline constexpr float kTetraVortexShapeKey = 1.0F;

// LA PARURE QUE LA FIGURE LIT, choisie par l'IDENTIFIANT de l'effet :
//   970 → 20, 971 → 18, 972 → 17, 973 → 19.
//
// ⚠ L'EFFET 804 NE PASSE PAR AUCUN CAS, et le bâtisseur n'écrit alors RIEN :
// l'octet garde ce qu'avait la mémoire du nœud. Le constructeur de nœud ne le
// pose pas, et le bloc est alloué sans remise à zéro. La valeur du client est
// donc INDÉTERMINÉE. Nous posons zéro —
// celle d'une mémoire neuve —, qui dessine UNE couche blanche sans échanger les
// coins ; `EffectTetraVortexTintIsWritten` le dit.
[[nodiscard]] std::uint8_t EffectTetraVortexTintFor(std::uint32_t effect_id);
[[nodiscard]] bool EffectTetraVortexTintIsWritten(std::uint32_t effect_id);

// UN SITE qui appelle le bâtisseur.
struct EffectTetraVortexPattern {
  std::uint32_t effect_id = 0;
  // Le nom de la fonction du client.
  std::string_view client_function;
};

// Tous les sites, par identifiant croissant.
[[nodiscard]] std::span<const EffectTetraVortexPattern> EffectTetraVortexPatternTable();

// Les sites d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectTetraVortexPattern> EffectTetraVortexPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, dont une seule est vive.
//
// `target_position` est le point visé dans NOTRE repère.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedTetraVortexTracks(
    std::uint32_t effect_id, glm::vec3 target_position);

// LE NŒUD que le bâtisseur pose à un tic de semis, ou rien sans point visé.
//
// Le nœud naît sur l'instance, donc sur le porteur (à un écart nul) : c'est
// `owner_position`. Il n'y
// sert qu'à trier le lot, la figure étant posée en absolu.
//
// ⚠ LA VIE EST CELLE DE L'INSTANCE, et elle
// court depuis la naissance du nœud : un nœud semé au tic 75 vit autant que celui
// du tic 0.
[[nodiscard]] std::optional<PooledEffectNode> SowTetraVortexNode(
    std::uint32_t effect_id, glm::vec3 owner_position,
    const std::optional<glm::vec3>& target_position, std::int32_t effect_life_ticks);

}  // namespace ro::render
