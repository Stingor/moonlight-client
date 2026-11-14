#include "ro/render/map_sky.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::render::MapClearColorOf;
using ro::render::MapSkyLook;
using ro::render::MapSkyTable;

namespace {

// La table du client, réduite à quelques cartes.
constexpr std::string_view kMiniature = R"(
version: 1
maps:
  gonryun:
    background: [102, 153, 204]
    styles: [1]
  einbroch:
    star: true
    styles: [3, 6, 1]
  moc_akhet:
    background_fog: true
  moc_fild22:
    background_fog: false
    clouds:
      - {count: 400, cull_distance: 400, color: [24, 16, 43]}
      - {count: 200, cull_distance: 400, color: [13, 22, 35]}
)";

[[nodiscard]] MapSkyTable ReadTable(std::string_view text = kMiniature) {
  auto table = MapSkyTable::Parse(text);
  REQUIRE(table.has_value());
  return std::move(*table);
}

[[nodiscard]] std::string WithMap(std::string_view entry) {
  return std::string("version: 1\nmaps:\n  ile: ") + std::string(entry) + "\n";
}

}  // namespace

TEST_CASE("Ciel : la couleur se lit et se ramène à l'unité", "[render][sky]") {
  // La table compte de zéro à 255 ; le rendu attend de zéro à un. Oublier la
  // division peindrait un fond deux cent fois trop clair, donc blanc.
  const MapSkyLook look = ReadTable().LookOf("gonryun");
  REQUIRE(look.background.has_value());
  CHECK((*look.background)[0] == 102.0F / 255.0F);
  CHECK((*look.background)[1] == 153.0F / 255.0F);
  CHECK((*look.background)[2] == 204.0F / 255.0F);
}

TEST_CASE("Ciel : la clé est le nom de la carte, sans extension", "[render][sky]") {
  const MapSkyTable table = ReadTable();
  CHECK(table.map_count() == 4);
  CHECK(table.LookOf("gonryun").background.has_value());
  CHECK_FALSE(table.LookOf("gonryun.rsw").background.has_value());
}

TEST_CASE("Ciel : une carte absente de la table n'en a pas", "[render][sky]") {
  // Le cas ORDINAIRE : la plupart des cartes sont closes, on n'en voit jamais le
  // bord, et elles ne figurent pas dans la table.
  const MapSkyLook look = ReadTable().LookOf("prontera");
  CHECK_FALSE(look.background.has_value());
  CHECK(look.effects.empty());
  CHECK(look.parameterised_clouds == 0);
  CHECK_FALSE(look.background_fog);
}

TEST_CASE("Ciel : une carte que la table ne cite pas s'efface en noir opaque", "[render][sky]") {
  // Comme le jeu. Tout autre fond découperait les cubes noirs que certaines
  // cartes d'intérieur posent hors du champ.
  CHECK(MapClearColorOf(std::nullopt) == std::array<float, 4>{0.0F, 0.0F, 0.0F, 1.0F});
}

TEST_CASE("Ciel : la couleur déclarée efface l'écran, opaque", "[render][sky]") {
  const std::optional<std::array<float, 3>> declared = ReadTable().LookOf("gonryun").background;
  REQUIRE(declared.has_value());
  CHECK(MapClearColorOf(declared) ==
        std::array<float, 4>{(*declared)[0], (*declared)[1], (*declared)[2], 1.0F});
}

TEST_CASE("Ciel : une couleur incomplète refuse la table", "[render][sky]") {
  // Deux composantes ne font pas une couleur. En inventer la troisième peindrait
  // un fond faux que rien ne signalerait.
  CHECK_FALSE(MapSkyTable::Parse(WithMap("{background: [102, 153]}")).has_value());
  CHECK_FALSE(MapSkyTable::Parse(WithMap("{background: [102, 153, 300]}")).has_value());
}

