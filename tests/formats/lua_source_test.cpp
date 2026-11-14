#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/formats/lua_table.h"

#include "support/lua_builder.h"

using ro::core::ErrorCode;
using ro::formats::LuaTable;
using ro::formats::LuaValue;
using ro::formats::ReadLuaChunk;
using ro::formats::ReadLuaFile;
using ro::formats::ReadLuaSource;
using ro::test_support::LuaChunkBuilder;

namespace {

// La table nommée `name`, ou nullptr si elle manque ou n'est pas une table.
const LuaTable* TableOf(const LuaTable& globals, std::string_view name) {
  const LuaValue* value = globals.Find(name);
  if (value == nullptr || value->kind != LuaValue::Kind::kTable) {
    return nullptr;
  }
  return value->table.get();
}

std::vector<std::uint8_t> Bytes(std::string_view text) {
  return std::vector<std::uint8_t>(text.begin(), text.end());
}

}  // namespace

TEST_CASE("Lua en clair : une table nommée devient une globale", "[formats][lua]") {
  const auto globals = ReadLuaSource("Carte = {\n\t[\"BG_Color\"] = { 102, 153, 204 },\n}\n");
  REQUIRE(globals.has_value());
  const LuaTable* map = TableOf(*globals, "Carte");
  REQUIRE(map != nullptr);
  const LuaValue* color = map->Find("BG_Color");
  REQUIRE(color != nullptr);
  REQUIRE(color->table != nullptr);
  // Les valeurs sans clé se numérotent à partir de UN, comme en Lua.
  REQUIRE(color->table->Find(1.0) != nullptr);
  CHECK(color->table->Find(1.0)->number == 102.0);
  CHECK(color->table->Find(2.0)->number == 153.0);
  CHECK(color->table->Find(3.0)->number == 204.0);
  CHECK(color->table->Find(0.0) == nullptr);
  CHECK(color->table->Find(4.0) == nullptr);
}

TEST_CASE("Lua en clair : une clé se résout à travers les globales déjà lues", "[formats][lua]") {
  // C'est ainsi que le client enchaîne ses fichiers : NPCIdentity nomme les
  // identifiants, jobName les emploie comme clés.
  auto first = ReadLuaSource("jobtbl = {\n\tJT_PORING = 1002,\n\tJT_CHONCHON = 1011\n}\n");
  REQUIRE(first.has_value());
  const auto second = ReadLuaSource(
      "JobNameTable = {\n"
      "\t[jobtbl.JT_PORING] = \"Poring\",\n"
      "\t[jobtbl.JT_CHONCHON] = \"Chocho\"\n"
      "}\n",
      std::move(*first));
  REQUIRE(second.has_value());
  CHECK(TableOf(*second, "jobtbl") != nullptr);
  const LuaTable* names = TableOf(*second, "JobNameTable");
  REQUIRE(names != nullptr);
  REQUIRE(names->Find(1002.0) != nullptr);
  CHECK(names->Find(1002.0)->text == "Poring");
  REQUIRE(names->Find(1011.0) != nullptr);
  CHECK(names->Find(1011.0)->text == "Chocho");
}

TEST_CASE("Lua en clair : un identifiant que rien n'a nommé refuse le fichier", "[formats][lua]") {
  // Lua refuse une clé nulle, et le client n'aurait donc pas chargé la table :
  // la lire à moitié ferait porter à un monstre le sprite d'un autre.
  const auto absent_key = ReadLuaSource(
      "jobtbl = { JT_PORING = 1002 }\nJobNameTable = {\n\t[jobtbl.JT_ABSENT] = \"x\"\n}\n");
  REQUIRE_FALSE(absent_key.has_value());
  CHECK(absent_key.error().code == ErrorCode::kCorruptData);
  CHECK(absent_key.error().detail.starts_with("ligne 3 "));

  // Indexer une globale que rien n'a posée : jobName lu sans NPCIdentity.
  const auto absent_table = ReadLuaSource("JobNameTable = { [jobtbl.JT_PORING] = \"Poring\" }\n");
  REQUIRE_FALSE(absent_table.has_value());
  CHECK(absent_table.error().code == ErrorCode::kCorruptData);
  CHECK(absent_table.error().detail.starts_with("ligne 1 "));
}

