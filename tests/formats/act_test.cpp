#include "ro/formats/act.h"

#include <cmath>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "support/sprite_builder.h"

using ro::formats::ActAnimation;
using ro::test_support::ActActionSpec;
using ro::test_support::ActAnchorSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;

namespace {

bool NearlyEqual(float actual, float expected) { return std::abs(actual - expected) < 1e-4F; }

// Un calque aux valeurs toutes distinctes : toute permutation de champs se voit.
ActLayerSpec DistinctLayer() {
  return ActLayerSpec{.offset_x = 11.0F,
                      .offset_y = -22.0F,
                      .frame_index = 3,
                      .section = 1,
                      .mirrored = true,
                      .tint = {10, 20, 30, 40},
                      .scale_x = 2.0F,
                      .scale_y = 3.0F,
                      .rotation_degrees = 90};
}

ActBuildOptions OneLayer() {
  ActBuildOptions options;
  options.actions = {ActActionSpec{.frames = {ActFrameSpec{.layers = {DistinctLayer()}}}}};
  options.speeds = {4.0F};
  return options;
}

}  // namespace

TEST_CASE("ACT 2.5 : tous les champs d'un calque sont lus", "[formats][act]") {
  auto animation = ActAnimation::Parse(BuildAct(OneLayer()));
  REQUIRE(animation.has_value());
  CHECK(animation->version() == 25);
  REQUIRE(animation->actions().size() == 1);
  REQUIRE(animation->actions()[0].frames.size() == 1);
  REQUIRE(animation->actions()[0].frames[0].layers.size() == 1);

  const auto& layer = animation->actions()[0].frames[0].layers[0];
  CHECK(NearlyEqual(layer.offset_x, 11.0F));
  CHECK(NearlyEqual(layer.offset_y, -22.0F));
  CHECK(layer.frame_index == 3);
  CHECK(layer.section == 1);
  CHECK(layer.mirrored);
  CHECK(layer.tint == std::array<std::uint8_t, 4>{10, 20, 30, 40});
  CHECK(NearlyEqual(layer.scale_x, 2.0F));
  CHECK(NearlyEqual(layer.scale_y, 3.0F));
  CHECK(layer.rotation_degrees == 90);
}

TEST_CASE("ACT 2.5 : les dimensions déclarées du calque sont gardées telles quelles",
          "[formats][act]") {
  ActBuildOptions options = OneLayer();
  options.actions[0].frames[0].layers[0].declared_width = 38;
  options.actions[0].frames[0].layers[0].declared_height = -41;
  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  const auto& layer = animation->actions()[0].frames[0].layers[0];
  CHECK(layer.declared_width == 38);
  CHECK(layer.declared_height == -41);
}

TEST_CASE("ACT : avant 2.4 une seule échelle vaut pour les deux axes", "[formats][act]") {
  // L'oublier laisse l'échelle verticale à sa valeur par défaut et aplatit le
  // sprite — sans qu'aucun compte de calques ne change.
  ActBuildOptions options = OneLayer();
  options.minor = 3;
  options.actions[0].frames[0].layers[0].scale_x = 2.5F;

  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  const auto& layer = animation->actions()[0].frames[0].layers[0];
  CHECK(NearlyEqual(layer.scale_x, 2.5F));
  CHECK(NearlyEqual(layer.scale_y, 2.5F));
  // Pas de dimensions déclarées avant la 2.5.
  CHECK_FALSE(layer.declared_width.has_value());
  CHECK_FALSE(layer.declared_height.has_value());
}

TEST_CASE("ACT 2.6 : les décalages sont des flottants tronqués", "[formats][act]") {
  // Les lire en entiers donnerait le motif binaire du flottant, donc des
  // positions absurdes plutôt qu'une erreur franche.
  ActBuildOptions options = OneLayer();
  options.minor = 6;
  options.actions[0].frames[0].layers[0].offset_x = 12.75F;
  options.actions[0].frames[0].layers[0].offset_y = -3.25F;

  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  const auto& layer = animation->actions()[0].frames[0].layers[0];
  CHECK(NearlyEqual(layer.offset_x, 12.0F));
  // Troncature vers le BAS, y compris pour un négatif.
  CHECK(NearlyEqual(layer.offset_y, -4.0F));
}

TEST_CASE("ACT : les ancres sont lues, et disent où accrocher", "[formats][act]") {
  ActBuildOptions options = OneLayer();
  options.actions[0].frames[0].anchors = {ActAnchorSpec{.x = 5, .y = -9, .attribute = 1}};

  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  CHECK_FALSE(animation->anchors_dropped());
  REQUIRE(animation->actions()[0].frames[0].anchors.size() == 1);
  const auto& anchor = animation->actions()[0].frames[0].anchors[0];
  CHECK(anchor.x == 5);
  CHECK(anchor.y == -9);
  CHECK(anchor.attribute == 1);
}

