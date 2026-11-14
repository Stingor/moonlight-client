#include "ro/formats/lua_calls.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "support/lua_builder.h"

namespace {

std::vector<std::uint8_t> Bytes(std::string_view text) { return {text.begin(), text.end()}; }

using ro::formats::LuaValue;
using ro::formats::ReadLuaFunctionCalls;
using ro::test_support::LuaChunkBuilder;

// L'en-tête d'un chunk : douze octets avant la fonction principale.
constexpr std::size_t kHeaderBytes = 12;
// La queue d'une fonction sans sous-fonction : leur nombre, puis les trois
// tables de débogage vides.
constexpr std::size_t kEmptyTailBytes = 16;
constexpr std::uint32_t kOpCall = 28;

std::uint32_t Call(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
  return kOpCall | (a << 6) | (c << 14) | (b << 23);
}

void PushU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    out.push_back(static_cast<std::uint8_t>(value >> shift));
  }
}

// Le chunk dont la fonction principale pose `child` sous le nom `name` : le
// fabricant ne sait déclarer que des sous-fonctions vides, on greffe donc le
// corps de `child` à la place de la queue de la principale.
std::vector<std::uint8_t> ChunkDefining(const char* name, const LuaChunkBuilder& child) {
  LuaChunkBuilder main;
  main.Closure(0, 0, 0);
  main.SetGlobal(0, main.AddString(name));
  main.Return();
  std::vector<std::uint8_t> bytes = main.Build();
  bytes.resize(bytes.size() - kEmptyTailBytes);
  PushU32(bytes, 1);
  const std::vector<std::uint8_t> body = child.Build();
  bytes.insert(bytes.end(), body.begin() + kHeaderBytes, body.end());
  for (int table = 0; table < 3; ++table) {
    PushU32(bytes, 0);
  }
  return bytes;
}

// `InsertRecipe(9001, 9069, 610, 10)` puis `InsertFeeding(9069)`.
LuaChunkBuilder RecipeFunction() {
  LuaChunkBuilder body;
  const std::uint32_t recipe = body.AddString("InsertRecipe");
  const std::uint32_t feeding = body.AddString("InsertFeeding");
  const std::uint32_t source = body.AddNumber(9001);
  const std::uint32_t target = body.AddNumber(9069);
  const std::uint32_t item = body.AddNumber(610);
  const std::uint32_t amount = body.AddNumber(10);
  body.GetGlobal(0, recipe);
  body.LoadConstant(1, source);
  body.LoadConstant(2, target);
  body.LoadConstant(3, item);
  body.LoadConstant(4, amount);
  body.RawInstruction(Call(0, 5, 1));
  body.GetGlobal(0, feeding);
  body.LoadConstant(1, target);
  body.RawInstruction(Call(0, 2, 1));
  body.Return();
  return body;
}

}  // namespace

TEST_CASE("LuaCalls : les appels de la fonction nommée, dans l'ordre", "[luacalls]") {
  const auto calls = ReadLuaFunctionCalls(ChunkDefining("main", RecipeFunction()), "main");
  REQUIRE(calls.has_value());
  REQUIRE(calls->size() == 2);
  CHECK((*calls)[0].name == "InsertRecipe");
  REQUIRE((*calls)[0].arguments.size() == 4);
  CHECK((*calls)[0].arguments[0].kind == LuaValue::Kind::kNumber);
  CHECK((*calls)[0].arguments[0].number == 9001);
  CHECK((*calls)[0].arguments[3].number == 10);
  CHECK((*calls)[1].name == "InsertFeeding");
  REQUIRE((*calls)[1].arguments.size() == 1);
  CHECK((*calls)[1].arguments[0].number == 9069);
}

TEST_CASE("LuaCalls : une fonction absente se dit introuvable", "[luacalls]") {
  const auto calls = ReadLuaFunctionCalls(ChunkDefining("main", RecipeFunction()), "autre");
  REQUIRE_FALSE(calls.has_value());
  CHECK(calls.error().code == ro::core::ErrorCode::kNotFound);
}

