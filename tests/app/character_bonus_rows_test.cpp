// La table des apports : ce qu'elle garde, dans quel ordre, et sous quelle forme.
//
// 🔴 CE QUI SE VÉRIFIE ICI NE SE VOIT PAS EN JEU. Une rangée qui manque ne se
// remarque que si l'on portait justement la pièce qui la donne ; deux rangées
// interverties ne se remarquent que si les deux valent quelque chose EN MÊME
// TEMPS. Sur un banc, les quarante-cinq valent quelque chose d'un coup.

#include "character/character_bonus_rows.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/player_bonuses.h"
#include "ro/sim/skill_tree.h"
#include "ro/ui/character_window.h"

#include "support/monster_words_fixture.h"

using moonclient::BuildBonusRows;
using moonclient::BuildConditionalRows;
using moonclient::BuildItemRows;
using moonclient::BuildSkillRows;
using ro::sim::PlayerBonuses;
using ro::test_support::EnglishMonsterWords;
using ro::test_support::FrenchMonsterWords;
using ro::ui::CharacterBonusRow;

TEST_CASE("Apports : rien tant que le serveur n'a rien dit", "[app][bonus]") {
  std::vector<CharacterBonusRow> rows;
  PlayerBonuses silent;
  silent.melee_pct = 12;         // un relevé qu'on n'a pas reçu ne se montre pas,
  BuildBonusRows(silent, rows);  // même s'il traîne des valeurs.
  CHECK(rows.empty());

  // ⚠ ET UN PERSONNAGE NU N'EST PAS LA MÊME CHOSE : le serveur a parlé, il n'a
  // rien à dire. Les deux se taisent à l'écran, mais pour des raisons qu'il
  // faut pouvoir distinguer.
  PlayerBonuses naked;
  naked.told = true;
  BuildBonusRows(naked, rows);
  CHECK(rows.empty());
}

TEST_CASE("Apports : seul le non-nul passe, et dans l'ordre de la liste", "[app][bonus]") {
  PlayerBonuses bonuses;
  bonuses.told = true;
  // Trois apports pris dans trois lots différents du serveur, pour que l'ordre
  // ait quelque chose à prouver.
  bonuses.crit_dmg_pct = 30;  // le troisième de la liste
  bonuses.hp_add = 500;       // le quatrième
  bonuses.magic_sp_gain = 7;  // le dernier

  std::vector<CharacterBonusRow> rows;
  BuildBonusRows(bonuses, rows);

  REQUIRE(rows.size() == 3);
  CHECK(rows[0].value == 30);
  CHECK(rows[1].value == 500);
  CHECK(rows[2].value == 7);

  // ⚠ LE POUR CENT NE SE DEVINE PAS DE LA VALEUR : « 30 » et « 500 » se
  // ressemblent, et le serveur mélange les deux sortes dans la même trame.
  CHECK(rows[0].unit == ro::ui::BonusUnit::kPercent);
  CHECK(rows[1].unit == ro::ui::BonusUnit::kFlat);
  CHECK(rows[2].unit == ro::ui::BonusUnit::kFlat);

  // Chaque rangée porte de quoi s'écrire ET de quoi s'expliquer.
  for (const CharacterBonusRow& row : rows) {
    CHECK_FALSE(row.label.empty());
    CHECK_FALSE(row.tooltip.empty());
  }
}

TEST_CASE("Apports : un apport négatif se garde", "[app][bonus]") {
  PlayerBonuses bonuses;
  bonuses.told = true;
  // 🔴 UNE RÉDUCTION DE TEMPS D'INCANTATION EST NÉGATIVE, et c'est le bonus le
  // plus recherché du jeu. Filtrer « ce qui est positif » l'aurait fait
  // disparaître de la fiche de tous ceux qui en portent.
  bonuses.vcast_pct = -20;

  std::vector<CharacterBonusRow> rows;
  BuildBonusRows(bonuses, rows);
  REQUIRE(rows.size() == 1);
  CHECK(rows[0].value == -20);
  CHECK(rows[0].unit == ro::ui::BonusUnit::kPercent);
}

