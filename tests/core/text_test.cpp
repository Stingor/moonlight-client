#include "ro/core/text.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::core::AppendUtf8;
using ro::core::Cp1252FromUtf8;
using ro::core::kReplacementCharacter;
using ro::core::NextCodePoint;
using ro::core::Utf8FromCp1252;

TEST_CASE("Texte : l'ASCII traverse les deux conversions sans changer", "[core][text]") {
  // C'est ce qui rend la conversion sûre à poser partout : tant qu'on n'écrit
  // que de l'ASCII, elle ne fait rien du tout.
  const std::string plain = "Alice : @speed 200";
  CHECK(Utf8FromCp1252(plain) == plain);
  CHECK(Cp1252FromUtf8(plain) == plain);
}

TEST_CASE("Texte : un accent du jeu devient un accent lisible", "[core][text]") {
  // Le serveur écrit « é » sur UN octet. Lu comme de l'UTF-8, cet octet
  // commence une séquence qui n'arrive jamais, et la lettre disparaît — c'est
  // le trou qu'on voyait dans le journal.
  const std::string game = "ignor\xE9s \xE0 terre";
  const std::string ours = Utf8FromCp1252(game);
  CHECK(ours == "ignorés à terre");
  // Et le retour rend exactement les octets du départ.
  CHECK(Cp1252FromUtf8(ours) == game);
}

TEST_CASE("Texte : les trente-deux octets que Windows a remplis", "[core][text]") {
  // CP1252 ne diffère de Latin-1 que là : l'euro, les guillemets typographiques,
  // les tirets. Les traiter comme du Latin-1 rendrait des caractères de
  // commande invisibles.
  CHECK(Utf8FromCp1252("\x80") == "€");
  CHECK(Utf8FromCp1252("\x92") == "’");
  CHECK(Cp1252FromUtf8("€") == "\x80");
  CHECK(Cp1252FromUtf8("’") == "\x92");
  // Cinq octets n'y sont pas définis : ils ne peuvent rendre aucune lettre.
  std::string replacement;
  AppendUtf8(replacement, kReplacementCharacter);
  CHECK(Utf8FromCp1252("\x81") == replacement);
}

TEST_CASE("Texte : ce que la page de codes ne porte pas devient un trou visible", "[core][text]") {
  // Un octet inventé partirait au serveur, qui le relaierait à tout le monde.
  CHECK(Cp1252FromUtf8("한글") == "??");
  CHECK(Cp1252FromUtf8("a한b") == "a?b");
}

TEST_CASE("Texte : une séquence UTF-8 cassée n'arrête pas la lecture", "[core][text]") {
  // Du texte mal encodé se lit de travers ; il ne doit pas faire boucler qui
  // le lit, ni sortir de la chaîne.
  const std::string broken = "a\xC3";
  std::size_t at = 0;
  CHECK(NextCodePoint(broken, at) == U'a');
  CHECK(NextCodePoint(broken, at) == kReplacementCharacter);
  CHECK(at == broken.size());
}

TEST_CASE("Texte : chaque longueur d'UTF-8 s'écrit et se relit", "[core][text]") {
  for (const char32_t code : {U'A', U'é', U'€', U'\U0001F600'}) {
    std::string written;
    AppendUtf8(written, code);
    std::size_t at = 0;
    CHECK(NextCodePoint(written, at) == code);
    CHECK(at == written.size());
  }
}

TEST_CASE("Texte : le nombre qui suit une commande", "[core][text]") {
  // ⚠ CET ESSAI EXISTE POUR UN DÉFAUT PRÉCIS, et il l'aurait vu. L'argument de
  // « /echelle » était lu par `substr` sur la `std::string` de l'appelant, posé
  // dans une vue : la chaîne temporaire mourait aussitôt, la lecture rendait
  // n'importe quoi, et toute échelle demandée retombait sur son plancher — un
  // demi. « 0.5 » semblait donc marcher, ce qui rendait le défaut invisible.
  //
  // La ligne est une `std::string`, comme dans le jeu.
  const std::string scale_two("/echelle 2.0");
  REQUIRE(ro::core::NumberAfterCommand(scale_two, "/echelle").has_value());
  CHECK(*ro::core::NumberAfterCommand(scale_two, "/echelle") == 2.0F);

  const std::string half("/echelle 0.5");
  CHECK(*ro::core::NumberAfterCommand(half, "/echelle") == 0.5F);
  const std::string integer_command("/echelle 1");
  CHECK(*ro::core::NumberAfterCommand(integer_command, "/echelle") == 1.0F);
  // Plusieurs espaces.
  const std::string espaces("/echelle   1.25");
  CHECK(*ro::core::NumberAfterCommand(espaces, "/echelle") == 1.25F);
}

