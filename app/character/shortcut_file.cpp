#include "character/shortcut_file.h"

#include <array>
#include <cstdio>
#include <optional>
#include <string>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"

#include "game/save_folder.h"
#include "game/yaml_load.h"

namespace moonclient {
namespace {

// Les champs d'une case. ⚠ GRAVÉS : les changer vide la barre de tous les joueurs sans que rien ne
// le dise.
constexpr const char* kSkillField = "skill";
constexpr const char* kLevelField = "level";
constexpr const char* kItemField = "item";
// Les jeux de barres par métier d'un personnage, rangés sous son entrée et
// indexés par numéro de métier. ⚠ GRAVÉ comme les champs ci-dessus.
constexpr const char* kJobsField = "jobs";

// LE NOM D'UNE BARRE DANS LE FICHIER, et il compte à partir de UN.
//
// ⚠ C'EST LE NUMÉRO QUE LE JOUEUR VOIT, pas l'index du tableau. Le fichier est
// fait pour se corriger à la main : y écrire `bar0` obligerait à traduire de
// tête ce que les réglages appellent « barre 1 ».
//
// ⚠ GRAVÉ comme les champs ci-dessus.
[[nodiscard]] std::string BarKey(std::size_t bar) {
  std::array<char, 16> name{};
  std::snprintf(name.data(), name.size(), "bar%zu", bar + 1);
  return std::string(name.data());
}

constexpr const char* kFileHeader =
    "# Les barres de raccourcis de moonclient, un jeu de barres par personnage.\n"
    "#\n"
    "# Huit barres de douze cases, nommees bar1 a bar8. Une barre absente est\n"
    "# vide, et chaque liste s'arrete a sa derniere case remplie.\n"
    "#\n"
    "# Une competence s'ecrit par son NOM INTERNE -- MG_COLDBOLT -- et non par un\n"
    "# numero : le fichier se relit et se corrige a la main.\n"
    "#\n"
    "# Une case vide s'ecrit ~. Une competence inconnue des tables laisse la case\n"
    "# vide, sans effacer le reste.\n"
    "#\n"
    "# Un compte staff garde en plus un jeu par metier, sous `jobs`, indexe par\n"
    "# numero de metier : changer de metier en jeu y reprend ses barres.\n";

// Lit le fichier entier, ou rend un document vide. ⚠ IL FAUT LE TOUT pour
// écrire : la barre d'un personnage ne doit pas emporter celles des autres.
[[nodiscard]] YAML::Node ReadWhole(const std::filesystem::path& path) {
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return YAML::Node(YAML::NodeType::Map);
  }
  try {
    YAML::Node whole = LoadYamlFile(path);
    return whole.IsMap() ? whole : YAML::Node(YAML::NodeType::Map);
  } catch (const YAML::Exception& broken) {
    // La seule exception qu'on attrape, et pour la même raison qu'ailleurs : la
    // bibliothèque n'offre pas d'autre voie, et un fichier abîmé ne doit pas
    // empêcher de jouer.
    // ⚠ CE QUE CE FICHIER ABÎMÉ COÛTE VRAIMENT : `SaveShortcutBar` relit par
    // CETTE fonction avant d'écrire, et repart donc d'une carte vide. La
    // première sauvegarde suivante emporte les barres de tous les autres
    // personnages.
    ro::core::LogWarning(
        "barre de raccourcis : fichier illisible ({}) — les barres repartent vides, et le "
        "prochain enregistrement écrasera celles de TOUS les personnages",
        broken.what());
    return YAML::Node(YAML::NodeType::Map);
  }
}

// Range une LISTE de cases à partir d'un index, sans dépasser `how_many`.
//
// ⚠ LA BORNE N'EST PAS UN CONFORT : une liste trop longue écrite à la main
// déborderait sur la barre suivante, et le joueur verrait ses cases apparaître
// ailleurs sans comprendre pourquoi.
void ReadSlots(const YAML::Node& list, std::size_t first_index, std::size_t how_many,
               const ro::sim::SkillTree& tree, ro::sim::ShortcutBars& into) {
  for (std::size_t rank = 0; rank < list.size() && rank < how_many; ++rank) {
    const YAML::Node entry = list[rank];
    if (!entry || !entry.IsMap()) {
      continue;  // une case vide s'écrit `~`
    }
    const std::size_t index = first_index + rank;
    // UN OBJET SE RETIENT PAR SON NUMERO, une competence par son nom.
    //
    // ⚠ ET C'EST VOULU : le nom interne d'une competence survit aux
    // renumerotations du client, alors qu'un objet n'a pas de nom interne — le
    // sien est du texte affiche, traduit, et qui change d'une installation a
    // l'autre. Le numero est la seule chose stable qu'il ait.
    if (const YAML::Node held = entry[kItemField]; held && held.IsScalar()) {
      const auto item_id = held.as<std::uint32_t>(std::uint32_t{0});
      if (item_id != 0) {
        into.Assign(index, ro::sim::ShortcutSlot{.item_id = item_id});
      }
      continue;
    }
    const YAML::Node named = entry[kSkillField];
    if (!named || !named.IsScalar()) {
      continue;
    }
    const ro::sim::SkillNode* node = tree.FindByName(named.as<std::string>(std::string()));
    if (node == nullptr) {
      // ⚠ LA CASE RESTE VIDE PLUTÔT QUE FAUSSE. Une compétence que les tables du
      // client ne portent plus n'a ni icône ni nom : la garder par son numéro
      // donnerait une case muette qu'on ne saurait ni lire ni retirer.
      ro::core::LogWarning("barre de raccourcis : compétence inconnue « {} »",
                           named.as<std::string>(std::string()));
      continue;
    }
    const YAML::Node level = entry[kLevelField];
    into.Assign(index,
                ro::sim::ShortcutSlot{.skill_id = node->id,
                                      .level = level && level.IsScalar()
                                                   ? level.as<std::uint16_t>(std::uint16_t{1})
                                                   : std::uint16_t{1}});
  }
}

// Une case, telle qu'elle s'écrit.
[[nodiscard]] YAML::Node WriteSlot(const ro::sim::ShortcutSlot& slot,
                                   const ro::sim::SkillTree& tree) {
  if (slot.holds_item()) {
    YAML::Node entry(YAML::NodeType::Map);
    entry[kItemField] = static_cast<int>(slot.item_id);
    return entry;
  }
  const ro::sim::SkillNode* node = slot.filled() ? tree.Find(slot.skill_id) : nullptr;
  if (node == nullptr || node->idname.empty()) {
    return YAML::Node();  // `~`
  }
  YAML::Node entry(YAML::NodeType::Map);
  entry[kSkillField] = node->idname;
  entry[kLevelField] = static_cast<int>(slot.level);
  return entry;
}

// Range les `bar1`…`bar8` d'un jeu de barres.
void ReadBars(const YAML::Node& bars, const ro::sim::SkillTree& tree, ro::sim::ShortcutBars& into) {
  for (std::size_t bar = 0; bar < ro::sim::kShortcutBarCount; ++bar) {
    const YAML::Node list = bars[BarKey(bar)];
    if (!list || !list.IsSequence()) {
      continue;  // une barre absente est vide
    }
    ReadSlots(list, ro::sim::ShortcutIndex(bar, 0), ro::sim::kShortcutSlotsPerBar, tree, into);
  }
}

// Un jeu de barres, tel qu'il s'écrit.
//
// 🔴 UN JEU ENTIÈREMENT VIDE S'ÉCRIT QUAND MÊME, comme une table vide. Sans
// cela, vider sa dernière case laisserait l'ANCIENNE entrée du fichier en
// place — et la barre reviendrait au prochain démarrage.
[[nodiscard]] YAML::Node WriteBars(const ro::sim::ShortcutBars& from,
                                   const ro::sim::SkillTree& tree) {
  YAML::Node bars(YAML::NodeType::Map);
  for (std::size_t bar = 0; bar < ro::sim::kShortcutBarCount; ++bar) {
    // ⚠ JUSQU'À LA DERNIÈRE CASE REMPLIE, ET PAS PLUS LOIN. Écrire les douze
    // rendrait illisible un fichier qui est fait pour se corriger à la main.
    // Tronquer ne déplace rien : l'index d'une case ne dépend pas de ce qui la
    // suit.
    const std::size_t used = from.used_span(bar);
    if (used == 0) {
      continue;  // une barre vide ne s'écrit pas du tout
    }
    YAML::Node list(YAML::NodeType::Sequence);
    for (const ro::sim::ShortcutSlot& slot : from.BarSlots(bar).first(used)) {
      list.push_back(WriteSlot(slot, tree));
    }
    bars[BarKey(bar)] = list;
  }
  return bars;
}

}  // namespace