TEST_CASE("Apports : les quarante-cinq ont chacun leur rangée", "[app][bonus]") {
  PlayerBonuses bonuses;
  bonuses.told = true;
  // Tout à un : c'est le seul relevé où une rangée oubliée se compte.
  bonuses.melee_pct = 1;
  bonuses.ranged_pct = 1;
  bonuses.crit_dmg_pct = 1;
  bonuses.hp_add = 1;
  bonuses.sp_add = 1;
  bonuses.aspd_add = 1;
  bonuses.vcast_pct = 1;
  bonuses.fcast_pct = 1;
  bonuses.atk_pct = 1;
  bonuses.matk_pct = 1;
  bonuses.dmg_ret_melee = 1;
  bonuses.dmg_ret_ranged = 1;
  bonuses.dmg_ret_magic = 1;
  bonuses.double_pct = 1;
  bonuses.perfect_hit = 1;
  bonuses.hp_pct = 1;
  bonuses.sp_pct = 1;
  bonuses.hp_regen_pct = 1;
  bonuses.sp_regen_pct = 1;
  bonuses.crit_def_pct = 1;
  bonuses.hp_on_kill = 1;
  bonuses.sp_on_kill = 1;
  bonuses.unbreak_pct = 1;
  bonuses.pot_hp_pct = 1;
  bonuses.pot_sp_pct = 1;
  bonuses.heal_up_pct = 1;
  bonuses.delay_pct = 1;
  bonuses.add_vcast_ms = 1;
  bonuses.add_fcast_ms = 1;
  bonuses.steal_pct = 1;
  bonuses.def_melee_pct = 1;
  bonuses.def_ranged_pct = 1;
  bonuses.def_magic_pct = 1;
  bonuses.def_misc_pct = 1;
  bonuses.splash = 1;
  bonuses.splash_add = 1;
  bonuses.hp_drain_pct = 1;
  bonuses.sp_drain_pct = 1;
  bonuses.break_weapon_pct = 1;
  bonuses.break_armor_pct = 1;
  bonuses.zeny_bonus_pct = 1;
  bonuses.classchange_pct = 1;
  bonuses.dmg_ret_reduce = 1;
  bonuses.magic_hp_gain = 1;
  bonuses.magic_sp_gain = 1;

  std::vector<CharacterBonusRow> rows;
  BuildBonusRows(bonuses, rows);
  CHECK(rows.size() == 45);

  // ⚠ ET AUCUNE RANGÉE EN DOUBLE : deux rangées du même nom et de la même unité
  // sont ce que donne un copier-coller, et à l'écran elles se lisent comme une
  // seule répétée. Un même nom en deux unités se distingue par sa valeur —
  // « PV max +300 » et « PV max +5% » — : le libellé ne répète pas l'unité.
  for (std::size_t first = 0; first < rows.size(); ++first) {
    INFO(rows[first].label);
    CHECK(rows[first].label.find('%') == std::string_view::npos);
    for (std::size_t second = first + 1; second < rows.size(); ++second) {
      CHECK((rows[first].label != rows[second].label || rows[first].unit != rows[second].unit));
    }
  }

  // ⚠ LES QUATRE PARTS N'Y SONT PAS, et c'est délibéré : `eatk`, `ematk` et les
  // deux du raffinage se liront DANS les rangées ATK et DEF. Les compter ici
  // ferait quarante-neuf et les montrerait deux fois.
  bonuses.eatk = 1;
  bonuses.ematk = 1;
  bonuses.refine_atk = 1;
  bonuses.refine_def = 1;
  BuildBonusRows(bonuses, rows);
  CHECK(rows.size() == 45);
}

// ── CE QUI NE VAUT QUE CONTRE QUELQU'UN ─────────────────────────────────────

namespace {

// Une entrée du relevé, telle que le serveur l'émet.
ro::sim::ConditionalBonus Against(ro::sim::ConditionalBonusKind kind, std::int16_t index,
                                  std::int32_t value) {
  return ro::sim::ConditionalBonus{
      .code = static_cast<std::uint16_t>(kind), .index = index, .value = value};
}

}  // namespace