TEST_CASE("Texte : un argument absent ou illisible ne rend rien", "[core][text]") {
  CHECK_FALSE(ro::core::NumberAfterCommand("/echelle", "/echelle").has_value());
  CHECK_FALSE(ro::core::NumberAfterCommand("/echelle ", "/echelle").has_value());
  CHECK_FALSE(ro::core::NumberAfterCommand("/echelle grand", "/echelle").has_value());
  // Une autre commande n'est pas celle-ci.
  CHECK_FALSE(ro::core::NumberAfterCommand("/langue en", "/echelle").has_value());
}

TEST_CASE("Texte : les codes de couleur du jeu se découpent", "[core][text]") {
  const std::string line("^1F81D4Warp Agent^000000 !");
  const std::vector<ro::core::ColorRun> runs = ro::core::SplitColorRuns(line);

  REQUIRE(runs.size() == 2);
  CHECK(runs[0].text == "Warp Agent");
  CHECK(runs[0].colored);
  CHECK(runs[0].rgb == 0x1F81D4);
  // ⚠ `^000000` N'EST PAS DU NOIR, c'est le retour à la couleur par défaut. Le
  // prendre au mot écrirait en noir sur fond sombre la moitié de chaque
  // dialogue, puisque c'est par là que les scripts terminent un passage coloré.
  CHECK(runs[1].text == " !");
  CHECK_FALSE(runs[1].colored);
}

TEST_CASE("Texte : ce qui n'est pas un code reste du texte", "[core][text]") {
  // Un texte sans code rend un seul morceau, sans couleur.
  const std::string nu("Bonjour, aventurier.");
  const std::vector<ro::core::ColorRun> simple = ro::core::SplitColorRuns(nu);
  REQUIRE(simple.size() == 1);
  CHECK(simple[0].text == nu);
  CHECK_FALSE(simple[0].colored);

  // Un accent circonflexe qui n'annonce pas six chiffres est du texte : le
  // client le laisse passer, et un dialogue peut en contenir.
  const std::string faux("2^10 et ^12345 et ^GGGGGG");
  const std::vector<ro::core::ColorRun> garde = ro::core::SplitColorRuns(faux);
  REQUIRE(garde.size() == 1);
  CHECK(garde[0].text == faux);

  CHECK(ro::core::SplitColorRuns("").empty());
}

TEST_CASE("Texte : un code en tête ne laisse pas de morceau vide", "[core][text]") {
  const std::string line("^ff0000Danger");
  const std::vector<ro::core::ColorRun> runs = ro::core::SplitColorRuns(line);
  REQUIRE(runs.size() == 1);
  CHECK(runs[0].text == "Danger");
  CHECK(runs[0].rgb == 0xFF0000);

  // Deux codes qui se suivent : le premier ne colore RIEN, et ne doit donc pas
  // produire un morceau vide que l'affichage compterait comme une ligne.
  const std::string two_codes("^ff0000^00ff00Vert");
  const std::vector<ro::core::ColorRun> colles = ro::core::SplitColorRuns(two_codes);
  REQUIRE(colles.size() == 1);
  CHECK(colles[0].text == "Vert");
  CHECK(colles[0].rgb == 0x00FF00);
}

