#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/core/error.h"

namespace YAML {
class Node;
}  // namespace YAML

namespace moonclient {

// ── LE LIVRE DES RECETTES : ce que le serveur sait fabriquer ────────────────
//
// Lu dans `craft_recipes.yml`, exporté des propres tables du serveur par
// `scripts/yaml_tables/craft_recipes.py` et livré avec le patch.
//
// 🔴 C'EST LA SEULE SOURCE QUI SE PARCOURT. Le serveur n'envoie une liste de
// fabrication qu'APRÈS le lancement d'un skill, et déjà filtrée sur ce que le
// personnage sait faire ; un joueur ne peut donc apprendre ni ce qu'exige une
// recette avant d'avoir le job, ni ce que deviennent les matériaux qu'il
// ramasse. La section `by_skill` et l'index INVERSE (matériau → recettes),
// bâti ici à la lecture, sont ce qui rend l'Atlas possible.
//
// Il porte aussi les termes des CHANCES — taux de fabrication, kits de cuisine,
// niveaux d'arme, table d'affinage —, que lisent les fenêtres de fabrication et
// d'affinage.
//
// ⚠ UNE ABSENCE N'EST PAS UNE PANNE : le fichier arrive avec le patch, et un
// joueur peut ne pas l'avoir encore. Le reste du client fonctionne sans lui.

// Où l'installation le range : avec les autres tables traduites, dans
// `SystemEN\` — que `moonlight.grf` porte, et qu'un dossier du même nom sur le
// disque remplace. Il se lit donc par le VFS, jamais par un chemin de fichier.
inline constexpr std::array<std::string_view, 1> kCraftBookPaths = {
    "data\\yaml_files\\craft_recipes.yml",
};

// Le « skill » d'une recette qui ne demande aucun skill : elle s'ouvre en
// UTILISANT un objet — un fourneau, un marteau, un kit de cuisine.
inline constexpr int kCraftByItemSkill = 0;

// Les `itemlv` d'un PLAT. C'est le test du serveur lui-même avant d'appliquer
// la formule de cuisine, pas une convention d'ici.
inline constexpr int kDishLeastItemLevel = 11;
inline constexpr int kDishMostItemLevel = 20;

[[nodiscard]] constexpr bool IsDishItemLevel(int item_level) noexcept {
  return item_level >= kDishLeastItemLevel && item_level <= kDishMostItemLevel;
}

// Un matériau d'une recette.
struct CraftIngredient {
  std::uint32_t item_id = 0;
  int quantity = 0;
  // 🔴 QUANTITÉ ZÉRO DANS LE FICHIER : l'objet doit être POSSÉDÉ, mais il n'est
  // PAS consommé — les guides de fabrication. Il ne borne donc pas le nombre de
  // fabrications possibles, et c'est pourquoi l'état est un drapeau plutôt
  // qu'une quantité ramenée à un, qui aurait effacé la distinction.
  bool kept = false;
};

// Ce qu'une fabrication REND. Distinct de l'ingrédient parce que le sens du
// nombre s'inverse : ici la quantité est un gain, pas une exigence.
struct CraftYield {
  std::uint32_t item_id = 0;
  int quantity = 0;
};

struct CraftRecipe {
  std::uint32_t product_id = 0;
  // `itemlv` : 1 à 3 pour une arme, 11 à 20 pour un plat, exact sinon.
  int item_level = 0;
  // `kCraftByItemSkill` pour une fabrication par objet.
  int skill_id = kCraftByItemSkill;
  int skill_level = 0;
  std::vector<CraftIngredient> ingredients;
};

// ── COMBIEN D'EXEMPLAIRES UNE FABRICATION RÉUSSIE REND ──────────────────────
//
// 🔴 UN, PRESQUE PARTOUT — mais pas partout, et rien dans les recettes ne le
// dit : le serveur recalcule lui-même la quantité pour une poignée de skills
// (runes, poisons, cuisine et pharmacie de Genetic, transmutation). Montrer une
// recette de rune sans le dire annoncerait « 1 » là où il en rend deux à six.
//
// Les bornes sont celles du SERVEUR, tous niveaux et toutes stats confondus :
// la valeur exacte dépend du personnage, que le fichier ne connaît pas.
enum class CraftYieldMode : std::uint8_t {
  // Un seul exemplaire : le cas ordinaire.
  kFixed,
  // Chaque exemplaire est tiré séparément : on peut en obtenir moins.
  kPerUnit,
  // La quantité part d'un coup si la fabrication réussit.
  kAllOrNone,
  // 🔴 PROPRE À CE SERVEUR : une fabrication DÉJÀ réussie reçoit un bonus de
  // quelques exemplaires, tiré une seule fois. Rien en jeu ne l'annonce.
  kBonusRoll,
  // Le rendement est décrit par une AUTRE table du serveur (transmutation).
  kTable,
};

struct CraftYieldRule {
  int least = 1;
  int most = 1;
  CraftYieldMode mode = CraftYieldMode::kFixed;
  // La probabilité du bonus, en POUR MILLE (162 = 16,2 %) — `kBonusRoll`
  // seulement. ⚠ EN POUR MILLE ET NON EN POUR CENT : la décimale compte, et
  // arrondie à 16 % la valeur serait fausse de plus d'un point.
  int bonus_permille = 0;

