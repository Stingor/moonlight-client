#include "ro/formats/rsw.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"

#include "support/map_builder.h"

using ro::core::ErrorCode;
using ro::formats::RswScene;
using ro::test_support::BuildRsw;
using ro::test_support::RswBuildOptions;

TEST_CASE("RSW : en-tête version 2.1", "[formats][rsw]") {
  auto rsw = RswScene::Parse(BuildRsw(RswBuildOptions{
      .ground_file = "prontera.gnd", .altitude_file = "prontera.gat", .water_level = 12.5F}));
  REQUIRE(rsw.has_value());

  CHECK(rsw->header().major_version == 2);
  CHECK(rsw->header().minor_version == 1);
  CHECK(rsw->header().version() == 0x0201);
  CHECK(rsw->header().ground_file == "prontera.gnd");
  CHECK(rsw->header().altitude_file == "prontera.gat");
  CHECK(rsw->water().present);
  CHECK(rsw->water().level == 12.5F);
}

TEST_CASE("RSW : les champs conditionnels décalent bien l'en-tête", "[formats][rsw]") {
  // Trois dispositions différentes, et le niveau d'eau doit sortir juste dans
  // les trois. C'est exactement ce que les offsets 166 / 167 / 171 codés en dur
  // dans rAthena décrivent ; les lire structurellement évite de les recopier.
  SECTION("1.9 — ni build number ni octet de rendu") {
    auto rsw =
        RswScene::Parse(BuildRsw(RswBuildOptions{.major = 1, .minor = 9, .water_level = 7.0F}));
    REQUIRE(rsw.has_value());
    CHECK(rsw->water().level == 7.0F);
  }

  SECTION("2.2 — octet de rendu, pas de build number") {
    auto rsw =
        RswScene::Parse(BuildRsw(RswBuildOptions{.major = 2, .minor = 2, .water_level = 8.0F}));
    REQUIRE(rsw.has_value());
    CHECK(rsw->water().level == 8.0F);
  }

  SECTION("2.5 — build number ET octet de rendu") {
    auto rsw = RswScene::Parse(
        BuildRsw(RswBuildOptions{.major = 2, .minor = 5, .build_number = 42, .water_level = 9.0F}));
    REQUIRE(rsw.has_value());
    CHECK(rsw->header().build_number == 42);
    CHECK(rsw->water().level == 9.0F);
  }
}

TEST_CASE("RSW : le fichier d'altitude n'existe qu'à partir de 1.4", "[formats][rsw]") {
  auto rsw = RswScene::Parse(BuildRsw(
      RswBuildOptions{.major = 1, .minor = 3, .ground_file = "vieux.gnd", .water_level = 1.0F}));
  REQUIRE(rsw.has_value());
  CHECK(rsw->header().ground_file == "vieux.gnd");
  CHECK(rsw->header().altitude_file.empty());
}

TEST_CASE("RSW : à partir de 2.6 la section eau n'est plus là", "[formats][rsw]") {
  auto rsw = RswScene::Parse(BuildRsw(RswBuildOptions{.major = 2, .minor = 6}));
  REQUIRE(rsw.has_value());
  CHECK_FALSE(rsw->water().present);
  CHECK_FALSE(rsw->ServerWaterLevel().has_value());
}

TEST_CASE("RSW : signature invalide", "[formats][rsw]") {
  std::vector<std::uint8_t> rsw = BuildRsw(RswBuildOptions{});
  rsw[2] = 'X';
  auto parsed = RswScene::Parse(rsw);
  REQUIRE_FALSE(parsed.has_value());
  CHECK(parsed.error().code == ErrorCode::kBadSignature);
}

TEST_CASE("ServerWaterLevel rend le niveau BRUT, sans troncature", "[formats][rsw][parité]") {
  // La troncature n'appartient pas à cette couche : c'est une règle de
  // COMPARAISON, portée par WaterLevelRule dans DeriveServerCells. La tronquer
  // ici perdrait l'information et rendrait la règle brute inatteignable.
  // Raisonnement complet dans docs/adr/0004-niveau-d-eau-brut.md.
  const auto level_for = [](float raw) {
    auto rsw = RswScene::Parse(BuildRsw(RswBuildOptions{.water_level = raw}));
    REQUIRE(rsw.has_value());
    return rsw->ServerWaterLevel();
  };

  CHECK(level_for(12.9F) == 12.9F);
  CHECK(level_for(-0.5F) == -0.5F);
  CHECK(level_for(0.0F) == 0.0F);
}

