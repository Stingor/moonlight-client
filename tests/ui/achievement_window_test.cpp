// LA GÉOMÉTRIE DE LA FENÊTRE DES SUCCÈS, sans fenêtre : l'arbre, la liste, les
// récompenses, les jauges, l'ascenseur et la bannière.

#include "ro/ui/achievement_window.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>

#include <imgui.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/gauge_fill.h"

#include "support/lone_draw_list.h"

namespace {

using ro::test::LoneDrawList;
using ro::ui::AchievementGaugeFill;
using ro::ui::AchievementGaugeFillBox;
using ro::ui::AchievementGaugeInterior;
using ro::ui::AchievementListRowAt;
using ro::ui::AchievementRewardX;
using ro::ui::AchievementScrollThumb;
using ro::ui::AchievementTabTop;
using ro::ui::AxisBox;
using ro::ui::FillRoundedBoxWithin;
using ro::ui::HitAchievementTab;
using ro::ui::RoundedBox;

constexpr int kTabs = ro::ui::kAchievementTabCount;
// L'onglet Aventure : six sous-onglets, donc un ascenseur.
constexpr int kAdventure = 3;
constexpr int kAdventureSubtabs = 6;
// Le cadre d'une récompense.
constexpr float kFrame = 62.0F;

// La jauge globale du résumé : 558 sur 10 pixels d'art, deux cadres d'un pixel.
const ImVec2 kGaugeMin(43.0F, 49.0F);
const ImVec2 kGaugeMax(601.0F, 59.0F);
constexpr float kGaugeInset = 2.0F;
constexpr ImU32 kFillInk = IM_COL32(0x71, 0x8A, 0xC7, 0xFF);

// LES SOMMETS DU REMPLISSAGE d'une jauge à `current` sur `maximum`, peints
// comme la fenêtre les peint. Tous tiennent dans l'intérieur ; rend leur nombre.
[[nodiscard]] int PaintedFillVertices(std::int64_t current, std::int64_t maximum,
                                      float& rightmost) {
  LoneDrawList sheet;
  const RoundedBox interior = AchievementGaugeInterior(kGaugeMin, kGaugeMax);
  const std::optional<AxisBox> filled = AchievementGaugeFillBox(interior, current, maximum);
  REQUIRE(filled.has_value());
  FillRoundedBoxWithin(sheet.into, interior, *filled, kFillInk);
  int seen = 0;
  rightmost = interior.min.x;
  for (const ImDrawVert& vertex : sheet.into.VtxBuffer) {
    ++seen;
    CHECK(vertex.pos.x >= interior.min.x);
    CHECK(vertex.pos.x <= interior.max.x);
    CHECK(vertex.pos.y >= interior.min.y);
    CHECK(vertex.pos.y <= interior.max.y);
    rightmost = std::max(rightmost, vertex.pos.x);
  }
  return seen;
}

}  // namespace

TEST_CASE("Succès : les racines sous la racine choisie descendent de sa bande", "[ui][succes]") {
  // Rien d'ouvert sous le résumé : trente-cinq pixels par racine.
  CHECK(AchievementTabTop(3, 0, 0) == 105.0F);
  // Aventure ouverte : trois sous-lignes montrées sur six, 78 pixels de plus.
  CHECK(AchievementTabTop(kAdventure, kAdventure, kAdventureSubtabs) == 105.0F);
  CHECK(AchievementTabTop(4, kAdventure, kAdventureSubtabs) == 218.0F);
  // Combat ouvert : deux sous-lignes, 52 pixels.
  CHECK(AchievementTabTop(5, 4, 2) == 227.0F);
}

