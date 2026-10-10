// Les mots de la fenêtre des succès. Voir l'en-tête.

#include "ro/vfs/achievement_words.h"

#include <array>
#include <string>
#include <tuple>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/vfs/item_words.h"
#include "ro/vfs/trade_words.h"

#include "message_word.h"

namespace ro::vfs {
namespace {

// Un libellé d'onglet : il porte un espace final, comme dans le client.
[[nodiscard]] std::string Tab(const char* text) { return Edged("", text, " "); }

// La seule forme que le gabarit de la bannière peut prendre : une chaîne.
constexpr std::string_view kCompletedShape = "s";

}  // namespace

std::string_view AchievementText(const ItemWords& words, AchievementWord word) {
  const auto rank = static_cast<std::size_t>(word);
  if (rank >= kAchievementWordCount) {
    return {};
  }
  return words.achievement.texts[rank];
}

std::string AchievementCompletedText(const ItemWords& words, std::string_view name) {
  const std::string_view format = AchievementText(words, AchievementWord::kCompleted);
  if (format.empty() || TradeFormatShape(format) != kCompletedShape) {
    return {};
  }
  std::string text;
  text.reserve(format.size() + name.size());
  for (std::size_t at = 0; at < format.size(); ++at) {
    if (format[at] != '%') {
      text.push_back(format[at]);
      continue;
    }
    // La forme vérifiée ne laisse que « %% » et un seul « %s ».
    ++at;
    if (format[at] == '%') {
      text.push_back('%');
    } else {
      text.append(name);
    }
  }
  return text;
}

void FillAchievementWords(AchievementWords& words) {
  // DANS L'ORDRE DE `AchievementWord` : le rang de l'un est l'indice de l'autre.
  auto texts = std::to_array<std::string>({
      core::Tr("Succès"),
      core::Tr("Défis accomplis"),
      core::Tr("Succès récemment obtenus"),
      core::Tr("Terminé"),
      core::Tr("Incomplet"),
      core::Tr("Impossible d'en ajouter davantage."),
      core::Tr("< %s > obtenu."),
      Tab(core::Tr("Résumé")),
      Tab(core::Tr("Général")),
      Tab(core::Tr("Personnage")),
      Tab(core::Tr("Action")),
      Tab(core::Tr("Autre")),
      Tab(core::Tr("Aventure")),
      Tab(core::Tr("Rune Midgarts")),
      Tab(core::Tr("Schwartzvald")),
      Tab(core::Tr("Arunafeltz")),
      Tab(core::Tr("Nouveau Monde")),
      Tab(core::Tr("Localisation")),
      Tab(core::Tr("Donjon")),
      Tab(core::Tr("Combat")),
      Tab(core::Tr("Duel")),
      Tab(core::Tr("Entraînement")),
      Tab(core::Tr("Quête")),
      Tab(core::Tr("Épisode")),
      Tab(core::Tr("Général")),
      Tab(core::Tr("Memorial")),
      Tab(core::Tr("Midgard")),
      Tab(core::Tr("Nouveau Monde")),
      Tab(core::Tr("Autre")),
      Tab(core::Tr("Exploit")),
  });
  // ⚠ UN TEXTE OUBLIÉ NE SE VERRAIT PAS : tous les mots suivants seraient
  // décalés d'un rang. Le compte se vérifie à la compilation.
  static_assert(std::tuple_size_v<decltype(texts)> == kAchievementWordCount);
  words.texts = std::move(texts);
}

}  // namespace ro::vfs
