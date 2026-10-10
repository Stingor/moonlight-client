// La LECTURE de l'arbre des compétences : quatre tables du client, enchaînées,
// dont on ne garde que la forme de l'arbre.

#include "ro/sim/skill_tree.h"

#include <algorithm>
#include <array>
#include <utility>

namespace ro::sim {
namespace {

// Les champs d'une fiche, tels que `skillinfolist` les nomme.
constexpr std::string_view kFieldName = "SkillName";
constexpr std::string_view kFieldMaxLevel = "MaxLv";
constexpr std::string_view kFieldSpAmount = "SpAmount";
constexpr std::string_view kFieldAttackRange = "AttackRange";
constexpr std::string_view kFieldNeedList = "_NeedSkillList";
// ⚠ ET LA MÊME LISTE S'ÉCRIT PARFOIS SANS SON TIRET BAS : vingt-sept fiches de
// cette installation le font, dont celle de Heal. Ne chercher que la première
// orthographe rendrait leurs prérequis invisibles — aucune flèche, aucune ligne
// dans l'infobulle, et rien qui dise pourquoi.
constexpr std::string_view kFieldNeedListPlain = "NeedSkillList";
// ⚠ ORTHOGRAPHE DU CLIENT, faute comprise : « bSeperateLv ». La corriger ici,
// c'est ne plus rien trouver.
constexpr std::string_view kFieldSeparateLevel = "bSeperateLv";
// LA ZONE DESSINÉE AU SOL, par rang : `SkillScale[lv] = { x = .., y = .. }`.
// C'est la table que le client interroge par sa globale `GetSkillScale`, et la
// seule source de la taille — le paquet du serveur ne la porte pas.
constexpr std::string_view kFieldSkillScale = "SkillScale";
constexpr std::string_view kScaleX = "x";
constexpr std::string_view kScaleY = "y";

// Le nom interne est la PREMIÈRE entrée numérotée de la fiche, sans clé.
constexpr double kIdNameIndex = 1.0;
// Un prérequis est un couple { compétence, rang }.
constexpr double kNeedSkillIndex = 1.0;
constexpr double kNeedLevelIndex = 2.0;

// Une chaîne d'héritage plus longue que cela n'existe pas — la plus profonde en
// compte six. Le plafond n'est pas là pour elle mais pour un cycle dans les
// tables, qui ferait tourner la remontée sans fin.
constexpr std::size_t kLongestInheritance = 16;

[[nodiscard]] const formats::LuaTable* TableOf(const formats::LuaValue* value) {
  if (value == nullptr || value->kind != formats::LuaValue::Kind::kTable ||
      value->table == nullptr) {
    return nullptr;
  }
  return value->table.get();
}

[[nodiscard]] std::uint16_t NumberOr(const formats::LuaValue* value, std::uint16_t fallback) {
  if (value == nullptr || value->kind != formats::LuaValue::Kind::kNumber || value->number < 0.0) {
    return fallback;
  }
  return static_cast<std::uint16_t>(value->number);
}

// Une liste de nombres par rang — coût, portée. Les tables en donnent une par
// niveau maximal ; une compétence sans coût n'en a aucune.
[[nodiscard]] std::vector<std::uint16_t> NumbersByLevel(const formats::LuaValue* value) {
  std::vector<std::uint16_t> levels;
  const formats::LuaTable* table = TableOf(value);
  if (table == nullptr) {
    return levels;
  }
  levels.reserve(table->by_index.size());
  for (const auto& [rank, entry] : table->by_index) {
    levels.push_back(NumberOr(&entry, 0));
  }
  return levels;
}

// La zone par rang. ⚠ CHAQUE ENTRÉE EST UNE TABLE `{x = .., y = ..}`, et non un
// nombre : `NumbersByLevel` rendrait des zéros sans rien signaler.
//
// ⚠ ET LES RANGS SONT CREUX. Une compétence peut n'en déclarer que pour
// quelques-uns ; la table Lua est indexée, et un rang absent laisse une zone non
// déclarée plutôt que de décaler ceux qui suivent.
[[nodiscard]] std::vector<SkillScaleCells> ScaleByLevel(const formats::LuaValue* value) {
  std::vector<SkillScaleCells> levels;
  const formats::LuaTable* table = TableOf(value);
  if (table == nullptr) {
    return levels;
  }
  for (const auto& [rank, entry] : table->by_index) {
    const formats::LuaTable* cell = TableOf(&entry);
    if (cell == nullptr) {
      continue;
    }
    // Le rang est celui du client, à partir de un : la case n vaut l'entrée
    // n-1, et les trous restent non déclarés.
    if (rank >= 1) {
      levels.resize(std::max<std::size_t>(levels.size(), static_cast<std::size_t>(rank)));
      levels[static_cast<std::size_t>(rank) - 1] = SkillScaleCells{
          .x = NumberOr(cell->Find(kScaleX), 0), .y = NumberOr(cell->Find(kScaleY), 0)};
    }
  }
  return levels;
}

[[nodiscard]] std::vector<SkillRequirement> NeedsOf(const formats::LuaValue* value) {
  std::vector<SkillRequirement> needs;
  const formats::LuaTable* table = TableOf(value);
  if (table == nullptr) {
    return needs;
  }
  for (const auto& [rank, entry] : table->by_index) {
    const formats::LuaTable* pair = TableOf(&entry);
    if (pair == nullptr) {
      continue;
    }
    const std::uint16_t skill_id = NumberOr(pair->Find(kNeedSkillIndex), 0);
    if (skill_id == 0) {
      continue;
    }
    needs.push_back(
        SkillRequirement{.skill_id = skill_id, .level = NumberOr(pair->Find(kNeedLevelIndex), 1)});
  }
  return needs;
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
  const auto found = slots_by_job_.find(job);
  if (found != slots_by_job_.end()) {
    return found->second;
  }
  // ⚠ UN MÉTIER TRANSCENDANT N'A PAS DE GRILLE À LUI : il prend celle de sa
  // classe d'origine. Sans ce report, un Chevalier Dragon perd tout l'arbre du
  // Chevalier Runique — sa chaîne passe par le transcendant, qui n'a rien.
  const auto origin = origin_by_job_.find(job);
  if (origin == origin_by_job_.end()) {
    return {};
  }
  const auto borrowed = slots_by_job_.find(origin->second);
  return borrowed == slots_by_job_.end() ? std::span<const SkillSlot>{} : borrowed->second;
}

std::vector<std::uint32_t> SkillTree::InheritanceOfJob(std::uint32_t job) const {
  std::vector<std::uint32_t> chain;
  std::uint32_t current = job;
  while (chain.size() < kLongestInheritance) {
    chain.push_back(current);
    const auto parent = parent_by_job_.find(current);
    if (parent == parent_by_job_.end() || parent->second == current) {
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
    const auto found = tab_names_by_job_.find(*step);
    if (found != tab_names_by_job_.end()) {
      return found->second;
    }
  }
  return {};
}

core::Result<SkillTree> SkillTree::FromGlobals(const formats::LuaTable& globals) {
  const formats::LuaTable* infos = TableOf(globals.Find(kSkillInfoListGlobal));
  if (infos == nullptr) {
    return std::unexpected(core::Error{core::ErrorCode::kCorruptData,
                                       std::string(kSkillInfoListGlobal) + " est absente"});
  }

  SkillTree tree;
  tree.nodes_.reserve(infos->by_index.size());
  for (const auto& [skill_id, entry] : infos->by_index) {
    const formats::LuaTable* skill_table = TableOf(&entry);
    if (skill_table == nullptr || skill_id <= 0.0) {
      continue;
    }
    const formats::LuaValue* idname = skill_table->Find(kIdNameIndex);
    SkillNode node;
    node.id = static_cast<std::uint16_t>(skill_id);
    if (idname != nullptr && idname->kind == formats::LuaValue::Kind::kString) {
      node.idname = idname->text;
    }
    const formats::LuaValue* display = skill_table->Find(kFieldName);
    if (display != nullptr && display->kind == formats::LuaValue::Kind::kString) {
      node.display_name = display->text;
    }
    node.max_level = NumberOr(skill_table->Find(kFieldMaxLevel), 0);
    node.sp_by_level = NumbersByLevel(skill_table->Find(kFieldSpAmount));
    node.range_cells_by_level = NumbersByLevel(skill_table->Find(kFieldAttackRange));
    node.scale_by_level = ScaleByLevel(skill_table->Find(kFieldSkillScale));
    const formats::LuaValue* needs = skill_table->Find(kFieldNeedList);
    node.needs = NeedsOf(needs != nullptr ? needs : skill_table->Find(kFieldNeedListPlain));
    // 🔴 LE DÉFAUT N'EST PAS « FAUX », ET C'EST LE CLIENT QUI LE DIT. Sa fonction
    // `IsLevelUseSkill` vit dans `skillinfo_f.lub` ; son bytecode répond en TROIS
    // temps, pas en deux :
    //
    //   pas de fiche          → faux ;
    //   `bSeperateLv` présent → sa valeur ;
    //   `bSeperateLv` ABSENT  → VRAI si la fiche porte `SpAmount`, faux sinon.
    //
    // 🔴 ET C'EST LE TROISIÈME CAS QUI DOMINE. Dans cette installation, cent
    // cinquante-trois compétences des classes de base n'ont pas le champ — Bash,
    // Heal, Fire Bolt, Soul Strike, Cold Bolt. Les lire « faux » retirerait le
    // réglage du rang de lancement à presque tout ce qu'un joueur lance : la
    // molette n'aurait plus rien à régler, et le menu plus sa section.
    //
    // ⚠ `sp_by_level` EST LE TÉMOIN DE `SpAmount`, et il vient d'être rempli
    // juste au-dessus : une fiche sans coût par rang est une passive, et une
    // passive ne se dose pas.
    const formats::LuaValue* separate = skill_table->Find(kFieldSeparateLevel);
    const bool told = separate != nullptr && separate->kind == formats::LuaValue::Kind::kBoolean;
    node.level_matters = told ? separate->boolean : !node.sp_by_level.empty();

    tree.slot_by_skill_.emplace(node.id, tree.nodes_.size());
    if (!node.idname.empty()) {
      tree.slot_by_name_.emplace(node.idname, tree.nodes_.size());
    }
    tree.nodes_.push_back(std::move(node));
  }

  // ── Les grilles ──────────────────────────────────────────────────────────
  // ⚠ ABSENTE N'EST PAS UNE ERREUR ICI : un serveur peut ne livrer que les
  // fiches. Le grimoire retombe alors sur une grille vide par métier, ce qui
  // reste lisible — contrairement à des fiches manquantes, qui laisseraient des
  // cases sans nom ni niveau maximal.
  const formats::LuaTable* grids = TableOf(globals.Find(kSkillTreeViewGlobal));
  if (grids != nullptr) {
    for (const auto& [job, entry] : grids->by_index) {
      const formats::LuaTable* grid = TableOf(&entry);
      if (grid == nullptr || job < 0.0) {
        continue;
      }
      std::vector<SkillSlot> slots;
      slots.reserve(grid->by_index.size());
      for (const auto& [position, skill] : grid->by_index) {
        const std::uint16_t skill_id = NumberOr(&skill, 0);
        if (skill_id == 0 || position < 0.0) {
          continue;
        }
        slots.push_back(
            SkillSlot{.position = static_cast<std::uint16_t>(position), .skill_id = skill_id});
      }
      if (slots.empty()) {
        continue;
      }
      // Rangées par case : les tables les donnent presque toujours dans l'ordre,
      // mais « presque » ne suffit pas quand c'est l'ordre du dessin.
      std::sort(slots.begin(), slots.end(),
                [](const SkillSlot& a, const SkillSlot& b) { return a.position < b.position; });
      tree.slots_by_job_.emplace(static_cast<std::uint32_t>(job), std::move(slots));
    }
  }

  // ── L'héritage ───────────────────────────────────────────────────────────
  const auto read_links = [&globals](std::string_view global,
                                     std::unordered_map<std::uint32_t, std::uint32_t>& into) {
    const formats::LuaTable* table = TableOf(globals.Find(global));
    if (table == nullptr) {
      return;
    }
    for (const auto& [job, parent] : table->by_index) {
      if (job < 0.0 || parent.kind != formats::LuaValue::Kind::kNumber || parent.number < 0.0) {
        continue;
      }
      into.insert_or_assign(static_cast<std::uint32_t>(job),
                            static_cast<std::uint32_t>(parent.number));
    }
  };
  read_links(kJobInheritListGlobal, tree.parent_by_job_);
  read_links(kJobOriginListGlobal, tree.origin_by_job_);

  // ── Les noms d'onglets ───────────────────────────────────────────────────
  // Ils ne sont dans aucune table : le fichier de grilles se termine par dix
  // APPELS qui les posent, un par métier qui s'en donne.
  for (const formats::LuaCall& call : globals.calls) {
    if (call.name != kTabNameCall || call.arguments.size() < 2) {
      continue;
    }
    const formats::LuaValue& named_job = call.arguments.front();
    if (named_job.kind != formats::LuaValue::Kind::kNumber || named_job.number < 0.0) {
      continue;
    }
    std::vector<std::string> names;
    names.reserve(call.arguments.size() - 1);
    for (std::size_t rank = 1; rank < call.arguments.size(); ++rank) {
      if (call.arguments[rank].kind == formats::LuaValue::Kind::kString) {
        names.push_back(call.arguments[rank].text);
      }
    }
    if (!names.empty()) {
      tree.tab_names_by_job_.insert_or_assign(static_cast<std::uint32_t>(named_job.number),
                                              std::move(names));
    }
  }
  return tree;
}

}  // namespace ro::sim
