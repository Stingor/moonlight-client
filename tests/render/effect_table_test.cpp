#include "ro/render/effect_table.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <span>
#include <string>

#include <catch2/catch_test_macros.hpp>

using ro::render::EffectEntry;
using ro::render::EffectFile;
using ro::render::EffectPrimitiveTable;
using ro::render::EffectTable;

TEST_CASE("Table des effets : elle est triée et sans doublon", "[render][effect]") {
  // La recherche est DICHOTOMIQUE : un identifiant hors d'ordre serait
  // introuvable, et un doublon rendrait l'un des deux inatteignable. Le
  // générateur trie, mais c'est ici que la propriété est vérifiée — la table se
  // régénère, et rien ne dit que la prochaine sortira dans le même ordre.
  const std::span<const ro::render::EffectEntry> table = EffectTable();
  REQUIRE(!table.empty());
  std::uint16_t previous = 0;
  bool first = true;
  for (const ro::render::EffectEntry& entry : table) {
    CHECK(!entry.path.empty());
    if (!first) {
      CHECK(previous < entry.id);
    }
    previous = entry.id;
    first = false;
  }
}

TEST_CASE("Table des effets : un identifiant connu donne son fichier", "[render][effect]") {
  CHECK(EffectFile(13) == "SafetyWall.str");
  CHECK(EffectFile(89) == "StormGust.str");
  // Les chemins portent le séparateur de l'archive, et le coréen s'écrit tel
  // quel : c'est le nom du fichier, pas une traduction.
  CHECK(EffectFile(1061) == "RL_C_MAKER\\deffender.str");
  CHECK(EffectFile(204) == "빨간포션.str");
}

TEST_CASE("Table des effets : un identifiant inconnu ne rend rien", "[render][effect]") {
  // Les identifiants que le client ne nomme pas sont procéduraux ou
  // inemployés. Rendre un chemin vide vaut mieux qu'un chemin faux : le
  // premier ne se charge pas, le second chargerait l'effet du voisin.
  CHECK(EffectFile(0).empty());
  CHECK(EffectFile(12).empty());
  CHECK(EffectFile(100000).empty());
}

TEST_CASE("Table des effets : un effet à variantes en tire une", "[render][effect]") {
  // Pneuma existe en trois fichiers, et le client en tire un au sort à chaque
  // lancement. Jouer toujours le premier se verrait.
  std::set<std::string> drawn;
  for (std::uint32_t draw = 0; draw < 9; ++draw) {
    const std::string file = EffectFile(141, draw);
    CHECK(file.starts_with("Pneuma"));
    CHECK(file.ends_with(".str"));
    drawn.insert(file);
  }
  CHECK(drawn.size() == 3);
  // Le tirage est pris MODULO le nombre de variantes : il revient au bout de
  // trois, et aucun tirage ne sort de l'intervalle.
  CHECK(EffectFile(141, 0) == "Pneuma1.str");
  CHECK(EffectFile(141, 3) == "Pneuma1.str");
  CHECK(EffectFile(141, 2) == "Pneuma3.str");
}

TEST_CASE("Table des effets : les effets d'essai portent leur numéro", "[render][effect]") {
  // Le client les forme avec « test_effect\\%02d\\%02d.str » et l'identifiant
  // moins 1244, deux fois. Le nombre étant connu d'avance, il est écrit :
  // ce qui se calcule une fois pour toutes n'a pas à se recalculer en jeu.
  CHECK(EffectFile(1244) == "test_effect\\00\\00.str");
  CHECK(EffectFile(1253) == "test_effect\\09\\09.str");
}

TEST_CASE("Table des effets : la seconde table est triée et sans doublon", "[render][effect]") {
  // La recherche y est dichotomique, comme dans la première : un désordre la
  // ferait manquer une entrée sans rien dire.
  const std::span<const EffectEntry> table = EffectPrimitiveTable();
  CHECK(table.size() > 200);
  for (std::size_t index = 1; index < table.size(); ++index) {
    CHECK(table[index - 1].id < table[index].id);
  }
}

