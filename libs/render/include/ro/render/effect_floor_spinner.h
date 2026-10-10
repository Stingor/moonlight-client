#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE CARRÉ QUI TOURNE À PLAT, sous-type 143 : un carré texturé couché au-dessus
// de son porteur, qui tourne sur lui-même en respirant et fait défiler ses
// images.
//
// Trois effets le posent : les trois torches de 829, la poussière de 827 et
// l'eau de 838. C'est l'OCTET de leur site qui choisit l'image — torche,
// eau ou poussière — et, par elle, tout ce qui distingue l'eau des deux autres.
//
// ⚠ IL NE FAIT PAS FACE À LA CAMÉRA. Ses quatre coins restent dans le plan du
// sol, sans aucune rotation de panneau : c'est un disque de flamme vu d'en haut,
// qui s'aplatit quand la caméra se couche.
//
// ⚠ L'EAU NE TOURNE PAS. Sa case de repos vaut un : le mouvement ne fait alors
// ni tourner le cap ni avancer la respiration, et le dessin passe au mélange
// ordinaire. Les torches et la poussière, à zéro, tournent et s'ajoutent.

// LES CASES DE LA PISTE.
//
// LA HAUTEUR DU CARRÉ, dans le repère DESCENDANT du client : moins deux, soit
// deux unités au-dessus du nœud.
inline constexpr std::size_t kFloorSpinnerSlotOfHeightDown = 0;
// L'IMAGE COURANTE, en flottant : son entier choisit la texture.
inline constexpr std::size_t kFloorSpinnerSlotOfImage = 1;
// LE REPOS : un pour l'eau, zéro sinon. Il fige la rotation ET choisit le
// mélange.
inline constexpr std::size_t kFloorSpinnerSlotOfRest = 2;

// LE MOUVEMENT : six degrés de cap et trois de respiration par tic, hors
// repos ; une image de plus tous les quatre tics, bouclée sur le compte.
inline constexpr std::int16_t kFloorSpinnerTurnPerTick = 6;
inline constexpr std::int16_t kFloorSpinnerBreathPerTick = 3;
inline constexpr std::int32_t kFloorSpinnerTicksPerImage = 4;

// LA RESPIRATION DU RAYON : entre quatre cinquièmes et une fois le rayon de la
// piste, `(sin(phase) + 1) · R/10 + 4R/5`.
inline constexpr float kFloorSpinnerSteadyPart = 0.8F;
inline constexpr float kFloorSpinnerSwingPart = 0.1F;

// LES QUATRE COINS, à un quart de tour l'un de l'autre à partir du cap.
inline constexpr std::int32_t kFloorSpinnerCornerDegrees = 90;

// LE SEMIS : l'alpha, fixe ; la hauteur ; le rayon des torches et de la
// poussière, et celui de l'eau quand son objet de carte n'en donne pas.
inline constexpr std::int16_t kFloorSpinnerAlpha = 200;
inline constexpr float kFloorSpinnerHeightDown = -2.0F;
inline constexpr float kFloorSpinnerRadius = 18.0F;
inline constexpr float kFloorSpinnerWaterRadius = 20.0F;
// L'EAU LIT SON RAYON dans le premier paramètre de son objet de carte, mais
// seulement à partir de cinq ; en deçà, vingt.
inline constexpr float kFloorSpinnerWaterRadiusFloor = 5.0F;
// ET SON CAP dans le second, tronqué, s'il tient dans un tour ; sinon
// quarante-cinq.
inline constexpr std::int16_t kFloorSpinnerWaterFallbackDegrees = 45;

// LE MOT DE RENDU que le bâtisseur écrit en dur.
inline constexpr std::uint32_t kFloorSpinnerRenderWord = 5;

// LES TROIS OCTETS DU SITE.
inline constexpr std::uint8_t kFloorSpinnerByteOfDust = 0;
inline constexpr std::uint8_t kFloorSpinnerByteOfTorch = 1;
inline constexpr std::uint8_t kFloorSpinnerByteOfWater = 2;

// UN SITE D'APPEL : l'effet, la fonction du client, l'octet, le cap que le
// site passe, et la vie de l'instance que le nœud recopie.
//
// ⚠ UN CAS PEUT APPELER PLUSIEURS FOIS : 829 pose trois torches, aux caps zéro,
// cent vingt et deux cent quarante. Elles partagent leur centre ; c'est leur cap
// qui les sépare.
struct EffectFloorSpinnerPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::uint8_t byte = 0;
  std::int16_t azimuth_degrees = 0;
  std::int32_t life_ticks = 0;
};

// La table entière, triée par identifiant, les sites d'un même effet dans
// l'ordre de leurs appels.
[[nodiscard]] std::span<const EffectFloorSpinnerPattern> EffectFloorSpinnerPatternTable();

[[nodiscard]] std::span<const EffectFloorSpinnerPattern> EffectFloorSpinnerPatternsFor(
    std::uint32_t effect_id);

// LES IMAGES QUE L'OCTET CHOISIT, dans l'ordre du défilement : treize torches
// numérotées à partir de un, cinquante et une eaux à partir de ZÉRO, neuf
// poussières à partir de un.
[[nodiscard]] std::vector<std::string> EffectFloorSpinnerTextures(std::uint8_t byte);

// LES QUATRE PISTES DU NŒUD, la première seule allumée. Le générateur avance
// d'UN tirage, la phase de respiration.
//
// `instance_first` et `instance_second` sont les deux premiers paramètres de
// l'objet de carte qui a lancé l'effet, zéro sans objet ; seule l'eau les lit.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedFloorSpinnerTracks(
    const EffectFloorSpinnerPattern& pattern, float instance_first, float instance_second,
    std::uint32_t& generator);

// LE NŒUD qu'un site décrit, né à la place du porteur.
[[nodiscard]] PooledEffectNode SowFloorSpinnerNode(const EffectFloorSpinnerPattern& pattern,
                                                   glm::vec3 owner_position, float instance_first,
                                                   float instance_second, std::uint32_t& generator);

// LE MOUVEMENT D'UN TIC, pour chaque piste allumée. `image_count` est le compte
// d'images du nœud, sur lequel l'image boucle.
void AdvanceFloorSpinnerTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               std::size_t image_count);

// L'IMAGE QUE LA PREMIÈRE PISTE MONTRE, tronquée.
[[nodiscard]] std::int32_t FloorSpinnerImageOf(const EffectTrack& track);

// LE CARRÉ D'UNE PISTE, ou rien quand elle est éteinte.
//
// ⚠ AUCUNE GARDE SUR L'ALPHA : le client peint la piste allumée quel qu'il
// soit, et le sien ne bouge jamais.
[[nodiscard]] std::optional<EffectWorldQuad> ComposeFloorSpinnerQuad(const EffectTrack& track,
                                                                     glm::vec3 node_origin);

}  // namespace ro::render