TEST_CASE("Fil : de l'UTF-8 ne repasse pas par la table 1252", "[core][texte]") {
  // 🔴 LE FIL PORTE LES DEUX ENCODAGES. Le serveur relaie les octets qu'on
  // lui donne : nos emoji reviennent en UTF-8, là où le client d'origine envoie du
  // 1252. Repasser de l'UTF-8 par la table le DOUBLE, et la phrase revient en
  // bouillie.
  const std::string emoji = "salut \xF0\x9F\x98\x8D";  // U+1F60D
  CHECK(ro::core::LooksLikeUtf8(emoji));
  CHECK(ro::core::Utf8FromWire(emoji) == emoji);

  // Du 1252, lui, se convertit comme avant : 0xE9 est un « é ».
  const std::string latin = "caf\xE9";
  CHECK_FALSE(ro::core::LooksLikeUtf8(latin));
  CHECK(ro::core::Utf8FromWire(latin) == Utf8FromCp1252(latin));

  // 🔴 « VOILÀ… » EN 1252 FINIT PAR C0 85 : une forme SURLONGUE, bien formée
  // en apparence, qu'un décodeur laxiste lit U+0005. Le texte doit passer par
  // la table, sans quoi le « À » s'affiche en losange.
  const std::string voila = "VOIL\xC0\x85";
  CHECK_FALSE(ro::core::LooksLikeUtf8(voila));
  CHECK(ro::core::Utf8FromWire(voila) == Utf8FromCp1252(voila));
  // Les moitiés de paire UTF-16 et l'au-delà de U+10FFFF ne sont pas de l'UTF-8.
  CHECK_FALSE(ro::core::LooksLikeUtf8("\xED\xA0\x80"));
  CHECK_FALSE(ro::core::LooksLikeUtf8("\xF4\x90\x80\x80"));
  CHECK_FALSE(ro::core::LooksLikeUtf8("\xE0\x80\xAF"));

  // ⚠ DE L'ASCII PUR REND FAUX : les deux tables coïncident, la question ne se
  // pose pas, et le chemin habituel convient.
  CHECK_FALSE(ro::core::LooksLikeUtf8("hello"));
  CHECK(ro::core::Utf8FromWire("hello") == "hello");
}

TEST_CASE("Fil : une séquence coupée ne renvoie pas la phrase en mojibake", "[core][texte]") {
  // 🔴 LE SERVEUR REMPLIT DES CHAMPS DE TAILLE FIXE et coupe à l'octet
  // près, sans savoir ce qu'il coupe — or un emoji pèse quatre octets. Sans ce
  // rattrapage, c'est le message ENTIER qui repart en 1252 : on ne perd pas le
  // dernier caractère, on perd la phrase.
  const std::string whole =
      "o\xC3\xB9 \xC3\xA7"
      "a \xF0\x9F\x98\x8D";
  const std::string cut = whole.substr(0, whole.size() - 2);
  CHECK(ro::core::DanglingUtf8TailLength(cut) == 2);
  // La queue orpheline est jetée, le reste survit tel quel.
  CHECK(ro::core::Utf8FromWire(cut) ==
        "o\xC3\xB9 \xC3\xA7"
        "a ");
}

TEST_CASE("Fil : le rattrapage EXIGE du non-ASCII avant la coupure", "[core][texte]") {
  // 🔴 ET C'EST CE QUI PROTÈGE LE TEXTE 1252. « voilà » finit par 0xE0,
  // qui EST un octet de tête UTF-8 annonçant trois octets : la fonction y voit
  // donc une coupure. Si l'on acceptait un préfixe purement ASCII, on rendrait
  // « voil » — l'accent perdu en silence, sur une phrase parfaitement valide.
  //
  // Le prix de cette prudence : une séquence coupée après de l'ASCII PUR repart
  // en 1252 et sort en mojibake. C'est le cas rare, et le seul des deux qui se
  // VOIE — un accent qui disparaît, personne ne le remarque.
  CHECK(ro::core::DanglingUtf8TailLength("voil\xE0") == 1);
  CHECK(ro::core::Utf8FromWire("voil\xE0") == Utf8FromCp1252("voil\xE0"));
}

