#pragma once

#include <string_view>

#include <imgui.h>

#include "ro/ui/skin_look.h"

namespace ro::ui {

// UNE LIGNE DU JOURNAL DE SESSION d'un atelier — fabrication ou affinage.
//
// ⚠ C'EST LE SEUL ENDROIT OÙ UN SUCCÈS ET UN ÉCHEC D'AFFINAGE SE DISTINGUENT : le
// client leur donne le même texte, la couleur les sépare — par son rôle, que
// l'habillage du moment résout.
struct CraftLogLineView {
  std::string_view text;
  SkinTone tone = SkinTone::kText;
  // « HH:MM:SS », calculé à l'insertion.
  std::string_view time;
};

}  // namespace ro::ui
