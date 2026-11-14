// LE MENU DE MORT S'OUVRE SEUL — et rend ce qu'il a pris, pas davantage.
//
// ⚠ ÉPROUVÉ SUR `FollowDeath` ET NON SUR LE DESSIN : ce qui peut se tromper
// ici est un FRANCHISSEMENT, pas un pixel. Quatre branches, et chacune se
// trompe en silence — un menu qui se rouvre à chaque image ne se referme plus,
// un menu qu'on referme sous le joueur lui prend son écran.

#include "ro/ui/game_menu.h"

#include <catch2/catch_test_macros.hpp>

namespace {

// Une partie ordinaire : vivant, menu fermé, et le souvenir en accord.
[[nodiscard]] ro::ui::GameMenuState Alive() {
  ro::ui::GameMenuState state;
  ro::ui::FollowDeath(state, false);
  return state;
}

}  // namespace

TEST_CASE("Menu : la mort l'ouvre", "[ui][menu]") {
  ro::ui::GameMenuState state = Alive();
  CHECK_FALSE(state.open);

  ro::ui::FollowDeath(state, true);
  CHECK(state.open);
}

TEST_CASE("Menu : refermé mort, il le reste", "[ui][menu]") {
  // 🔴 LE CAS QUI DISTINGUE UN FRANCHISSEMENT D'UN ÉTAT. Un menu rouvert tant
  // que le personnage est à terre serait impossible à fermer, et le joueur ne
  // verrait plus l'endroit où il est tombé — ce qu'on regarde justement en
  // attendant un soigneur.
  ro::ui::GameMenuState state = Alive();
  ro::ui::FollowDeath(state, true);
  state.open = false;

  for (int frame = 0; frame < 3; ++frame) {
    ro::ui::FollowDeath(state, true);
    CHECK_FALSE(state.open);
  }
}

TEST_CASE("Menu : la résurrection referme ce que la mort avait ouvert", "[ui][menu]") {
  ro::ui::GameMenuState state = Alive();
  ro::ui::FollowDeath(state, true);
  REQUIRE(state.open);

  ro::ui::FollowDeath(state, false);
  CHECK_FALSE(state.open);
}

TEST_CASE("Menu : la résurrection ne referme pas ce que le joueur avait ouvert", "[ui][menu]") {
  // ⚠ MOURIR MENU OUVERT NE NOUS EN REND PAS PROPRIÉTAIRE : il était au joueur
  // avant la mort, il lui appartient encore après.
  ro::ui::GameMenuState state = Alive();
  state.open = true;
  ro::ui::FollowDeath(state, true);

  ro::ui::FollowDeath(state, false);
  CHECK(state.open);
}

TEST_CASE("Menu : rouvert à la main, il survit à la résurrection", "[ui][menu]") {
  // L'autre bout du même partage : la mort l'ouvre, le joueur le ferme, puis
  // le rouvre par Échap. Ce second geste est le SIEN.
  ro::ui::GameMenuState state = Alive();
  ro::ui::FollowDeath(state, true);
  state.open = false;
  ro::ui::FollowDeath(state, true);
  state.open = true;
  ro::ui::FollowDeath(state, true);

  ro::ui::FollowDeath(state, false);
  CHECK(state.open);
}

TEST_CASE("Menu : deux morts de suite l'ouvrent deux fois", "[ui][menu]") {
  // Le souvenir se remet à zéro au retour à la vie, sans quoi la mort suivante
  // ne serait plus un franchissement.
  ro::ui::GameMenuState state = Alive();
  for (int life = 0; life < 2; ++life) {
    ro::ui::FollowDeath(state, true);
    CHECK(state.open);
    ro::ui::FollowDeath(state, false);
    CHECK_FALSE(state.open);
  }
}
