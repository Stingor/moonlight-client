// LA FICHE D'UN OBJET ÉDITÉE : la description saisie, une ligne de la table
// par ligne de texte.

#include "ro/ui/item_sheet_editor.h"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::ui::ItemSheetEditorLines;
using ro::ui::ItemSheetEditorText;

TEST_CASE("Fiche éditée : une ligne de table par ligne de texte", "[ui][fiche_editee]") {
  CHECK(ItemSheetEditorLines("").empty());
  CHECK(ItemSheetEditorLines("a") == std::vector<std::string>{"a"});
  CHECK(ItemSheetEditorLines("a\nb#\n\nc") == std::vector<std::string>{"a", "b#", "", "c"});
  // La fin de ligne Windows perd son retour chariot ; le saut final ne fait
  // pas de ligne.
  CHECK(ItemSheetEditorLines("a\r\nb\r\n") == std::vector<std::string>{"a", "b"});
}

TEST_CASE("Fiche éditée : les lignes de la table reviennent telles quelles", "[ui][fiche_editee]") {
  const std::vector<std::string> lines = {"MaxHP +700", "", "^0000CCPoids :^000000 1#"};
  CHECK(ItemSheetEditorText(lines) == "MaxHP +700\n\n^0000CCPoids :^000000 1#");
  CHECK(ItemSheetEditorLines(ItemSheetEditorText(lines)) == lines);
}
