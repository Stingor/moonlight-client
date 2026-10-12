#include "game/settings_profiles.h"

#include "ro/core/i18n.h"

#include "options/game_options.h"
#include "options/option_targets.h"

namespace moonclient {

void SettingsPort::AfterAssign() {
  if (to_ == nullptr) {
    return;
  }
  options_.ApplyAll(*to_, false);
  if (after_pose_) {
    after_pose_();
  }
}

ProfileFamilyText SettingsProfiles::Text() {
  return ProfileFamilyText{
      .title = ro::core::Tr("Profils de réglages"),
      .help = ro::core::Tr(
          "Un profil retient tous les réglages des options et les touches des raccourcis, sauf "
          "les réglages de la machine : écran, volumes, langue et échelle de l'interface. "
          "Chaque personnage garde le sien."),
      .id = "settings_profiles"};
}

}  // namespace moonclient