TEST_CASE("Conditionnels : le même index ne dit pas la même chose sous deux codes",
          "[app][bonus]") {
  // 🔴 C'EST LA RÈGLE DE TOUTE LA SECTION. L'index 2 vaut « Earth » sous une
  // résistance élémentaire et « Brute » sous une résistance de race : lire
  // l'index sans son code donnerait des phrases fausses mais plausibles, que
  // rien à l'écran ne trahirait.
  PlayerBonuses bonuses;
  bonuses.told = true;
  bonuses.conditional = {
      Against(ro::sim::ConditionalBonusKind::kSubElement, 2, 10),
      Against(ro::sim::ConditionalBonusKind::kSubRace, 2, 20),
  };

  std::vector<std::string> labels;
  std::vector<CharacterBonusRow> rows;
  BuildConditionalRows(bonuses, EnglishMonsterWords(), labels, rows);

  REQUIRE(rows.size() == 2);
  CHECK(rows[0].label == "Résist. vs Earth");
  CHECK(rows[1].label == "Résist. vs Brute");
  CHECK(rows[0].value == 10);
  // ⚠ TOUJOURS UN POUR CENT : les trente-deux codes sont des magnitudes
  // relatives, et la forme ne se devine pas de la valeur.
  CHECK(rows[1].unit == ro::ui::BonusUnit::kPercent);
}

TEST_CASE("Conditionnels : race, élément et taille se disent avec les mots de la table",
          "[app][bonus]") {
  // 🔴 AUCUN MOT ANGLAIS EN DUR : la même fiche, sous une table française, écrit
  // les mots français ; sous une table qui ne porte pas la clé, elle n'écrit rien.
  PlayerBonuses bonuses;
  bonuses.told = true;
  bonuses.conditional = {
      Against(ro::sim::ConditionalBonusKind::kSubRace, 1, 10),
      Against(ro::sim::ConditionalBonusKind::kSubElement, 3, 10),
      Against(ro::sim::ConditionalBonusKind::kAddSize, 2, 10),
  };

  std::vector<std::string> labels;
  std::vector<CharacterBonusRow> rows;
  BuildConditionalRows(bonuses, FrenchMonsterWords(), labels, rows);
  REQUIRE(rows.size() == 3);
  CHECK(rows[0].label == "Résist. vs Mort-vivant");
  CHECK(rows[1].label == "Résist. vs Feu");
  CHECK(rows[2].label == "Dégâts vs Grand");

  const ro::vfs::ItemWords none;
  BuildConditionalRows(bonuses, none, labels, rows);
  REQUIRE(rows.size() == 3);
  CHECK(rows[0].label == "Résist. vs ");
}

TEST_CASE("Conditionnels : joueur, Doram et jokers sont des textes à nous", "[app][bonus]") {
  // Le client d'origine n'écrit rien pour ces indices : ils ne dépendent pas de
  // la table de messages.
  PlayerBonuses bonuses;
  bonuses.told = true;
  bonuses.conditional = {
      Against(ro::sim::ConditionalBonusKind::kSubRace, 10, 10),
      Against(ro::sim::ConditionalBonusKind::kSubRace, 11, 10),
      Against(ro::sim::ConditionalBonusKind::kSubRace, 12, 10),
      Against(ro::sim::ConditionalBonusKind::kSubElement, 10, 10),
      Against(ro::sim::ConditionalBonusKind::kAddSize, 3, 10),
  };

  std::vector<std::string> labels;
  std::vector<CharacterBonusRow> rows;
  const ro::vfs::ItemWords none;
  BuildConditionalRows(bonuses, none, labels, rows);
  REQUIRE(rows.size() == 5);
  CHECK(rows[0].label == "Résist. vs Player");
  CHECK(rows[1].label == "Résist. vs Doram Player");
  CHECK(rows[2].label == "Résist. vs all races");
  CHECK(rows[3].label == "Résist. vs all elements");
  CHECK(rows[4].label == "Dégâts vs all sizes");
}

