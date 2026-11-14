#include "ro/sim/banned_words.h"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

// LES MOTS INTERDITS : la liste lue dans le fichier de l'installation, et le
// masque qu'elle pose sur un texte.

using ro::sim::BannedWords;

TEST_CASE("Mots interdits : une liste vide ne masque rien", "[sim][banned]") {
  const BannedWords none;
  CHECK(none.empty());
  CHECK(none.Mask("Bonjour a tous.") == "Bonjour a tous.");
}

TEST_CASE("Mots interdits : un mot se masque où qu'il paraisse, sans égard à la casse",
          "[sim][banned]") {
  const BannedWords banned(std::vector<std::string>{"Chou"});
  CHECK(banned.words().front() == "chou");
  // Dans un mot comme seul, une étoile par caractère, la casse ASCII ignorée.
  CHECK(banned.Mask("Un CHOU, des choux.") == "Un ****, des ****x.");
  // Une étoile par CARACTÈRE, pas par octet.
  const BannedWords accented(std::vector<std::string>{"été"});
  CHECK(accented.Mask("un été chaud") == "un *** chaud");
}

TEST_CASE("Mots interdits : l'ordre de la liste départage, et rien ne se chevauche",
          "[sim][banned]") {
  // « ab » vient d'abord : il gagne sur « abc », plus long, à la même position.
  const BannedWords ordered(std::vector<std::string>{"ab", "abc"});
  CHECK(ordered.Mask("abcd") == "**cd");
  // Après un mot masqué, la lecture reprend derrière lui : « bc » ne se
  // cherche pas dans le « b » déjà masqué.
  const BannedWords overlapping(std::vector<std::string>{"ab", "bc"});
  CHECK(overlapping.Mask("abc") == "**c");
}

TEST_CASE("Mots interdits : un lien d'objet ne se masque pas", "[sim][banned]") {
  const BannedWords banned(std::vector<std::string>{"epee"});
  CHECK(banned.Mask("<ITEML>000001epee</ITEML> epee") == "<ITEML>000001epee</ITEML> ****");
  // Sans balise fermante, le lien n'en est pas un.
  CHECK(banned.Mask("<ITEML>epee") == "<ITEML>****");
}

TEST_CASE("Mots interdits : l'épreuve d'une saisie lit comme le masque", "[sim][banned]") {
  const BannedWords none;
  CHECK_FALSE(none.Contains("brocoli"));
  const BannedWords banned(std::vector<std::string>{"brocoli"});
  CHECK(banned.Contains("Un BROCOLI"));
  CHECK(banned.Contains("brocolis"));
  CHECK_FALSE(banned.Contains("broc oli"));
  // ⚠ LE LIEN D'OBJET SE SAUTE ICI AUSSI.
  CHECK_FALSE(banned.Contains("<ITEML>000001brocoli</ITEML>"));
  CHECK(banned.Contains("<ITEML>000001</ITEML> brocoli"));
  // Sans liste, la forme qui la tolère ne masque rien.
  CHECK(BannedWords::MaskWith(nullptr, "brocoli") == "brocoli");
  CHECK(BannedWords::MaskWith(&banned, "brocoli") == "*******");
}

TEST_CASE("Mots interdits : la liste s'abaisse et garde les espaces de ses mots", "[sim][banned]") {
  // La liste que la conversion tire du fichier « Alpha\r\n// un
  // commentaire#qui continue\r\nbeta#gamma#\r\n \t\r\n delta\r\nperdu\ndernier ».
  const BannedWords read({"Alpha", "beta", "gamma", " delta", ""});
  const std::vector<std::string> expected{"alpha", "beta", "gamma", " delta"};
  CHECK(std::vector<std::string>(read.words().begin(), read.words().end()) == expected);
  // L'espace de tête fait partie du mot.
  CHECK(read.Mask("delta et  delta") == "delta et ******");
}
