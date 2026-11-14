#include "ro/formats/lua_table.h"

#include <cstdint>
#include <limits>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "support/lua_builder.h"

using ro::formats::LuaTable;
using ro::formats::LuaValue;
using ro::formats::ReadLuaChunk;
using ro::test_support::LuaChunkBuilder;

namespace {

// « Carte = { ["BG_Color"] = { 102, 153, 204 } } », tel que le compilateur Lua
// l'écrirait : une table pour la carte, une table pour la couleur, la seconde
// remplie par SETLIST puisque ses clés se suivent.
std::vector<std::uint8_t> SkyChunk() {
  LuaChunkBuilder builder;
  const std::uint32_t name = builder.AddString("Carte");
  const std::uint32_t field = builder.AddString("BG_Color");
  const std::uint32_t red = builder.AddNumber(102.0);
  const std::uint32_t green = builder.AddNumber(153.0);
  const std::uint32_t blue = builder.AddNumber(204.0);

  builder.set_max_stack(8);
  builder.NewTable(0);  // la carte
  builder.NewTable(1);  // la couleur
  builder.LoadConstant(2, red);
  builder.LoadConstant(3, green);
  builder.LoadConstant(4, blue);
  builder.SetList(1, 3, 1);                                    // couleur[1..3]
  builder.SetTable(0, LuaChunkBuilder::kConstant | field, 1);  // carte.BG_Color
  builder.SetGlobal(0, name);
  builder.Return();
  return builder.Build();
}

}  // namespace

TEST_CASE("Lua : une table nommée devient une globale", "[formats][lua]") {
  const auto globals = ReadLuaChunk(SkyChunk());
  REQUIRE(globals.has_value());

  const LuaValue* map = globals->Find("Carte");
  REQUIRE(map != nullptr);
  REQUIRE(map->kind == LuaValue::Kind::kTable);
  REQUIRE(map->table != nullptr);

  const LuaValue* color = map->table->Find("BG_Color");
  REQUIRE(color != nullptr);
  REQUIRE(color->kind == LuaValue::Kind::kTable);
  REQUIRE(color->table != nullptr);

  // SETLIST numérote à partir de UN, pas de zéro : c'est la convention de Lua,
  // et se tromper décalerait chaque composante d'une couleur sur sa voisine.
  const LuaValue* red = color->table->Find(1.0);
  const LuaValue* green = color->table->Find(2.0);
  const LuaValue* blue = color->table->Find(3.0);
  REQUIRE(red != nullptr);
  REQUIRE(green != nullptr);
  REQUIRE(blue != nullptr);
  CHECK(red->number == 102.0);
  CHECK(green->number == 153.0);
  CHECK(blue->number == 204.0);
  CHECK(color->table->Find(0.0) == nullptr);
}

TEST_CASE("Lua : les lots de SETLIST se suivent", "[formats][lua]") {
  // Le compilateur vide la pile toutes les cinquante valeurs. Le rang du lot
  // décide donc de la clé, et l'ignorer écraserait les cinquante premières.
  LuaChunkBuilder builder;
  const std::uint32_t name = builder.AddString("Liste");
  const std::uint32_t value = builder.AddNumber(7.0);

  builder.set_max_stack(4);
  builder.NewTable(0);
  builder.LoadConstant(1, value);
  builder.SetList(0, 1, 3);  // troisième lot : la clé vaut 2 x 50 + 1
  builder.SetGlobal(0, name);
  builder.Return();

  const auto globals = ReadLuaChunk(builder.Build());
  REQUIRE(globals.has_value());
  const LuaValue* list = globals->Find("Liste");
  REQUIRE(list != nullptr);
  REQUIRE(list->table != nullptr);
  CHECK(list->table->Find(101.0) != nullptr);
  CHECK(list->table->Find(1.0) == nullptr);
}

TEST_CASE("Lua : un lot trop grand se lit dans le mot suivant", "[formats][lua]") {
  // Au-delà de ce que C peut porter, le rang du lot occupe le mot suivant, qui
  // n'est alors PAS une instruction. L'exécuter reviendrait à interpréter un
  // nombre, ce qui ne se voit pas : le mot ressemble à une instruction valide.
  LuaChunkBuilder builder;
  const std::uint32_t name = builder.AddString("Loin");
  const std::uint32_t value = builder.AddNumber(1.0);

  builder.set_max_stack(4);
  builder.NewTable(0);
  builder.LoadConstant(1, value);
  builder.SetListExtended(0, 1, 600);
  builder.SetGlobal(0, name);
  builder.Return();

  const auto globals = ReadLuaChunk(builder.Build());
  REQUIRE(globals.has_value());
  const LuaValue* list = globals->Find("Loin");
  REQUIRE(list != nullptr);
  REQUIRE(list->table != nullptr);
  CHECK(list->table->Find(599.0 * 50.0 + 1.0) != nullptr);
}