TEST_CASE("ACT 2.4 : une lecture d'ancres impossible déclenche le repli", "[formats][act]") {
  // Quelques fichiers 2.3 et 2.4 du jeu ont une table d'ancres inexploitable.
  // Les refuser priverait le client de ces sprites entiers ; le remède est de
  // tout relire SANS les ancres, et de le dire — un composeur averti traite
  // l'absence d'ancre comme un décalage nul plutôt que comme une erreur.
  ActBuildOptions options;
  options.minor = 4;
  options.absurd_anchor_count = true;
  options.actions = {ActActionSpec{.frames = {ActFrameSpec{.layers = {DistinctLayer()}}}}};
  options.speeds = {5.0F};

  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  CHECK(animation->anchors_dropped());
  REQUIRE(animation->actions().size() == 1);
  REQUIRE(animation->actions()[0].frames.size() == 1);
  CHECK(animation->actions()[0].frames[0].layers.size() == 1);
  CHECK(animation->actions()[0].frames[0].anchors.empty());
}

TEST_CASE("ACT 2.5 : pas de repli, l'erreur est rendue", "[formats][act]") {
  // Le repli est réservé aux deux versions où le défaut existe. L'étendre
  // masquerait de vraies corruptions ailleurs.
  ActBuildOptions options;
  options.absurd_anchor_count = true;
  options.actions = {ActActionSpec{.frames = {ActFrameSpec{.layers = {DistinctLayer()}}}}};

  CHECK_FALSE(ActAnimation::Parse(BuildAct(options)).has_value());
}

TEST_CASE("ACT : la cadence se convertit en millisecondes", "[formats][act]") {
  // Le nombre nu ne veut rien dire : c'est un multiple de 24 ms, le pas du
  // client.
  ActBuildOptions options = OneLayer();
  options.speeds = {6.0F};

  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  CHECK(NearlyEqual(animation->actions()[0].speed_units, 6.0F));
  CHECK(NearlyEqual(animation->actions()[0].frame_interval_ms(), 144.0F));
}

TEST_CASE("ACT : sans table de cadences, chaque action prend quatre unités", "[formats][act]") {
  // Une version 2.1 précède la table : 96 ms par image, pas une valeur moyenne
  // des fichiers récents.
  ActFrameSpec frame{.layers = {DistinctLayer()}};
  ActBuildOptions options;
  options.minor = 1;
  options.actions = {ActActionSpec{.frames = {frame}}, ActActionSpec{.frames = {frame}}};

  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  REQUIRE(animation->actions().size() == 2);
  CHECK(NearlyEqual(animation->actions()[0].speed_units, 4.0F));
  CHECK(NearlyEqual(animation->actions()[1].frame_interval_ms(), 96.0F));
}

TEST_CASE("ACT : une table de cadences tronquée laisse le défaut aux actions qu'elle n'atteint pas",
          "[formats][act]") {
  // La lecture n'échoue pas : l'action lue garde sa cadence, les suivantes
  // prennent celle du défaut, et non la dernière lue.
  ActFrameSpec frame{.layers = {DistinctLayer()}};
  ActBuildOptions options;
  options.actions = {ActActionSpec{.frames = {frame}}, ActActionSpec{.frames = {frame}},
                     ActActionSpec{.frames = {frame}}};
  options.speeds = {8.0F};  // une seule cadence pour trois actions

  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  REQUIRE(animation->actions().size() == 3);
  CHECK(NearlyEqual(animation->actions()[0].speed_units, 8.0F));
  CHECK(NearlyEqual(animation->actions()[1].speed_units, 4.0F));
  CHECK(NearlyEqual(animation->actions()[2].speed_units, 4.0F));
}

TEST_CASE("ACT : les noms de sons sont lus dans leur champ fixe", "[formats][act]") {
  ActBuildOptions options = OneLayer();
  options.sound_files = {"atk.wav", "_STOP_"};
  options.actions[0].frames[0].sound_index = 1;

  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  REQUIRE(animation->sound_files().size() == 2);
  CHECK(animation->sound_files()[0] == "atk.wav");
  // La table porte aussi des marqueurs d'animation : tout n'y est pas un son.
  CHECK(animation->sound_files()[1] == "_STOP_");
  CHECK(animation->actions()[0].frames[0].sound_index == 1);
}

TEST_CASE("ACT : un calque vide est conservé", "[formats][act]") {
  // L'index -1 existe réellement ; l'écarter décalerait tous les calques
  // suivants et poserait chaque pièce du personnage au mauvais rang.
  ActBuildOptions options = OneLayer();
  options.actions[0].frames[0].layers.push_back(ActLayerSpec{.frame_index = -1});

  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  REQUIRE(animation->actions()[0].frames[0].layers.size() == 2);
  CHECK(animation->actions()[0].frames[0].layers[1].frame_index == -1);
}

TEST_CASE("ACT : les fichiers malformés sont refusés", "[formats][act]") {
  SECTION("signature absente") {
    std::vector<std::uint8_t> act = BuildAct(OneLayer());
    act[1] = 'X';
    CHECK_FALSE(ActAnimation::Parse(act).has_value());
  }
  SECTION("fichier tronqué en plein calque") {
    std::vector<std::uint8_t> act = BuildAct(OneLayer());
    act.resize(24);
    CHECK_FALSE(ActAnimation::Parse(act).has_value());
  }
  SECTION("fichier vide") { CHECK_FALSE(ActAnimation::Parse({}).has_value()); }
}