TEST_CASE("Conditionnels : un verbe par code, et la bonne famille de cibles", "[app][bonus]") {
  PlayerBonuses bonuses;
  bonuses.told = true;
  bonuses.conditional = {
      Against(ro::sim::ConditionalBonusKind::kAddSize, 1, 15),
      Against(ro::sim::ConditionalBonusKind::kDropClass, 1, 5),
      Against(ro::sim::ConditionalBonusKind::kSubRaceGroup, 3, 25),
      Against(ro::sim::ConditionalBonusKind::kMagicAddElement, 3, 30),
  };

  std::vector<std::string> labels;
  std::vector<CharacterBonusRow> rows;
  BuildConditionalRows(bonuses, EnglishMonsterWords(), labels, rows);

  REQUIRE(rows.size() == 4);
  CHECK(rows[0].label == "Dégâts vs Medium");
  // ⚠ « BOSS » EST UNE CLASSE DE MONSTRE, PAS UNE RACE : le serveur les range
  // dans deux familles distinctes, et un boss a par ailleurs sa race.
  CHECK(rows[1].label == "Drop vs Boss");
  CHECK(rows[2].label == "Réduc. vs Orc");
  CHECK(rows[3].label == "Dég. mag. vs Fire");
}

TEST_CASE("Conditionnels : un groupe sans nom se nomme par son numéro", "[app][bonus]") {
  PlayerBonuses bonuses;
  bonuses.told = true;
  // ⚠ LA TABLE DES GROUPES EST INCOMPLÈTE ET LE RESTERA : le serveur en ajoute
  // au fil de ses épisodes. Le numéro, lui, permet de le retrouver dans `mob_db`.
  bonuses.conditional = {Against(ro::sim::ConditionalBonusKind::kSubRaceGroup, 99, 7)};

  std::vector<std::string> labels;
  std::vector<CharacterBonusRow> rows;
  BuildConditionalRows(bonuses, EnglishMonsterWords(), labels, rows);

  REQUIRE(rows.size() == 1);
  CHECK(rows[0].label == "Réduc. vs groupe #99");
}

TEST_CASE("Conditionnels : un code inconnu se montre avec son numéro", "[app][bonus]") {
  PlayerBonuses bonuses;
  bonuses.told = true;
  // 🔴 LE TRENTE-TROISIÈME CODE ARRIVERA. Le taire ferait disparaître un bonus
  // que le joueur porte vraiment ; « Bonus ? » ne dirait pas lequel.
  constexpr std::uint16_t kCodeFromTheFuture = 999;
  bonuses.conditional = {
      ro::sim::ConditionalBonus{.code = kCodeFromTheFuture, .index = 4, .value = 12}};

  std::vector<std::string> labels;
  std::vector<CharacterBonusRow> rows;
  BuildConditionalRows(bonuses, EnglishMonsterWords(), labels, rows);

  REQUIRE(rows.size() == 1);
  CHECK(rows[0].label == "Bonus #999");
  CHECK(rows[0].value == 12);
}

TEST_CASE("Conditionnels : les libellés vivent aussi longtemps que les rangées", "[app][bonus]") {
  PlayerBonuses bonuses;
  bonuses.told = true;
  // ⚠ ASSEZ D'ENTRÉES POUR QUE LE TABLEAU S'AGRANDISSE s'il n'était pas réservé :
  // une chaîne courte se range DANS son propre objet, et un déplacement en cours
  // de route laisserait les premières vues pointer dans le vide.
  for (std::int16_t index = 0; index < 13; ++index) {
    bonuses.conditional.push_back(
        Against(ro::sim::ConditionalBonusKind::kSubRace, index, index + 1));
  }

  std::vector<std::string> labels;
  std::vector<CharacterBonusRow> rows;
  BuildConditionalRows(bonuses, EnglishMonsterWords(), labels, rows);

  REQUIRE(rows.size() == labels.size());
  for (std::size_t rank = 0; rank < rows.size(); ++rank) {
    CHECK(rows[rank].label == labels[rank]);
    CHECK_FALSE(rows[rank].label.empty());
  }
  CHECK(rows.front().label == "Résist. vs Formless");
  CHECK(rows.back().label == "Résist. vs all races");
}

TEST_CASE("Conditionnels : rien tant que le serveur n'a rien dit", "[app][bonus]") {
  PlayerBonuses silent;
  silent.conditional = {Against(ro::sim::ConditionalBonusKind::kSubRace, 1, 10)};

  std::vector<std::string> labels;
  std::vector<CharacterBonusRow> rows;
  BuildConditionalRows(silent, EnglishMonsterWords(), labels, rows);
  CHECK(rows.empty());
  CHECK(labels.empty());
}

