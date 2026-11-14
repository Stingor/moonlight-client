// LA CARTE DU MONDE : l'image ajustée à la fenêtre, et la carte sous un point.

#include "ro/ui/world_map_window.h"

#include <array>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using ro::ui::FitWorldMap;
using ro::ui::WorldMapAreaAt;
using ro::ui::WorldMapAreaView;

TEST_CASE("Carte du monde : l'image tient dans la place, centrée, sans déformation",
          "[ui][worldmap]") {
  // Une place deux fois plus large que haute : la hauteur borne.
  const ro::ui::WorldMapFit wide =
      FitWorldMap(ImVec2(1280.0F, 1024.0F), ImVec2(0.0F, 100.0F), ImVec2(2000.0F, 612.0F));
  CHECK(wide.scale == Approx(0.5F));
  CHECK(wide.origin.y == Approx(100.0F));
  // Centrée en largeur : (2000 - 640) / 2.
  CHECK(wide.origin.x == Approx(680.0F));
  // Une place vide n'ajuste rien.
  CHECK(FitWorldMap(ImVec2(1280.0F, 1024.0F), ImVec2(5.0F, 5.0F), ImVec2(5.0F, 5.0F)).scale ==
        0.0F);
}

TEST_CASE("Carte du monde : sous un point, la plus petite carte qui le contient",
          "[ui][worldmap]") {
  const std::array<WorldMapAreaView, 3> areas = {
      WorldMapAreaView{.rect = {.left = 0, .top = 0, .right = 100, .bottom = 100},
                       .map_name = "champ"},
      WorldMapAreaView{.rect = {.left = 40, .top = 40, .right = 60, .bottom = 60},
                       .map_name = "ville"},
      WorldMapAreaView{.rect = {.left = 200, .top = 0, .right = 300, .bottom = 100},
                       .map_name = "loin"},
  };
  CHECK(WorldMapAreaAt(areas, ImVec2(50.0F, 50.0F)) == 1U);
  CHECK(WorldMapAreaAt(areas, ImVec2(10.0F, 10.0F)) == 0U);
  CHECK(WorldMapAreaAt(areas, ImVec2(250.0F, 50.0F)) == 2U);
  CHECK_FALSE(WorldMapAreaAt(areas, ImVec2(150.0F, 50.0F)).has_value());
}

TEST_CASE("Carte du monde : un trait texturé, l'image en travers", "[ui][worldmap]") {
  const auto band =
      ro::ui::WorldMapBand(ImVec2(0.0F, 0.0F), ImVec2(10.0F, 0.0F), 2.0F, IM_COL32_WHITE);
  REQUIRE(band.has_value());
  // Haut gauche et haut droit au départ, de part et d'autre du trait.
  CHECK(band->corners[0].y == Approx(-2.0F));
  CHECK(band->corners[1].y == Approx(2.0F));
  CHECK(band->corners[2].x == Approx(10.0F));
  CHECK(band->corners[3].y == Approx(2.0F));
  CHECK_FALSE(ro::ui::WorldMapBand(ImVec2(5.0F, 5.0F), ImVec2(5.0F, 5.0F), 2.0F, IM_COL32_WHITE)
                  .has_value());
}

TEST_CASE("Carte du monde : un dirigeable garde son circuit et boucle", "[ui][worldmap]") {
  using ro::ui::WorldMapAirStop;
  using ro::ui::WorldMapAirTour;
  const WorldMapAirStop here{.rect = {.left = 0, .top = 0, .right = 0, .bottom = 0},
                             .map_name = "ici"};
  const WorldMapAirStop there{.rect = {.left = 200, .top = 0, .right = 200, .bottom = 0},
                              .map_name = "la"};
  const std::array<WorldMapAirStop, 3> from_here = {here, there, here};
  const std::array<WorldMapAirStop, 3> from_there = {there, here, there};
  const std::array<WorldMapAirTour, 2> tours = {
      WorldMapAirTour{.start = "ici", .stops = from_here},
      WorldMapAirTour{.start = "la", .stops = from_there}};
  const ro::ui::WorldMapAirLine line{.tours = tours};
  ro::ui::WorldMapAirState state;

  CHECK_FALSE(ro::ui::AdvanceWorldMapAir(state, line, "ailleurs", 0.5F, 1.0F).has_value());
  // Deux cents pixels par seconde, sur une étape de deux cents.
  CHECK(ro::ui::AdvanceWorldMapAir(state, line, "ici", 0.5F, 1.0F) == Approx(0.5F));
  CHECK(state.tour == 0);
  // Passer sur l'autre escale garde le circuit choisi.
  CHECK(ro::ui::AdvanceWorldMapAir(state, line, "la", 0.5F, 1.0F) == Approx(1.0F));
  CHECK(state.tour == 0);
  CHECK(ro::ui::AdvanceWorldMapAir(state, line, "la", 0.25F, 1.0F) == Approx(1.25F));
  // L'étape dépassée, la suivante commence.
  CHECK(ro::ui::AdvanceWorldMapAir(state, line, "la", 0.25F, 1.0F) == Approx(0.25F));
  CHECK(state.leg == 1U);
  CHECK(ro::ui::AdvanceWorldMapAir(state, line, "la", 1.0F, 1.0F) == Approx(1.25F));
  // Après la dernière, la première.
  CHECK(ro::ui::AdvanceWorldMapAir(state, line, "la", 0.1F, 1.0F) == Approx(0.1F));
  CHECK(state.leg == 0U);
  // La souris partie, le vol se pose.
  CHECK_FALSE(ro::ui::AdvanceWorldMapAir(state, line, "", 0.1F, 1.0F).has_value());
  CHECK(state.tour == -1);
}

