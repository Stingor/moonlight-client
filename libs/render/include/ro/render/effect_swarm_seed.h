#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_track_burst.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE L'ESSAIM, sous-types 155 et 170.
//
// Deux bâtisseurs, un pour l'étroit et un pour le large. Les deux écrivent
// les MÊMES champs dans le MÊME ordre, et ne diffèrent que par trois choses —
// combien de pistes ils allument, la vitesse qu'ils tirent, et la largeur du
// tirage de cap. C'est exactement le rapport qu'ont déjà leurs deux mouvements.
//
// ⚠ UN EFFET SÈME TROIS FOIS, à DIX TICS D'INTERVALLE. Le client rappelle son
// bâtisseur à chaque tic de l'instance et le garde derrière `tic < 30 &&
// tic % 10 == 0` : l'essaim part donc en trois bouffées, et non d'un coup. Tout
// semer au premier tic donnerait un tiers de la durée et trois fois la densité.
//
// ⚠ ET LE LARGE ALLUME SES QUATRE PISTES, L'ÉTROIT UNE SEULE. Son bâtisseur
// boucle sur les quatre et tire pour chacune ; l'étroit en couche trois. Un
// nœud large porte donc QUATRE essaims indépendants — seize fois la matière
// d'un nœud étroit, puisque chaque piste peint deux positions de deux quads.

// QUAND UN EFFET DE CETTE FAMILLE SÈME : aux tics zéro, dix et vingt.
inline constexpr EffectNodeSchedule kEffectSwarmSchedule{0, 20, 10};

// LEQUEL DES DEUX BÂTISSEURS un site appelle.
enum class EffectSwarmKind : std::uint8_t {
  // Sous-type 155 : une piste, vite, et un cap à quarante-cinq degrés près.
  kNarrow,
  // Sous-type 170 : quatre pistes, deux fois moins vite, et un cap n'importe où.
  kWide,
};

// CE QUE CHACUN TIRE POUR SA VITESSE, en centièmes et en millièmes.
//
// ⚠ LES DEUX PAS NE SONT PAS DU MÊME ORDRE. L'étroit tire par CENTIÈMES entre
// quatre-vingts centièmes et un, le large par MILLIÈMES entre quatre cents et
// quatre cent vingt millièmes : l'un varie de vingt-cinq pour cent, l'autre de
// cinq. Les rapprocher ferait de l'essaim large une nuée régulière.
inline constexpr float kSwarmNarrowSpeedFloor = 0.8F;
inline constexpr float kSwarmNarrowSpeedStep = 0.01F;
inline constexpr float kSwarmWideSpeedFloor = 0.4F;
inline constexpr float kSwarmWideSpeedStep = 0.001F;
inline constexpr std::int32_t kSwarmSpeedSpread = 21;

// LE RAYON DU CARRÉ, tiré lui aussi — et la marque TROIS en change les bornes.
//
// ⚠ SEUL LE LARGE REGARDE SA MARQUE ICI, et seulement pour la valeur trois :
// elle ouvre le rayon de UN à CINQ au lieu de trois à cinq. C'est la seule
// place du semis où l'octet du site décide d'autre chose que d'une teinte.
inline constexpr float kSwarmRadiusFloor = 3.0F;
inline constexpr std::int32_t kSwarmRadiusSpread = 21;
inline constexpr float kSwarmSmallRadiusFloor = 1.0F;
inline constexpr std::int32_t kSwarmSmallRadiusSpread = 41;
inline constexpr float kSwarmRadiusStep = 0.1F;
inline constexpr std::uint8_t kSwarmSmallRadiusMark = 3;

// DE COMBIEN UN CAP TIRÉ PEUT S'ÉCARTER de la direction de la cible.
//
// ⚠ CE SONT LES MÊMES NOMBRES QUE CEUX DU MOUVEMENT, et ce n'est pas un
// hasard : le semis tire ses deux premiers caps comme le mouvement tirera les
// suivants. Ils vivent pourtant à part — `EffectSwarmTiming` décrit le
// mouvement, et rien ne garantit qu'un troisième bâtisseur suivra la règle.
inline constexpr std::int32_t kSwarmNarrowHeadingSpread = 91;
inline constexpr std::int32_t kSwarmNarrowHeadingBias = 45;
inline constexpr std::int32_t kSwarmWideHeadingSpread = 360;

// L'INCLINAISON DE DÉPART DES QUATRE CASES : l'horizontale.
//
// ⚠ CENT QUATRE-VINGTS DEGRÉS, ET NON ZÉRO. Cela s'accorde avec le mouvement,
// qui tire ses cibles neuves entre cent soixante-quinze et cent quatre-vingt-
// cinq : l'essaim oscille autour du plan du sol. Les semer à zéro le ferait
// partir vers le haut et ne jamais revenir.
inline constexpr float kSwarmLevelDegrees = 180.0F;

// DE COMBIEN LES DEUX POSITIONS NAISSENT AU-DESSUS DU NŒUD.
//
// ⚠ LE CLIENT RETRANCHE CINQ, ET SON REPÈRE DESCEND : les deux positions
// naissent donc CINQ unités plus HAUT que le nœud, à hauteur de poitrine.
// Recopier le signe les enterrerait.
inline constexpr float kSwarmBirthHeight = 5.0F;

// UN NŒUD D'ESSAIM qu'un site pose.
struct EffectSwarmPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  EffectSwarmKind kind = EffectSwarmKind::kNarrow;
  // L'OCTET DU SITE, tel quel — aucun des deux bâtisseurs ne le filtre.
  std::uint8_t mark = 0;
};

[[nodiscard]] std::span<const EffectSwarmPattern> EffectSwarmPatternTable();

[[nodiscard]] std::span<const EffectSwarmPattern> EffectSwarmPatternsFor(std::uint32_t effect_id);

// LE CAP D'UN VECTEUR, en degrés entiers, à la façon du client.
//
// Le client ne prend PAS un arc
// tangente : il balaie sa table de cosinus du degré zéro au degré cent
// quatre-vingts et s'arrête au PREMIER dont le cosinus passe sous le rapport
// cherché, puis retourne le tour quand la profondeur est négative. Le résultat
// est donc arrondi VERS LE HAUT au degré, et un `atan2` arrondi autrement
// décalerait tous les caps semés d'un degré.
//
// ⚠ UN VECTEUR NUL REND ZÉRO, et ce n'est pas un choix : la division du
// client donne alors un NaN, toutes ses comparaisons sont fausses, et la boucle
// sort au premier degré.
[[nodiscard]] std::int16_t EffectSwarmHeadingDegrees(float toward_x, float toward_z);

// LES QUATRE PISTES d'un nœud, dont une ou quatre sont vives.
//
// `toward_target` est le vecteur du nœud vers la cible du sort, dans le monde.
// `render` ne connaît pas cette cible (CLAUDE.md) : l'appelant la donne, comme
// il donne la sienne à la colonne.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSwarmTracks(
    const EffectSwarmPattern& pattern, glm::vec3 toward_target, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectSwarmNode(const EffectSwarmPattern& pattern,
                                                  glm::vec3 anchor, glm::vec3 toward_target,
                                                  std::int32_t effect_life_ticks,
                                                  std::uint32_t& generator);

}  // namespace ro::render
