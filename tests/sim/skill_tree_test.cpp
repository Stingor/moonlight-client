// L'arbre des compétences, monté de fiches et d'une disposition écrites ici
// même : aucune archive, aucun asset. Les identifiants sont ceux du jeu.

#include "ro/sim/skill_tree.h"

#include <unordered_map>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr std::uint16_t kSwordMastery = 2;    // SM_SWORD
constexpr std::uint16_t kTwoHandMastery = 3;  // SM_TWOHAND
constexpr std::uint16_t kBash = 5;            // SM_BASH
constexpr std::uint16_t kMagnumBreak = 7;     // SM_MAGNUM
constexpr std::uint32_t kNovice = 0;
constexpr std::uint32_t kSwordman = 1;
constexpr std::uint32_t kKnight = 7;

[[nodiscard]] std::vector<ro::sim::SkillNode> Sheets() {
  return {
      ro::sim::SkillNode{
          .id = kSwordMastery, .idname = "SM_SWORD", .display_name = "Sword Mastery"},
      ro::sim::SkillNode{.id = kTwoHandMastery,
                         .idname = "SM_TWOHAND",
                         .display_name = "Two-Handed Sword Mastery"},
      ro::sim::SkillNode{.id = kBash,
                         .idname = "SM_BASH",
                         .display_name = "Bash",
                         .sp_by_level = {8, 8, 8, 8, 8, 15, 15, 15, 15, 15},
                         .range_cells_by_level = std::vector<std::uint16_t>(10, 1),
                         .level_matters = true},
      ro::sim::SkillNode{.id = kMagnumBreak,
                         .idname = "SM_MAGNUM",
                         .display_name = "Magnum Break",
                         .sp_by_level = std::vector<std::uint16_t>(10, 30),
                         .scale_by_level = {{.x = 5, .y = 5}, {.x = 5, .y = 5}, {.x = 7, .y = 7}}},
  };
}

// La grille de l'Épéiste, DANS LE DÉSORDRE : l'arbre la range par case.
[[nodiscard]] ro::sim::SkillTreeLayout Layout() {
  ro::sim::SkillTreeLayout layout;
  layout.slots_by_job[kSwordman] = {{.position = 10, .skill_id = kMagnumBreak},
                                    {.position = 1, .skill_id = kSwordMastery},
                                    {.position = 8, .skill_id = kTwoHandMastery},
                                    {.position = 3, .skill_id = kBash}};
  // Une grille vide n'en est pas une.
  layout.slots_by_job[kKnight] = {};
  layout.parent_by_job = {{kSwordman, kNovice}, {kKnight, kSwordman}};
  return layout;
}

[[nodiscard]] ro::sim::SkillTree Tree() {
  return ro::sim::SkillTree::FromTables(Sheets(), Layout());
}

[[nodiscard]] ro::sim::SkillTree ChainOnly(
    std::unordered_map<std::uint32_t, std::uint32_t> parent_by_job) {
  ro::sim::SkillTreeLayout layout;
  layout.parent_by_job = std::move(parent_by_job);
  return ro::sim::SkillTree::FromTables({}, std::move(layout));
}

}  // namespace

TEST_CASE("l'arbre garde la fiche de chaque compétence", "[sim][grimoire]") {
  const ro::sim::SkillTree tree = Tree();
  CHECK(tree.node_count() == 4);

  const ro::sim::SkillNode* bash = tree.Find(kBash);
  REQUIRE(bash != nullptr);
  // Le nom INTERNE nomme l'icône : le confondre avec le libellé affiché
  // chercherait « Bash.bmp ».
  CHECK(bash->idname == "SM_BASH");
  CHECK(bash->display_name == "Bash");
  // Le coût est donné PAR RANG, et il change au milieu de l'échelle.
  REQUIRE(bash->sp_by_level.size() == 10);
  CHECK(bash->sp_by_level[0] == 8);
  CHECK(bash->sp_by_level[9] == 15);
  CHECK(bash->level_matters);
  CHECK(tree.FindByName("SM_BASH") == bash);
  CHECK(tree.FindByName("Bash") == nullptr);
}

TEST_CASE("la première fiche d'un numéro ou d'un nom l'emporte", "[sim][grimoire]") {
  std::vector<ro::sim::SkillNode> sheets = Sheets();
  sheets.push_back(ro::sim::SkillNode{.id = kBash, .idname = "SM_BASH2", .display_name = "Autre"});
  sheets.push_back(ro::sim::SkillNode{.id = 9000, .idname = "SM_SWORD"});
  const ro::sim::SkillTree tree = ro::sim::SkillTree::FromTables(std::move(sheets), {});
  REQUIRE(tree.Find(kBash) != nullptr);
  CHECK(tree.Find(kBash)->display_name == "Bash");
  REQUIRE(tree.FindByName("SM_SWORD") != nullptr);
  CHECK(tree.FindByName("SM_SWORD")->id == kSwordMastery);
}

