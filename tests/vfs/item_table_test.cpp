#include "ro/vfs/item_table.h"

#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/vfs.h"

#include "support/temp_directory.h"

using ro::test_support::TempDirectory;
using ro::vfs::ItemEntry;
using ro::vfs::ItemTable;
using ro::vfs::ParseItemTable;
using ro::vfs::ReadItemTable;
using ro::vfs::Vfs;

namespace {

// Une installation nue : juste de quoi monter.
//
// ⚠ Un montage sans archive exige un dossier `data` : sans lui, il n'y a rien
// à monter et le montage échoue.
void MakeClient(const TempDirectory& client) { client.WriteText("data/marqueur.txt", ""); }

void PutTable(const TempDirectory& client, std::string_view items) {
  client.WriteText("data/yaml_files/item_info.yml",
                   "version: 1\nitems: {\n" + std::string(items) + "}\n");
}

[[nodiscard]] ItemTable Read(const TempDirectory& client) {
  auto vfs = Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  auto table = ReadItemTable(*vfs);
  REQUIRE(table.has_value());
  return std::move(*table);
}

}  // namespace

TEST_CASE("Table des objets : une entrée complète se lit champ par champ", "[vfs][objets]") {
  TempDirectory client;
  MakeClient(client);
  PutTable(client,
           "  1201: {name: \"Knife\", resource: \"knife\", slots: 3, view: 1, "
           "description: [\"Une lame courte.#\", \"^0000CCPoids :^000000 40\"]}\n");

  const ItemTable table = Read(client);
  REQUIRE(table.size() == 1);
  const ItemEntry* knife = table.Find(1201);
  REQUIRE(knife != nullptr);
  CHECK(knife->name == "Knife");
  CHECK(knife->resource == "knife");
  CHECK(knife->card_slots == 3);
  CHECK(knife->view_id == 1);
  CHECK(table.ViewOf(1201) == 1);
  CHECK(table.ResourceOf(1201) == "knife");
  // 🔴 TROIS LIGNES PRÉCÈDENT CELLES DE LA TABLE, et elles ne sont dans AUCUNE
  // donnée : c'est le script de chargement du client qui les pose. Sans elles,
  // le lien de base de données et le numéro d'apparence n'existeraient nulle
  // part — et la fenêtre de description, qui les attend, retomberait toujours
  // sur son repli.
  REQUIRE(knife->description.size() == 6);
  CHECK(knife->description[0] ==
        "<URL>ItemID : 1201<INFO>https://site.moonlight-destiny.fr/objets/1201</INFO></URL>");
  CHECK(knife->description[1] == "ViewID : 1");
  // ⚠ UNE ESPACE, PAS UNE LIGNE VIDE : c'est ce que le script écrit.
  CHECK(knife->description[2] == " ");
  // ⚠ LE BALISAGE DE COULEUR RESTE : c'est ce que le client rendra, et le
  // retirer ici priverait de couleur qui saura le lire.
  //
  // 🔴 LE DIÈSE DE FIN DE LIGNE, LUI, N'EST PAS DU TEXTE : voir l'essai suivant.
  CHECK(knife->description[3] == "Une lame courte.");
  CHECK(knife->description[5] == "^0000CCPoids :^000000 40");
}

TEST_CASE("Table des objets : l'adresse de la fiche a UNE seule source", "[vfs][objets]") {
  // 🔴 LA MEME QUE CELLE DE LA DESCRIPTION. Le menu d'un lien de chat l'ouvre
  // aussi ; deux endroits qui l'assemblent finiraient par ne plus pointer au
  // meme endroit, et l'un des deux le ferait sans que rien ne le signale.
  const std::string url = ro::vfs::ItemDatabaseUrl(1201);
  CHECK(url.ends_with("/objets/1201"));
  CHECK(url.starts_with("https://"));
  // Et la ligne de description la PORTE, balisage autour.
  CHECK(std::string(ro::vfs::kDatabaseLinkPrefix).find("ItemID") != std::string::npos);
}

TEST_CASE("Table des objets : sans apparence, la ligne ViewID n'existe pas", "[vfs][objets]") {
  // ⚠ LE SCRIPT TESTE `ViewID > 0`. Une apparence nulle veut dire « rien à
  // montrer » : l'écrire serait une ligne de bruit sur la moitié des objets.
  TempDirectory client;
  MakeClient(client);
  PutTable(client, "  501: {name: \"Potion\", resource: \"p\", description: [\"Rouge.\"]}\n");

  // ⚠ LA TABLE SE GARDE : `Find` rend un pointeur DANS elle, et l'écrire sur la
  // valeur de retour de `Read` le ferait pendre dès la fin de l'expression.
  const ItemTable table = Read(client);
  const ItemEntry* potion = table.Find(501);
  REQUIRE(potion != nullptr);
  REQUIRE(potion->description.size() == 3);
  CHECK(potion->description[1] == " ");
  CHECK(potion->description[2] == "Rouge.");
}