  [[nodiscard]] bool IsSingle() const noexcept {
    return mode == CraftYieldMode::kFixed && least == 1 && most == 1;
  }
};

// ── LA PRODUCTION EN MASSE ──────────────────────────────────────────────────
//
// Quelques skills rendent une fournée entière d'un seul lancement, sans liste
// et sans recette : le serveur les fabrique en dur. Les objets rendus figurent
// bien parmi les recettes, mais à l'unité, sous un autre skill — sans ceci, la
// fiche d'une White Potion dirait « un par fabrication » alors qu'un seul
// lancement en sort deux cents.
struct CraftBulkSource {
  int skill_id = 0;
  // 🔴 « JUSQU'À », PAS « EXACTEMENT » : chaque exemplaire de la fournée est
  // tiré au taux de réussite, comme le reste.
  int least = 0;
  int most = 0;
};

// ── LA FABRICATION DE FLÈCHES ───────────────────────────────────────────────
//
// ⚠ SA CLÉ EST LE MATÉRIAU CONSOMMÉ, PAS LE PRODUIT : une fabrication consomme
// UN exemplaire de la source et rend TOUT ce que porte la liste. C'est l'inverse
// de toutes les autres recettes, et c'est ainsi que le serveur range sa table.
struct CraftArrowRecipe {
  std::uint32_t source_id = 0;
  std::vector<CraftYield> yields;
};

// ── LES TERMES DES CHANCES ──────────────────────────────────────────────────
//
// 🔴 « INCONNU » N'EST PAS UNE VALEUR PLAUSIBLE. Un taux absent vaut moins un, et
// qui le lit doit alors SE TAIRE : un défaut de cent afficherait un nombre faux
// sur un serveur qui a changé son réglage.
inline constexpr int kUnknownCraftRate = -1;

// Le plus haut niveau d'arme, et le plus haut raffinement qu'on puisse viser.
inline constexpr int kMostWeaponLevel = 4;
inline constexpr int kMostRefine = 10;

// LES RÉGLAGES DU SERVEUR QUI MULTIPLIENT LES CHANCES, en pour cent. ⚠ ILS
// MULTIPLIENT : à cinq cents, les chances de forge sont quintuplées.
struct CraftRates {
  int weapon_produce = kUnknownCraftRate;
  int potion_produce = kUnknownCraftRate;
  // Le bonus d'Oridecon Research vaut-il pour les armes de niveau trois et plus ?
  bool oridecon_research_fix = false;
};

class CraftBook {
 public:
  // Lit le texte du fichier. Échoue seulement sur un YAML illisible ou sans
  // aucune recette : une section absente se lit comme vide, et une ligne mal
  // formée se saute — un fichier généré par une version plus récente doit
  // rester lisible.
  [[nodiscard]] static ro::core::Result<CraftBook> Parse(std::string_view yaml_text);

  // Toutes les recettes, dans l'ordre du fichier.
  //
  // ⚠ UN PRODUIT N'Y FIGURE QU'UNE FOIS : le serveur a de vrais doublons, et
  // c'est la PREMIÈRE entrée qui gagne — comme lui quand il cherche la recette
  // d'un produit.
  [[nodiscard]] std::span<const CraftRecipe> recipes() const noexcept { return recipes_; }
  [[nodiscard]] std::span<const CraftArrowRecipe> arrows() const noexcept { return arrows_; }
  // Combien d'items DIFFÉRENTS entrent dans au moins une recette.
  [[nodiscard]] std::size_t material_count() const noexcept { return used_by_.size(); }

