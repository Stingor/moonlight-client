#include "ro/render/effect_case_table.h"

#include <cstdint>
#include <set>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_table.h"

using ro::render::EffectCaseEntry;
using ro::render::EffectCaseFor;
using ro::render::EffectCaseTable;

TEST_CASE("Cas d'effet : la table tient ses invariants", "[render][effectcase]") {
  const std::span<const EffectCaseEntry> table = EffectCaseTable();
  // Le relevé en compte mille cent douze, et c'est le DÉNOMINATEUR de tout ce
  // que le juge mesure. Un chiffre qui bougerait sans qu'on l'ait voulu ferait
  // bouger tous les rapports d'un coup, sans que rien ne le dise.
  CHECK(table.size() == 1112);

  std::uint16_t previous = 0;
  bool first = true;
  for (const EffectCaseEntry& entry : table) {
    if (!first) {
      // Strictement croissante : un identifiant ne peut avoir qu'un cas, et la
      // recherche dichotomique en dépend.
      CHECK(previous < entry.effect_id);
    }
    first = false;
    previous = entry.effect_id;
    // Les trois étages du répartiteur, et rien d'autre.
    CHECK(entry.stage >= 1);
    CHECK(entry.stage <= 3);
  }
}

TEST_CASE("Cas d'effet : la recherche rend le cas, ou rien", "[render][effectcase]") {
  for (const EffectCaseEntry& entry : EffectCaseTable()) {
    const EffectCaseEntry* found = EffectCaseFor(entry.effect_id);
    REQUIRE(found != nullptr);
    CHECK(found->effect_id == entry.effect_id);
    CHECK(found->stage == entry.stage);
  }
  // Au-delà du plus haut cas relevé, le client tombe sur son défaut.
  CHECK(EffectCaseFor(0xFFFF) == nullptr);
}

TEST_CASE("Cas d'effet : la table est le DÉNOMINATEUR, pas un numérateur", "[render][effectcase]") {
  // ⚠ CE QUE CE TEST GARDE. La table des cas dit ce qu'il y a À montrer ; les
  // nôtres disent ce que nous SAVONS montrer. Les deux ne doivent jamais se
  // confondre — et une table de cas qui se mettrait à ne contenir que ce que
  // nous savons peindre rendrait le juge incapable de mesurer un manque.
  //
  // Le rapport se lit donc dans les deux sens : il reste de la place, et ce
  // que nous savons montrer ne déborde pas de ce que le client traite.
  std::set<std::uint32_t> has_case;
  for (const EffectCaseEntry& entry : EffectCaseTable()) {
    has_case.insert(entry.effect_id);
  }

  std::size_t connus = 0;
  std::size_t without_case_count = 0;
  for (const ro::render::EffectNodePattern& pattern : ro::render::EffectNodePatternTable()) {
    if (has_case.contains(pattern.effect_id)) {
      ++connus;
    } else {
      ++without_case_count;
    }
  }
  CHECK(connus != 0);
  // Un patron dont le client n'a aucun cas décrirait un effet que PERSONNE ne
  // dessine : notre relevé des nœuds part des cas, il ne peut pas en sortir.
  CHECK(without_case_count == 0);
}

TEST_CASE("Cas d'effet : les deux familles sont presque DISJOINTES", "[render][effectcase]") {
  // ⚠ CE TEST GARDE UNE MESURE QUI A DÉFAIT UNE HYPOTHÈSE, et c'est pour cela
  // qu'il existe. On pouvait croire que les deux familles passent par le même
  // répartiteur — qu'un effet en fichier a un cas dont la seule tâche est de
  // lire son `.str`. C'EST FAUX : sur les 1 157 identifiants auxquels les deux
  // tables de fichiers du client donnent un chemin, 227 seulement ont aussi un
  // cas procédural.
  //
  // La conséquence est tout l'intérêt : un juge assis sur la seule table des
  // cas déclarerait introuvables 930 effets que le client montre très bien. Son
  // dénominateur est l'UNION, et ce test empêche l'hypothèse de revenir.
  std::set<std::uint32_t> in_file;
  for (const std::span<const ro::render::EffectEntry> table :
       {ro::render::EffectTable(), ro::render::EffectPrimitiveTable()}) {
    for (const ro::render::EffectEntry& entry : table) {
      in_file.insert(entry.id);
    }
  }

  std::size_t with_case_count = 0;
  for (const std::uint32_t effect_id : in_file) {
    if (EffectCaseFor(effect_id) != nullptr) {
      ++with_case_count;
    }
  }
  CHECK(in_file.size() == 1157);
  CHECK(with_case_count == 227);
  // Le dénominateur d'un juge honnête, et il ne se lit dans aucune table seule.
  std::set<std::uint32_t> union_of_both = in_file;
  for (const EffectCaseEntry& entry : EffectCaseTable()) {
    union_of_both.insert(entry.effect_id);
  }
  CHECK(union_of_both.size() == 2042);
}
