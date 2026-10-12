// Les réglages de la fiche du pet. Voir l'en-tête.

#include "options/pet_options.h"

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// Les clés du fichier. ⚠ GRAVÉES, et en anglais : les changer rend à tous les
// joueurs les réglages d'origine, sans que rien ne le dise.
constexpr const char* kConfirmReturnToEggKey = "pet_confirm_return_to_egg";
constexpr const char* kAnimateSpriteKey = "pet_animate_sprite";

[[nodiscard]] ro::ui::OptionRowView Toggle(PetRow row, const char* label, const char* help,
                                           bool on) {
  return ro::ui::OptionRowView{.id = PetRowId(row),
                               .label = label,
                               .help = help,
                               .kind = ro::ui::OptionKind::kToggle,
                               .on = on};
}

}  // namespace

void PetOptions::Declare(SettingsFile& into) {
  into.Declare(kConfirmReturnToEggKey, &settings_.confirm_return_to_egg);
  into.Declare(kAnimateSpriteKey, &settings_.animate_sprite);
}

void PetOptions::ComposeInto(std::vector<ro::ui::OptionRowView>& rows) const {
  rows.push_back(Toggle(PetRow::kConfirmReturnToEgg, Tr("Confirmer « Remettre dans l'œuf »"),
                        Tr("Demande avant de remettre le pet dans son œuf. Le jeu ne le "
                           "demandait pas, et un clic de travers range le pet."),
                        settings_.confirm_return_to_egg));
  rows.back().heading = Tr("Fiche de pet");
  rows.push_back(Toggle(PetRow::kAnimateSprite, Tr("Animer le sprite"),
                        Tr("Le pet bouge dans sa fiche ; éteint, il reste sur sa première "
                           "image."),
                        settings_.animate_sprite));
}

bool PetOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kPetRowBase || asked.changed >= PetRowId(PetRow::kCount)) {
    return false;
  }
  switch (static_cast<PetRow>(asked.changed - kPetRowBase)) {
    case PetRow::kConfirmReturnToEgg:
      settings_.confirm_return_to_egg = asked.on;
      break;
    case PetRow::kAnimateSprite:
      settings_.animate_sprite = asked.on;
      break;
    case PetRow::kCount:
      break;
  }
  return true;
}

}  // namespace moonclient
