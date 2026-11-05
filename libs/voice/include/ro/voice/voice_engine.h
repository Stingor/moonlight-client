#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include "ro/voice/voice_link.h"

namespace ro::voice {

// Le pas du fil de la voix : quatre par trame de 20 ms, pour qu'un paquet
// reçu ou une trame du micro n'attende jamais longtemps.
inline constexpr std::uint32_t kVoiceStepMs = 5;

// Au-delà, la boucle de jeu est figée : la touche « Parler » qu'elle disait
// enfoncée ne se lit plus, et le micro se referme au lieu de rester ouvert.
inline constexpr std::uint32_t kVoiceMostSteerAgeMs = 500;

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
  // Un ORDRE, et non un état : il part une fois, au pas suivant du fil.
  void Block(std::uint32_t character_id, bool blocked);
  [[nodiscard]] VoiceStatus status() const;
  // LA COOP SURVIVORS, par le même fil : une trame à envoyer, et celles reçues.
  void PostCoop(std::vector<std::uint8_t> payload);
  [[nodiscard]] std::vector<CoopRelayed> TakeCoop();

 private:
  struct BlockOrder {
    std::uint32_t character_id = 0;
    bool blocked = false;
  };

  void Run();

  VoiceLink link_;
  mutable std::mutex guard_;
  VoiceControls controls_;
  std::chrono::steady_clock::time_point steered_at_;
  std::vector<BlockOrder> block_orders_;
  std::vector<std::vector<std::uint8_t>> coop_to_send_;
  std::vector<CoopRelayed> coop_received_;
  VoiceStatus status_;
  std::atomic<bool> stopping_ = false;
  std::thread worker_;
};

}  // namespace ro::voice
