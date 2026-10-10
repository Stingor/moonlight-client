#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/sim/actor.h"
#include "ro/sim/status_effects.h"

namespace ro::sim {

// ── LA LISTE DES STATUTS DE NOTRE COMPAGNON ─────────────────────────────────
//
// Le client ne voit pas tous les statuts d'un compagnon : le serveur n'en
// diffuse qu'une partie. Mais à une demande, il rend la liste COMPLÈTE de ceux
// de NOTRE homoncule ou mercenaire, à toute distance, et il distingue une liste
// vide (rien n'est porté) d'une réponse inconnue. Une liste fraîche décide donc
// aussi « ne porte pas ».
//
// Cette classe dit quand redemander, et garde la dernière liste qui fait foi.

// AU PLUS UNE DEMANDE PAR CET INTERVALLE, hors trace neuve d'un skill.
inline constexpr std::uint32_t kCompanionStatusAskMs = 2000;

class CompanionStatusWatch {
 public:
  // FAUT-IL DEMANDER À CET INSTANT ? Seulement si une règle lit un statut du
  // compagnon (`wanted`) et que le compagnon est en vue (`self`) : à
  // l'intervalle, ou aussitôt après la trace d'un skill qu'il a lancé — un buff
  // sur lui-même change sa liste. Rend vrai, et retient la demande.
  [[nodiscard]] bool ShouldAsk(bool wanted, const Actor* self, std::uint32_t now_ms);

  // UNE RÉPONSE : `valid` quand le serveur la donne pour sûre, liste vide
  // comprise. Une réponse inconnue ne change rien.
  void Receive(bool valid, std::span<const StatusEffectId> statuses, std::uint32_t now_ms);

  // UN CHANGEMENT DE CARTE : la liste se périme.
  void Forget() noexcept;

  [[nodiscard]] const std::vector<StatusEffectId>& statuses() const noexcept { return statuses_; }
  // L'instant de la dernière liste qui fait foi, vide sans elle.
  [[nodiscard]] const std::optional<std::uint32_t>& received_ms() const noexcept {
    return received_ms_;
  }

 private:
  std::vector<StatusEffectId> statuses_;
  std::optional<std::uint32_t> received_ms_;
  std::optional<std::uint32_t> asked_ms_;
  // L'instant de la dernière trace de skill du compagnon déjà suivie d'une
  // demande.
  std::optional<std::uint32_t> trace_asked_ms_;
};

}  // namespace ro::sim
