#include "ro/ui/doll_gif.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("GIF du pantin : le nom proposé dit qui, quelle pose et quelle direction",
          "[ui][doll_gif]") {
  CHECK(ro::ui::DollGifFileName("Sting", "Combat", 3) == "avatar_Sting_Combat_d3.gif");
}

TEST_CASE("GIF du pantin : ce que le système refuse devient un souligné", "[ui][doll_gif]") {
  // Un pseudo est libre, une pose traduite porte des espaces et des
  // parenthèses : seuls les caractères interdits et l'espace sont remplacés.
  CHECK(ro::ui::DollGifFileName("A/B:C*D", "Marche (animé)", 0) ==
        "avatar_A_B_C_D_Marche_(animé)_d0.gif");
  CHECK(ro::ui::DollGifFileName("x\\y?\"<>|", "Repos", 7) == "avatar_x_y______Repos_d7.gif");
}

TEST_CASE("GIF du pantin : sans pseudo, « perso »", "[ui][doll_gif]") {
  CHECK(ro::ui::DollGifFileName("", "Assis", 1) == "avatar_perso_Assis_d1.gif");
}
