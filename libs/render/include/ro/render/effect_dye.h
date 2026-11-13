#pragma once

#include <array>
#include <cstdint>

namespace ro::render {

// LA TEINTURE D'UN EFFET DE CHAPEAU, appliquée AU DESSIN.
//
// Un effet de chapeau n'a pas de rampes : ses textures sont en couleurs
// directes, et une part de sa couleur vient des images clés de son fichier.
// La teinture est donc une transformation de la couleur PEINTE — texel fois
// couleur de la clé —, faite par le nuanceur des effets pour les seuls quads
// nés de cet effet chez ce porteur. Aucune texture n'est copiée : une
// compétence qui partage la texture, ou un autre porteur du même costume, la
// voient d'origine.
//
// La couleur passe en TSL : la teinte tourne, la saturation et la clarté se
// multiplient. Un gris n'a pas de teinte et reste un gris.
//
// ⚠ PAS DE MATRICE DE COULEUR. Une rotation linéaire autour de l'axe des gris
// garde la luminance, et c'est ce qui la trahit : elle affadit les teintes
// saturées — un magenta tourne au rose, un rouge vire au vert sombre. La TSL
// rend la teinte que le joueur a choisie.
struct EffectDye {
  float hue_degrees = 0.0F;
  float saturation_scale = 1.0F;
  float lightness_scale = 1.0F;
  // ASSOMBRIR : sous un mélange additif, un effet ne peut qu'éclaircir le
  // fond, et baisser la clarté le rend plus pâle, jamais plus foncé. Assombrir
  // peint en alpha ses SEULS quads additifs : leur texel prend pour alpha sa
  // plus forte composante, et pour couleur sa teinte portée à pleine
  // intensité, avant la TSL, et ils se mélangent « source par son alpha,
  // destination par son inverse ». Ses quads alpha ne sont que teints.
  bool darken = false;

  [[nodiscard]] friend bool operator==(const EffectDye&, const EffectDye&) = default;
};

// Les bornes du réglage, celles d'une entrée de teinture : la teinte en
// degrés relatifs, la saturation et la clarté en pour-cent relatifs.
inline constexpr int kEffectDyeHueMost = 359;
inline constexpr int kEffectDyePercentMost = 100;

// La teinture d'un réglage. Hors bornes, les valeurs sont ramenées aux bornes.
// `saturation_percent` à −100 donne des gris ; `value_percent` à −100, du
// noir.
[[nodiscard]] EffectDye MakeEffectDye(int hue_degrees, int saturation_percent, int value_percent,
                                      bool darken);

// Vrai quand la teinture ne change aucun texel.
[[nodiscard]] bool IsNeutralEffectDye(const EffectDye& dye);

// LE TEXEL PEINT, teint : ce que fait le nuanceur, sur une couleur déjà
// multipliée par celle de la clé. Sert au banc et aux essais, et c'est la
// référence du nuanceur. `darken` s'y applique tel quel : c'est à l'appelant
// de ne le poser que pour un quad additif (`EffectBlendAdds`).
[[nodiscard]] std::array<std::uint8_t, 4> DyeEffectTexel(std::array<std::uint8_t, 4> rgba,
                                                         const EffectDye& dye);

// Vrai quand ce couple de facteurs Direct3D AJOUTE la source au fond : c'est
// sur ces quads qu'« assombrir » change le mélange. Sur la cible opaque de la
// scène, l'alpha de la destination vaut un.
[[nodiscard]] bool EffectBlendAdds(std::int32_t blend_source, std::int32_t blend_destination);

// Les facteurs Direct3D du mélange alpha qu'« assombrir » impose.
inline constexpr std::int32_t kDarkenedBlendSource = 5;
inline constexpr std::int32_t kDarkenedBlendDestination = 6;

}  // namespace ro::render
