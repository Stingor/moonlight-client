#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace ro::core {

// PCG-XSH-RR 64/32 : multiplicateur de la publication de référence de PCG.
inline constexpr std::uint64_t kPcg32Multiplier = 6364136223846793005ULL;

// UN GÉNÉRATEUR QUI REND LA MÊME SUITE PARTOUT. Ses tirages bornés sont écrits
// ici : les distributions de la bibliothèque standard ne donnent pas la même
// suite sous MSVC et sous libstdc++. Chaque tirage consomme un nombre de sorties
// fixé par sa documentation : c'est ce qui rend un flux rejouable.
class Pcg32 {
 public:
  Pcg32() noexcept : Pcg32(0, 0) {}
  // `stream` choisit une suite indépendante pour une même graine.
  Pcg32(std::uint64_t seed, std::uint64_t stream) noexcept;

  [[nodiscard]] std::uint32_t Next() noexcept;
  // Dans [0, bound), sans biais, par rejet sous le seuil (2^32 - bound) % bound.
  // 0 sans tirage si bound vaut 0 ; au moins un tirage sinon.
  [[nodiscard]] std::uint32_t Below(std::uint32_t bound) noexcept;
  // Dans [least, most], bornes comprises ; least sans tirage si most < least.
  // L'intervalle entier de int32 : un seul Next().
  [[nodiscard]] std::int32_t Between(std::int32_t least, std::int32_t most) noexcept;
  // Vrai avec la probabilité numerator / denominator. Faux sans tirage si
  // denominator vaut 0 ; sinon exactement un Below(denominator), même quand
  // numerator >= denominator.
  [[nodiscard]] bool Chance(std::uint32_t numerator, std::uint32_t denominator) noexcept;
  // L'index tiré selon les poids, par un Below(somme). weights.size() sans tirage
  // si la somme est nulle ou dépasse 0xFFFFFFFF.
  [[nodiscard]] std::size_t PickWeighted(std::span<const std::uint32_t> weights) noexcept;

  [[nodiscard]] bool operator==(const Pcg32&) const = default;

 private:
  std::uint64_t state_ = 0;
  std::uint64_t increment_ = 0;
};

// Fisher-Yates, du dernier au premier : un Below(last) par place.
template <typename T>
void ShuffleWith(Pcg32& random, std::span<T> items) {
  for (std::size_t last = items.size(); last > 1; --last) {
    std::swap(items[last - 1], items[random.Below(static_cast<std::uint32_t>(last))]);
  }
}

}  // namespace ro::core
