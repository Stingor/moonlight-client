#include "game/settings_profile_file.h"

#include <optional>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"

#include "game/profile_yaml.h"
#include "game/save_folder.h"

namespace moonclient {
namespace {

// Le champ d'un profil qui porte ses réglages. ⚠ GRAVÉ.
constexpr const char* kSettingsField = "settings";

constexpr const char* kFileHeader =
    "# Les profils de réglages.\n"
    "#\n"
    "# Sous « profiles », chaque profil porte les réglages de settings.yml, sous\n"
    "# les mêmes clés — sauf ceux de la machine : écran, son, langue, échelle.\n"
    "# Une clé absente prend sa valeur d'origine. Sous « characters », le profil\n"
    "# que chaque personnage pose en entrant en jeu ; sans fiche, c'est « Default ».\n";

[[nodiscard]] std::optional<Named<SettingsValues>> ReadProfile(const YAML::Node& entry) {
  if (!entry.IsMap() || !entry[kProfileNameField] || !entry[kProfileNameField].IsScalar()) {
    return std::nullopt;
  }
  Named<SettingsValues> named;
  named.name = entry[kProfileNameField].Scalar();
  if (named.name.empty()) {
    return std::nullopt;
  }
  if (const YAML::Node settings = entry[kSettingsField]; settings && settings.IsMap()) {
    for (const auto& pair : settings) {
      if (pair.first.IsScalar() && pair.second.IsScalar()) {
        // ⚠ LA FORME ÉCRITE, guillemets compris : `SettingsFile` relit ce texte
        // comme il relit le sien.
        named.profile.insert_or_assign(pair.first.Scalar(), YAML::Dump(pair.second));
      }
    }
  }
  return named;
}

}  // namespace

std::filesystem::path SettingsProfilePath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kSettingsProfileFileName;
}

std::string WriteSettingsProfiles(const SettingsProfileBook& book) {
  YAML::Emitter out;
  out << YAML::BeginMap;
  out << YAML::Key << kProfileVersionField << YAML::Value << kSettingsProfileFileVersion;
  out << YAML::Key << kProfilesField << YAML::Value << YAML::BeginSeq;
  for (const Named<SettingsValues>& named : book.profiles) {
    out << YAML::BeginMap;
    out << YAML::Key << kProfileNameField << YAML::Value << named.name;
    out << YAML::Key << kSettingsField << YAML::Value << YAML::BeginMap;
    for (const auto& [key, text] : named.profile) {
      YAML::Node value;
      try {
        value = YAML::Load(text);
      } catch (const YAML::Exception&) {
        continue;
      }
      out << YAML::Key << key << YAML::Value << value;
    }
    out << YAML::EndMap;
    out << YAML::EndMap;
  }
  out << YAML::EndSeq;
  WriteBindings(out, book.characters);
  out << YAML::EndMap;
  return std::string(kFileHeader) + out.c_str() + "\n";
}

bool ReadSettingsProfiles(std::string_view text, SettingsProfileBook& into) {
  const std::optional<YAML::Node> whole =
      OpenProfileText(std::string(text), kSettingsProfileFileVersion);
  if (!whole.has_value()) {
    return false;
  }
  SettingsProfileBook book;
  if (!*whole) {
    into = std::move(book);
    return true;
  }
  if (const YAML::Node profiles = (*whole)[kProfilesField]; profiles && profiles.IsSequence()) {
    for (const YAML::Node& entry : profiles) {
      std::optional<Named<SettingsValues>> named = ReadProfile(entry);
      // ⚠ UN NOM EN DOUBLE GARDE LE PREMIER, et le plafond tient.
      if (named && book.Find(named->name) == nullptr && book.profiles.size() < kProfileMost) {
        book.profiles.push_back(std::move(*named));
      }
    }
  }
  book.characters = ReadBindings(*whole);
  into = std::move(book);
  return true;
}

bool LoadSettingsProfiles(const std::filesystem::path& client_root, SettingsProfileBook& into) {
  const std::filesystem::path path = SettingsProfilePath(client_root);
  const std::optional<std::string> text = ReadProfileFile(path);
  if (!text.has_value()) {
    ro::core::LogWarning("profils de réglages : {} ne s'ouvre pas", path.string());
    return false;
  }
  if (!ReadSettingsProfiles(*text, into)) {
    ro::core::LogWarning(
        "profils de réglages : {} illisible ou d'une version inconnue, il ne sera pas réécrit",
        path.string());
    return false;
  }
  return true;
}

bool SaveSettingsProfiles(const std::filesystem::path& client_root,
                          const SettingsProfileBook& from) {
  if (const ro::core::Result<void> written = ro::core::WriteFileAtomically(
          SettingsProfilePath(client_root), WriteSettingsProfiles(from));
      !written) {
    ro::core::LogWarning("profils de réglages : écriture impossible ({})",
                         ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace moonclient
