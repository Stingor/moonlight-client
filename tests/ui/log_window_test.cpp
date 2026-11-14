// La fenêtre de journal, sans fenêtre ni carte graphique.

#include "ro/ui/log_window.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

using ro::core::LogLevel;

[[nodiscard]] std::vector<ro::core::LoggedLine> FourLevels() {
  return {{.level = LogLevel::kDebug, .since_start_ms = 10, .text = "un détail"},
          {.level = LogLevel::kInfo, .since_start_ms = 20, .text = "une information"},
          {.level = LogLevel::kWarning, .since_start_ms = 30, .text = "une alerte"},
          {.level = LogLevel::kError, .since_start_ms = 40, .text = "une erreur"}};
}

}  // namespace

TEST_CASE("le détail est éteint au départ, et les trois autres allumés", "[ui]") {
  // Allumé par défaut, le détail noie les trois autres et rend la fenêtre
  // inutile au moment même où elle sert.
  const ro::ui::LogWindowState state;
  const std::vector<ro::core::LoggedLine> kept = ro::ui::SelectLogLines(FourLevels(), state);
  REQUIRE(kept.size() == 3);
  for (const ro::core::LoggedLine& line : kept) {
    CHECK(line.level != LogLevel::kDebug);
  }
}

TEST_CASE("chaque gravité se montre et se cache séparément", "[ui]") {
  ro::ui::LogWindowState state;
  state.shown = {false, false, false, true};

  const std::vector<ro::core::LoggedLine> kept = ro::ui::SelectLogLines(FourLevels(), state);
  REQUIRE(kept.size() == 1);
  CHECK(kept[0].level == LogLevel::kError);
  CHECK(kept[0].text == "une erreur");
}

TEST_CASE("l'ordre d'écriture est conservé, jamais trié par gravité", "[ui]") {
  // Trier séparerait une erreur de la ligne qui l'explique, laquelle la
  // précède presque toujours.
  ro::ui::LogWindowState state;
  state.shown = {true, true, true, true};

  const std::vector<ro::core::LoggedLine> kept = ro::ui::SelectLogLines(FourLevels(), state);
  REQUIRE(kept.size() == 4);
  for (std::size_t rank = 1; rank < kept.size(); ++rank) {
    CHECK(kept[rank - 1].since_start_ms < kept[rank].since_start_ms);
  }
  CHECK(kept.front().level == LogLevel::kDebug);
  CHECK(kept.back().level == LogLevel::kError);
}

TEST_CASE("la fenêtre de journal montre ce que le filtre a retenu", "[ui]") {
  ro::ui::UiContext ui;
  ro::ui::LogWindowState state;
  const std::vector<ro::core::LoggedLine> lines = FourLevels();

  ui.BeginFrame(kViewport, kFrameSeconds);
  const std::size_t closed = ro::ui::DrawLogWindow(state, lines);
  ui.EndFrame();
  CHECK(closed == 0);

  state.open = true;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const std::size_t shown = ro::ui::DrawLogWindow(state, lines);
  ui.EndFrame();
  CHECK(shown == 3);
}

TEST_CASE("le journal en mémoire garde ce qu'on lui écrit, et dans l'ordre", "[ui]") {
  // ⚠ C'est ce qui nourrit la fenêtre : sans lui, elle n'aurait rien à montrer
  // que ce que l'appelant aurait recopié à la main.
  const std::size_t before = ro::core::LoggedLineCount();
  ro::core::LogInfo("essai de journal {}", 1);
  ro::core::LogWarning("essai de journal {}", 2);
  CHECK(ro::core::LoggedLineCount() == before + 2);

  const std::vector<ro::core::LoggedLine> last = ro::core::RecentLogLines(2);
  REQUIRE(last.size() == 2);
  CHECK(last[0].level == LogLevel::kInfo);
  CHECK(last[0].text == "essai de journal 1");
  CHECK(last[1].level == LogLevel::kWarning);
  CHECK(last[1].text == "essai de journal 2");

  // La lecture SANS ATTENTE du rapport de plantage rend la même chose quand
  // personne n'écrit.
  const auto tried = ro::core::TryRecentLogLines(2);
  REQUIRE(tried.has_value());
  REQUIRE(tried->size() == 2);
  CHECK((*tried)[1].text == "essai de journal 2");
}

TEST_CASE("un filtre vide ne retient rien", "[ui]") {
  const ro::ui::LogWindowState state;
  CHECK(ro::ui::LinePassesFilter("n'importe quoi", ""));
  // Et le réglage par défaut montre donc les trois gravités allumées, sans rien
  // retrancher de plus.
  CHECK(ro::ui::SelectLogLines(FourLevels(), state).size() == 3);
}

