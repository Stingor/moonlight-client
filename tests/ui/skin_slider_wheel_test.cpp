// LA MOLETTE SUR UNE BARRE DE RÉGLAGE : un cran de flèche par cran, et rien
// au passage quand on parcourt une page de réglages.

#include <array>
#include <cstddef>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/skin_widgets.h"
#include "ro/ui/ui_context.h"

using ro::ui::SkinArt;
using ro::ui::SkinSliderFloat;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
// Plus que le délai de pose, d'une demi-seconde près : trente images.
constexpr int kSettleFrames = 30;
// Plus que la fin d'une salve, de défilement comme de réglage.
constexpr int kBurstOverFrames = 60;
// Entre deux crans d'une même salve : bien moins que sa durée, mais plus que
// le délai de pose — c'est le verrou de salve qui doit tenir, pas la pose.
constexpr int kFramesBetweenNotches = 20;
constexpr int kNotchesInBurst = 15;
constexpr int kTuningNotches = 6;
// Un point qui tombe DANS la barre : elle commence au coin de la fenêtre, et
// quelques dizaines de pixels suffisent à être dessus sans être sur une flèche.
// La barre s'étend de (8, 27) à (840, 46) dans une fenêtre posée au coin :
// ce point tombe dedans, et loin de ses deux flèches.
constexpr float kOnTheBarX = 400.0F;
constexpr float kOnTheBarY = 36.0F;
// Ailleurs dans la fenêtre, sur aucune barre.
constexpr float kOffTheBarY = 600.0F;
// Au milieu d'une page de barres, loin de son bord.
constexpr float kInThePageY = 150.0F;
constexpr float kPageHeight = 300.0F;
constexpr std::size_t kSliderCount = 40;
constexpr float kStartValue = 50.0F;
constexpr float kLargestValue = 100.0F;
constexpr int kNoRow = -1;
// Un item actif qui n'est pas la barre. Sa valeur n'a aucune importance :
// tout ce qui compte est qu'elle ne soit ni zéro ni celle de la barre.
constexpr ImGuiID kSomeOtherWidget = 12345;

template <typename Draw>
void Frame(ro::ui::UiContext& ui, Draw&& draw) {
  ui.BeginFrame(kViewport, kFrameSeconds);
  draw();
  ui.EndFrame();
}

template <typename Draw>
void Frames(ro::ui::UiContext& ui, int count, Draw&& draw) {
  for (int frame = 0; frame < count; ++frame) {
    Frame(ui, draw);
  }
}

// Une page de réglages plus haute que sa fenêtre : des barres jointives, sans
// espace entre elles — où que tombe le pointeur dans la page, il est sur une
// barre.
struct SliderPage {
  SkinArt art;
  std::array<float, kSliderCount> values{};
  float scroll = 0.0F;
  float scroll_max = 0.0F;
  int hovered_row = kNoRow;

  SliderPage() { values.fill(kStartValue); }

  void Draw() {
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kViewport.width_px, kPageHeight));
    ImGui::Begin("page");
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0F));
    hovered_row = kNoRow;
    for (std::size_t row = 0; row < values.size(); ++row) {
      ImGui::PushID(static_cast<int>(row));
      static_cast<void>(
          SkinSliderFloat("##barre", &values[row], 0.0F, kLargestValue, art, "%.0f", 1.0F));
      if (ImGui::IsItemHovered()) {
        hovered_row = static_cast<int>(row);
      }
      ImGui::PopID();
    }
    ImGui::PopStyleVar();
    scroll = ImGui::GetScrollY();
    scroll_max = ImGui::GetScrollMaxY();
    ImGui::End();
  }

  [[nodiscard]] bool Untouched() const {
    for (const float value : values) {
      if (value != kStartValue) {
        return false;
      }
    }
    return true;
  }
};

}  // namespace

TEST_CASE("Barre : posée dessus, la molette règle sans qu'on ait empoigné la valeur",
          "[ui][widgets][slider]") {
  // ⚠ LA SOURIS SE POSE AVANT L'IMAGE. ImGui décide de la fenêtre survolée en
  // tête de `NewFrame`, et depuis la file d'évènements — écrire `io.MousePos` ne
  // survole rien, et l'essai passerait sans avoir rien exercé.
  ro::ui::UiContext ui;
  const SkinArt art;  // sans habillage : la barre se peint elle-même
  float value = kStartValue;
  ImGuiIO& io = ImGui::GetIO();
  const auto draw = [&art, &value]() {
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kViewport.width_px, kViewport.height_px));
    ImGui::Begin("essai");
    static_cast<void>(SkinSliderFloat("##barre", &value, 0.0F, kLargestValue, art, "%.0f", 1.0F));
    ImGui::End();
  };

  Frame(ui, draw);
  io.AddMousePosEvent(kOnTheBarX, kOnTheBarY);
  Frames(ui, kSettleFrames, draw);
  io.AddMouseWheelEvent(0.0F, 1.0F);
  Frame(ui, draw);
  CHECK(value == kStartValue + 1.0F);
}

