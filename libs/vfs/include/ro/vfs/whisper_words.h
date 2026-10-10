#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace ro::vfs {

struct ItemWords;

// LES MOTS DES CHUCHOTEMENTS : les trois refus d'un envoi, que le client écrit
// après « (Nom) ». Des textes de l'interface, posés par `FillWhisperWords`
// (ADR 0090).
enum class WhisperWord : std::uint8_t {
  kNotConnected,
  kIgnored,
  kIgnoresAll,
  kCount,
};

inline constexpr std::size_t kWhisperWordCount = static_cast<std::size_t>(WhisperWord::kCount);

// Les textes posés, rangés par `WhisperWord`.
struct WhisperWords {
  std::array<std::string, kWhisperWordCount> texts;
};

// Le texte d'un mot ; vide tant que les mots ne sont pas posés.
[[nodiscard]] std::string_view WhisperText(const ItemWords& words, WhisperWord word);

// Pose ces mots dans la langue courante.
void FillWhisperWords(WhisperWords& words);

}  // namespace ro::vfs
