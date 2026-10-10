#pragma once

#include <cmath>
#include <cstdint>
#include <numbers>

#include "ro/render/effect_track_fwd.h"

// LES ANGLES DE LA FAMILLE PROCÉDURALE, en un seul endroit.
//
// En-tête PRIVÉ : il vit à côté des sources qui l'emploient et ne sort pas de
// `libs/render/src`. Rien de public n'en dépend.
//
// ⚠ IL EXISTE PARCE QUE LES MOUVEMENTS ONT DÛ SE COUPER EN DEUX FICHIERS, au
// plafond de huit cents lignes. Recopier ces quatre fonctions dans le second
// aurait suffi à les faire diverger à la première correction — c'est déjà arrivé
// dans ce dépôt, sur des lecteurs de valeur dupliqués.
//
// ⚠ ET LE CLIENT LIT DES TABLES DE TROIS CENT SOIXANTE ENTRÉES, indexées par un
// degré ENTIER. Nous calculons la même valeur ; le résultat ne se distingue pas
// à l'œil, et une table de plus serait une donnée à tenir à jour pour rien.

namespace ro::render {

[[nodiscard]] inline float DegreesToRadians(float degrees) {
  return degrees * std::numbers::pi_v<float> / 180.0F;
}

// Le sinus d'un angle ENTIER de degrés, comme le client l'indexe.
//
// Il lit une table de trois cent soixante entrées ; nous calculons la même
// valeur. Le résultat ne se distingue pas à l'œil, et une table de plus serait
// une donnée à tenir à jour pour rien.
[[nodiscard]] inline float SinOfDegrees(std::int32_t degrees) {
  return std::sin(DegreesToRadians(static_cast<float>(degrees)));
}

// Le cosinus d'un angle ENTIER de degrés, par la même règle que le sinus : le
// client indexe une seconde table de trois cent soixante entrées.
[[nodiscard]] inline float CosOfDegrees(std::int32_t degrees) {
  return std::cos(DegreesToRadians(static_cast<float>(degrees)));
}

// Ramène un angle dans un tour, y compris quand il est négatif — le reste de la
// division entière du C++ garde le signe du dividende, et un angle négatif
// indexerait avant le début de la table du client.
[[nodiscard]] inline std::int32_t WithinFullTurn(std::int32_t degrees) {
  const std::int32_t remainder = degrees % kEffectTrackFullTurnDegrees;
  return remainder < 0 ? remainder + kEffectTrackFullTurnDegrees : remainder;
}

// LE CAP D'UN VECTEUR À PLAT, en degrés ENTIERS, à la façon du client.
//
// ⚠ IL NE PREND PAS UN ARC TANGENTE : il balaie sa table de cosinus du degré
// zéro au degré cent quatre-vingts et s'arrête au PREMIER dont le cosinus passe
// sous le rapport cherché, puis retourne le tour quand la profondeur est
// négative. Le résultat est donc arrondi VERS LE HAUT au degré, et un `atan2`
// arrondi autrement décalerait d'un degré tous les caps qui en sortent.
//
// ⚠ UN VECTEUR NUL REND ZÉRO, comme chez le client : la division du client
// donne alors un NaN, toutes ses comparaisons sont fausses, et sa boucle sort
// au premier degré.
//
// ⚠ ET DEUX FAMILLES L'EMPLOIENT pour des choses sans rapport — l'essaim pour
// viser sa cible, l'étincelle pour poser sa section en travers. C'est bien la
// même fonction du client, et la transcrire deux fois l'aurait fait diverger à
// la première correction.
[[nodiscard]] inline std::int32_t HeadingOfVector(float toward_x, float toward_z) {
  const float length = std::sqrt(toward_x * toward_x + toward_z * toward_z);
  const float cote = toward_x / length;
  std::int32_t degre = 0;
  while (degre <= kEffectTrackHalfTurnDegrees && cote < CosOfDegrees(degre)) {
    ++degre;
  }
  if (degre > kEffectTrackHalfTurnDegrees) {
    return kEffectTrackHalfTurnDegrees;
  }
  if (toward_z / length < 0.0F) {
    degre = kEffectTrackFullTurnDegrees - degre;
  }
  return degre;
}

}  // namespace ro::render
