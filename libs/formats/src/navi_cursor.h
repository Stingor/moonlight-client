#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "ro/core/error.h"
#include "ro/formats/navi_tables.h"

// Le lecteur commun aux tables de navigation, partagé par `navi_tables.cpp` et
// `navi_npc_distances.cpp`.
//
// Interne à ro_formats : rien ici n'est une promesse d'API.
namespace ro::formats::navi_internal {

// ── Le curseur ───────────────────────────────────────────────────────────────
//
// Un lecteur de la grammaire commune aux six tables : des accolades, des
// virgules, des chaînes entre guillemets doubles, des entiers, et des
// commentaires `--` jusqu'à la fin de la ligne.
class Cursor {
 public:
  explicit Cursor(std::span<const std::uint8_t> bytes)
      : text_(reinterpret_cast<const char*>(bytes.data()), bytes.size()) {}

  void SkipBlanks() {
    while (at_ < text_.size()) {
      const char c = text_[at_];
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        ++at_;
      } else if (c == '-' && at_ + 1 < text_.size() && text_[at_ + 1] == '-') {
        while (at_ < text_.size() && text_[at_] != '\n') ++at_;
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

  // Une virgule entre deux éléments est facultative avant une accolade fermante :
  // Lua l'accepte en fin de liste, et le serveur l'écrit.
  void TakeOptionalComma() { (void)Take(','); }

  [[nodiscard]] std::optional<std::string_view> Name() {
    SkipBlanks();
    const std::size_t start = at_;
    while (at_ < text_.size()) {
      const char c = text_[at_];
      const bool word =
          (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
      if (!word) break;
      ++at_;
    }
    if (at_ == start) return std::nullopt;
    return text_.substr(start, at_ - start);
  }

  // Une chaîne entre guillemets doubles. Les échappements usuels sont défaits ;
  // le serveur n'en écrit pas, mais un nom de PNJ peut porter un guillemet.
  [[nodiscard]] std::optional<std::string> String() {
    if (!Take('"')) return std::nullopt;
    std::string out;
    while (at_ < text_.size()) {
      const char c = text_[at_++];
      if (c == '"') return out;
      if (c == '\\' && at_ < text_.size()) {
        const char escaped = text_[at_++];
        out.push_back(escaped == 'n' ? '\n' : escaped == 't' ? '\t' : escaped);
        continue;
      }
      if (c == '\n') return std::nullopt;
      out.push_back(c);
    }
    return std::nullopt;
  }

  [[nodiscard]] std::optional<std::int64_t> Integer() {
    SkipBlanks();
    bool negative = false;
    if (at_ < text_.size() && text_[at_] == '-') {
      negative = true;
      ++at_;
    }
    const std::size_t start = at_;
    std::int64_t value = 0;
    constexpr std::int64_t kCeiling = std::numeric_limits<std::int32_t>::max();
    while (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9') {
      value = value * 10 + (text_[at_] - '0');
      if (value > kCeiling) return std::nullopt;
      ++at_;
    }
    if (at_ == start) return std::nullopt;
    return negative ? -value : value;
  }

  [[nodiscard]] std::size_t offset() const noexcept { return at_; }

 private:
  std::string_view text_;
  std::size_t at_ = 0;
};

[[nodiscard]] inline std::unexpected<core::Error> Broken(std::string_view table,
                                                         const Cursor& cursor,
                                                         std::string_view what) {
  return core::Fail(core::ErrorCode::kCorruptData, std::string(table) + " : " + std::string(what) +
                                                       " à l'octet " +
                                                       std::to_string(cursor.offset()));
}

// `Nom = {` — l'en-tête de chaque fichier.
[[nodiscard]] inline bool OpenTable(Cursor& cursor, std::string_view expected_name) {
  const auto name = cursor.Name();
  return name && *name == expected_name && cursor.Take('=') && cursor.Take('{');
}

// Une valeur bornée à un type étroit. Hors bornes, la ligne est illisible.
template <typename T>
[[nodiscard]] std::optional<T> Narrow(std::optional<std::int64_t> value) {
  if (!value || *value < std::numeric_limits<T>::min() || *value > std::numeric_limits<T>::max())
    return std::nullopt;
  return static_cast<T>(*value);
}

// Lit `, <entier>` : la virgule qui SÉPARE un champ du précédent.
[[nodiscard]] inline std::optional<std::int64_t> NextInteger(Cursor& cursor) {
  if (!cursor.Take(',')) return std::nullopt;
  return cursor.Integer();
}

[[nodiscard]] inline std::optional<std::string> NextString(Cursor& cursor) {
  if (!cursor.Take(',')) return std::nullopt;
  return cursor.String();
}

// Parcourt une liste `{ {…}, {…}, … }` et confie chaque entrée à `read_entry`,
// qui lit ce qu'il y a ENTRE ses accolades. Rend faux au premier accroc.
template <typename ReadEntry>
[[nodiscard]] inline bool ForEachEntry(Cursor& cursor, ReadEntry&& read_entry) {
  while (!cursor.Peek('}')) {
    if (!cursor.Take('{')) return false;
    if (!read_entry()) return false;
    if (!cursor.Take('}')) return false;
    cursor.TakeOptionalComma();
  }
  return cursor.Take('}');
}

// Un fichier lu entièrement : la table fermée, et rien derrière.
[[nodiscard]] inline bool ClosedAtEnd(Cursor& cursor) { return cursor.AtEnd(); }

// La ligne qui ouvre une table de distances aux longueurs exactes. C'est un
// commentaire Lua : le curseur la saute comme les autres.
inline constexpr std::string_view kFormatLine = "-- navi_format:";

// Comment une table de distances écrit ses longueurs, d'après sa première ligne.
[[nodiscard]] inline NaviSteps StepsOf(std::span<const std::uint8_t> bytes) {
  const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  if (!text.starts_with(kFormatLine)) return NaviSteps::kModulo256;
  Cursor cursor(bytes.subspan(kFormatLine.size()));
  const std::optional<std::int64_t> format = cursor.Integer();
  return format && *format >= kNaviExactStepsFormat ? NaviSteps::kExact : NaviSteps::kModulo256;
}

}  // namespace ro::formats::navi_internal
