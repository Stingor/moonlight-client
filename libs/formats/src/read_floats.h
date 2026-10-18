#pragma once

#include <array>
#include <cstddef>

#include "ro/core/byte_reader.h"

// LES TABLEAUX DE FLOTTANTS DES FORMATS DE CARTE ET D'EFFET : des F32 à la
// suite, dans l'ordre du tableau, sans en-tête ni compte.
//
// En-tête PRIVÉ de `libs/formats/src`.
namespace ro::formats {

// Remplit `out` d'autant de flottants qu'il en porte.
template <std::size_t N>
void ReadFloats(core::ByteReader& reader, std::array<float, N>& out) {
  for (float& component : out) {
    component = reader.F32();
  }
}

// Un vecteur x, y, z.
[[nodiscard]] inline std::array<float, 3> ReadVector3(core::ByteReader& reader) {
  std::array<float, 3> value{};
  ReadFloats(reader, value);
  return value;
}

}  // namespace ro::formats