TEST_CASE("Table des objets : un dièse final devient un trait de séparation", "[vfs][objets]") {
  // 🔴 C'EST LE SCRIPT DE CHARGEMENT DU CLIENT QUI LE VEUT, et il ne fait rien
  // d'autre que ceci :
  //
  //     if string.find(v, '#', -1) then
  //         AddItemIdentifiedDesc(ItemID, v:sub(1, -2))
  //         AddItemIdentifiedDesc(ItemID, "________________________")
  //
  // La ligne perd son dernier caractère, et VINGT-QUATRE soulignés la suivent.
  // Sans cela, le joueur lit « ...look.# » et les blocs de sa description se
  // touchent.
  TempDirectory client;
  MakeClient(client);
  PutTable(client,
           "  2279: {name: \"Wick\", resource: \"w\", description: [\"Une phrase.#\", "
           "\"Un # au milieu ne compte pas\", \"^0000CCPoids :^000000 10#\"]}\n");

  const ItemTable table = Read(client);
  const ItemEntry* wick = table.Find(2279);
  REQUIRE(wick != nullptr);
  // Deux lignes d'en-tête — pas de ViewID ici —, puis les cinq du texte.
  REQUIRE(wick->description.size() == 7);
  CHECK(wick->description[2] == "Une phrase.");
  CHECK(wick->description[3] == "________________________");
  // ⚠ AU MILIEU D'UNE PHRASE, UN DIÈSE EST UN DIÈSE : le script du client ne
  // regarde que le DERNIER caractère, et nous non plus.
  CHECK(wick->description[4] == "Un # au milieu ne compte pas");
  CHECK(wick->description[5] == "^0000CCPoids :^000000 10");
  CHECK(wick->description[6] == "________________________");
}

TEST_CASE("Table des objets : le nom reste en UTF-8, la ressource passe en CP949",
          "[vfs][objets]") {
  // ⚠ DEUX ENCODAGES DANS LA MÊME ENTRÉE, et c'est voulu. Le nom est un texte à
  // montrer : il reste en UTF-8. La ressource est un CHEMIN D'ARCHIVE, et les
  // archives sont indexées en CP949 : en UTF-8, elle ferait chercher un fichier
  // qui n'existe nulle part.
  TempDirectory client;
  MakeClient(client);
  PutTable(client, "  1: {name: \"고글\", resource: \"고글\"}\n");

  const ItemTable table = Read(client);
  const ItemEntry* entry = table.Find(1);
  REQUIRE(entry != nullptr);
  CHECK(entry->name == "고글");
  // « 고글 » en CP949.
  CHECK(entry->resource == "\xB0\xED\xB1\xDB");
}

TEST_CASE("Table des objets : les champs absents gardent leurs défauts", "[vfs][objets]") {
  TempDirectory client;
  MakeClient(client);
  PutTable(client, "  501: {name: \"Red Potion\"}\n");

  // ⚠ LA TABLE SE NOMME AVANT QU'ON Y CHERCHE : `Read(client).Find(501)` rendrait
  // l'adresse d'une entrée d'une table temporaire, détruite à la fin de
  // l'expression.
  const ItemTable table = Read(client);
  const ItemEntry* potion = table.Find(501);
  REQUIRE(potion != nullptr);
  CHECK(potion->resource.empty());
  CHECK(potion->card_slots == 0);
  CHECK(potion->view_id == 0);
  // 🔴 SA DESCRIPTION N'EST PAS VIDE POUR AUTANT : le script de chargement pose
  // son lien de base de données et sa ligne d'espace même sur un objet dont la
  // table ne dit rien. C'est ce que le client montre.
  REQUIRE(potion->description.size() == 2);
  CHECK(potion->description[1] == " ");
}

TEST_CASE("Table des objets : les échappements de JSON se résolvent", "[vfs][objets]") {
  // ⚠ LE SCRIPT ÉCRIT SES TEXTES COMME JSON : guillemets, antislash, fin de
  // ligne et caractères hors du plan de base par paire de substitution.
  const auto table = ParseItemTable(
      "# en-tête\n"
      "version: 1\n"
      "items: {\n"
      "  7: {name: \"true\", description: [\"Il dit \\\"non\\\".\", \"a\\\\b\\/c\", "
      "\"un\\ndeux\\ttrois\", \"\\u00e9t\\u00C9 \\ud83d\\ude00\"]},\n"
      "  8: {name: \"0012\", slots: 1},\n"
      "}\n");
  REQUIRE(table.has_value());
  REQUIRE(table->Find(7) != nullptr);
  // Un nom qui ressemble à un booléen ou à un nombre reste un texte.
  CHECK(table->Find(7)->name == "true");
  REQUIRE(table->Find(8) != nullptr);
  CHECK(table->Find(8)->name == "0012");
  const auto& lines = table->Find(7)->description;
  REQUIRE(lines.size() == 6);
  CHECK(lines[2] == "Il dit \"non\".");
  CHECK(lines[3] == "a\\b/c");
  CHECK(lines[4] == "un\ndeux\ttrois");
  CHECK(lines[5] == "étÉ 😀");
}

