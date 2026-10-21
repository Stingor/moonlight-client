#pragma once

#include <cstdint>

// LE DESSIN ENTRE DEUX TICS : ce qui avance par pas fixes de seize millisecondes
// se peint, entre deux pas, à mi-chemin de l'état du tic PRÉCÉDENT et de celui
// du tic COURANT.
//
// 🔴 C'EST UNE INTERPOLATION, JAMAIS UNE EXTRAPOLATION : l'image ne devine pas
// le tic suivant, elle retarde d'au plus un pas. Une extrapolation se tromperait
// à chaque virage et ferait trembler ce qu'elle cherche à lisser.
//
// ⚠ LA LOGIQUE NE CHANGE PAS : le pas reste fixe, les sons, les durées de vie
// et les battements d'image comptent les mêmes tics. Seul le dessin lit le
// poids. Et le rendu ne lit aucune horloge : l'appelant compte le temps écoulé
// depuis le dernier tic et le lui passe.
namespace ro::render {

// Le poids qui peint EXACTEMENT l'état du tic courant : celui d'une image sans
// interpolation.
inline constexpr float kWholeTickWeight = 1.0F;

// LE POIDS DU TIC COURANT dans l'image, de zéro (l'état du tic précédent) à un
// (celui du tic courant) : le reste du pas déjà écoulé, rapporté au pas.
//
// ⚠ UN SANS INTERPOLATION, quoi qu'ait compté l'horloge : l'image est alors
// celle d'avant l'option, au bit près.
[[nodiscard]] float TickBlendWeight(std::uint32_t since_tick_ms, std::uint32_t tick_ms,
                                    bool blended) noexcept;

// La grandeur entre ses deux états. Au poids un, l'état courant TEL QUEL, sans
// arrondi de calcul.
[[nodiscard]] float BlendScalar(float previous, float current, float weight) noexcept;

// L'ANGLE ENTRE SES DEUX ÉTATS, en degrés, par le PLUS COURT CHEMIN : de 350° à
// 10°, il passe par 0° et non par 180°. Le résultat n'est pas ramené dans un
// tour — un angle hors de [0, 360) tourne pareil. Au poids un, l'angle courant
// TEL QUEL.
[[nodiscard]] float BlendDegrees(float previous, float current, float weight) noexcept;

}  // namespace ro::render
