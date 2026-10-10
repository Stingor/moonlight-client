#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU CŒUR QUI ERRE ET DE SES ÉCLATS DE GEL, sous-type 145.
//
// UN SEUL bâtisseur et un seul cas qui l'appelle : l'effet 833. Tout est écrit
// en dur ou tiré.
//
// ⚠ LE CAS PASSE UN TROIS que le bâtisseur ne lit JAMAIS.
//
// ⚠ DES NŒUDS AU PREMIER TIC SEULEMENT : la garde est l'âge nul de l'instance.
// Aux tics suivants le bâtisseur ne fait plus que recaler l'instance sur son
// porteur, et c'est de là que le mouvement recopie la place de chaque nœud.
//
// 🔴 LE NOMBRE DE NŒUDS N'EST PAS UN PARAMÈTRE DU CAS, c'est l'ABSCISSE de
// l'écart de naissance de l'instance, tronquée et plafonnée à cinq — et rien si
// elle est nulle ou négative. Les trois lanceurs qui font naître l'effet 833
// réécrivent aussitôt cet écart par la commande 14 avec le vecteur `(2, 0, 0)` :
// à la pose de l'état 350, au rebâtissage des états et à la compétence 2054.
// DEUX NŒUDS, donc, partout où le client le fait naître.

// LE MOT DE RENDU que le bâtisseur écrit en dur. Voir
// `effect_render_word.h`.
inline constexpr std::uint32_t kThunderFreezeRenderWord = 5;

// LES DEUX IMAGES, écrites en dur et rangées dans cet ordre au tableau des
// textures du nœud.
inline constexpr std::string_view kThunderFreezeCoreImage =
    "data\\texture\\effect\\thunder_center.bmp";
inline constexpr std::string_view kThunderFreezeShardImage = "data\\texture\\effect\\freeze_a.bmp";

// CE QUE LES TROIS LANCEURS POSENT à l'abscisse de l'écart de naissance (voir
// l'en-tête), et le plafond que le bâtisseur lui applique.
inline constexpr float kThunderFreezeLaunchedCount = 2.0F;
inline constexpr std::int32_t kThunderFreezeMaxNodes = 5;

// LES ANGLES TIRÉS sur le tour : `rand() % 360`, ramené à zéro s'il était
// négatif, ce qu'un tirage du client n'est jamais.
inline constexpr std::int32_t kThunderFreezeAngleDraws = 360;

// L'ÉCART DES NŒUDS SUR LEUR CERCLE : `360·i / abscisse de l'écart de naissance`.
// ⚠ DIVISÉ PAR LE FLOTTANT DEMANDÉ, pas par le nombre plafonné : sept demandés
// donnent cinq nœuds espacés d'un septième de tour.
inline constexpr std::int32_t kThunderFreezeOrbitSpacingDegrees = 360;

// LA HAUTEUR DU CŒUR, telle que le client la range : MOINS quinze, dans son
// repère qui DESCEND — quinze au-dessus du nœud.
inline constexpr float kThunderFreezeClientHeight = -15.0F;
// Le rayon du cœur.
inline constexpr float kThunderFreezeCoreRadius = 1.0F;
// La marque qui divise le tremblement par deux.
inline constexpr std::uint8_t kThunderFreezeHalvesWander = 1;

// LES ÉCLATS, piste par piste : un retard de vingt tics de plus à chaque rang
// (−20·k), une taille de trois et un alpha de deux cents.
inline constexpr std::int32_t kThunderFreezeShardDelayStep = -20;
inline constexpr float kThunderFreezeShardBirthSize = 3.0F;
inline constexpr std::int16_t kThunderFreezeShardBirthAlpha = 200;

// UN SITE : l'effet et la fonction du client. Le bâtisseur ne lit rien du cas.
struct EffectThunderFreezePattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
};

// La table entière, triée par identifiant.
[[nodiscard]] std::span<const EffectThunderFreezePattern> EffectThunderFreezePatternTable();

[[nodiscard]] std::span<const EffectThunderFreezePattern> EffectThunderFreezePatternsFor(
    std::uint32_t effect_id);

// Combien de nœuds le bâtisseur pose pour cette abscisse de l'écart de
// naissance : tronquée, plafonnée à cinq, et zéro en deçà de un.
[[nodiscard]] std::int32_t ThunderFreezeNodeCount(float launched_count);

// LES QUATRE PISTES DU NŒUD DE RANG `node_rank`. Le générateur avance dans
// l'ordre du client : six angles du tremblement, la respiration, puis deux
// tirages par éclat — son angle propre et son cap.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedThunderFreezeTracks(
    std::int32_t node_rank, float launched_count, std::uint32_t& generator);

// LES NŒUDS, tous nés à la place du porteur, ou aucun.
//
// `owner_position` est la place du porteur au semis ; `launched_count` l'abscisse
// de l'écart de naissance — `kThunderFreezeLaunchedCount` pour les trois lanceurs
// du client ; `effect_life_ticks` la vie de l'effet, que le bâtisseur recopie.
[[nodiscard]] std::vector<PooledEffectNode> SowThunderFreezeNodes(glm::vec3 owner_position,
                                                                  float launched_count,
                                                                  std::int32_t effect_life_ticks,
                                                                  std::uint32_t& generator);

}  // namespace ro::render
