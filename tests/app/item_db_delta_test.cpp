// LES BONS DE TRAVAIL « ITEM_DB » DU STAFF.
//
// 🔴 LE FICHIER EST LU PAR UN SCRIPT : ses clés sont celles de l'item_db, un
// nombre et `Refineable` s'écrivent nus, un script entre guillemets.

#include "items/item_db_delta.h"

#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "items/item_sheet_editor_stage.h"
#include "support/temp_directory.h"

using moonclient::ItemDbDelta;
using moonclient::ItemDbDeltaEntry;
using moonclient::ItemSheetServerBase;
using moonclient::ItemSheetServerEdit;
using moonclient::ItemSheetServerNumberOf;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint32_t kPoringCard = 4001;

[[nodiscard]] std::string FileText(const TempDirectory& root) {
  std::ifstream in(root.path() / "savedata" / "item_db_delta.yml");
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

[[nodiscard]] ro::vfs::ServerItem PoringCard() {
  ro::vfs::ServerItem item;
  item.item_id = kPoringCard;
  item.buy_zeny = 20;
  item.weight_tenths = 10;
  item.script = "bonus bLuk,2; bonus bFlee2,1;\n";
  return item;
}

}  // namespace

TEST_CASE("Bons d'item_db : un nombre nu, un script entre guillemets, et se relisent",
          "[app][fiche_editee]") {
  TempDirectory root;
  const ItemDbDeltaEntry card{
      .item_id = kPoringCard,
      .fields = {{"Weight", "20"}, {"Refineable", "true"}, {"Script", "bonus bLuk,3;\n\"cité\""}}};
  {
    ItemDbDelta delta(root.path());
    REQUIRE(delta.Set(card));
  }
  const std::string text = FileText(root);
  CHECK(text.find("item_db_delta:") != std::string::npos);
  CHECK(text.find("Weight: 20") != std::string::npos);
  CHECK(text.find("Refineable: true") != std::string::npos);
  CHECK(text.find("Script: \"bonus bLuk,3;\\n\\\"cité\\\"\"") != std::string::npos);

  ItemDbDelta again(root.path());
  REQUIRE(again.Find(kPoringCard) != nullptr);
  CHECK(*again.Find(kPoringCard) == card);
  REQUIRE(again.Set({.item_id = kPoringCard}));
  CHECK(again.entries().empty());
}

TEST_CASE("Bons d'item_db : une clé que l'onglet ne connaît pas ne se relit pas",
          "[app][fiche_editee]") {
  TempDirectory root;
  root.WriteText("savedata/item_db_delta.yml",
                 "item_db_delta:\n  - {item: 4001, Weight: 5, Jobs: x, AegisName: y}\n");
  ItemDbDelta delta(root.path());
  REQUIRE(delta.Find(kPoringCard) != nullptr);
  CHECK(delta.Find(kPoringCard)->fields ==
        std::vector<std::pair<std::string, std::string>>{{"Weight", "5"}});
}

TEST_CASE("Bons d'item_db : la saisie ne retient que ce qui diffère de la base",
          "[app][fiche_editee]") {
  const ro::vfs::ServerItem item = PoringCard();
  CHECK(ItemSheetServerNumberOf(item, "Buy") == 20);
  CHECK(ItemSheetServerNumberOf(item, "Weight") == 10);
  CHECK(ItemSheetServerNumberOf(item, "Inconnu") == 0);

  const ro::ui::ItemSheetServerFields base = ItemSheetServerBase(item);
  ro::ui::ItemSheetEditorState typed;
  typed.numbers = base.numbers;
  typed.refineable = base.refineable;
  for (std::size_t rank = 0; rank < typed.scripts.size(); ++rank) {
    std::memcpy(typed.scripts[rank].data(), base.scripts[rank].data(), base.scripts[rank].size());
  }
  CHECK(ItemSheetServerEdit(kPoringCard, base, typed).fields.empty());

  typed.numbers[2] = 30;  // Weight
  typed.refineable = true;
  typed.scripts[0] = {};
  CHECK(ItemSheetServerEdit(kPoringCard, base, typed).fields ==
        std::vector<std::pair<std::string, std::string>>{
            {"Weight", "30"}, {"Refineable", "true"}, {"Script", ""}});
}