TEST_CASE("ServerWaterLevel refuse les versions hors plage serveur", "[formats][rsw][parité]") {
  // rAthena n'accepte que 1.4 à 2.5 et rend RSW_NO_WATER en dehors. La carte
  // est alors traitée comme sans eau CÔTÉ SERVEUR, même si elle en a une. On
  // reproduit ce refus : c'est la grille du serveur qui fait autorité.
  SECTION("trop ancienne") {
    auto rsw =
        RswScene::Parse(BuildRsw(RswBuildOptions{.major = 1, .minor = 3, .water_level = 50.0F}));
    REQUIRE(rsw.has_value());
    CHECK(rsw->water().present);  // la section EST là…
    CHECK(rsw->water().level == 50.0F);
    CHECK_FALSE(rsw->ServerWaterLevel().has_value());  // …mais le serveur l'ignore
  }

  SECTION("trop récente") {
    auto rsw =
        RswScene::Parse(BuildRsw(RswBuildOptions{.major = 2, .minor = 6, .water_level = 50.0F}));
    REQUIRE(rsw.has_value());
    CHECK_FALSE(rsw->ServerWaterLevel().has_value());
  }

  SECTION("aux bornes, acceptée") {
    auto low =
        RswScene::Parse(BuildRsw(RswBuildOptions{.major = 1, .minor = 4, .water_level = 30.0F}));
    REQUIRE(low.has_value());
    CHECK(low->ServerWaterLevel() == 30.0F);

    auto high =
        RswScene::Parse(BuildRsw(RswBuildOptions{.major = 2, .minor = 5, .water_level = 30.0F}));
    REQUIRE(high.has_value());
    CHECK(high->ServerWaterLevel() == 30.0F);
  }
}

TEST_CASE("RSW : la section lumière", "[formats][rsw]") {
  auto rsw = RswScene::Parse(BuildRsw(RswBuildOptions{.light_longitude = 130,
                                                      .light_latitude = 20,
                                                      .light_diffuse = {1.0F, 0.9F, 0.8F},
                                                      .light_ambient = {0.55F, 0.5F, 0.5F}}));
  REQUIRE(rsw.has_value());
  REQUIRE(rsw->light().present);

  CHECK(rsw->light().longitude == 130);
  CHECK(rsw->light().latitude == 20);
  CHECK(rsw->light().diffuse[1] == 0.9F);
  CHECK(rsw->light().ambient[0] == 0.55F);
}

TEST_CASE("RSW : un fichier sans lumière reste exploitable", "[formats][rsw]") {
  // La lumière suit l'eau. Un fichier qui s'arrête avant n'est pas invalide :
  // l'en-tête et l'eau restent lisibles, et la lumière garde ses valeurs
  // neutres. Refuser tout le fichier priverait le client d'une carte pour un
  // champ qu'il sait remplacer.
  auto rsw = RswScene::Parse(BuildRsw(RswBuildOptions{.water_level = 3.0F, .write_light = false}));
  REQUIRE(rsw.has_value());

  CHECK_FALSE(rsw->light().present);
  CHECK(rsw->light().ambient[0] == 1.0F);
  CHECK(rsw->water().level == 3.0F);
  CHECK(rsw->ServerWaterLevel() == 3.0F);
}

TEST_CASE("RSW : la liste de modèles", "[formats][rsw]") {
  RswBuildOptions options;
  options.models = {
      {.name = "porte",
       .model_name = "prontera\\porte.rsm",
       .node_name = "racine",
       .position = {10.0F, -20.0F, 30.0F},
       .rotation = {0.0F, 90.0F, 0.0F},
       .scale = {1.0F, 2.0F, 1.0F}},
      {.name = "fontaine", .model_name = "prontera\\fontaine.rsm"},
  };

  auto rsw = RswScene::Parse(BuildRsw(options));
  REQUIRE(rsw.has_value());
  REQUIRE(rsw->models().size() == 2);

  const auto& first = rsw->models()[0];
  CHECK(first.name == "porte");
  CHECK(first.model_name == "prontera\\porte.rsm");
  CHECK(first.node_name == "racine");
  CHECK(first.position == std::array<float, 3>{10.0F, -20.0F, 30.0F});
  CHECK(first.rotation[1] == 90.0F);
  CHECK(first.scale[1] == 2.0F);
  CHECK(rsw->models()[1].model_name == "prontera\\fontaine.rsm");
}

