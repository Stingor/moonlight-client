#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

// L'ÂGE DU MARQUEUR DE CIBLE DE CHAQUE PORTEUR : voir
// `ro/render/companion_target_mark.h`.
//
// L'anneau naît sur un acteur à la première image où la marque l'atteint, et
// son animation part de là. Il meurt dès qu'elle le quitte : une marque qui
// revient sur le même acteur fait naître un anneau neuf. Deux compagnons qui
// marquent le même acteur ne lui en donnent qu'un, né avec la première marque.
namespace moonclient {

class CompanionTargetMarkClock {
 public:
  // Les acteurs qui portent le marqueur à cette image, `now_ms`.
  void Update(std::span<const std::uint32_t> bearer_ids, std::uint32_t now_ms);

  // Depuis quand cet acteur porte le marqueur, ou rien quand il ne le porte pas.
  [[nodiscard]] std::optional<std::uint32_t> MarkedSince(std::uint32_t actor_id) const noexcept;

 private:
  struct Bearer {
    std::uint32_t actor_id = 0;
    std::uint32_t since_ms = 0;
  };
  std::vector<Bearer> bearers_;
};

}  // namespace moonclient
