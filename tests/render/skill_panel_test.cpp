#include "ro/render/skill_panel.h"

#include <array>
#include <cstddef>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// LE GRIMOIRE, côté géométrie : où il se pose, ce qu'un point y désigne, et ce
// qu'il faut peindre. Rien ici ne sait ce qu'est une compétence.
using ro::render::ComposeSkillPanel;
using ro::render::kSkillPanelMarginPixels;
using ro::render::kSkillPanelRowPixels;
using ro::render::kSkillPanelTitlePixels;
using ro::render::kSkillPanelWidthPixels;
using ro::render::SkillPanelHolds;
using ro::render::SkillPanelLayout;
using ro::render::SkillPanelPlacement;
using ro::render::SkillPanelRow;
using ro::render::SkillPanelRowAt;
using ro::render::SkillPanelScrolled;

TEST_CASE("Grimoire : le panneau s'accroche en haut à droite", "[render][panel]") {
  const SkillPanelLayout layout = SkillPanelPlacement(1280.0F, 720.0F, 4);
  CHECK(layout.rows_shown == 4);
  CHECK(layout.origin.x ==
        Catch::Approx(1280.0F - kSkillPanelMarginPixels - kSkillPanelWidthPixels));
  CHECK(layout.origin.y == Catch::Approx(kSkillPanelMarginPixels));
  CHECK(layout.size.x == Catch::Approx(kSkillPanelWidthPixels));
  CHECK(layout.size.y == Catch::Approx(kSkillPanelTitlePixels + 4.0F * kSkillPanelRowPixels));
}

TEST_CASE("Grimoire : une liste plus haute que la fenêtre se coupe", "[render][panel]") {
  // Elle ne déborde PAS : un panneau plus haut que la fenêtre écrirait ses
  // dernières lignes hors de l'écran, où rien ne dirait qu'elles existent. Le
  // titre, lui, dit combien manquent.
  const SkillPanelLayout layout = SkillPanelPlacement(1280.0F, 120.0F, 200);
  CHECK(layout.rows_shown < 200);
  CHECK(layout.origin.y + layout.size.y <= 120.0F);

  // Une fenêtre trop basse pour une seule ligne n'en montre aucune, plutôt
  // qu'un nombre négatif de lignes.
  const SkillPanelLayout crushed = SkillPanelPlacement(1280.0F, 8.0F, 200);
  CHECK(crushed.rows_shown == 0);
}

TEST_CASE("Grimoire : le panneau retient le clic, le titre y compris", "[render][panel]") {
  // C'est ce qui empêche un clic sur le grimoire d'atteindre le sol derrière
  // lui : sans quoi, ouvrir le panneau ferait marcher le personnage sous les
  // lignes qu'on lit.
  const SkillPanelLayout layout = SkillPanelPlacement(1280.0F, 720.0F, 3);
  const glm::vec2 inside{layout.origin.x + 4.0F, layout.origin.y + 4.0F};
  CHECK(SkillPanelHolds(layout, inside));
  // Sur le titre : retenu, mais aucune ligne désignée.
  CHECK_FALSE(SkillPanelRowAt(layout, inside).has_value());

  CHECK_FALSE(SkillPanelHolds(layout, glm::vec2{layout.origin.x - 1.0F, layout.origin.y + 4.0F}));
  CHECK_FALSE(SkillPanelHolds(
      layout, glm::vec2{layout.origin.x + 4.0F, layout.origin.y + layout.size.y + 1.0F}));
}

TEST_CASE("Grimoire : chaque ligne occupe sa bande, et rien au-delà", "[render][panel]") {
  const SkillPanelLayout layout = SkillPanelPlacement(1280.0F, 720.0F, 3);
  const float rows_top = layout.origin.y + kSkillPanelTitlePixels;
  const float middle = layout.origin.x + kSkillPanelWidthPixels * 0.5F;

  for (std::size_t row = 0; row < 3; ++row) {
    const float within = rows_top + static_cast<float>(row) * kSkillPanelRowPixels + 1.0F;
    const auto found = SkillPanelRowAt(layout, glm::vec2{middle, within});
    REQUIRE(found.has_value());
    CHECK(*found == row);
  }

  // Juste sous la dernière ligne : plus rien, et le panneau ne retient pas non
  // plus le clic.
  const glm::vec2 below{middle, layout.origin.y + layout.size.y + 0.5F};
  CHECK_FALSE(SkillPanelRowAt(layout, below).has_value());
  CHECK_FALSE(SkillPanelHolds(layout, below));
}

