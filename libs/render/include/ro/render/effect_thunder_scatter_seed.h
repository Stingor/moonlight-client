#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DES ÉTINCELLES QUI JAILLISSENT DE LA CIBLE, sous-type 45.
//
// UN SEUL bâtisseur et deux cas qui l'appellent sans argument : les effets 264
// (EF_TEIHIT2) et 275 (EF_BACKSTAP). Tout est écrit en dur ou tiré.
//
// ⚠ UN NŒUD, AU PREMIER TIC SEULEMENT : la garde est l'âge nul de l'instance.
// Aux tics suivants le bâtisseur ne fait plus que recaler l'instance sur son
// porteur, et c'est de là que le mouvement recopie la place du nœud.
//
// 🔴 LES ANCRES NAISSENT SUR LE POINT VISÉ, pas sur le nœud. Le bâtisseur les
// prend au point visé de l'instance, que la commande 14 remplit avec la
// position de la cible. Sans elle, ce champ ne tient que l'ÉCART de naissance
// de l'instance — un point près de l'origine de la carte, loin de tout : le
// carré teinté pose ses coins en absolu, et rien ne se verrait. C'est pourquoi
// le semis REFUSE un effet sans cible au lieu de la deviner.

// LE MOT DE RENDU que le bâtisseur écrit en dur. Voir `effect_render_word.h`.
inline constexpr std::uint32_t kThunderScatterRenderWord = 5;

// L'IMAGE, écrite en dur et rangée en première case des images du nœud.
inline constexpr std::string_view kThunderScatterTexture =
    "data\\texture\\effect\\thunder_center.bmp";

// CE QUE LE BÂTISSEUR TIRE pour chaque piste, dans cet ordre.
//
// Le compteur de départ : `rand() % 10 − 40`, de −40 à −31. La piste attend
// donc de trente-deux à quarante et un tics.
inline constexpr std::int32_t kThunderScatterDelayDraws = 10;
inline constexpr std::int32_t kThunderScatterDelayBase = -40;
// Le rayon du carré : `(rand() & 15) × 0,05 + 0,1`, de 0,1 à 0,85.
inline constexpr std::int32_t kThunderScatterRadiusDraws = 16;
inline constexpr float kThunderScatterRadiusStep = 0.05F;
inline constexpr float kThunderScatterRadiusBase = 0.1F;
// La vitesse : `rand() % 50 × 0,01 + 0,75`, de 0,75 à 1,24 unité par tic.
inline constexpr std::int32_t kThunderScatterSpeedDraws = 50;
inline constexpr float kThunderScatterDrawStep = 0.01F;
inline constexpr float kThunderScatterSpeedBase = 0.75F;
// La montée : `rand() % 200 × 0,01 − 1`, de −1 à +0,99 — dans le repère
// DESCENDANT du client, que le mouvement retourne. Rangée telle que le client
// la range.
inline constexpr std::int32_t kThunderScatterRiseDraws = 200;
inline constexpr float kThunderScatterRiseBase = -1.0F;

// DE COMBIEN LES ANCRES NAISSENT AU-DESSUS DU POINT VISÉ. ⚠ LE CLIENT RETRANCHE
// HUIT à l'ordonnée dans un repère qui DESCEND : la soustraction élève.
inline constexpr float kThunderScatterLiftAboveAim = 8.0F;

// LA TEINTE : la marque que le carré teinté lit sur la PREMIÈRE piste — un bleu
// (130, 130, 250), coins échangés, mélange additif.
inline constexpr std::uint8_t kThunderScatterTint = 2;

// UN SITE : l'effet et la fonction du client. Le bâtisseur ne reçoit rien.
struct EffectThunderScatterPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
};

// La table entière, triée par identifiant.
[[nodiscard]] std::span<const EffectThunderScatterPattern> EffectThunderScatterPatternTable();

[[nodiscard]] std::span<const EffectThunderScatterPattern> EffectThunderScatterPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES, toutes allumées, ancrées huit unités au-dessus du point
// visé. Le générateur avance de quatre tirages par piste, dans l'ordre du
// client : compteur, rayon, vitesse, montée.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedThunderScatterTracks(
    glm::vec3 aim_point, std::uint32_t& generator);

// LE NŒUD, né à la place du porteur, ou RIEN quand il n'y a pas de point visé.
//
// `owner_position` est la place du porteur au semis ; `effect_life_ticks` la
// vie de l'effet, que le bâtisseur recopie dans le nœud.
[[nodiscard]] std::optional<PooledEffectNode> SowThunderScatterNode(
    glm::vec3 owner_position, const std::optional<glm::vec3>& aim_point,
    std::int32_t effect_life_ticks, std::uint32_t& generator);

}  // namespace ro::render