TEST_CASE("Carte du monde : la trace derrière le dirigeable, et l'appareil devant",
          "[ui][worldmap]") {
  const ro::ui::WorldMapFlight half =
      ro::ui::FlyWorldMapLeg(ImVec2(0.0F, 0.0F), ImVec2(100.0F, 0.0F), 0.5F);
  // Deux empreintes de seize pixels, espacées de huit, à six pixels du trait.
  REQUIRE(half.trail.size() == 2U);
  CHECK(half.trail[0].corners[0].x == Approx(0.0F));
  CHECK(half.trail[0].corners[0].y == Approx(-6.0F));
  CHECK(half.trail[0].corners[3].x == Approx(16.0F));
  CHECK(half.trail[0].corners[3].y == Approx(10.0F));
  CHECK(half.trail[1].corners[0].x == Approx(24.0F));
  // Chacune plus nette que la précédente.
  CHECK(half.trail[0].color == IM_COL32(255, 255, 255, 0x33));
  CHECK(half.trail[1].color == IM_COL32(255, 255, 255, 0x66));
  // L'appareil : quarante pixels, le haut de l'image vers l'escale suivante.
  REQUIRE(half.airship.has_value());
  CHECK(half.airship->corners[0].x == Approx(88.0F));
  CHECK(half.airship->corners[0].y == Approx(22.0F));
  CHECK(half.airship->corners[3].x == Approx(48.0F));
  CHECK(half.airship->corners[3].y == Approx(-18.0F));
  // L'étape achevée, l'appareil disparaît.
  CHECK_FALSE(
      ro::ui::FlyWorldMapLeg(ImVec2(0.0F, 0.0F), ImVec2(100.0F, 0.0F), 1.0F).airship.has_value());
}

TEST_CASE("Carte du monde : le palier de taille suit la largeur de l'image", "[ui][worldmap]") {
  CHECK(ro::ui::WorldMapSizeTier(640.0F) == 0U);
  CHECK(ro::ui::WorldMapSizeTier(641.0F) == 1U);
  CHECK(ro::ui::WorldMapSizeTier(800.0F) == 1U);
  CHECK(ro::ui::WorldMapSizeTier(1024.0F) == 2U);
  CHECK(ro::ui::WorldMapSizeTier(1920.0F) == 3U);
}

TEST_CASE("Carte du monde : l'aperçu se pose à droite, ou à gauche quand la place manque",
          "[ui][worldmap]") {
  const ImVec2 screen(1000.0F, 800.0F);
  const ImVec2 right =
      ro::ui::WorldMapPreviewAt(ImVec2(100.0F, 100.0F), ImVec2(200.0F, 150.0F), screen);
  CHECK(right.x == Approx(210.0F));
  CHECK(right.y == Approx(110.0F));
  // Trop près du bord droit : à gauche de la carte ; trop bas : remonté.
  const ImVec2 left =
      ro::ui::WorldMapPreviewAt(ImVec2(800.0F, 700.0F), ImVec2(900.0F, 750.0F), screen);
  CHECK(left.x == Approx(660.0F));
  CHECK(left.y == Approx(670.0F));
}

TEST_CASE("Carte du monde : la liste du groupe se range à côté de l'aperçu", "[ui][worldmap]") {
  const ImVec2 screen(1000.0F, 800.0F);
  const ImVec2 size(80.0F, 40.0F);
  // L'aperçu à droite de la carte du groupe : la liste à sa droite, plus bas.
  const ImVec2 beside = ro::ui::WorldMapMemberListAt(ImVec2(210.0F, 110.0F), ImVec2(100.0F, 100.0F),
                                                     ImVec2(200.0F, 150.0F), size, screen);
  CHECK(beside.x == Approx(350.0F));
  CHECK(beside.y == Approx(127.0F));
  // L'aperçu à gauche : la liste à sa gauche, à sa hauteur.
  const ImVec2 left = ro::ui::WorldMapMemberListAt(ImVec2(660.0F, 110.0F), ImVec2(800.0F, 100.0F),
                                                   ImVec2(900.0F, 150.0F), size, screen);
  CHECK(left.x == Approx(570.0F));
  CHECK(left.y == Approx(110.0F));
}

TEST_CASE("Carte du monde : le Warp Agent se tient au coin de la ville, son nom à côté",
          "[ui][worldmap]") {
  ro::ui::WorldMapWarpAgentView agent;
  agent.sprite_feet = ImVec2(20.0F, 55.0F);
  agent.sprite_height = 50.0F;
  const ro::ui::WorldMapRect town{.left = 100, .top = 200, .right = 160, .bottom = 240};
  const auto placed = ro::ui::PlaceWorldMapWarpAgent(agent, town, 0.0F, 12.0F);
  // Les pieds sur le coin haut droit de la ville.
  CHECK(placed.sprite_corner.x == Approx(140.0F));
  CHECK(placed.sprite_corner.y == Approx(145.0F));
  // Le nom à quatre pixels à droite, centré sur la hauteur de la figure.
  CHECK(placed.label_at.x == Approx(164.0F));
  CHECK(placed.label_at.y == Approx(169.0F));
  // Trop près du haut de l'image : la figure descend pour y tenir entière.
  const ro::ui::WorldMapRect high{.left = 100, .top = 10, .right = 160, .bottom = 50};
  const auto lowered = ro::ui::PlaceWorldMapWarpAgent(agent, high, 0.0F, 12.0F);
  CHECK(lowered.sprite_corner.y == Approx(1.0F));
  CHECK(lowered.label_at.y == Approx(25.0F));
}
