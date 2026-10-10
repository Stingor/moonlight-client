#pragma once

#include <cstdint>

namespace ro::render {

// L'échelle du corps ENTIER d'un acteur d'après sa classe : un pour presque
// tout le monde, moins pour les BÉBÉS.
//
// Le client la multiplie à l'échelle d'écran de tous les quads de l'acteur —
// corps, tête, coiffes et le reste, décalages d'ancrage compris : trois quarts pour un bébé de
// première classe, quatre cinquièmes pour le bébé invocateur, quatre-vingt-deux centièmes pour tous
// les autres bébés. La table des classes est recopiée telle quelle, jusque
// dans ce qu'elle omet : le bébé maître des esprits n'y figure pas et garde
// sa taille.
[[nodiscard]] float BodyScaleFor(std::uint32_t job);

// ── La taille annoncée par le serveur ───────────────────────────────────────
//
// Le protocole n'a pas de champ pour elle : le serveur envoie un EFFET, et le
// client en tire une échelle qui multiplie celle de la classe. Elle ne saute
// pas d'une valeur à l'autre — elle y va par une COURBE, et c'est cette même
// échelle qui dimensionne l'ombre au sol, laquelle suit donc son porteur.

// Où va un corps rapetissé, et un corps agrandi.
inline constexpr float kSmallBodyScale = 0.5F;
inline constexpr float kBigBodyScale = 1.5F;

// Le passage d'une taille à l'autre, en PAS.
//
// Le client fait avancer un angle de QUATRE degrés par pas, de quatre-vingt-dix
// à deux cent soixante-dix, et lit le sinus de cet angle : quarante-cinq pas,
// une courbe qui démarre et s'arrête en douceur.
//
// ⚠ LE PAS N'EST PAS UNE DURÉE, et l'avoir cru en était une devinette. Le
// compteur est un OCTET de l'acteur — un pour un corps qui rapetisse, un autre
// pour un corps qui grandit — que l'effet en cours y écrit depuis sa
// propre horloge. Qui l'écrit, à quelle cadence et jusqu'où se lisent dans la
// table des effets d'acteur, et c'est elle qui fait avancer cette courbe.
inline constexpr std::int32_t kSizeChangeSteps = 45;

// L'échelle atteinte à ce pas, en allant de UN vers `target`.
//
// Au pas zéro l'échelle vaut un ; au quarante-cinquième et au-delà, la cible.
[[nodiscard]] float SizeChangeScale(float target, std::int32_t steps);

}  // namespace ro::render
