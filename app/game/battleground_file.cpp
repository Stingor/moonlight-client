// Le fichier des terrains du champ de bataille. Voir l'en-tête.

#include "game/battleground_file.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace moonclient {
namespace {

constexpr const char* kVersionKey = "version";
constexpr const char* kFieldsKey = "fields";
constexpr const char* kServerNameKey = "server_name";
constexpr const char* kNameKey = "name";
constexpr const char* kObjectiveKey = "objective";
constexpr const char* kHelpKey = "help";
constexpr const char* kMinimapKey = "minimap";
constexpr const char* kTeamKey = "team";
constexpr const char* kLevelKey = "level";
constexpr const char* kMinKey = "min";
constexpr const char* kMaxKey = "max";
constexpr const char* kApplyKey = "apply";
constexpr const char* kRewardsKey = "rewards";
constexpr const char* kWinKey = "win";
constexpr const char* kDrawKey = "draw";
constexpr const char* kLoseKey = "lose";
constexpr const char* kItemKey = "item";
constexpr const char* kCountKey = "count";
constexpr const char* kTextKey = "text";
constexpr std::string_view kApplySolo = "solo";
constexpr std::string_view kApplyParty = "party";
constexpr std::string_view kApplyGuild = "guild";

[[nodiscard]] std::unexpected<ro::core::Error> Broken(const std::string& why) {
  return ro::core::Fail(ro::core::ErrorCode::kCorruptData,
                        std::string(kBattlegroundFilePath) + " : " + why);
}

// La chaîne sous `key`, ou vide.
[[nodiscard]] std::string ScalarAt(const YAML::Node& map, const char* key) {
  const YAML::Node value = map[key];
  return value && value.IsScalar() ? value.as<std::string>() : std::string();
}

// L'entier positif sous `key`, ou zéro quand il manque ou ne se lit pas.
[[nodiscard]] int WholeAt(const YAML::Node& map, const char* key) {
  const YAML::Node value = map[key];
  if (!value || !value.IsScalar()) {
    return 0;
  }
  return std::max(0, value.as<int>(0));
}

// Les récompenses d'une issue. Une entrée sans objet ni texte est sautée, et dite.
[[nodiscard]] std::vector<BattlegroundReward> RewardsAt(const YAML::Node& rewards, const char* key,
                                                        const std::string& where,
                                                        std::vector<std::string>& problems) {
  std::vector<BattlegroundReward> found;
  const YAML::Node list = rewards ? rewards[key] : YAML::Node();
  if (!list || !list.IsSequence()) {
    return found;
  }
  for (std::size_t rank = 0; rank < list.size(); ++rank) {
    const YAML::Node entry = list[rank];
    BattlegroundReward reward;
    if (entry.IsMap()) {
      reward.item_id = static_cast<std::uint32_t>(WholeAt(entry, kItemKey));
      if (entry[kCountKey]) {
        reward.count = WholeAt(entry, kCountKey);
      }
      reward.text = ReadYamlText(entry[kTextKey]);
    }
    if (reward.item_id == 0 && reward.text.empty()) {
      problems.push_back(where + " : récompense « " + key + " » n° " + std::to_string(rank + 1) +
                         " sans objet ni texte, écartée");
      continue;
    }
    found.push_back(std::move(reward));
  }
  return found;
}

// Les modes d'inscription : une liste de mots. Un mot inconnu est dit.
void ReadApply(const YAML::Node& list, BattlegroundField& field, const std::string& where,
               std::vector<std::string>& problems) {
  if (!list || !list.IsSequence()) {
    return;
  }
  for (const YAML::Node& entry : list) {
    const std::string word = entry.IsScalar() ? entry.as<std::string>() : std::string();
    if (word == kApplySolo) {
      field.solo_apply = true;
    } else if (word == kApplyParty) {
      field.party_apply = true;
    } else if (word == kApplyGuild) {
      field.guild_apply = true;
    } else {
      problems.push_back(where + " : mode d'inscription « " + word + " » inconnu, ignoré");
    }
  }
}

// Un terrain, ou rien quand l'entrée n'a pas de nom à envoyer au serveur : on
// ne saurait pas s'y inscrire.
[[nodiscard]] std::optional<BattlegroundField> FieldOf(const YAML::Node& entry, std::size_t rank,
                                                       std::vector<std::string>& problems) {
  const std::string where = "terrain n° " + std::to_string(rank + 1);
  if (!entry.IsMap()) {
    problems.push_back(where + " : une table est attendue, écarté");
    return std::nullopt;
  }
  BattlegroundField field;
  field.server_name = ScalarAt(entry, kServerNameKey);
  if (field.server_name.empty()) {
    problems.push_back(where + " : sans « server_name », écarté");
    return std::nullopt;
  }
  const std::string named = where + " (" + field.server_name + ")";
  field.name = ReadYamlText(entry[kNameKey]);
  field.objective = ReadYamlText(entry[kObjectiveKey]);
  field.help = ReadYamlText(entry[kHelpKey]);
  field.minimap_file = ScalarAt(entry, kMinimapKey);
  if (const YAML::Node team = entry[kTeamKey]; team && team.IsMap()) {
    field.team_min = WholeAt(team, kMinKey);
    field.team_max = WholeAt(team, kMaxKey);
  }
  if (const YAML::Node level = entry[kLevelKey]; level && level.IsMap()) {
    field.level_min = WholeAt(level, kMinKey);
    field.level_max = WholeAt(level, kMaxKey);
  }
  ReadApply(entry[kApplyKey], field, named, problems);
  const YAML::Node rewards = entry[kRewardsKey];
  field.reward_win = RewardsAt(rewards, kWinKey, named, problems);
  field.reward_draw = RewardsAt(rewards, kDrawKey, named, problems);
  field.reward_lose = RewardsAt(rewards, kLoseKey, named, problems);
  return field;
}

// LES TRADUCTIONS INCOMPLÈTES : une langue écrite quelque part dans le fichier
// et absente d'un texte se verrait en français chez qui la lit.
void TellMissingLanguages(const std::vector<BattlegroundField>& fields,
                          std::vector<std::string>& problems) {
  std::set<std::string> languages;
  const auto collect = [&languages](const BattlegroundText& text) {
    for (const auto& [language, written] : text.by_language) {
      languages.insert(language);
    }
  };
  const auto rewards_of = [](const BattlegroundField& field) {
    return std::array<const std::vector<BattlegroundReward>*, 3>{
        &field.reward_win, &field.reward_draw, &field.reward_lose};
  };
  for (const BattlegroundField& field : fields) {
    collect(field.name);
    collect(field.objective);
    collect(field.help);
    for (const std::vector<BattlegroundReward>* rewards : rewards_of(field)) {
      for (const BattlegroundReward& reward : *rewards) {
        collect(reward.text);
      }
    }
  }
  const auto check = [&](const BattlegroundField& field, const BattlegroundText& text,
                         std::string_view what) {
    if (text.empty()) {
      return;
    }
    for (const std::string& language : languages) {
      const bool written = std::ranges::any_of(
          text.by_language, [&language](const auto& entry) { return entry.first == language; });
      if (!written) {
        problems.push_back(field.server_name + " : « " + std::string(what) + " » sans texte en « " +
                           language + " »");
      }
    }
  };
  for (const BattlegroundField& field : fields) {
    check(field, field.name, kNameKey);
    check(field, field.objective, kObjectiveKey);
    check(field, field.help, kHelpKey);
    for (const std::vector<BattlegroundReward>* rewards : rewards_of(field)) {
      for (const BattlegroundReward& reward : *rewards) {
        check(field, reward.text, kRewardsKey);
      }
    }
  }
}

}  // namespace

