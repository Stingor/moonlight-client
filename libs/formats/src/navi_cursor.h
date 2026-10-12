#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "ro/core/error.h"

// Le lecteur du texte de `navigation.yml`, pour `navi_tables.cpp`.
//
// Interne à ro_formats : rien ici n'est une promesse d'API.
namespace ro::formats::navi_internal {

inline constexpr std::string_view kTableFile = "navigation.yml";

// Un lecteur du YAML que le script écrit : des accolades et des crochets, des
// virgules, des deux-points, des chaînes entre guillemets doubles (seuls `\"`
// et `\\` s'y échappent ; le reste est de l'UTF-8 tel quel), des entiers, des
// mots nus, et des commentaires `#` jusqu'à la fin de la ligne.
class Cursor {
 public:
  explicit Cursor(std::span<const std::uint8_t> bytes)
      : text_(reinterpret_cast<const char*>(bytes.data()), bytes.size()) {}

  void SkipBlanks() {
    while (at_ < text_.size()) {
      const char c = text_[at_];
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        ++at_;
      } else if (c == '#') {
        const std::size_t end = text_.find('\n', at_);
        at_ = end == std::string_view::npos ? text_.size() : end;
      } else {
        return;
      }
    }
  }

  [[nodiscard]] bool AtEnd() {
    SkipBlanks();
    return at_ >= text_.size();
  }

  [[nodiscard]] bool Peek(char wanted) {
    SkipBlanks();
    return at_ < text_.size() && text_[at_] == wanted;
  }

  [[nodiscard]] bool Take(char wanted) {
    if (!Peek(wanted)) return false;
    ++at_;
    return true;
  }

  // Un mot nu : des minuscules, des chiffres et des soulignés, commençant par
  // une lettre.
  [[nodiscard]] std::optional<std::string_view> Word() {
    SkipBlanks();
    const std::size_t start = at_;
    if (at_ >= text_.size() || text_[at_] < 'a' || text_[at_] > 'z') return std::nullopt;
    while (at_ < text_.size()) {
      const char c = text_[at_];
      if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) break;
      ++at_;
    }
    return text_.substr(start, at_ - start);
  }

  // Une chaîne entre guillemets doubles, sur une seule ligne.
  [[nodiscard]] std::optional<std::string> Quoted() {
    if (!Take('"')) return std::nullopt;
    std::string out;
    while (at_ < text_.size()) {
      const char c = text_[at_++];
      if (c == '"') return out;
      if (c == '\n') return std::nullopt;
      if (c == '\\') {
        if (at_ >= text_.size() || (text_[at_] != '"' && text_[at_] != '\\')) return std::nullopt;
        out.push_back(text_[at_++]);
        continue;
      }
      out.push_back(c);
    }
    return std::nullopt;
  }

  // Un entier, borné à ce que tient un entier signé de 32 bits.
  [[nodiscard]] std::optional<std::int64_t> Integer() {
    SkipBlanks();
    const bool negative = at_ < text_.size() && text_[at_] == '-';
    if (negative) ++at_;
    const std::size_t start = at_;
    std::int64_t value = 0;
    constexpr std::int64_t kDecimalBase = 10;
    constexpr std::int64_t kCeiling = std::numeric_limits<std::int32_t>::max();
    while (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9') {
      value = value * kDecimalBase + (text_[at_] - '0');
      if (value > kCeiling) return std::nullopt;
      ++at_;
    }
    if (at_ == start) return std::nullopt;
    return negative ? -value : value;
  }

  // Le refus, avec la ligne où la lecture s'est arrêtée.
  [[nodiscard]] core::Error Refused(std::string_view what) const {
    const auto line =
        std::count(text_.begin(),
                   text_.begin() + static_cast<std::ptrdiff_t>(std::min(at_, text_.size())), '\n');
    return core::Error{core::ErrorCode::kCorruptData, std::string(kTableFile) + ", ligne " +
                                                          std::to_string(line + 1) + " : " +
                                                          std::string(what)};
  }

 private:
  std::string_view text_;
  std::size_t at_ = 0;
};

// Une valeur bornée à un type étroit. Hors bornes, rien.
template <typename T>
[[nodiscard]] std::optional<T> Narrow(std::optional<std::int64_t> value) {
  if (!value || *value < std::numeric_limits<T>::min() || *value > std::numeric_limits<T>::max())
    return std::nullopt;
  return static_cast<T>(*value);
}

// `<clé>:` en tête d'un champ.
[[nodiscard]] inline std::optional<std::string_view> FieldKey(Cursor& cursor) {
  const auto key = cursor.Word();
  if (!key || !cursor.Take(':')) return std::nullopt;
  return key;
}

// Parcourt `<open> élément, élément, … <close>` — virgule finale admise — et
// confie chaque élément à `read_item`. Rend faux au premier accroc.
template <typename ReadItem>
[[nodiscard]] bool ForEachItem(Cursor& cursor, char open, char close, ReadItem&& read_item) {
  if (!cursor.Take(open)) return false;
  while (!cursor.Take(close)) {
    if (!read_item()) return false;
    if (!cursor.Take(',') && !cursor.Peek(close)) return false;
  }
  return true;
}

// Parcourt `{ <clé>: <valeur>, … }` et confie chaque clé à `read_value`, qui
// lit sa valeur.
template <typename ReadValue>
[[nodiscard]] bool ForEachField(Cursor& cursor, ReadValue&& read_value) {
  return ForEachItem(cursor, '{', '}', [&] {
    const auto key = FieldKey(cursor);
    return key.has_value() && read_value(*key);
  });
}

}  // namespace ro::formats::navi_internal
