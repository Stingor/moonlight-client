#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/formats/image.h"

namespace ro::formats {

// LA TEINTE D'UNE ICÔNE D'OBJET : une couleur posée sur l'icône au chargement.
//
// Toutes les cartes partagent la même icône de sac, le dos de carte générique,
// et l'installation n'en porte aucune variante de couleur. La teinte en fait
// une sans fichier neuf : l'icône se recolore à sa lecture, en gardant ses
// lumières et ses ombres, et rien de l'installation n'est réécrit.
//
// Elle s'écrit par un MOT dans `item_info.yml` (`tint: "gold"`). ⚠ CES MOTS
// SONT UN CONTRAT : la table les porte, et
// `scripts/yaml_tables/apply_item_info_delta.py` les relit dans ce fichier.
enum class IconTint : std::uint8_t {
  kNone,
  kGold,
  kRed,
  kPurple,
  kBlue,
  kGreen,
  kPink,
  kOrange,
  kBlack
};

// Une teinte proposée : son mot dans la table, son libellé, et la couleur que
// prend le ton moyen de l'icône.
struct IconTintShade {
  IconTint tint = IconTint::kNone;
  std::string_view word;
  const char* label = "";
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
};

// Les teintes, dans l'ordre où une liste les propose. `kNone` n'y est pas :
// c'est l'icône telle que l'installation la donne.
inline constexpr std::array<IconTintShade, 8> kIconTintShades = {{
    {IconTint::kGold, "gold", core::TrText("Or"), 232, 176, 40},
    {IconTint::kRed, "red", core::TrText("Rouge"), 214, 46, 46},
    {IconTint::kPurple, "purple", core::TrText("Violet"), 146, 70, 206},
    {IconTint::kBlue, "blue", core::TrText("Bleu"), 52, 110, 224},
    {IconTint::kGreen, "green", core::TrText("Vert"), 54, 168, 74},
    {IconTint::kPink, "pink", core::TrText("Rose"), 236, 110, 180},
    {IconTint::kOrange, "orange", core::TrText("Orange"), 240, 128, 32},
    {IconTint::kBlack, "black", core::TrText("Noir"), 70, 70, 82},
}};

// La teinte qu'un mot désigne ; vide pour un mot qu'on ne connaît pas. Le mot
// vide désigne `kNone`.
[[nodiscard]] std::optional<IconTint> IconTintFromWord(std::string_view word) noexcept;

// Le mot d'une teinte ; vide pour `kNone`.
[[nodiscard]] std::string_view IconTintWord(IconTint tint) noexcept;

// La teinte décrite, ou nulle pour `kNone`.
[[nodiscard]] const IconTintShade* FindIconTintShade(IconTint tint) noexcept;

// RECOLORE L'IMAGE : chaque pixel visible prend la couleur de la teinte, à la
// luminance qu'il avait. Le noir reste noir, le blanc reste blanc, et le ton
// moyen (`kIconTintMidLuma`) prend la couleur pleine. Un pixel transparent ne
// change pas, ni l'alpha d'aucun. `kNone` ne change rien.
//
// À appliquer APRÈS la couleur clé : le magenta d'un pixel effacé garde sa
// couleur, et c'est elle que le téléversement repeint.
void ApplyIconTint(Image& image, IconTint tint);

// La luminance, sur 1, qui prend la couleur pleine de la teinte. Au-dessus de
// la moitié : le dos de carte est clair, et une teinte posée au milieu
// l'aurait délavé.
inline constexpr float kIconTintMidLuma = 0.62F;

}  // namespace ro::formats
