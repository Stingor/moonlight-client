// LES BONS DE TRAVAIL DU STAFF sur la fiche d'un objet.
//
// 🔴 LE FICHIER EST LU PAR UN SCRIPT, et c'est lui le vrai lecteur : sa clé
// racine, ses champs et le mot qui retire la teinte sont un contrat.

#include "items/item_info_delta.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "items/item_sheet_editor_stage.h"
#include "support/temp_directory.h"

using moonclient::ItemInfoDelta;
using moonclient::ItemInfoDeltaEntry;
using moonclient::ItemSheetEdit;
using moonclient::ItemSheetTintRank;
using moonclient::ItemSheetTintWord;
using ro::test_support::TempDirectory;
using ro::vfs::ItemInfoRecord;

namespace {

constexpr std::uint32_t kPoringCard = 4001;
constexpr std::uint32_t kRedPotion = 501;

[[nodiscard]] std::string FileText(const TempDirectory& root) {
  std::ifstream in(root.path() / "savedata" / "item_info_delta.yml");
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

}  // namespace

TEST_CASE("Bons de fiche : absents, rien n'est demandé", "[app][fiche_editee]") {
  TempDirectory root;
  ItemInfoDelta delta(root.path());
  CHECK(delta.entries().empty());
  CHECK(delta.Find(kPoringCard) == nullptr);
  CHECK(delta.Clear(kPoringCard));
}

TEST_CASE("Bons de fiche : seuls les champs demandés s'écrivent, et se relisent",
          "[app][fiche_editee]") {
  TempDirectory root;
  const ItemInfoDeltaEntry card{
      .item_id = kPoringCard, .resource = std::string("이름없는카드"), .tint = std::string("gold")};
  const ItemInfoDeltaEntry potion{.item_id = kRedPotion,
                                  .name = std::string("yes"),
                                  .tint = std::string(),
                                  .description = std::vector<std::string>{"Soigne.#", ""}};
  {
    ItemInfoDelta delta(root.path());
    REQUIRE(delta.Set(card));
    REQUIRE(delta.Set(potion));
    CHECK(delta.revision() == 2);
  }
  const std::string text = FileText(root);
  CHECK(text.find("item_info_delta:") != std::string::npos);
  // Le mot qui retire la teinte, et un nom entre guillemets.
  CHECK(text.find("tint: \"none\"") != std::string::npos);
  CHECK(text.find("name: \"yes\"") != std::string::npos);
  // Un champ que le bon ne demande pas ne s'écrit pas.
  CHECK(text.find("description") == text.rfind("description"));

  ItemInfoDelta again(root.path());
  REQUIRE(again.entries().size() == 2);
  REQUIRE(again.Find(kPoringCard) != nullptr);
  CHECK(*again.Find(kPoringCard) == card);
  REQUIRE(again.Find(kRedPotion) != nullptr);
  CHECK(*again.Find(kRedPotion) == potion);
}

TEST_CASE("Bons de fiche : un bon remplace le précédent, un bon vide le retire",
          "[app][fiche_editee]") {
  TempDirectory root;
  ItemInfoDelta delta(root.path());
  REQUIRE(delta.Set({.item_id = kPoringCard, .tint = std::string("red")}));
  REQUIRE(delta.Set({.item_id = kPoringCard, .tint = std::string("blue")}));
  REQUIRE(delta.entries().size() == 1);
  CHECK(delta.Find(kPoringCard)->tint == "blue");
  REQUIRE(delta.Set({.item_id = kPoringCard}));
  CHECK(delta.entries().empty());
  CHECK(ItemInfoDelta(root.path()).entries().empty());
}

TEST_CASE("Bons de fiche : la saisie ne retient que ce qui diffère de la table",
          "[app][fiche_editee]") {
  const ItemInfoRecord table{.item_id = kPoringCard,
                             .name = "Poring Card",
                             .resource = "이름없는카드",
                             .description = {"LUK +2"}};
  ItemInfoRecord typed = table;
  CHECK(ItemSheetEdit(kPoringCard, table, typed).empty());
  typed.tint = "gold";
  const ItemInfoDeltaEntry tinted = ItemSheetEdit(kPoringCard, table, typed);
  CHECK(tinted == ItemInfoDeltaEntry{.item_id = kPoringCard, .tint = std::string("gold")});
  typed.description.emplace_back("Perfect Dodge +1");
  CHECK(ItemSheetEdit(kPoringCard, table, typed).description.has_value());
}

TEST_CASE("Bons de fiche : le rang de la liste des teintes", "[app][fiche_editee]") {
  CHECK(ItemSheetTintRank("") == 0);
  CHECK(ItemSheetTintRank("mauve") == 0);
  CHECK(ItemSheetTintWord(0).empty());
  for (std::size_t rank = 0; rank < ro::formats::kIconTintShades.size(); ++rank) {
    const std::string_view word = ro::formats::kIconTintShades[rank].word;
    CHECK(ItemSheetTintWord(ItemSheetTintRank(word)) == word);
  }
  CHECK(ItemSheetTintWord(999).empty());
}
