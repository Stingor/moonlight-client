#include "items/craft_book.h"

#include <algorithm>
#include <charconv>
#include <exception>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>

#include <yaml-cpp/yaml.h>

namespace moonclient {
namespace {

// Les sections et les champs du fichier. GRAVÉS par le générateur du serveur.
constexpr const char* kRecipesSection = "recipes";
constexpr const char* kArrowsSection = "arrows";
constexpr const char* kBySkillSection = "by_skill";
constexpr const char* kServerSection = "server";
constexpr const char* kYieldSection = "produce_qty";
constexpr const char* kBulkSection = "bulk_recipes";
constexpr const char* kRatesSection = "rates";
constexpr const char* kCookingKitsSection = "cooking_kits";
constexpr const char* kRefineSection = "refine_weapon";
constexpr const char* kWeaponLevelSection = "weapon_lv";

constexpr const char* kWeaponProduceField = "weapon_produce";
constexpr const char* kPotionProduceField = "potion_produce";
constexpr const char* kOrideconFixField = "oridecon_research_fix";

// Le séparateur d'une plage d'identifiants, « 1101-1109 ».
constexpr char kRangeDash = '-';

constexpr const char* kProductField = "id";
constexpr const char* kItemLevelField = "lv";
constexpr const char* kSkillField = "skill";
constexpr const char* kSkillLevelField = "skill_lv";
constexpr const char* kIngredientsField = "mats";
constexpr const char* kSourceField = "src";
constexpr const char* kYieldsField = "yields";
constexpr const char* kUnlearnableField = "unavailable_skills";
constexpr const char* kLeastField = "min";
constexpr const char* kMostField = "max";
constexpr const char* kModeField = "mode";
constexpr const char* kBonusField = "chance_permille";

constexpr std::string_view kPerUnitMode = "per_unit";
constexpr std::string_view kAllOrNoneMode = "all_or_none";
constexpr std::string_view kBonusRollMode = "bonus_roll";
constexpr std::string_view kTableMode = "table";

// Une paire `[id, quantité]`, et un triplet `[id, min, max]` de la production
// en masse.
constexpr std::size_t kPairFields = 2;
constexpr std::size_t kBulkFields = 3;
constexpr std::size_t kBulkLeastRank = 1;
constexpr std::size_t kBulkMostRank = 2;

// Rendu quand on interroge une clé absente : l'appelant parcourt sans garde, et
// une table vide se parcourt comme une table sans l'entrée.
template <typename Key, typename Value>
[[nodiscard]] std::span<const Value> Listed(const std::unordered_map<Key, std::vector<Value>>& map,
                                            const Key& key) {
  const auto found = map.find(key);
  return found == map.end() ? std::span<const Value>() : std::span<const Value>(found->second);
}

[[nodiscard]] bool ReadMode(std::string_view word, CraftYieldMode* mode) {
  if (word == kPerUnitMode) {
    *mode = CraftYieldMode::kPerUnit;
  } else if (word == kAllOrNoneMode) {
    *mode = CraftYieldMode::kAllOrNone;
  } else if (word == kBonusRollMode) {
    *mode = CraftYieldMode::kBonusRoll;
  } else if (word == kTableMode) {
    *mode = CraftYieldMode::kTable;
  } else {
    return false;
  }
  return true;
}

// Les paires `[id, quantité]` d'une liste, sautant celles qui sont tronquées
// ou sans identifiant.
template <typename Visit>
void ForEachPair(const YAML::Node& list, Visit visit) {
  if (!list || !list.IsSequence()) {
    return;
  }
  for (const YAML::Node& pair : list) {
    if (!pair.IsSequence() || pair.size() < kPairFields) {
      continue;
    }
    const auto item_id = pair[0].as<std::uint32_t>(0);
    if (item_id != 0) {
      visit(item_id, pair[1].as<int>(0));
    }
  }
}

}  // namespace

ro::core::Result<CraftBook> CraftBook::Parse(std::string_view yaml_text) {
  CraftBook book;
  YAML::Node root;
  try {
    root = YAML::Load(std::string(yaml_text));
  } catch (const std::exception& trouble) {
    return ro::core::Fail(ro::core::ErrorCode::kCorruptData,
                          std::string("recettes : YAML illisible — ") + trouble.what());
  }
  if (!root.IsMap()) {
    return ro::core::Fail(ro::core::ErrorCode::kCorruptData,
                          "recettes : racine qui n'est pas une table");
  }

  // ⚠ UN SEUL `try` AUTOUR DES LECTURES : `as<T>(défaut)` ne lève pas sur une
  // valeur mal typée, mais un accès sur un nœud d'une autre forme le peut. Une
  // exception à ce stade est une surprise du format, et elle se dit.
  try {
    if (const YAML::Node recipes = root[kRecipesSection]; recipes && recipes.IsSequence()) {
      for (const YAML::Node& entry : recipes) {
        if (!entry.IsMap()) {
          continue;
        }
        CraftRecipe recipe{.product_id = entry[kProductField].as<std::uint32_t>(0),
                           .item_level = entry[kItemLevelField].as<int>(0),
                           .skill_id = entry[kSkillField].as<int>(kCraftByItemSkill),
                           .skill_level = entry[kSkillLevelField].as<int>(0),
                           .ingredients = {}};
        if (recipe.product_id == 0) {
          continue;
        }
        ForEachPair(entry[kIngredientsField], [&](std::uint32_t item_id, int quantity) {
          // 🔴 QUANTITÉ ZÉRO = « À POSSÉDER, NON CONSOMMÉ », pas « aucune » :
          // le perdre ferait disparaître un matériau bel et bien exigé.
          recipe.ingredients.push_back(
              CraftIngredient{.item_id = item_id, .quantity = quantity, .kept = quantity == 0});
        });
        // ⚠ LE TEST AVANT L'AJOUT : sans quoi le rang désignerait le doublon
        // qu'on voulait ignorer.
        const auto rank = static_cast<int>(book.recipes_.size());
        if (!book.recipe_rank_.emplace(recipe.product_id, rank).second) {
          continue;
        }
        for (const CraftIngredient& ingredient : recipe.ingredients) {
          std::vector<int>& users = book.used_by_[ingredient.item_id];
          // Un matériau cité deux fois par la même recette ne la compte qu'une.
          if (users.empty() || users.back() != rank) {
            users.push_back(rank);
          }
        }
        book.recipes_.push_back(std::move(recipe));
      }
    }

    if (const YAML::Node by_skill = root[kBySkillSection]; by_skill && by_skill.IsMap()) {
      for (auto it = by_skill.begin(); it != by_skill.end(); ++it) {
        const int skill_id = it->first.as<int>(-1);
        // ⚠ ZÉRO EST LÉGITIME — la fabrication par objet. Le rejeter
        // retirerait de l'Atlas les soixante plats qu'aucun job ne fabrique.
        if (skill_id < 0) {
          continue;
        }
        // 🔴 UNE COPIE DU NŒUD, et ce n'est pas de la prudence gratuite :
        // l'itérateur de table rend sa paire à travers un temporaire, et une
        // boucle qui s'y lie directement parcourt un nœud déjà mort — la liste
        // se lit VIDE, sans un message.
        const YAML::Node products = it->second;
        if (!products.IsSequence()) {
          continue;
        }
        std::vector<std::uint32_t> listed;
        for (const YAML::Node& product : products) {
          if (const auto product_id = product.as<std::uint32_t>(0); product_id != 0) {
            listed.push_back(product_id);
          }
        }
        if (!listed.empty() &&
            book.products_by_skill_.emplace(skill_id, std::move(listed)).second) {
          book.skills_.push_back(skill_id);
        }
      }
      std::ranges::sort(book.skills_);
    }

    if (const YAML::Node server = root[kServerSection]; server && server.IsMap()) {
      if (const YAML::Node unlearnable = server[kUnlearnableField];
          unlearnable && unlearnable.IsSequence()) {
        book.knows_learnable_ = true;
        for (const YAML::Node& skill : unlearnable) {
          book.unlearnable_skills_.push_back(skill.as<int>(-1));
        }
      }
    }

    if (const YAML::Node yields = root[kYieldSection]; yields && yields.IsMap()) {
      for (auto it = yields.begin(); it != yields.end(); ++it) {
        const int skill_id = it->first.as<int>(-1);
        const YAML::Node rule_node = it->second;
        CraftYieldRule rule;
        // Un mode inconnu se TAIT plutôt que de se deviner : la recette reste
        // alors « un exemplaire », ce que le serveur fait presque partout.
        if (skill_id < 0 || !rule_node.IsMap() ||
            !ReadMode(rule_node[kModeField].as<std::string>(""), &rule.mode)) {
          continue;
        }
        rule.least = rule_node[kLeastField].as<int>(1);
        rule.most = rule_node[kMostField].as<int>(1);
        rule.bonus_permille = rule_node[kBonusField].as<int>(0);
        book.yield_rules_[skill_id] = rule;
      }
    }

    if (const YAML::Node bulk = root[kBulkSection]; bulk && bulk.IsMap()) {
      for (auto it = bulk.begin(); it != bulk.end(); ++it) {
        const int skill_id = it->first.as<int>(0);
        const YAML::Node entries = it->second;  // une copie : voir `by_skill`
        if (skill_id == 0 || !entries.IsSequence()) {
          continue;
        }
        for (const YAML::Node& entry : entries) {
          if (!entry.IsSequence() || entry.size() < kBulkFields) {
            continue;
          }
          const auto item_id = entry[0].as<std::uint32_t>(0);
          if (item_id == 0) {
            continue;
          }
          book.bulk_by_item_[item_id].push_back(
              CraftBulkSource{.skill_id = skill_id,
                              .least = entry[kBulkLeastRank].as<int>(0),
                              .most = entry[kBulkMostRank].as<int>(0)});
        }
      }
    }

    if (const YAML::Node arrows = root[kArrowsSection]; arrows && arrows.IsSequence()) {
      for (const YAML::Node& entry : arrows) {
        if (!entry.IsMap()) {
          continue;
        }
        CraftArrowRecipe arrow{.source_id = entry[kSourceField].as<std::uint32_t>(0), .yields = {}};
        ForEachPair(entry[kYieldsField], [&](std::uint32_t item_id, int quantity) {
          arrow.yields.push_back(CraftYield{.item_id = item_id, .quantity = quantity});
        });
        // Une source sans rendement ne dit rien.
        if (arrow.source_id == 0 || arrow.yields.empty()) {
          continue;
        }
        const auto rank = static_cast<int>(book.arrows_.size());
        if (!book.arrow_rank_.emplace(arrow.source_id, rank).second) {
          continue;
        }
        for (const CraftYield& yield : arrow.yields) {
          std::vector<int>& sources = book.arrows_yielding_[yield.item_id];
          if (sources.empty() || sources.back() != rank) {
            sources.push_back(rank);
          }
        }
        book.arrows_.push_back(std::move(arrow));
      }
    }
    ReadChanceTerms(root, book);
  } catch (const std::exception& trouble) {
    return ro::core::Fail(ro::core::ErrorCode::kCorruptData,
                          std::string("recettes : forme inattendue — ") + trouble.what());
  }

  if (book.recipes_.empty() && book.arrows_.empty()) {
    return ro::core::Fail(ro::core::ErrorCode::kCorruptData, "recettes : aucune recette");
  }
  return book;
}

void CraftBook::ReadChanceTerms(const YAML::Node& root, CraftBook& book) {
  if (const YAML::Node rates = root[kRatesSection]; rates && rates.IsMap()) {
    book.rates_.weapon_produce = rates[kWeaponProduceField].as<int>(kUnknownCraftRate);
    book.rates_.potion_produce = rates[kPotionProduceField].as<int>(kUnknownCraftRate);
    book.rates_.oridecon_research_fix = rates[kOrideconFixField].as<int>(0) != 0;
  }

  if (const YAML::Node kits = root[kCookingKitsSection]; kits && kits.IsMap()) {
    for (auto it = kits.begin(); it != kits.end(); ++it) {
      const auto item_id = it->first.as<std::uint32_t>(0);
      const int level = it->second.as<int>(0);
      if (item_id != 0 && level > 0) {
        book.cooking_kits_[item_id] = level;
      }
    }
  }

  if (const YAML::Node table = root[kRefineSection]; table && table.IsMap()) {
    for (auto it = table.begin(); it != table.end(); ++it) {
      const int weapon_level = it->first.as<int>(0);
      const YAML::Node row = it->second;  // une copie : voir `by_skill`
      if (weapon_level < 1 || weapon_level > kMostWeaponLevel || !row.IsMap()) {
        continue;
      }
      for (auto cell = row.begin(); cell != row.end(); ++cell) {
        const int target = cell->first.as<int>(0);
        if (target >= 1 && target <= kMostRefine) {
          book.refine_rates_[static_cast<std::size_t>(weapon_level)]
                            [static_cast<std::size_t>(target)] =
              cell->second.as<int>(kUnknownCraftRate);
        }
      }
    }
  }

  // 🔴 LES CLÉS SONT DES PLAGES « a-b » QUAND DES IDENTIFIANTS CONSÉCUTIFS ONT LE
  // MÊME NIVEAU : les armes se suivent par familles, et le générateur les
  // compresse. Une clé seule est une plage d'un élément.
  if (const YAML::Node levels = root[kWeaponLevelSection]; levels && levels.IsMap()) {
    for (auto it = levels.begin(); it != levels.end(); ++it) {
      const std::string key = it->first.as<std::string>("");
      const int level = it->second.as<int>(0);
      WeaponLevelRange range{.first = 0, .last = 0, .level = level};
      const std::size_t dash = key.find(kRangeDash);
      const std::string first = key.substr(0, dash);
      const std::string last = dash == std::string::npos ? first : key.substr(dash + 1);
      const std::errc first_error =
          std::from_chars(first.data(), first.data() + first.size(), range.first).ec;
      const std::errc last_error =
          std::from_chars(last.data(), last.data() + last.size(), range.last).ec;
      if (first_error != std::errc() || last_error != std::errc() || range.first == 0 ||
          range.last < range.first || level < 1 || level > kMostWeaponLevel) {
        continue;
      }
      book.weapon_levels_.push_back(range);
    }
    std::ranges::sort(book.weapon_levels_, {}, &WeaponLevelRange::first);
  }
}

int CraftBook::CookingKitLevel(std::uint32_t item_id) const {
  const auto found = cooking_kits_.find(item_id);
  return found == cooking_kits_.end() ? 0 : found->second;
}

int CraftBook::WeaponLevel(std::uint32_t item_id) const {
  // La dernière plage qui commence au plus à cet identifiant.
  const auto after =
      std::ranges::upper_bound(weapon_levels_, item_id, {}, &WeaponLevelRange::first);
  if (after == weapon_levels_.begin()) {
    return 0;
  }
  const WeaponLevelRange& range = *std::prev(after);
  return item_id <= range.last ? range.level : 0;
}

int CraftBook::RefineRate(int weapon_level, int target_refine) const {
  if (weapon_level < 1 || weapon_level > kMostWeaponLevel || target_refine < 1 ||
      target_refine > kMostRefine) {
    return kUnknownCraftRate;
  }
  return refine_rates_[static_cast<std::size_t>(weapon_level)]
                      [static_cast<std::size_t>(target_refine)];
}

const CraftRecipe* CraftBook::RecipeOf(std::uint32_t product_id) const {
  const auto found = recipe_rank_.find(product_id);
  return found == recipe_rank_.end() ? nullptr : &recipes_[static_cast<std::size_t>(found->second)];
}

std::span<const std::uint32_t> CraftBook::ProductsOfSkill(int skill_id) const {
  return Listed(products_by_skill_, skill_id);
}

std::span<const int> CraftBook::RecipesUsing(std::uint32_t material_id) const {
  return Listed(used_by_, material_id);
}

const CraftArrowRecipe* CraftBook::ArrowFrom(std::uint32_t source_id) const {
  const auto found = arrow_rank_.find(source_id);
  return found == arrow_rank_.end() ? nullptr : &arrows_[static_cast<std::size_t>(found->second)];
}

std::span<const int> CraftBook::ArrowsYielding(std::uint32_t arrow_id) const {
  return Listed(arrows_yielding_, arrow_id);
}

CraftYieldRule CraftBook::YieldOf(int skill_id) const {
  const auto found = yield_rules_.find(skill_id);
  return found == yield_rules_.end() ? CraftYieldRule{} : found->second;
}

std::span<const CraftBulkSource> CraftBook::BulkSourcesOf(std::uint32_t item_id) const {
  return Listed(bulk_by_item_, item_id);
}

bool CraftBook::SkillIsLearnable(int skill_id) const {
  // La fabrication par objet ne s'apprend pas : elle reste toujours ouverte.
  if (!knows_learnable_ || skill_id == kCraftByItemSkill) {
    return true;
  }
  return std::ranges::find(unlearnable_skills_, skill_id) == unlearnable_skills_.end();
}

}  // namespace moonclient
