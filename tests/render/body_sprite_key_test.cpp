// LA CLÉ D'UN CORPS, et pourquoi elle se vérifie chiffre en main.
//
// 🔴 C'EST LE SERVEUR QUI L'IMPOSE, et tous les clients doivent en trouver la
// même : c'est elle qui dit, dans une recette de couleurs, de quel corps on
// parle. Une normalisation qui diverge fait choisir une AUTRE variante chez le
// voisin — ou aucune —, et rien ne le signale.

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/sprite_catalog.h"

using ro::render::BodySpriteKey;

namespace {

// Le corps d'un novice masculin, tel que le catalogue le nomme.
const std::string kNovice = "data\\sprite\\인간족\\몸통\\남\\초보자_남";

// 🔴 UNE VALEUR TÉMOIN, ET NON UN CALCUL REFAIT ICI. Recalculer la formule
// dans l'essai ne prouverait que l'accord du fichier avec lui-même ; ce
// nombre-ci vient de la formule de référence appliquée au chemin ci-dessus, et
// il tombe le jour où la normalisation change.
//
// 🔴 ET IL ÉPINGLE L'ENCODAGE, qui est la seule chose que la relecture ne
// voit pas. Le condensé se prend sur les octets CP949 — ceux que le client
// tient —, pas sur nos octets UTF-8 : les mêmes octets en UTF-8 donneraient
// 0x1eb3de79, une clé que personne d'autre ne trouverait, et la variante de
// couleurs d'un joueur ne serait jamais choisie. L'écart ne se verrait qu'en
// jeu, et seulement à deux.
constexpr std::uint32_t kNoviceKey = 0x1f87791d;

}  // namespace

TEST_CASE("Clé de corps : une valeur témoin, pas un calcul refait", "[render][palette]") {
  CHECK(BodySpriteKey(kNovice) == kNoviceKey);
}

TEST_CASE("Clé de corps : la base et le fichier désignent le même corps", "[render][palette]") {
  // Selon l'appelant, le chemin est une base ou un fichier.
  CHECK(BodySpriteKey(kNovice + ".spr") == kNoviceKey);
  CHECK(BodySpriteKey(kNovice + ".SPR") == kNoviceKey);
  // ⚠ ET SEULE L'EXTENSION `.spr` : un `.act` désigne l'animation, pas le corps.
  CHECK(BodySpriteKey(kNovice + ".act") != kNoviceKey);
}

TEST_CASE("Clé de corps : les deux séparateurs se valent", "[render][palette]") {
  std::string slashes = kNovice;
  for (char& letter : slashes) {
    if (letter == '\\') {
      letter = '/';
    }
  }
  CHECK(BodySpriteKey(slashes) == kNoviceKey);
}

TEST_CASE("Clé de corps : la casse ASCII ne compte pas", "[render][palette]") {
  CHECK(BodySpriteKey("DATA\\Sprite\\a") == BodySpriteKey("data\\sprite\\a"));
  // Deux dossiers coréens différents restent deux corps différents : le repli
  // ne touche que 'A' à 'Z', et jamais les octets d'une syllabe.
  CHECK(BodySpriteKey("data\\sprite\\몸통") != BodySpriteKey("data\\sprite\\머리통"));
}

TEST_CASE("Clé de corps : seul le chemin VIDE ne désigne aucune variante", "[render][palette]") {
  CHECK(BodySpriteKey("") == 0);
  // Tout le reste en désigne une : la sentinelle ne se rend jamais par accident.
  CHECK(BodySpriteKey("a") != 0);
  // ⚠ L'EXTENSION NE SE RETIRE QUE D'UN CHEMIN PLUS LONG QU'ELLE, et c'est la
  // règle de la référence — `n > 4`, strictement. Un chemin réduit à « .spr »
  // se condense donc comme un autre. Aucun corps ne s'appelle ainsi ; ce qui
  // compte est de ne pas s'écarter de la référence sur un cas de bord, parce
  // qu'un écart y est un écart partout.
  CHECK(BodySpriteKey(".spr") != 0);
}
