#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "ro/formats/palette.h"
#include "ro/render/hue_classes.h"
#include "ro/render/palette_ramps.h"
#include "ro/render/palette_rest.h"

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
//   * peau du visage : zéro, une seule par personnage ;
//   * peau du corps, corps, suite : la clé du corps, celle de la recette v7.
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
  kSkin = 4,
  // LES RAMPES 9 À 12 DU CORPS, que la v7 ne porte pas : voir
  // `DetectBodyContinuation`. Moonclient seul les lit ; un autre client voit
  // ces pièces dans leurs couleurs.
  kBodyMore = 5,
  // LA PEAU DU CORPS, propre à moonclient : les bras et les jambes d'un corps,
  // sous la clé de ce corps, comme un vêtement porté par-dessus. Un corps sans
  // la sienne prend la peau du visage (`kSkin`) : voir `StyleSet::SkinOf`.
  kBodySkin = 6
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
  // LE RESTE (profil mixte) : un réglage par classe de teinte, pour chaque index
  // qu'aucune pièce ni la peau ne couvre. Aux profils 1 et 3 seulement ; au
  // profil 2, les classes couvrent déjà tout.
  HueClassAdjusts rest{};
  // LES RETOUCHES, appliquées en dernier, peau comprise : la couleur exacte
  // d'un index. Triées par index, une par index. Jamais sur la peau.
  std::vector<PaletteRetouch> retouches;
};

// Le reste ne s'applique qu'à une teinture de pièces palettisées : la tête,
// une coiffe ou une cape au profil 1, la suite du corps au profil 3.
[[nodiscard]] bool LayerTakesRest(const LayerDye& dye) noexcept;
// Les retouches, à ces mêmes teintures et à celles du profil 2 ; jamais à la
// peau.
[[nodiscard]] bool LayerTakesRetouches(const LayerDye& dye) noexcept;

// LA RECETTE DE LA SUITE DU CORPS : ses `kBodyContinuationRamps` premiers
// réglages, aux premiers rangs d'une recette à appliquer sur les rampes de
// `DetectBodyContinuation`.
[[nodiscard]] PaletteRecipe BodyContinuationRecipe(const LayerDye& dye);

// LA RECETTE DE RAMPES D'UNE TEINTURE au profil 1 : ses `kMaxRamps` premiers
// réglages, que `ApplyRecipe` applique comme ceux d'un corps.
[[nodiscard]] PaletteRecipe LayerRecipe(const LayerDye& dye);
// Les réglages d'une teinture au profil 2, un par classe.
[[nodiscard]] HueClassAdjusts LayerHueClasses(const LayerDye& dye);

// LA SUITE ET LE RESTE DU CORPS, posés sur `painted` : les rampes de
// `continuation` reprennent la base réglée par la suite, puis chaque index hors
// des pièces v7, de la suite et de la peau reçoit le réglage de sa classe. La
// peau et les retouches viennent après, à l'appelant.
void ApplyBodyMore(formats::Palette& painted, const formats::Palette& base,
                   std::span<const PaletteRamp> v7, std::span<const PaletteRamp> continuation,
                   SkinRace race, const LayerDye& more);

// LE RESTE D'UN CALQUE au profil 1, posé sur `painted` : hors de ses pièces et,
// pour une tête, hors de la peau. Sans effet pour une teinture qui n'en prend
// pas.
void ApplyLayerRest(formats::Palette& painted, const formats::Palette& base,
                    std::span<const PaletteRamp> ramps, SkinRace race, const LayerDye& dye);

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

  // LA PEAU QUI S'APPLIQUE À UNE PIÈCE, ou nulle : la tête prend la peau du
  // visage ; un corps prend la sienne, rangée sous `body_key`, et À DÉFAUT celle
  // du visage. Une peau d'un profil inconnu ne compte pas.
  //
  // 🔴 LE REPLI REND LA SÉPARATION GRATUITE : qui ne règle que le visage voit
  // ses bras le suivre.
  [[nodiscard]] const LayerDye* SkinOf(StyleLayer piece, std::uint32_t body_key) const;
};

// UN CORPS DONT LA PEAU NE SE RÈGLE PAS, par le chemin de son `.spr`.
//
// 🔴 UNE LISTE PAR NOM, ET RIEN D'AUTRE NE LES DISTINGUE. Sur ces montures,
// les index de peau peignent aussi l'animal : le bec des autruches, des mèches
// de la crinière du lion de la Royal Guard en tenue alternative. Rien dans les
// données ne sépare ces pixels de la peau du cavalier ; seul l'œil le fait.
// Teindre la peau y teindrait l'animal, et le visage seul se désassortirait du
// corps : ces corps gardent la peau du dessin, tête comprise, et aucune des
// deux peaux ne s'y règle.
[[nodiscard]] bool SkinAdjustable(std::string_view body_sprite_path);

// LA PEAU, appliquée EN DERNIER : les index de `SkinIndices(race)` de
// `painted` reprennent la couleur de `base` réglée par `adjust`.
//
// 🔴 ELLE L'EMPORTE SUR TOUT CE QUI A ÉTÉ PEINT AVANT, recette du corps ou
// rampes de la tête : c'est le seul réglage de ces index. Un réglage neutre
// rend donc la peau du dessin.
void ApplySkin(formats::Palette& painted, const formats::Palette& base, SkinRace race,
               const RampAdjust& adjust);

}  // namespace ro::render
