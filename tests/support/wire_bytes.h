#pragma once

// LES ENTIERS DU FIL, ÉCRITS ET RELUS À LA MAIN PAR LES ESSAIS : petit-boutistes,
// l'octet de poids faible d'abord, comme le serveur et le client les échangent.
//
// ⚠ ÉCRIRE AJOUTE EN FIN DE TRAME. Un essai qui réécrit un champ EN PLACE, à un
// décalage donné, garde son aide à trois arguments : c'est une autre fonction.
//
// LIRE INDEXE DIRECTEMENT, sans garde de bornes : un décalage hors de la trame
// est une faute de l'essai, et une lecture qui rendrait zéro la cacherait.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ro::test_support {

inline void PutU16(std::vector<std::uint8_t>& into, std::uint16_t value) {
  constexpr unsigned kBitsPerByte = 8;
  constexpr unsigned kByteMask = 0xFFU;
  into.push_back(static_cast<std::uint8_t>(value & kByteMask));
  into.push_back(static_cast<std::uint8_t>(value >> kBitsPerByte));
}

inline void PutU32(std::vector<std::uint8_t>& into, std::uint32_t value) {
  constexpr unsigned kHalfBits = 16;
  constexpr std::uint32_t kHalfMask = 0xFFFFU;
  PutU16(into, static_cast<std::uint16_t>(value & kHalfMask));
  PutU16(into, static_cast<std::uint16_t>(value >> kHalfBits));
}

inline void PutU64(std::vector<std::uint8_t>& into, std::uint64_t value) {
  constexpr unsigned kHalfBits = 32;
  constexpr std::uint64_t kHalfMask = 0xFFFFFFFFU;
  PutU32(into, static_cast<std::uint32_t>(value & kHalfMask));
  PutU32(into, static_cast<std::uint32_t>(value >> kHalfBits));
}

[[nodiscard]] inline std::uint16_t ReadU16(const std::vector<std::uint8_t>& bytes, std::size_t at) {
  constexpr unsigned kBitsPerByte = 8;
  return static_cast<std::uint16_t>(bytes[at] | (bytes[at + 1] << kBitsPerByte));
}

[[nodiscard]] inline std::uint32_t ReadU32(const std::vector<std::uint8_t>& bytes, std::size_t at) {
  constexpr unsigned kHalfBits = 16;
  return static_cast<std::uint32_t>(ReadU16(bytes, at)) |
         (static_cast<std::uint32_t>(ReadU16(bytes, at + sizeof(std::uint16_t))) << kHalfBits);
}

}  // namespace ro::test_support