std::filesystem::path ShortcutFilePath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kShortcutFileName;
}

void LoadShortcutBar(const std::filesystem::path& client_root, std::string_view character,
                     const ro::sim::SkillTree* tree, ro::sim::ShortcutBars& into) {
  if (character.empty() || tree == nullptr) {
    return;
  }
  const YAML::Node whole = ReadWhole(ShortcutFilePath(client_root));
  const YAML::Node mine = whole[std::string(character)];
  if (!mine) {
    return;
  }
  if (!mine.IsMap()) {
    return;
  }
  ReadBars(mine, *tree, into);
}

bool LoadJobShortcutBar(const std::filesystem::path& client_root, std::string_view character,
                        std::uint16_t job, const ro::sim::SkillTree* tree,
                        ro::sim::ShortcutBars& into) {
  if (character.empty() || tree == nullptr) {
    return false;
  }
  const YAML::Node whole = ReadWhole(ShortcutFilePath(client_root));
  const YAML::Node mine = whole[std::string(character)];
  if (!mine || !mine.IsMap()) {
    return false;
  }
  const YAML::Node jobs = mine[kJobsField];
  if (!jobs || !jobs.IsMap()) {
    return false;
  }
  const YAML::Node bars = jobs[std::to_string(job)];
  if (!bars || !bars.IsMap()) {
    return false;
  }
  ReadBars(bars, *tree, into);
  return true;
}