TEST_CASE("Lua : une fonction reste opaque et ses valeurs externes se sautent", "[formats][lua]") {
  // Les pseudo-instructions qui suivent CLOSURE décrivent les valeurs externes
  // et ne s'exécutent pas. Les prendre pour du code ferait ici écraser le
  // registre où la table vient d'être posée.
  LuaChunkBuilder builder;
  const std::uint32_t kept = builder.AddString("Gardee");
  const std::uint32_t routine = builder.AddString("Routine");
  const std::uint32_t marker = builder.AddNumber(42.0);

  builder.set_max_stack(4);
  builder.set_proto_count(1);
  builder.set_proto_upvalues(2);
  builder.NewTable(0);
  builder.LoadConstant(1, marker);
  builder.SetList(0, 1, 1);
  builder.Closure(2, 0, 2);
  builder.SetGlobal(2, routine);
  builder.SetGlobal(0, kept);
  builder.Return();

  const auto globals = ReadLuaChunk(builder.Build());
  REQUIRE(globals.has_value());

  const LuaValue* function = globals->Find("Routine");
  REQUIRE(function != nullptr);
  CHECK(function->kind == LuaValue::Kind::kFunction);

  const LuaValue* table = globals->Find("Gardee");
  REQUIRE(table != nullptr);
  REQUIRE(table->table != nullptr);
  CHECK(table->table->Find(1.0) != nullptr);
}

TEST_CASE("Lua : une clé posée deux fois garde la DERNIÈRE valeur", "[formats][lua]") {
  LuaChunkBuilder builder;
  const std::uint32_t name = builder.AddString("Table");
  const std::uint32_t key = builder.AddString("champ");
  const std::uint32_t first = builder.AddNumber(1.0);
  const std::uint32_t second = builder.AddNumber(2.0);

  builder.set_max_stack(4);
  builder.NewTable(0);
  builder.SetTable(0, LuaChunkBuilder::kConstant | key, LuaChunkBuilder::kConstant | first);
  builder.SetTable(0, LuaChunkBuilder::kConstant | key, LuaChunkBuilder::kConstant | second);
  builder.SetGlobal(0, name);
  builder.Return();

  const auto globals = ReadLuaChunk(builder.Build());
  REQUIRE(globals.has_value());
  const LuaValue* table = globals->Find("Table");
  REQUIRE(table != nullptr);
  REQUIRE(table->table != nullptr);
  REQUIRE(table->table->by_name.size() == 1);
  const LuaValue* field = table->table->Find("champ");
  REQUIRE(field != nullptr);
  CHECK(field->number == 2.0);
}

TEST_CASE("Lua : une valeur nulle EFFACE la clé", "[formats][lua]") {
  // C'est la sémantique de Lua. Garder l'entrée ferait répondre « présent » à
  // une recherche portant sur ce que le fichier a justement retiré.
  LuaChunkBuilder builder;
  const std::uint32_t name = builder.AddString("Table");
  const std::uint32_t key = builder.AddString("champ");
  const std::uint32_t value = builder.AddNumber(1.0);
  const std::uint32_t nothing = builder.AddNil();

  builder.set_max_stack(4);
  builder.NewTable(0);
  builder.SetTable(0, LuaChunkBuilder::kConstant | key, LuaChunkBuilder::kConstant | value);
  builder.SetTable(0, LuaChunkBuilder::kConstant | key, LuaChunkBuilder::kConstant | nothing);
  builder.SetGlobal(0, name);
  builder.Return();

  const auto globals = ReadLuaChunk(builder.Build());
  REQUIRE(globals.has_value());
  const LuaValue* table = globals->Find("Table");
  REQUIRE(table != nullptr);
  REQUIRE(table->table != nullptr);
  CHECK(table->table->Find("champ") == nullptr);
}

TEST_CASE("Lua : lire une table à la clé nil rend nil, pas l'entrée zéro", "[formats][lua]") {
  // « Lu = Table[CONSTANTE_INCONNUE] » : la constante vaut nil. Chercher son
  // champ numérique, qui vaut zéro, rendrait en silence l'entrée [0].
  LuaChunkBuilder builder;
  const std::uint32_t table_name = builder.AddString("Table");
  const std::uint32_t read_name = builder.AddString("Lu");
  const std::uint32_t zero = builder.AddNumber(0.0);
  const std::uint32_t novice = builder.AddNumber(7.0);
  const std::uint32_t nothing = builder.AddNil();

  builder.set_max_stack(4);
  builder.NewTable(0);
  builder.SetTable(0, LuaChunkBuilder::kConstant | zero, LuaChunkBuilder::kConstant | novice);
  builder.SetGlobal(0, table_name);
  builder.GetTable(1, 0, LuaChunkBuilder::kConstant | nothing);
  builder.SetGlobal(1, read_name);
  builder.Return();

  const auto globals = ReadLuaChunk(builder.Build());
  REQUIRE(globals.has_value());
  const LuaValue* read = globals->Find("Lu");
  CHECK((read == nullptr || read->kind == LuaValue::Kind::kNil));
}

