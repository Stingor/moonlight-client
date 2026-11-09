#include "character/character_styles.h"

#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "ro/core/log.h"
#include "ro/sim/style_code.h"

#include "character/style_yaml.h"

namespace moonclient {
namespace {

// Les champs du fichier. ⚠ GRAVÉS : les changer rend à tous les joueurs une
// tablée sans couleurs, sans que rien ne le dise.
constexpr const char* kCharactersField = "characters";
constexpr const char* kFallbackField = "fallback";
constexpr const char* kCodesField = "styles";

constexpr const char* kFileHeader =
    "# Les couleurs de chaque personnage joue sur ce poste, pour la selection.\n"
    "#\n"
    "# Le serveur ne les envoie qu'une fois en jeu : ce fichier les garde pour\n"
    "# que la tablee montre chacun dans ses couleurs. Il se reecrit a chaque\n"
    "# fois que le serveur les redit. L'effacer ne perd rien : les couleurs\n"
    "# reviennent au prochain passage en jeu de chaque personnage.\n"
    "#\n"
    "# Chaque code porte le corps sur lequel sa recette a ete faite ; « fallback »\n"
    "# designe celle qui vaut pour un corps qui n'a pas la sienne.\n";

// CE QU'UN PERSONNAGE PORTE, TEL QU'IL S'ÉCRIT : ses codes, dans l'ordre des
// corps, et son repli. C'est sur cette forme qu'on compare, champ par champ.
struct Written {
  std::vector<std::string> codes;
  std::uint32_t fallback_body = 0;

  bool operator==(const Written&) const = default;
};

[[nodiscard]] Written WrittenOf(const ro::sim::StyleVariants& variants) {
  Written written;
  written.fallback_body = variants.fallback_body;
  for (const auto& [body_key, recipe] : variants.by_body) {
    written.codes.push_back(ro::sim::EncodeStyleCode(recipe, body_key));
  }
  return written;
}

// Les couleurs d'une entrée, ou rien si aucune ne se lit.
[[nodiscard]] std::optional<ro::sim::StyleVariants> VariantsOf(const YAML::Node& entry) {
  if (!entry.IsMap() || !entry[kCodesField] || !entry[kCodesField].IsSequence()) {
    return std::nullopt;
  }
  ro::sim::StyleVariants variants;
  for (const YAML::Node& code : entry[kCodesField]) {
    const std::optional<ro::sim::StyleCode> read = StyleCodeAt(code);
    // ⚠ UN CODE SANS CORPS NE DÉSIGNE AUCUNE VARIANTE : il ne se retrouverait
    // jamais, puisque c'est le corps qui la choisit.
    if (read.has_value() && read->body_key != 0) {
      variants.by_body[read->body_key] = read->recipe;
    }
  }
  if (variants.empty()) {
    return std::nullopt;
  }
  const auto fallback = entry[kFallbackField] ? entry[kFallbackField].as<std::uint32_t>(0) : 0U;
  // Un repli qui ne désigne aucune variante gardée se rabat sur la première,
  // comme le fait le livre des styles à la réception.
  variants.fallback_body =
      variants.by_body.contains(fallback) ? fallback : variants.by_body.begin()->first;
  return variants;
}

}  // namespace

std::filesystem::path CharacterStylesPath(const std::filesystem::path& client_root) {
  return StyleFilePath(client_root, kCharacterStylesFileName);
}

std::map<std::uint32_t, ro::sim::StyleVariants> LoadCharacterStyles(
    const std::filesystem::path& client_root) {
  std::map<std::uint32_t, ro::sim::StyleVariants> kept;
  const std::optional<YAML::Node> whole =
      ReadStyleFile(CharacterStylesPath(client_root), "couleurs des personnages");
  if (!whole.has_value()) {
    return kept;
  }
  const YAML::Node characters = (*whole)[kCharactersField];
  if (!characters || !characters.IsMap()) {
    return kept;
  }
  try {
    for (const auto& entry : characters) {
      // ⚠ ZÉRO N'EST PAS UN PERSONNAGE : c'est ce qu'une clé illisible rend.
      const auto character_id = entry.first.as<std::uint32_t>(0);
      if (character_id == 0) {
        continue;
      }
      if (std::optional<ro::sim::StyleVariants> variants = VariantsOf(entry.second)) {
        kept.emplace(character_id, std::move(*variants));
      }
    }
  } catch (const std::exception& trouble) {
    ro::core::LogWarning("couleurs des personnages : entrée illisible ({})", trouble.what());
  }
  return kept;
}

bool NoteCharacterStyles(const std::filesystem::path& client_root, std::uint32_t character_id,
                         const ro::sim::StyleVariants* variants) {
  if (character_id == 0) {
    return false;
  }
  std::map<std::uint32_t, ro::sim::StyleVariants> kept = LoadCharacterStyles(client_root);
  const auto known = kept.find(character_id);
  const bool wears = variants != nullptr && !variants->empty();
  if (!wears) {
    if (known == kept.end()) {
      return false;
    }
    kept.erase(known);
  } else {
    if (known != kept.end() && WrittenOf(known->second) == WrittenOf(*variants)) {
      return false;
    }
    kept.insert_or_assign(character_id, *variants);
  }

  YAML::Emitter out;
  out << YAML::BeginMap << YAML::Key << kCharactersField << YAML::Value << YAML::BeginMap;
  for (const auto& [id, worn] : kept) {
    const Written written = WrittenOf(worn);
    out << YAML::Key << id << YAML::Value << YAML::BeginMap << YAML::Key << kFallbackField
        << YAML::Value << written.fallback_body << YAML::Key << kCodesField << YAML::Value
        << YAML::BeginSeq;
    for (const std::string& code : written.codes) {
      out << code;
    }
    out << YAML::EndSeq << YAML::EndMap;
  }
  out << YAML::EndMap << YAML::EndMap;
  return WriteStyleFile(CharacterStylesPath(client_root), kFileHeader, out,
                        "couleurs des personnages");
}

}  // namespace moonclient
