#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"

namespace ro::formats {

// Le format des EFFETS animés du jeu : des couches de billboards texturés, dont
// chaque image clé donne la géométrie, la texture, la couleur et le mélange.
//
// C'est la SEULE des deux familles d'effets du client qui soit pilotée par les
// données. L'autre — les particules — est codée en dur, une classe par sort,
// et ne se porte pas. Voir la mémoire du projet.
//
// Référence de lecture : GRFEditor, GRF/FileFormats/StrFormat/.

// Comment une image clé enchaîne sur la suivante : e_animation_type du client,
// tel que GRFEditor le nomme.
enum class StrAnimationType : std::int32_t {
  kStop = 0,
  kInterpolation = 1,
  kOnce = 2,
  kLoop = 3,
  kReverseLoop = 4,
  kBiLoop = 5,
};

// Une image clé d'une couche.
//
// Champs BRUTS, sans interprétation géométrique : composer le quad est
// l'affaire de qui dessine. Les quatre abscisses viennent avant les quatre
// ordonnées, et non par sommet.
struct StrKeyFrame {
  // L'image de l'animation où cette clé se pose.
  std::int32_t frame_index = 0;
  // Zéro pour une clé POSÉE, un pour une clé d'interpolation, qui ne porte que
  // l'instant vers lequel la précédente se déforme.
  std::int32_t type = 0;
  // Le centre de la couche, en pixels d'écran, autour de l'origine ci-dessous.
  float offset_x = 0.0F;
  float offset_y = 0.0F;
  // La portion de texture : origine puis ÉTENDUE, en indices 0 à 3 ; les quatre
  // suivants ne servent pas au rendu du client de référence.
  std::array<float, 8> uv = {};
  // Les quatre coins, abscisses puis ordonnées, en pixels autour du centre.
  std::array<float, 8> corners = {};
  // Laquelle des textures de la couche, en flottant dans le fichier.
  float texture_index = 0.0F;
  StrAnimationType animation = StrAnimationType::kStop;
  float delay = 0.0F;
  // En DEGRÉS. Le fichier l'écrit en 1024e de tour ; la conversion est faite à
  // la lecture, parce qu'aucun autre endroit n'a de raison de la connaître.
  float angle_degrees = 0.0F;
  // Rouge, vert, bleu, alpha, de zéro à 255 — et non de zéro à un.
  std::array<float, 4> color = {};
  // Les facteurs de mélange, dans la numérotation de Direct3D.
  std::int32_t blend_source = 0;
  std::int32_t blend_destination = 0;
  std::int32_t multi_texture = 0;
};

// Une couche : ses textures, et les images clés qui les animent.
struct StrLayer {
  // Des noms de FICHIER, relatifs au dossier du .str lui-même.
  std::vector<std::string> texture_names;
  std::vector<StrKeyFrame> key_frames;
};

// L'origine d'écran autour de laquelle un effet se compose, en pixels.
//
// Le fichier donne des positions absolues dans une fenêtre de référence ; c'est
// cette origine qu'on retranche pour les ramener autour du point où l'effet est
// joué. C'est le CENTRE d'un écran de six cent quarante sur quatre cent
// quatre-vingts, et le client le retranche tel quel : « offset moins 320 » sur
// les abscisses, « offset moins 240 » sur les ordonnées.
//
// La donnée le confirme : sur les trois mille deux cent quatre-vingt-deux
// couches d'un échantillon de quatre cent vingt-six effets, l'abscisse médiane
// vaut exactement 320, l'ordonnée moyenne 241,6, et deux cent quatre-vingt-douze
// couches ouvrent exactement sur (320, 240) contre dix sur (320, 290).
inline constexpr float kStrOriginX = 320.0F;
inline constexpr float kStrOriginY = 240.0F;

// Un effet entier.
class StrEffect {
 public:
  [[nodiscard]] static core::Result<StrEffect> Parse(std::span<const std::uint8_t> bytes);

  [[nodiscard]] std::uint8_t major_version() const noexcept { return major_version_; }
  [[nodiscard]] std::uint8_t minor_version() const noexcept { return minor_version_; }
  // Images par seconde de l'animation.
  [[nodiscard]] std::int32_t frames_per_second() const noexcept { return frames_per_second_; }
  // La DERNIÈRE image de l'animation ; elle en compte donc une de plus.
  [[nodiscard]] std::int32_t last_frame() const noexcept { return last_frame_; }
  [[nodiscard]] std::int32_t frame_count() const noexcept { return last_frame_ + 1; }
  [[nodiscard]] const std::vector<StrLayer>& layers() const noexcept { return layers_; }

 private:
  std::uint8_t major_version_ = 0;
  std::uint8_t minor_version_ = 0;
  std::int32_t frames_per_second_ = 0;
  std::int32_t last_frame_ = 0;
  std::vector<StrLayer> layers_;
};

}  // namespace ro::formats