TEST_CASE("Barre : la molette n'abat pas le programme quand un autre widget est actif",
          "[ui][widgets][slider]") {
  // 🔴 C'EST UN PLANTAGE EN JEU, pas un défaut d'affichage. Marquer un item
  // « édité » n'est permis qu'à l'item ACTIF — ou quand aucun ne l'est — et ImGui
  // le vérifie par une assertion, qui TUE le programme.
  //
  // ⚠ OR LA MOLETTE ÉDITE SANS SAISIR : elle nous atteint dès le survol, et rien
  // ne nous rend actifs. Il suffit qu'un autre widget tienne l'item actif au
  // moment où le cran tombe.
  ro::ui::UiContext ui;
  const SkinArt art;
  float value = kStartValue;
  ImGuiIO& io = ImGui::GetIO();
  const auto draw = [&art, &value]() {
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kViewport.width_px, kViewport.height_px));
    ImGui::Begin("essai");
    static_cast<void>(SkinSliderFloat("##barre", &value, 0.0F, kLargestValue, art, "%.0f", 1.0F));
    ImGui::End();
  };

  Frame(ui, draw);
  io.AddMousePosEvent(kOnTheBarX, kOnTheBarY);
  Frames(ui, kSettleFrames, draw);

  io.AddMouseWheelEvent(0.0F, 1.0F);
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::SetActiveID(kSomeOtherWidget, ImGui::GetCurrentWindow());
  // Le survol reste permis malgré l'item actif : c'est ce que pose tout widget
  // qui accepte le recouvrement, et c'est là que la molette nous atteint sans
  // que nous soyons actifs.
  ImGui::GetCurrentContext()->ActiveIdAllowOverlap = true;
  draw();
  ui.EndFrame();

  // ⚠ CE QUI EST VÉRIFIÉ ICI EST QU'ON Y ARRIVE : une assertion d'ImGui abat le
  // programme d'essai, elle ne se rattrape pas. La valeur, elle, bouge quand
  // même — se taire pour ne pas planter serait une autre panne.
  CHECK(value > kStartValue);
}

TEST_CASE("Barre : un cran vaut l'unité que le format affiche", "[ui][widgets][slider]") {
  // 🔴 UN PAS D'UN CENTIÈME QUELLE QUE SOIT LA BARRE ne marcherait que dans un
  // sens : la molette vers le bas descendrait la valeur, la molette vers le haut
  // ne ferait RIEN. Le réglage arrondit vers zéro en rangeant la valeur —
  // 100 + 0,01 reviendrait à 100, tandis que 100 - 0,01 tomberait à 99.
  //
  // ⚠ LE FORMAT EST LA SEULE CHOSE QUI DISE L'UNITÉ du réglage. « %.0f px »
  // compte en pixels, et un centième de pixel n'a aucun sens.
  ro::ui::UiContext ui;
  const SkinArt art;
  ImGuiIO& io = ImGui::GetIO();
  float whole = kStartValue;
  float fine = kStartValue;
  const auto draw = [&art, &whole, &fine]() {
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kViewport.width_px, kViewport.height_px));
    ImGui::Begin("essai");
    static_cast<void>(SkinSliderFloat("##entiers", &whole, 0.0F, kLargestValue, art, "%.0f px"));
    static_cast<void>(SkinSliderFloat("##centiemes", &fine, 0.0F, kLargestValue, art, "%.2f"));
    ImGui::End();
  };

  Frame(ui, draw);
  io.AddMousePosEvent(kOnTheBarX, kOnTheBarY);
  Frames(ui, kSettleFrames, draw);

  io.AddMouseWheelEvent(0.0F, 1.0F);
  Frame(ui, draw);
  CHECK(whole == kStartValue + 1.0F);
  CHECK(fine == kStartValue);  // la souris n'est que sur la première barre

  // ⚠ ET LES DEUX SENS AVANCENT PAREIL : c'est l'asymétrie qui se verrait en
  // jeu, pas la valeur du pas.
  io.AddMouseWheelEvent(0.0F, -1.0F);
  Frame(ui, draw);
  CHECK(whole == kStartValue);
}