TEST_CASE("Ciel : une table mal formée est refusée", "[render][sky]") {
  CHECK_FALSE(MapSkyTable::Parse("maps: {}").has_value());
  CHECK_FALSE(MapSkyTable::Parse("version: 2\nmaps: {}").has_value());
  CHECK_FALSE(MapSkyTable::Parse("version: 1").has_value());
  CHECK_FALSE(MapSkyTable::Parse(WithMap("{styles: [256]}")).has_value());
  CHECK_FALSE(MapSkyTable::Parse(WithMap("{styles: 1}")).has_value());
  CHECK_FALSE(MapSkyTable::Parse(WithMap("{clouds: 2}")).has_value());
  CHECK_FALSE(MapSkyTable::Parse(WithMap("{rain: true}")).has_value());
  CHECK_FALSE(MapSkyTable::Parse(WithMap("{star: maybe}")).has_value());
  CHECK(MapSkyTable::Parse("version: 1\nmaps: {}").has_value());
}

TEST_CASE("Ciel : un style de nuage désigne son effet", "[render][sky]") {
  // 🔴 LA TABLE MESURÉE, et c'est tout ce que la donnée d'une carte porte : un
  // NUMÉRO DE STYLE. Sans cette traduction, le style 3 ne désigne rien — et
  // c'est elle qui décide si einbroch reçoit sa brume ou les anneaux d'une
  // compétence.
  CHECK(ro::render::SkyEffectIdForStyle(1) == 230);  // les nuages de gonryun
  CHECK(ro::render::SkyEffectIdForStyle(3) == 515);  // la brume d'einbroch
  CHECK(ro::render::SkyEffectIdForStyle(9) == 945);  // le champ d'étoiles
  CHECK(ro::render::SkyEffectIdForStyle(15) == 1145);

  // ⚠ LES TROUS SONT RÉELS, et les combler allumerait un ciel qu'aucune carte ne
  // demande, sur l'effet d'une compétence. Six n'est pas un style de ciel ;
  // douze à quatorze n'existent que semés avec onze, jamais seuls.
  CHECK_FALSE(ro::render::SkyEffectIdForStyle(6).has_value());
  CHECK_FALSE(ro::render::SkyEffectIdForStyle(12).has_value());
  CHECK_FALSE(ro::render::SkyEffectIdForStyle(14).has_value());
  CHECK_FALSE(ro::render::SkyEffectIdForStyle(200).has_value());
}

TEST_CASE("Ciel : les étoiles d'abord, puis les styles dans l'ordre", "[render][sky]") {
  // Le client lance le champ d'étoiles AVANT les styles ; un style sans effet
  // (six) ne lance rien et ne décale rien.
  CHECK(ReadTable().LookOf("einbroch").effects == std::vector<std::uint32_t>{945, 515, 230});
  CHECK(ReadTable().LookOf("gonryun").effects == std::vector<std::uint32_t>{230});
}

TEST_CASE("Ciel : les nuages paramétrés se comptent", "[render][sky]") {
  CHECK(ReadTable().LookOf("moc_fild22").parameterised_clouds == 2);
  CHECK(ReadTable().LookOf("gonryun").parameterised_clouds == 0);
}

TEST_CASE("Ciel : le fond se mêle au brouillard quand la table le dit", "[render][sky]") {
  CHECK(ReadTable().LookOf("moc_akhet").background_fog);
  // ⚠ FAUX ET ABSENT DISENT LA MÊME CHOSE : le client rend faux faute de valeur.
  CHECK_FALSE(ReadTable().LookOf("moc_fild22").background_fog);
  CHECK_FALSE(ReadTable().LookOf("gonryun").background_fog);
}

TEST_CASE("Ciel : le style des étoiles n'est pas dans la table de la carte", "[render][sky]") {
  // Le client ne lit pas de numéro pour les étoiles : il appelle son lanceur
  // avec NEUF en dur quand la carte dit « star ». Le chiffre vit donc chez
  // nous, et il doit désigner un effet.
  CHECK(ro::render::SkyEffectIdForStyle(ro::render::kStarSkyStyle).has_value());
}