TEST_CASE("Lua : ce qui n'est pas un chunk 5.1 se refuse", "[formats][lua]") {
  CHECK_FALSE(ReadLuaChunk(std::vector<std::uint8_t>{}).has_value());
  CHECK_FALSE(ReadLuaChunk(std::vector<std::uint8_t>{'n', 'o', 'n'}).has_value());
  // Une autre version : le format a changé après la 5.1, et le lire comme tel
  // rendrait des tables plausibles et fausses.
  CHECK_FALSE(ReadLuaChunk(LuaChunkBuilder::Header(0x53)).has_value());
  // Une autre machine : gros-boutiste, ou des nombres sur quatre octets.
  CHECK_FALSE(ReadLuaChunk(LuaChunkBuilder::Header(0x51, 0)).has_value());
  CHECK_FALSE(ReadLuaChunk(LuaChunkBuilder::Header(0x51, 1, 4)).has_value());
  // L'en-tête seul, sans la fonction qui devrait suivre.
  CHECK_FALSE(ReadLuaChunk(LuaChunkBuilder::Header()).has_value());
}

TEST_CASE("Lua : un chunk qui CALCULE se refuse", "[formats][lua]") {
  // Le lecteur ne sait construire que des tables. Un fichier qui fait autre
  // chose doit se refuser plutôt que rendre le résultat partiel de ce qu'il a
  // compris — personne ne saurait alors ce qui manque.
  LuaChunkBuilder builder;
  builder.set_max_stack(4);
  builder.NewTable(0);
  builder.RawInstruction(12);  // ADD
  builder.Return();
  CHECK_FALSE(ReadLuaChunk(builder.Build()).has_value());
}

TEST_CASE("Lua : un registre hors de la pile se refuse", "[formats][lua]") {
  // La pile déclarée borne ce que le chunk peut toucher. Un index au-delà vient
  // d'un fichier abîmé, et le suivre lirait de la mémoire au hasard.
  LuaChunkBuilder builder;
  const std::uint32_t value = builder.AddNumber(1.0);
  builder.set_max_stack(2);
  builder.LoadConstant(200, value);
  builder.Return();
  CHECK_FALSE(ReadLuaChunk(builder.Build()).has_value());
}

TEST_CASE("Lua : une constante hors table se refuse", "[formats][lua]") {
  LuaChunkBuilder builder;
  builder.set_max_stack(2);
  builder.LoadConstant(0, 99);
  builder.Return();
  CHECK_FALSE(ReadLuaChunk(builder.Build()).has_value());
}

TEST_CASE("Lua : poser une clé dans autre chose qu'une table se refuse", "[formats][lua]") {
  LuaChunkBuilder builder;
  const std::uint32_t key = builder.AddString("champ");
  const std::uint32_t value = builder.AddNumber(1.0);
  builder.set_max_stack(4);
  builder.LoadConstant(0, value);  // un nombre, pas une table
  builder.SetTable(0, LuaChunkBuilder::kConstant | key, LuaChunkBuilder::kConstant | value);
  builder.Return();
  CHECK_FALSE(ReadLuaChunk(builder.Build()).has_value());
}

TEST_CASE("Lua : un nombre ne devient identifiant que s'il en est un", "[formats][lua]") {
  // 🔴 UN `static_cast` D'UN DOUBLE HORS DE PLAGE EST INDÉFINI, et ces nombres
  // viennent de fichiers que l'on ne contrôle pas.
  using ro::formats::LuaWholeNumber;
  constexpr std::uint32_t kSixteenBits = 65'535;
  CHECK(LuaWholeNumber(501.0, kSixteenBits) == 501U);
  CHECK(LuaWholeNumber(0.0, kSixteenBits) == 0U);
  CHECK(LuaWholeNumber(kSixteenBits, kSixteenBits) == kSixteenBits);
  CHECK_FALSE(LuaWholeNumber(70'000.0, kSixteenBits).has_value());
  CHECK_FALSE(LuaWholeNumber(5e9, 0xFFFF'FFFFU).has_value());
  CHECK_FALSE(LuaWholeNumber(-1.0, kSixteenBits).has_value());
  CHECK_FALSE(LuaWholeNumber(1.5, kSixteenBits).has_value());
  CHECK_FALSE(LuaWholeNumber(std::numeric_limits<double>::quiet_NaN(), kSixteenBits).has_value());
  CHECK_FALSE(LuaWholeNumber(std::numeric_limits<double>::infinity(), kSixteenBits).has_value());
}

TEST_CASE("Lua compilé : une clé nulle refuse le fichier, comme en clair", "[formats][lua]") {
  LuaChunkBuilder builder;
  const std::uint32_t name = builder.AddString("Table");
  const std::uint32_t kept_key = builder.AddNumber(1.0);
  const std::uint32_t value = builder.AddNumber(7.0);
  const std::uint32_t nothing = builder.AddNil();

  builder.set_max_stack(4);
  builder.NewTable(0);
  builder.SetTable(0, LuaChunkBuilder::kConstant | nothing, LuaChunkBuilder::kConstant | value);
  builder.SetTable(0, LuaChunkBuilder::kConstant | kept_key, LuaChunkBuilder::kConstant | value);
  builder.SetGlobal(0, name);
  builder.Return();
  const std::vector<std::uint8_t> chunk = builder.Build();

  CHECK_FALSE(ReadLuaChunk(chunk).has_value());
}
