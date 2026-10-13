#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

namespace ro::render {

// ── LE CALCUL DES EFFETS DE CARTE, TEL QUE LE CLIENT LE FAIT ────────────────
//
// Les émetteurs et les anneaux de vent se simulent DANS LE REPÈRE DU CLIENT —
// celui du RSW : centré sur la carte, ordonnée qui DESCEND — et avec ses
// propres matrices. On ne passe au nôtre qu'au moment de rendre des coins. C'est
// ce qui permet de transcrire chaque produit tel qu'il est écrit, sans
// retourner un signe de tête.

// UNE MATRICE DU CLIENT : trois lignes de trois, puis une translation, qui
// s'applique à un VECTEUR LIGNE — `p' = p · M`. Dans un produit `A · B`, A
// s'applique donc d'abord.
using NativeMatrix = std::array<float, 12>;

inline constexpr NativeMatrix kNativeIdentity = {1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F,
                                                 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F};

// L'ÉTIQUETTE D'UN QUAD D'EFFET DE CARTE : l'emplacement de sa particule dans
// les poids forts, sa génération dans les faibles. Un emplacement recyclé change
// de génération, donc d'étiquette ; et les quads d'un objet sortent par
// emplacement croissant, donc par étiquette croissante.
inline constexpr unsigned kMapEffectSlotShift = 32;
[[nodiscard]] constexpr std::uint64_t MapEffectQuadTag(std::size_t slot,
                                                       std::uint32_t generation) noexcept {
  return (static_cast<std::uint64_t>(slot) << kMapEffectSlotShift) | generation;
}

// Une mise à l'échelle.
[[nodiscard]] NativeMatrix NativeScale(float x, float y, float z);
// Une rotation autour de X : la matrice est REMPLACÉE.
[[nodiscard]] NativeMatrix NativeRotationX(float radians);
// Les trois rotations composées : la rotation s'applique APRÈS la matrice, translation comprise.
void MultiplyRotationX(NativeMatrix& matrix, float radians);
void MultiplyRotationY(NativeMatrix& matrix, float radians);
void MultiplyRotationZ(NativeMatrix& matrix, float radians);
// Une translation composée, qui s'applique APRÈS la matrice.
void MultiplyTranslation(NativeMatrix& matrix, glm::vec3 offset);
[[nodiscard]] NativeMatrix Multiply(const NativeMatrix& first, const NativeMatrix& then);
[[nodiscard]] glm::vec3 Transform(glm::vec3 point, const NativeMatrix& matrix);

// L'ANGLE D'UN NOMBRE ENTIER DE DEGRÉS, lu dans la table de degrés du client.
//
// 🔴 UN ANGLE NÉGATIF TOMBE UN DEGRÉ À CÔTÉ. La table des négatifs est
// indexée À REBOURS depuis la fin de celle des positifs, qui compte TROIS CENT
// SOIXANTE ET UNE entrées : −k lit donc l'angle de 361 − k, et non de 360 − k.
// Au-delà d'un tour, l'angle se ramène d'abord au reste de sa division par 360,
// signe compris — et −360 lui-même n'est pas ramené : il lit un degré.
[[nodiscard]] float NativeTableRadians(std::int32_t degrees);

// LE TIRAGE DU CLIENT ENTRE DEUX BORNES : sur une grille d'un DIXIÈME.
//
// ⚠ IL PART TOUJOURS DE LA PREMIÈRE BORNE VERS LE HAUT, de l'écart entre les
// deux : une seconde borne plus petite ne se rejoint jamais. Le rayon par défaut
// d'un émetteur (−1 en abscisse) donne ainsi des naissances entre UN et TROIS,
// pas entre −1 et 1.
//
// Le générateur est celui des nœuds d'effet (`NextEffectNodeDraw`), dont
// l'étendue est celle de `rand()`.
[[nodiscard]] float EffectToolRandomBetween(std::uint32_t& draws, float low, float high);

// LE PASSAGE DU REPÈRE DU CLIENT AU NÔTRE, pour une carte donnée : le même que
// pour la place d'un modèle — demi-plateau, ordonnée retournée, profondeur
// retournée avec son retrait de dix unités. Voir `ModelInstanceMatrix`.
struct MapEffectFrame {
  std::int32_t gnd_width = 0;
  std::int32_t gnd_height = 0;

  [[nodiscard]] glm::vec3 ToWorld(glm::vec3 native) const;
  [[nodiscard]] glm::vec3 ToNative(glm::vec3 world) const;
};

}  // namespace ro::render
