// LE NOM LISIBLE D'UNE CARTE, ET LES LIGNES DE LA FENÊTRE DES DESTINATIONS.
//
// 🔴 CE BANC TIENT TROIS CHOSES QU'UNE PARTIE NE MONTRERAIT PAS SANS UN WARP :
//
//   · la clé de la table se CALCULE depuis le fichier `.gat`, extension et
//     copie d'instance comprises ;
//   · une carte sans nom n'en a pas : c'est l'appelant qui décide ;
//   · « Random.gat », que le serveur envoie pour Teleport, N'EST PAS « Random ».

#include "ro/vfs/map_display_names.h"

#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/vfs/item_words.h"
#include "ro/vfs/vfs.h"
#include "ro/vfs/warp_list_words.h"

#include "support/temp_directory.h"

using ro::test_support::TempDirectory;
using ro::vfs::ItemWords;
using ro::vfs::MapDisplayNames;
using ro::vfs::MapNameKey;
using ro::vfs::WarpDestinationLabel;

namespace {

// La forme que le script écrit : un en-tête, puis les cartes, chacune avec les
// seuls champs qu'elle pose.
constexpr std::string_view kMapNames = R"yaml(# Le nom lisible des cartes
version: 1
maps:
  prontera:
    name: Prontera City, Capital of Rune-Midgard
    announced: true
    subtitle: Rune-Midgard
    title: Prontera
    background: village
  1@tower:
    name: Endless Tower
    announced: true
  nameless:
    title: Silent
  quiet:
    name: Quiet
    background: field
  pvp_n_1-1:
    name: 'PvP : Room Sandwich'
)yaml";

[[nodiscard]] MapDisplayNames Names() {
  auto names = MapDisplayNames::Parse(kMapNames);
  REQUIRE(names.has_value());
  return std::move(*names);
}

}  // namespace

TEST_CASE("Noms de cartes : la clé se calcule depuis le fichier .gat", "[vfs][destinations]") {
  CHECK(MapNameKey("prontera.gat") == "prontera");
  // LA COPIE D'UNE CARTE D'INSTANCE se range sous le nom de son modèle.
  CHECK(MapNameKey("0001@tower.gat") == "1@tower");
  // Un « @ » trop près du début ne retire rien.
  CHECK(MapNameKey("1@tower.gat") == "1@tower");
  // Un nom de moins de cinq caractères garde son extension, un nom sans
  // extension reste entier.
  CHECK(MapNameKey("a.gt") == "a.gt");
  CHECK(MapNameKey("prontera") == "prontera");
}

TEST_CASE("Noms de cartes : la table se lit, et une carte sans nom n'en a pas",
          "[vfs][destinations]") {
  const MapDisplayNames names = Names();
  CHECK(names.size() == 4);
  CHECK(names.DisplayNameOf("prontera.gat") == "Prontera City, Capital of Rune-Midgard");
  CHECK(names.DisplayNameOf("0001@tower.gat") == "Endless Tower");
  CHECK(names.DisplayNameOf("pvp_n_1-1.gat") == "PvP : Room Sandwich");
  // Ce qu'une ligne écrit : le nom lisible et le fichier, ou le fichier seul.
  CHECK(names.LabelOf("pvp_n_1-1") == "PvP : Room Sandwich (pvp_n_1-1)");
  CHECK(names.LabelOf("no_such_map") == "no_such_map");
  // Une carte sans `name`, ou absente, n'a pas de nom : c'est l'appelant qui
  // décide.
  CHECK_FALSE(names.DisplayNameOf("nameless.gat").has_value());
  CHECK_FALSE(names.DisplayNameOf("geffen.gat").has_value());
}

TEST_CASE("Noms de cartes : une table mal formée se refuse", "[vfs][destinations]") {
  CHECK_FALSE(MapDisplayNames::Parse("- une liste\n").has_value());
  CHECK_FALSE(MapDisplayNames::Parse("version: 2\nmaps: {}\n").has_value());
  CHECK_FALSE(MapDisplayNames::Parse("version: 1\n").has_value());
  CHECK_FALSE(MapDisplayNames::Parse("version: 1\nmaps:\n  prontera: Prontera\n").has_value());
  CHECK_FALSE(
      MapDisplayNames::Parse("version: 1\nmaps:\n  prontera:\n    name: [a, b]\n").has_value());
  CHECK_FALSE(MapDisplayNames::Parse("version: 1\nmaps: {\n").has_value());
  CHECK(MapDisplayNames::Parse("version: 1\nmaps: {}\n").has_value());
}

TEST_CASE("Noms de cartes : la table se lit par le VFS", "[vfs][destinations]") {
  TempDirectory client;
  client.WriteText("data/yaml_files/pets.yml", "version: 1\npets: {}\n");
  {
    auto vfs = ro::vfs::Vfs::Mount({.root = client.path()});
    REQUIRE(vfs.has_value());
    CHECK_FALSE(MapDisplayNames::Load(*vfs).has_value());
  }
  client.WriteText("data/yaml_files/map_names.yml", std::string(kMapNames));
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  const auto names = MapDisplayNames::Load(*vfs);
  REQUIRE(names.has_value());
  CHECK(names->DisplayNameOf("prontera.gat") == "Prontera City, Capital of Rune-Midgard");
}

