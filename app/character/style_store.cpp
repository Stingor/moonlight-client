#include "character/style_store.h"

#include <algorithm>
#include <optional>
#include <system_error>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/sim/style_code.h"

namespace moonclient {
namespace {

// Les champs des deux fichiers. ⚠ GRAVÉS : les changer rend à tous les joueurs
// une liste vide, sans que rien ne le dise.
constexpr const char* kPresetsField = "presets";
constexpr const char* kPresetNameField = "name";
constexpr const char* kPresetStyleField = "style";
constexpr const char* kDraftsField = "drafts";

constexpr const char* kPresetsHeader =
    "# Les styles que tu as mis de côté.\n"
    "#\n"
    "# Chaque ligne porte un nom et un CODE — la même chaîne que « Copier le\n"
    "# code » met dans le presse-papiers. Tu peux donc en recopier un d'ici,\n"
    "# ou en coller un qu'on t'a donné.\n"
    "#\n"
    "# ⚠ UN STYLE NE VAUT VRAIMENT QUE POUR LE CORPS SUR LEQUEL IL A ÉTÉ FAIT.\n"
    "# Appliqué ailleurs, il se ré-applique par RANG de pièce : le résultat\n"
    "# reste plausible — les pièces sont classées par surface — sans être\n"
    "# identique.\n"
    "#\n"
    "# ⚠ Un code d'une version périmée est ignoré à la lecture, et la ligne\n"
    "# disparaît à la prochaine écriture.\n";

constexpr const char* kDraftsHeader =
    "# Ce que tu composais sans le valider, par CORPS.\n"
    "#\n"
    "# Un FILET, pas un historique : il s'écrase sans prévenir et disparaît\n"
    "# dès que tu valides.\n"
    "#\n"
    "# La clé est celle du sprite de corps — à pied, en selle, en costume :\n"
    "# une recette ne désigne ses pièces que par des index de palette, et les\n"
    "# index n'ont aucune signification commune d'un corps à l'autre.\n";

[[nodiscard]] std::optional<ro::sim::StyleRecipe> ReadCode(const YAML::Node& node) {
  if (!node || !node.IsScalar()) {
    return std::nullopt;
  }
  const std::optional<ro::sim::StyleCode> read =
      ro::sim::DecodeStyleCode(node.as<std::string>(std::string{}));
  if (!read.has_value()) {
    return std::nullopt;
  }
  return read->recipe;
}

// La seule exception qu'on attrape, et pour la même raison qu'ailleurs : un
// fichier abîmé ne doit pas empêcher de jouer.
[[nodiscard]] std::optional<YAML::Node> ReadFile(const std::filesystem::path& path,
                                                 const char* what) {
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return std::nullopt;  // première partie : rien n'a encore été rangé
  }
  try {
    YAML::Node whole = YAML::LoadFile(path.string());
    // ⚠ UNE RACINE QUI N'EST PAS UNE TABLE N'A PAS DE CHAMPS : l'indexer lève
    // chez l'appelant, hors de ce `try`.
    if (!whole.IsMap()) {
      return std::nullopt;
    }
    return whole;
  } catch (const YAML::Exception& broken) {
    ro::core::LogWarning("{} : {}", what, broken.what());
    return std::nullopt;
  }
}

[[nodiscard]] bool WriteFile(const std::filesystem::path& path, const std::string& text,
                             const char* what) {
  if (const ro::core::Result<void> written = ro::core::WriteFileAtomically(path, text); !written) {
    ro::core::LogWarning("{} : {}", what, ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace

bool StylePresets::Save(std::string_view name, const ro::sim::StyleRecipe& recipe) {
  if (name.empty()) {
    return false;
  }
  const auto found = std::ranges::find(kept_, name, &Named::name);
  if (found != kept_.end()) {
    // ⚠ À SA PLACE : mettre à jour un préréglage ne doit pas le faire sauter
    // en fin de liste sous les doigts de celui qui le retouche.
    found->recipe = recipe;
    return true;
  }
  if (full()) {
    return false;
  }
  kept_.push_back(Named{.name = std::string(name), .recipe = recipe});
  return true;
}

const ro::sim::StyleRecipe* StylePresets::Find(std::string_view name) const {
  const auto found = std::ranges::find(kept_, name, &Named::name);
  return found == kept_.end() ? nullptr : &found->recipe;
}

void StylePresets::Erase(std::string_view name) {
  std::erase_if(kept_, [name](const Named& kept) { return kept.name == name; });
}

bool StyleDrafts::Put(std::uint32_t body_key, const ro::sim::StyleRecipe& recipe) {
  if (body_key == 0) {
    return false;
  }
  // ⚠ COMPARÉ PAR SON CODE, ce qui compare tous les champs d'un coup sans
  // risquer d'en oublier un à la prochaine version de la recette.
  const std::string encoded = ro::sim::EncodeStyleCode(recipe);
  const auto found = kept_.find(body_key);
  if (found != kept_.end() && ro::sim::EncodeStyleCode(found->second) == encoded) {
    return false;  // rien de neuf : pas d'écriture
  }
  kept_[body_key] = recipe;
  return true;
}

bool StyleDrafts::Forget(std::uint32_t body_key) { return kept_.erase(body_key) != 0; }

const ro::sim::StyleRecipe* StyleDrafts::Find(std::uint32_t body_key) const {
  const auto found = kept_.find(body_key);
  return found == kept_.end() ? nullptr : &found->second;
}

std::filesystem::path StylePresetsPath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kStylePresetsFileName;
}

std::filesystem::path StyleDraftsPath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kStyleDraftsFileName;
}

void LoadStylePresets(const std::filesystem::path& client_root, StylePresets& into) {
  const std::optional<YAML::Node> whole =
      ReadFile(StylePresetsPath(client_root), "préréglages de style");
  if (!whole.has_value()) {
    return;
  }
  const YAML::Node listed = (*whole)[kPresetsField];
  if (!listed || !listed.IsSequence()) {
    return;
  }
  for (const YAML::Node& entry : listed) {
    if (!entry.IsMap()) {
      continue;
    }
    const YAML::Node name = entry[kPresetNameField];
    if (!name || !name.IsScalar()) {
      continue;
    }
    if (const std::optional<ro::sim::StyleRecipe> recipe = ReadCode(entry[kPresetStyleField])) {
      static_cast<void>(into.Save(name.as<std::string>(std::string{}), *recipe));
    }
  }
}

void LoadStyleDrafts(const std::filesystem::path& client_root, StyleDrafts& into) {
  const std::optional<YAML::Node> whole =
      ReadFile(StyleDraftsPath(client_root), "brouillon de style");
  if (!whole.has_value()) {
    return;
  }
  const YAML::Node listed = (*whole)[kDraftsField];
  if (!listed || !listed.IsMap()) {
    return;
  }
  for (const auto& entry : listed) {
    const auto body_key = entry.first.as<std::uint32_t>(0U);
    if (const std::optional<ro::sim::StyleRecipe> recipe = ReadCode(entry.second)) {
      static_cast<void>(into.Put(body_key, *recipe));
    }
  }
}

bool SaveStylePresets(const std::filesystem::path& client_root, const StylePresets& from) {
  YAML::Emitter out;
  out << YAML::BeginMap << YAML::Key << kPresetsField << YAML::Value << YAML::BeginSeq;
  for (const StylePresets::Named& kept : from.all()) {
    out << YAML::BeginMap << YAML::Key << kPresetNameField << YAML::Value << kept.name << YAML::Key
        << kPresetStyleField << YAML::Value << ro::sim::EncodeStyleCode(kept.recipe)
        << YAML::EndMap;
  }
  out << YAML::EndSeq << YAML::EndMap;
  return WriteFile(StylePresetsPath(client_root), std::string(kPresetsHeader) + out.c_str() + "\n",
                   "préréglages de style");
}

bool SaveStyleDrafts(const std::filesystem::path& client_root, const StyleDrafts& from) {
  YAML::Emitter out;
  out << YAML::BeginMap << YAML::Key << kDraftsField << YAML::Value << YAML::BeginMap;
  for (const auto& [body_key, recipe] : from.all()) {
    out << YAML::Key << body_key << YAML::Value << ro::sim::EncodeStyleCode(recipe);
  }
  out << YAML::EndMap << YAML::EndMap;
  return WriteFile(StyleDraftsPath(client_root), std::string(kDraftsHeader) + out.c_str() + "\n",
                   "brouillon de style");
}

}  // namespace moonclient