TEST_CASE("Table des objets : une forme inattendue refuse la table", "[vfs][objets]") {
  // Mieux vaut le savoir que jouer avec des noms faux.
  for (const std::string_view broken : {
           "version: 2\nitems: {}\n",
           "items: {}\n",
           "version: 1\nitems: []\n",
           "version: 1\nitems: {abc: {name: \"x\"}}\n",
           "version: 1\nitems: {1: \"x\"}\n",
           "version: 1\nitems: {1: {slots: 256}}\n",
           "version: 1\nitems: {1: {view: -1}}\n",
           "version: 1\nitems: {1: {slots: \"peut-être\"}}\n",
           // Deux champs retirés de la table : une table d'avant se refuse.
           "version: 1\nitems: {1: {costume: true}}\n",
           "version: 1\nitems: {1: {effect: 1186}}\n",
           "version: 1\nitems: {1: {description: \"une ligne\"}}\n",
           "version: 1\nitems: {1: {description: [[\"imbriquée\"]]}}\n",
           // Une ressource que le CP949 ne sait pas écrire : aucune archive ne
           // la porterait.
           "version: 1\nitems: {1: {resource: \"\xF0\x9F\x98\x80\"}}\n",
           "version: 1\nitems: {1: {name: \"sans fin}}\n",
           "version: 1\nitems: {1: {name: \"x\"}}\nautre: 1\n",
           "version: 1\nitems: {1: {name: \"x\"} 2: {name: \"y\"}}\n",
           // Du YAML valide, mais pas la forme que le script écrit.
           "version: 1\nitems: {1: {name: Knife}}\n",
           "version: 1\nitems: {1: {name: 'Knife'}}\n",
           "version: 1\nitems: {1: {name: \"\\x41\"}}\n",
           "version: 1\nitems: {1: {name: \"\\ud83d\"}}\n",
           "version: 1\nitems: {1: {inconnu: 1}}\n",
       }) {
    INFO(broken);
    CHECK_FALSE(ParseItemTable(broken).has_value());
  }
}

TEST_CASE("Table des objets : un refus dit sa ligne", "[vfs][objets]") {
  const auto table =
      ParseItemTable("version: 1\nitems: {\n  1: {name: \"a\"},\n  2: {slots: x}\n}\n");
  REQUIRE_FALSE(table.has_value());
  CHECK(table.error().detail.find("ligne 4") != std::string::npos);
}

TEST_CASE("Table des objets : absente, elle est VIDE et non fautive", "[vfs][objets]") {
  // Le jeu se joue sans : il montre des numéros là où il montrerait des noms.
  // En faire une erreur empêcherait de se connecter à une installation
  // incomplète, ce qui est pire.
  TempDirectory client;
  MakeClient(client);

  const ItemTable table = Read(client);
  CHECK(table.empty());
  CHECK(table.Find(501) == nullptr);
  CHECK(table.ViewOf(501) == 0);
  CHECK(table.ResourceOf(501).empty());
}

TEST_CASE("Table des objets : l'ancienne table Lua de l'installation n'est plus lue",
          "[vfs][objets]") {
  // Le client ne dépend plus des tables Lua du client d'origine : une
  // installation qui porte encore `SystemEN\itemInfoMerged.lua` sans la table
  // YAML n'a pas de noms.
  TempDirectory client;
  MakeClient(client);
  client.WriteText("SystemEN/itemInfoMerged.lua", "tbl = { [501] = { Name = \"Red Potion\" } }\n");

  CHECK(Read(client).empty());
}

TEST_CASE("Table des objets : la teinte se lit par son mot, et un mot inconnu se refuse",
          "[vfs][objets]") {
  const auto table = ParseItemTable(
      "version: 1\nitems: {\n"
      "  4001: {resource: \"r\", tint: \"purple\"},\n"
      "  4002: {resource: \"r\"}\n"
      "}\n");
  REQUIRE(table.has_value());
  REQUIRE(table->Find(4001) != nullptr);
  CHECK(table->Find(4001)->tint == ro::formats::IconTint::kPurple);
  CHECK(table->Find(4002)->tint == ro::formats::IconTint::kNone);
  CHECK_FALSE(ParseItemTable("version: 1\nitems: {1: {tint: \"mauve\"}}\n").has_value());
}