  // La recette d'un produit, nul si aucune.
  [[nodiscard]] const CraftRecipe* RecipeOf(std::uint32_t product_id) const;

  // Les skills qui fabriquent quelque chose, TRIÉS.
  //
  // ⚠ `kCraftByItemSkill` EN FAIT PARTIE, et ce n'est pas une valeur manquante :
  // c'est la fabrication par objet, qu'aucun job n'ouvre.
  [[nodiscard]] std::span<const int> skills() const noexcept { return skills_; }
  // Les produits d'un skill, dans l'ordre du serveur. Vide s'il ne fabrique
  // rien.
  [[nodiscard]] std::span<const std::uint32_t> ProductsOfSkill(int skill_id) const;

  // ── L'INDEX INVERSE, que ni le client ni le serveur ne tiennent ─────────
  //
  // « Mes deux cents Jellopy servent à quoi ? ». Des INDICES dans `recipes()`
  // plutôt que des pointeurs : l'appelant les garde, les trie et les filtre
  // sans rien copier.
  [[nodiscard]] std::span<const int> RecipesUsing(std::uint32_t material_id) const;

  // La recette de flèches qui consomme cet objet, nulle si aucune.
  [[nodiscard]] const CraftArrowRecipe* ArrowFrom(std::uint32_t source_id) const;
  // Les indices, dans `arrows()`, des recettes qui PRODUISENT ces flèches —
  // la seule façon de répondre à « où trouve-t-on celles-ci ? ».
  [[nodiscard]] std::span<const int> ArrowsYielding(std::uint32_t arrow_id) const;

  // Le rendement des recettes d'un skill. `{1, 1, kFixed}` pour tout ce qui n'a
  // pas d'entrée : de très loin le cas majoritaire, et la bonne réponse.
  //
  // ⚠ LA CLÉ EST LE SKILL DE LA RECETTE, même quand le serveur le ramène à un
  // autre avant de choisir sa règle : c'est ce qui la retrouve depuis une
  // recette sans rejouer la normalisation.
  [[nodiscard]] CraftYieldRule YieldOf(int skill_id) const;

  // Les skills qui produisent cet objet en masse. Vide pour presque tout.
  [[nodiscard]] std::span<const CraftBulkSource> BulkSourcesOf(std::uint32_t item_id) const;

  // Une classe peut-elle APPRENDRE ce skill sur ce serveur ? Calculé par le
  // générateur sur l'arbre EFFECTIF, et c'est le seul critère juste pour
  // masquer une recette injouable.
  //
  // 🔴 LE MODE RENEWAL NE DIT RIEN DE CE QUI SE FABRIQUE : c'est l'arbre qui
  // décide, et un serveur pre-renewal peut très bien ouvrir des classes de
  // troisième — dont les recettes font près de la moitié du fichier.
  //
  // ⚠ VRAI QUAND L'INFORMATION MANQUE : dans le doute on MONTRE. Cacher une
  // recette réelle est une perte silencieuse ; en montrer une injouable ne
  // coûte qu'une ligne de trop.
  [[nodiscard]] bool SkillIsLearnable(int skill_id) const;

  // ── LES TERMES DES CHANCES ────────────────────────────────────────────────
  [[nodiscard]] const CraftRates& rates() const noexcept { return rates_; }

  // Le niveau d'un kit de cuisine, zéro si l'objet n'en est pas un. 🔴 IL N'EST
  // DANS AUCUN PAQUET : le serveur le garde pour lui, et seul l'objet consommé
  // le retrouve. ⚠ Un niveau hors des plats n'est pas de la cuisine.
  [[nodiscard]] int CookingKitLevel(std::uint32_t item_id) const;

  // Le niveau d'arme d'un objet, de un à quatre, zéro si inconnu. Ni la liste
  // d'affinage ni la table des objets du client ne le portent.
  [[nodiscard]] int WeaponLevel(std::uint32_t item_id) const;

  // Le taux de base d'un affinage, sur dix mille, pour ce niveau d'arme et ce
  // raffinement VISÉ. `kUnknownCraftRate` hors table.
  [[nodiscard]] int RefineRate(int weapon_level, int target_refine) const;

 private:
  // Une plage d'identifiants consécutifs de même niveau d'arme.
  struct WeaponLevelRange {
    std::uint32_t first = 0;
    std::uint32_t last = 0;
    int level = 0;
  };

