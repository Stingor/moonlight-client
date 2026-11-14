// Le texte balisé du client : ses couleurs, ses liens, et sa disposition.
//
// ⚠ AUCUNE POLICE, AUCUNE FENÊTRE. La mesure est passée au moteur, et l'essai
// lui en donne une à lui — un pixel par octet —, si bien que les distances
// deviennent des nombres qu'on peut écrire ici.

#include "ro/ui/rich_text.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/skin_look.h"

using ro::ui::LayOutRichText;
using ro::ui::RichLinkKind;
using ro::ui::RichText;

namespace {

// UN PIXEL PAR OCTET. ⚠ PAR OCTET ET NON PAR CARACTÈRE : c'est la règle la plus
// simple qui distingue encore un caractère accentué d'une lettre, donc celle qui
// fait tomber l'essai si le découpage UTF-8 se perd.
float ByteWide(std::string_view piece, void* /*owner*/) { return static_cast<float>(piece.size()); }

constexpr ImU32 kDefault = IM_COL32(10, 20, 30, 255);
constexpr float kLineHeight = 10.0F;
// Assez large pour que rien ne se replie, sauf là où l'essai le veut.
constexpr float kNoWrap = 1000.0F;

[[nodiscard]] RichText LayOut(const std::vector<std::string>& lines, float wrap = kNoWrap) {
  return LayOutRichText(lines, kDefault, wrap, kLineHeight, &ByteWide, nullptr);
}

}  // namespace

namespace {

constexpr float kColumnsLineHeight = 10.0F;

[[nodiscard]] RichText LaidForColumns(const std::vector<std::string>& lines) {
  constexpr float kVeryWide = 1000.0F;
  return LayOutRichText(lines, kDefault, kVeryWide, kColumnsLineHeight, ByteWide, nullptr);
}

// Le texte d'une colonne, rangée par rangée, glyphes réunis.
[[nodiscard]] std::vector<std::string> ColumnRows(const RichText& column) {
  std::vector<std::string> rows;
  for (const ro::ui::RichBox& box : column.boxes) {
    rows.resize(std::max(rows.size(), static_cast<std::size_t>(box.row) + 1));
    rows[static_cast<std::size_t>(box.row)] += column.text.substr(
        static_cast<std::size_t>(box.offset), static_cast<std::size_t>(box.length));
  }
  return rows;
}

}  // namespace

TEST_CASE("Colonnes : un bloc qui tient ressort entier", "[ui][texte]") {
  const std::vector<RichText> columns =
      ro::ui::SplitRichTextColumns(LaidForColumns({"a", "b", "c"}), kColumnsLineHeight, 3, 4);
  REQUIRE(columns.size() == 1);
  CHECK(columns[0].height == 3.0F * kColumnsLineHeight);
}

TEST_CASE("Colonnes : chaque colonne repart de zéro et rien ne se perd", "[ui][texte]") {
  const std::vector<RichText> columns = ro::ui::SplitRichTextColumns(
      LaidForColumns({"a", "b", "c", "d", "e"}), kColumnsLineHeight, 2, 4);
  REQUIRE(columns.size() == 3);
  CHECK(ColumnRows(columns[0]) == std::vector<std::string>{"a", "b"});
  CHECK(ColumnRows(columns[1]) == std::vector<std::string>{"c", "d"});
  CHECK(ColumnRows(columns[2]) == std::vector<std::string>{"e"});
  REQUIRE_FALSE(columns[1].boxes.empty());
  CHECK(columns[1].boxes.front().top == 0.0F);
  CHECK(columns[1].height == 2.0F * kColumnsLineHeight);
  CHECK(columns[2].height == kColumnsLineHeight);
}

