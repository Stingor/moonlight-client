#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "ro/sim/stat_presets.h"

namespace ro::sim {

// L'APPLICATION D'UN PRÉRÉGLAGE, une montée après l'autre.
//
// Chaque montée part comme un clic sur la flèche — un cran, `CZ_STATUS_CHANGE`
// à quantité un — et la suivante attend la réponse du serveur. Le serveur
// accepterait une quantité plus grande, mais il la BORNE en silence à ce que
// les points paient : un lot ne dirait pas où il s'est arrêté, un cran le dit.
//
// ⚠ LE PLAN N'EST QU'UNE PRÉVISION. Avant chaque envoi, la file relit ce que le
// serveur dit — coût du point, points libres, valeurs — et elle S'ARRÊTE au
// premier écart plutôt que de dépenser sur une hypothèse fausse.
//
// ⚠ ELLE N'ÉMET RIEN ELLE-MÊME : elle dit quelle caractéristique monter, et
// l'appelant envoie, puis lui dit si l'envoi est parti.

// Ce que la file lit du monde, à chaque image.
struct StatRaiseSight {
  BaseStats base = {};
  // Le coût du point suivant tel que le serveur l'a annoncé ; nul au plafond,
  // négatif tant qu'il n'est pas dit.
  BaseStats next_cost = {};
  std::int64_t status_point = 0;
  // Une montée attend sa réponse — la nôtre ou celle d'un clic du joueur.
  bool awaited = false;
  // Les refus du serveur comptés depuis la connexion.
  std::uint32_t refusals = 0;
  std::uint32_t now_ms = 0;
};

enum class StatRunStop : std::uint8_t {
  // La file tourne encore, ou n'a jamais tourné.
  kNone,
  // Toutes les montées prévues sont faites.
  kFinished,
  // Le serveur a refusé une montée.
  kRefused,
  // Les points libres ont changé hors de nos montées.
  kPointsChanged,
  // Une caractéristique a changé hors de nos montées.
  kStatsChanged,
  // Le point suivant coûte plus que ce qui reste, ou le serveur le dit au
  // plafond.
  kShort,
  // Aucune réponse dans le délai.
  kNoReply,
  // L'envoi n'est pas parti : connexion fermée, personnage mort.
  kNotSent,
  // Le joueur a arrêté.
  kCancelled,
};

// Le délai au-delà duquel une montée sans réponse arrête la file.
inline constexpr std::uint32_t kStatRaiseReplyTimeoutMs = 5000;

class StatRaiseRun {
 public:
  // Lance l'application de `plan`. Refuse — et rend faux — tant qu'une autre
  // tourne, ou quand le plan ne monte rien.
  bool Start(const StatPlan& plan, const StatRaiseSight& seen);

  // CE QU'IL FAUT FAIRE À CETTE IMAGE : le rang de la caractéristique à monter
  // d'un cran, ou rien. Une file qui s'arrête le fait ici, et `stop()` dit
  // pourquoi.
  [[nodiscard]] std::optional<std::uint8_t> Tick(const StatRaiseSight& seen);

  // L'appelant dit si la montée que `Tick` vient de demander est partie.
  void Sent(bool emitted, std::uint32_t now_ms);

  void Cancel();

  [[nodiscard]] bool running() const noexcept { return running_; }
  [[nodiscard]] StatRunStop stop() const noexcept { return stop_; }
  [[nodiscard]] std::size_t done() const noexcept { return done_; }
  [[nodiscard]] std::size_t total() const noexcept { return queue_.size(); }

 private:
  void Halt(StatRunStop why);

  std::vector<std::uint8_t> queue_;
  std::size_t done_ = 0;
  bool running_ = false;
  StatRunStop stop_ = StatRunStop::kNone;
  // Ce que le monde doit montrer quand rien d'autre que nous ne l'a touché.
  BaseStats expected_base_ = {};
  std::int64_t expected_points_ = 0;
  // La montée en vol, demandée par `Tick` puis confirmée par `Sent`.
  bool asked_ = false;
  bool waiting_ = false;
  std::uint32_t sent_ms_ = 0;
  std::uint32_t refusals_at_send_ = 0;
  BaseStats base_after_ = {};
  std::int64_t points_after_ = 0;
};

}  // namespace ro::sim
