#pragma once

#include <cstdint>
#include <optional>

// LA CADENCE DU PLAFOND D'IMAGES : l'instant où l'image suivante peut partir.
namespace moonclient {

inline constexpr std::uint64_t kNanosecondsPerSecond = 1'000'000'000;

// ⚠ DES ÉCHÉANCES CUMULÉES, et non une durée par image. La n-ième image de la
// seconde part à `ancre + n × 10⁹ / plafond` nanosecondes, calculé d'un bloc :
// une durée arrondie puis répétée accumulerait son erreur — mille divisé par
// 144 donne 6 ms en entiers, soit 166 images par seconde au lieu de 144.
// L'ancre avance d'une seconde exacte à chaque seconde pleine.
//
// 🔴 UN RETARD DE PLUS D'UNE IMAGE RECALE L'ANCRE sur maintenant. Le rattraper
// ferait partir d'un coup, sans attente, toutes les images en retard : une
// rafale après chaque accroc, là où l'on voulait une cadence.
//
// L'horloge est INJECTÉE : l'appelant passe l'instant, en nanosecondes d'une
// horloge monotone, et attend lui-même.
class FramePacer {
 public:
  // L'instant où l'image suivante peut partir, appelé une fois par image,
  // l'image présentée. Rien quand il n'y a pas de plafond, ou quand l'échéance
  // est déjà passée : l'image suivante part aussitôt.
  [[nodiscard]] std::optional<std::uint64_t> NextFrameStartNs(std::uint64_t now_ns,
                                                              int frames_per_second);

 private:
  std::uint64_t anchor_ns_ = 0;
  // Les images déjà cadencées depuis l'ancre, de zéro au plafond exclu.
  std::uint64_t frames_since_anchor_ = 0;
  // Le plafond sous lequel l'ancre a été posée ; zéro tant qu'il n'y en a pas.
  int anchored_frames_per_second_ = 0;
};

}  // namespace moonclient
