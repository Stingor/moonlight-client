#include "afk/afk_screen.h"

#include <algorithm>
#include <cmath>

namespace moonclient {
namespace {

constexpr float kMillisecondsPerSecond = 1000.0F;
constexpr float kFullTurnDegrees = 360.0F;
constexpr float kHalfTurnDegrees = 180.0F;

// LA RAMPE LA PLUS COURTE QU'ON ACCEPTE DE DÉROULER. Sous ce seuil, la division
// par la durée renverrait l'infini — et le réglage, lui, ne descend pas si bas.
constexpr float kShortestEaseSeconds = 0.05F;

// CE QU'UNE IMAGE PEUT DURER AU PLUS, pour l'orbite.
//
// ⚠ UNE IMAGE ANORMALEMENT LONGUE — un chargement, une fenêtre réduite, un
// point d'arrêt — ferait sauter l'orbite d'un quart de tour d'un coup. On borne
// le pas, pas l'horloge : le temps perdu l'est, et le rattraper ferait
// justement le saut qu'on évite.
constexpr float kLongestStepSeconds = 0.25F;

// La rampe en S : départ et arrivée sans à-coup.
[[nodiscard]] float Smooth(float ratio) noexcept { return ratio * ratio * (3.0F - 2.0F * ratio); }

// Ramène un angle dans le demi-tour le plus proche, dans un sens comme dans
// l'autre.
[[nodiscard]] float Shortest(float degrees) noexcept {
  float folded = std::fmod(degrees, kFullTurnDegrees);
  if (folded > kHalfTurnDegrees) {
    folded -= kFullTurnDegrees;
  }
  if (folded < -kHalfTurnDegrees) {
    folded += kFullTurnDegrees;
  }
  return folded;
}

}  // namespace

bool AfkScreen::hiding_ui(const AfkSettings& settings) const noexcept {
  return settings.hide_ui && (phase_ == AfkPhase::kEntering || phase_ == AfkPhase::kHeld);
}

bool AfkScreen::hiding_cursor(const AfkSettings& settings) const noexcept {
  return settings.hide_cursor && (phase_ == AfkPhase::kEntering || phase_ == AfkPhase::kHeld);
}

ro::render::ScreenTintLook AfkScreen::tint(const AfkSettings& settings) const noexcept {
  return ro::render::ScreenTintLook{
      .vignette = std::clamp(settings.vignette, 0.0F, 1.0F) * blend_,
      .desaturate = std::clamp(settings.desaturate, 0.0F, 1.0F) * blend_,
      .grain = std::clamp(settings.grain, 0.0F, 1.0F) * blend_};
}

float AfkScreen::EaseBlend(const AfkSettings& settings, std::uint32_t now_ms) const noexcept {
  if (phase_ == AfkPhase::kAwake) {
    return 0.0F;
  }
  if (phase_ == AfkPhase::kHeld) {
    return 1.0F;
  }
  float ease_s = std::max(settings.ease_s, kShortestEaseSeconds);
  if (phase_ == AfkPhase::kLeaving) {
    ease_s /= kAfkWakeSpeedup;
  }
  const float spent = static_cast<float>(now_ms - phase_started_ms_) / kMillisecondsPerSecond;
  const float ratio = std::clamp(spent / ease_s, 0.0F, 1.0F);
  return phase_ == AfkPhase::kLeaving ? 1.0F - Smooth(ratio) : Smooth(ratio);
}

void AfkScreen::Begin(AfkIdle& idle, const ro::render::Camera& camera, std::uint32_t now_ms) {
  base_pitch_ = camera.pitch_degrees;
  base_yaw_ = camera.yaw_degrees;
  base_distance_ = camera.distance;
  base_taken_ = true;

  spin_degrees_ = 0.0F;
  phase_ = AfkPhase::kEntering;
  phase_started_ms_ = now_ms;
  sleep_started_ms_ = now_ms;
  last_step_ms_ = now_ms;
  blend_ = 0.0F;
  idle.SetSleeping(true);
}

void AfkScreen::StartNow(AfkIdle& idle, const ro::render::Camera& camera, std::uint32_t now_ms) {
  if (phase_ != AfkPhase::kAwake) {
    return;
  }
  Begin(idle, camera, now_ms);
}

void AfkScreen::Wake(AfkIdle& idle, std::uint32_t now_ms) {
  if (phase_ == AfkPhase::kAwake || phase_ == AfkPhase::kLeaving) {
    return;
  }
  // L'orbite accumulée est ramenée au plus court : après vingt minutes de veille
  // elle peut valoir plusieurs tours, et les dérouler à l'envers ferait tourner
  // le décor pendant un temps absurde alors que le joueur veut jouer.
  spin_at_wake_ = Shortest(spin_degrees_);
  spin_degrees_ = spin_at_wake_;
  phase_ = AfkPhase::kLeaving;
  phase_started_ms_ = now_ms;
  idle.SetSleeping(false);
}

void AfkScreen::Restore(ro::render::Camera& camera) const {
  if (!base_taken_) {
    return;
  }
  // ⚠ LA POSE DE DÉPART SE REPOSE EXACTEMENT, et non « à peu près » : la rampe
  // a beau finir à zéro, elle y arrive par un calcul en virgule flottante. Sans
  // cette dernière écriture, chaque veille décalerait la caméra d'un cheveu, et
  // vingt veilles d'un angle bien visible.
  camera.pitch_degrees = base_pitch_;
  camera.yaw_degrees = base_yaw_;
  camera.distance = base_distance_;
}

void AfkScreen::EndNow(AfkIdle& idle, ro::render::Camera& camera) {
  if (phase_ == AfkPhase::kAwake) {
    return;
  }
  Restore(camera);
  base_taken_ = false;
  phase_ = AfkPhase::kAwake;
  blend_ = 0.0F;
  idle.SetSleeping(false);
}

void AfkScreen::PoseCamera(const AfkSettings& settings, ro::render::Camera& camera, float seconds) {
  if (!base_taken_) {
    return;
  }

  if (phase_ == AfkPhase::kLeaving) {
    // L'orbite se résorbe avec la rampe, en même temps que le recul et la
    // plongée : tout revient d'un seul mouvement.
    spin_degrees_ = spin_at_wake_ * blend_;
  } else {
    spin_degrees_ += settings.spin_deg_s * seconds;
    // ⚠ REPLIÉ POUR LUI SEUL. Ce n'est pas l'image qui en a besoin — un angle
    // de mille degrés se dessine comme deux cent quatre-vingts — mais le
    // FLOTTANT : une veille laissée tourner une nuit accumulerait deux millions
    // de degrés, et l'incrément de six finirait par se perdre dans les
    // décimales que la mantisse n'a plus.
    spin_degrees_ = std::fmod(spin_degrees_, kFullTurnDegrees);
  }

  // 🔴 LE RECUL PART DU RECUL DE JEU, ET NON DE CELUI DU JOUEUR. La distance
  // COURANTE au moment où la veille bascule n'est pas une habitude, c'est là
  // où la molette se trouvait : la mise en scène changerait de cadrage à
  // chaque endormissement, et rien à l'écran ne dirait pourquoi.
  // `kGameViewDistance` ne bouge pas : la veille rend la même image,
  // que le joueur se soit rapproché ou éloigné juste avant.
  //
  // ⚠ ET ELLE NE « RECULE » PLUS TOUJOURS : depuis une vue très éloignée, elle
  // RAPPROCHE pour atteindre sa pose. C'est le prix d'un cadrage fixe, et c'est
  // ce qu'on veut — le départ de la rampe reste la pose du joueur.
  const float wanted_distance =
      ro::render::kGameViewDistance * std::clamp(settings.zoom_factor, kLeastAfkZoom, kMostAfkZoom);
  // Bornée au plafond que la molette elle-même ne dépasse pas : la veille
  // n'emmène pas la caméra là où le joueur ne pourrait pas la poser.
  const float distance = std::min(wanted_distance, ro::render::kMaxCameraDistance);

  // 🔴 LA PLONGÉE EST POSITIVE, ET C'EST L'INVERSE DU CLIENT D'ORIGINE. Notre
  // champ se lit « hauteur au-dessus de l'horizon » et `Camera::Position` en
  // tire `y = look_at.y + distance × sin(élévation)` : l'œil monte. Le moteur
  // du client veut l'opposé — son axe vertical pointe vers le bas —, si bien
  // que son repère écrit `-tilt` là où nous écrivons `tilt`. Recopier son signe
  // enverrait la caméra SOUS le terrain, et lui ferait montrer le dessous du
  // monde. Voir aussi `shell/backdrop_camera.cpp`, qui porte la même note.
  const float wanted_pitch =
      std::clamp(settings.tilt_deg, kLeastAfkTiltDegrees, kMostAfkTiltDegrees);

  camera.pitch_degrees = base_pitch_ + (wanted_pitch - base_pitch_) * blend_;
  // ⚠ RETRANCHÉE : notre cap est l'OPPOSÉ de celui du client (`NativeViewOf`).
  // Le réglage garde le sens du client — positif, la vue tourne comme la
  // sienne — et c'est ici que le signe se retourne.
  camera.yaw_degrees = base_yaw_ - spin_degrees_;
  camera.distance = base_distance_ + (distance - base_distance_) * blend_;
}

void AfkScreen::Advance(const AfkSettings& settings, AfkIdle& idle, ro::render::Camera& camera,
                        std::uint32_t now_ms) {
  // Le filtre d'entrées n'a pas d'autre moyen d'atteindre les réglages : on les
  // lui repose à chaque image, comme l'état de veille.
  idle.SetWakeMode(settings.wake_on);

  if (phase_ == AfkPhase::kAwake) {
    if (!settings.enabled) {
      return;
    }
    const auto delay_ms = static_cast<std::uint32_t>(std::clamp(
                              settings.delay_s, kLeastAfkDelaySeconds, kMostAfkDelaySeconds)) *
                          1000U;
    if (idle.IdleMs(now_ms) >= delay_ms) {
      Begin(idle, camera, now_ms);
    }
    return;
  }

  // EN VEILLE : LA MOINDRE ENTRÉE RÉVEILLE.
  //
  // ⚠ `settings.enabled` NE FIGURE PAS DANS CE TEST, et ce n'est pas un oubli :
  // il gouverne le DÉCLENCHEMENT automatique, pas la veille en cours. L'y
  // remettre couperait net toute veille lancée à la main — au raccourci ou au
  // bouton d'essai — chez quiconque n'a pas coché la mise en veille
  // automatique, c'est-à-dire précisément ceux à qui ce geste sert.
  if (idle.WokeSince(sleep_started_ms_)) {
    Wake(idle, now_ms);
  }

  float seconds = static_cast<float>(now_ms - last_step_ms_) / kMillisecondsPerSecond;
  last_step_ms_ = now_ms;
  seconds = std::clamp(seconds, 0.0F, kLongestStepSeconds);

  blend_ = EaseBlend(settings, now_ms);
  PoseCamera(settings, camera, seconds);

  if (phase_ == AfkPhase::kEntering && blend_ >= 1.0F) {
    phase_ = AfkPhase::kHeld;
  }
  if (phase_ == AfkPhase::kLeaving && blend_ <= 0.0F) {
    // 🔴 AUCUNE POSE À REMETTRE ICI : la caméra ne lisse pas, la pose écrite
    // EST celle qui se dessine, et la rampe de sortie atteint exactement zéro
    // (`1 - Smooth(1)`), si bien que `PoseCamera` vient de rendre la pose de
    // repos AU BIT PRÈS.
    //
    // ⚠ UNE REMISE ICI NE SERAIT ATTEINTE PAR AUCUN ESSAI, et une garde
    // qu'aucun essai n'atteint ment sur ce qu'elle protège. `EndNow`, lui,
    // garde la sienne — il coupe EN PLEINE rampe, et c'est un essai qui l'y
    // tient.
    base_taken_ = false;
    phase_ = AfkPhase::kAwake;
  }
}

std::uint8_t AfkScreen::WantedAnnounce(const AfkSettings& settings,
                                       std::uint32_t idle_ms) const noexcept {
  if (!settings.announce) {
    return 0;
  }
  const auto delay_ms =
      static_cast<std::uint32_t>(
          std::clamp(settings.announce_delay_s, kLeastAfkDelaySeconds, kMostAfkAnnounceSeconds)) *
      1000U;
  if (idle_ms < delay_ms) {
    return 0;
  }
  std::uint8_t mask = 0;
  if (settings.announce_zzz) {
    mask |= kAfkAnnounceSleep;
  }
  if (settings.announce_tag) {
    mask |= kAfkAnnounceTag;
  }
  return mask;
}

}  // namespace moonclient
