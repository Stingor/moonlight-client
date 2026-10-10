#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LES QUATRE ANNEAUX : le semis et le mouvement du sous-type 41.
//
// CINQ identifiants — 231, 247, 248, 259 et 260 —, DEUX bâtisseurs : l'un pour
// quatre d'entre eux, l'autre pour le cinquième.
//
// ⚠ SA FIGURE EST DÉJÀ PORTÉE : c'est la jupe d'anneau, `Ring_0`. Ce qui
// manquait n'était pas un dessinateur mais un MOUVEMENT.
//
// ⚠ ET LES QUATRE PISTES NE PARTENT PAS ENSEMBLE. Le premier bâtisseur leur
// donne des compteurs de ZÉRO, TRENTE, SOIXANTE et QUATRE-VINGT-DIX : elles
// s'ouvrent l'une après l'autre, et c'est ce décalage qui fait la vague. Les
// poser au même compteur les ferait s'ouvrir d'un bloc.

inline constexpr std::int32_t kRingQuartetTicksPerTrack = 30;
inline constexpr std::int16_t kRingQuartetDegreesPerTrack = 90;
inline constexpr std::int16_t kRingQuartetFullSweep = 360;

// LA LONGUEUR QUE LE RUBAN VISE — et le mouvement la fait monter en sinus.
inline constexpr float kRingQuartetReach = 120.0F;

// CE QUE LE SECOND BÂTISSEUR CHANGE : ses quatre pistes partent DÉJÀ ouvertes,
// au deux-centième tic, et leur inclinaison DÉCROÎT d'une piste à l'autre.
inline constexpr std::int32_t kRingQuartetOpenAtOnceTick = 200;
inline constexpr std::int16_t kRingQuartetFirstTilt = 89;
inline constexpr std::int16_t kRingQuartetSecondBuilderTilt = 83;

// UN QUATUOR D'ANNEAUX qu'un effet pose.
struct EffectRingQuartetPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // LE COMPTEUR DE DÉPART de la première piste : zéro pour le premier
  // bâtisseur, deux cents pour le second.
  std::int32_t first_tick = 0;
  // DE COMBIEN LE COMPTEUR AVANCE d'une piste à la suivante — trente, ou zéro
  // pour le second bâtisseur, dont les quatre partent ensemble.
  std::int32_t ticks_per_track = kRingQuartetTicksPerTrack;
  std::int16_t alpha = 50;
  // LE RAYON DE LA PREMIÈRE PISTE ; chacune gagne un demi.
  float first_radius = 2.0F;
  std::int16_t first_tilt = kRingQuartetFirstTilt;
  // ⚠ LE SECOND BÂTISSEUR FAIT DÉCROÎTRE L'INCLINAISON d'une piste à l'autre,
  // le premier la garde. Un degré par piste, et cela suffit à décaler les
  // quatre jupes.
  std::int16_t tilt_per_track = 0;
  // LE STYLE, que le mouvement lit pour savoir si l'alpha tombe.
  std::int16_t style = 0;
  // 🔴 LE NUMÉRO DE VARIANTE DU RUBAN, que le bâtisseur écrit en flottant dans
  // le champ qu'un sprite lirait comme son échelle. La primitive de ruban le
  // TRONQUE en entier et y choisit sa teinte et son mélange : cinq ajoute du
  // blanc, quatre du bleu. Ce n'est pas une échelle — la jupe n'en lit aucune.
  float trail_variant = 4.0F;
};

[[nodiscard]] std::span<const EffectRingQuartetPattern> EffectRingQuartetPatternTable();

[[nodiscard]] std::span<const EffectRingQuartetPattern> EffectRingQuartetPatternsFor(
    std::uint32_t effect_id);

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRingQuartetTracks(
    const EffectRingQuartetPattern& pattern);

[[nodiscard]] PooledEffectNode SowEffectRingQuartetNode(const EffectRingQuartetPattern& pattern,
                                                        glm::vec3 anchor,
                                                        std::int32_t effect_life_ticks);

// ---------------------------------------------------------------------------
// LE MOUVEMENT
// ---------------------------------------------------------------------------

// ENTRE QUELS TICS LA JUPE S'OUVRE, et de quelle courbe.
//
// ⚠ C'EST UN SINUS DE ZÉRO À QUATRE-VINGT-DIX DEGRÉS, un degré par tic : la
// jupe s'ouvre vite puis ralentit, et s'arrête net à sa pleine longueur. Une
// rampe droite la ferait pousser d'un trait.
inline constexpr std::int32_t kRingQuartetOpensAtTick = 200;
inline constexpr std::int32_t kRingQuartetOpenedAtTick = 290;

// QUAND L'ALPHA TOMBE — et seulement pour le style UN.
inline constexpr std::int32_t kRingQuartetFadeAfterTick = 250;
inline constexpr std::int16_t kRingQuartetStyleThatFades = 1;

// QUAND LA JUPE SE REFERME, et de combien par tic.
inline constexpr std::int32_t kRingQuartetClosesAfterTick = 800;
inline constexpr float kRingQuartetClosePerTick = 1.0F;

// LE MOUVEMENT DU SOUS-TYPE 41.
//
// ⚠ LES VINGT ET UNE LONGUEURS REÇOIVENT TOUTES LA MÊME VALEUR. Le client
// recalcule la sienne à chaque case sans jamais lire l'index : la jupe est
// UNIFORME, et lui donner un profil par segment inventerait une forme qu'il
// n'a pas.
void AdvanceRingQuartetTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

}  // namespace ro::render
