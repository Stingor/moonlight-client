#pragma once

#include <array>
#include <cstdint>

#include "ro/render/camera.h"
#include "ro/render/post_fx.h"

namespace ro::render {

// LE FLOU DE MOUVEMENT DE LA CAMÉRA : il n'agit QUE quand la caméra TOURNE.
//
// 🔴 PAS DE FLOU QUAND ELLE SUIT LE PERSONNAGE. La caméra vise le joueur : un
// flou par vitesse de chaque pixel brouillerait tout le décor, joueur compris,
// à chaque pas. Seule la ROTATION — le lacet, et l'inclinaison quand elle
// glisse — étire l'image, autour du point visé.
//
// ⚠ UN DÉPLACEMENT D'UN CRAN N'EST PAS UN MOUVEMENT : un cran de molette, une
// remise de la caméra ou un changement de carte la posent ailleurs d'un coup.
// C'est l'appelant qui dit si la rotation vient d'un geste continu.
struct SpinBlurSettings {
  bool enabled = false;
  // La part de l'exposition pleine : zéro, aucun flou ; un, la plus longue.
  float strength = 0.5F;
};

inline constexpr PostFxRange kSpinBlurStrengthRange{0.0F, 1.0F};

namespace spin_blur {

// Sous cette vitesse de rotation, rien : une caméra qui dérive à peine reste nette.
inline constexpr float kFloorDegreesPerSecond = 30.0F;
// Le temps de pose au réglage plein : la rotation qu'il couvre est le balayage.
inline constexpr float kExposureSeconds = 1.0F / 30.0F;
// Le plus grand balayage, en degrés : au-delà, l'image ne se lit plus.
inline constexpr float kMostSweepDegrees = 4.0F;
// La constante de temps du lissage de la vitesse. Une souris lue moins souvent
// que l'écran ne se peint laisse des images sans mouvement au milieu d'un
// geste : sans lissage, le flou y clignoterait.
inline constexpr float kSmoothingMs = 40.0F;
// Les lectures le long de l'arc.
inline constexpr int kTaps = 16;
// Le plancher de l'écrasement vertical : une caméra rasante écraserait l'ellipse
// en une droite, et la division qui la déplie exploserait.
inline constexpr float kSquashFloor = 0.1F;

}  // namespace spin_blur

// La vitesse de rotation de la caméra, en degrés par seconde.
struct CameraTurnRate {
  float yaw_degrees_per_second = 0.0F;
  float pitch_degrees_per_second = 0.0F;
};

// L'écart le plus court d'un angle à un autre, dans [-180, 180) : 359° → 1°
// donne 2°, pas -358°.
[[nodiscard]] float ShortestTurnDegrees(float from_degrees, float to_degrees);

// Ce qu'une rotation balaie pendant la pose, en degrés : nul sous le seuil,
// proportionnel à ce qui le dépasse, borné par `kMostSweepDegrees`.
struct SpinSweep {
  float yaw_degrees = 0.0F;
  float pitch_degrees = 0.0F;
};

[[nodiscard]] SpinSweep SpinSweepOf(const SpinBlurSettings& settings, const CameraTurnRate& rate);

// CE QUE LA PASSE DU FLOU REÇOIT pour une image.
struct SpinBlurFrame {
  // L'arc parcouru autour du point visé, en radians, de bout en bout.
  float arc_radians = 0.0F;
  // L'étirement vertical autour du point visé, en part de la distance, de bout
  // en bout : ce que glisse l'inclinaison.
  float stretch = 0.0F;
  // Le point visé à l'écran, en coordonnées de texture.
  std::array<float, 2> pivot_uv = {0.5F, 0.5F};
  // Ce que l'inclinaison écrase de la verticale : un cercle du sol se voit en
  // ellipse de cette hauteur.
  float squash = 1.0F;

  [[nodiscard]] bool active() const noexcept { return arc_radians != 0.0F || stretch != 0.0F; }
};

// Le flou d'une image : le balayage de cette vitesse, autour du point que la
// caméra vise projeté à l'écran. Inactif quand le réglage est éteint.
[[nodiscard]] SpinBlurFrame SpinBlurFrameOf(const SpinBlurSettings& settings,
                                            const CameraTurnRate& rate, const Camera& camera,
                                            float aspect_ratio);

// LA VITESSE DE ROTATION, mesurée d'une image à l'autre et lissée dans le temps.
//
// ⚠ AUCUNE HORLOGE : l'instant de l'image est donné, celui que tout le dessin
// partage. Le lissage se compte en temps et non en images : le même geste
// donne le même flou à 60 comme à 240 images par seconde.
class CameraSpinMeter {
 public:
  // Retient l'orientation de cette image et rend la vitesse lissée.
  //
  // `turned_by_hand` : la rotation depuis l'image précédente vient d'un geste
  // continu. Faux, l'écart n'est pas un mouvement — un cran, une remise —, et
  // compte pour une vitesse nulle.
  CameraTurnRate Measure(float yaw_degrees, float pitch_degrees, bool turned_by_hand,
                         std::uint32_t now_ms);

  // Oublie l'image précédente : la prochaine mesure part de zéro.
  void Forget() noexcept { started_ = false; }

 private:
  bool started_ = false;
  float yaw_degrees_ = 0.0F;
  float pitch_degrees_ = 0.0F;
  std::uint32_t last_ms_ = 0;
  CameraTurnRate smoothed_;
};

}  // namespace ro::render