TEST_CASE("le rang maximal et les prérequis viennent du serveur, par métier", "[sim][grimoire]") {
  ro::sim::SkillTree tree = Tree();
  // 🔴 LA FICHE NE DIT RIEN DE CE QUI S'APPREND : sans les règles du serveur,
  // aucun rang maximal ni prérequis.
  CHECK_FALSE(tree.has_server_rules());
  CHECK(tree.MaxLevelOf(kSwordman, kBash) == 0);

  ro::sim::SkillRules rules;
  // Dans le désordre : l'arbre les range.
  rules.by_job[kSwordman] = {
      ro::sim::SkillRule{
          .skill_id = kMagnumBreak, .max_level = 10, .needs = {{.skill_id = kBash, .level = 5}}},
      ro::sim::SkillRule{.skill_id = kBash, .max_level = 10}};
  // Le Chevalier relève Bash à 5 et retire le prérequis : c'est SON arbre.
  rules.by_job[kKnight] = {ro::sim::SkillRule{.skill_id = kBash, .max_level = 5},
                           ro::sim::SkillRule{.skill_id = kMagnumBreak, .max_level = 10}};
  tree.UseServerRules(std::move(rules));

  CHECK(tree.has_server_rules());
  CHECK(tree.prerequisites_enforced());
  CHECK(tree.MaxLevelOf(kSwordman, kBash) == 10);
  CHECK(tree.MaxLevelOf(kKnight, kBash) == 5);
  REQUIRE(tree.NeedsOf(kSwordman, kMagnumBreak).size() == 1);
  CHECK(tree.NeedsOf(kSwordman, kMagnumBreak)[0].level == 5);
  CHECK(tree.NeedsOf(kKnight, kMagnumBreak).empty());
  // Une compétence que le serveur ne donne pas au métier n'a pas de règle.
  CHECK(tree.RuleOf(kNovice, kBash) == nullptr);
  CHECK(tree.RuleOf(kSwordman, kSwordMastery) == nullptr);
  REQUIRE(tree.RulesOfJob(kSwordman).size() == 2);
  CHECK(tree.RulesOfJob(kSwordman)[0].skill_id == kBash);
}

TEST_CASE("sous player_skillfree, aucun prérequis ne verrouille", "[sim][grimoire]") {
  // Le serveur n'en vérifie aucun : les payer d'office dépenserait des points
  // pour rien. La guilde, elle, garde les siens.
  ro::sim::SkillTree tree = Tree();
  constexpr std::uint16_t kApproval = 10000;
  constexpr std::uint16_t kKafra = 10001;
  ro::sim::SkillRules rules{.skill_free = true};
  rules.by_job[kSwordman] = {ro::sim::SkillRule{
      .skill_id = kMagnumBreak, .max_level = 10, .needs = {{.skill_id = kBash, .level = 5}}}};
  rules.guild = {
      ro::sim::SkillRule{
          .skill_id = kKafra, .max_level = 1, .needs = {{.skill_id = kApproval, .level = 1}}},
      ro::sim::SkillRule{.skill_id = kApproval, .max_level = 1}};
  tree.UseServerRules(std::move(rules));

  CHECK_FALSE(tree.prerequisites_enforced());
  CHECK(tree.NeedsOf(kSwordman, kMagnumBreak).empty());
  // La forme de l'arbre, elle, reste : c'est elle que tracent les flèches.
  REQUIRE(tree.DeclaredNeedsOf(kSwordman, kMagnumBreak).size() == 1);
  CHECK(tree.DeclaredNeedsOf(kSwordman, kMagnumBreak)[0].skill_id == kBash);
  CHECK(tree.MaxLevelOf(kSwordman, kMagnumBreak) == 10);
  REQUIRE(tree.GuildRules().size() == 2);
  CHECK(tree.GuildRules()[0].skill_id == kApproval);
  CHECK(tree.GuildRules()[1].needs.size() == 1);
}