TEST_CASE("Lua en clair : une fonction reste opaque, quel que soit son corps", "[formats][lua]") {
  // Le corps emploie tout ce qui ouvre et ferme un bloc : c'est lui qui
  // éprouve le comptage, et un `end` dans un commentaire ne doit pas compter.
  const auto globals = ReadLuaSource(
      "function ReqJobName(jobID)\n"
      "\tlocal name = JobNameTable[jobID]\n"
      "\tif name == nil then\n"
      "\t\tfor i = 1, 2 do end\n"
      "\t\twhile false do end\n"
      "\t\trepeat until true\n"
      "\t\treturn \"end\" -- end\n"
      "\telseif name then\n"
      "\t\treturn name\n"
      "\telse\n"
      "\t\treturn 1 + 2\n"
      "\tend\n"
      "end\n"
      "T = { 1 }\n"
      "GetJobString = function(a) return function() end end\n");
  REQUIRE(globals.has_value());
  const LuaValue* function = globals->Find("ReqJobName");
  REQUIRE(function != nullptr);
  CHECK(function->kind == LuaValue::Kind::kFunction);
  const LuaTable* table = TableOf(*globals, "T");
  REQUIRE(table != nullptr);
  CHECK(table->Find(1.0) != nullptr);
  const LuaValue* assigned = globals->Find("GetJobString");
  REQUIRE(assigned != nullptr);
  CHECK(assigned->kind == LuaValue::Kind::kFunction);
}

TEST_CASE("Lua en clair : les commentaires ne comptent pas", "[formats][lua]") {
  const auto globals = ReadLuaSource(
      "--[[ T = { 99 } ]] T = { 1 } -- T = { 2 }\n"
      "--[==[ ]] T = { 3 }\n"
      "]==]\n"
      "-- ceci est la dernière ligne, sans saut de ligne final");
  REQUIRE(globals.has_value());
  const LuaTable* table = TableOf(*globals, "T");
  REQUIRE(table != nullptr);
  REQUIRE(table->by_index.size() == 1);
  CHECK(table->Find(1.0)->number == 1.0);
}

TEST_CASE("Lua en clair : les chaînes gardent leurs octets et résolvent les échappements",
          "[formats][lua]") {
  // Les noms coréens sont en CP949 : ces octets doivent ressortir tels quels,
  // et `\\` doit devenir UNE barre, celle des sous-dossiers de sprites.
  const std::string source = "N = { \"\xBF\xA9\\\\x\", 'a\\\"b\\n\\65', [[\nab]], [==[a]]b]==] }\n";
  const auto globals = ReadLuaSource(source);
  REQUIRE(globals.has_value());
  const LuaTable* table = TableOf(*globals, "N");
  REQUIRE(table != nullptr);
  REQUIRE(table->Find(1.0) != nullptr);
  CHECK(table->Find(1.0)->text == std::string("\xBF\xA9\\x"));
  REQUIRE(table->Find(2.0) != nullptr);
  CHECK(table->Find(2.0)->text == "a\"b\nA");
  REQUIRE(table->Find(3.0) != nullptr);
  // Lua ignore le saut de ligne collé au crochet ouvrant.
  CHECK(table->Find(3.0)->text == "ab");
  REQUIRE(table->Find(4.0) != nullptr);
  CHECK(table->Find(4.0)->text == "a]]b");
}

TEST_CASE("Lua en clair : les nombres se lisent sous toutes leurs formes", "[formats][lua]") {
  const auto globals = ReadLuaSource("T = { -1, 0x10, 1.5, 2e3, .5, 7 }\n");
  REQUIRE(globals.has_value());
  const LuaTable* table = TableOf(*globals, "T");
  REQUIRE(table != nullptr);
  REQUIRE(table->by_index.size() == 6);
  CHECK(table->Find(1.0)->number == -1.0);
  CHECK(table->Find(2.0)->number == 16.0);
  CHECK(table->Find(3.0)->number == 1.5);
  CHECK(table->Find(4.0)->number == 2000.0);
  CHECK(table->Find(5.0)->number == 0.5);
  CHECK(table->Find(6.0)->number == 7.0);
}

TEST_CASE("Lua en clair : ce qui n'est pas une table se refuse", "[formats][lua]") {
  // Le même refus que pour un opcode hors cadre : un fichier qui calcule ou
  // appelle n'est pas un fichier de données, et se lirait de travers.
  for (const std::string_view source :
       {"local x = 1\n", "print(1 + 2)\n", "T = 1 + 2\n", "T = { a = 1 .. \"b\" }\n",
        "if x then end\n", "return 1\n"}) {
    const auto globals = ReadLuaSource(source);
    REQUIRE_FALSE(globals.has_value());
    CHECK(globals.error().code == ErrorCode::kUnsupportedVersion);
  }
  // Une syntaxe cassée est une autre erreur : le fichier est abîmé.
  const auto broken = ReadLuaSource("T = { 1, 2\n");
  REQUIRE_FALSE(broken.has_value());
  CHECK(broken.error().code == ErrorCode::kCorruptData);
}

