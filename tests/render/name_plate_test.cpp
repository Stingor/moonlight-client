// LES LIGNES D'UNE ÉTIQUETTE D'ACTEUR.
//
// 🔴 CE BANC FIXE LA COMPOSITION DU CLIENT, pas une mise en forme choisie.
// Le client met le titre DEVANT entre
// crochets, l'aside entre parenthèses, et ne fait une seconde ligne que si la
// BANNIÈRE est là — l'insigne seul ne suffit pas, ce qui prive un membre de
// clan de sa ligne. Chacune de ces trois règles se devine faussement dans
// l'autre sens, et rien à l'écran ne dirait laquelle est la bonne.

#include "ro/render/name_plate.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::render::ComposeNamePlateLines;
using ro::render::NamePlateLook;
using ro::render::NamePlateText;

namespace {

constexpr NamePlateLook kFull = {.korean_service = false};
constexpr NamePlateLook kKorean = {.korean_service = true};

}  // namespace

TEST_CASE("Étiquette : le titre passe DEVANT, entre crochets", "[render][nameplate]") {
  NamePlateText what;
  what.name = "Stingor";
  what.title = "Grand Master";
  const std::vector<std::string> lines = ComposeNamePlateLines(what, kFull);
  REQUIRE(lines.size() == 1);
  CHECK(lines[0] == "[Grand Master] Stingor");
}

TEST_CASE("Étiquette : deux lignes, l'aside et l'insigne à leur place", "[render][nameplate]") {
  NamePlateText what;
  what.name = "Stingor";
  what.aside = "Les Lunaires";
  what.banner = "Moonlight-Destiny";
  what.badge = "Handyman";
  const std::vector<std::string> lines = ComposeNamePlateLines(what, kFull);
  REQUIRE(lines.size() == 2);
  CHECK(lines[0] == "Stingor (Les Lunaires)");
  CHECK(lines[1] == "Moonlight-Destiny [Handyman]");
}

TEST_CASE("Étiquette : la vie d'un monstre sort par la PLACE de l'aside", "[render][nameplate]") {
  // ⚠ C'EST LE SERVEUR QUI DÉTOURNE SES CHAMPS, et le client ne fait que les
  // écrire à leur place. Un lecteur qui croirait lire un groupe rangerait
  // « Lv. 12 | HP: 63% » dans la fenêtre de groupe.
  NamePlateText what;
  what.name = "Poring";
  what.aside = "Lv. 12 | HP: 63%";
  what.banner = "Plant";
  what.badge = "Water";
  const std::vector<std::string> lines = ComposeNamePlateLines(what, kFull);
  REQUIRE(lines.size() == 2);
  CHECK(lines[0] == "Poring (Lv. 12 | HP: 63%)");
  CHECK(lines[1] == "Plant [Water]");
}

TEST_CASE("Étiquette : l'insigne SEUL ne fait pas de ligne", "[render][nameplate]") {
  // Le client ne regarde que la bannière pour décider d'écrire la seconde
  // ligne. Un membre de CLAN, dont le serveur ne remplit que l'insigne, ne
  // montre donc rien de son clan — c'est le client qui est ainsi, et le
  // corriger nous éloignerait de ce que le joueur voit à côté.
  NamePlateText what;
  what.name = "Stingor";
  what.badge = "Sword Clan";
  const std::vector<std::string> lines = ComposeNamePlateLines(what, kFull);
  REQUIRE(lines.size() == 1);
  CHECK(lines[0] == "Stingor");
}

// 🔴 LE DRAPEAU QUI COMMANDE LA COUPE EST VRAI HORS DES MÉTIERS DE JOUEUR.
// C'est donc TOUT CE QUI N'EST PAS UN JOUEUR qui se coupe au croisillon, et un
// joueur garde son nom entier.
//
// ⚠ ET CELA SE VOYAIT EN JEU : sept mille cent quarante et un PNJ de ce serveur
// portent un nom de script croisillonné, et le joueur lisait l'identifiant que
// l'auteur du script s'était écrit à lui-même.
TEST_CASE("Étiquette : ce qui n'est PAS un joueur se coupe au croisillon", "[render][nameplate]") {
  NamePlateText pnj;
  pnj.name = "Kafra Employee#alde";
  pnj.cut_at_hash = true;
  CHECK(ComposeNamePlateLines(pnj, kFull)[0] == "Kafra Employee");

  NamePlateText monstre;
  monstre.name = "Poring#2";
  monstre.cut_at_hash = true;
  CHECK(ComposeNamePlateLines(monstre, kFull)[0] == "Poring");

  // Un JOUEUR garde le sien entier — son nom n'a pas de partie cachée.
  NamePlateText player_plate;
  player_plate.name = "Stingor#2";
  CHECK(ComposeNamePlateLines(player_plate, kFull)[0] == "Stingor#2");
}