TEST_CASE("Succès : un clic dans l'arbre choisit une racine ou une sous-ligne", "[ui][succes]") {
  // Une racine au-dessus de la racine ouverte.
  const auto all = HitAchievementTab(45.0F, kAdventure, kAdventureSubtabs, 0, kTabs);
  REQUIRE(all.has_value());
  CHECK(all->tab == ro::ui::kAchievementAllTab);
  const auto general = HitAchievementTab(80.0F, kAdventure, kAdventureSubtabs, 0, kTabs);
  REQUIRE(general.has_value());
  CHECK(general->tab == ro::ui::kAchievementFirstCategoryTab);
  CHECK(general->subtab == ro::ui::kAchievementWholeTab);
  // La deuxième sous-ligne montrée, défilée d'un cran : le troisième
  // sous-onglet.
  const auto sub = HitAchievementTab(175.0F, kAdventure, kAdventureSubtabs, 1, kTabs);
  REQUIRE(sub.has_value());
  CHECK(sub->tab == kAdventure);
  CHECK(sub->subtab == 2);
  // Sous la bande : la racine suivante, la bande décomptée.
  const auto battle = HitAchievementTab(225.0F, kAdventure, kAdventureSubtabs, 0, kTabs);
  REQUIRE(battle.has_value());
  CHECK(battle->tab == 4);
}

TEST_CASE("Succès : sous la dernière racine, un clic ne choisit rien", "[ui][succes]") {
  // Huit racines de trente-cinq : la dernière s'arrête à 280.
  CHECK(HitAchievementTab(279.0F, 0, 0, 0, kTabs).has_value());
  CHECK_FALSE(HitAchievementTab(281.0F, 0, 0, 0, kTabs).has_value());
  CHECK_FALSE(HitAchievementTab(-1.0F, 0, 0, 0, kTabs).has_value());
}

TEST_CASE("Succès : les défilements se bornent à ce qui est montré", "[ui][succes]") {
  CHECK(ro::ui::AchievementSubtabScrollRange(kAdventureSubtabs) == 3);
  CHECK(ro::ui::AchievementSubtabScrollRange(2) == 0);
  CHECK(ro::ui::AchievementListScrollRange(12) == 7);
  CHECK(ro::ui::AchievementListScrollRange(3) == 0);
  CHECK(ro::ui::ClampAchievementScroll(9, 7) == 7);
  CHECK(ro::ui::ClampAchievementScroll(-2, 7) == 0);
  CHECK(ro::ui::ClampAchievementScroll(4, 0) == 0);
}

TEST_CASE("Succès : un sous-onglet ouvert par un lien se montre dans sa bande", "[ui][succes]") {
  CHECK(ro::ui::AchievementSubtabScrollShowing(1, kAdventureSubtabs) == 0);
  CHECK(ro::ui::AchievementSubtabScrollShowing(4, kAdventureSubtabs) == 2);
  CHECK(ro::ui::AchievementSubtabScrollShowing(5, kAdventureSubtabs) == 3);
  CHECK(ro::ui::AchievementSubtabScrollShowing(ro::ui::kAchievementWholeTab, kAdventureSubtabs) ==
        0);

  ro::ui::AchievementWindowState state;
  state.search[0] = 'x';
  state.list_scroll = 4;
  ro::ui::OpenAchievementTab(state, ro::ui::AchievementTabHit{.tab = kAdventure, .subtab = 5});
  CHECK(state.open);
  CHECK(state.tab == kAdventure);
  CHECK(state.subtab == 5);
  CHECK(state.subtab_scroll == 3);
  CHECK(state.list_scroll == 0);
  // ⚠ LE LIEN QUITTE LA RECHERCHE : la liste montrerait sinon ses résultats.
  CHECK(state.search[0] == '\0');
  // Un sous-onglet que l'onglet n'a pas rend l'onglet entier.
  ro::ui::OpenAchievementTab(
      state,
      ro::ui::AchievementTabHit{.tab = kTabs - 1, .subtab = ro::ui::kAchievementFeatMinigames + 1});
  CHECK(state.tab == kTabs - 1);
  CHECK(state.subtab == ro::ui::kAchievementWholeTab);
  CHECK(state.subtab_scroll == 0);
}

TEST_CASE("Succès : un clic dans la liste ne touche que les rangées montrées", "[ui][succes]") {
  // Trois rangées : la quatrième place est vide, et ne se choisit pas.
  CHECK(AchievementListRowAt(10.0F, 0, 3) == std::optional<std::size_t>(0));
  CHECK(AchievementListRowAt(170.0F, 0, 3) == std::optional<std::size_t>(2));
  CHECK_FALSE(AchievementListRowAt(250.0F, 0, 3).has_value());
  // Défilée de deux : la première place montre la troisième rangée.
  CHECK(AchievementListRowAt(10.0F, 2, 12) == std::optional<std::size_t>(2));
  // Sous la cinquième place, plus rien.
  CHECK_FALSE(AchievementListRowAt(406.0F, 0, 12).has_value());
}