TEST_CASE("Lua en clair : les valeurs sans clé suivent la numérotation de Lua", "[formats][lua]") {
  const auto globals = ReadLuaSource(
      "A = { x = 5, y = 6 }\n"
      "T = { A.x, A.y, [10] = 3, 7; A[\"x\"], nil, 9 }\n");
  REQUIRE(globals.has_value());
  const LuaTable* table = TableOf(*globals, "T");
  REQUIRE(table != nullptr);
  REQUIRE(table->Find(1.0) != nullptr);
  CHECK(table->Find(1.0)->number == 5.0);
  CHECK(table->Find(2.0)->number == 6.0);
  CHECK(table->Find(10.0)->number == 3.0);
  CHECK(table->Find(3.0)->number == 7.0);
  CHECK(table->Find(4.0)->number == 5.0);
  // Une valeur nulle consomme son rang sans rien poser.
  CHECK(table->Find(5.0) == nullptr);
  REQUIRE(table->Find(6.0) != nullptr);
  CHECK(table->Find(6.0)->number == 9.0);
}

TEST_CASE("Lua en clair : une clé posée deux fois garde la dernière valeur", "[formats][lua]") {
  const auto globals = ReadLuaSource("T = { a = 1, a = 2 }\nX = 1\nX = nil\n");
  REQUIRE(globals.has_value());
  const LuaTable* table = TableOf(*globals, "T");
  REQUIRE(table != nullptr);
  REQUIRE(table->by_name.size() == 1);
  CHECK(table->Find("a")->number == 2.0);
  // Une globale mise à nil disparaît, comme en Lua.
  CHECK(globals->Find("X") == nullptr);
}

TEST_CASE("Lua en clair : trop de tables imbriquées refuse le fichier", "[formats][lua]") {
  std::string source = "T = ";
  for (int depth = 0; depth < ro::formats::kMaxLuaTableDepth + 2; ++depth) {
    source += "{";
  }
  for (int depth = 0; depth < ro::formats::kMaxLuaTableDepth + 2; ++depth) {
    source += "}";
  }
  const auto globals = ReadLuaSource(source);
  REQUIRE_FALSE(globals.has_value());
  CHECK(globals.error().code == ErrorCode::kCorruptData);
}

TEST_CASE("Lua en clair : des index imbriqués sans fin refusent le fichier", "[formats][lua]") {
  // `A[A[A[…` n'ouvre aucune accolade : sans plafond sur l'expression, cent
  // mille niveaux feraient déborder la pile au lieu de rendre une erreur.
  constexpr int kHostileDepth = 100'000;
  std::string source = "A = {}\nX = ";
  for (int depth = 0; depth < kHostileDepth; ++depth) {
    source += "A[";
  }
  source += "1";
  source.append(kHostileDepth, ']');
  const auto globals = ReadLuaSource(source);
  REQUIRE_FALSE(globals.has_value());
  CHECK(globals.error().code == ErrorCode::kCorruptData);
}

TEST_CASE("Lua : un fichier se lit selon sa signature", "[formats][lua]") {
  // Compilé : la signature de Lua ouvre le chunk.
  LuaChunkBuilder builder;
  const std::uint32_t name = builder.AddString("Compile");
  builder.NewTable(0);
  builder.SetGlobal(0, name);
  builder.Return();
  const auto compiled = ReadLuaFile(builder.Build());
  REQUIRE(compiled.has_value());
  CHECK(TableOf(*compiled, "Compile") != nullptr);

  // En clair : tout le reste est du texte.
  const auto plain = ReadLuaFile(Bytes("Clair = { 1 }\n"));
  REQUIRE(plain.has_value());
  CHECK(TableOf(*plain, "Clair") != nullptr);
}

TEST_CASE("Lua : un chunk s'évalue dans les globales déjà lues", "[formats][lua]") {
  auto first = ReadLuaSource("Avant = { 1 }\n");
  REQUIRE(first.has_value());

  LuaChunkBuilder builder;
  const std::uint32_t name = builder.AddString("Apres");
  builder.NewTable(0);
  builder.SetGlobal(0, name);
  builder.Return();
  const auto both = ReadLuaChunk(builder.Build(), std::move(*first));
  REQUIRE(both.has_value());
  CHECK(TableOf(*both, "Avant") != nullptr);
  CHECK(TableOf(*both, "Apres") != nullptr);
}