TEST_CASE("Colonnes : une colonne finit sur une fin de section", "[ui][texte]") {
  // Six rangées de place : le trait, en cinquième, est dans le dernier tiers.
  const std::vector<RichText> by_rule = ro::ui::SplitRichTextColumns(
      LaidForColumns({"a", "b", "c", "d", "____", "e", "f", "g"}), kColumnsLineHeight, 6, 4);
  REQUIRE(by_rule.size() == 2);
  CHECK(ColumnRows(by_rule[0]).back() == "____");
  CHECK(ColumnRows(by_rule[1]).front() == "e");

  // La rangée vide finit la colonne, et ne commence pas la suivante.
  const std::vector<RichText> by_blank = ro::ui::SplitRichTextColumns(
      LaidForColumns({"a", "b", "c", "d", "", "e", "f", "g"}), kColumnsLineHeight, 6, 4);
  REQUIRE(by_blank.size() == 2);
  CHECK(ColumnRows(by_blank[1]).front() == "e");

  // Trop tôt pour y couper : la colonne se remplit, plutôt que de rester à moitié vide.
  const std::vector<RichText> too_early = ro::ui::SplitRichTextColumns(
      LaidForColumns({"a", "____", "c", "d", "e", "f", "g"}), kColumnsLineHeight, 6, 4);
  REQUIRE(too_early.size() == 2);
  CHECK(ColumnRows(too_early[1]).front() == "g");
}

TEST_CASE("Colonnes : la dernière permise prend tout le reste", "[ui][texte]") {
  const std::vector<RichText> columns = ro::ui::SplitRichTextColumns(
      LaidForColumns({"a", "b", "c", "d", "e"}), kColumnsLineHeight, 1, 2);
  REQUIRE(columns.size() == 2);
  CHECK(ColumnRows(columns[1]) == std::vector<std::string>{"b", "c", "d", "e"});
}

TEST_CASE("Texte balisé : le balisage ne s'affiche pas", "[ui][texte]") {
  const RichText laid = LayOut({"^FF0000rouge^000000 noir"});
  // Le texte plat est ce qu'on LIT, et ce qu'on copie : pas un token.
  CHECK(laid.text == "rouge noir\n");
}

TEST_CASE("Texte balisé : ^000000 rend la couleur par défaut, pas du noir", "[ui][texte]") {
  // 🔴 LE PIÈGE DU FORMAT. Pris à la lettre, il rendrait illisible tout texte
  // écrit sur un fond sombre — un livre, une infobulle — alors que la source dit
  // seulement « je referme ma couleur ».
  const RichText laid = LayOut({"^FF0000a^000000b"});
  REQUIRE(laid.boxes.size() == 2);
  CHECK(laid.boxes[0].color == IM_COL32(255, 0, 0, 255));
  CHECK(laid.boxes[1].color == kDefault);
}

TEST_CASE("Texte balisé : une couleur ouverte traverse les lignes", "[ui][texte]") {
  // 🔴 C'EST UN FLUX, PAS UN ATTRIBUT DE LIGNE. Les tables du jeu écrivent la
  // couleur une fois en tête de paragraphe et la referment plusieurs lignes plus
  // bas ; la réinitialiser à chaque ligne décolorerait tout sauf la première.
  const RichText laid = LayOut({"^FF0000un", "deux"});
  REQUIRE(laid.boxes.size() >= 5);
  CHECK(laid.boxes[0].color == IM_COL32(255, 0, 0, 255));
  CHECK(laid.boxes.back().color == IM_COL32(255, 0, 0, 255));
}

TEST_CASE("Texte balisé : un accent circonflexe seul reste du texte", "[ui][texte]") {
  // ⚠ IL FAUT LES SIX CHIFFRES. Sans ce contrôle, un « ^^ » ou un « ^ok » ferait
  // sauter sept octets et mangerait le texte qui suit.
  const RichText laid = LayOut({"a^b"});
  CHECK(laid.text == "a^b\n");
}