TEST_CASE("Succès : les récompenses se centrent au pas de soixante-dix", "[ui][succes]") {
  CHECK(AchievementRewardX(0, 1, kFrame) == 133.0F);
  CHECK(AchievementRewardX(0, 2, kFrame) == 98.0F);
  CHECK(AchievementRewardX(1, 2, kFrame) == 168.0F);
  CHECK(AchievementRewardX(0, 3, kFrame) == 63.0F);
  CHECK(AchievementRewardX(1, 3, kFrame) == 133.0F);
  CHECK(AchievementRewardX(2, 3, kFrame) == 203.0F);
}

TEST_CASE("Succès : la jauge remplit son intérieur, et se tait au-delà", "[ui][succes]") {
  constexpr float kWidth = 250.0F;
  CHECK(AchievementGaugeFill(kWidth, 5, 10) == std::optional<float>(125.0F));
  // Pleine, elle vaut l'intérieur, et jamais plus.
  CHECK(AchievementGaugeFill(kWidth, 10, 10) == std::optional<float>(kWidth));
  // Le pourcentage est tronqué avant la multiplication : 1/3 vaut 33 %.
  CHECK(AchievementGaugeFill(kWidth, 1, 3) == std::optional<float>(82.0F));
  CHECK(AchievementGaugeFill(kWidth, 0, 10) == std::optional<float>(0.0F));
  CHECK(AchievementGaugeFill(kWidth, 0, 0) == std::optional<float>(0.0F));
  // Au-delà du maximum, rien — pas même le cadre.
  CHECK_FALSE(AchievementGaugeFill(kWidth, 11, 10).has_value());
}

TEST_CASE("Succès : le pouce de l'ascenseur", "[ui][succes]") {
  // Les sous-lignes d'Aventure : 78 de haut, trois sur six.
  const ro::ui::AchievementThumb top = AchievementScrollThumb(78.0F, 0, 3, 3, 6);
  CHECK(top.top == 11.0F);
  CHECK(top.length == 28.0F);
  const ro::ui::AchievementThumb bottom = AchievementScrollThumb(78.0F, 3, 3, 3, 6);
  CHECK(bottom.top == 39.0F);
  // Une longue liste : le pouce ne descend pas sous huit pixels.
  const ro::ui::AchievementThumb tiny = AchievementScrollThumb(405.0F, 0, 995, 5, 1000);
  CHECK(tiny.length == 8.0F);
}

TEST_CASE("Succès : sous la dernière racine, la place est libre pour la recherche",
          "[ui][succes]") {
  // Huit racines, et la plus haute bande de sous-lignes : 358 pixels au plus.
  // Rien n'est touché sous ce bord, quel que soit l'onglet ouvert.
  for (int tab = 0; tab < kTabs; ++tab) {
    CHECK_FALSE(HitAchievementTab(359.0F, tab, kAdventureSubtabs, 0, kTabs).has_value());
  }
  CHECK(HitAchievementTab(357.0F, kTabs - 2, kAdventureSubtabs, 0, kTabs).has_value());
}

TEST_CASE("Succès : la liste remplace le résumé sur une catégorie, ou pendant une recherche",
          "[ui][succes]") {
  ro::ui::AchievementView view;
  CHECK_FALSE(ro::ui::AchievementListMode(view));
  view.searching = true;
  CHECK(ro::ui::AchievementListMode(view));
  view.searching = false;
  view.tab = kAdventure;
  CHECK(ro::ui::AchievementListMode(view));
}

TEST_CASE("Succès : chaque bannière a sa durée, et aucune n'en a pas", "[ui][succes]") {
  using ro::ui::AchievementBannerLifeMs;
  using ro::ui::AchievementBannerStyle;
  CHECK(AchievementBannerLifeMs(AchievementBannerStyle::kNative) == ro::ui::kAchievementBannerMs);
  CHECK(AchievementBannerLifeMs(AchievementBannerStyle::kDiscreet) ==
        ro::ui::kAchievementDiscreetBannerMs);
  CHECK(AchievementBannerLifeMs(AchievementBannerStyle::kNone) == 0);
}

