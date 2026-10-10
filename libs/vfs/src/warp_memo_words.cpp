// Les phrases du point de Warp mémorisé. Voir l'en-tête.

#include "ro/vfs/warp_memo_words.h"

#include <cstddef>

#include "ro/core/i18n.h"

namespace ro::vfs {
namespace {

[[nodiscard]] std::string& Sentence(WarpMemoWords& words, WarpMemoMessage message) {
  return words.texts[static_cast<std::size_t>(message)];
}

}  // namespace

// La dernière phrase est celle du dernier sens : l'énumération et le tableau ne
// dérivent pas l'un de l'autre sans que cela se voie à la compilation.
static_assert(static_cast<std::size_t>(WarpMemoMessage::kItemForbiddenHere) + 1 ==
              kWarpMemoMessageCount);

std::string_view WarpMemoText(const ItemWords& words, WarpMemoMessage message) {
  const auto rank = static_cast<std::size_t>(message);
  if (rank >= words.warp_memo.texts.size()) {
    return {};
  }
  return words.warp_memo.texts[rank];
}

void FillWarpMemoWords(WarpMemoWords& words) {
  Sentence(words, WarpMemoMessage::kSaved) =
      core::Tr("Le lieu actuel est enregistré comme point Memo pour le skill Warp.");
  Sentence(words, WarpMemoMessage::kSkillTooLow) = core::Tr("Niveau de skill insuffisant");
  Sentence(words, WarpMemoMessage::kSkillNotLearned) =
      core::Tr("Vous n'avez pas appris le skill Warp.");
  Sentence(words, WarpMemoMessage::kCannotSaveHere) =
      core::Tr("Impossible d'enregistrer un point de mémo à cet endroit.");
  Sentence(words, WarpMemoMessage::kTeleportForbidden) =
      core::Tr("Téléportation impossible dans cette zone");
  Sentence(words, WarpMemoMessage::kMemoForbidden) =
      core::Tr("Impossible de mémoriser ce lieu comme point de warp");
  Sentence(words, WarpMemoMessage::kSkillForbiddenHere) =
      core::Tr("Ce skill ne peut pas être utilisé dans cette zone.");
  Sentence(words, WarpMemoMessage::kItemForbiddenHere) =
      core::Tr("Cet item ne peut pas être utilisé dans cette zone.");
}

}  // namespace ro::vfs
