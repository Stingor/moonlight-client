#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ro/formats/image.h"
#include "ro/formats/palette.h"
#include "ro/render/palette_ramps.h"

namespace ro::render {

// LE PROFIL 2 : LES CLASSES DE TEINTE.
//
// Chaque couleur tombe dans l'une de treize classes : douze secteurs de 30°
// centrés sur 0°, 30°, 60°…, plus les gris. Une teinture de ce profil porte un
// réglage par classe, et chaque couleur reçoit celui de sa classe.
//
// 🔴 C'EST UN REPLI, PAS LA RÈGLE. Sur une image palettisée, deux pièces que
// les rampes séparent tombent souvent dans la même classe, et une pièce sur
// deux chevauche deux classes. Le profil 2 ne sert que là où les rampes ne
// trouvent rien : les images en couleurs vraies, et les coiffes sans pièce.
//
// ⚠ LE CLASSEMENT SE FAIT EN ENTIERS, comme les coupes du profil 1 : aucun
// flottant ne décide de la classe d'une couleur, et deux compilateurs rangent
// donc chaque couleur au même endroit.

// Le numéro que porte une teinture découpée en classes de teinte.
inline constexpr std::uint8_t kHueClassProfileNumber = 2;

inline constexpr std::size_t kHueSectorClasses = 12;
// La classe des gris vient après les douze secteurs.
inline constexpr std::size_t kGreyHueClass = kHueSectorClasses;
inline constexpr std::size_t kHueClassCount = kHueSectorClasses + 1;
// Sous ce pourcentage de saturation TSL, une couleur est grise.
inline constexpr int kHueGreyPercent = 8;

// Un réglage par classe, dans l'ordre des classes : le secteur de 0° d'abord,
// les gris en dernier.
using HueClassAdjusts = std::array<RampAdjust, kHueClassCount>;

// La classe d'une couleur : le secteur de 30° centré le plus proche de sa
// teinte, ou les gris sous `grey_percent` de saturation TSL.
[[nodiscard]] std::size_t HueClassOf(int red, int green, int blue,
                                     int grey_percent = kHueGreyPercent);

// Une palette réglée classe par classe, index par index. L'index zéro, le
// transparent, ne se touche pas, ni l'octet d'alpha.
[[nodiscard]] formats::Palette ApplyHueClasses(const formats::Palette& base,
                                               const HueClassAdjusts& adjusts);

// Une image en couleurs vraies réglée classe par classe, pixel par pixel. Un
// pixel tout à fait transparent ne se touche pas ; l'alpha reste celui du
// fichier.
//
// ⚠ ELLE SE FAIT AU TÉLÉVERSEMENT, une fois par image : la clé de texture
// porte la teinture, voir `SpriteImageKey::tint`.
void TintImage(formats::Image& image, const HueClassAdjusts& adjusts);

}  // namespace ro::render
