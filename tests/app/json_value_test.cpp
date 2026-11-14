// LE LECTEUR JSON DU LOGIN WEB.
//
// 🔴 IL LIT DES OCTETS VENUS DU RÉSEAU, et c'est ce qui décide de ce qu'on
// éprouve : pas « sait-il lire une réponse juste » — n'importe quel lecteur le
// sait — mais « que fait-il d'une réponse qui ne l'est pas ». Une réponse
// tronquée, une imbrication sans fond, un échappement à moitié écrit : chacun
// est ce qu'un serveur en panne, un proxy bavard ou un attaquant envoie.

#include "web/json_value.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

using moonclient::web::JsonValue;
using moonclient::web::kJsonMaxDepth;
using moonclient::web::ParseJson;

namespace {

// La réponse d'`action=auth`, telle que le site l'écrit — avec ses accents
// échappés, parce que `json_encode` est appelé sans `JSON_UNESCAPED_UNICODE`.
constexpr const char* kAuthReply = R"({"ok":true,"web_ticket":"1.2.abcdef","accounts":[)"
                                   R"({"account_id":2000000,"userid":"stingor","label":"stingor",)"
                                   R"("char_count":3,"last_login":"2026-09-20","banned":false,)"
                                   R"("online":true,"autotrade":false},)"
                                   R"({"account_id":2000001,"userid":"second","label":"second",)"
                                   R"("char_count":0,"last_login":"","banned":true,)"
                                   R"("online":false,"autotrade":false}]})";

}  // namespace

TEST_CASE("JSON : la réponse du site se lit champ par champ", "[app][json]") {
  const auto read = ParseJson(kAuthReply);
  REQUIRE(read.has_value());
  CHECK(read->TruthAt("ok"));
  CHECK(read->TextAt("web_ticket") == "1.2.abcdef");

  const JsonValue* accounts = read->Field("accounts");
  REQUIRE(accounts != nullptr);
  REQUIRE(accounts->is_array());
  REQUIRE(accounts->Items().size() == 2);

  const JsonValue& first = accounts->Items()[0];
  CHECK(first.WholeAt("account_id") == 2'000'000);
  CHECK(first.TextAt("userid") == "stingor");
  CHECK(first.WholeAt("char_count") == 3);
  CHECK(first.TruthAt("online"));
  CHECK_FALSE(first.TruthAt("banned"));

  const JsonValue& second = accounts->Items()[1];
  CHECK(second.WholeAt("account_id") == 2'000'001);
  CHECK(second.TruthAt("banned"));
  // Une date vide est une réponse : ce compte ne s'est jamais connecté.
  CHECK(second.TextAt("last_login").empty());
}

TEST_CASE("JSON : les accents arrivent ÉCHAPPÉS, et se décodent", "[app][json]") {
  // 🔴 LE SITE APPELLE `json_encode` SANS `JSON_UNESCAPED_UNICODE`. Tout
  // caractère non ASCII traverse donc le fil sous la forme `\uXXXX`, et un
  // lecteur qui l'ignorerait montrerait au joueur « Réessaie dans 3
  // minute(s) » à la place de son message.
  const auto read = ParseJson(R"({"ok":false,"error":"R\u00e9essaie dans 3 minute(s)."})");
  REQUIRE(read.has_value());
  CHECK_FALSE(read->TruthAt("ok", true));
  CHECK(read->TextAt("error") == "Réessaie dans 3 minute(s).");
}

TEST_CASE("JSON : un caractère hors du plan de base tient en DEUX échappements", "[app][json]") {
  // ⚠ UN PSEUDONYME PEUT PORTER UN EMOJI, et le site l'échappe en deux moitiés
  // — un substitut haut puis un substitut bas — qui se recombinent. Les lire
  // séparément rendrait deux losanges là où il y a un caractère.
  const auto read = ParseJson(R"({"label":"lune \ud83c\udf19"})");
  REQUIRE(read.has_value());
  // ⚠ ON COMPARE DES OCTETS, ET ON LES NOMME : une comparaison de chaînes
  // afficherait, quand elle tombe, deux séquences que le terminal ne sait pas
  // rendre — donc « attendu ??? , obtenu ??? ». Les quatre octets d'un
  // caractère recombiné se lisent en nombres.
  const std::string_view moon = read->TextAt("label");
  REQUIRE(moon.size() == 9);  // « lune » + espace + quatre octets
  CHECK(static_cast<unsigned char>(moon[5]) == 0xF0);
  CHECK(static_cast<unsigned char>(moon[6]) == 0x9F);
  CHECK(static_cast<unsigned char>(moon[7]) == 0x8C);
  CHECK(static_cast<unsigned char>(moon[8]) == 0x99);
}