TEST_CASE("Lua en clair : un appel de premier niveau se garde", "[formats][lua]") {
  // ⚠ IL NE S'EXÉCUTE PAS, ET IL NE SE JETTE PAS. Certains de ces fichiers
  // portent de la DONNÉE sous forme d'appels — `skilltreeview` nomme les onglets
  // du grimoire ainsi, et sa grille vit dans le même fichier. Le refuser perdrait
  // les deux ; le sauter perdrait les noms sans que personne ne le sache.
  auto globals = ReadLuaSource(
      "T = { [1] = 2 }\n"
      "setmetatable(T, (T))\n"
      "JobSkillTab.ChangeSkillTabName(3, \"1st\", \"2nd\")\n"
      "U = { 3 }\n");
  REQUIRE(globals.has_value());
  // Les tables autour de l'appel se lisent normalement : c'est tout l'objet.
  CHECK(globals->Find("T") != nullptr);
  CHECK(globals->Find("U") != nullptr);

  REQUIRE(globals->calls.size() == 2);
  // Le nom garde ses points : c'est lui qui dit de quoi l'appel parle.
  CHECK(globals->calls[0].name == "setmetatable");
  CHECK(globals->calls[1].name == "JobSkillTab.ChangeSkillTabName");
  REQUIRE(globals->calls[1].arguments.size() == 3);
  CHECK(globals->calls[1].arguments[0].number == 3.0);
  CHECK(globals->calls[1].arguments[1].text == "1st");
  CHECK(globals->calls[1].arguments[2].text == "2nd");

  // Un argument CALCULÉ refuse le fichier, comme partout ailleurs : on ne devine
  // pas ce qu'on ne sait pas évaluer.
  auto computed = ReadLuaSource("T = {}\ntable.insert(T, 1 + 2)\n");
  REQUIRE_FALSE(computed.has_value());
  CHECK(computed.error().code == ErrorCode::kUnsupportedVersion);

  // Un appel inachevé est un fichier ABÎMÉ, pas un fichier hors cadre.
  auto unclosed = ReadLuaSource("T = {}\nsetmetatable(T\n");
  REQUIRE_FALSE(unclosed.has_value());
  CHECK(unclosed.error().code == ErrorCode::kCorruptData);
}

TEST_CASE("Lua en clair : une table se remplit aussi APRÈS avoir été posée", "[formats][lua]") {
  // 🔴 C'EST LA FORME DE `stateiconinfo` : une table vide, puis huit cents
  // affectations indexées. Un lecteur qui n'accepte que les constructeurs refuse
  // le fichier à sa première ligne utile, et perd les huit cents.
  auto globals = ReadLuaSource(
      "Noms = {}\n"
      "IDs = { PREMIER = 1, SECOND = 2 }\n"
      "Noms[IDs.PREMIER] = { titre = \"un\" }\n"
      "Noms[IDs.SECOND] = { titre = \"deux\" }\n"
      "Noms[\"texte\"] = 7\n");
  REQUIRE(globals.has_value());

  const LuaTable* name_table = TableOf(*globals, "Noms");
  REQUIRE(name_table != nullptr);
  const LuaValue* first_name = name_table->Find(1.0);
  REQUIRE(first_name != nullptr);
  REQUIRE(first_name->table != nullptr);
  REQUIRE(first_name->table->Find("titre") != nullptr);
  CHECK(first_name->table->Find("titre")->text == "un");

  const LuaValue* second = name_table->Find(2.0);
  REQUIRE(second != nullptr);
  REQUIRE(second->table != nullptr);
  CHECK(second->table->Find("titre")->text == "deux");

  // Une clé TEXTE se pose aussi : c'est la même instruction.
  REQUIRE(name_table->Find("texte") != nullptr);
  CHECK(name_table->Find("texte")->number == 7.0);
}

TEST_CASE("Lua en clair : écrire dans une table que rien n'a posée se refuse", "[formats][lua]") {
  // ⚠ EN LUA C'EST UNE ERREUR D'EXÉCUTION, et ici c'est le symptôme d'un fichier
  // lu dans le mauvais ordre. La créer au vol masquerait précisément cela.
  const auto absente = ReadLuaSource("Noms[1] = 2\n");
  CHECK_FALSE(absente.has_value());

  // Et écrire dans une valeur qui n'est PAS une table se refuse de même.
  const auto not_a_table = ReadLuaSource(
      "Noms = 3\n"
      "Noms[1] = 2\n");
  CHECK_FALSE(not_a_table.has_value());
}

TEST_CASE("Lua en clair : une clé indexée que rien n'a nommée refuse le fichier",
          "[formats][lua]") {
  // ⚠ LA MÊME RÈGLE QUE DANS UN CONSTRUCTEUR : une clé nulle veut presque
  // toujours dire qu'un fichier précédent n'a pas été lu.
  constexpr std::string_view kSource =
      "Noms = {}\n"
      "IDs = { CONNU = 1 }\n"
      "Noms[IDs.CONNU] = \"un\"\n"
      "Noms[IDs.ABSENT] = \"personne\"\n"
      "Noms[IDs.CONNU] = \"un bis\"\n";

  CHECK_FALSE(ReadLuaSource(kSource).has_value());
}