TEST_CASE("Grimoire : la ligne armée porte son propre fond", "[render][panel]") {
  // La compétence ARMÉE attend qu'on lui désigne une cible : rien d'autre ne
  // dit, entre le choix et le clic suivant, ce que ce clic va faire.
  const SkillPanelLayout layout = SkillPanelPlacement(1280.0F, 720.0F, 2);
  const std::array<SkillPanelRow, 2> rows = {
      SkillPanelRow{"MG_COLDBOLT", "Nv4 12PM", false, false},
      SkillPanelRow{"NV_BASIC", "Nv9", false, true},
  };
  const std::array<SkillPanelRow, 2> armed = {
      SkillPanelRow{"MG_COLDBOLT", "Nv4 12PM", true, false},
      SkillPanelRow{"NV_BASIC", "Nv9", false, true},
  };

  // Sans police gravée, aucun texte ne se pose : ce qui reste est le décor, et
  // c'est justement lui qu'on compte.
  const ro::render::FontAtlas none;
  const std::vector<ro::render::ScreenQuad> plain =
      ComposeSkillPanel(none, layout, "Grimoire (2)", rows, 0);
  const std::vector<ro::render::ScreenQuad> lit =
      ComposeSkillPanel(none, layout, "Grimoire (2)", armed, 0);
  CHECK(lit.size() == plain.size() + 1);

  // Le cadre, le fond, le bandeau : trois quads, et le panneau ne se dessine
  // pas hors de lui-même.
  REQUIRE(plain.size() == 3);
  CHECK(plain[1].min.x == Catch::Approx(layout.origin.x));
  CHECK(plain[1].max.y == Catch::Approx(layout.origin.y + layout.size.y));
}

TEST_CASE("Grimoire : il ne peint que les lignes qu'il montre", "[render][panel]") {
  // Le rang de départ est celui de l'appelant : une liste plus longue que le
  // panneau ne doit pas déborder par le bas.
  const SkillPanelLayout layout = SkillPanelPlacement(1280.0F, 720.0F, 1);
  const std::array<SkillPanelRow, 3> rows = {
      SkillPanelRow{"UN", "", true, false},
      SkillPanelRow{"DEUX", "", true, false},
      SkillPanelRow{"TROIS", "", true, false},
  };
  const ro::render::FontAtlas none;
  // Une seule ligne tient : un seul fond d'armement se peint, sur les trois.
  const std::vector<ro::render::ScreenQuad> shown =
      ComposeSkillPanel(none, layout, "Grimoire 1/3", rows, 0);
  CHECK(shown.size() == 4);

  // Et à partir de la dernière, il n'y a plus qu'elle : rien ne se peint
  // au-delà du tableau.
  const std::vector<ro::render::ScreenQuad> tail =
      ComposeSkillPanel(none, layout, "Grimoire 1/3", rows, 2);
  CHECK(tail.size() == 4);
  const std::vector<ro::render::ScreenQuad> past =
      ComposeSkillPanel(none, layout, "Grimoire 1/3", rows, 9);
  CHECK(past.size() == 3);
}

TEST_CASE("Grimoire : la molette pousse le papier, pas la fenêtre", "[render][panel]") {
  // Un cran vers le HAUT remonte la liste. Sans défilement, les compétences
  // sous le bord du panneau seraient inatteignables — un prêtre en sait plus
  // que la fenêtre n'a de lignes.
  const SkillPanelLayout layout = SkillPanelPlacement(1280.0F, 720.0F, 4);
  REQUIRE(layout.rows_shown == 4);

  CHECK(SkillPanelScrolled(layout, 30, 0, -3.0F) == 3);
  CHECK(SkillPanelScrolled(layout, 30, 10, 4.0F) == 6);

  // Les deux bouts se BORNENT. Remonter au-delà du premier rang déborderait
  // par le bas d'un compteur non signé et sauterait à la fin de la liste.
  CHECK(SkillPanelScrolled(layout, 30, 1, 9.0F) == 0);
  CHECK(SkillPanelScrolled(layout, 30, 26, -9.0F) == 26);

  // Une liste qui tient entière ne défile pas.
  CHECK(SkillPanelScrolled(layout, 4, 0, -5.0F) == 0);
  CHECK(SkillPanelScrolled(layout, 2, 0, -5.0F) == 0);
}

TEST_CASE("Grimoire : un rang gardé se rattrape tout seul", "[render][panel]") {
  // Le rang survit d'une image à l'autre, mais la fenêtre grandit et le
  // grimoire raccourcit. Un pas NUL suffit à le ramener dans les clous, ce qui
  // évite d'avoir à savoir QUAND le rattraper.
  const SkillPanelLayout roomy = SkillPanelPlacement(1280.0F, 720.0F, 40);
  CHECK(SkillPanelScrolled(roomy, 8, 30, 0.0F) == 0);

  const SkillPanelLayout narrow = SkillPanelPlacement(1280.0F, 120.0F, 40);
  REQUIRE(narrow.rows_shown > 0);
  REQUIRE(narrow.rows_shown < 40);
  CHECK(SkillPanelScrolled(narrow, 40, 999, 0.0F) == 40 - narrow.rows_shown);
}