TEST_CASE("Succès : la bannière discrète s'efface en ligne droite à la fin de sa vie",
          "[ui][succes]") {
  using ro::ui::AchievementDiscreetBannerAlpha;
  constexpr std::uint32_t kLife = ro::ui::kAchievementDiscreetBannerMs;
  constexpr std::uint32_t kFade = ro::ui::kAchievementDiscreetFadeMs;
  CHECK(AchievementDiscreetBannerAlpha(0) == 1.0F);
  CHECK(AchievementDiscreetBannerAlpha(kLife - kFade) == 1.0F);
  CHECK(AchievementDiscreetBannerAlpha(kLife - kFade / 2) == 0.5F);
  CHECK(AchievementDiscreetBannerAlpha(kLife) == 0.0F);
  CHECK(AchievementDiscreetBannerAlpha(kLife + kFade) == 0.0F);
}

TEST_CASE("Succès : la bannière se coupe à 72 caractères, jamais dans un caractère",
          "[ui][succes]") {
  CHECK(ro::ui::SplitAchievementBanner("").empty());
  const std::string short_line = "< Premier pas > achieved.";
  REQUIRE(ro::ui::SplitAchievementBanner(short_line).size() == 1);

  const std::string long_line(100, 'a');
  const auto halves = ro::ui::SplitAchievementBanner(long_line);
  REQUIRE(halves.size() == 2);
  CHECK(halves[0].size() == ro::ui::kAchievementBannerLineChars);
  CHECK(halves[1].size() == 28);

  // « é » tient sur deux octets : 72 caractères en font 144.
  std::string accented;
  for (int letter = 0; letter < 80; ++letter) {
    accented += "\xC3\xA9";
  }
  const auto lines = ro::ui::SplitAchievementBanner(accented);
  REQUIRE(lines.size() == 2);
  CHECK(lines[0].size() == 144);
  CHECK(lines[1].size() == 16);
}

TEST_CASE("Succès : l'intérieur d'une jauge est carré, ses deux cadres déduits", "[ui][succes]") {
  const RoundedBox interior = AchievementGaugeInterior(kGaugeMin, kGaugeMax);
  CHECK(interior.min.x == kGaugeMin.x + kGaugeInset);
  CHECK(interior.min.y == kGaugeMin.y + kGaugeInset);
  CHECK(interior.max.x == kGaugeMax.x - kGaugeInset);
  CHECK(interior.max.y == kGaugeMax.y - kGaugeInset);
  CHECK(interior.rounding == 0.0F);
  // Au-delà du maximum, pas de boîte : la jauge entière se tait.
  CHECK_FALSE(AchievementGaugeFillBox(interior, 11, 10).has_value());
}

TEST_CASE("Succès : pleine, la jauge tient dans son intérieur", "[ui][succes]") {
  const RoundedBox interior = AchievementGaugeInterior(kGaugeMin, kGaugeMax);
  float rightmost = 0.0F;
  CHECK(PaintedFillVertices(10, 10, rightmost) > 0);
  // Elle atteint le bord droit de l'intérieur, sans passer sur les cadres.
  CHECK(rightmost == interior.max.x);
}

TEST_CASE("Succès : vide, la jauge ne peint aucun remplissage", "[ui][succes]") {
  float rightmost = 0.0F;
  CHECK(PaintedFillVertices(0, 10, rightmost) == 0);
  CHECK(PaintedFillVertices(0, 0, rightmost) == 0);
}

TEST_CASE("Succès : à moitié, la jauge s'arrête au milieu de son intérieur", "[ui][succes]") {
  const RoundedBox interior = AchievementGaugeInterior(kGaugeMin, kGaugeMax);
  float rightmost = 0.0F;
  CHECK(PaintedFillVertices(5, 10, rightmost) > 0);
  CHECK(rightmost == interior.min.x + (interior.max.x - interior.min.x) * 0.5F);
}
