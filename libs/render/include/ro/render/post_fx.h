#pragma once

#include <array>
#include <cstdint>

namespace ro::render {

// LES FILTRES DE COULEUR, dans l'ordre du menu. ⚠ LE RANG EST ÉCRIT DANS LE
// FICHIER DE RÉGLAGES : les réordonner changerait le filtre de qui en a posé un.
enum class PostFxFilter : int {
  kNone = 0,
  kGrayscale,
  kSepia,
  kNegative,
  // ⚠ UNE TEINTE, PAS UNE SIMULATION DE DALTONISME : un léger décalage vers le
  // rouge et le bleu, sans matrice de confusion des couleurs.
  kColorblind,
  kCount,
};

// LE POST-TRAITEMENT DE L'IMAGE : un étalonnage, des effets d'écran, puis un
// anticrénelage.
//
// 🔴 TOUS LES DÉFAUTS SONT NEUTRES — un réglage laissé à sa valeur ne change rien à l'image.
struct PostFxSettings {
  bool enabled = false;
  // Ajoutée à chaque canal.
  float brightness = 0.0F;
  // Autour du gris moyen : un, rien ne bouge.
  float contrast = 1.0F;
  // Une courbe de puissance. ⚠ PLUS GRAND ÉCLAIRCIT : c'est l'INVERSE qui sert
  // d'exposant.
  float gamma = 1.0F;
  // Zéro = gris, un = intacte.
  float saturation = 1.0F;
  // Négatif = froid, positif = chaud.
  float temperature = 0.0F;
  // Un `PostFxFilter`, rangé en entier pour le fichier.
  int filter = 0;
  float vignette = 0.0F;
  float grain = 0.0F;
  float aberration = 0.0F;
  float sharpen = 0.0F;
  bool fxaa = false;
  // Le mélange du résultat de l'anticrénelage avec l'image : un, tout le
  // résultat.
  float fxaa_strength = 0.35F;
};

// LES NOMBRES DES DEUX PASSES, que se partagent les nuanceurs et leur témoin
// calculé par le processeur (`post_fx_reference.h`).
//
// 🔴 UNE SEULE SOURCE : les nuanceurs reçoivent ces valeurs écrites dans leur
// texte au moment de leur compilation. Recopiés à la main dans le GLSL, un
// nombre changé d'un côté seulement ferait diverger le jeu de son témoin sans
// que rien ne le dise.
namespace post_fx {

// La luminance d'une couleur, pondérée comme la télévision d'avant le HD.
inline constexpr std::array<float, 3> kLuminance = {0.299F, 0.587F, 0.114F};
// L'écart de l'aberration, en part de la distance au centre, au réglage plein.
inline constexpr float kAberrationScale = 0.01F;
// Le renfort de la netteté, au réglage plein.
inline constexpr float kSharpenGain = 2.0F;
// Ce que la température ajoute au rouge et retire au bleu, au réglage plein.
inline constexpr float kTemperatureStep = 0.10F;
inline constexpr std::array<float, 3> kSepiaTint = {1.07F, 0.74F, 0.43F};
inline constexpr std::array<float, 3> kColorblindTint = {1.05F, 0.90F, 1.10F};
// Où la vignette commence à mordre et où elle est au plus sombre, en distance au
// centre DANS LE CARRÉ DES COORDONNÉES DE TEXTURE : l'ellipse suit la fenêtre.
inline constexpr float kVignetteClear = 0.35F;
inline constexpr float kVignetteFull = 0.8F;
// De combien le grain écarte un pixel, au réglage plein.
inline constexpr float kGrainSwing = 0.15F;
// Le hachage du grain : ses deux axes et son facteur.
inline constexpr std::array<float, 2> kNoiseAxis = {12.9898F, 78.233F};
inline constexpr float kNoiseScale = 43758.5453F;
// L'anticrénelage : l'écart de luminance sous lequel un pixel est laissé
// intact, en absolu puis en part du plus clair des cinq lus.
inline constexpr float kFxaaEdgeFloor = 1.0F / 16.0F;
inline constexpr float kFxaaEdgeShare = 0.125F;
// Ce que la moyenne des quatre coins retranche au dénominateur de la direction,
// et son plancher.
inline constexpr float kFxaaReduceShare = 0.125F;
inline constexpr float kFxaaReduceFloor = 1.0F / 128.0F;
// La longueur de la recherche, en texels.
inline constexpr float kFxaaSpanMost = 8.0F;
// Où tombent les lectures le long de l'arête, en part de la direction.
inline constexpr float kFxaaNearTap = 1.0F / 3.0F - 0.5F;
inline constexpr float kFxaaFarTap = 2.0F / 3.0F - 0.5F;
inline constexpr float kFxaaEndTap = 0.5F;

}  // namespace post_fx

// Les bornes d'un réglage, celles de son curseur.
struct PostFxRange {
  float lowest = 0.0F;
  float highest = 1.0F;
};

inline constexpr PostFxRange kBrightnessRange{-0.5F, 0.5F};
inline constexpr PostFxRange kContrastRange{0.5F, 2.0F};
inline constexpr PostFxRange kGammaRange{0.5F, 2.0F};
inline constexpr PostFxRange kSaturationRange{0.0F, 2.0F};
inline constexpr PostFxRange kTemperatureRange{-1.0F, 1.0F};
// Vignette, grain, aberration, netteté et force de l'anticrénelage.
inline constexpr PostFxRange kEffectRange{0.0F, 1.0F};

// Chaque réglage ramené dans sa plage, le filtre compris.
//
// ⚠ À LA LECTURE DU FICHIER, pas seulement dans les curseurs : le fichier se
// modifie à la main, et une valeur hors plage irait telle quelle au nuanceur.
[[nodiscard]] PostFxSettings Clamped(PostFxSettings settings);

// LES PRÉRÉGLAGES, dans l'ordre des boutons.
enum class PostFxPreset : int {
  kNeutral = 0,
  kNight,
  kCinema,
  kRetro,
  kBlackAndWhite,
  kCount,
};

// Les réglages d'un préréglage. ⚠ TOUS ALLUMENT LE POST-TRAITEMENT, et chacun
// part des défauts : ce qu'il ne nomme pas revient au neutre.
[[nodiscard]] PostFxSettings PresetSettings(PostFxPreset preset);

// CE QUE LA PASSE DE COULEUR REÇOIT, en quatre vecteurs.
struct ColorPassConstants {
  // Luminosité, contraste, inverse du gamma, saturation.
  std::array<float, 4> grade{};
  // Température, filtre, vignette, grain.
  std::array<float, 4> tone{};
  // Aberration, netteté, secondes du grain, rien.
  std::array<float, 4> effects{};
  // La taille d'un texel de la source, en largeur puis en hauteur.
  std::array<float, 2> texel{};
};

// Les constantes d'une image.
//
// ⚠ LE GAMMA SE PLANCHE À UN DIXIÈME AVANT D'ÊTRE INVERSÉ, et l'horloge du grain
// se replie à soixante-cinq secondes : un nombre qui grandit sans fin sous un
// sinus perd ses décimales et fige le grain.
[[nodiscard]] ColorPassConstants ColorPassConstantsOf(const PostFxSettings& settings,
                                                      std::uint32_t now_ms, int source_width,
                                                      int source_height);

}  // namespace ro::render
