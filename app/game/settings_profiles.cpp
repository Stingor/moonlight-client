#include "game/settings_profiles.h"

#include "ro/core/i18n.h"

#include "options/game_options.h"
#include "options/option_targets.h"

namespace moonclient {

void SettingsPort::Pose(const SettingsValues& profile) {
  file_.AssignShared(profile);
  if (to_ != nullptr) {
    options_.ApplyAll(*to_, false);
    if (after_pose_) {
      after_pose_();
    }
  }
  // ⚠ LE FICHIER VIVANT SUIT : le profil posé est le réglage présent, et la
  // prochaine session le relira avant même d'entrer en jeu.
  file_.Touch();
}

bool SettingsPort::Load(SettingsProfileBook& into) {
  return LoadSettingsProfiles(client_root_, into);
}

void SettingsPort::Save(const SettingsProfileBook& from) {
  static_cast<void>(SaveSettingsProfiles(client_root_, from));
}

ProfileFamilyText SettingsProfiles::Text() {
  return ProfileFamilyText{
      .title = ro::core::Tr("Profils de réglages"),
      .help = ro::core::Tr(
          "Un profil retient tous les réglages des options, sauf ceux de la machine : écran, "
          "volumes, langue et échelle de l'interface. Chaque personnage garde le sien."),
      .id = "settings_profiles"};
}

}  // namespace moonclient