  // Lit les sections des chances : taux, kits, table d'affinage, niveaux d'arme.
  static void ReadChanceTerms(const YAML::Node& root, CraftBook& book);

  using RefineRateTable = std::array<std::array<int, kMostRefine + 1>, kMostWeaponLevel + 1>;
  [[nodiscard]] static constexpr RefineRateTable UnknownRefineRates() {
    RefineRateTable table{};
    for (auto& row : table) {
      row.fill(kUnknownCraftRate);
    }
    return table;
  }

  std::vector<CraftRecipe> recipes_;
  std::unordered_map<std::uint32_t, int> recipe_rank_;
  std::unordered_map<std::uint32_t, std::vector<int>> used_by_;
  std::unordered_map<int, std::vector<std::uint32_t>> products_by_skill_;
  std::vector<int> skills_;
  std::unordered_map<int, CraftYieldRule> yield_rules_;
  std::unordered_map<std::uint32_t, std::vector<CraftBulkSource>> bulk_by_item_;
  // 🔴 DEUX ÉTATS À NE PAS CONFONDRE : « la liste est vide » — tout
  // s'apprend — et « la liste n'a pas été calculée » — le générateur n'a pas pu
  // lire l'arbre. Sans ce drapeau, un fichier qui n'a pas la clé se lirait
  // comme le premier : la bonne conclusion, pour la mauvaise raison.
  bool knows_learnable_ = false;
  std::vector<int> unlearnable_skills_;
  std::vector<CraftArrowRecipe> arrows_;
  std::unordered_map<std::uint32_t, int> arrow_rank_;
  std::unordered_map<std::uint32_t, std::vector<int>> arrows_yielding_;
  CraftRates rates_;
  std::unordered_map<std::uint32_t, int> cooking_kits_;
  // Triées par premier identifiant : la recherche est une dichotomie.
  std::vector<WeaponLevelRange> weapon_levels_;
  // [niveau d'arme][raffinement visé], sur dix mille.
  RefineRateTable refine_rates_ = UnknownRefineRates();
};

// ── LA CHANCE D'UN AFFINAGE ─────────────────────────────────────────────────
//
// 🔴 C'EST LA PROBABILITÉ EXACTE, pas une estimation. Le serveur calcule
// `per = taux / 100 + (job level - 50) / 2`, puis réussit si `per > rnd() % 100` :
// le tirage étant uniforme sur 0..99, la probabilité VAUT `per`, bornée à
// [0, 100].
//
// ⚠ Sous le job level cinquante, le bonus est NÉGATIF : le serveur fait bien cela.
// ⚠ Le bonus des troisièmes classes (+10 à plat) n'est pas pris.
inline constexpr int kRefineJobLevelPivot = 50;
inline constexpr int kRefineJobLevelDivisor = 2;
inline constexpr int kRateScalePerPercent = 100;
inline constexpr int kWholeChance = 100;

[[nodiscard]] constexpr int RefineChancePercent(int base_rate, int job_level) noexcept {
  const int per = base_rate / kRateScalePerPercent +
                  (job_level - kRefineJobLevelPivot) / kRefineJobLevelDivisor;
  return per < 0 ? 0 : (per > kWholeChance ? kWholeChance : per);
}

// ── CE QUE LE SAC PERMET ────────────────────────────────────────────────────

// Combien de fois la recette est réalisable avec ce que le sac contient.
//
// Moins un quand on ne peut pas conclure : une recette faite de guides
// seulement n'a aucun matériau consommé pour la borner.
//
// 🔴 DEUX RÈGLES OPPOSÉES POUR LA MÊME LIGNE : un guide ne borne rien, mais son
// absence interdit TOUT. Compter sa quantité afficherait « 0 » sur une recette
// parfaitement réalisable.
template <typename Owned>
[[nodiscard]] int CraftableCount(const CraftRecipe& recipe, Owned owned) {
  int best = -1;
  for (const CraftIngredient& ingredient : recipe.ingredients) {
    if (ingredient.kept) {
      if (owned(ingredient.item_id) < 1) {
        return 0;
      }
      continue;
    }
    if (ingredient.quantity <= 0) {
      continue;
    }
    const int possible = owned(ingredient.item_id) / ingredient.quantity;
    best = best < 0 ? possible : std::min(best, possible);
  }
  return best;
}

}  // namespace moonclient
