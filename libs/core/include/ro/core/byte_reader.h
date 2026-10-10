#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace ro::core {

// Lecteur borné à échec RÉMANENT.
//
// Toute lecture hors bornes rend une valeur nulle et lève le drapeau d'échec,
// qui ne redescend jamais. On enchaîne donc les lectures d'un en-tête sans les
// tester une par une, et on vérifie `ok()` une seule fois à la fin.
//
// C'est le seul modèle qui tienne dans un parseur de format binaire : un
// lecteur qui rend un Result par champ oblige à écrire trente tests d'affilée,
// et le trentième finit par manquer. Ici, oublier le test final est visible —
// oublier l'un des trente ne l'était pas.
//
// Aucune lecture ne peut sortir du tampon : ces octets viennent d'archives que
// l'on ne contrôle pas.
//
// Toutes les valeurs multi-octets sont lues en PETIT-BOUTISTE (les formats RO le
// sont), assemblées octet par octet — la couche `formats` doit compiler à
// l'identique sur le runner Linux de la CI, qui nous donne les sanitizers.
class ByteReader {
 public:
  explicit ByteReader(std::span<const std::uint8_t> data) noexcept : data_(data) {}

  [[nodiscard]] bool ok() const noexcept { return ok_; }
  [[nodiscard]] std::size_t offset() const noexcept { return offset_; }
  [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }
  [[nodiscard]] std::size_t remaining() const noexcept { return ok_ ? data_.size() - offset_ : 0; }

  // Un compte lu dans le fichier, ramené à ce qu'il reste d'octets : chaque
  // élément en coûte au moins un. C'est ce qu'on RÉSERVE, jamais le compte brut.
  //
  // 🔴 SANS CETTE BORNE, UN FICHIER DE QUARANTE OCTETS RÉSERVAIT CENT MÉGAS : le
  // plafond d'un compte le garde du déraisonnable, pas de ce que le fichier
  // peut effectivement porter.
  [[nodiscard]] std::size_t AtMostRemaining(std::size_t count) const noexcept {
    return count < remaining() ? count : remaining();
  }

  // Lève le drapeau d'échec sans lire : sert au parseur qui constate une
  // incohérence entre deux champs pourtant lus correctement.
  void Invalidate() noexcept { ok_ = false; }

  bool Seek(std::size_t position) noexcept {
    if (position > data_.size()) {
      ok_ = false;
      return false;
    }
    offset_ = position;
    return ok_;
  }

  void Skip(std::size_t count) noexcept { static_cast<void>(Take(count)); }

  std::uint8_t U8() noexcept {
    const std::span<const std::uint8_t> raw = Take(1);
    return raw.empty() ? std::uint8_t{0} : raw[0];
  }
  std::uint16_t U16() noexcept { return static_cast<std::uint16_t>(Little(2)); }
  std::uint32_t U32() noexcept { return static_cast<std::uint32_t>(Little(4)); }
  std::uint64_t U64() noexcept { return Little(8); }

  std::int8_t I8() noexcept { return static_cast<std::int8_t>(U8()); }
  std::int16_t I16() noexcept { return static_cast<std::int16_t>(U16()); }
  std::int32_t I32() noexcept { return static_cast<std::int32_t>(U32()); }
  std::int64_t I64() noexcept { return static_cast<std::int64_t>(U64()); }

  float F32() noexcept { return std::bit_cast<float>(U32()); }

  // Vue sur les octets consommés. Vide (et échec) si le tampon est trop court.
  [[nodiscard]] std::span<const std::uint8_t> Bytes(std::size_t count) noexcept {
    return Take(count);
  }

  // Chaîne terminée par NUL, le NUL étant consommé. Son absence est un échec :
  // une table d'entrées tronquée en plein nom ne doit pas rendre un nom valide.
  [[nodiscard]] std::string_view CString() noexcept {
    if (!ok_) {
      return {};
    }
    const std::size_t begin = offset_;
    while (offset_ < data_.size() && data_[offset_] != 0) {
      ++offset_;
    }
    if (offset_ >= data_.size()) {
      ok_ = false;
      return {};
    }
    const std::size_t length = offset_ - begin;
    ++offset_;
    return {reinterpret_cast<const char*>(data_.data()) + begin, length};
  }

  // Champ de largeur fixe, tronqué au premier NUL. C'est la forme des noms de
  // fichiers dans les formats RO : `count` octets sont TOUJOURS consommés,
  // que la chaîne les remplisse ou non.
  // TOUT CE QUI RESTE, LU COMME UNE CHAÎNE C.
  //
  // ⚠ UNE QUEUE DE TEXTE PORTE SON ZÉRO TERMINAL, et le serveur le COMPTE dans
  // la longueur qu'il annonce (`len = strlen(mes) + 1`). Le garder, c'est
  // transporter un caractère nul jusqu'au dessin — où il sort en glyphe
  // « caractère manquant », invisible tant que la police n'en a pas, et bien
  // visible dès qu'on en charge une. C'est la même règle que `FixedString`,
  // dont l'absence ici était une asymétrie, pas une décision.
  [[nodiscard]] std::string_view TailString() noexcept {
    const std::span<const std::uint8_t> raw = Take(remaining());
    const std::string_view field(reinterpret_cast<const char*>(raw.data()), raw.size());
    const std::size_t terminator = field.find('\0');
    return terminator == std::string_view::npos ? field : field.substr(0, terminator);
  }

  [[nodiscard]] std::string_view FixedString(std::size_t count) noexcept {
    const std::span<const std::uint8_t> raw = Take(count);
    if (raw.size() != count) {
      return {};
    }
    const std::string_view field(reinterpret_cast<const char*>(raw.data()), count);
    const std::size_t terminator = field.find('\0');
    return terminator == std::string_view::npos ? field : field.substr(0, terminator);
  }

 private:
  [[nodiscard]] std::span<const std::uint8_t> Take(std::size_t count) noexcept {
    if (!ok_ || count > data_.size() - offset_) {
      ok_ = false;
      return {};
    }
    const std::span<const std::uint8_t> slice = data_.subspan(offset_, count);
    offset_ += count;
    return slice;
  }

  [[nodiscard]] std::uint64_t Little(std::size_t width) noexcept {
    const std::span<const std::uint8_t> raw = Take(width);
    if (raw.size() != width) {
      return 0;
    }
    std::uint64_t value = 0;
    for (std::size_t index = width; index-- > 0;) {
      value = (value << 8) | static_cast<std::uint64_t>(raw[index]);
    }
    return value;
  }

  std::span<const std::uint8_t> data_;
  std::size_t offset_ = 0;
  bool ok_ = true;
};

}  // namespace ro::core
