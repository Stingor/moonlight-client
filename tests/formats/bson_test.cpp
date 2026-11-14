#include "ro/formats/bson.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "support/bson_builder.h"

using ro::core::ErrorCode;
using ro::formats::BsonDocument;
using ro::formats::BsonValue;
using ro::formats::ParseBson;
using ro::test_support::BsonBuilder;

TEST_CASE("BSON : les six sortes que l'installation emploie se relisent", "[formats][bson]") {
  // Les types employés par les trois fichiers `.bson` du client, et eux seuls :
  // double, chaîne, document, tableau, booléen, entier 32 bits.
  BsonBuilder record;
  record.Text("FilePath", "HYPER_NOVICE\\jack_frost_nova.str")
      .Number("PosY", 12.0)
      .Integer("StartDelayTime", 496)
      .Integer("QuakeDelay", -1)
      .Flag("IsInfinite", true)
      .Flag("IsFloor", false);

  BsonBuilder list;
  list.Text("0", "premier").Text("1", "second");

  BsonBuilder root;
  root.Document("EZ2STREffect", record).Array("liste", list);

  const std::vector<std::uint8_t> bytes = root.Build();
  const auto lu = ParseBson(bytes);
  REQUIRE(lu.has_value());
  REQUIRE(lu->fields.size() == 2);

  const BsonValue* table = lu->Find("EZ2STREffect");
  REQUIRE(table != nullptr);
  REQUIRE(table->kind == BsonValue::Kind::kDocument);

  const BsonDocument& relue = *table->document;
  REQUIRE(relue.Find("FilePath") != nullptr);
  CHECK(relue.Find("FilePath")->kind == BsonValue::Kind::kText);
  CHECK(relue.Find("FilePath")->text == "HYPER_NOVICE\\jack_frost_nova.str");
  CHECK(relue.Find("PosY")->number == 12.0);
  CHECK(relue.Find("StartDelayTime")->integer == 496);
  // Le négatif se relit négatif : QuakeDelay vaut -1 sur les 223 fiches du jeu,
  // et un entier lu sans signe en ferait quatre milliards.
  CHECK(relue.Find("QuakeDelay")->integer == -1);
  CHECK(relue.Find("IsInfinite")->boolean);
  CHECK_FALSE(relue.Find("IsFloor")->boolean);

  // Un tableau se distingue d'un document par sa SORTE, pas par son contenu :
  // BSON l'écrit comme un document aux clés numérotées.
  const BsonValue* list_read = lu->Find("liste");
  REQUIRE(list_read != nullptr);
  CHECK(list_read->kind == BsonValue::Kind::kArray);
  REQUIRE(list_read->document->fields.size() == 2);
  CHECK(list_read->document->Find("1")->text == "second");

  // Un champ absent n'est pas une erreur, c'est une absence.
  CHECK(lu->Find("Absent") == nullptr);
}

TEST_CASE("BSON : l'ordre du fichier est conservé", "[formats][bson]") {
  // Le rang d'une fiche est ce dont le client fait un identifiant. Un lecteur
  // qui rangerait ses champs autrement le perdrait sans rien dire.
  BsonBuilder root;
  root.Integer("zzz", 1).Integer("aaa", 2).Integer("mmm", 3);

  const auto lu = ParseBson(root.Build());
  REQUIRE(lu.has_value());
  REQUIRE(lu->fields.size() == 3);
  CHECK(lu->fields[0].first == "zzz");
  CHECK(lu->fields[1].first == "aaa");
  CHECK(lu->fields[2].first == "mmm");
}

TEST_CASE("BSON : un document vide se lit, un tampon vide non", "[formats][bson]") {
  const BsonBuilder empty_builder;
  const std::vector<std::uint8_t> bytes = empty_builder.Build();
  // Quatre octets de taille et le zéro final : c'est le plus petit document.
  REQUIRE(bytes.size() == 5);
  const auto lu = ParseBson(bytes);
  REQUIRE(lu.has_value());
  CHECK(lu->fields.empty());

  const auto rien = ParseBson({});
  REQUIRE_FALSE(rien.has_value());
  CHECK(rien.error().code == ErrorCode::kUnexpectedEnd);
}