TEST_CASE("RSW : une scène assemblée garde son en-tête, sa lumière et ses modèles, sans eau",
          "[formats][rsw]") {
  // Les mêmes champs qu'un fichier lu, posés en mémoire.
  auto read = RswScene::Parse(BuildRsw(RswBuildOptions{
      .ground_file = "sol.gnd",
      .light_longitude = 130,
      .light_latitude = 20,
      .models = {{.name = "arbre", .model_name = "arbre.rsm", .position = {1.0F, 2.0F, 3.0F}}}}));
  REQUIRE(read.has_value());
  const RswScene assembled = RswScene::Assemble(read->header(), read->light(), read->models());

  CHECK(assembled.header().version() == read->header().version());
  CHECK(assembled.header().ground_file == "sol.gnd");
  CHECK(assembled.light().present);
  CHECK(assembled.light().longitude == 130);
  CHECK(assembled.light().latitude == 20);
  REQUIRE(assembled.models().size() == 1);
  CHECK(assembled.models()[0].model_name == "arbre.rsm");
  CHECK(assembled.models()[0].position == std::array<float, 3>{1.0F, 2.0F, 3.0F});
  CHECK_FALSE(assembled.water().present);
  CHECK_FALSE(assembled.ServerWaterLevel().has_value());
  CHECK(assembled.sounds().empty());
  CHECK(assembled.effects().empty());
}

TEST_CASE("RSW : lumières, sons et effets sont TRAVERSÉS sans décaler", "[formats][rsw]") {
  // Le point qui compte. Ces objets ne servent pas encore, mais se tromper
  // d'un octet sur leur longueur décale tous les modèles suivants — qui
  // sortiraient alors avec des noms et des positions absurdes, sans la moindre
  // erreur. Le constructeur les écrit AVANT les modèles pour cette raison.
  RswBuildOptions options;
  options.light_object_count = 3;
  options.sound_object_count = 2;
  options.effect_object_count = 1;
  options.models = {{.name = "apres", .model_name = "apres.rsm", .position = {1.0F, 2.0F, 3.0F}}};

  auto rsw = RswScene::Parse(BuildRsw(options));
  REQUIRE(rsw.has_value());

  CHECK(rsw->object_counts().lights == 3);
  CHECK(rsw->object_counts().sounds == 2);
  CHECK(rsw->object_counts().effects == 1);
  CHECK(rsw->object_counts().unknown == 0);

  // ⚠ ET L'ÉMETTEUR EST GARDÉ, plus seulement compté : c'est de lui que sort la
  // fumée d'une cheminée. Prontera en porte cent cinquante-deux. Son numéro
  // n'est PAS un identifiant d'effet — c'est un renvoi vers la table
  // `effecttool` de la carte, qui porte la texture et la cadence.
  REQUIRE(rsw->effects().size() == 1);

  REQUIRE(rsw->models().size() == 1);
  CHECK(rsw->models()[0].model_name == "apres.rsm");
  CHECK(rsw->models()[0].position == std::array<float, 3>{1.0F, 2.0F, 3.0F});
}

TEST_CASE("RSW : une source de son garde chacun de ses champs", "[formats][rsw]") {
  RswBuildOptions options;
  options.sounds = {{.name = "fontaine",
                     .wave_name = "fountain.wav",
                     .position = {10.0F, -2.0F, 30.0F},
                     .volume = 0.8F,
                     .width = 40,
                     .height = 20,
                     .range = 120.0F,
                     .cycle_seconds = 6.5F}};
  options.models = {{.name = "apres", .model_name = "apres.rsm"}};

  auto rsw = RswScene::Parse(BuildRsw(options));
  REQUIRE(rsw.has_value());
  REQUIRE(rsw->sounds().size() == 1);
  const ro::formats::RswSound& son = rsw->sounds()[0];
  CHECK(son.name == "fontaine");
  CHECK(son.wave_name == "fountain.wav");
  CHECK(son.position == std::array<float, 3>{10.0F, -2.0F, 30.0F});
  CHECK(son.volume == 0.8F);
  CHECK(son.width == 40);
  CHECK(son.height == 20);
  CHECK(son.range == 120.0F);
  CHECK(son.cycle_seconds == 6.5F);
  // Et ce qui suit n'est pas décalé.
  REQUIRE(rsw->models().size() == 1);
  CHECK(rsw->models()[0].model_name == "apres.rsm");
}

