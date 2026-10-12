// L'arbre des compétences : ses fiches, sa disposition et les règles du serveur,
// déjà lues, et le classement des métiers en onglets.

#include "ro/sim/skill_tree.h"

#include <algorithm>
#include <array>
#include <utility>

namespace ro::sim {
namespace {

// Une chaîne d'héritage plus longue que cela n'existe pas — la plus profonde en
// compte six. Le plafond n'est pas là pour elle mais pour un cycle dans les
// tables, qui ferait tourner la remontée sans fin.
constexpr std::size_t kLongestInheritance = 16;

// La règle d'une compétence dans une liste rangée par compétence, ou nullptr.
[[nodiscard]] const SkillRule* RuleIn(std::span<const SkillRule> rules, std::uint16_t skill_id) {
  const auto found = std::ranges::lower_bound(rules, skill_id, {}, &SkillRule::skill_id);
  return found == rules.end() || found->skill_id != skill_id ? nullptr : &*found;
}

void SortBySkill(std::vector<SkillRule>& rules) {
  std::ranges::sort(rules, {}, &SkillRule::skill_id);
}

// ── Le classement des métiers en onglets ────────────────────────────────────
//
// ⚠ TOUT CE QUI SUIT EST LE CLASSEMENT DU CLIENT, PAS UNE DÉDUCTION : deux
// prédicats — le palier d'aura et la quatrième classe —, faits d'intervalles
// et de listes de métiers, puis des exceptions.

// Le Doram prend le premier onglet où qu'il soit dans la chaîne : il n'a pas de
// classe d'avant.
constexpr std::uint32_t kSummonerJob = 4218;      // JT_DO_SUMMONER
constexpr std::uint32_t kBabySummonerJob = 4220;  // JT_DO_SUMMONER_B

// Le palier d'aura — ce que le jeu appelle la troisième classe. Un intervalle,
// et une liste de métiers que l'intervalle ne rattrape pas.
constexpr std::uint32_t kAuraFirstJob = 4054;  // JT_RUNE_KNIGHT
constexpr std::uint32_t kAuraLastJob = 4112;   // JT_MADOGEAR_B
constexpr std::array<std::uint32_t, 26> kAuraJobs = {
    4120, 4127, 4129, 4132, 4133, 4140, 4143, 4148, 4151, 4173, 4174, 4175, 4176,
    4177, 4178, 4179, 4180, 4182, 4197, 4198, 4201, 4202, 4205, 4206, 4208, 4209};
// Quatre métiers que le client range au palier d'aura sans passer par le
// prédicat : Empereurs et Faucheurs, ordinaires et bébés.
constexpr std::uint32_t kStarEmperorJob = 4239;
constexpr std::uint32_t kSoulReaperJob = 4240;
constexpr std::uint32_t kBabyStarEmperorJob = 4241;
constexpr std::uint32_t kBabySoulReaperJob = 4242;

// La quatrième classe : deux intervalles.
constexpr std::uint32_t kFourthFirstJob = 4252;  // JT_DRAGON_KNIGHT
constexpr std::uint32_t kFourthLastJob = 4299;
constexpr std::uint32_t kLateFourthFirstJob = 4302;  // JT_SKY_EMPEROR
constexpr std::uint32_t kLateFourthLastJob = 4329;

// ⚠ LES EXCEPTIONS SE POSENT APRÈS LE CLASSEMENT, et trois d'entre elles
// CONTREDISENT l'intervalle de quatrième classe où elles tombent pourtant. Les
// traiter avant donnerait le bon onglet à deux d'entre elles et le mauvais aux
// quatre autres.
constexpr std::uint32_t kSkyEmperorJob = 4302;
constexpr std::uint32_t kSoulAsceticJob = 4303;
constexpr std::uint32_t kAuraOverrideFirstJob = 4304;  // JT_SHINKIRO
constexpr std::uint32_t kAuraOverrideLastJob = 4307;   // JT_HYPER_NOVICE
constexpr std::uint32_t kSpiritHandlerJob = 4308;

[[nodiscard]] bool InAuraSet(std::uint32_t job) {
  if (job >= kAuraFirstJob && job <= kAuraLastJob) {
    return true;
  }
  return std::find(kAuraJobs.begin(), kAuraJobs.end(), job) != kAuraJobs.end();
}

[[nodiscard]] bool IsFourthClass(std::uint32_t job) {
  return (job >= kFourthFirstJob && job <= kFourthLastJob) ||
         (job >= kLateFourthFirstJob && job <= kLateFourthLastJob);
}

// `oldest` : ce métier est le plus ancien de la chaîne, celui qui ouvre l'arbre.
[[nodiscard]] SkillTab TabOfJob(std::uint32_t job, bool oldest) {
  SkillTab tab = SkillTab::kSecond;
  if (job == 0 || oldest || job == kSummonerJob || job == kBabySummonerJob) {
    tab = SkillTab::kFirst;
  } else if (InAuraSet(job) || job == kStarEmperorJob || job == kSoulReaperJob ||
             job == kBabyStarEmperorJob || job == kBabySoulReaperJob) {
    tab = SkillTab::kSpecial;
  } else if (IsFourthClass(job)) {
    tab = SkillTab::kFourth;
  }

  if (job == kSkyEmperorJob || job == kSoulAsceticJob) {
    return SkillTab::kFourth;
  }
  if (job >= kAuraOverrideFirstJob && job <= kAuraOverrideLastJob) {
    return SkillTab::kSpecial;
  }
  if (job == kSpiritHandlerJob) {
    return SkillTab::kSecond;
  }
  return tab;
}

}  // namespace

const SkillNode* SkillTree::Find(std::uint16_t skill_id) const {
  const auto found = slot_by_skill_.find(skill_id);
  return found == slot_by_skill_.end() ? nullptr : &nodes_[found->second];
}

const SkillNode* SkillTree::FindByName(std::string_view idname) const {
  const auto found = slot_by_name_.find(std::string(idname));
  return found == slot_by_name_.end() ? nullptr : &nodes_[found->second];
}

std::span<const SkillSlot> SkillTree::SlotsOfJob(std::uint32_t job) const {
  const auto found = layout_.slots_by_job.find(job);
  if (found != layout_.slots_by_job.end()) {
    return found->second;
  }
  // ⚠ UN MÉTIER TRANSCENDANT N'A PAS DE GRILLE À LUI : il prend celle de sa
  // classe d'origine. Sans ce report, un Chevalier Dragon perd tout l'arbre du
  // Chevalier Runique — sa chaîne passe par le transcendant, qui n'a rien.
  const auto origin = layout_.origin_by_job.find(job);
  if (origin == layout_.origin_by_job.end()) {
    return {};
  }
  const auto borrowed = layout_.slots_by_job.find(origin->second);
  return borrowed == layout_.slots_by_job.end() ? std::span<const SkillSlot>{} : borrowed->second;
}

std::vector<std::uint32_t> SkillTree::InheritanceOfJob(std::uint32_t job) const {
  std::vector<std::uint32_t> chain;
  std::uint32_t current = job;
  while (chain.size() < kLongestInheritance) {
    chain.push_back(current);
    const auto parent = layout_.parent_by_job.find(current);
    if (parent == layout_.parent_by_job.end() || parent->second == current) {
      break;
    }
    // Un métier déjà vu ferme un cycle : les tables en portent un — le Spirit
    // Handler est déclaré DEUX FOIS, avec deux parents différents — et remonter
    // sans ce garde-fou ne s'arrêterait qu'au plafond.
    if (std::find(chain.begin(), chain.end(), parent->second) != chain.end()) {
      break;
    }
    current = parent->second;
  }
  // Du plus ancien au plus récent : c'est l'ordre de lecture de l'arbre, et
  // celui des onglets.
  std::reverse(chain.begin(), chain.end());
  return chain;
}

std::vector<SkillJobTab> SkillTree::TabsOfJob(std::uint32_t job) const {
  const std::vector<std::uint32_t> chain = InheritanceOfJob(job);
  // ⚠ LE NOVICE N'EST PAS UN MAILLON, et c'est ce qui décide de tout le reste.
  // La table que le client monte pour classer les onglets — `JobInheritTable` —
  // ne le contient pas : son arbre rejoint celui du
  // PREMIER MÉTIER, dans le même onglet. Les deux grilles sont faites pour
  // cohabiter — le Novice occupe la colonne zéro, l'Épéiste les six suivantes.
  //
  // Le compter comme un maillon lui donnerait le premier onglet et pousserait le
  // premier métier au second, où il écraserait la deuxième classe case pour
  // case.
  std::size_t oldest = 0;
  while (oldest + 1 < chain.size() && chain[oldest] == kNoviceJob) {
    ++oldest;
  }
  std::vector<SkillJobTab> tabs;
  tabs.reserve(chain.size());
  for (std::size_t rank = 0; rank < chain.size(); ++rank) {
    tabs.push_back(SkillJobTab{.job = chain[rank], .tab = TabOfJob(chain[rank], rank == oldest)});
  }
  return tabs;
}

std::span<const std::string> SkillTree::TabNamesOfJob(std::uint32_t job) const {
  const std::vector<std::uint32_t> chain = InheritanceOfJob(job);
  // Du plus RÉCENT au plus ancien : le métier qui s'est donné des noms l'emporte
  // sur ses ancêtres.
  for (auto step = chain.rbegin(); step != chain.rend(); ++step) {
    const auto found = layout_.tab_names_by_job.find(*step);
    if (found != layout_.tab_names_by_job.end()) {
      return found->second;
    }
  }
  return {};
}

void SkillTree::UseServerRules(SkillRules rules) {
  for (auto& [job, job_rules] : rules.by_job) {
    SortBySkill(job_rules);
  }
  SortBySkill(rules.guild);
  rules_ = std::move(rules);
  has_rules_ = true;
}

std::span<const SkillRule> SkillTree::RulesOfJob(std::uint32_t job) const {
  const auto found = rules_.by_job.find(job);
  return found == rules_.by_job.end() ? std::span<const SkillRule>{} : found->second;
}

const SkillRule* SkillTree::RuleOf(std::uint32_t job, std::uint16_t skill_id) const {
  return RuleIn(RulesOfJob(job), skill_id);
}

std::uint16_t SkillTree::MaxLevelOf(std::uint32_t job, std::uint16_t skill_id) const {
  const SkillRule* rule = RuleOf(job, skill_id);
  return rule != nullptr ? rule->max_level : 0;
}

std::span<const SkillRequirement> SkillTree::NeedsOf(std::uint32_t job,
                                                     std::uint16_t skill_id) const {
  const SkillRule* rule = RuleOf(job, skill_id);
  if (rule == nullptr || !prerequisites_enforced()) {
    return {};
  }
  return rule->needs;
}

SkillTree SkillTree::FromTables(std::vector<SkillNode> nodes, SkillTreeLayout layout) {
  SkillTree tree;
  tree.nodes_ = std::move(nodes);
  for (std::size_t slot = 0; slot < tree.nodes_.size(); ++slot) {
    const SkillNode& node = tree.nodes_[slot];
    tree.slot_by_skill_.emplace(node.id, slot);
    if (!node.idname.empty()) {
      tree.slot_by_name_.emplace(node.idname, slot);
    }
  }
  // Rangées par case : c'est l'ordre du dessin, et une table retouchée à la main
  // peut ne pas le suivre.
  std::erase_if(layout.slots_by_job, [](const auto& grid) { return grid.second.empty(); });
  for (auto& [job, slots] : layout.slots_by_job) {
    std::ranges::sort(slots, {}, &SkillSlot::position);
  }
  tree.layout_ = std::move(layout);
  return tree;
}

}  // namespace ro::sim
