#include "capture/zone_recorder.h"

#include <algorithm>
#include <utility>

#include "ro/core/error.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/formats/gif.h"

#include "capture/capture_clipboard.h"

namespace moonclient {
namespace {

// Combien de temps un mot flottant reste à l'écran. Assez pour se lire sans
// quitter le jeu des yeux, assez court pour ne pas traîner sur la prise
// suivante.
constexpr std::uint32_t kMessageMs = 3000;

// Le délai d'une image, en centièmes de seconde — l'unité du format.
[[nodiscard]] int DelayOf(int frames_per_second) {
  return std::max(ro::formats::kFastestGifDelayCs, 100 / std::max(1, frames_per_second));
}

}  // namespace

ZoneRecorder::~ZoneRecorder() {
  // ⚠ ON ATTEND LE FIL PLUTÔT QUE DE L'ABANDONNER : il écrit dans des tampons
  // que ce destructeur s'apprête à libérer.
  if (encoder_.joinable()) {
    encoder_.join();
  }
}

void ZoneRecorder::Say(const char* what) {
  message_ = what;
  message_ms_ = 0;
}

void ZoneRecorder::AskZone() {
  switch (phase_) {
    case ro::ui::ZoneCapturePhase::kSelecting:
      // Un second appel referme : la même touche ouvre et annule.
      phase_ = ro::ui::ZoneCapturePhase::kIdle;
      return;
    case ro::ui::ZoneCapturePhase::kIdle:
    case ro::ui::ZoneCapturePhase::kCountdown:
      // ⚠ LE DÉCOMPTE SE RENONCE : c'est précisément le moment où l'on voit que
      // le cadrage est mauvais.
      frames_.clear();
      phase_ = ro::ui::ZoneCapturePhase::kSelecting;
      return;
    case ro::ui::ZoneCapturePhase::kRecording:
    case ro::ui::ZoneCapturePhase::kEncoding:
      // 🔴 REFUSÉ, ET ON LE DIT : toutes les images d'un GIF ont la même taille,
      // donc recadrer en cours de route casserait le clip.
      Say(ro::core::Tr("Recadrage impossible pendant un enregistrement."));
      return;
  }
}

std::optional<ro::ui::ZoneRect> ZoneRecorder::Note(const ro::ui::ZoneCaptureAsk& asked) {
  if (phase_ != ro::ui::ZoneCapturePhase::kSelecting) {
    return std::nullopt;
  }
  if (asked.cancelled) {
    phase_ = ro::ui::ZoneCapturePhase::kIdle;
    return std::nullopt;
  }
  if (!asked.drawn) {
    return std::nullopt;
  }
  phase_ = ro::ui::ZoneCapturePhase::kIdle;
  if (asked.zone.width < ro::ui::kSmallestZone || asked.zone.height < ro::ui::kSmallestZone) {
    // Un clic qui a glissé de trois pixels n'est pas une zone.
    Say(ro::core::Tr("Zone trop petite."));
    return std::nullopt;
  }
  how_.zone = asked.zone;
  return asked.zone;
}

bool ZoneRecorder::Arm(std::uint32_t now_ms) {
  if (how_.zone.empty()) {
    Say(ro::core::Tr("Aucune zone définie."));
    return false;
  }
  if (ro::ui::CaptureBytes(how_) > ro::ui::kLargestCaptureBytes) {
    Say(
        ro::core::Tr("Trop long ou trop large pour la mémoire : réduis la durée ou la "
                     "largeur."));
    return false;
  }
  const ro::ui::ZoneRect out = ro::ui::OutputSize(how_.zone, how_.widest_output);
  auto grab = ro::render::ScreenGrab::Create(out.width, out.height);
  if (!grab) {
    // EN ALERTE : rien n'est engagé — la phase reste au repos, aucune image
    // n'existe — et le joueur lit la phrase ci-dessous puis peut réessayer
    // aussitôt.
    ro::core::LogWarning("capture impossible : {}", ro::core::Describe(grab.error()));
    Say(ro::core::Tr("La carte graphique a refusé la capture."));
    return false;
  }
  grab_.emplace(std::move(*grab));

  wanted_frames_ = std::max(1, how_.frames_per_second * how_.seconds);
  frames_.clear();
  frames_.reserve(static_cast<std::size_t>(wanted_frames_));
  interval_ms_ = static_cast<std::uint32_t>(std::max(1, 1000 / how_.frames_per_second));
  countdown_end_ms_ = now_ms + static_cast<std::uint32_t>(how_.start_delay_seconds) * 1000U;
  phase_ = ro::ui::ZoneCapturePhase::kCountdown;
  message_.clear();
  return true;
}

void ZoneRecorder::Start(std::uint32_t now_ms) {
  phase_ = ro::ui::ZoneCapturePhase::kRecording;
  next_grab_ms_ = now_ms;
}

void ZoneRecorder::ToggleRecording(std::uint32_t now_ms) {
  switch (phase_) {
    case ro::ui::ZoneCapturePhase::kIdle:
      static_cast<void>(Arm(now_ms));
      return;
    case ro::ui::ZoneCapturePhase::kCountdown:
    case ro::ui::ZoneCapturePhase::kRecording:
      // Un second appui arrête avant la fin : c'est ce qu'on fait quand la
      // scène qu'on voulait montrer est finie plus tôt que prévu.
      Finish();
      return;
    case ro::ui::ZoneCapturePhase::kSelecting:
    case ro::ui::ZoneCapturePhase::kEncoding:
      return;
  }
}

void ZoneRecorder::AskPicture() {
  if (busy() || how_.zone.empty()) {
    if (how_.zone.empty()) {
      Say(ro::core::Tr("Aucune zone définie."));
    }
    return;
  }
  picture_due_ = true;
}

void ZoneRecorder::Cancel() {
  if (phase_ == ro::ui::ZoneCapturePhase::kCountdown ||
      phase_ == ro::ui::ZoneCapturePhase::kRecording ||
      phase_ == ro::ui::ZoneCapturePhase::kSelecting) {
    frames_.clear();
    phase_ = ro::ui::ZoneCapturePhase::kIdle;
  }
}

void ZoneRecorder::Finish() {
  if (frames_.empty()) {
    phase_ = ro::ui::ZoneCapturePhase::kIdle;
    Say(ro::core::Tr("Rien n'a été enregistré."));
    return;
  }
  phase_ = ro::ui::ZoneCapturePhase::kEncoding;
  encoded_.store(false);
  const int delay = DelayOf(how_.frames_per_second);
  // ⚠ LES IMAGES SONT CÉDÉES AU FIL, pas partagées : il en est seul lecteur
  // jusqu'à ce qu'il pose son drapeau.
  encoder_ = std::thread([this, delay] {
    auto bytes = ro::formats::EncodeAnimatedGif(frames_, delay);
    if (bytes) {
      bytes_ = std::move(*bytes);
    } else {
      bytes_.clear();
      ro::core::LogError("encodage du GIF impossible : {}", ro::core::Describe(bytes.error()));
    }
    // 🔴 POSÉ EN DERNIER, ET C'EST CE QUE LA BOUCLE GUETTE : tout ce que le fil
    // écrit doit être visible avant que ce drapeau ne le soit.
    encoded_.store(true);
  });
}

void ZoneRecorder::Tick(std::uint32_t now_ms) {
  if (message_ms_ == 0 && !message_.empty()) {
    message_ms_ = now_ms;
  } else if (!message_.empty() && now_ms - message_ms_ > kMessageMs) {
    message_.clear();
    message_ms_ = 0;
  }

  if (phase_ == ro::ui::ZoneCapturePhase::kCountdown) {
    // Arrondi VERS LE HAUT : un décompte qui affiche « 0 » pendant une seconde
    // se lit comme une panne.
    seconds_left_ = static_cast<int>((countdown_end_ms_ - now_ms + 999U) / 1000U);
    if (now_ms >= countdown_end_ms_) {
      seconds_left_ = 0;
      Start(now_ms);
    }
  }
  if (phase_ == ro::ui::ZoneCapturePhase::kEncoding && encoded_.load()) {
    if (encoder_.joinable()) {
      encoder_.join();
    }
    const bool placed = !bytes_.empty() && PutAnimationOnClipboard(bytes_);
    Say(placed ? ro::core::Tr("Copié — Ctrl+V pour le coller.")
               : ro::core::Tr("Copie impossible : une autre application retient le "
                              "presse-papier."));
    message_ms_ = now_ms;
    // Les images ne servent plus, et elles pèsent : on les rend tout de suite.
    frames_.clear();
    frames_.shrink_to_fit();
    bytes_.clear();
    bytes_.shrink_to_fit();
    phase_ = ro::ui::ZoneCapturePhase::kIdle;
  }
}

ro::render::ScreenRegion ZoneRecorder::InPixels(const FrameSurface& surface) const {
  // 🔴 LES POINTS D'INTERFACE NE SONT PAS LES PIXELS DU TAMPON. Sur un écran à
  // facteur d'échelle le système en donne davantage, et une zone reportée telle
  // quelle viserait le quart haut-gauche de ce qu'on a tracé.
  const float across = surface.point_width > 0 ? static_cast<float>(surface.drawable_width) /
                                                     static_cast<float>(surface.point_width)
                                               : 1.0F;
  const float down = surface.point_height > 0 ? static_cast<float>(surface.drawable_height) /
                                                    static_cast<float>(surface.point_height)
                                              : 1.0F;
  ro::render::ScreenRegion zone{
      .left = static_cast<int>(static_cast<float>(how_.zone.left) * across),
      .top = static_cast<int>(static_cast<float>(how_.zone.top) * down),
      .width = static_cast<int>(static_cast<float>(how_.zone.width) * across),
      .height = static_cast<int>(static_cast<float>(how_.zone.height) * down)};
  // ⚠ RAMENÉE DANS L'ÉCRAN : une fenêtre rétrécie depuis le tracé laisserait la
  // zone déborder, et la relecture rendrait une bande noire.
  zone.left = std::clamp(zone.left, 0, std::max(0, surface.drawable_width - 1));
  zone.top = std::clamp(zone.top, 0, std::max(0, surface.drawable_height - 1));
  zone.width = std::clamp(zone.width, 0, surface.drawable_width - zone.left);
  zone.height = std::clamp(zone.height, 0, surface.drawable_height - zone.top);
  return zone;
}

void ZoneRecorder::GrabIfDue(const FrameSurface& surface, std::uint32_t now_ms) {
  if (picture_due_) {
    picture_due_ = false;
    const ro::render::ScreenRegion zone = InPixels(surface);
    // ⚠ À SA TAILLE RÉELLE : la largeur maximale n'existe que pour contenir le
    // poids d'une ANIMATION, et une image fixe n'a pas ce problème.
    auto grab = ro::render::ScreenGrab::Create(zone.width, zone.height);
    if (grab) {
      auto picture = grab->Take(zone, surface.drawable_height);
      if (picture) {
        Say(PutPictureOnClipboard(*picture)
                ? ro::core::Tr("Copié — Ctrl+V pour le coller.")
                : ro::core::Tr("Copie impossible : une autre application retient le "
                               "presse-papier."));
        message_ms_ = now_ms;
      }
    }
  }

  if (phase_ != ro::ui::ZoneCapturePhase::kRecording || now_ms < next_grab_ms_) {
    return;
  }
  auto image = grab_->Take(InPixels(surface), surface.drawable_height);
  if (!image) {
    // EN ALERTE : l'appel enchaîne sur `Finish()`, qui encode et copie les
    // images déjà prises. L'enregistrement est écourté, pas perdu.
    ro::core::LogWarning("relecture d'écran impossible : {} — l'enregistrement s'arrête là",
                         ro::core::Describe(image.error()));
    Finish();
    return;
  }
  frames_.push_back(std::move(*image));
  next_grab_ms_ += interval_ms_;
  // Un à-coup ne doit pas faire rattraper dix images d'un coup : on repart de
  // maintenant plutôt que d'accumuler le retard.
  if (next_grab_ms_ < now_ms) {
    next_grab_ms_ = now_ms + interval_ms_;
  }
  if (static_cast<int>(frames_.size()) >= wanted_frames_) {
    Finish();
  }
}

ro::ui::ZoneCaptureView ZoneRecorder::view(bool show_zone) const {
  ro::ui::ZoneCaptureView shown;
  shown.phase = phase_;
  shown.zone = how_.zone;
  shown.seconds_left = seconds_left_;
  shown.frames_taken = static_cast<int>(frames_.size());
  shown.frames_wanted = wanted_frames_;
  shown.show_zone = show_zone && phase_ == ro::ui::ZoneCapturePhase::kIdle;
  shown.message = message_.empty() ? nullptr : message_.c_str();
  return shown;
}

}  // namespace moonclient