TEST_CASE("JSON : un substitut orphelin ne casse pas la lecture", "[app][json]") {
  // La moitié d'un caractère n'est pas un caractère : elle se remplace, et la
  // réponse reste lisible. Refuser toute la réponse pour un octet abîmé dans
  // un pseudonyme empêcherait de se connecter.
  const auto read = ParseJson(R"({"label":"seul \ud83c fin"})");
  REQUIRE(read.has_value());
  CHECK(read->TextAt("label") == "seul \xEF\xBF\xBD fin");
}

TEST_CASE("JSON : une imbrication sans fond est REFUSÉE, pas suivie", "[app][json]") {
  // 🔴 UNE DESCENTE RÉCURSIVE SUR DES OCTETS DU RÉSEAU DÉBORDE LA PILE si rien
  // ne la borne, et mille crochets ouvrants tiennent dans un paquet. Ce n'est
  // pas une donnée corrompue qu'on rattrape : c'est le programme qui meurt.
  std::string deep;
  for (int level = 0; level < kJsonMaxDepth + 5; ++level) {
    deep += '[';
  }
  deep += "1";
  for (int level = 0; level < kJsonMaxDepth + 5; ++level) {
    deep += ']';
  }
  CHECK_FALSE(ParseJson(deep).has_value());

  // Et ce qui tient dans la borne passe : la réponse du site en emploie trois.
  CHECK(ParseJson(R"({"a":[{"b":1}]})").has_value());
}

TEST_CASE("JSON : une réponse TRONQUÉE est refusée", "[app][json]") {
  // ⚠ C'EST LE CAS D'UNE COUPURE EN PLEIN TRANSFERT, et le plus dangereux :
  // une liste de comptes coupée en deux se lirait comme une liste courte, et
  // le joueur ne verrait pas le compte qu'il cherche sans savoir pourquoi.
  CHECK_FALSE(ParseJson(R"({"ok":true,"accounts":[{"account_id":1)").has_value());
  CHECK_FALSE(ParseJson(R"({"ok":tr)").has_value());
  CHECK_FALSE(ParseJson(R"({"error":"coupée au milieu)").has_value());
  CHECK_FALSE(ParseJson("").has_value());
}

TEST_CASE("JSON : ce qui suit la valeur de tête est une erreur", "[app][json]") {
  // Un lecteur qui s'arrêterait à la première valeur accepterait une réponse
  // où une page d'erreur HTML suit le JSON — ce que fait un serveur qui émet
  // une notice après coup.
  CHECK_FALSE(ParseJson(R"({"ok":true} <br />Notice: undefined index)").has_value());
  // Les blancs, eux, ne comptent pas.
  CHECK(ParseJson("  {\"ok\":true}  \n").has_value());
}

TEST_CASE("JSON : un genre demandé sur un autre rend le défaut, jamais une conversion",
          "[app][json]") {
  // ⚠ LE SITE RÉPOND EN TYPES, et une conversion silencieuse masquerait un
  // changement de sa part : « "2000000" » au lieu de 2000000 se lirait comme
  // un identifiant valide, et le jour où il devient « deux millions » le
  // client se connecterait au compte zéro.
  const auto read = ParseJson(R"({"account_id":"2000000","ok":"true","error":42})");
  REQUIRE(read.has_value());
  CHECK(read->WholeAt("account_id", -1) == -1);
  CHECK_FALSE(read->TruthAt("ok"));
  CHECK(read->TextAt("error").empty());
  // Et un champ absent rend le défaut, sans rien supposer.
  CHECK(read->WholeAt("absent", 7) == 7);
  CHECK(read->TextAt("absent").empty());
  CHECK(read->Field("absent") == nullptr);
}

TEST_CASE("JSON : un identifiant à virgule n'est pas un identifiant", "[app][json]") {
  // Tronquer en silence rendrait un compte VOISIN — le pire des résultats,
  // puisqu'il existe.
  const auto read = ParseJson(R"({"account_id":2000000.5,"rond":2000000.0})");
  REQUIRE(read.has_value());
  CHECK(read->WholeAt("account_id", -1) == -1);
  // Un nombre écrit avec une décimale nulle reste un entier.
  CHECK(read->WholeAt("rond", -1) == 2'000'000);
}
