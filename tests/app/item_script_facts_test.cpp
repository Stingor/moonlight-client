// LE LECTEUR DE SCRIPTS D'OBJET : ce qu'il tire des motifs exacts, et ce qu'il
// tait dès qu'une famille paraît sous une autre forme. Les scripts sont ceux de
// `dist/data/yaml_files/item_db.yml`.

#include "game/item_script_facts.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using moonclient::ItemScriptFacts;
using moonclient::kNoScriptElement;
using moonclient::ReadItemScript;

namespace {

// Les rangs des éléments que les essais nomment.
constexpr std::uint8_t kWater = 1;
constexpr std::uint8_t kFire = 3;
constexpr std::uint8_t kHoly = 6;

}  // namespace

TEST_CASE("Script d'objet : un soin tiré au sort et des états levés", "[app][item_script]") {
  // 526, « Royal Jelly ».
  const ItemScriptFacts jelly = ReadItemScript(
      "itemheal rand(325,405),rand(40,60); sc_end SC_POISON; sc_end SC_SILENCE; sc_end SC_BLIND; "
      "sc_end SC_CONFUSION; sc_end SC_CURSE; sc_end SC_Hallucination;\n");
  REQUIRE(jelly.heal.has_value());
  CHECK_FALSE(jelly.heal->percent);
  CHECK(jelly.heal->hp_least == 325);
  CHECK(jelly.heal->hp_most == 405);
  CHECK(jelly.heal->sp_least == 40);
  CHECK(jelly.heal->sp_most == 60);
  CHECK(jelly.cured == std::vector<std::string>{"POISON", "SILENCE", "BLIND", "CONFUSION", "CURSE",
                                                "HALLUCINATION"});
  // 523, « Holy Water » : un état, sans soin.
  const ItemScriptFacts water = ReadItemScript("sc_end SC_CURSE;\n");
  CHECK_FALSE(water.heal.has_value());
  CHECK(water.cured == std::vector<std::string>{"CURSE"});
}

TEST_CASE("Script d'objet : les formes de soin", "[app][item_script]") {
  // 501 : un tirage et une valeur nulle ; 505 : l'inverse.
  const ItemScriptFacts red = ReadItemScript("itemheal rand(45,65),0;");
  REQUIRE(red.heal.has_value());
  CHECK(red.heal->hp_least == 45);
  CHECK(red.heal->sp_most == 0);
  // Les bornes d'un tirage se rangent, comme le serveur les range.
  CHECK(ReadItemScript("itemheal rand(65,45),0;").heal->hp_least == 45);
  // 500 : des pour cent.
  const ItemScriptFacts choko = ReadItemScript("percentheal 10,10;");
  REQUIRE(choko.heal.has_value());
  CHECK(choko.heal->percent);
  CHECK(choko.heal->hp_most == 10);
  // `heal` ne tire pas au sort, et un nombre négatif blesse : rien.
  CHECK(ReadItemScript("heal 100,0;").heal.has_value());
  CHECK_FALSE(ReadItemScript("heal -100,0;").heal.has_value());
  CHECK_FALSE(ReadItemScript("percentheal rand(1,5),0;").heal.has_value());
  // Deux soins : on ne sait plus dire lequel.
  CHECK_FALSE(ReadItemScript("itemheal 10,0; itemheal 20,0;").heal.has_value());
  // Une valeur calculée n'est pas un motif.
  CHECK_FALSE(ReadItemScript("itemheal .@hp,0;").heal.has_value());
}

TEST_CASE("Script d'objet : un fait conditionnel se tait, entier", "[app][item_script]") {
  // Dans un bloc, ou derrière un `if` sans accolade : peut-être pas exécuté.
  CHECK_FALSE(ReadItemScript("if (BaseLevel > 50) { itemheal 100,0; }").heal.has_value());
  CHECK_FALSE(ReadItemScript("if (BaseLevel > 50) itemheal 100,0;").heal.has_value());
  // Un soin sûr et un soin conditionnel : la plage serait incomplète.
  CHECK_FALSE(
      ReadItemScript("itemheal 10,0; if (Class == Job_Priest) itemheal 50,0;").heal.has_value());
  // Un état levé sous condition tait toute la liste.
  CHECK(ReadItemScript("sc_end SC_POISON; if (x) sc_end SC_CURSE;").cured.empty());
  // Après une rupture, plus rien ne se lit.
  CHECK_FALSE(ReadItemScript("if (x) end; itemheal 10,0;").heal.has_value());
  CHECK(ReadItemScript("callfunc \"F_Rand\"; sc_end SC_POISON;").cured.empty());
  // Ce qui précède une rupture reste acquis.
  CHECK(ReadItemScript("itemheal 10,0; callfunc \"F_X\";").heal.has_value());
  // Un bloc d'une autre famille ne touche pas le soin.
  const ItemScriptFacts mixed = ReadItemScript("itemheal 10,0; if (x) { bonus bStr,1; }");
  CHECK(mixed.heal.has_value());
  // Un commentaire n'est pas une instruction, une chaîne non plus.
  CHECK(ReadItemScript("// itemheal 99,0;\nsc_end SC_CURSE;").cured.size() == 1);
  CHECK(ReadItemScript("/* sc_end SC_POISON; */ itemheal 1,0;").cured.empty());
  CHECK(ReadItemScript("autobonus \"{ itemheal 5,0; }\",10,1000; itemheal 1,0;").heal.has_value());
}