TEST_CASE("Texte balisé : un token en plein mot n'injecte pas d'espace", "[ui][texte]") {
  // 🔴 « Knight^777777s » EST UN SEUL MOT. Le vidage du mot en cours est
  // déclenché aussi bien par une espace que par un token ; seul le premier doit
  // produire un séparateur, sinon le texte se disloque — « Knight s ».
  const RichText laid = LayOut({"Knight^777777s"});
  CHECK(laid.text == "Knights\n");
}

TEST_CASE("Texte balisé : un caractère accentué est UN glyphe", "[ui][texte]") {
  // 🔴 UNE BOÎTE PAR GLYPHE, PAS PAR OCTET. Découpé en octets, un « é » se
  // dessinerait en deux losanges et se sélectionnerait par moitié — une copie
  // coupée en plein accent.
  const RichText laid = LayOut({"é"});
  REQUIRE(laid.boxes.size() == 1);
  CHECK(laid.boxes[0].length == 2);
  CHECK(laid.text == "é\n");
}

TEST_CASE("Texte balisé : l'adresse d'un lien ne se montre pas", "[ui][texte]") {
  const RichText laid = LayOut({"voir <URL>ici<INFO>https://exemple.fr</INFO></URL> merci"});
  CHECK(laid.text == "voir ici merci\n");
  REQUIRE(laid.links.size() == 1);
  CHECK(laid.links[0].kind == RichLinkKind::kAddress);
  CHECK(laid.links[0].payload == "https://exemple.fr");
}

TEST_CASE("Texte balisé : un lieu se distingue d'une adresse", "[ui][texte]") {
  const RichText laid = LayOut({"<NAVI>Prontera<INFO>prontera,116,72</INFO></NAVI>"});
  REQUIRE(laid.links.size() == 1);
  CHECK(laid.links[0].kind == RichLinkKind::kPlace);
  CHECK(laid.links[0].payload == "prontera,116,72");
  // Tous les glyphes portent le lien : c'est par là que le survol le trouve.
  for (const auto& box : laid.boxes) {
    CHECK(box.link == 0);
  }
}

TEST_CASE("Texte balisé : un objet est un lien, son identifiant ne se montre pas", "[ui][texte]") {
  // La forme des tables de quêtes : le nom, puis l'identifiant en charge.
  const RichText laid =
      LayOut({"Gather 30 <ITEM>Poor Can<INFO>6426</INFO></ITEM> from the Starry Coral Area."});
  CHECK(laid.text == "Gather 30 Poor Can from the Starry Coral Area.\n");
  REQUIRE(laid.links.size() == 1);
  CHECK(laid.links[0].kind == RichLinkKind::kItem);
  CHECK(laid.links[0].payload == "6426");
  CHECK(ro::ui::ReadRichItem(laid.links[0].payload) == std::optional<std::uint32_t>(6426));
  CHECK(ro::ui::RichLinkText(laid, 0) == "Poor Can");
  // Le nom entier porte le lien, et la suite du texte ne le porte plus.
  int linked = 0;
  for (const auto& box : laid.boxes) {
    linked += box.link == 0 ? 1 : 0;
  }
  CHECK(linked >= static_cast<int>(std::string_view("Poor Can").size()));
  CHECK(laid.boxes.back().link == -1);
}

TEST_CASE("Texte balisé : un objet sans identifiant lisible reste du texte", "[ui][texte]") {
  // Ni charge, une charge qui n'est pas un nombre, un identifiant nul : le nom
  // se lit dans la couleur du flux, et rien ne s'y clique.
  for (const std::string line :
       {"^FF0000<ITEM>Nom</ITEM>", "^FF0000<ITEM>Nom<INFO>abc</INFO></ITEM>",
        "^FF0000<ITEM>Nom<INFO>0</INFO></ITEM>", "^FF0000<ITEM>Nom<INFO>501x</INFO></ITEM>"}) {
    const RichText laid = LayOut({line});
    CHECK(laid.text == "Nom\n");
    REQUIRE(laid.boxes.size() == 3);
    for (const auto& box : laid.boxes) {
      CHECK(box.link == -1);
      CHECK(box.color == IM_COL32(255, 0, 0, 255));
    }
  }
}