TEST_CASE("Table des effets : là où les deux tables se recouvrent, elles s'accordent",
          "[render][effect]") {
  // Le client résout un identifiant d'effet par DEUX fonctions, selon le chemin
  // qui le fait naître. Nous les consultons toutes deux parce que nous
  // cherchons à dessiner, pas à reproduire le chemin — mais cela ne tiendrait
  // pas si elles nommaient le même identifiant différemment : le fichier joué
  // dépendrait alors de l'ordre de consultation, qui n'est fondé sur rien.
  const std::span<const EffectEntry> first = EffectTable();
  const std::span<const EffectEntry> second = EffectPrimitiveTable();
  std::size_t shared = 0;
  for (const EffectEntry& entry : second) {
    const auto found = std::ranges::lower_bound(first, entry.id, {}, &EffectEntry::id);
    if (found == first.end() || found->id != entry.id) {
      continue;
    }
    ++shared;
    CHECK(found->path == entry.path);
    CHECK(found->variants == entry.variants);
  }
  // Le recouvrement est MINCE, et c'est tout le propos : la première table ne
  // nomme presque aucun effet de compétence.
  CHECK(shared > 0);
  CHECK(shared < second.size() / 2);
}

TEST_CASE("Table des effets : les compétences classiques ont enfin leur fichier",
          "[render][effect]") {
  // Ces identifiants-là ne sont dans AUCUNE des tables que nous lisions avant :
  // ils viennent du second résolveur, celui des sprites-enfants. Chaque couple
  // se vérifie par son nom, et c'est ce qui prouve qu'aucun n'a été tiré du
  // mauvais cas.
  CHECK(EffectFile(29) == "Lightning.str");
  CHECK(EffectFile(30) == "ThunderStorm.str");
  CHECK(EffectFile(96) == "FirePillar.str");
  CHECK(EffectFile(143) == "SonicBlow.str");
  CHECK(EffectFile(148) == "Pierce.str");
  CHECK(EffectFile(170) == "CartRevolution.str");
  CHECK(EffectFile(183) == "sui_explosion.str");
  CHECK(EffectFile(440) == "asum.str");

  // Un effet à variantes de la seconde table se tire comme ceux de la
  // première : le mur de feu en a deux, la pluie de météores trois.
  CHECK(EffectFile(25, 0) == "Firewall1.str");
  CHECK(EffectFile(25, 1) == "Firewall2.str");
  CHECK(EffectFile(25, 2) == "Firewall1.str");
  CHECK(EffectFile(92, 2) == "Meteor3.str");

  // Et la PREMIÈRE garde la main là où les deux nomment : elle est consultée
  // d'abord, et les deux disent la même chose.
  CHECK(EffectFile(89) == "StormGust.str");
}

TEST_CASE("Table des effets : les noms CORÉENS que le relevé ne voyait pas", "[render][effect]") {
  // ⚠ UN NOM EN OCTETS BRUTS RESTE UN NOM. Le second résolveur nomme ses effets
  // coréens par des chaînes d'octets bruts, et ces identifiants ont bien un
  // fichier : les prendre pour des effets procéduraux ferait attendre une
  // figure qui n'existe pas.
  //
  // EF_DISPELL et EF_MAGICROD sont les deux plus émis du lot : neuf sites à eux
  // deux, et pas une ligne de code à écrire pour les montrer.
  CHECK(EffectFile(235) == "디스펠.str");
  CHECK(EffectFile(244) == "매직로드.str");
  CHECK(EffectFile(406) == "소울번.str");
  CHECK(EffectFile(646) == "트랙킹.str");

  // ⚠ ET QUATRE IDENTIFIANTS VIVENT HORS DES TABLES DE SAUTS, aux BORNES des
  // cinq bandes : le compilateur les traite par un saut d'égalité avant de
  // soustraire la base. Un parcours qui ne lit que les tables les manque.
  CHECK(EffectFile(64) == "ArrowShot.str");
  CHECK(EffectFile(337) == "JobLvUP.str");
  CHECK(EffectFile(635) == "fire dragon.str");
  CHECK(EffectFile(920) == "Firewall_per.str");
}