TEST_CASE("LuaCalls : un résultat gardé refuse le fichier", "[luacalls]") {
  LuaChunkBuilder body;
  body.GetGlobal(0, body.AddString("Compute"));
  body.RawInstruction(Call(0, 1, 2));
  body.Return();
  CHECK_FALSE(ReadLuaFunctionCalls(ChunkDefining("main", body), "main").has_value());
}

TEST_CASE("LuaCalls : un argument tiré d'une globale refuse le fichier", "[luacalls]") {
  LuaChunkBuilder body;
  body.GetGlobal(0, body.AddString("Insert"));
  body.GetGlobal(1, body.AddString("SOME_ID"));
  body.RawInstruction(Call(0, 2, 1));
  body.Return();
  CHECK_FALSE(ReadLuaFunctionCalls(ChunkDefining("main", body), "main").has_value());
}

TEST_CASE("LuaCalls : un saut refuse le fichier", "[luacalls]") {
  LuaChunkBuilder body;
  body.LoadBoolean(0, true, true);
  body.Return();
  CHECK_FALSE(ReadLuaFunctionCalls(ChunkDefining("main", body), "main").has_value());
}

TEST_CASE("LuaCalls : un chunk principal qui calcule se refuse", "[luacalls]") {
  LuaChunkBuilder main;
  main.NewTable(0);
  main.Return();
  CHECK_FALSE(ReadLuaFunctionCalls(main.Build(), "main").has_value());
}

TEST_CASE("LuaCalls : la forme en clair donne les mêmes appels", "[luacalls]") {
  const auto calls = ReadLuaFunctionCalls(Bytes("-- en-tête de traduction\r\n"
                                                "function main()\r\n"
                                                "\tInsertRecipe(9001, 9069, 610, 10)\r\n"
                                                "\tInsertFeeding(9069);\r\n"
                                                "\treturn true, \"success\"\r\n"
                                                "end\r\n"),
                                          "main");
  REQUIRE(calls.has_value());
  REQUIRE(calls->size() == 2);
  CHECK((*calls)[0].name == "InsertRecipe");
  REQUIRE((*calls)[0].arguments.size() == 4);
  CHECK((*calls)[0].arguments[1].number == 9069);
  CHECK((*calls)[1].name == "InsertFeeding");
  REQUIRE((*calls)[1].arguments.size() == 1);
  CHECK((*calls)[1].arguments[0].number == 9069);
}

TEST_CASE("LuaCalls : en clair, seuls les appels de la fonction nommée comptent", "[luacalls]") {
  const auto calls = ReadLuaFunctionCalls(
      Bytes("function other() Insert(1) end function main() Insert(2) end"), "main");
  REQUIRE(calls.has_value());
  REQUIRE(calls->size() == 1);
  CHECK((*calls)[0].arguments[0].number == 2);
  const auto absent = ReadLuaFunctionCalls(Bytes("function other() end"), "main");
  REQUIRE_FALSE(absent.has_value());
  CHECK(absent.error().code == ro::core::ErrorCode::kNotFound);
}

TEST_CASE("LuaCalls : en clair, un calcul, une variable ou une boucle refuse le fichier",
          "[luacalls]") {
  CHECK_FALSE(ReadLuaFunctionCalls(Bytes("function main() Insert(1 + 2) end"), "main").has_value());
  CHECK_FALSE(
      ReadLuaFunctionCalls(Bytes("function main() Insert(SOME_ID) end"), "main").has_value());
  CHECK_FALSE(ReadLuaFunctionCalls(Bytes("function main() local x = 1 end"), "main").has_value());
  CHECK_FALSE(
      ReadLuaFunctionCalls(Bytes("function main() for i = 1, 2 do end end"), "main").has_value());
  CHECK_FALSE(ReadLuaFunctionCalls(Bytes("x = 1 function main() end"), "main").has_value());
  CHECK_FALSE(
      ReadLuaFunctionCalls(Bytes("function main() return 1 Insert(1) end"), "main").has_value());
}