TEST_CASE("Texte balisé : un objet non refermé s'arrête en fin de ligne", "[ui][texte]") {
  const RichText laid = LayOut({"<ITEM>Potion<INFO>501</INFO>", "suite"});
  CHECK(laid.text == "Potion\nsuite\n");
  REQUIRE(laid.links.size() == 1);
  CHECK(laid.boxes.front().link == 0);
  CHECK(laid.boxes.back().link == -1);
}

TEST_CASE("Texte balisé : une fiche d'aide ne montre que son texte", "[ui][texte]") {
  // La forme des fiches d'aide : un texte, puis le rang de la fiche en charge.
  const RichText laid = LayOut({"?<TIPBOX>View Tip List<INFO>1000</INFO></TIPBOX>"});
  CHECK(laid.text == "?View Tip List\n");
  CHECK(laid.links.empty());
  for (const auto& box : laid.boxes) {
    CHECK(box.link == -1);
    CHECK(box.color == kDefault);
  }
}

TEST_CASE("Texte balisé : un identifiant d'objet est un décimal non nul, seul", "[ui][texte]") {
  CHECK(ro::ui::ReadRichItem("501") == std::optional<std::uint32_t>(501));
  CHECK_FALSE(ro::ui::ReadRichItem("").has_value());
  CHECK_FALSE(ro::ui::ReadRichItem("0").has_value());
  CHECK_FALSE(ro::ui::ReadRichItem("-1").has_value());
  CHECK_FALSE(ro::ui::ReadRichItem("501,1").has_value());
  CHECK_FALSE(ro::ui::ReadRichItem("Potion").has_value());
}

TEST_CASE("Texte balisé : un lien porte sa couleur, pas celle du flux", "[ui][texte]") {
  const RichText laid = LayOut({"^FF0000<URL>ici<INFO>https://a.fr</INFO></URL>"});
  REQUIRE_FALSE(laid.boxes.empty());
  CHECK(laid.boxes[0].color != IM_COL32(255, 0, 0, 255));
}

TEST_CASE("Texte balisé : le repli coupe entre les mots", "[ui][texte]") {
  // Cinq octets par mot, une espace : « aaaaa bbbbb » ne tient pas dans huit.
  const RichText laid = LayOut({"aaaaa bbbbb"}, 8.0F);
  REQUIRE(laid.boxes.size() == 10);
  CHECK(laid.boxes[0].row == 0);
  CHECK(laid.boxes[4].row == 0);
  CHECK(laid.boxes[5].row == 1);
  // 🔴 ET LE TEXTE PLAT GARDE UNE ESPACE, PAS UN SAUT DE LIGNE. Un repli est un
  // accident d'AFFICHAGE — il dépend de la largeur de la fenêtre au moment du
  // clic — et le graver dans ce qu'on copie collerait cette largeur-là au texte.
  // Seules les fins de ligne de la SOURCE deviennent des sauts.
  CHECK(laid.text == "aaaaa bbbbb\n");
}

TEST_CASE("Texte balisé : un mot plus large que la fenêtre se coupe au caractère", "[ui][texte]") {
  // 🔴 SANS COUPE DURE, une adresse ou un mot sans espace sortait par la droite
  // de la fenêtre, jusqu'au bout : il n'y avait aucune espace où replier.
  const RichText laid = LayOut({"aaaaaaaaaaaa"}, 5.0F);
  REQUIRE(laid.boxes.size() == 12);
  CHECK(laid.boxes[4].row == 0);
  CHECK(laid.boxes[5].row == 1);
  CHECK(laid.boxes[5].left == 0.0F);
  CHECK(laid.boxes[11].row == 2);
  // La coupe n'est qu'un accident d'affichage : le mot copié reste entier.
  CHECK(laid.text == "aaaaaaaaaaaa\n");
}