TEST_CASE("RSW : avant la 2.0, une source de son se rejoue toutes les quatre secondes",
          "[formats][rsw]") {
  RswBuildOptions options;
  options.major = 1;
  options.minor = 9;
  options.sounds = {{.wave_name = "wind.wav", .width = 10, .height = 10, .range = 50.0F}};
  options.models = {{.name = "apres", .model_name = "apres.rsm"}};

  auto rsw = RswScene::Parse(BuildRsw(options));
  REQUIRE(rsw.has_value());
  REQUIRE(rsw->sounds().size() == 1);
  CHECK(rsw->sounds()[0].cycle_seconds == ro::formats::kRswDefaultSoundCycleSeconds);
  REQUIRE(rsw->models().size() == 1);
  CHECK(rsw->models()[0].model_name == "apres.rsm");
}

TEST_CASE("RSW : un type d'objet inconnu arrête la lecture", "[formats][rsw]") {
  // Sa taille est inconnue, donc il n'y a aucun moyen de se replacer :
  // continuer lirait des octets quelconques comme des modèles. Mieux vaut
  // s'arrêter et le compter.
  RswBuildOptions options;
  options.models = {{.name = "avant", .model_name = "avant.rsm"}};
  options.write_unknown_object = true;

  auto rsw = RswScene::Parse(BuildRsw(options));
  REQUIRE(rsw.has_value());
  CHECK(rsw->models().size() == 1);
  CHECK(rsw->object_counts().unknown == 1);
}

TEST_CASE("RSW : une vitesse d'animation aberrante retombe à 1", "[formats][rsw]") {
  // Les bornes sont celles du natif : zéro retombe à 1, cent est gardé.
  RswBuildOptions options;
  options.models = {{.model_name = "a.rsm", .animation_speed = -5.0F},
                    {.model_name = "b.rsm", .animation_speed = 250.0F},
                    {.model_name = "c.rsm", .animation_speed = 2.5F},
                    {.model_name = "d.rsm", .animation_speed = 0.0F},
                    {.model_name = "e.rsm", .animation_speed = 100.0F}};

  auto rsw = RswScene::Parse(BuildRsw(options));
  REQUIRE(rsw.has_value());
  REQUIRE(rsw->models().size() == 5);

  CHECK(rsw->models()[0].animation_speed == 1.0F);
  CHECK(rsw->models()[1].animation_speed == 1.0F);
  CHECK(rsw->models()[2].animation_speed == 2.5F);
  CHECK(rsw->models()[3].animation_speed == 1.0F);
  CHECK(rsw->models()[4].animation_speed == 100.0F);
}

TEST_CASE("RSW : l'emprise du sol", "[formats][rsw]") {
  RswBuildOptions options;
  options.ground_top = -7;
  options.ground_right = 42;

  auto rsw = RswScene::Parse(BuildRsw(options));
  REQUIRE(rsw.has_value());
  CHECK(rsw->ground().present);
  CHECK(rsw->ground().top == -7);
  CHECK(rsw->ground().right == 42);
}

TEST_CASE("RSW : sans liste d'objets, la carte reste exploitable", "[formats][rsw]") {
  RswBuildOptions options;
  options.water_level = 5.0F;
  options.write_objects = false;

  auto rsw = RswScene::Parse(BuildRsw(options));
  REQUIRE(rsw.has_value());
  CHECK(rsw->models().empty());
  CHECK(rsw->ServerWaterLevel() == 5.0F);
  CHECK(rsw->light().present);
}

TEST_CASE("RSW : aucune troncature ne fait planter le parseur", "[formats][rsw]") {
  const std::vector<std::uint8_t> complete =
      BuildRsw(RswBuildOptions{.major = 2, .minor = 5, .build_number = 7, .water_level = 4.0F});
  for (std::size_t length = 0; length <= complete.size(); ++length) {
    const std::vector<std::uint8_t> truncated(
        complete.begin(), complete.begin() + static_cast<std::ptrdiff_t>(length));
    static_cast<void>(RswScene::Parse(truncated));
  }
  SUCCEED("aucune troncature n'a provoqué de lecture hors bornes");
}

