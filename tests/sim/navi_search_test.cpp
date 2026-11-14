#include "ro/sim/navi_search.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/navi_tables.h"

using ro::formats::NaviData;
using ro::formats::NaviMap;
using ro::formats::NaviNpc;
using ro::formats::NaviSpawn;
using ro::sim::ContentsOfMap;
using ro::sim::FoldForSearch;
using ro::sim::NaviHitKind;
using ro::sim::NaviSearchQuery;
using ro::sim::SearchNavi;
using ro::sim::VisibleNpcName;

namespace {

NaviData Sample() {
  NaviData data;
  data.maps = {NaviMap{"prontera", "Prontera", 5003, 312, 392},
               NaviMap{"geffen", "Geffen", 5003, 240, 240},
               NaviMap{"gef_fild00", "Geffen Field", 5001, 400, 400}};
  NaviNpc kafra_p;
  kafra_p.map = 0;
  kafra_p.npc_id = 1;
  kafra_p.name = "Kafra Employee";
  NaviNpc kafra_g = kafra_p;
  kafra_g.map = 1;
  kafra_g.npc_id = 2;
  NaviNpc retour;
  retour.map = 1;
  retour.npc_id = 3;
  retour.name = "Retour à Gonryun";
  data.npcs = {kafra_p, kafra_g, retour};
  NaviSpawn poring;
  poring.map = 2;
  poring.spawn_id = 10;
  poring.name = "Poring";
  data.spawns = {poring};
  data.BuildIndexes();
  return data;
}

}  // namespace

TEST_CASE("Recherche de navigation : un groupe par nom, la carte du joueur en tête",
          "[sim][navi]") {
  const NaviData data = Sample();
  NaviSearchQuery query;
  query.term = "KAFRA";
  query.prefer_map = 1;
  const auto groups = SearchNavi(data, query);
  REQUIRE(groups.size() == 1);
  CHECK(groups[0].kind == NaviHitKind::kNpc);
  REQUIRE(groups[0].hits.size() == 2);
  CHECK(data.npcs[groups[0].hits[0].rank].map == 1);
}

TEST_CASE("Recherche de navigation : sans accent, par morceaux, et par la carte", "[sim][navi]") {
  const NaviData data = Sample();
  NaviSearchQuery query;
  query.term = "retour a gon";
  CHECK(SearchNavi(data, query).size() == 1);
  // « kaf pront » : un mot dans le nom, l'autre dans la carte qui le porte.
  query.term = "kaf pront";
  const auto via_map = SearchNavi(data, query);
  REQUIRE(via_map.size() == 1);
  REQUIRE(via_map[0].hits.size() == 1);
  CHECK(data.npcs[via_map[0].hits[0].rank].map == 0);
  // Le nom exact passe avant ce qui ne fait que le contenir.
  query.term = "geffen";
  const auto maps = SearchNavi(data, query);
  REQUIRE(maps.size() >= 2);
  CHECK(maps[0].name == "Geffen");
}

TEST_CASE("Recherche de navigation : la borne tombe après le classement", "[sim][navi]") {
  // 🔴 LES MONSTRES SE PARCOURENT EN DERNIER : bornée pendant la collecte, la
  // liste était pleine de correspondances faibles avant qu'un nom exact n'y
  // entre.
  NaviData data = Sample();
  NaviNpc crowd;
  crowd.map = 2;  // « Geffen Field » : tous répondent à « geffen » par la carte
  crowd.name = "Passant";
  for (std::uint32_t id = 100; id < 110; ++id) {
    crowd.npc_id = id;
    data.npcs.push_back(crowd);
  }
  NaviSpawn exact;
  exact.map = 0;
  exact.spawn_id = 11;
  exact.name = "Geffen";
  data.spawns.push_back(exact);
  data.BuildIndexes();

  NaviSearchQuery query;
  query.term = "geffen";
  query.max_hits = 3;
  const auto groups = SearchNavi(data, query);
  std::size_t total = 0;
  bool spawn_kept = false;
  for (const auto& group : groups) {
    total += group.hits.size();
    spawn_kept = spawn_kept || group.kind == NaviHitKind::kSpawn;
  }
  CHECK(total == 3);
  CHECK(spawn_kept);
}

TEST_CASE("Recherche de navigation : bornage, partie cachée et contenu d'une carte",
          "[sim][navi]") {
  const NaviData data = Sample();
  NaviSearchQuery query;
  query.term = "Kafra Employee#pront";
  query.only_map = 0;
  const auto bounded = SearchNavi(data, query);
  REQUIRE(bounded.size() == 1);
  CHECK(bounded[0].hits.size() == 1);
  CHECK(VisibleNpcName("Market Guide#info") == "Market Guide");
  CHECK(FoldForSearch("ÉtÉ Ça") == "ete ca");

  const auto contents = ContentsOfMap(data, 1);
  REQUIRE(contents.size() == 1);
  CHECK(contents[0].hits.size() == 2);
  CHECK(ContentsOfMap(data, 2)[0].kind == NaviHitKind::kSpawn);
}