TEST_CASE("Étiquette : le service CORÉEN met la parenthèse aux deux lignes",
          "[render][nameplate]") {
  // 🔴 ET C'EST CE QUE DÉCLARE CETTE INSTALLATION. Le type de service vaut zéro
  // pour `korea`, et le client du joueur compose alors « %s (%s) », espace
  // finale comprise, pour le nom comme pour la bannière. Le niveau et la vie
  // d'un monstre se lisent donc bien.
  NamePlateText what;
  what.name = "Poring";
  what.aside = "Lv. 12 | HP: 63%";
  what.banner = "Plant";
  what.badge = "Water";
  const std::vector<std::string> lines = ComposeNamePlateLines(what, kKorean);
  REQUIRE(lines.size() == 2);
  CHECK(lines[0] == "Poring (Lv. 12 | HP: 63%) ");
  CHECK(lines[1] == "Plant (Water) ");
}

TEST_CASE("Étiquette : l'insigne vide s'écrit quand même, pas l'aside vide",
          "[render][nameplate]") {
  // Le client ne regarde que la bannière pour écrire la seconde ligne, et la
  // compose avec ses deux champs ; la première, elle, ne prend sa suite que si
  // elle en a une.
  NamePlateText what;
  what.name = "Stingor";
  what.banner = "Moonlight-Destiny";
  const std::vector<std::string> korean = ComposeNamePlateLines(what, kKorean);
  REQUIRE(korean.size() == 2);
  CHECK(korean[0] == "Stingor");
  CHECK(korean[1] == "Moonlight-Destiny () ");
  const std::vector<std::string> elsewhere = ComposeNamePlateLines(what, kFull);
  REQUIRE(elsewhere.size() == 2);
  CHECK(elsewhere[1] == "Moonlight-Destiny []");
}

TEST_CASE("Étiquette : sans nom, rien ne s'écrit", "[render][nameplate]") {
  NamePlateText what;
  what.banner = "Moonlight-Destiny";
  const std::vector<std::string> lines = ComposeNamePlateLines(what, kFull);
  // La bannière tient seule : le client écrit sa seconde ligne sans regarder la
  // première.
  REQUIRE(lines.size() == 1);
  CHECK(lines[0] == "Moonlight-Destiny []");

  CHECK(ComposeNamePlateLines(NamePlateText{}, kFull).empty());
}

// 🔴 LA COULEUR D'ÉCART DE NIVEAU EST LA SURCHARGE LA PLUS VOYANTE DU CLIENT,
// et elle passe pour la marque d'un boss : un monstre de niveau cinquante
// s'écrit en rouge devant un personnage de trente, boss ou pas. Ce qui la
// distingue vraiment d'un boss, c'est l'insigne, qui ne dépend que du rang.
TEST_CASE("Étiquette : l'écart de niveau écrase la couleur d'espèce", "[render][nameplate]") {
  using ro::render::kMonsterNameColor;
  using ro::render::kOverLevelNameColor;
  using ro::render::kUnderLevelNameColor;
  using ro::render::NameColorForLevelGap;

  // Onze niveaux au-dessus de moi : rouge. Dix seulement : la couleur d'espèce
  // tient — le client compare `< -10`, et non `<= -10`.
  CHECK(NameColorForLevelGap(kMonsterNameColor, 30, 41) == kOverLevelNameColor);
  CHECK(NameColorForLevelGap(kMonsterNameColor, 30, 40) == kMonsterNameColor);

  // Seize niveaux en dessous : gris. Quinze : la couleur d'espèce tient.
  CHECK(NameColorForLevelGap(kMonsterNameColor, 50, 34) == kUnderLevelNameColor);
  CHECK(NameColorForLevelGap(kMonsterNameColor, 50, 35) == kMonsterNameColor);

  // ⚠ UN NIVEAU INCONNU N'EST PAS UN NIVEAU ZÉRO. Sans cette garde, tout
  // monstre dont le serveur tait le niveau se peindrait en gris, et tout
  // monstre croisé avant que notre propre niveau n'arrive, en rouge.
  CHECK(NameColorForLevelGap(kMonsterNameColor, 50, 0) == kMonsterNameColor);
  CHECK(NameColorForLevelGap(kMonsterNameColor, 0, 99) == kMonsterNameColor);
}

// 🔴 CES COULEURS SONT DES `COLORREF`, DONC LE ROUGE EN DERNIER. Le client les
// passe à `SetTextColor` de GDI ; la valeur `0xC3C3FF` est donc un ROSE. La
// preuve est une capture du client d'origine : la surcharge
// d'écart de niveau y écrit `255`, et le texte sort en `#FF0000` au pixel — ce
// que seule cette lecture explique.
TEST_CASE("Étiquette : les couleurs se lisent à l'envers du désassemblage", "[render][nameplate]") {
  CHECK(ro::render::kOverLevelNameColor == std::array<std::uint8_t, 4>{0xFF, 0x00, 0x00, 255});
  CHECK(ro::render::kMonsterNameColor == std::array<std::uint8_t, 4>{0xFF, 0xC3, 0xC3, 255});
  CHECK(ro::render::kHostileNameColor == std::array<std::uint8_t, 4>{0x95, 0xB8, 0xF7, 255});
}
