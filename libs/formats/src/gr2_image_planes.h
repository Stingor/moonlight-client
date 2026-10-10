#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/error.h"

namespace ro::formats::gr2 {

// UNE COUCHE D'UNE TEXTURE CODÉE PAR ONDELETTES, en coefficients sur seize
// bits signés, rangée par rangée sur toute la largeur arrondie.
//
// LA DISPOSITION DES SOUS-BANDES. Au niveau n (1 le plus fin, 4 le plus
// grossier), une bande fait largeur >> n sur hauteur >> n. Ses rangées se
// posent tous les 2^n rangs du plan ; celles des bandes basses verticalement
// sur les rangs pairs de ce pas, celles des bandes hautes au demi-pas. Dans une
// rangée, la bande basse horizontalement occupe la moitié gauche de la largeur
// du niveau suivant, la haute la moitié droite. La bande la plus grossière
// (basse dans les deux sens) ouvre le plan.
//
// La synthèse remonte ces niveaux en place ; chacun rend une image entrelacée
// que le niveau suivant lit comme sa bande basse.
struct Gr2Plane {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::vector<std::int16_t> samples;
};

// Le nombre de niveaux de la décomposition.
inline constexpr std::uint32_t kGr2WaveletLevels = 4;

// Décode les sous-bandes d'une couche, puis le drapeau de chacune de ses
// rangées — voir `SynthesizeGr2Plane`. Rend le nombre d'octets de la couche.
[[nodiscard]] core::Result<std::size_t> DecodeGr2PlaneBands(std::span<const std::uint8_t> data,
                                                            Gr2Plane& plane,
                                                            std::vector<std::uint8_t>& row_flags);

// Remonte les quatre niveaux d'ondelettes. `row_flags` dit, pour chaque rangée
// du plan, si la synthèse horizontale du dernier niveau tient compte de sa
// moitié haute : à zéro, la rangée ne s'en sert pas.
void SynthesizeGr2Plane(Gr2Plane& plane, std::span<const std::uint8_t> row_flags);

}  // namespace ro::formats::gr2
