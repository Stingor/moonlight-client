// Les mots des ateliers. Voir l'en-tête.

#include "ro/vfs/craft_words.h"

#include <utility>

#include "ro/core/i18n.h"

namespace ro::vfs {
namespace {

constexpr std::string_view kStringConversion = "%s";

// Les quatre réponses d'un affinage, dans l'ordre du serveur.
constexpr int kRefineOutcomeCount = 4;

}  // namespace

std::string_view CantMakeText(const ItemWords& words) { return words.craft.cant_make; }

std::string_view MakeListTitle(const ItemWords& words) { return words.craft.make_list; }

std::string_view RequiredForText(const ItemWords& words) { return words.craft.required_for; }

std::string_view MakeResultTemplate(const ItemWords& words, bool succeeded) {
  return succeeded ? words.craft.make_succeeded : words.craft.make_failed;
}

std::string_view RefineResultTemplate(const ItemWords& words, int outcome) {
  switch (outcome) {
    case 0:
      return words.craft.refine_succeeded;
    case 1:
      return words.craft.refine_failed;
    case 2:
      return words.craft.refine_skill_too_low;
    case kRefineOutcomeCount - 1:
      return words.craft.refine_missing_ore;
    default:
      return {};
  }
}

bool FillFirstString(std::string_view pattern, std::string_view value, std::string* into) {
  const std::size_t at = pattern.find(kStringConversion);
  if (at == std::string_view::npos || into == nullptr) {
    return false;
  }
  std::string filled;
  filled.reserve(pattern.size() + value.size());
  filled.append(pattern.substr(0, at));
  filled.append(value);
  filled.append(pattern.substr(at + kStringConversion.size()));
  *into = std::move(filled);
  return true;
}

void FillCraftWords(CraftWords& words) {
  words.cant_make = core::Tr("Vous ne pouvez pas encore fabriquer d'items.");
  words.make_list = core::Tr("Liste de fabrication");
  words.required_for = core::Tr(": matériaux requis");
  words.make_succeeded = core::Tr("Fabrication de %s réussie.");
  words.make_failed = core::Tr("Échec de la fabrication de %s.");
  // LA RÉUSSITE ET L'ÉCHEC NE DISENT QUE L'ARME : « Succès — » ou « Échec — »
  // la précède déjà dans le journal, et la phrase entière faisait passer à la
  // ligne le nom le plus court. Ce n'est plus un texte à traduire.
  words.refine_succeeded = "%s";
  words.refine_failed = "%s";
  words.refine_skill_too_low = core::Tr(
      "Vous ne pouvez pas raffiner %s tant que vous n'avez pas augmenté le niveau de "
      "votre skill Upgrade Weapon.");
  words.refine_missing_ore = core::Tr("%s est nécessaire pour raffiner cette arme.");
}

}  // namespace ro::vfs
