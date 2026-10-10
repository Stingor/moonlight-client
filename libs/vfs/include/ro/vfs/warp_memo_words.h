#pragma once

#include <string>
#include <string_view>

#include "ro/vfs/item_words.h"

namespace ro::vfs {

// CE QUE LE CLIENT DIT D'UN POINT DE WARP MÉMORISÉ, ET D'UN REFUS TENANT À LA
// CARTE. Une phrase par sens, posée par `FillWarpMemoWords` (ADR 0090). Vide
// quand rien ne l'a posée : l'appelant n'écrit alors aucune ligne.
//
// 🔴 CE N'EST PAS UN CODE DU PROTOCOLE. Deux paquets distincts se rangent ici, et
// c'est la couche qui les décode qui dit quel code donne quel sens : `vfs` ne
// sait que nommer les textes.
enum class WarpMemoMessage {
  kSaved,
  kSkillTooLow,
  kSkillNotLearned,
  kCannotSaveHere,
  kTeleportForbidden,
  kMemoForbidden,
  kSkillForbiddenHere,
  kItemForbiddenHere,
};

// La phrase du sens demandé.
//
// La vue rendue vit aussi longtemps que `words` : elle ne pend pas.
[[nodiscard]] std::string_view WarpMemoText(const ItemWords& words, WarpMemoMessage message);

// Pose ces phrases dans la langue courante.
void FillWarpMemoWords(WarpMemoWords& words);

}  // namespace ro::vfs
