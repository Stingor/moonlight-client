#pragma once

// LE LIBELLÉ D'INTIMITÉ D'UN FAMILIER, tel que le montrent sa fenêtre et celle
// de l'éclosion.
//
// En-tête PRIVÉ de `libs/ui/src`.

#include "ro/core/i18n.h"
#include "ro/ui/pet_window.h"

namespace ro::ui {

[[nodiscard]] inline const char* PetIntimacyText(PetIntimacyLabel label) {
  // ⚠ DES LITTÉRAUX, un par branche : `core::Tr` veut voir sa chaîne à l'appel.
  switch (label) {
    case PetIntimacyLabel::kHateWithPassion:
      return core::Tr("Haine profonde");
    case PetIntimacyLabel::kHate:
      return core::Tr("Haine");
    case PetIntimacyLabel::kAwkward:
      return core::Tr("Distant");
    case PetIntimacyLabel::kShy:
      return core::Tr("Timide");
    case PetIntimacyLabel::kNeutral:
      return core::Tr("Neutre");
    case PetIntimacyLabel::kCordial:
      return core::Tr("Cordial");
    case PetIntimacyLabel::kLoyal:
      return core::Tr("Loyal");
    case PetIntimacyLabel::kUnknown:
      break;
  }
  return core::Tr("Inconnue");
}

}  // namespace ro::ui
