#pragma once

#include <glm/glm.hpp>

#include "ro/render/scene_light.h"

namespace ro::render {

// LA NUIT D'UNE CARTE : un fondu de la lumière diffuse, et non un voile.
//
// 🔴 SEULS LE ROUGE ET LE VERT DESCENDENT, et le bleu reste : c'est ce qui rend
// la nuit bleutée. Chaque image du client, une composante au-dessus du plancher
// perd un cran ; une composante déjà au plancher ou en dessous ne bouge pas.
// Le jour, chaque composante remonte d'un cran tant qu'elle reste sous la
// diffuse du RSW ET sous un — elle peut donc la dépasser d'un cran au plus.
//
// La lumière qui en sort ne teinte que le SOL et le DÉCOR. Sprites, effets, eau
// et interface ne la voient pas.
inline constexpr float kNightDiffuseFloor = 0.5F;
inline constexpr float kDayNightStep = 0.005F;

// UNE IMAGE DU CLIENT, en millisecondes : le fondu avance d'un cran par image
// qu'il dessine, et l'on compte soixante images par seconde, l'hypothèse du
// voile du décor. Environ cent une images, donc un peu moins de deux secondes,
// séparent le plein jour de la nuit.
inline constexpr float kDayNightFrameMs = 1000.0F / 60.0F;

// Au-delà, une longue absence (fenêtre réduite, chargement) ne rejoue pas ses
// images une à une : le fondu tient tout entier dans ce compte.
inline constexpr int kMostDayNightFrames = 256;

class DayNightLight {
 public:
  // UNE CARTE CHARGÉE : la diffuse repart de celle du RSW, SANS fondu, que la
  // nuit tienne ou non. Si elle tient, la carte s'affiche en plein jour puis
  // s'assombrit.
  void Enter(const SceneLight& map_light);

  // Le temps écoulé depuis l'appel précédent, et si la nuit tient sur le joueur.
  void Advance(bool night, float elapsed_ms);

  // UNE IMAGE DU CLIENT, sans horloge.
  void Step(bool night);

  [[nodiscard]] const glm::vec3& diffuse() const noexcept { return diffuse_; }

  // CE QUE LE SOL ET LE DÉCOR MULTIPLIENT À LEUR LUMIÈRE, canal par canal. La
  // lumière d'une carte vaut 1 − (1 − ambiante) × (1 − diffuse) ; la teinte est
  // le rapport de celle de l'instant à celle du RSW. Un en plein jour : le
  // dessin de jour ne change pas.
  [[nodiscard]] glm::vec3 Tint() const;

  // La lumière du RSW, sa teinte posée.
  [[nodiscard]] SceneLight Lit(const SceneLight& map_light) const;

 private:
  glm::vec3 diffuse_{1.0F, 1.0F, 1.0F};
  glm::vec3 map_diffuse_{1.0F, 1.0F, 1.0F};
  glm::vec3 map_ambient_{0.0F, 0.0F, 0.0F};
  float pending_ms_ = 0.0F;
};

}  // namespace ro::render