bool SaveShortcutBar(const std::filesystem::path& client_root, std::string_view character,
                     const ro::sim::SkillTree* tree, const ro::sim::ShortcutBars& from,
                     std::optional<std::uint16_t> job) {
  if (character.empty() || tree == nullptr) {
    return false;
  }
  const std::filesystem::path path = ShortcutFilePath(client_root);
  // ⚠ ON RELIT POUR ÉCRIRE : les autres personnages du joueur ont leur barre
  // dans le même fichier, et n'écrire que la sienne les effacerait tous.
  YAML::Node whole = ReadWhole(path);
  YAML::Node mine = WriteBars(from, *tree);
  // ⚠ LES JEUX DES AUTRES MÉTIERS SURVIVENT à la réécriture de l'entrée : ils
  // se relisent de l'ancienne, et seul celui du métier en cours change.
  const YAML::Node before = whole[std::string(character)];
  const YAML::Node kept_jobs = before && before.IsMap() ? before[kJobsField] : YAML::Node();
  YAML::Node jobs =
      kept_jobs && kept_jobs.IsMap() ? YAML::Clone(kept_jobs) : YAML::Node(YAML::NodeType::Map);
  if (job.has_value()) {
    jobs[std::to_string(*job)] = WriteBars(from, *tree);
  }
  if (jobs.size() > 0) {
    mine[kJobsField] = jobs;
  }
  whole[std::string(character)] = mine;

  YAML::Emitter out;
  out << whole;
  // ⚠ ATOMIQUE, et il le faut d'autant plus ici : le fichier porte les barres
  // de TOUS les personnages du joueur. Une écriture tronquée ne perdrait pas
  // la dernière case posée, elle les perdrait toutes.
  const std::string text = std::string(kFileHeader) + out.c_str() + "\n";
  if (const ro::core::Result<void> written = ro::core::WriteFileAtomically(path, text); !written) {
    ro::core::LogWarning(
        "barre de raccourcis : écriture impossible ({}) — les cases posées cette partie ne "
        "seront pas retrouvées",
        ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace moonclient