TEST_CASE("Script d'objet : l'élément d'attaque et d'armure", "[app][item_script]") {
  // 1131, « Ice Falchion ».
  const ItemScriptFacts falchion = ReadItemScript(
      "bonus bAtkEle,Ele_Water; bonus2 bAddEff,Eff_Freeze,500; bonus2 bAddEff2,Eff_Freeze,10; "
      "skill \"MG_COLDBOLT\",3; bonus3 bAutoSpell,\"MG_COLDBOLT\",3,100;");
  CHECK(falchion.attack_element == kWater);
  CHECK(falchion.defense_element == kNoScriptElement);
  CHECK(ReadItemScript("bonus bDefEle,Ele_Holy;").defense_element == kHoly);
  CHECK(ReadItemScript("bonus bAtkEle,Ele_Fire; bonus bAtkEle,Ele_Fire;").attack_element == kFire);
  // Deux éléments différents, ou un élément sous condition : rien.
  CHECK(ReadItemScript("bonus bAtkEle,Ele_Fire; bonus bAtkEle,Ele_Water;").attack_element ==
        kNoScriptElement);
  CHECK(ReadItemScript("if (getrefine() > 8) bonus bAtkEle,Ele_Fire;").attack_element ==
        kNoScriptElement);
  CHECK(ReadItemScript("bonus bAtkEle,.@ele;").attack_element == kNoScriptElement);
}

TEST_CASE("Script d'objet : la capture et la durée commune", "[app][item_script]") {
  // 628, « Well-Dried Bone » : l'objet est son propre appât.
  const ItemScriptFacts bone = ReadItemScript("pet;\n");
  CHECK(bone.captures);
  CHECK(bone.lure_item_id == 0);
  const ItemScriptFacts lure = ReadItemScript("pet 619;");
  CHECK(lure.captures);
  CHECK(lure.lure_item_id == 619);
  CHECK_FALSE(ReadItemScript("if (x) pet;").captures);
  // 645, « Concentration Potion » : trente minutes.
  CHECK(ReadItemScript("sc_start SC_ATTHASTE_POTION1,1800000,0;").effect_duration_ms == 1800000);
  CHECK(ReadItemScript("sc_start4 SC_FOOD,60000,1,2,3,4; sc_start SC_X,60000,0;")
            .effect_duration_ms == 60000);
  // Deux durées différentes, une durée calculée ou infinie : rien.
  CHECK(ReadItemScript("sc_start SC_A,1000,0; sc_start SC_B,2000,0;").effect_duration_ms == 0);
  CHECK(ReadItemScript("sc_start SC_A,.@t,0;").effect_duration_ms == 0);
  CHECK(ReadItemScript("sc_start SC_A,-1,0;").effect_duration_ms == 0);
}

TEST_CASE("Script d'objet : les états posés et leurs valeurs", "[app][item_script]") {
  // 12434, « Petite Tail Noodles ».
  const ItemScriptFacts noodles = ReadItemScript("sc_start SC_PUTTI_TAILS_NOODLES,300000,20;");
  REQUIRE(noodles.starts.size() == 1);
  CHECK(noodles.starts[0].status == "PUTTI_TAILS_NOODLES");
  CHECK(noodles.starts[0].values == std::array<std::int32_t, 4>{20, 0, 0, 0});
  // `sc_start2` donne val1 et val2 ; `sc_start4` les quatre.
  const ItemScriptFacts two = ReadItemScript("sc_start2 sc_increase_maxhp,3600000,5,10;");
  REQUIRE(two.starts.size() == 1);
  CHECK(two.starts[0].status == "INCREASE_MAXHP");
  CHECK(two.starts[0].values == std::array<std::int32_t, 4>{5, 10, 0, 0});
  const ItemScriptFacts four = ReadItemScript("sc_start4 SC_FOOD,60000,1,-2,3,4;");
  REQUIRE(four.starts.size() == 1);
  CHECK(four.starts[0].values == std::array<std::int32_t, 4>{1, -2, 3, 4});
  // Une valeur tirée au sort, une durée qui diffère, un état dans un bloc :
  // aucun état ne se dit.
  CHECK(ReadItemScript("sc_start SC_A,1000,rand(1,5);").starts.empty());
  CHECK(ReadItemScript("sc_start SC_A,1000,1; sc_start SC_B,2000,1;").starts.empty());
  CHECK(ReadItemScript("sc_start SC_A,1000,1; if (x) { sc_start SC_B,1000,1; }").starts.empty());
  // Une valeur hors de la largeur d'un état : la durée reste, pas l'état.
  const ItemScriptFacts wide = ReadItemScript("sc_start SC_A,1000,9999999999;");
  CHECK(wide.effect_duration_ms == 1000);
  CHECK(wide.starts.empty());
}