TEST_CASE("BSON : un type hors du sous-ensemble est REFUSÉ, pas sauté", "[formats][bson]") {
  // 0x05 est le type binaire. BSON le définit, aucun fichier de l'installation
  // ne l'emploie, et le sauter demanderait de croire une longueur que l'on n'a
  // jamais éprouvée. Un type inconnu veut dire que la lecture a dérapé.
  constexpr std::array<std::uint8_t, 4> charge = {0, 0, 0, 0};
  BsonBuilder root;
  root.Integer("avant", 7).RawElement(0x05, "binaire", charge);

  const auto lu = ParseBson(root.Build());
  REQUIRE_FALSE(lu.has_value());
  CHECK(lu.error().code == ErrorCode::kUnsupportedVersion);
  // Le diagnostic nomme le champ fautif : c'est ce qui distingue « ce fichier
  // n'est pas du BSON » de « ce fichier en est, et porte un champ de plus ».
  CHECK(lu.error().detail.find("binaire") != std::string::npos);
}

TEST_CASE("BSON : une taille qui ne tombe pas juste est refusée", "[formats][bson]") {
  BsonBuilder root;
  root.Integer("valeur", 1);

  SECTION("annoncée trop courte, le zéro final ne tombe pas au bon endroit") {
    const auto lu = ParseBson(root.BuildWithSizeOffBy(-1));
    REQUIRE_FALSE(lu.has_value());
    CHECK(lu.error().code == ErrorCode::kCorruptData);
  }
  SECTION("annoncée trop longue, elle déborde du tampon") {
    const auto lu = ParseBson(root.BuildWithSizeOffBy(1));
    REQUIRE_FALSE(lu.has_value());
    CHECK(lu.error().code == ErrorCode::kUnexpectedEnd);
  }
}

TEST_CASE("BSON : une chaîne dont la longueur ment est refusée", "[formats][bson]") {
  // La longueur annoncée compte le zéro final. Zéro veut dire qu'il n'y a même
  // pas ce zéro-là : la lecture ne peut pas continuer sur cette base.
  constexpr std::array<std::uint8_t, 4> zero_length = {0, 0, 0, 0};
  BsonBuilder root;
  root.RawElement(0x02, "texte", zero_length);

  const auto lu = ParseBson(root.Build());
  REQUIRE_FALSE(lu.has_value());
  CHECK(lu.error().code == ErrorCode::kCorruptData);
}

TEST_CASE("BSON : un booléen qui n'est ni zéro ni un est refusé", "[formats][bson]") {
  // Un octet qui vaut 42 là où le format n'admet que deux valeurs ne se lit pas
  // « à peu près vrai » : c'est le signe que l'on n'est plus aligné.
  constexpr std::array<std::uint8_t, 1> neither_type = {42};
  BsonBuilder root;
  root.RawElement(0x08, "drapeau", neither_type);

  const auto lu = ParseBson(root.Build());
  REQUIRE_FALSE(lu.has_value());
  CHECK(lu.error().code == ErrorCode::kCorruptData);
}

TEST_CASE("BSON : des octets APRÈS le document racine sont refusés", "[formats][bson]") {
  // La racine occupe tout le fichier. Des octets de rab veulent dire que l'on
  // ne lit pas le fichier que l'on croit — et le lecteur rendrait pourtant une
  // table qui a l'air complète.
  BsonBuilder root;
  root.Integer("valeur", 1);
  std::vector<std::uint8_t> bytes = root.Build();
  bytes.push_back(0xFF);

  const auto lu = ParseBson(bytes);
  REQUIRE_FALSE(lu.has_value());
  CHECK(lu.error().code == ErrorCode::kCorruptData);
}

TEST_CASE("BSON : l'imbrication est BORNÉE", "[formats][bson]") {
  // Ces octets viennent d'une archive que l'on ne contrôle pas. Un document qui
  // se dit imbriqué mille fois ferait déborder la pile d'un lecteur récursif :
  // c'est un plantage là où il faut un refus.
  BsonBuilder fond;
  fond.Integer("fond", 1);
  BsonBuilder pile = fond;
  for (std::size_t etage = 0; etage <= ro::formats::kMaxBsonDepth; ++etage) {
    BsonBuilder dessus;
    dessus.Document("dedans", pile);
    pile = dessus;
  }

  const auto lu = ParseBson(pile.Build());
  REQUIRE_FALSE(lu.has_value());
  CHECK(lu.error().code == ErrorCode::kCorruptData);
}
