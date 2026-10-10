#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "game/profile_book.h"
#include "game/settings_file.h"

namespace moonclient {

// ── LES PROFILS DE RÉGLAGES SUR LE DISQUE : `savedata/settings_profiles.yml`
//
// Des profils nommés, chacun les réglages de `settings.yml` sauf ceux de la
// machine — voir `SettingsFile::MachineScope` —, et le profil de chaque
// personnage. `settings.yml` reste le réglage VIVANT ; un profil en est une
// copie, qui se pose en entrant en jeu.
//
// ⚠ LE FORMAT EST VERSIONNÉ : un fichier d'une version inconnue ne se relit
// pas, et ne s'écrase pas.

inline constexpr int kSettingsProfileFileVersion = 1;
inline constexpr std::string_view kSettingsProfileFileName = "settings_profiles.yml";

using SettingsProfileBook = ProfileBook<SettingsValues>;

[[nodiscard]] std::filesystem::path SettingsProfilePath(const std::filesystem::path& client_root);

[[nodiscard]] std::string WriteSettingsProfiles(const SettingsProfileBook& book);
// Faux pour un texte illisible ou d'une version inconnue : `into` reste alors
// tel quel. Une valeur qui n'est pas un scalaire est sautée.
bool ReadSettingsProfiles(std::string_view text, SettingsProfileBook& into);

// Un fichier absent laisse le livre vide, et rend vrai. Faux pour un fichier
// qu'on ne doit pas écraser.
bool LoadSettingsProfiles(const std::filesystem::path& client_root, SettingsProfileBook& into);
// Vrai si le fichier est écrit.
bool SaveSettingsProfiles(const std::filesystem::path& client_root,
                          const SettingsProfileBook& from);

}  // namespace moonclient
