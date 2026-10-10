// Les mots des chuchotements. Voir l'en-tête.

#include "ro/vfs/whisper_words.h"

#include <array>
#include <string>
#include <tuple>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/vfs/item_words.h"

namespace ro::vfs {

std::string_view WhisperText(const ItemWords& words, WhisperWord word) {
  const auto rank = static_cast<std::size_t>(word);
  if (rank >= kWhisperWordCount) {
    return {};
  }
  return words.whisper.texts[rank];
}

void FillWhisperWords(WhisperWords& words) {
  // DANS L'ORDRE DE `WhisperWord` : le rang de l'un est l'indice de l'autre.
  auto texts = std::to_array<std::string>({
      core::Tr("Ce nom de personnage n'existe pas ou le joueur est hors ligne."),
      core::Tr("ne souhaite pas recevoir vos messages."),
      core::Tr("n'a envie de parler à personne."),
  });
  // ⚠ UN TEXTE OUBLIÉ NE SE VERRAIT PAS : tous les mots suivants seraient
  // décalés d'un rang. Le compte se vérifie à la compilation.
  static_assert(std::tuple_size_v<decltype(texts)> == kWhisperWordCount);
  words.texts = std::move(texts);
}

}  // namespace ro::vfs
