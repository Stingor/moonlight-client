#pragma once

// LA TRAÎNÉE COLORÉE : l'effet 166, que Energy Coat, Overthrust, Two-Hand
// Quicken et quelques autres états font porter à un personnage.
//
// Ce n'est ni un fichier ni un semis de nœuds : l'effet ne dessine rien
// lui-même. Il tient une COULEUR — trois flottants, rouge, vert, bleu — et, à
// chaque tic de sa vie :
//
//   · il la RÉÉCRIT sur son porteur, l'alpha laissé tel quel. Le personnage
//     prend donc la teinte de l'état tant que l'effet vit ;
//   · tous les cinq tics, si le porteur BOUGE — marche, frappe, incante, est
//     touché —, il sème une COPIE figée du corps à l'endroit où il se tient,
//     de la même couleur, à cent quatre-vingts d'alpha, qui perd huit d'alpha
//     par tic et disparaît sous huit. Immobile, le porteur n'en sème aucune.
//
// D'OÙ VIENT LA COULEUR. L'état qui pose l'effet la donne aussitôt après
// l'avoir créé (voir `AfterimageBlurStateColor`). Sans état — un effet nommé
// nu par le serveur —, rien ne la donne : l'effet garde ce que sa naissance
// y avait mis, la PLACE de son porteur dans le repère du client, et chaque
// coordonnée tronquée en entier donne son octet de poids faible pour
// composante. La teinte dépend alors de l'endroit où l'effet est né.
//
// DEUX VALEURS SENTINELLES. Une couleur dont le rouge vaut −1 ne sème rien et
// fait clignoter le porteur entre un rose et le blanc ; −2, qu'aucun état ne
// pose, le teindrait d'un jaune pâle fixe.
//
// ⚠ L'EFFET NE MEURT PAS DE LUI-MÊME. Son âge revient à zéro à 9998 tics, un
// de moins que sa vie : il vit jusqu'à ce que la levée de l'état le retire.

#include <array>
#include <cstdint>
#include <optional>

#include "ro/render/actor_effect_table.h"
#include "ro/render/sprite_frame.h"

namespace ro::render {

inline constexpr std::uint32_t kAfterimageBlurEffect = 166;

// Tous les combien de tics une copie naît, et l'âge où celui de l'effet
// repart de zéro.
inline constexpr std::uint32_t kAfterimageBlurSowPeriodTicks = 5;
inline constexpr std::uint32_t kAfterimageBlurAgeWrapTicks = 9998;

// L'alpha d'une copie à sa naissance, ce qu'elle en perd par tic, et le seuil
// sous lequel elle disparaît.
inline constexpr std::uint8_t kAfterimageGhostAlpha = 180;
inline constexpr std::uint8_t kAfterimageGhostFadePerTick = 8;
// Vingt-trois tics : l'alpha descend de 180 à 4, et la mise à jour suivante,
// qui le trouve sous huit, retire la copie.
inline constexpr std::uint32_t kAfterimageGhostLifeTicks =
    (kAfterimageGhostAlpha - kAfterimageGhostFadePerTick) / kAfterimageGhostFadePerTick + 2;

// Les deux rouges sentinelles.
inline constexpr float kAfterimageBlurFlicker = -1.0F;
inline constexpr float kAfterimageBlurPale = -2.0F;

// Une traînée qui joue : sa couleur, et l'âge de l'effet en tics.
struct AfterimageBlur {
  std::array<float, 3> color = {0.0F, 0.0F, 0.0F};
  std::uint32_t age_ticks = 0;
};

// La couleur qu'un ÉTAT donne à la traînée qu'il pose, ou rien pour un état
// qui n'en pose pas. Les deux instants du client — la pose et le rebâtissage
// du sprite du joueur — donnent la même.
[[nodiscard]] std::optional<std::array<float, 3>> AfterimageBlurStateColor(
    std::uint16_t state_id) noexcept;

// La teinte du porteur, rouge-vert-bleu-alpha, une fois la traînée passée.
//
// `draw` est le tirage de ce tic, que l'appelant fournit : il ne sert qu'au
// clignotement, qui voit le blanc une fois sur quatorze.
[[nodiscard]] std::array<std::uint8_t, 4> AfterimageBlurCarrierTint(
    std::array<std::uint8_t, 4> tint, const AfterimageBlur& blur, std::uint32_t draw) noexcept;

// CE QUE LA TRAÎNÉE ÉCRIT sur son porteur à chaque tic, dans l'ordre de la
// table des effets d'acteur — alpha, rouge, vert, bleu —, `kTintUnchanged` là
// où elle n'écrit rien. C'est l'écriture que `TintAfterEffectsAt` range au
// rang de l'effet : ⚠ UN EFFET QUI TEINT, POSÉ APRÈS ELLE, LA RECOUVRE — la
// Concentration du Lord Knight sur son Two-Hand Quicken —, comme chez le
// client, qui met ses effets à jour dans l'ordre de leur naissance.
[[nodiscard]] std::array<std::int16_t, 4> AfterimageBlurWrittenTint(const AfterimageBlur& blur,
                                                                    std::uint32_t draw) noexcept;

// Vrai quand la traînée sème une copie à ce tic, le porteur jouant `motion`.
//
// ⚠ IMMOBILE, RIEN : ni l'attente, ni la garde, ni l'assise ne sèment.
[[nodiscard]] bool AfterimageBlurSowsAt(const AfterimageBlur& blur, Motion motion) noexcept;

// Vrai quand la traînée a semé une copie depuis l'âge `previous_age_ticks`,
// celui de la dernière image regardée — ou, sans image précédente, à ce tic.
//
// ⚠ UNE IMAGE PEUT SAUTER UN TIC. Le client met l'effet à jour à CHAQUE tic ;
// nous le regardons à chaque image, et une image qui tombe deux tics plus tard
// que la précédente ne voit jamais l'âge multiple de cinq qu'elle a enjambé. Ne
// tester que l'âge courant perdait ces copies-là, et la traînée s'éclaircissait
// d'autant que l'image ralentissait.
[[nodiscard]] bool AfterimageBlurSowsSince(const AfterimageBlur& blur,
                                           std::optional<std::uint32_t> previous_age_ticks,
                                           Motion motion) noexcept;

// La teinte d'une copie à sa naissance, rouge-vert-bleu-alpha. Une composante
// qui tombe sur −1 reste blanche : la copie n'en reçoit pas.
[[nodiscard]] std::array<std::uint8_t, 4> AfterimageGhostTint(const AfterimageBlur& blur) noexcept;

// L'alpha d'une copie `age_ticks` après sa naissance, ou rien quand elle a
// disparu.
[[nodiscard]] std::optional<std::uint8_t> AfterimageGhostAlphaAt(std::uint32_t age_ticks) noexcept;

}  // namespace ro::render
