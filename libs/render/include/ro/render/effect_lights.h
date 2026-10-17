#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/image.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/point_lights.h"
#include "ro/render/sprite_billboard.h"

namespace ro::render {

// LA LUEUR D'UNE TEXTURE D'EFFET : ce qu'elle rend de lumière quand elle se
// peint en addition. Voir docs/adr/0120.
struct TextureGlow {
  // La moyenne de ses texels CLAIRS, pondérée par leur luminance : de zéro à un
  // par canal.
  glm::vec3 color{0.0F};
  // De zéro à un : la part de l'image qui luit. Nulle pour une texture sombre,
  // qui n'éclaire rien.
  float strength = 0.0F;
};

// Un texel est CLAIR à partir de cette luminance, de zéro à un : en deçà, un
// fond sombre ou une fumée, qui n'éclaire pas.
inline constexpr float kGlowTexelLuminance = 0.35F;
// LA COUVERTURE LUMINEUSE — la somme, sur l'image, de la luminance de ses
// texels clairs fois leur opacité, rapportée au nombre de texels — à partir de
// laquelle une texture luit pleinement. Une étincelle n'en couvre qu'une
// fraction et luit d'autant moins.
inline constexpr float kGlowFullCoverage = 0.15F;
// En deçà de cette couverture, la texture est sombre : elle n'éclaire rien.
inline constexpr float kGlowMinCoverage = 0.01F;

// La lueur de `image`, ses texels lus une fois. Une image vide ne luit pas.
[[nodiscard]] TextureGlow GlowOfImage(const formats::Image& image);

// LES LUEURS DÉJÀ CALCULÉES, par chemin d'archive : une texture se lit une
// fois, et non à chaque image. Le chemin IDENTIFIE la texture, comme pour son
// téléversement.
class TextureGlowCache {
 public:
  [[nodiscard]] const TextureGlow& Of(const std::string& path, const formats::Image& image);

  [[nodiscard]] std::size_t size() const noexcept { return glows_.size(); }

 private:
  std::map<std::string, TextureGlow, std::less<>> glows_;
};

// L'intensité d'une couche pleine, opaque, d'une texture qui luit pleinement,
// à sa taille de pleine lueur.
inline constexpr float kEffectLightGain = 1.2F;
// Le plafond de chaque canal d'une lumière d'effet : au-delà, toute la couleur
// baisse ensemble, et la teinte reste. Un grand sort l'atteint d'un bout à
// l'autre : au-delà, le sol qu'il éclaire vire au blanc.
inline constexpr float kEffectLightPeak = 1.2F;
// L'OMBRE D'UN SORT EST DOUCE : un sort est un volume de lumière, non une
// flamme, et un obstacle ne le cache jamais tout entier. Son ombre ne retire
// que cette part de sa lumière, avec une pénombre de cette part de la distance
// — voir `PointLight::shadow_depth` et `PointLight::shadow_spread`.
inline constexpr float kEffectShadowDepth = 0.6F;
inline constexpr float kEffectShadowSpread = 0.06F;
// Le demi-diamètre d'une couche, en unités du monde, à partir duquel elle luit
// de toute son intensité : une couche plus petite luit en proportion. Une
// étincelle ou un flocon n'éclaire pas le sol.
inline constexpr float kEffectLightFullSizeWorld = 6.0F;
// LA PORTÉE d'une lumière d'effet : le demi-diamètre moyen de ses couches fois
// ce facteur, entre ces bornes, en unités du monde.
inline constexpr float kEffectLightRangePerSize = 3.0F;
inline constexpr float kEffectLightMinRangeWorld = 15.0F;
inline constexpr float kEffectLightMaxRangeWorld = 90.0F;
// En deçà de cette intensité — la plus claire de ses composantes —, un effet
// n'éclaire pas : il ne prend pas de place.
inline constexpr float kEffectLightMinStrength = 0.05F;

// LES LUMIÈRES DES EFFETS DE L'IMAGE, une par effet qui en porte une, ajoutées
// à `out` : seules leurs couches ADDITIVES éclairent, chacune de la lueur de sa
// texture, teintée de sa couleur et pesée par son alpha et sa taille de
// l'instant. La lumière se tient au centre pondéré de ces couches.
//
// `effects` : les fichiers `.str`, en panneaux que `basis` dresse face à la
// caméra. `world_lots` : les quads du monde des nœuds et des méthodes nommées,
// une lumière par lot.
void EffectLightsOf(std::span<const DrawnEffect> effects,
                    std::span<const DrawnWorldQuads> world_lots, const BillboardBasis& basis,
                    TextureGlowCache& glows, std::vector<PointLight>& out);

// LA PLACE DES EFFETS dans le jeu de l'image : au plus tant de lumières, les
// plus proches de l'œil. Le reste reste aux lumières du document.
inline constexpr std::size_t kMaxEffectLights = 4;

// LE JEU DE L'IMAGE : les lumières du document `document`, déjà rangées par
// `NearestPointLights`, D'ABORD, puis les `kMaxEffectLights` lumières
// d'`effects` les plus proches de `eye`, sans dépasser `kMaxPointLights`. Une
// lumière d'effet ne chasse une lumière du document que lorsque les deux
// réunies débordent : le document garde alors
// `kMaxPointLights − kMaxEffectLights` places au moins, les plus proches.
//
// ⚠ LES OMBRES VONT AU DOCUMENT D'ABORD : `DrawPointShadows` ombre les
// premières lumières du jeu, et une lumière d'effet n'en prend une place que
// si le document la laisse libre.
[[nodiscard]] PointLightSet WithEffectLights(const PointLightSet& document,
                                             std::span<const PointLight> effects, glm::vec3 eye);

}  // namespace ro::render
