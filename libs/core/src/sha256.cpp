#include "ro/core/sha256.h"

#include <algorithm>
#include <cstring>

namespace ro::core {
namespace {

// Racines cubiques des 64 premiers nombres premiers, partie fractionnaire.
constexpr std::array<std::uint32_t, 64> kRoundConstants = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
    0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
    0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
    0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
    0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
    0xc67178f2U};

constexpr std::uint32_t RotateRight(std::uint32_t value, unsigned bits) noexcept {
  return (value >> bits) | (value << (32U - bits));
}

}  // namespace

void Sha256::CompressBlock(std::span<const std::uint8_t, 64> block) noexcept {
  std::array<std::uint32_t, 64> schedule{};
  for (std::size_t index = 0; index < 16; ++index) {
    const std::size_t base = index * 4;
    schedule[index] = (static_cast<std::uint32_t>(block[base]) << 24) |
                      (static_cast<std::uint32_t>(block[base + 1]) << 16) |
                      (static_cast<std::uint32_t>(block[base + 2]) << 8) |
                      static_cast<std::uint32_t>(block[base + 3]);
  }
  for (std::size_t index = 16; index < 64; ++index) {
    const std::uint32_t previous = schedule[index - 15];
    const std::uint32_t recent = schedule[index - 2];
    const std::uint32_t sigma0 =
        RotateRight(previous, 7) ^ RotateRight(previous, 18) ^ (previous >> 3);
    const std::uint32_t sigma1 = RotateRight(recent, 17) ^ RotateRight(recent, 19) ^ (recent >> 10);
    schedule[index] = schedule[index - 16] + sigma0 + schedule[index - 7] + sigma1;
  }

  std::uint32_t a = state_[0];
  std::uint32_t b = state_[1];
  std::uint32_t c = state_[2];
  std::uint32_t d = state_[3];
  std::uint32_t e = state_[4];
  std::uint32_t f = state_[5];
  std::uint32_t g = state_[6];
  std::uint32_t h = state_[7];

  for (std::size_t index = 0; index < 64; ++index) {
    const std::uint32_t big_sigma1 = RotateRight(e, 6) ^ RotateRight(e, 11) ^ RotateRight(e, 25);
    const std::uint32_t choice = (e & f) ^ (~e & g);
    const std::uint32_t temp1 = h + big_sigma1 + choice + kRoundConstants[index] + schedule[index];
    const std::uint32_t big_sigma0 = RotateRight(a, 2) ^ RotateRight(a, 13) ^ RotateRight(a, 22);
    const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
    const std::uint32_t temp2 = big_sigma0 + majority;

    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }

  state_[0] += a;
  state_[1] += b;
  state_[2] += c;
  state_[3] += d;
  state_[4] += e;
  state_[5] += f;
  state_[6] += g;
  state_[7] += h;
}

void Sha256::Update(std::span<const std::uint8_t> data) noexcept {
  total_bytes_ += data.size();

  // Complète d'abord le bloc partiel en attente.
  if (buffered_ > 0) {
    const std::size_t needed = std::min(buffer_.size() - buffered_, data.size());
    std::memcpy(buffer_.data() + buffered_, data.data(), needed);
    buffered_ += needed;
    data = data.subspan(needed);
    if (buffered_ < buffer_.size()) {
      return;
    }
    CompressBlock(std::span<const std::uint8_t, 64>(buffer_));
    buffered_ = 0;
  }

  while (data.size() >= 64) {
    CompressBlock(data.first<64>());
    data = data.subspan(64);
  }

  if (!data.empty()) {
    std::memcpy(buffer_.data(), data.data(), data.size());
    buffered_ = data.size();
  }
}

Sha256::Digest Sha256::Finish() noexcept {
  // Capturée AVANT le remplissage : la longueur encodée est celle du message,
  // pas celle du message rembourré. Les Update() qui suivent font monter
  // total_bytes_, et c'est sans conséquence puisqu'il n'est plus relu.
  const std::uint64_t bit_length = total_bytes_ * 8;

  // Remplissage : un octet 0x80, des zéros jusqu'à 56 mod 64, puis la longueur
  // sur 8 octets en GRAND-boutiste — SHA-256 l'est, contrairement aux formats RO.
  constexpr std::array<std::uint8_t, 1> kTerminator = {0x80};
  Update(kTerminator);

  // Après un Update, buffered_ est dans [0, 63] : le bloc plein est toujours
  // consommé sur-le-champ. Les deux branches couvrent donc tout le domaine.
  constexpr std::array<std::uint8_t, 64> kZeros{};
  const std::size_t zeros_needed = (buffered_ <= 56) ? (56 - buffered_) : (120 - buffered_);
  if (zeros_needed > 0) {
    Update(std::span<const std::uint8_t>(kZeros.data(), zeros_needed));
  }

  std::array<std::uint8_t, 8> length_bytes{};
  for (std::size_t index = 0; index < 8; ++index) {
    length_bytes[index] = static_cast<std::uint8_t>((bit_length >> (56 - 8 * index)) & 0xFFU);
  }
  Update(length_bytes);

  Digest digest{};
  for (std::size_t index = 0; index < state_.size(); ++index) {
    digest[index * 4] = static_cast<std::uint8_t>((state_[index] >> 24) & 0xFFU);
    digest[index * 4 + 1] = static_cast<std::uint8_t>((state_[index] >> 16) & 0xFFU);
    digest[index * 4 + 2] = static_cast<std::uint8_t>((state_[index] >> 8) & 0xFFU);
    digest[index * 4 + 3] = static_cast<std::uint8_t>(state_[index] & 0xFFU);
  }
  return digest;
}

Sha256::Digest Sha256::Hash(std::span<const std::uint8_t> data) noexcept {
  Sha256 hasher;
  hasher.Update(data);
  return hasher.Finish();
}

std::string Sha256::ToHex(const Digest& digest) {
  static constexpr char kDigits[] = "0123456789abcdef";
  std::string hex;
  hex.reserve(digest.size() * 2);
  for (const std::uint8_t byte : digest) {
    hex.push_back(kDigits[byte >> 4]);
    hex.push_back(kDigits[byte & 0x0FU]);
  }
  return hex;
}

}  // namespace ro::core