TEST_CASE("la zone d'un sort se lit par rang, et manque le plus souvent", "[sim][grimoire]") {
  const ro::sim::SkillTree tree = Tree();
  const ro::sim::SkillNode* magnum = tree.Find(kMagnumBreak);
  REQUIRE(magnum != nullptr);
  REQUIRE(magnum->scale_by_level.size() == 3);
  // Elle CHANGE avec le rang : la lire au premier annoncerait 5×5 pour un rang
  // qui couvre 7×7.
  CHECK(ro::sim::SkillScaleAt(*magnum, 1).x == 5);
  CHECK(ro::sim::SkillScaleAt(*magnum, 3).x == 7);
  CHECK(ro::sim::SkillScaleAt(*magnum, 3).y == 7);
  CHECK(ro::sim::SkillScaleAt(*magnum, 3).Declared());

  // ⚠ LE RANG ZÉRO N'EXISTE PAS, et hors des bornes RIEN N'EST INVENTÉ : une
  // fiche qui ne déclare sa zone que pour trois rangs n'en a pas au quatrième.
  CHECK_FALSE(ro::sim::SkillScaleAt(*magnum, 0).Declared());
  CHECK_FALSE(ro::sim::SkillScaleAt(*magnum, 4).Declared());
  CHECK_FALSE(ro::sim::SkillScaleAt(*magnum, 10).Declared());

  // Et le cas ORDINAIRE est l'absence : presque aucune compétence n'en déclare.
  const ro::sim::SkillNode* bash = tree.Find(kBash);
  REQUIRE(bash != nullptr);
  CHECK(bash->scale_by_level.empty());
  CHECK_FALSE(ro::sim::SkillScaleAt(*bash, 1).Declared());
}

TEST_CASE("une compétence absente des tables ne se trouve pas", "[sim][grimoire]") {
  // Le cas est NORMAL : le serveur accorde des compétences que le client ne
  // connaît pas, et le grimoire doit pouvoir le dire sans se briser.
  CHECK(Tree().Find(9999) == nullptr);
}

TEST_CASE("la grille d'un métier est rangée par case", "[sim][grimoire]") {
  const ro::sim::SkillTree tree = Tree();
  const std::span<const ro::sim::SkillSlot> slots = tree.SlotsOfJob(kSwordman);
  REQUIRE(slots.size() == 4);
  CHECK(slots[0].position == 1);
  CHECK(slots[0].skill_id == kSwordMastery);
  // Le rang 8 est la DEUXIÈME ligne sur une grille de sept colonnes : c'est lui
  // qui place la maîtrise des épées à deux mains sous celle des épées.
  CHECK(slots[2].position == 8);
  CHECK(slots[2].skill_id == kTwoHandMastery);
  CHECK(slots[2].position / ro::sim::kSkillGridColumns == 1);
  CHECK(slots[3].position == 10);

  // Un métier sans grille n'est pas une erreur : la plupart n'en ont aucune.
  CHECK(tree.SlotsOfJob(kKnight).empty());
}

TEST_CASE("la chaîne d'héritage remonte jusqu'au premier métier", "[sim][grimoire]") {
  const ro::sim::SkillTree tree = Tree();
  // DU PLUS ANCIEN AU PLUS RÉCENT : un Chevalier montre l'arbre de l'Épéiste
  // avant le sien, comme le client.
  const std::vector<std::uint32_t> chain = tree.InheritanceOfJob(kKnight);
  REQUIRE(chain.size() == 3);
  CHECK(chain[0] == kNovice);
  CHECK(chain[1] == kSwordman);
  CHECK(chain[2] == kKnight);

  // Le sommet de la chaîne ne remonte à personne.
  const std::vector<std::uint32_t> alone = tree.InheritanceOfJob(kNovice);
  REQUIRE(alone.size() == 1);
  CHECK(alone[0] == kNovice);
}

TEST_CASE("un héritage circulaire s'arrête au lieu de tourner", "[sim][grimoire]") {
  // Une table retouchée peut fermer un cycle : sans garde-fou, la remontée ne
  // s'arrêterait qu'au plafond de profondeur.
  CHECK(ChainOnly({{1, 2}, {2, 1}}).InheritanceOfJob(1).size() == 2);
}

TEST_CASE("les métiers de la chaîne se rangent en onglets", "[sim][grimoire]") {
  // ⚠ LE CLASSEMENT EST CELUI DU CLIENT, PAS UNE DÉDUCTION. Le plus
  // ancien MÉTIER prend le premier onglet ; un palier d'aura prend le troisième,
  // une quatrième classe le quatrième, et tout le reste le deuxième.
  const ro::sim::SkillTree tree =
      ChainOnly({{1, 0}, {7, 1}, {4008, 7}, {4060, 4008}, {4252, 4060}});

  // Novice, Épéiste, Chevalier, Seigneur Chevalier, Chevalier Runique,
  // Chevalier Dragon : six maillons pour quatre onglets, ce qui montre bien que
  // le rang dans la chaîne ne peut pas décider.
  const std::vector<ro::sim::SkillJobTab> tabs = tree.TabsOfJob(4252);
  REQUIRE(tabs.size() == 6);
  // ⚠ LE NOVICE ET L'ÉPÉISTE SONT DANS LE MÊME ONGLET : leurs deux grilles
  // cohabitent — le Novice occupe la colonne zéro, l'Épéiste les six suivantes.
  CHECK(tabs[0].job == 0);
  CHECK(tabs[0].tab == ro::sim::SkillTab::kFirst);
  CHECK(tabs[1].tab == ro::sim::SkillTab::kFirst);    // Épéiste
  CHECK(tabs[2].tab == ro::sim::SkillTab::kSecond);   // Chevalier
  CHECK(tabs[3].tab == ro::sim::SkillTab::kSecond);   // Seigneur Chevalier
  CHECK(tabs[4].tab == ro::sim::SkillTab::kSpecial);  // Chevalier Runique
  CHECK(tabs[5].tab == ro::sim::SkillTab::kFourth);   // Chevalier Dragon
}

