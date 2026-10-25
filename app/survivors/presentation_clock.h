#pragma once

#include <cstdint>

namespace moonclient {

// L'HORLOGE DE PRÉSENTATION DU MODE SURVIVORS : sprites, effets, nombres et
// fondus la suivent, jamais `now_ms`. Tant que la partie court, elle vaut
// `pas × 20 + blend_per_mille × 20 / 1000` ; elle gèle donc avec la simulation
// (pause, choix, coffre) et ne saute pas à la reprise, et une même graine
// jouée avec les mêmes entrées redonne les mêmes images. Une fois la partie
// finie, elle court encore du temps d'écran (`RunOn`), pour que la chute du
// héros ou du boss et le fondu de la horde se jouent.

// La part de l'horloge que la simulation dicte.
[[nodiscard]] std::uint64_t PresentationMsOf(std::uint32_t step,
                                             std::uint32_t blend_per_mille) noexcept;

class PresentationClock {
 public:
  // Suit la partie : l'horloge ne recule jamais, même quand l'ouverture d'un
  // choix remet le reste du pas à zéro.
  void Follow(std::uint32_t step, std::uint32_t blend_per_mille) noexcept;
  // L'épilogue d'une partie finie : le temps d'écran s'ajoute tel quel.
  void RunOn(std::uint32_t elapsed_ms) noexcept;
  // Une partie neuve repart de zéro.
  void Reset() noexcept;
  [[nodiscard]] std::uint64_t presentation_ms() const noexcept {
    return followed_ms_ + epilogue_ms_;
  }

 private:
  std::uint64_t followed_ms_ = 0;
  std::uint64_t epilogue_ms_ = 0;
};

}  // namespace moonclient