// ── CE QUI TIENT À UNE COMPÉTENCE, OU À UN OBJET ────────────────────────────

namespace {

ro::sim::SkillBonus Holds(ro::sim::SkillBonusKind kind, std::uint16_t skill_id, std::int32_t value,
                          std::int16_t level = 0, std::uint16_t trigger = 0) {
  return ro::sim::SkillBonus{.code = static_cast<std::uint16_t>(kind),
                             .skill_id = skill_id,
                             .level = level,
                             .value = value,
                             .triggered_by_skill = trigger};
}

// Deux compétences nommées. ⚠ AUCUNE ARCHIVE : les fiches sont écrites ici même.
ro::sim::SkillTree TwoSkills() {
  return ro::sim::SkillTree::FromTables(
      {ro::sim::SkillNode{.id = 5, .idname = "SM_BASH", .display_name = "Bash"},
       ro::sim::SkillNode{.id = 7, .idname = "SM_MAGNUM", .display_name = "Magnum Break"}},
      {});
}

}  // namespace

TEST_CASE("Skills : le taux d'un auto-sort se compte par MILLE", "[app][bonus]") {
  PlayerBonuses bonuses;
  bonuses.told = true;
  // 🔴 CINQ UNITÉS DANS LA MÊME TRAME, et rien dans la valeur ne dit laquelle :
  // « 125 » vaut ici douze et demi pour cent, et cent vingt-cinq pour cent deux
  // rangées plus bas.
  bonuses.skills = {
      Holds(ro::sim::SkillBonusKind::kAutoSpell, 5, 125, 3),
      Holds(ro::sim::SkillBonusKind::kSkillAttack, 5, 125),
      Holds(ro::sim::SkillBonusKind::kSkillCooldownMs, 5, -500),
      Holds(ro::sim::SkillBonusKind::kSkillSpCost, 5, -8),
  };

  const ro::sim::SkillTree tree = TwoSkills();
  std::vector<std::string> labels;
  std::vector<CharacterBonusRow> rows;
  BuildSkillRows(bonuses, &tree, nullptr, labels, rows);

  REQUIRE(rows.size() == 4);
  CHECK(rows[0].label == "Autocast Bash Niv 3");
  CHECK(rows[0].unit == ro::ui::BonusUnit::kPerMille);
  CHECK(rows[1].label == "Dégâts Bash");
  CHECK(rows[1].unit == ro::ui::BonusUnit::kPercent);
  // ⚠ LE MÊME MOT, UNE AUTRE UNITÉ : le serveur a deux champs pour le coût en
  // SP, l'un en pour cent et l'autre en points.
  CHECK(rows[2].unit == ro::ui::BonusUnit::kMilliseconds);
  CHECK(rows[3].label == "Coût SP Bash");
  CHECK(rows[3].unit == ro::ui::BonusUnit::kFlat);
}

TEST_CASE("Skills : un auto-sort sur déclencheur nomme les deux compétences", "[app][bonus]") {
  PlayerBonuses bonuses;
  bonuses.told = true;
  bonuses.skills = {Holds(ro::sim::SkillBonusKind::kAutoSpellOnSkill, 5, 100, 2, 7)};

  const ro::sim::SkillTree tree = TwoSkills();

  std::vector<std::string> labels;
  std::vector<CharacterBonusRow> rows;
  BuildSkillRows(bonuses, &tree, nullptr, labels, rows);
  REQUIRE(rows.size() == 1);
  CHECK(rows[0].label == "Autocast Bash Niv 2 sur Magnum Break");
}

TEST_CASE("Skills : sans catalogue, une compétence se nomme par son numéro", "[app][bonus]") {
  PlayerBonuses bonuses;
  bonuses.told = true;
  bonuses.skills = {Holds(ro::sim::SkillBonusKind::kSubSkill, 42, -15)};

  std::vector<std::string> labels;
  std::vector<CharacterBonusRow> rows;
  // ⚠ UNE INSTALLATION SANS TABLES NE FAIT PAS PLANTER LA FICHE : elle la fait
  // parler en numéros, qui se retrouvent dans les tables.
  BuildSkillRows(bonuses, nullptr, nullptr, labels, rows);
  REQUIRE(rows.size() == 1);
  CHECK(rows[0].label == "Réduc. skill #42");
  CHECK(rows[0].value == -15);
}

