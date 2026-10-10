#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ro::formats {

// LE CODAGE ARITHMÉTIQUE DES FICHIERS GRANNY 2, commun aux sections
// compressées et aux textures.
//
// Deux pièces : un décodeur d'intervalle sur 31 bits, et un modèle adaptatif
// de fréquences qui apprend ses symboles au fil du flux. Elles sont publiques
// parce que l'ENCODEUR des essais doit tenir exactement la même comptabilité
// que le décodeur : un seul modèle pour les deux sens, c'est la garantie qu'ils
// ne divergent pas.

// Un flux de bits lu du bit de poids FAIBLE au bit de poids fort de chaque
// octet, les octets dans l'ordre. Au-delà de la fin, il rend des zéros.
class Gr2BitReader {
 public:
  Gr2BitReader() = default;
  Gr2BitReader(std::span<const std::uint8_t> data, std::size_t start_byte) noexcept
      : data_(data), bit_(start_byte * kBitsPerByte) {}

  std::uint32_t Bit() noexcept;
  // `count` bits, le premier lu étant le poids faible du résultat. 32 au plus.
  std::uint32_t Bits(std::uint32_t count) noexcept;
  // Vrai tant qu'aucune lecture n'a débordé la fin.
  [[nodiscard]] bool within() const noexcept { return bit_ <= data_.size() * kBitsPerByte; }

 private:
  static constexpr std::size_t kBitsPerByte = 8;
  std::span<const std::uint8_t> data_;
  std::size_t bit_ = 0;
};

// L'intervalle courant d'un codage : le cumul de ce qui précède le symbole, sa
// fréquence, et le total, dans le même modèle.
struct Gr2Interval {
  std::uint32_t cumulative = 0;
  std::uint32_t frequency = 0;
  std::uint32_t total = 0;
};

// Le décodeur d'intervalle. Ses bornes et son code tiennent sur 31 bits ; le
// code avance d'un bit à chaque renormalisation, le premier lu entrant au
// poids fort.
class Gr2RangeDecoder {
 public:
  Gr2RangeDecoder(std::span<const std::uint8_t> data, std::size_t start_byte) noexcept;

  // La position du code dans un total de `total` parts : le symbole est celui
  // dont l'intervalle la contient.
  [[nodiscard]] std::uint32_t Target(std::uint32_t total) const noexcept;
  // Réduit l'intervalle à celui du symbole décodé.
  void Narrow(const Gr2Interval& interval) noexcept;
  // Une valeur uniforme dans [0, count).
  std::uint32_t Uniform(std::uint32_t count) noexcept;

  [[nodiscard]] bool within() const noexcept { return bits_.within(); }

 private:
  Gr2BitReader bits_;
  std::uint32_t low_ = 0;
  std::uint32_t high_ = 0;
  std::uint32_t code_ = 0;
};

// Le modèle adaptatif. Chaque case porte un symbole appris et sa fréquence ;
// la case 0 est l'ÉCHAPPEMENT, qui annonce un symbole encore inconnu. Les
// cases se regroupent en seize seaux dont on tient les cumuls, pour trouver
// vite la case d'un rang.
//
// Quand le total atteint son plafond, toutes les fréquences se divisent par
// deux ; une case retombée à zéro s'efface, et la plus fréquente remonte en
// tête de son seau.
class Gr2AdaptiveModel {
 public:
  // Un modèle qui apprendra au plus `capacity` symboles.
  explicit Gr2AdaptiveModel(std::uint32_t capacity);

  struct Decoded {
    // Vrai quand la case est l'échappement : le symbole est neuf, et l'appelant
    // doit lire sa valeur par ailleurs puis la confier à `Learn`.
    bool escaped = false;
    std::uint16_t value = 0;
  };

  // Décode un symbole et met le modèle à jour.
  Decoded Decode(Gr2RangeDecoder& decoder);
  // La valeur du symbole neuf que le dernier `Decode` a annoncé.
  void Learn(std::uint16_t value) noexcept;

  // LE SENS DE L'ENCODEUR. `Encode` rend l'intervalle à coder pour `value`, et
  // met le modèle à jour comme `Decode` l'aurait fait : l'échappement quand la
  // valeur est neuve — l'appelant code alors la valeur par ailleurs.
  struct Encoded {
    Gr2Interval interval;
    bool escaped = false;
  };
  Encoded Encode(std::uint16_t value);

 private:
  static constexpr std::size_t kBuckets = 16;

  [[nodiscard]] std::size_t BucketOf(std::size_t slot) const noexcept;
  void Add(std::size_t slot, std::int32_t delta) noexcept;
  void ChooseBuckets(std::uint32_t symbol_count) noexcept;
  void Rescale();
  // Compte l'usage de la case `slot`, dont `interval` a été tiré avant.
  void Commit(std::size_t slot) noexcept;

  std::uint32_t capacity_ = 0;
  std::array<std::uint16_t, kBuckets> cumulative_{};
  std::vector<std::uint16_t> frequency_;
  std::vector<std::uint16_t> value_;
  std::uint32_t used_ = 0;
  std::uint32_t bucket_shift_ = 0;
  std::uint32_t bucket_limit_ = 0;
};

}  // namespace ro::formats
