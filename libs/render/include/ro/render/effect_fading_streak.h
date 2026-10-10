#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA TRAÎNÉE QUI S'EFFACE : le dessinateur du sous-type 134, la neige lancée.
//
// Il sort par la voie A
// CINQ fois par piste vive : le même quatre-coins, posé cinq fois de suite le
// long de la trajectoire, chacun plus en arrière et plus pâle que le précédent.
// C'est la traînée, et elle n'a pas d'historique : les cinq copies se calculent
// toutes du même état.
//
// ⚠ LE FLOCON AVANCE AVEC LE TEMPS, pas avec un pas. Le client multiplie la
// DISTANCE jusqu'à la cible par le compteur de tics, divisée par une clé tirée
// au semis : le flocon met donc entre quarante et soixante tics à couvrir sa
// course, quelle qu'elle soit. Un pas fixe ferait arriver les courtes trop tôt.
//
// ⚠ ET DEUX DE SES TROIS MARQUES SE LISENT SUR LA PISTE ZÉRO, pour les quatre
// pistes. Le client indexe le NŒUD et non la piste courante pour la teinte et
// pour la clé : les quatre flocons d'un nœud partagent donc la couleur et la
// vitesse du premier, alors que chacun a tiré les siennes. C'est un défaut du
// client, pas une simplification de notre part — le transcrire est ce qui garde
// la parité.

// COMBIEN DE COPIES par piste, et de combien la suivante recule.
inline constexpr std::size_t kFadingStreakCopies = 5;
inline constexpr float kFadingStreakCopyGap = 2.0F;

// CE QUE CHAQUE COPIE PERD D'ALPHA, à partir de la seconde : vingt fois cinq
// plus son rang. La première garde tout.
inline constexpr std::int16_t kFadingStreakFadeUnit = 20;
inline constexpr std::int16_t kFadingStreakFadeBase = 5;

// DE COMBIEN LE QUAD S'ÉLÈVE AU-DESSUS DU NŒUD. Le client la retranche dans
// son repère qui descend ; dans le nôtre, elle s'ajoute.
inline constexpr float kFadingStreakRise = 12.0F;

// DE COMBIEN LE RAYON RESPIRE, et la case qui porte l'angle de cette
// respiration. Aucun bâtisseur ne la pose : elle vaut zéro, et le rayon ne
// respire donc pas. L'écrire est ce qui permettra de s'en apercevoir.
inline constexpr float kFadingStreakBreath = 0.05F;
inline constexpr std::size_t kSlotOfFadingStreakBreath = 8;

// LA CASE QUI CHOISIT LA FORME DES QUATRE COINS : carré, ou l'un des deux
// losanges. Aucun bâtisseur ne la pose non plus — elle vaut zéro, le carré.
inline constexpr std::size_t kSlotOfFadingStreakShape = 10;

// LES TROIS MARQUES : l'image, la teinte, et la clé de vitesse.
inline constexpr std::size_t kMarkOfFadingStreakImage = 0;
inline constexpr std::size_t kMarkOfFadingStreakTint = 2;
inline constexpr std::size_t kMarkOfFadingStreakSpeedKey = 5;

// LA TEINTE QU'UNE MARQUE DONNE, ou rien pour une marque que le client ne nomme
// pas — la piste ne paraît alors PAS DU TOUT, son choix n'ayant pas de cas par
// défaut.
//
// ⚠ SES QUATRE TEINTES NE DIFFÈRENT QUE PAR LEUR BLANCHEUR : le bleu y est
// toujours plein, et ce sont le rouge et le vert qui montent ensemble. C'est de
// la neige, pas un arc-en-ciel.
[[nodiscard]] bool EffectFadingStreakTintFor(std::uint8_t mark, std::array<std::uint8_t, 3>& tint);

// LES QUADS D'UNE PISTE : cinq copies, ou un lot VIDE.
//
// `node_origin` est le point d'où la traînée part — l'anneau du lanceur. L'ancre
// de la piste porte la CIBLE, et c'est la distance entre les deux qui règle la
// vitesse.
//
// `shared_tint_mark` et `shared_speed_key` sont les marques de la piste ZÉRO :
// voir l'avertissement ci-dessus.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeFadingStreakQuads(
    const EffectTrack& track, std::int16_t turn_degrees, std::int16_t tilt_degrees,
    glm::vec3 node_origin, std::uint8_t shared_tint_mark, std::uint8_t shared_speed_key,
    const EffectOwnerTint& owner = {});

}  // namespace ro::render
