#include "character/style_store.h"

#include <algorithm>
#include <optional>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/sim/style_code.h"

#include "character/style_yaml.h"

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
    "# Ce que tu composais sans le valider, par PERSONNAGE puis par CORPS.\n"
    "#\n"
    "# Un FILET, pas un historique : il s'écrase sans prévenir et disparaît\n"
    "# dès que tu valides.\n"
    "#\n"
    "# La première clé est le numéro du personnage : deux personnages de même\n"
    "# classe ne partagent pas leurs essais. La seconde est celle du sprite de\n"
    "# corps — à pied, en selle, en costume : une recette ne désigne ses pièces\n"
    "# que par des index de palette, et les index n'ont aucune signification\n"
    "# commune d'un corps à l'autre.\n";

[[nodiscard]] std::optional<ro::sim::StyleRecipe> ReadCode(const YAML::Node& node) {
  const std::optional<ro::sim::StyleCode> read = StyleCodeAt(node);
  if (!read.has_value()) {
    return std::nullopt;
  }
  return read->recipe;
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

bool StylePresets::Rename(std::string_view from, std::string_view to) {
  if (to.empty() || Find(to) != nullptr) {
    return false;
  }
  const auto found = std::ranges::find(kept_, from, &Named::name);
  if (found == kept_.end()) {
    return false;
  }
  // ⚠ À SA PLACE, comme une mise à jour.
  found->name = std::string(to);
  return true;
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
  return StyleFilePath(client_root, kStylePresetsFileName);
}

std::filesystem::path StyleDraftsPath(const std::filesystem::path& client_root) {
  return StyleFilePath(client_root, kStyleDraftsFileName);
}

void LoadStylePresets(const std::filesystem::path& client_root, StylePresets& into) {
  const std::optional<YAML::Node> whole =
      ReadStyleFile(StylePresetsPath(client_root), "préréglages de style");
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

namespace {

// Les brouillons d'un personnage, lus dans sa section.
void ReadDrafts(const YAML::Node& listed, StyleDrafts& into) {
  for (const auto& entry : listed) {
    const auto body_key = entry.first.as<std::uint32_t>(0U);
    if (const std::optional<ro::sim::StyleRecipe> recipe = ReadCode(entry.second)) {
      static_cast<void>(into.Put(body_key, *recipe));
    }
  }
}

}  // namespace

void LoadStyleDrafts(const std::filesystem::path& client_root, std::uint32_t character_id,
                     StyleDrafts& into) {
  const std::optional<YAML::Node> whole =
      ReadStyleFile(StyleDraftsPath(client_root), "brouillon de style");
  if (!whole.has_value()) {
    return;
  }
  const YAML::Node listed = (*whole)[kDraftsField];
  if (!listed || !listed.IsMap()) {
    return;
  }
  for (const auto& entry : listed) {
    // ⚠ L'ANCIENNE FORME, un code par corps sans personnage : le premier
    // personnage qui ouvre l'éditeur la reprend, plutôt que de la perdre.
    if (entry.second.IsScalar()) {
      if (const std::optional<ro::sim::StyleRecipe> recipe = ReadCode(entry.second)) {
        static_cast<void>(into.Put(entry.first.as<std::uint32_t>(0U), *recipe));
      }
      continue;
    }
    if (entry.second.IsMap() && entry.first.as<std::uint32_t>(0U) == character_id) {
      ReadDrafts(entry.second, into);
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
  return WriteStyleFile(StylePresetsPath(client_root), kPresetsHeader, out, "préréglages de style");
}

bool SaveStyleDrafts(const std::filesystem::path& client_root, std::uint32_t character_id,
                     const StyleDrafts& from) {
  // 🔴 LES AUTRES PERSONNAGES SE RELISENT ET SE RÉÉCRIVENT TELS QUELS : le
  // fichier est commun, et l'écrire depuis ce seul personnage effacerait les
  // brouillons des autres.
  std::map<std::uint32_t, std::map<std::uint32_t, std::string>> everyone;
  if (const std::optional<YAML::Node> whole =
          ReadStyleFile(StyleDraftsPath(client_root), "brouillon de style")) {
    const YAML::Node listed = (*whole)[kDraftsField];
    if (listed && listed.IsMap()) {
      for (const auto& entry : listed) {
        const auto owner = entry.first.as<std::uint32_t>(0U);
        if (!entry.second.IsMap() || owner == character_id) {
          continue;
        }
        for (const auto& kept : entry.second) {
          everyone[owner][kept.first.as<std::uint32_t>(0U)] = kept.second.as<std::string>("");
        }
      }
    }
  }
  everyone.erase(character_id);
  for (const auto& [body_key, recipe] : from.all()) {
    everyone[character_id][body_key] = ro::sim::EncodeStyleCode(recipe);
  }

  YAML::Emitter out;
  out << YAML::BeginMap << YAML::Key << kDraftsField << YAML::Value << YAML::BeginMap;
  for (const auto& [owner, drafts] : everyone) {
    if (drafts.empty()) {
      continue;
    }
    out << YAML::Key << owner << YAML::Value << YAML::BeginMap;
    for (const auto& [body_key, code] : drafts) {
      out << YAML::Key << body_key << YAML::Value << code;
    }
    out << YAML::EndMap;
  }
  out << YAML::EndMap << YAML::EndMap;
  return WriteStyleFile(StyleDraftsPath(client_root), kDraftsHeader, out, "brouillon de style");
}

}  // namespace moonclient
