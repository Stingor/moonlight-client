// LE FILTRE DU PANNEAU DE RÉGLAGES ET L'ORDRE DE SA NAVIGATION : ce qu'un mot
// tapé garde d'une page, et comment les familles se trient.

#include "ro/ui/options_search.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::ui::FindInSection;
using ro::ui::KeptRows;
using ro::ui::NavEntry;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;
using ro::ui::OptionSectionView;
using ro::ui::OptionsSearch;
using ro::ui::SortedNavEntries;

namespace {

[[nodiscard]] OptionRowView Row(int id, const char* label) {
  return OptionRowView{.id = id, .label = label};
}

[[nodiscard]] std::vector<bool> Kept(const std::vector<OptionRowView>& rows, const char* typed) {
  return KeptRows(rows, OptionsSearch(typed), false);
}

}  // namespace

TEST_CASE("Filtre : casse, accents et identité ImGui", "[ui][options_search]") {
  const OptionsSearch search("ECHELLE");
  CHECK(search.active());
  CHECK(search.Finds("Échelle de l'interface"));
  CHECK_FALSE(search.Finds(nullptr));
  CHECK_FALSE(OptionsSearch("cadre").Finds("Taille##cadre"));
  CHECK_FALSE(OptionsSearch("   ").active());
  CHECK_FALSE(OptionsSearch("").active());
}

TEST_CASE("Filtre : sans recherche, tout se garde", "[ui][options_search]") {
  const std::vector<OptionRowView> rows = {Row(1, "Langue"), Row(2, "Police")};
  CHECK(Kept(rows, "") == std::vector<bool>{true, true});
  CHECK(KeptRows(rows, OptionsSearch("zzz"), true) == std::vector<bool>{true, true});
}

TEST_CASE("Filtre : une rangée trouvée par son libellé ou sa commande", "[ui][options_search]") {
  std::vector<OptionRowView> rows = {Row(1, "Langue"), Row(2, "Police"), Row(3, "Échelle")};
  rows[2].command = "/echelle";
  CHECK(Kept(rows, "police") == std::vector<bool>{false, true, false});
  CHECK(Kept(rows, "/ech") == std::vector<bool>{false, false, true});
}

TEST_CASE("Filtre : l'aide et les notes ne se cherchent pas", "[ui][options_search]") {
  std::vector<OptionRowView> rows = {Row(1, "Langue"), Row(2, "Le son est coupé")};
  rows[0].help = "Le son des dialogues";
  rows[1].kind = OptionKind::kNote;
  CHECK(Kept(rows, "son") == std::vector<bool>{false, false});
}

TEST_CASE("Filtre : un intertitre trouvé garde son bloc", "[ui][options_search]") {
  std::vector<OptionRowView> rows = {Row(1, "Langue"), Row(2, "Confirmer"), Row(3, "Animer"),
                                     Row(4, "Renoncer")};
  rows[1].heading = "Fiche de pet";
  rows[3].heading = "Rapport de bug";
  CHECK(Kept(rows, "pet") == std::vector<bool>{false, true, true, false});
}

TEST_CASE("Filtre : un titre de bloc garde tout jusqu'au suivant", "[ui][options_search]") {
  std::vector<OptionRowView> rows = {Row(1, "Afficher"), Row(2, "Couleur"), Row(3, "Afficher")};
  rows[0].fold = "Barres";
  rows[1].heading = "Teinte";
  rows[2].fold = "Nom";
  CHECK(Kept(rows, "barres") == std::vector<bool>{true, true, false});
}

TEST_CASE("Filtre : une rangée décalée emmène sa maîtresse", "[ui][options_search]") {
  std::vector<OptionRowView> rows = {Row(1, "Temps restant"), Row(2, "Taille du texte"),
                                     Row(3, "Fond")};
  rows[1].indented = true;
  CHECK(Kept(rows, "taille") == std::vector<bool>{true, true, false});
}

TEST_CASE("Filtre : les sœurs d'une ligne se montrent ensemble", "[ui][options_search]") {
  std::vector<OptionRowView> rows = {Row(1, "Joueurs"), Row(2, "Monstres"), Row(3, "PNJ"),
                                     Row(4, "Opacité")};
  rows[1].same_line = true;
  rows[2].same_line = true;
  CHECK(Kept(rows, "pnj") == std::vector<bool>{true, true, true, false});
}

TEST_CASE("Filtre : une section trouvée par son titre ou un onglet", "[ui][options_search]") {
  const std::vector<OptionRowView> rows = {Row(1, "Durée"), Row(2, "Opacité")};
  constexpr std::array<const char*, 2> kTabs = {"Vie", "Magie"};
  const OptionSectionView bubbles{.title = "Bulles", .rows = rows};
  const OptionSectionView bars{.title = "Jauges", .rows = rows, .tabs = kTabs};

  const auto whole = FindInSection(bubbles, OptionsSearch("bulle"));
  CHECK(whole.shown);
  CHECK(whole.whole);
  CHECK(whole.settings == 2);

  // Un nom d'onglet trouvé montre la section ; sans copie des onglets, la page
  // composée se montre entière.
  const auto by_tab = FindInSection(bars, OptionsSearch("magie"));
  CHECK(by_tab.shown);
  CHECK_FALSE(by_tab.whole);
  CHECK(by_tab.tab_whole == std::vector<bool>{false, true});
  CHECK(by_tab.first_tab == 1);
  CHECK(by_tab.settings == 2);

  const auto by_row = FindInSection(bubbles, OptionsSearch("duree"));
  CHECK(by_row.shown);
  CHECK_FALSE(by_row.whole);
  CHECK(by_row.settings == 1);

  CHECK_FALSE(FindInSection(bubbles, OptionsSearch("police")).shown);
}