TEST_CASE("les exceptions d'onglet contredisent leur propre intervalle", "[sim][grimoire]") {
  // ⚠ TROIS MÉTIERS TOMBENT DANS L'INTERVALLE DE QUATRIÈME CLASSE ET N'Y VONT
  // PAS. Le client pose ces exceptions APRÈS son classement.
  const ro::sim::SkillTree tree = ChainOnly({{4302, 1}, {4304, 1}, {4308, 1}});
  // L'Empereur du ciel reste en quatrième classe.
  CHECK(tree.TabsOfJob(4302).back().tab == ro::sim::SkillTab::kFourth);
  // Shinkiro est dans le même intervalle et passe au palier d'aura.
  CHECK(tree.TabsOfJob(4304).back().tab == ro::sim::SkillTab::kSpecial);
  // Spirit Handler aussi, et il retombe au deuxième onglet.
  CHECK(tree.TabsOfJob(4308).back().tab == ro::sim::SkillTab::kSecond);
}

TEST_CASE("le Doram ouvre le premier onglet où qu'il soit", "[sim][grimoire]") {
  // Il n'a pas de classe d'avant : le client lui donne le premier onglet sans
  // regarder sa place dans la chaîne.
  const std::vector<ro::sim::SkillJobTab> tabs = ChainOnly({{4218, 1}}).TabsOfJob(4218);
  REQUIRE(tabs.size() == 2);
  CHECK(tabs[1].tab == ro::sim::SkillTab::kFirst);
}

TEST_CASE("les noms d'onglets se cherchent sur le métier avant ses ancêtres", "[sim][grimoire]") {
  ro::sim::SkillTreeLayout layout;
  layout.parent_by_job = {{1, 0}, {23, 0}};
  layout.tab_names_by_job[0] = {"1st", "2nd", "3rd", "4th"};
  layout.tab_names_by_job[23] = {"Novice", "2nd", "3rd"};
  const ro::sim::SkillTree tree = ro::sim::SkillTree::FromTables({}, std::move(layout));

  // L'Épéiste n'en nomme aucun : il prend ceux de son ancêtre.
  const std::span<const std::string> inherited = tree.TabNamesOfJob(1);
  REQUIRE(inherited.size() == 4);
  CHECK(inherited[0] == "1st");

  // ⚠ LE MÉTIER LUI-MÊME L'EMPORTE SUR SES ANCÊTRES. Le Super Novice nomme son
  // premier onglet « Novice » ; remonter d'abord lui donnerait « 1st ».
  const std::span<const std::string> own = tree.TabNamesOfJob(23);
  REQUIRE(own.size() == 3);
  CHECK(own[0] == "Novice");

  // Un métier dont personne dans la chaîne ne nomme les onglets n'en a pas :
  // c'est à l'interface de poser les siens, dans sa langue.
  CHECK(tree.TabNamesOfJob(9999).empty());
}

TEST_CASE("un métier transcendant prend la grille de sa classe d'origine", "[sim][grimoire]") {
  // ⚠ SANS CELA, UN CHEVALIER DRAGON PERD TOUT L'ARBRE DE LA TROISIÈME CLASSE.
  // Sa chaîne passe par le Chevalier Runique TRANSCENDANT, qui n'a pas de grille
  // à lui : c'est la seconde chaîne qui dit de quel métier ordinaire il descend.
  ro::sim::SkillTreeLayout layout;
  layout.slots_by_job[4054] = {{.position = 0, .skill_id = 1001}};
  layout.parent_by_job = {{4252, 4060}};
  layout.origin_by_job = {{4060, 4054}};
  const ro::sim::SkillTree tree = ro::sim::SkillTree::FromTables(
      {ro::sim::SkillNode{.id = 1001, .idname = "RK_SONICWAVE", .display_name = "Sonic Wave"}},
      std::move(layout));

  // Le transcendant n'a rien en propre, et rend pourtant la grille de l'autre.
  const std::span<const ro::sim::SkillSlot> borrowed = tree.SlotsOfJob(4060);
  REQUIRE(borrowed.size() == 1);
  CHECK(borrowed[0].skill_id == 1001);

  // Un métier sans grille ET sans origine n'en a toujours pas : le report ne
  // doit rien inventer.
  CHECK(tree.SlotsOfJob(4252).empty());
}