ro::core::Result<BattlegroundFile> BattlegroundFile::Parse(std::string_view text) {
  YAML::Node root;
  try {
    root = YAML::Load(std::string(text));
  } catch (const YAML::Exception& broken) {
    return Broken(broken.what());
  }
  if (!root.IsMap()) {
    return Broken("une table est attendue à la racine");
  }
  if (const int version = WholeAt(root, kVersionKey); version != kBattlegroundFileVersion) {
    return Broken("version " + std::to_string(version) +
                  " inconnue (attendue : " + std::to_string(kBattlegroundFileVersion) + ")");
  }
  const YAML::Node list = root[kFieldsKey];
  if (!list || !list.IsSequence()) {
    return Broken("aucune liste « fields »");
  }
  BattlegroundFile file;
  for (std::size_t rank = 0; rank < list.size(); ++rank) {
    std::optional<BattlegroundField> field = FieldOf(list[rank], rank, file.problems_);
    if (!field.has_value()) {
      continue;
    }
    // Deux terrains du même nom : le serveur n'en connaît qu'un, la liste aussi.
    if (file.FindByServerName(field->server_name) != nullptr) {
      file.problems_.push_back("terrain n° " + std::to_string(rank + 1) + " : « " +
                               field->server_name + " » déjà décrit, écarté");
      continue;
    }
    file.fields_.push_back(std::move(*field));
  }
  TellMissingLanguages(file.fields_, file.problems_);
  return file;
}

const BattlegroundField* BattlegroundFile::FindByServerName(std::string_view server_name) const {
  const auto found = std::ranges::find(fields_, server_name, &BattlegroundField::server_name);
  return found == fields_.end() ? nullptr : &*found;
}

}  // namespace moonclient