TEST_CASE("Texte balisé : une ligne vide garde sa rangée", "[ui][texte]") {
  // ⚠ ELLE SÉPARE DEUX PARAGRAPHES, et l'avaler collerait les deux.
  const RichText laid = LayOut({"un", "", "deux"});
  CHECK(laid.text == "un\n\ndeux\n");
  REQUIRE(laid.boxes.size() == 6);
  CHECK(laid.boxes[0].row == 0);
  CHECK(laid.boxes.back().row == 2);
}

TEST_CASE("Texte balisé : un lien ne traverse pas une fin de ligne", "[ui][texte]") {
  // ⚠ CONTRAIREMENT À LA COULEUR. Une balise que la source ouvre, la source la
  // ferme ; une couleur, elle, coule. Sans cette remise à zéro, une source qui
  // oublie son `</URL>` peindrait tout le reste du bloc en lien cliquable.
  const RichText laid = LayOut({"<URL>ici<INFO>https://a.fr</INFO>", "suite"});
  REQUIRE(laid.boxes.size() >= 8);
  CHECK(laid.boxes[0].link == 0);
  CHECK(laid.boxes.back().link == -1);
}

TEST_CASE("Texte balisé : sans mesure, rien n'est disposé", "[ui][texte]") {
  // Une couche de dessin peut être appelée sans police — un banc, une trame
  // sans dos. Elle rend un bloc vide plutôt que de déréférencer rien.
  const std::vector<std::string> lines = {"du texte"};
  const RichText laid = LayOutRichText(lines, kDefault, 100.0F, kLineHeight, nullptr, nullptr);
  CHECK(laid.text.empty());
  CHECK(laid.boxes.empty());
}

TEST_CASE("Texte balisé : un lieu se lit carte, x, y", "[ui][texte]") {
  const auto place = ro::ui::ReadRichPlace("prontera,150,180,0,000,0");
  REQUIRE(place.has_value());
  CHECK(place->map == "prontera");
  CHECK(place->x == 150);
  CHECK(place->y == 180);
  CHECK(ro::ui::ReadRichPlace("prontera,150,180").has_value());
  // Sans carte, sans coordonnées, ou avec des lettres : pas de lieu.
  CHECK_FALSE(ro::ui::ReadRichPlace(",150,180").has_value());
  CHECK_FALSE(ro::ui::ReadRichPlace("prontera").has_value());
  CHECK_FALSE(ro::ui::ReadRichPlace("prontera,x,180").has_value());
}

TEST_CASE("Texte balisé : ses couleurs restent lisibles sur le papier", "[ui][texte]") {
  // 🔴 LE `^0000FF` D'UNE DESCRIPTION EST ÉCRIT POUR LE BLANC : sur un papier
  // sombre, il s'éclaircit — et le même bleu deux fois donne deux fois le même.
  constexpr ImU32 kDarkPaper = IM_COL32(40, 42, 50, 255);
  RichText laid = LayOut({"^0000FFType:^000000 ^0000FFPoids:"});
  REQUIRE(laid.boxes.size() > 1);
  const ImU32 blue = laid.boxes.front().color;
  REQUIRE(blue == IM_COL32(0, 0, 255, 255));
  ro::ui::KeepReadable(laid, kDarkPaper);
  const ImU32 shown = laid.boxes.front().color;
  CHECK(shown != blue);
  CHECK(ro::ui::ContrastRatio(shown, kDarkPaper) >= ro::ui::kSkinLeastContrast);
  CHECK(laid.boxes.back().color == shown);
  // Sur le blanc pour lequel il est écrit, il ne bouge pas.
  RichText on_white = LayOut({"^0000FFType:"});
  ro::ui::KeepReadable(on_white, IM_COL32_WHITE);
  CHECK(on_white.boxes.front().color == blue);
}
