#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "ro/formats/palette.h"
#include "ro/render/hue_classes.h"
#include "ro/render/palette_ramps.h"

namespace ro::render {

// UN STYLE EST UN ENSEMBLE, sous un seul identifiant : la recette du corps,
// plus des teintures de calques — la tête, la peau, les coiffes, la cape.
//
// 🔴 CHAQUE TEINTURE SE DÉSIGNE PAR SA CLÉ, celle que l'observateur reçoit
// déjà du serveur :
//   * tête : la coupe et la couleur de cheveux annoncées, voir `HeadDyeKey` ;
//   * coiffe : l'apparence affichée ; la teinture appartient à l'exemplaire,
//     mais le serveur la remet sous l'apparence avant de l'envoyer ;
//   * cape : la robe affichée, de même ;
//   * peau : zéro, une seule par personnage.
// La garde-robe cherche ainsi la teinture de chaque pièce qu'elle habille, et
// une pièce sans teinture garde ses couleurs.
//
// ⚠ COMME POUR LE CORPS, CE QUI VOYAGE NE PORTE AUCUNE COULEUR : des réglages
// que chaque client applique sur le sprite qu'il possède.

// Les calques, numérotés comme sur le fil. Le corps n'en est pas un : sa
// recette est la v7, à part ; il ne sert qu'à désigner une pièce.
enum class StyleLayer : std::uint8_t {
  kBody = 0,
  kHead = 1,
  kHeadgear = 2,
  kGarment = 3,
  kSkin = 4
};

// Le nombre de réglages d'une teinture : un par classe de teinte au profil 2,
// dont le profil 1 n'emploie que les `kMaxRamps` premiers.
inline constexpr std::size_t kLayerAdjustCount = kHueClassCount;
static_assert(kLayerAdjustCount >= kMaxRamps);

// La clé de teinture d'une tête : la coupe annoncée dans le mot haut, la
// couleur de cheveux annoncée dans le mot bas.
[[nodiscard]] constexpr std::uint32_t HeadDyeKey(std::uint16_t hair_style,
                                                 std::int16_t hair_palette) noexcept {
  constexpr unsigned kHairStyleShift = 16;
  return (static_cast<std::uint32_t>(hair_style) << kHairStyleShift) |
         static_cast<std::uint16_t>(hair_palette);
}

// LA TEINTURE D'UN CALQUE.
struct LayerDye {
  StyleLayer layer = StyleLayer::kHead;
  std::uint32_t key = 0;
  // Le profil qui l'a découpée : `kLayerProfileNumber` (rampes) ou
  // `kHueClassProfileNumber` (classes de teinte). Un profil inconnu fait
  // ignorer la teinture, et le calque garde ses couleurs.
  std::uint8_t profile = kLayerProfileNumber;
  // Au profil 1, un réglage par rampe, dans l'ordre de `DetectLayerRamps` ; au
  // profil 2, un par classe de teinte. La peau n'emploie que le premier.
  std::array<RampAdjust, kLayerAdjustCount> adjusts{};
};

// LA RECETTE DE RAMPES D'UNE TEINTURE au profil 1 : ses `kMaxRamps` premiers
// réglages, que `ApplyRecipe` applique comme ceux d'un corps.
[[nodiscard]] PaletteRecipe LayerRecipe(const LayerDye& dye);
// Les réglages d'une teinture au profil 2, un par classe.
[[nodiscard]] HueClassAdjusts LayerHueClasses(const LayerDye& dye);

// Le calque que le profil 1 découpe pour ce calque de style, ou rien pour la
// peau et le corps, qui n'en ont pas.
[[nodiscard]] std::optional<RampLayer> RampLayerOf(StyleLayer layer) noexcept;

// L'ENSEMBLE.
struct StyleSet {
  // La recette v7 du corps, ou rien : le corps garde alors ses couleurs.
  std::optional<PaletteRecipe> body;
  // UNE PALETTE DE CORPS TOUTE FAITE, qui l'emporte sur `body` : l'appelant a
  // déjà fusionné et peint. C'est l'aperçu de l'éditeur, qui fait pulser une
  // pièce, ce qu'aucun réglage n'exprime. La même vaut pour tous les corps.
  std::optional<formats::Palette> body_palette;
  // Au plus une teinture par couple (calque, clé) : la première l'emporte.
  std::vector<LayerDye> layers;

  // La teinture de ce calque sous cette clé, ou nulle.
  [[nodiscard]] const LayerDye* Find(StyleLayer layer, std::uint32_t key) const;
};

// UN CORPS DONT LA PEAU NE SE RÈGLE PAS, par le chemin de son `.spr`.
//
// 🔴 UNE LISTE PAR NOM, ET RIEN D'AUTRE NE LES DISTINGUE. Sur ces montures,
// les index de peau peignent aussi l'animal : le bec des autruches, des mèches
// de la crinière du lion de la Royal Guard en tenue alternative. Rien dans les
// données ne sépare ces pixels de la peau du cavalier ; seul l'œil le fait.
// Teindre la peau y teindrait l'animal, et le visage seul se désassortirait du
// corps : ces corps gardent la peau du dessin, tête comprise.
[[nodiscard]] bool SkinAdjustable(std::string_view body_sprite_path);

// LA PEAU, appliquée EN DERNIER : les index de `SkinIndices(race)` de
// `painted` reprennent la couleur de `base` réglée par `adjust`.
//
// 🔴 ELLE L'EMPORTE SUR TOUT CE QUI A ÉTÉ PEINT AVANT, recette du corps ou
// rampes de la tête : c'est le seul réglage de ces index, et le visage et les
// bras ne peuvent pas se désassortir. Un réglage neutre rend donc la peau du
// dessin.
void ApplySkin(formats::Palette& painted, const formats::Palette& base, SkinRace race,
               const RampAdjust& adjust);

}  // namespace ro::render
