#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace ro::core {

// SHA-256 (FIPS 180-4), sans dépendance.
//
// Le serveur Moonlight vérifie déjà une empreinte SHA-256 du client au contrôle
// d'intégrité : la même primitive servira au patcher et à la validation des
// ressources. La sortir d'OpenSSL pour ça seul ne se justifie pas — et un
// algorithme à vecteurs de test publiés est une cible de test unitaire idéale.
class Sha256 {
 public:
  static constexpr std::size_t kDigestSize = 32;
  using Digest = std::array<std::uint8_t, kDigestSize>;

  Sha256() noexcept = default;

  void Update(std::span<const std::uint8_t> data) noexcept;

  // Clôt l'état et rend l'empreinte. L'objet n'est plus réutilisable.
  [[nodiscard]] Digest Finish() noexcept;

  [[nodiscard]] static Digest Hash(std::span<const std::uint8_t> data) noexcept;
  [[nodiscard]] static std::string ToHex(const Digest& digest);

 private:
  void CompressBlock(std::span<const std::uint8_t, 64> block) noexcept;

  std::array<std::uint32_t, 8> state_{0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                                      0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
  std::array<std::uint8_t, 64> buffer_{};
  std::size_t buffered_ = 0;
  std::uint64_t total_bytes_ = 0;
};

}  // namespace ro::core
