#pragma once

#include <array>
#include <optional>

namespace ro::render {

// ── LA VIE D'UN NOMBRE ──────────────────────────────────────────────────────
//
// Sa taille et son opacité au fil du temps, que le joueur règle PAR TYPE de
// dégât : il naît à `birth_scale` fois sa taille finale, la rejoint d'un pas
// régulier en `birth_settle_ms`, reste opaque jusqu'à `fade_start_ms`, puis
// s'efface d'un pas régulier jusqu'à `life_ms`, où il s'éteint.
//
// 🔴 LE MOUVEMENT N'EN FAIT PAS PARTIE : tout ce qui déplace un nombre est sa
// trajectoire — voir `ArrivalDriftAt`. Le client donne à chacune de ses allures
// une vie et un chemin ; `ClientLifeOf` et `ClientRouteOf` (floating_number.h)
// les rendent, et ce sont les défauts.
//
// ⚠ EN MILLISECONDES VRAIES : c'est le temps que le joueur lit sur le curseur.
struct NumberLife {
  float birth_scale = 1.0F;
  float birth_settle_ms = 0.0F;
  float fade_start_ms = 0.0F;
  float life_ms = 1000.0F;
};

// CE QUE LE JOUEUR RÈGLE D'UN NOMBRE DE DÉGÂTS — et d'une marque, qui suit les
// mêmes curseurs.
//
// L'échelle et l'opacité sont des FACTEURS sur ce que la vie calcule à chaque
// instant : à un, elles laissent le client intact. Le chemin et la vie sont
// ceux du TYPE du nombre, que le jeu plie ici avant de composer.
struct NumberLook {
  // L'échelle, en facteur sur celle que la vie calcule à chaque instant.
  float scale = 1.0F;
  // ── LE POINT DE DÉPART ────────────────────────────────────────────────────
  //
  // De combien le nombre naît plus haut que le point où le client le fait
  // naître, en pixels de sprite et APRÈS l'échelle : il se déplace sans
  // s'étirer.
  float lift_pixels = 0.0F;
  // Et de combien il naît plus à droite — plus à gauche en négatif —, dans la
  // même unité.
  float start_x_pixels = 0.0F;
  // ── LE POINT D'ARRIVÉE ────────────────────────────────────────────────────
  //
  // Où le nombre FINIT, par rapport à son point de départ, en pixels de sprite
  // et après l'échelle — x vers la droite, y vers le haut. Il y va au fil de sa
  // vie. Voir `ArrivalDriftAt`.
  float arrival_x_pixels = 0.0F;
  float arrival_y_pixels = 0.0F;
  // LES DEUX POIGNÉES DE LA COURBE entre le départ et l'arrivée : l'écart de
  // chacune au point du chemin droit qu'elle tire — le tiers, puis les deux
  // tiers —, en pixels, y vers le haut. Nulles, le chemin est droit et parcouru
  // à vitesse égale ; égales et verticales, c'est un arc — la cloche du client
  // en est un ; opposées, un S. Elles valent même sans arrivée — le nombre fait
  // alors une boucle et revient.
  float bend_one_x_pixels = 0.0F;
  float bend_one_y_pixels = 0.0F;
  float bend_two_x_pixels = 0.0F;
  float bend_two_y_pixels = 0.0F;
  // L'écart entre deux chiffres, en pixels de sprite avant échelle : le client
  // pose le chiffre de rang i, à partir des unités, à 4 × (n − 1 − 2i), soit
  // huit pixels d'un centre au suivant.
  float digit_pitch_pixels = 8.0F;
  // L'opacité, en facteur sur celle que la vie calcule.
  float opacity = 1.0F;
  // LA VIE DU NOMBRE, ou rien pour celle que le client donne à son allure —
  // voir `ClientLifeOf`.
  std::optional<NumberLife> life;
  // Écrire les grands nombres en milliers (K) et en millions (M), comme le
  // client sous son réglage « Damage Display: Expand ». Sans lui, tout se borne
  // à 999 999 — c'est son défaut, et le nôtre.
  bool abbreviate = false;
};

// L'écart du client entre deux chiffres. ⚠ LE DÉFAUT DU CHAMP CI-DESSUS, et
// nommé à part parce que la composition et ses essais le lisent.
inline constexpr float kDigitPitchPixels = 8.0F;

// ── LES BORNES DES CURSEURS ─────────────────────────────────────────────────
//
// ⚠ ELLES BORNENT LE PANNEAU, PAS LA COMPOSITION : un fichier de réglages
// retouché à la main peut porter n'importe quoi, et un nombre à l'échelle dix
// n'est qu'un nombre trop gros. Rien ici ne peut planter — sauf une durée
// nulle, que `SaneLifeMs` refuse.
inline constexpr float kSmallestNumberScale = 0.25F;
inline constexpr float kLargestNumberScale = 3.0F;
inline constexpr float kLargestNumberLift = 400.0F;
// De part et d'autre : le départ se décale à gauche comme à droite.
inline constexpr float kLargestStartShiftPixels = 400.0F;
// Le nombre qui monte naît au sixième de sa taille finale : le plancher passe
// dessous.
inline constexpr float kSmallestBirthScale = 0.1F;
inline constexpr float kLargestBirthScale = 10.0F;
inline constexpr float kLongestBirthSettleMs = 2000.0F;
inline constexpr float kShortestNumberLifeMs = 200.0F;
inline constexpr float kLongestNumberLifeMs = 6000.0F;
// De part et d'autre : l'arrivée se règle à gauche et en bas aussi, et une
// poignée tire la courbe dans tous les sens.
inline constexpr float kLargestArrivalPixels = 400.0F;
inline constexpr float kLargestBendPixels = 400.0F;
inline constexpr float kSmallestDigitPitch = 6.0F;
inline constexpr float kLargestDigitPitch = 16.0F;
inline constexpr float kFaintestNumberOpacity = 0.1F;

// LE FACTEUR D'ÉTIREMENT tel qu'une marque l'emploie, et la durée de vie telle
// qu'un nombre l'emploie : jamais nuls ni négatifs, quoi que le fichier porte.
// Le temps se DIVISE par eux, et diviser par zéro n'est pas un réglage.
inline constexpr float kSmallestLifeFactor = 0.01F;
inline constexpr float kSmallestLifeMs = 1.0F;

[[nodiscard]] constexpr float SaneLifeFactor(float life) noexcept {
  return life > kSmallestLifeFactor ? life : kSmallestLifeFactor;
}

[[nodiscard]] constexpr float SaneLifeMs(float life_ms) noexcept {
  return life_ms > kSmallestLifeMs ? life_ms : kSmallestLifeMs;
}

// LA TAILLE À CET INSTANT, en fois la taille finale : `birth_scale` à la
// naissance, puis un pas régulier qui la ramène à un en `birth_settle_ms`. Sans
// temps de retour, un dès la naissance.
[[nodiscard]] constexpr float SettlingScaleAt(float elapsed_ms, const NumberLife& life) noexcept {
  if (life.birth_settle_ms <= 0.0F || elapsed_ms >= life.birth_settle_ms) {
    return 1.0F;
  }
  const float progress = elapsed_ms > 0.0F ? elapsed_ms / life.birth_settle_ms : 0.0F;
  return life.birth_scale + (1.0F - life.birth_scale) * progress;
}

// L'OPACITÉ À CET INSTANT, de zéro à un : pleine jusqu'à `fade_start_ms`, puis
// un pas régulier jusqu'à zéro à `life_ms`.
[[nodiscard]] constexpr float FadeAt(float elapsed_ms, const NumberLife& life) noexcept {
  if (elapsed_ms <= life.fade_start_ms) {
    return 1.0F;
  }
  if (elapsed_ms >= life.life_ms || life.fade_start_ms >= life.life_ms) {
    return 0.0F;
  }
  return 1.0F - (elapsed_ms - life.fade_start_ms) / (life.life_ms - life.fade_start_ms);
}

// ── LE CHEMIN VERS LE POINT D'ARRIVÉE ───────────────────────────────────────

// Les coefficients d'une courbe de Bézier cubique : trois pour les deux
// poignées, et les tiers du chemin droit où elles s'accrochent.
inline constexpr float kBezierHandleWeight = 3.0F;
inline constexpr float kFirstHandleShare = 1.0F / 3.0F;
inline constexpr float kSecondHandleShare = 2.0F / 3.0F;

// Un point d'une courbe de Bézier cubique qui part de zéro, passe près de
// `one` puis de `two` et finit en `end`, à `progress` de son chemin.
[[nodiscard]] constexpr float BezierAt(float progress, float one, float two, float end) noexcept {
  const float rest = 1.0F - progress;
  return kBezierHandleWeight * rest * rest * progress * one +
         kBezierHandleWeight * rest * progress * progress * two +
         progress * progress * progress * end;
}

// Où le nombre en est de son chemin vers le point d'arrivée, à `progress` de
// sa vie — zéro à la naissance, un à l'extinction : le décalage à AJOUTER à son
// point de départ, en pixels de sprite, x vers la droite et y vers le haut.
//
// 🔴 LE TEMPS DE LA COURBE EST CELUI DE LA VIE, sans accélération : poignées
// aux tiers, le nombre va droit et à vitesse égale ; poignées rapprochées du
// départ, il part vite et se pose. C'est ce qui rend EXACTE la cloche du
// client — une parabole dans le temps est une courbe de Bézier.
[[nodiscard]] constexpr std::array<float, 2> ArrivalDriftAt(float progress,
                                                            const NumberLook& look) noexcept {
  const float clamped = progress < 0.0F ? 0.0F : (progress > 1.0F ? 1.0F : progress);
  return {BezierAt(clamped, look.arrival_x_pixels * kFirstHandleShare + look.bend_one_x_pixels,
                   look.arrival_x_pixels * kSecondHandleShare + look.bend_two_x_pixels,
                   look.arrival_x_pixels),
          BezierAt(clamped, look.arrival_y_pixels * kFirstHandleShare + look.bend_one_y_pixels,
                   look.arrival_y_pixels * kSecondHandleShare + look.bend_two_y_pixels,
                   look.arrival_y_pixels)};
}

}  // namespace ro::render
