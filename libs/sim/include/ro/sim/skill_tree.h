#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ro::sim {

// L'ARBRE DES COMPÉTENCES : CE QUE LE SERVEUR N'ENVOIE JAMAIS.
//
// ⚠ C'EST LA MOITIÉ MANQUANTE DU GRIMOIRE. Le serveur n'annonce que ce que le
// personnage sait DÉJÀ — une ligne par compétence apprise. Le reste vient de
// trois tables, et chacune a son domaine :
//
//   - les RÈGLES, ce qu'un métier peut apprendre — rang maximal, prérequis —
//     viennent des bases du serveur (`data\yaml_files\skill_trees.yml`) et
//     arrivent par `UseServerRules` ;
//   - les FICHES — nom interne, nom affiché, coût, portée et zone par rang —
//     viennent de `skill_info.yml` : le serveur pour ce qu'il tient, les
//     tables de l'installation pour le reste ;
//   - la FORME — cases de la grille, onglets, chaînes d'héritage — vient de
//     `skill_layout.yml`, tirée des tables de l'installation.
//
// Le grimoire croise cet arbre et `sim::SkillBook`, l'état.
//
// ⚠ ELLE NE LIT AUCUN FICHIER. On lui remet des fiches et une disposition déjà
// lues : `sim` ne connaît pas le VFS, et l'arbre n'a pas à savoir d'où viennent
// ses octets. Qui les lit trouvera `vfs::LoadSkillInfo`, `vfs::LoadSkillLayout`
// et `vfs::SkillTreeRules`.

// Un PRÉREQUIS : la compétence réclamée et le rang qu'il en faut.
struct SkillRequirement {
  std::uint16_t skill_id = 0;
  std::uint16_t level = 0;
};

// CE QUE LE SERVEUR PERMET À UN MÉTIER d'une compétence, héritage compris.
struct SkillRule {
  std::uint16_t skill_id = 0;
  std::uint16_t max_level = 0;
  // ⚠ LA LISTE DES PRÉREQUIS N'EST PAS UNE FERMETURE TRANSITIVE, et elle porte
  // en plus des RACCOURCIS : une compétence peut réclamer A et B alors que B
  // réclame déjà A. Qui monte une chaîne doit donc la remonter lui-même, et qui
  // la dessine doit écarter l'arête redondante.
  std::vector<SkillRequirement> needs;
  std::uint32_t base_level = 0;
  std::uint32_t job_level = 0;
  // Faux quand le serveur ne l'accorde pas contre des points à qui ne l'a pas
  // déjà : compétence de quête, de noces ou d'esprit. Une fois apprise, elle
  // monte comme les autres.
  bool by_points = true;
};

// Les règles du serveur, pour tous les métiers et pour la guilde.
struct SkillRules {
  std::unordered_map<std::uint32_t, std::vector<SkillRule>> by_job;
  std::vector<SkillRule> guild;
  // Le serveur ne vérifie ni prérequis ni niveau requis : `player_skillfree`.
  bool skill_free = false;
};

// LA ZONE QU'UN SORT COUVRE, en cases, telle que le client la déclare.
//
// ⚠ PUREMENT COSMÉTIQUE. C'est ce que le client DESSINE, pas ce que le serveur
// TOUCHE : la portée réelle vient de la disposition d'unité du `skill_db`, et
// rien ne garantit que les deux coïncident. Une zone plus petite que les dégâts
// serait un mensonge tranquille ; c'est au générateur des deux bouts de les
// tenir d'accord, pas à nous.
//
// Zéro sur l'un des deux côtés veut dire « pas de zone déclarée », ce qui est
// le cas de la grande majorité des compétences.
struct SkillScaleCells {
  std::uint16_t x = 0;
  std::uint16_t y = 0;

  [[nodiscard]] constexpr bool Declared() const noexcept { return x != 0 && y != 0; }
};

struct SkillNode {
  std::uint16_t id = 0;
  // Le nom INTERNE, « SM_BASH ». C'est lui qui nomme le fichier de l'icône et
  // qui désigne la compétence dans une barre sauvegardée ; le staff le lit
  // aussi au survol.
  std::string idname;
  // Le libellé affiché, dans la langue du client.
  std::string display_name;
  // Le coût et la portée PAR RANG, ceux que le serveur applique : le rang n vaut
  // l'entrée n-1. Une compétence sans coût n'en a aucun, et une portée nulle à
  // tout rang n'en a pas non plus.
  std::vector<std::uint16_t> sp_by_level;
  std::vector<std::uint16_t> range_cells_by_level;
  // La zone PAR RANG, même convention que les deux précédentes. Vide pour qui
  // n'en déclare aucune — l'immense majorité.
  std::vector<SkillScaleCells> scale_by_level;
  // Vrai quand l'effet dépend du rang lancé — c'est ce qui autorise à lancer
  // une compétence en dessous du rang appris.
  //
  // 🔴 LE DÉFAUT N'EST PAS « FAUX ». Une fiche qui ne le dit pas le règle dès
  // qu'elle porte un coût : cent cinquante-trois compétences des classes de base sont
  // dans ce cas — Bash, Heal, Fire Bolt. La conversion a déjà tranché ; ce champ
  // porte la réponse.
  bool level_matters = false;
};

