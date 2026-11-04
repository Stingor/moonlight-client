#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

#include "ro/net/connection.h"

#include "guard/guard_rules.h"

namespace moonclient {

// LE GARDE EN SERVICE : les sondes de la machine, et le signalement au serveur.
//
// Trois sondes, toutes muettes hors Windows :
//   - la provenance des touches, par l'entrée brute du clavier et un crochet
//     sur les messages de la fenêtre (thread principal) ;
//   - l'accord du compteur de performance et du temps du noyau, relevés à
//     chaque image ;
//   - les noms connus — processus, fenêtres, modules —, balayés par un thread
//     à part une fois par minute, parce qu'énumérer les processus prend du temps.
//
// Le garde vit aussi longtemps que le processus : il observe dès l'écran de
// connexion, et ne parle au serveur qu'une fois la session reconnue.
class CheatGuard {
 public:
  CheatGuard() = default;
  CheatGuard(const CheatGuard&) = delete;
  CheatGuard& operator=(const CheatGuard&) = delete;
  CheatGuard(CheatGuard&&) = delete;
  CheatGuard& operator=(CheatGuard&&) = delete;
  ~CheatGuard();

  // Arme les sondes, une fois, la fenêtre ouverte.
  void Start();

  // À chaque image, la liaison ouverte : relève les horloges, recueille les
  // trouvailles, et en dit une au serveur quand sa cadence le permet.
  void Pump(ro::net::Connection& link, bool session_verified, std::uint32_t now_ms);

  // Une touche vue par les sondes de la fenêtre : appelé par le crochet.
  void NoteRawKey(bool has_device) { keys_.NoteRawKey(has_device); }
  void NoteKeyMessage(bool key_down_in_system) { keys_.NoteKeyMessage(key_down_in_system); }

 private:
  void Report(GuardFinding finding);
  void ScanLoop(const std::stop_token& stop);

  bool started_ = false;
  KeyProvenance keys_;
  ClockAgreement clocks_;
  GuardSpokesman spokesman_;

  // Ce que le thread de balayage a trouvé, en attente du thread principal.
  std::mutex scanned_mutex_;
  std::vector<GuardFinding> scanned_;
  std::condition_variable_any scan_wake_;
  // ⚠ DÉCLARÉ EN DERNIER, donc détruit en premier : le thread s'arrête avant
  // que disparaisse ce qu'il touche.
  std::jthread scan_thread_;
};

}  // namespace moonclient
