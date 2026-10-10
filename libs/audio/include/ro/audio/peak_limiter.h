#pragma once

#include <span>

namespace ro::audio {

// LA RÉSERVE DU MÉLANGE : le mélangeur somme toutes les voix trente décibels
// sous la pleine échelle, puis `PeakLimiter::Process` les remonte d'autant.
//
// ⚠ UNE PUISSANCE DE DEUX, ET CE N'EST PAS UN GOÛT : multiplier un flottant par
// une puissance de deux ne l'arrondit pas, et la somme de deux nombres réduits
// d'autant est la somme réduite. Descendre puis remonter rend donc les
// échantillons BIT POUR BIT, et un mélange qui tient sous la pleine échelle
// sort de la chaîne tel qu'il y est entré.
//
// Sans réserve, le mélangeur écrête d'office ce qu'il somme au-delà d'un, avant
// qu'aucun limiteur ne puisse le voir.
inline constexpr float kMixHeadroom = 32.0F;

// Combien de temps le gain reste où une crête l'a mis, en millisecondes.
//
// ⚠ PLUS LONG QUE LA PÉRIODE DU SON LE PLUS GRAVE, et c'est ce qui évite de
// déformer : une enveloppe qui retomberait entre deux crêtes serait rattrapée
// par le front montant de la suivante, que le gain suivrait alors échantillon
// par échantillon — c'est-à-dire un écrêtage. Cinquante millisecondes couvrent
// une période à vingt hertz, le bas de l'audible.
inline constexpr float kLimiterHoldMs = 50.0F;

// Le temps que met le gain à revenir après le maintien, en millisecondes : la
// constante de temps de sa remontée exponentielle.
inline constexpr float kLimiterReleaseMs = 100.0F;

// LE FILET EN BOUT DE MÉLANGE : ce qui dépasse la pleine échelle est ramené
// dessous par un GAIN qui baisse et remonte, et non par un écrêtage.
//
// L'écrêtage aplatit la crête de chaque période, ce qui fabrique des
// harmoniques que le son n'avait pas : c'est le grésillement qu'on entend
// quand plusieurs bruits forts se superposent. Un gain qui baisse garde la
// forme d'onde, et ne fait que baisser le volume le temps du dépassement.
//
// ⚠ TRANSPARENT EN DEÇÀ : tant qu'aucun échantillon ne dépasse la pleine
// échelle, le gain vaut exactement un et le mélange sort inchangé.
//
// L'attaque est instantanée — aucune sortie ne dépasse la pleine échelle, pas
// même au premier échantillon d'un dépassement —, le gain se maintient
// `kLimiterHoldMs` puis remonte selon `kLimiterReleaseMs`. Les voies d'une même
// trame partagent un gain : baisser une oreille seule déplacerait le son.
//
// ⚠ SANS ANTICIPATION : le front montant de la PREMIÈRE crête d'un dépassement
// est suivi à la trame, donc écrêté sur un quart de période. Anticiper
// retarderait tout le son, propre compris ; les crêtes suivantes, elles,
// passent entières sous le gain maintenu.
class PeakLimiter {
 public:
  // Remonte `pcm` de `kMixHeadroom`, puis le borne à la pleine échelle.
  // `pcm` est entrelacé, `channels` voies par trame, à `rate_hz` trames par
  // seconde. Sans effet sur une disposition absurde.
  void Process(std::span<float> pcm, int channels, int rate_hz);

  // Le gain appliqué à la dernière trame : un tant qu'on ne limite pas.
  [[nodiscard]] float last_gain() const { return last_gain_; }

 private:
  // La plus haute crête récente, qui décroît d'elle-même entre deux crêtes.
  float envelope_ = 0.0F;
  // Les trames que l'enveloppe tient encore avant de retomber.
  int hold_frames_left_ = 0;
  // Les deux durées converties à `timing_rate_hz_` : le maintien en trames, et
  // ce que l'enveloppe garde d'une trame à la suivante en retombant.
  int hold_frames_ = 0;
  float release_per_frame_ = 0.0F;
  int timing_rate_hz_ = 0;
  float last_gain_ = 1.0F;
};

}  // namespace ro::audio
