#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

#include "ro/voice/voice_link.h"

namespace ro::voice {

// Le pas du fil de la voix : quatre par trame de 20 ms, pour qu'un paquet
// reçu ou une trame du micro n'attende jamais longtemps.
inline constexpr std::uint32_t kVoiceStepMs = 5;

// ── LA VOIX SUR SON PROPRE FIL ──────────────────────────────────────────────
//
// 🔴 LA BOUCLE DE JEU SE FIGE au chargement d'une carte ; la voix ne doit pas
// se figer avec elle. Le fil fait avancer une `VoiceLink` à lui seul, et la
// partie ne lui parle que par deux portes gardées : ce que le joueur demande,
// et ce que la liaison rend.
//
// La destruction arrête le fil et l'attend : au plus le délai d'une connexion
// en cours.
class VoiceEngine {
 public:
  VoiceEngine(VoiceTarget target, VoiceDevices devices);
  ~VoiceEngine();

  VoiceEngine(const VoiceEngine&) = delete;
  VoiceEngine& operator=(const VoiceEngine&) = delete;

  void Steer(const VoiceControls& controls);
  [[nodiscard]] VoiceStatus status() const;

 private:
  void Run();

  VoiceLink link_;
  mutable std::mutex guard_;
  VoiceControls controls_;
  VoiceStatus status_;
  std::atomic<bool> stopping_ = false;
  std::thread worker_;
};

}  // namespace ro::voice
