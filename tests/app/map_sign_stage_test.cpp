// LE BANDEAU DU NOM DE LA CARTE, côté `app` : ce qu'une carte chargée allume
// selon le réglage, et quand il s'éteint.

#include "world/map_sign_stage.h"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"

namespace {

using moonclient::MapSignMode;

constexpr std::uint32_t kArrivalMs = 10'000;

// Une ville classique : un titre, mais la table ne l'annonce pas.
const ro::vfs::MapSign kTown{.background = "village", .title = "Prontera", .announced = false};
// Une carte récente, annoncée.
const ro::vfs::MapSign kRecent{.background = "field_s2", .title = "Od Canyon", .announced = true};
// Annoncée, mais sans un mot : l'image seule.
const ro::vfs::MapSign kSilent{.background = "noname", .announced = true};

}  // namespace

TEST_CASE("Bandeau de carte : l'image et la clé de la table", "[app][map_sign]") {
  CHECK(moonclient::MapSignImagePath("village") ==
        std::string(ro::render::kSkinRoot) + "display_mapname\\village.png");
  // La scène porte le nom sans extension ; la table se consulte par le fichier.
  CHECK(moonclient::MapSignFileOf("prontera") == "prontera.gat");
}

TEST_CASE("Bandeau de carte : ce que chaque réglage montre", "[app][map_sign]") {
  // 🔴 LE DÉFAUT MONTRE LES VILLES : la table livrée n'en annonce aucune.
  CHECK(moonclient::MapSignWanted(kTown, MapSignMode::kEveryTitled));
  CHECK(moonclient::MapSignWanted(kRecent, MapSignMode::kEveryTitled));
  CHECK(moonclient::MapSignWanted(kSilent, MapSignMode::kEveryTitled));
  // Comme le jeu : l'annonce seule compte.
  CHECK_FALSE(moonclient::MapSignWanted(kTown, MapSignMode::kLikeGame));
  CHECK(moonclient::MapSignWanted(kRecent, MapSignMode::kLikeGame));
  CHECK(moonclient::MapSignWanted(kSilent, MapSignMode::kLikeGame));
  CHECK_FALSE(moonclient::MapSignWanted(kRecent, MapSignMode::kNever));
}

TEST_CASE("Bandeau de carte : il vit cinq secondes et un quart, puis s'éteint", "[app][map_sign]") {
  moonclient::MapSignStage stage;
  stage.Enter(&kRecent, MapSignMode::kEveryTitled, kArrivalMs);
  REQUIRE(stage.showing());
  // Sans images, rien ne se dessine, mais le bandeau court toujours.
  CHECK_FALSE(stage.Stage(nullptr, kArrivalMs + ro::ui::kMapSignShownMs).has_value());
  CHECK(stage.showing());
  // ⚠ LE COMPTE PART DE L'ARRIVÉE, pas du premier dessin.
  CHECK_FALSE(stage.Stage(nullptr, kArrivalMs + ro::ui::kMapSignShownMs + ro::ui::kMapSignFadeMs)
                  .has_value());
  CHECK_FALSE(stage.showing());
}

TEST_CASE("Bandeau de carte : une heure d'image antérieure à l'arrivée ne l'éteint pas",
          "[app][map_sign]") {
  // 🔴 LE SALON PREND L'HEURE DE L'IMAGE AVANT DE MONTER LA CARTE, et l'arrivée
  // après le montage : l'image suivante lit donc le bandeau avec une heure plus
  // ANCIENNE que son départ. Soustraite telle quelle, elle débordait en un
  // temps écoulé immense, et le bandeau se croyait fini avant d'avoir paru.
  moonclient::MapSignStage stage;
  stage.Enter(&kTown, MapSignMode::kEveryTitled, kArrivalMs);
  constexpr std::uint32_t kMountMs = 138;
  CHECK_FALSE(stage.Stage(nullptr, kArrivalMs - kMountMs).has_value());
  CHECK(stage.showing());
  // Et le compte part toujours de l'arrivée.
  CHECK_FALSE(stage.Stage(nullptr, kArrivalMs + ro::ui::kMapSignShownMs + ro::ui::kMapSignFadeMs)
                  .has_value());
  CHECK_FALSE(stage.showing());
}

TEST_CASE("Bandeau de carte : une carte écartée éteint celui d'avant", "[app][map_sign]") {
  moonclient::MapSignStage stage;
  stage.Enter(&kRecent, MapSignMode::kEveryTitled, kArrivalMs);
  REQUIRE(stage.showing());
  // Une ville, sous « comme le jeu » : pas de bandeau, et l'ancien s'en va.
  stage.Enter(&kTown, MapSignMode::kLikeGame, kArrivalMs + 1000);
  CHECK_FALSE(stage.showing());
  stage.Enter(&kRecent, MapSignMode::kEveryTitled, kArrivalMs + 2000);
  stage.Enter(nullptr, MapSignMode::kEveryTitled, kArrivalMs + 3000);
  CHECK_FALSE(stage.showing());
}
