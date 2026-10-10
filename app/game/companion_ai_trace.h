#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "ro/sim/companion_ai.h"
#include "ro/sim/world_fwd.h"

namespace moonclient {

// ── CE QUE L'IA D'UN COMPAGNON DÉCIDE, AU JOURNAL ───────────────────────────
//
// Une ligne quand la décision CHANGE — cible, raison, sorte d'ordre —, et une
// quand l'IA se tait trop longtemps alors qu'elle est active : la cible tenue
// et le monstre le plus proche, avec leurs distances. C'est ce qui sépare une
// cible refusée d'un combat qui n'émet plus rien.
//
// ⚠ JAMAIS UNE LIGNE PAR TIC : un combat émet une attaque à chaque amotion, et
// l'ordre répété n'est pas un changement.

// Combien de silence, compagnon actif, avant de le dire.
inline constexpr std::uint32_t kCompanionAiSilenceMs = 3000;

class CompanionAiTrace {
 public:
  // Après chaque tic : `order` est ce que le cerveau a rendu, ou rien.
  void Note(std::string_view companion, const ro::sim::World& world,
            const ro::sim::CompanionFacts& facts, const ro::sim::CompanionBrain& brain,
            const std::optional<ro::sim::CompanionOrder>& order, std::uint32_t now_ms);
  // Le compagnon n'est plus actif : la trace repart de zéro à son retour.
  void Forget() noexcept { *this = CompanionAiTrace(); }

 private:
  struct Decision {
    std::uint32_t target_id = 0;
    ro::sim::CompanionAiReason reason = ro::sim::CompanionAiReason::kNone;
    ro::sim::CompanionOrder::Kind kind = ro::sim::CompanionOrder::Kind::kNone;

    friend bool operator==(const Decision&, const Decision&) = default;
  };

  std::optional<Decision> said_;
  std::optional<std::uint32_t> last_order_ms_;
  bool silence_said_ = false;
};

}  // namespace moonclient