// La zone déclarée pour ce rang, ou une zone non déclarée.
//
// ⚠ RIEN N'EST INVENTÉ HORS DES BORNES. Une fiche qui ne déclare sa zone que
// pour les premiers rangs n'en a pas au-delà, et le client ne voit rien non
// plus : sa globale `GetSkillScale` lit la même case et rend zéro. Prolonger
// par la dernière entrée connue montrerait une zone là où le client n'en montre
// aucune.
[[nodiscard]] inline SkillScaleCells SkillScaleAt(const SkillNode& node,
                                                  std::uint16_t level) noexcept {
  if (level == 0 || level > node.scale_by_level.size()) {
    return {};
  }
  return node.scale_by_level[static_cast<std::size_t>(level) - 1];
}

// Une CASE de la grille d'un métier : quelle compétence, à quel rang de case.
//
// Le rang est l'index du client, à partir de zéro, sur une grille de sept
// colonnes : le rang 8 est la deuxième ligne, deuxième colonne.
struct SkillSlot {
  std::uint16_t position = 0;
  std::uint16_t skill_id = 0;
};

// La grille du client compte SEPT colonnes, et c'est elle qui décide de la
// disposition : les rangs de case viennent de ses tables.
inline constexpr int kSkillGridColumns = 7;

// LA DISPOSITION, déjà lue : les grilles, les deux chaînes d'héritage et les
// noms d'onglets.
struct SkillTreeLayout {
  // Les cases de chaque métier qui a une grille. L'ordre importe peu : l'arbre
  // les range par case.
  std::unordered_map<std::uint32_t, std::vector<SkillSlot>> slots_by_job;
  // De quel métier chaque métier descend. Le sommet de la chaîne n'y figure pas.
  std::unordered_map<std::uint32_t, std::uint32_t> parent_by_job;
  // ⚠ UNE SECONDE CHAÎNE, ET ELLE NE FAIT PAS DOUBLON. Elle dit de quel métier
  // ORDINAIRE descend un métier transcendant — Chevalier Runique transcendant
  // vers Chevalier Runique. Les treize métiers qu'elle nomme n'ont PAS de grille
  // à eux : sans elle, un Chevalier Dragon perd tout l'arbre de la troisième
  // classe, et rien ne le signale.
  std::unordered_map<std::uint32_t, std::uint32_t> origin_by_job;
  // Les noms d'onglets, par métier qui s'en est donné.
  std::unordered_map<std::uint32_t, std::vector<std::string>> tab_names_by_job;
};

// L'ONGLET où le client range la grille d'un métier de la chaîne d'héritage.
//
// ⚠ CE N'EST PAS LE RANG DANS LA CHAÎNE, qui compte jusqu'à six maillons pour
// une quatrième classe. Le client classe par PALIER : le plus ANCIEN métier de la chaîne prend le
// premier onglet, les paliers d'aura le troisième, la quatrième classe le quatrième, et tout le
// reste le deuxième — à trois exceptions près, que le client pose APRÈS son classement et qui
// contredisent ses propres intervalles.
enum class SkillTab : std::uint8_t {
  kFirst = 0,
  kSecond = 1,
  // Le palier « aura » : ce que le jeu appelle la troisième classe.
  kSpecial = 2,
  kFourth = 3,
};
inline constexpr std::size_t kSkillTabCount = 4;

// Le Novice. Son arbre rejoint celui du premier métier, dans le même onglet :
// voir `TabsOfJob`.
inline constexpr std::uint32_t kNoviceJob = 0;

// Un métier de la chaîne, et l'onglet qui lui revient.
struct SkillJobTab {
  std::uint32_t job = 0;
  SkillTab tab = SkillTab::kFirst;
};

class SkillTree {
 public:
  // Rend nullptr pour une compétence que les tables ne portent pas. Le cas est
  // NORMAL : le serveur en accorde que le client ne connaît pas.
  [[nodiscard]] const SkillNode* Find(std::uint16_t skill_id) const;

  // La même fiche, par son nom INTERNE — « SM_BASH ».
  //
  // ⚠ C'EST LA LECTURE D'UN FICHIER DE SAUVEGARDE QUI EN A BESOIN : une barre de
  // raccourcis garde des NOMS et non des numéros, pour se relire et pour ne pas
  // dépendre d'une numérotation qui pourrait bouger.
  [[nodiscard]] const SkillNode* FindByName(std::string_view idname) const;

