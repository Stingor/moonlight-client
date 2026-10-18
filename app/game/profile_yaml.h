#pragma once

#include <optional>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "game/profile_book.h"

namespace moonclient {

// ── CE QUE LES FICHIERS DE PROFILS PARTAGENT ───────────────────────────────

// La racine d'un fichier de profils de cette version ; un nœud nul pour un
// texte vide. Rien pour un texte illisible ou d'une autre version.
[[nodiscard]] std::optional<YAML::Node> OpenProfileText(const std::string& text, int version);

// Les fiches des personnages, sous `characters`.
void WriteBindings(YAML::Emitter& out, const std::vector<ProfileBinding>& characters);
// Les fiches lisibles ; une fiche en double garde la dernière.
[[nodiscard]] std::vector<ProfileBinding> ReadBindings(const YAML::Node& whole);

inline constexpr const char* kProfileVersionField = "version";
inline constexpr const char* kProfilesField = "profiles";
inline constexpr const char* kProfileNameField = "name";

}  // namespace moonclient