TEST_CASE("le filtre ignore la casse sur l'ASCII, et RESPECTE les accents", "[ui]") {
  // ⚠ C'est la règle de `core::AsciiLower`, et elle est délibérée : abaisser un
  // octet au-delà de 0x7F réécrirait de l'UTF-8 à deux octets et casserait le
  // mot qu'on cherche.
  CHECK(ro::ui::LinePassesFilter("une ERREUR de lecture", "erreur"));
  CHECK(ro::ui::LinePassesFilter("une erreur de lecture", "ERREUR"));
  CHECK(ro::ui::LinePassesFilter("un éteint", "éteint"));
  CHECK_FALSE(ro::ui::LinePassesFilter("un éteint", "ÉTEINT"));
}

TEST_CASE("un filtre plus long que la ligne ne trouve rien", "[ui]") {
  CHECK_FALSE(ro::ui::LinePassesFilter("court", "beaucoup plus long"));
  CHECK(ro::ui::LinePassesFilter("court", "court"));
}

TEST_CASE("le filtre se combine avec les gravités, il ne les remplace pas", "[ui]") {
  ro::ui::LogWindowState state;
  state.shown = {true, true, true, true};
  const std::string_view word = "une";
  std::copy(word.begin(), word.end(), state.filter.begin());

  // Les quatre lignes portent « une » sauf la première, qui dit « un détail ».
  const std::vector<ro::core::LoggedLine> kept = ro::ui::SelectLogLines(FourLevels(), state);
  REQUIRE(kept.size() == 3);
  CHECK(kept[0].level == LogLevel::kInfo);

  // Et une gravité éteinte retranche encore, par-dessus le filtre.
  state.shown = {false, false, true, true};
  CHECK(ro::ui::SelectLogLines(FourLevels(), state).size() == 2);
}

TEST_CASE("un tampon de filtre ENTIÈREMENT rempli ne fait pas lire au-delà", "[ui]") {
  // ⚠ Le cas que `TypedFilter` existe pour tenir : sans terminaison, une
  // lecture naïve déborderait du tableau.
  ro::ui::LogWindowState state;
  state.filter.fill('z');
  const std::vector<ro::core::LoggedLine> kept = ro::ui::SelectLogLines(FourLevels(), state);
  CHECK(kept.empty());
}

TEST_CASE("le texte copié est CE QUI EST MONTRÉ, horodatage compris", "[ui]") {
  // Copier le seul message donnerait un texte plus propre et moins UTILE :
  // c'est l'écart entre deux instants qui dit ce qui s'est passé.
  const std::vector<ro::core::LoggedLine> two = {
      {.serial = 1, .level = LogLevel::kInfo, .since_start_ms = 1234, .text = "première"},
      {.serial = 2, .level = LogLevel::kError, .since_start_ms = 12080, .text = "seconde"}};
  CHECK(ro::ui::JoinLogLines(two) == "1.234 première\n12.080 seconde\n");
  CHECK(ro::ui::JoinLogLines({}).empty());
}

TEST_CASE("copier ne rend que ce qui est À LA FOIS choisi et montré", "[ui]") {
  // ⚠ Une ligne choisie puis cachée par un filtre ne doit pas revenir dans le
  // presse-papiers : ce qu'on colle est ce qu'on voit.
  ro::ui::LogWindowState state;
  const std::vector<ro::core::LoggedLine> all_levels = FourLevels();
  // Les numéros que `FourLevels` ne pose pas : on les pose ici.
  std::vector<ro::core::LoggedLine> numerotees = all_levels;
  for (std::size_t line_rank = 0; line_rank < numerotees.size(); ++line_rank) {
    numerotees[line_rank].serial = line_rank + 1;
  }

  // Le détail est éteint par défaut : la première ligne ne se montre pas.
  state.selected = {1, 2};
  const std::vector<ro::core::LoggedLine> montrees = ro::ui::SelectLogLines(numerotees, state);
  const std::vector<ro::core::LoggedLine> copiees = ro::ui::SelectedLogLines(montrees, state);
  REQUIRE(copiees.size() == 1);
  CHECK(copiees[0].serial == 2);
  CHECK(copiees[0].level == LogLevel::kInfo);
}

TEST_CASE("un numéro de ligne ne se réemploie jamais", "[ui]") {
  // ⚠ C'est ce qui fait tenir une sélection pendant que le journal grandit :
  // désigner une ligne par son RANG la ferait glisser à chaque écriture.
  // ⚠ DEUX LIGNES ÉCRITES ICI MÊME, et non une seule en comptant sur celles
  // d'un autre essai : un essai qui dépend de ce qu'un voisin a laissé échoue le
  // jour où l'ordre change, et accuse alors le code plutôt que lui-même.
  ro::core::LogInfo("une ligne pour le numéro");
  ro::core::LogInfo("et celle qui suit");
  const std::vector<ro::core::LoggedLine> two = ro::core::RecentLogLines(2);
  REQUIRE(two.size() == 2);
  CHECK(two[0].serial != 0);
  CHECK(two[1].serial == two[0].serial + 1);
}
