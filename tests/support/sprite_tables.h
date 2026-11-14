#pragma once

// LES TABLES DE SPRITES D'UN ESSAI, lues dans leur texte YAML par le lecteur
// même du jeu : un essai qui écrit sa table de travers échoue ici, et non sur
// une assertion d'habillage qui ne dirait pas pourquoi.

#include <initializer_list>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/sprite_tables.h"

namespace ro::test_support {

struct SpriteTableText {
  render::SpriteTableFile file;
  std::string_view text;
};

inline render::SpriteTables SpriteTablesFrom(std::initializer_list<SpriteTableText> texts) {
  render::SpriteTables tables;
  for (const SpriteTableText& source : texts) {
    const auto parsed = render::ParseSpriteTableText(source.file, source.text, tables);
    INFO((parsed.has_value() ? std::string() : parsed.error().detail));
    REQUIRE(parsed.has_value());
  }
  return tables;
}

}  // namespace ro::test_support