TEST_CASE("Fil : le sélecteur de variante est retiré", "[core][texte]") {
  // ⚠ U+FE0F NE SE CONTENTE PAS D'ÊTRE INVISIBLE, IL PREND DE LA PLACE. Un moteur
  // de texte complet l'absorbe au façonnage ; ImGui n'en fait pas et dessine
  // bêtement son glyphe, vide mais large — chaque cœur serait suivi d'un trou.
  const std::string heart = "\xE2\x9D\xA4\xEF\xB8\x8F";  // U+2764 U+FE0F
  CHECK(ro::core::Utf8FromWire(heart) == "\xE2\x9D\xA4");
}

TEST_CASE("Fil : une phrase ne bascule en UTF-8 que si elle le doit", "[core][texte]") {
  // 🔴 1252 TANT QUE POSSIBLE. Une phrase accentuée ordinaire doit partir
  // EXACTEMENT comme avant — mêmes octets dans les journaux, dans la base, pour
  // les commandes d'administration. La migration ne doit se voir nulle part
  // ailleurs que sur la phrase qui contient vraiment un emoji.
  const std::string accented = "caf\xC3\xA9";  // « café » en UTF-8
  CHECK(ro::core::WireFromUtf8Text(accented) == "caf\xE9");
  CHECK(ro::core::WireFromUtf8Text("hello") == "hello");

  // ⚠ UN « ? » TAPÉ PAR LE JOUEUR EST REPRÉSENTABLE : chercher le caractère de
  // remplacement dans le résultat ferait basculer une phrase qui n'en a pas
  // besoin. C'est la CONVERSION qui refuse, pas une inspection du résultat.
  CHECK(ro::core::WireFromUtf8Text("quoi ?") == "quoi ?");

  // Un emoji, lui, n'a aucune représentation : la phrase part en UTF-8 telle
  // quelle, et c'est le lecteur d'en face qui s'en accommode.
  const std::string emoji = "salut \xF0\x9F\x98\x8D";
  CHECK(ro::core::WireFromUtf8Text(emoji) == emoji);
  CHECK_FALSE(ro::core::ExactCp1252FromUtf8(emoji).has_value());

  // Et l'aller-retour se referme : ce qui part revient identique.
  CHECK(ro::core::Utf8FromWire(ro::core::WireFromUtf8Text(emoji)) == emoji);
  CHECK(ro::core::Utf8FromWire(ro::core::WireFromUtf8Text(accented)) == accented);
}

TEST_CASE("Texte : une coupe à l'octet ne garde que des caractères entiers", "[core][text]") {
  // « é » pèse deux octets : coupé après le premier, il ne reste rien à montrer
  // de lui, et la queue doit tomber plutôt que s'afficher en carré.
  const std::string cut = std::string("abc") + static_cast<char>(0xC3);
  CHECK(ro::core::WithoutDanglingUtf8(cut) == "abc");
  CHECK(ro::core::WithoutDanglingUtf8("abcé") == "abcé");
  CHECK(ro::core::WithoutDanglingUtf8("").empty());
}

TEST_CASE("Texte : une coupe à un nombre d'octets recule jusqu'à un caractère", "[core][text]") {
  // « é » pèse deux octets : une coupe qui tomberait entre eux le laisse
  // entier dehors.
  CHECK(ro::core::Utf8Prefix("abé", 3) == "ab");
  CHECK(ro::core::Utf8Prefix("abé", 4) == "abé");
  CHECK(ro::core::Utf8Prefix("abé", 2) == "ab");
  CHECK(ro::core::Utf8Prefix("abc", 10) == "abc");
  CHECK(ro::core::Utf8Prefix("abc", 0).empty());
}

TEST_CASE("Texte : des octets se lisent comme du texte, tels quels", "[core][text]") {
  // Aucun transcodage : l'octet CP949 0xB0 reste 0xB0, et la vue porte sur les
  // octets mêmes, sans copie.
  const std::vector<std::uint8_t> bytes = {'a', 0xB0, 'b'};
  const std::string_view text = ro::core::AsText(bytes);
  CHECK(text ==
        "a\xB0"
        "b");
  CHECK(static_cast<const void*>(text.data()) == static_cast<const void*>(bytes.data()));
  const std::vector<std::uint8_t> none;
  CHECK(ro::core::AsText(none).empty());
}