TEST_CASE("Barre : un cran à peine arrivé dessus ne règle rien, la page défile",
          "[ui][widgets][slider]") {
  // 🔴 LE DÉLAI DE POSE : parcourir une page de réglages à la molette fait
  // passer le pointeur sur des barres, et aucune ne doit changer au passage.
  ro::ui::UiContext ui;
  SliderPage page;
  ImGuiIO& io = ImGui::GetIO();
  const auto draw = [&page]() { page.Draw(); };

  Frame(ui, draw);
  io.AddMousePosEvent(kOnTheBarX, kInThePageY);
  Frame(ui, draw);
  REQUIRE(page.hovered_row != kNoRow);
  io.AddMouseWheelEvent(0.0F, -1.0F);
  Frame(ui, draw);
  CHECK(page.Untouched());
  CHECK(page.scroll > 0.0F);
}

TEST_CASE("Barre : revenir sur une barre quittée repart de zéro", "[ui][widgets][slider]") {
  // ⚠ LE SURVOL EST NEUF AUSSI QUAND L'IMAGE PRÉCÉDENTE NE SURVOLAIT PAS : sans
  // quoi la barre hériterait du chrono de sa visite d'avant.
  ro::ui::UiContext ui;
  const SkinArt art;
  float value = kStartValue;
  ImGuiIO& io = ImGui::GetIO();
  const auto draw = [&art, &value]() {
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kViewport.width_px, kViewport.height_px));
    ImGui::Begin("essai");
    static_cast<void>(SkinSliderFloat("##barre", &value, 0.0F, kLargestValue, art, "%.0f", 1.0F));
    ImGui::End();
  };

  Frame(ui, draw);
  io.AddMousePosEvent(kOnTheBarX, kOnTheBarY);
  Frames(ui, kSettleFrames, draw);
  io.AddMousePosEvent(kOnTheBarX, kOffTheBarY);
  Frames(ui, kSettleFrames, draw);
  io.AddMousePosEvent(kOnTheBarX, kOnTheBarY);
  Frame(ui, draw);
  io.AddMouseWheelEvent(0.0F, 1.0F);
  Frame(ui, draw);
  CHECK(value == kStartValue);
}

TEST_CASE("Barre : tant qu'une salve fait défiler la page, aucune barre croisée ne prend rien",
          "[ui][widgets][slider]") {
  // 🔴 LE VERROU DE SALVE : les barres glissent sous un pointeur immobile, la
  // page arrive en butée, le pointeur s'y immobilise sur une barre — et les
  // crans qui suivent appartiennent encore au défilement, bien au-delà du délai
  // de pose.
  ro::ui::UiContext ui;
  SliderPage page;
  ImGuiIO& io = ImGui::GetIO();
  const auto draw = [&page]() { page.Draw(); };

  Frame(ui, draw);
  io.AddMousePosEvent(kOnTheBarX, kInThePageY);
  Frame(ui, draw);
  for (int notch = 0; notch < kNotchesInBurst; ++notch) {
    io.AddMouseWheelEvent(0.0F, -1.0F);
    Frames(ui, kFramesBetweenNotches, draw);
  }
  REQUIRE(page.scroll_max > 0.0F);
  CHECK(page.scroll == page.scroll_max);  // en butée bien avant la fin
  REQUIRE(page.hovered_row != kNoRow);
  CHECK(page.Untouched());

  // La salve finie et la pose atteinte, le cran suivant est à la barre.
  Frames(ui, kBurstOverFrames, draw);
  const int under = page.hovered_row;
  io.AddMouseWheelEvent(0.0F, 1.0F);
  Frame(ui, draw);
  CHECK(page.values.at(static_cast<std::size_t>(under)) == kStartValue + 1.0F);
  CHECK(page.scroll == page.scroll_max);
}

TEST_CASE("Barre : pendant qu'on la règle, elle garde la molette et la page ne bouge pas",
          "[ui][widgets][slider]") {
  ro::ui::UiContext ui;
  SliderPage page;
  ImGuiIO& io = ImGui::GetIO();
  const auto draw = [&page]() { page.Draw(); };

  Frame(ui, draw);
  io.AddMousePosEvent(kOnTheBarX, kInThePageY);
  Frames(ui, kSettleFrames, draw);
  const int under = page.hovered_row;
  REQUIRE(under != kNoRow);
  for (int notch = 0; notch < kTuningNotches; ++notch) {
    io.AddMouseWheelEvent(0.0F, 1.0F);
    Frames(ui, kFramesBetweenNotches, draw);
  }
  CHECK(page.values.at(static_cast<std::size_t>(under)) ==
        kStartValue + static_cast<float>(kTuningNotches));
  CHECK(page.scroll == 0.0F);
}