TEST_CASE("Navigation : chaque famille se trie, accents ignorés", "[ui][options_search]") {
  const std::array<OptionSectionView, 6> sections = {{
      {.title = "Sound", .group = "Core"},
      {.title = "Display"},
      {.title = "Interface", .group = "UI"},
      {.title = "États"},
      {.title = "Bulles"},
      {.title = "Commands", .group = "Server"},
  }};
  const std::vector<NavEntry> entries =
      SortedNavEntries(sections, std::vector<bool>(sections.size(), true));
  REQUIRE(entries.size() == sections.size());
  const std::array<int, 6> order = {1, 0, 4, 3, 2, 5};
  for (std::size_t rank = 0; rank < order.size(); ++rank) {
    INFO(rank);
    CHECK(entries[rank].target == order[rank]);
  }
  // Le titre de famille passe au premier membre APRÈS le tri.
  CHECK(entries[0].group != nullptr);
  CHECK(std::string_view(entries[0].group) == "Core");
  CHECK(entries[1].group == nullptr);
  CHECK(std::string_view(entries[2].group) == "UI");
  CHECK(std::string_view(entries[5].group) == "Server");
}

TEST_CASE("Navigation : les sections de tête passent avant le tri", "[ui][options_search]") {
  const std::array<OptionSectionView, 5> sections = {{
      {.title = "Interface", .group = "UI", .leads = true},
      {.title = "Skin", .leads = true},
      {.title = "Bulles"},
      {.title = "Chat"},
      {.title = "Albums"},
  }};
  const std::vector<NavEntry> entries =
      SortedNavEntries(sections, std::vector<bool>(sections.size(), true));
  REQUIRE(entries.size() == sections.size());
  const std::array<int, 5> order = {0, 1, 4, 2, 3};
  for (std::size_t rank = 0; rank < order.size(); ++rank) {
    INFO(rank);
    CHECK(entries[rank].target == order[rank]);
  }
  REQUIRE(entries[0].group != nullptr);
  CHECK(std::string_view(entries[0].group) == "UI");
}

TEST_CASE("Navigation : une section écartée n'a pas d'entrée", "[ui][options_search]") {
  const std::array<OptionSectionView, 3> sections = {{
      {.title = "Display", .group = "Core"},
      {.title = "Interface", .group = "UI"},
      {.title = "Bulles"},
  }};
  const std::vector<NavEntry> entries = SortedNavEntries(sections, {false, false, true});
  REQUIRE(entries.size() == 1);
  CHECK(entries[0].target == 2);
  // ⚠ LA FAMILLE GARDE SON TITRE, porté par le seul membre qui reste.
  REQUIRE(entries[0].group != nullptr);
  CHECK(std::string_view(entries[0].group) == "UI");
}

TEST_CASE("Filtre : un réglage d'un onglet fermé se trouve dans sa copie", "[ui][options_search]") {
  // L'appelant ne compose que l'onglet ouvert ; les autres arrivent recopiés.
  const std::vector<OptionRowView> first = {Row(1, "Taille des cases")};
  const std::vector<OptionRowView> second = {Row(1, "Taille des cases"),
                                             Row(2, "Couleur du cadre")};
  ro::ui::SearchedTabs copied;
  copied.Add(first);
  copied.Add(second);
  constexpr std::array<const char*, 2> kTabs = {"Barre 1", "Barre 2"};
  const OptionSectionView bars{
      .title = "Barres", .rows = first, .tabs = kTabs, .searched_tabs = copied.tabs()};

  const auto finding = FindInSection(bars, OptionsSearch("cadre"));
  CHECK(finding.shown);
  CHECK(finding.tab_settings == std::vector<int>{0, 1});
  CHECK(finding.first_tab == 1);
  CHECK(finding.settings == 1);

  // Le nom d'un onglet garde tout cet onglet, et lui seul.
  const auto by_name = FindInSection(bars, OptionsSearch("barre 2"));
  CHECK(by_name.TabWhole(1));
  CHECK_FALSE(by_name.TabWhole(0));
  CHECK(by_name.tab_settings == std::vector<int>{0, 2});
}

TEST_CASE("Filtre : la copie d'un onglet possède ses textes", "[ui][options_search]") {
  // Un libellé composé vit souvent dans un tampon que l'onglet suivant réécrit.
  std::string label = "Barre 3";
  ro::ui::SearchedTabs copied;
  const std::vector<OptionRowView> rows = {Row(7, label.c_str())};
  copied.Add(rows);
  label = "Barre 4";
  REQUIRE(copied.tabs().size() == 1);
  REQUIRE(copied.tabs()[0].size() == 1);
  CHECK(std::string_view(copied.tabs()[0][0].label) == "Barre 3");
  CHECK(copied.tabs()[0][0].id == 7);
  copied.Clear();
  CHECK(copied.tabs().empty());
}

TEST_CASE("Filtre : une page dessinée se trouve par ses libellés", "[ui][options_search]") {
  constexpr std::array<ro::ui::OptionBodyTerm, 3> kTerms = {{
      {.text = "Show EXP gain", .tab = 0},
      {.text = "@showexp", .tab = 0},
      {.text = "Autoloot MVP", .tab = 1},
  }};
  const OptionSectionView commands{.title = "Commands Settings", .body_terms = kTerms};

  const auto by_term = FindInSection(commands, OptionsSearch("mvp"));
  CHECK(by_term.shown);
  CHECK(by_term.body_found);
  CHECK(by_term.body_tab == 1);
  CHECK(by_term.settings == 1);

  const auto by_command = FindInSection(commands, OptionsSearch("@SHOWEXP"));
  CHECK(by_command.body_tab == 0);

  CHECK_FALSE(FindInSection(commands, OptionsSearch("police")).shown);
}