TEST_CASE("Skills : un statut voyage dans le champ d'une compétence", "[app][bonus]") {
  PlayerBonuses bonuses;
  bonuses.told = true;
  // 🔴 LE SERVEUR N'A PAS DE SECOND CHAMP : sous ces trois codes, `skill_id`
  // porte un EFST. Le passer au catalogue des compétences donnerait le nom d'une
  // compétence qui n'a rien à voir — et une phrase fausse mais plausible.
  const ro::sim::SkillTree tree = TwoSkills();
  bonuses.skills = {Holds(ro::sim::SkillBonusKind::kAddEffect, 5, 250),
                    Holds(ro::sim::SkillBonusKind::kResistEffect, 5, 1000)};

  std::vector<std::string> labels;
  std::vector<CharacterBonusRow> rows;
  BuildSkillRows(bonuses, &tree, nullptr, labels, rows);
  REQUIRE(rows.size() == 2);
  CHECK(rows[0].label == "Inflige statut #5");
  CHECK(rows[1].label == "Résist. statut #5");
  CHECK(rows[0].unit == ro::ui::BonusUnit::kCentiPercent);
}

TEST_CASE("Objets : un groupe ne se nomme pas par la table des objets", "[app][bonus]") {
  PlayerBonuses bonuses;
  bonuses.told = true;
  bonuses.items = {
      ro::sim::ItemBonus{.code = static_cast<std::uint16_t>(ro::sim::ItemBonusKind::kAddDrop),
                         .item_id = 501,
                         .rate = 250},
      ro::sim::ItemBonus{.code = static_cast<std::uint16_t>(ro::sim::ItemBonusKind::kAddDropGroup),
                         .item_id = 3,
                         .rate = 100},
  };

  std::vector<std::string> labels;
  std::vector<CharacterBonusRow> rows;
  BuildItemRows(
      bonuses,
      [](std::uint32_t item_id) {
        // ⚠ LE NOMMEUR NE DOIT PAS ÊTRE APPELÉ POUR UN GROUPE : le serveur
        // réemploie le champ, et « objet numéro 3 » n'a rien à voir avec le
        // groupe numéro 3.
        CHECK(item_id == 501);
        return std::string("Red Potion");
      },
      labels, rows);

  REQUIRE(rows.size() == 2);
  CHECK(rows[0].label == "Drop Red Potion");
  CHECK(rows[1].label == "Drop groupe #3");
  // ⚠ DE UN À DIX MILLE : le taux se compte au centième de pour cent.
  CHECK(rows[0].unit == ro::ui::BonusUnit::kCentiPercent);
  CHECK(rows[0].value == 250);
}

TEST_CASE("Skills : un statut nommé prend son nom, et rien d'autre", "[app][bonus]") {
  PlayerBonuses bonuses;
  bonuses.told = true;
  bonuses.skills = {Holds(ro::sim::SkillBonusKind::kAddEffect, 2, 250),
                    Holds(ro::sim::SkillBonusKind::kAddEffect, 199, 100)};

  std::vector<std::string> labels;
  std::vector<CharacterBonusRow> rows;
  // ⚠ LA TABLE DES ÉTATS EST INCOMPLÈTE, ET C'EST MESURÉ : elle nomme huit
  // cent deux statuts là où le serveur en déclare mille quatre cent soixante et
  // onze. Le numéro reste donc le repli, et il se retrouve dans l'énumération
  // `efst_type` du serveur — « ? » ne se retrouverait nulle part.
  BuildSkillRows(
      bonuses, nullptr,
      [](std::uint32_t effect_id) {
        return effect_id == 2 ? std::string_view("Stone Curse") : std::string_view();
      },
      labels, rows);

  REQUIRE(rows.size() == 2);
  CHECK(rows[0].label == "Inflige Stone Curse");
  CHECK(rows[1].label == "Inflige statut #199");
}
