// Les profils d'habillage. Voir l'en-tête.

#include "options/skin_look_profiles.h"

#include <optional>
#include <string>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"

#include "game/profile_yaml.h"
#include "game/save_folder.h"
#include "game/yaml_load.h"

namespace moonclient {

std::filesystem::path SkinLookProfilePath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kSkinLookProfileFileName;
}

void SkinLookPort::Pose(const SettingsValues& profile) {
  look_.AssignShared(profile);
  // ⚠ LE FICHIER DU SKIN SUIT : le profil posé est l'habillage présent.
  look_.Touch();
}

bool SkinLookPort::Load(SettingsProfileBook& into) {
  const std::optional<std::string> text = ReadFileText(profile_path_);
  if (!text.has_value()) {
    ro::core::LogWarning("profils d'habillage : {} ne s'ouvre pas",
                         ro::core::Utf8FromPath(profile_path_));
    return false;
  }
  if (!ReadSettingsProfiles(*text, into)) {
    ro::core::LogWarning(
        "profils d'habillage : {} illisible ou d'une version inconnue, il ne sera pas réécrit",
        ro::core::Utf8FromPath(profile_path_));
    return false;
  }
  return true;
}

void SkinLookPort::Save(const SettingsProfileBook& from) {
  if (const ro::core::Result<void> written =
          ro::core::WriteFileAtomically(profile_path_, WriteSettingsProfiles(from));
      !written) {
    ro::core::LogWarning("profils d'habillage : écriture impossible ({})",
                         ro::core::Describe(written.error()));
  }
}

ProfileFamilyText SkinLookProfiles::Text() {
  return ProfileFamilyText{
      .title = ro::core::Tr("Profils d'habillage"),
      .help = ro::core::Tr("Un profil retient tout l'habillage des fenêtres : le thème, chaque "
                           "couleur, la luminosité, l'opacité et l'arrondi. Communs à tous les "
                           "skins : un profil se pose sur n'importe lequel."),
      .id = "skin_look_profiles"};
}

}  // namespace moonclient