namespace {

// Un quadtree complet dont chaque nœud porte son rang dans chacun de ses douze
// réels : un décalage d'un réel, ou un champ lu à la place d'un autre, se voit.
[[nodiscard]] std::vector<std::array<float, 12>> RankedQuadTree(std::size_t nodes) {
  constexpr float kFieldStep = 0.125F;
  std::vector<std::array<float, 12>> tree(nodes);
  for (std::size_t rank = 0; rank < nodes; ++rank) {
    for (std::size_t field = 0; field < tree[rank].size(); ++field) {
      tree[rank][field] = static_cast<float>(rank) + kFieldStep * static_cast<float>(field);
    }
  }
  return tree;
}

}  // namespace

TEST_CASE("RSW : le quadtree suit la liste d'objets, dans l'ordre du fichier", "[formats][rsw]") {
  RswBuildOptions options;
  options.models = {{.name = "avant", .model_name = "avant.rsm"}};
  options.quad_tree = RankedQuadTree(ro::formats::kRswQuadTreeNodeCount);

  auto rsw = RswScene::Parse(BuildRsw(options));
  REQUIRE(rsw.has_value());
  REQUIRE(rsw->quad_tree().size() == ro::formats::kRswQuadTreeNodeCount);
  const ro::formats::RswQuadTreeNode& first = rsw->quad_tree().front();
  CHECK(first.max == std::array<float, 3>{0.0F, 0.125F, 0.25F});
  CHECK(first.min == std::array<float, 3>{0.375F, 0.5F, 0.625F});
  CHECK(first.half_size == std::array<float, 3>{0.75F, 0.875F, 1.0F});
  CHECK(first.center == std::array<float, 3>{1.125F, 1.25F, 1.375F});
  const ro::formats::RswQuadTreeNode& last = rsw->quad_tree().back();
  CHECK(last.max[0] == 1364.0F);
  CHECK(last.center[2] == 1364.0F + 1.375F);
}

TEST_CASE("RSW : une liste d'objets vide n'empêche pas le quadtree", "[formats][rsw]") {
  RswBuildOptions options;
  options.quad_tree = RankedQuadTree(ro::formats::kRswQuadTreeNodeCount);

  auto rsw = RswScene::Parse(BuildRsw(options));
  REQUIRE(rsw.has_value());
  CHECK(rsw->quad_tree().size() == ro::formats::kRswQuadTreeNodeCount);
}

TEST_CASE("RSW : le quadtree ne se lit qu'en 2.x, mineure non nulle", "[formats][rsw]") {
  struct Version {
    std::uint8_t major = 0;
    std::uint8_t minor = 0;
  };
  constexpr std::array<Version, 2> kWithout = {
      {{.major = 1, .minor = 9}, {.major = 2, .minor = 0}}};
  for (const Version& version : kWithout) {
    RswBuildOptions options;
    options.major = version.major;
    options.minor = version.minor;
    options.quad_tree = RankedQuadTree(ro::formats::kRswQuadTreeNodeCount);

    auto rsw = RswScene::Parse(BuildRsw(options));
    REQUIRE(rsw.has_value());
    CHECK(rsw->quad_tree().empty());
  }
}

TEST_CASE("RSW : un quadtree tronqué, ou après une liste interrompue, se laisse",
          "[formats][rsw]") {
  RswBuildOptions truncated;
  truncated.quad_tree = RankedQuadTree(ro::formats::kRswQuadTreeNodeCount - 1);
  auto short_tree = RswScene::Parse(BuildRsw(truncated));
  REQUIRE(short_tree.has_value());
  CHECK(short_tree->quad_tree().empty());

  RswBuildOptions interrupted;
  interrupted.write_unknown_object = true;
  interrupted.quad_tree = RankedQuadTree(ro::formats::kRswQuadTreeNodeCount);
  auto after_unknown = RswScene::Parse(BuildRsw(interrupted));
  REQUIRE(after_unknown.has_value());
  CHECK(after_unknown->quad_tree().empty());
}