  // Les cases d'UN métier, rangées par rang de case. Vide pour un métier sans
  // grille — ce qui est le cas de la plupart des métiers d'héritage.
  //
  // ⚠ UN MÉTIER TRANSCENDANT PREND LA GRILLE DE SA CLASSE D'ORIGINE, que la
  // seconde table d'héritage nomme. Les treize concernés n'ont pas de grille à
  // eux, et s'arrêter là leur ferait un arbre vide.
  [[nodiscard]] std::span<const SkillSlot> SlotsOfJob(std::uint32_t job) const;

  // LA CHAÎNE D'HÉRITAGE d'un métier, DU PLUS ANCIEN AU PLUS RÉCENT. C'est ce
  // que le natif remonte à chaque changement de métier : un Chevalier montre
  // l'arbre de l'Épéiste ET le sien.
  //
  // ⚠ CE N'EST PAS LA RÉPARTITION EN ONGLETS, qui ne se déduit pas de la
  // chaîne : voir `TabsOfJob`.
  [[nodiscard]] std::vector<std::uint32_t> InheritanceOfJob(std::uint32_t job) const;

  // Les métiers de la chaîne, chacun avec SON onglet, du plus ancien au plus
  // récent. C'est ce que le client monte à chaque changement de métier.
  [[nodiscard]] std::vector<SkillJobTab> TabsOfJob(std::uint32_t job) const;

  // Les noms que le client donne aux onglets d'un métier, au plus quatre.
  //
  // ⚠ CHERCHÉS SUR LE MÉTIER LUI-MÊME AVANT SES ANCÊTRES : un Super Novice
  // nomme son premier onglet « Novice » quand la chaîne, elle, remonte au Novice
  // ordinaire qui l'appelle « 1st ». Remonter d'abord donnerait le mauvais nom à
  // celui qui s'est donné la peine d'en choisir un.
  //
  // Vide quand aucun métier de la chaîne n'en nomme : c'est à l'interface de
  // poser ses propres libellés, dans sa langue.
  [[nodiscard]] std::span<const std::string> TabNamesOfJob(std::uint32_t job) const;

  [[nodiscard]] std::size_t node_count() const noexcept { return nodes_.size(); }

  // Monte l'arbre à partir de fiches et d'une disposition DÉJÀ lues. Séparé de
  // la lecture des fichiers pour que l'arbre se vérifie sans VFS ni archive.
  //
  // ⚠ LA PREMIÈRE FICHE D'UN NUMÉRO, et la première d'un nom interne,
  // l'emportent ; une grille vide n'en est pas une.
  [[nodiscard]] static SkillTree FromTables(std::vector<SkillNode> nodes, SkillTreeLayout layout);

  // ── Les règles du serveur ───────────────────────────────────────────────
  //
  // 🔴 SANS ELLES, RIEN NE SE MONTE DEPUIS LA FENÊTRE : aucun rang maximal
  // n'est connu. La grille se montre quand même, entière, et le serveur reste
  // seul juge d'une montée.
  void UseServerRules(SkillRules rules);
  [[nodiscard]] bool has_server_rules() const noexcept { return has_rules_; }
  // Faux sous `player_skillfree` : les prérequis ne bloquent rien.
  [[nodiscard]] bool prerequisites_enforced() const noexcept { return !rules_.skill_free; }

  // La règle de cette compétence pour ce métier, ou nullptr quand le serveur ne
  // la lui donne pas. ⚠ LE MÉTIER EST CELUI DU PERSONNAGE, pas celui dont la
  // grille porte la case : l'arbre du serveur contient déjà l'héritage, et un
  // métier peut y relever ou y retirer ce qu'il hérite.
  [[nodiscard]] const SkillRule* RuleOf(std::uint32_t job, std::uint16_t skill_id) const;
  // Toutes ses règles, par compétence croissante.
  [[nodiscard]] std::span<const SkillRule> RulesOfJob(std::uint32_t job) const;
  // Le rang maximal, 0 quand le serveur ne la lui donne pas.
  [[nodiscard]] std::uint16_t MaxLevelOf(std::uint32_t job, std::uint16_t skill_id) const;
  // Les prérequis que le serveur VÉRIFIE : aucun sous `player_skillfree`, même
  // quand sa base en déclare. Les montrer promettrait un verrou qui n'existe pas.
  [[nodiscard]] std::span<const SkillRequirement> NeedsOf(std::uint32_t job,
                                                          std::uint16_t skill_id) const;
  // L'arbre de la guilde, par compétence croissante. `player_skillfree` ne le
  // touche pas : le serveur y vérifie toujours les prérequis.
  [[nodiscard]] std::span<const SkillRule> GuildRules() const noexcept { return rules_.guild; }

 private:
  std::vector<SkillNode> nodes_;
  std::unordered_map<std::uint16_t, std::size_t> slot_by_skill_;
  std::unordered_map<std::string, std::size_t> slot_by_name_;
  // Les grilles rangées par case, sans grille vide.
  SkillTreeLayout layout_;
  // Les règles, chaque liste rangée par compétence.
  SkillRules rules_;
  bool has_rules_ = false;
};

}  // namespace ro::sim