TEST_CASE("Bandeau d'arrivée : le fond, le sous-titre et le titre d'une carte annoncée",
          "[vfs][map_sign]") {
  const MapDisplayNames names = Names();
  const ro::vfs::MapSign* prontera = names.SignOf("prontera.gat");
  REQUIRE(prontera != nullptr);
  CHECK(prontera->background == "village");
  CHECK(prontera->subtitle == "Rune-Midgard");
  CHECK(prontera->title == "Prontera");
  CHECK(prontera->announced);
  // ⚠ ANNONCÉE SANS FOND NI TITRES : l'image par défaut, sans texte. Et la copie
  // d'une instance a le bandeau de son modèle.
  const ro::vfs::MapSign* tower = names.SignOf("0001@tower.gat");
  REQUIRE(tower != nullptr);
  CHECK(tower->background == ro::vfs::kMapSignNoBackground);
  CHECK(tower->subtitle.empty());
  CHECK(tower->title.empty());
}

TEST_CASE("Bandeau d'arrivée : une carte titrée qui ne s'annonce pas le dit", "[vfs][map_sign]") {
  const MapDisplayNames names = Names();
  // 🔴 LE TITRE SEUL DONNE UN BANDEAU, MARQUÉ NON ANNONCÉ : le jeu le tairait,
  // ce client peut le montrer.
  const ro::vfs::MapSign* nameless = names.SignOf("nameless.gat");
  REQUIRE(nameless != nullptr);
  CHECK_FALSE(nameless->announced);
  CHECK(nameless->title == "Silent");
  // Absente : rien.
  CHECK(names.SignOf("geffen.gat") == nullptr);
}

TEST_CASE("Bandeau d'arrivée : sans annonce ni titre, pas de bandeau", "[vfs][map_sign]") {
  const MapDisplayNames names = Names();
  // Un fond seul ne suffit pas.
  CHECK(names.SignOf("quiet.gat") == nullptr);
}

TEST_CASE("Destinations : chaque ligne s'écrit comme chez le client", "[vfs][destinations]") {
  const MapDisplayNames names = Names();
  ItemWords words;
  words.warp_list.random_position = "Random Area";
  words.warp_list.cancel = "Cancel";
  words.warp_list.nowhere = "Random Area";
  CHECK(WarpDestinationLabel(words, &names, "prontera.gat") ==
        "Prontera City, Capital of Rune-Midgard");
  // Le nom EXACT « Random » a son propre mot…
  CHECK(WarpDestinationLabel(words, &names, "Random") == "Random Area");
  // … et « Random.gat », celui que le serveur envoie, passe par la table des
  // cartes et retombe sur le mot d'une destination inconnue.
  CHECK(WarpDestinationLabel(words, &names, "Random.gat") == "Random Area");
  CHECK(WarpDestinationLabel(words, &names, "cancel") == "Cancel");
  // Sans table du tout, toute carte est inconnue.
  CHECK(WarpDestinationLabel(words, nullptr, "prontera.gat") == "Random Area");
}

TEST_CASE("Destinations : chaque cas prend son mot, et les mots sont ceux de l'interface",
          "[vfs][destinations]") {
  // Quatre mots distincts, pour voir quel cas prend lequel.
  ItemWords words;
  words.warp_list.title = "Choisir une destination";
  words.warp_list.random_position = "Au hasard";
  words.warp_list.cancel = "Annuler la liste";
  words.warp_list.nowhere = "Nulle part";
  CHECK(ro::vfs::WarpListTitle(words) == "Choisir une destination");
  CHECK(WarpDestinationLabel(words, nullptr, "Random") == "Au hasard");
  CHECK(WarpDestinationLabel(words, nullptr, "cancel") == "Annuler la liste");
  CHECK(WarpDestinationLabel(words, nullptr, "Random.gat") == "Nulle part");
  // Le remplissage pose ceux de l'interface.
  ItemWords filled;
  ro::vfs::FillMessageWords(filled);
  CHECK(ro::vfs::WarpListTitle(filled) == ro::core::Tr("Choisissez une zone de Warp"));
  CHECK(ro::vfs::WarpListCancelWord(filled) == ro::core::Tr("Annuler"));
  // Sans remplissage, aucun libellé en dur : des textes vides.
  const ItemWords absent;
  CHECK(ro::vfs::WarpListTitle(absent).empty());
  CHECK(ro::vfs::WarpListCancelWord(absent).empty());
  CHECK(WarpDestinationLabel(absent, nullptr, "Random").empty());
  CHECK(WarpDestinationLabel(absent, nullptr, "cancel").empty());
  CHECK(WarpDestinationLabel(absent, nullptr, "prontera.gat").empty());
}
