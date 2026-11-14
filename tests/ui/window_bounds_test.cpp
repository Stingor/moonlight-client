// Les bornes de fenêtre et l'échelle, sans fenêtre ni carte graphique.

#include "ro/ui/window_bounds.h"

#include <cfloat>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

// Pose une fenêtre là où le joueur l'aurait traînée, puis la redessine.
//
// ⚠ DEUX IMAGES, ET C'EST CE QUI REND L'ESSAI VALABLE. La consigne de position
// ne vaut QUE dans l'image où elle est donnée — la donner hors trame ne fait
// rien du tout, et l'essai mesurerait alors une fenêtre posée par défaut, donc
// déjà dans l'écran. La seconde image ne redit rien : la borne tourne à son
// début, et plus aucun `SetNext...` ne vient la contredire.
void PlaceThenRedraw(ro::ui::UiContext& ui, const char* name, ImVec2 pos, ImVec2 size,
                     ImGuiWindowFlags flags = 0) {
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::SetNextWindowPos(pos);
  ImGui::SetNextWindowSize(size);
  ImGui::Begin(name, nullptr, flags);
  ImGui::TextUnformatted("quelque chose");
  ImGui::End();
  ui.EndFrame();

  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::Begin(name, nullptr, flags);
  ImGui::TextUnformatted("quelque chose");
  ImGui::End();
  ui.EndFrame();
}

// La taille d'une poignée, vue de l'intérieur de la fenêtre. Trois pixels
// suffisent à tomber dedans : ImGui lui en donne une douzaine.
constexpr float kInsideGrip = 3.0F;

// Tire la poignée bas-droite comme le joueur le ferait : survoler, cliquer,
// glisser. Rend la fenêtre telle que le glisser vient de la laisser.
//
// ⚠ QUATRE IMAGES, ET AUCUNE N'EST DE TROP. ImGui décide du survol à partir des
// rectangles de l'image PRÉCÉDENTE, n'arme la poignée qu'au clic, et ne
// recalcule la taille qu'au déplacement suivant. Sauter une étape, c'est mesurer
// une fenêtre que personne n'a tirée — un essai qui passe sans rien prouver.
//
// ⚠ ON MESURE AVANT L'IMAGE SUIVANTE, et c'est le point : les deux passes de
// bornage tournent au début de celle-ci et ramèneraient la fenêtre. L'essai ne
// dirait alors plus si la poignée a buté ou si le filet l'a rattrapée.
ImGuiWindow* DragLowerRightGrip(ro::ui::UiContext& ui, const char* name, ImVec2 pos, ImVec2 size,
                                ImVec2 toward, ImVec2 smallest = ImVec2(0.0F, 0.0F)) {
  const auto draw = [&]() {
    if (smallest.x > 0.0F) {
      ImGui::SetNextWindowSizeConstraints(smallest, ImVec2(FLT_MAX, FLT_MAX));
    }
    ro::ui::ConstrainNextWindowToScreen(name);
    ImGui::Begin(name, nullptr, 0);
    ImGui::TextUnformatted("quelque chose");
    ImGui::End();
  };

  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::SetNextWindowPos(pos);
  ImGui::SetNextWindowSize(size);
  draw();
  ui.EndFrame();

  // Le curseur sur la poignée, bouton relâché : c'est ce survol qui la rend
  // saisissable à l'image d'après.
  ImGuiIO& io = ImGui::GetIO();
  io.MousePos = ImVec2(pos.x + size.x - kInsideGrip, pos.y + size.y - kInsideGrip);
  io.MouseDown[0] = false;
  ui.BeginFrame(kViewport, kFrameSeconds);
  draw();
  ui.EndFrame();

  io.MouseDown[0] = true;  // le clic arme la poignée
  ui.BeginFrame(kViewport, kFrameSeconds);
  draw();
  ui.EndFrame();

  io.MousePos = toward;  // et le glisser la tire
  ui.BeginFrame(kViewport, kFrameSeconds);
  draw();
  ui.EndFrame();

  io.MouseDown[0] = false;
  return ImGui::FindWindowByName(name);
}

}  // namespace

TEST_CASE("une fenêtre ne quitte pas l'écran", "[ui][bornes]") {
  ro::ui::UiContext ui;
  PlaceThenRedraw(ui, "perdue", ImVec2(2000.0F, 900.0F), ImVec2(300.0F, 200.0F));

  ImGuiWindow* window = ImGui::FindWindowByName("perdue");
  REQUIRE(window != nullptr);
  // ENTIÈREMENT dedans : le coin bas-droit ne dépasse pas. Sans la borne, ImGui
  // n'en laisse qu'un moignon — de quoi perdre la fenêtre pour de bon.
  CHECK(window->Pos.x >= 0.0F);
  CHECK(window->Pos.y >= 0.0F);
  CHECK(window->Pos.x + window->Size.x <= kViewport.width_px);
  CHECK(window->Pos.y + window->Size.y <= kViewport.height_px);
}

TEST_CASE("une fenêtre posée en négatif revient dans l'écran", "[ui][bornes]") {
  ro::ui::UiContext ui;
  PlaceThenRedraw(ui, "en-haut-a-gauche", ImVec2(-400.0F, -300.0F), ImVec2(300.0F, 200.0F));

  ImGuiWindow* window = ImGui::FindWindowByName("en-haut-a-gauche");
  REQUIRE(window != nullptr);
  CHECK(window->Pos.x == 0.0F);
  CHECK(window->Pos.y == 0.0F);
}

TEST_CASE("une fenêtre ne dépasse pas quatre-vingt-dix pour cent de l'écran", "[ui][bornes]") {
  ro::ui::UiContext ui;
  PlaceThenRedraw(ui, "geante", ImVec2(0.0F, 0.0F), ImVec2(4000.0F, 3000.0F));

  ImGuiWindow* window = ImGui::FindWindowByName("geante");
  REQUIRE(window != nullptr);
  CHECK(window->SizeFull.x <= kViewport.width_px * ro::ui::kMaxScreenFraction);
  CHECK(window->SizeFull.y <= kViewport.height_px * ro::ui::kMaxScreenFraction);
  // Et il reste de quoi attraper la fenêtre et voir le jeu derrière.
  CHECK(window->SizeFull.x < kViewport.width_px);
}

TEST_CASE("une fenêtre qui ne se redimensionne pas garde sa taille", "[ui][bornes]") {
  // ⚠ CE N'EST PAS UN OUBLI. Sa taille vient du CONTENU ou de l'appelant, pas
  // d'un glisser du joueur : la couper tronquerait ce qu'elle a à montrer sans
  // que personne n'ait rien demandé.
  ro::ui::UiContext ui;
  PlaceThenRedraw(ui, "figee", ImVec2(0.0F, 0.0F), ImVec2(4000.0F, 3000.0F),
                  ImGuiWindowFlags_NoResize);

  ImGuiWindow* window = ImGui::FindWindowByName("figee");
  REQUIRE(window != nullptr);
  CHECK(window->SizeFull.x > kViewport.width_px);
}

TEST_CASE("le bornage de position rend la position telle quelle si elle tient", "[ui][bornes]") {
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);

  const ImVec2 dedans =
      ro::ui::ClampWindowPosToScreen(ImVec2(100.0F, 100.0F), ImVec2(200.0F, 200.0F));
  CHECK(dedans.x == 100.0F);
  CHECK(dedans.y == 100.0F);

  // Une fenêtre plus grande que l'écran s'aligne sur le coin haut-gauche :
  // c'est là que sont la barre de titre et les premiers éléments.
  const ImVec2 trop_grande =
      ro::ui::ClampWindowPosToScreen(ImVec2(-50.0F, -50.0F), ImVec2(2000.0F, 2000.0F));
  CHECK(trop_grande.x == 0.0F);
  CHECK(trop_grande.y == 0.0F);

  ui.EndFrame();
}

TEST_CASE("l'échelle se pose, se borne, et grandit les cadres", "[ui][echelle]") {
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);

  const float marge = ImGui::GetStyle().WindowPadding.x;
  REQUIRE(marge > 0.0F);
  CHECK(ui.interface_scale() == 1.0F);

  ui.SetInterfaceScale(2.0F);
  CHECK(ui.interface_scale() == 2.0F);
  CHECK(ImGui::GetStyle().WindowPadding.x == marge * 2.0F);
  // La police suit : sans cela les cadres grandiraient autour d'un texte resté
  // petit. `ScaleAllSizes` ne la touche pas, et le dit dans sa déclaration.
  CHECK(ImGui::GetStyle().FontScaleMain == 2.0F);

  // ⚠ LE STYLE REPART DE L'ORIGINE À CHAQUE FOIS. `ScaleAllSizes` MULTIPLIE ce
  // qui est déjà là : sans la copie d'origine, deux appels de suite donneraient
  // le CARRÉ du facteur, et revenir à un ne rendrait pas la taille de départ.
  // ⚠ LE FACTEUR VIENT DE LA BORNE, PAS D'UN LITTÉRAL : un plafond qui bouge ne
  // doit pas faire échouer un essai qui ne parle pas de lui.
  const float ceiling = ro::ui::kLargestInterfaceScale;
  ui.SetInterfaceScale(ceiling);
  CHECK(ImGui::GetStyle().WindowPadding.x == marge * ceiling);
  ui.SetInterfaceScale(1.0F);
  CHECK(ImGui::GetStyle().WindowPadding.x == marge);

  // Et le facteur est borné des deux côtés : une faute de frappe ne peut pas
  // rendre l'interface invisible.
  ui.SetInterfaceScale(0.0F);
  CHECK(ui.interface_scale() == ro::ui::kSmallestInterfaceScale);
  ui.SetInterfaceScale(100.0F);
  CHECK(ui.interface_scale() == ro::ui::kLargestInterfaceScale);

  ui.EndFrame();
}

TEST_CASE("changer l'échelle redimensionne les fenêtres déjà ouvertes", "[ui][echelle]") {
  ro::ui::UiContext ui;
  PlaceThenRedraw(ui, "mise à l'échelle", ImVec2(200.0F, 100.0F), ImVec2(300.0F, 180.0F));

  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGuiWindow* window = ImGui::FindWindowByName("mise à l'échelle");
  REQUIRE(window != nullptr);
  const ImVec2 before = window->SizeFull;
  REQUIRE(before.x > 0.0F);

  ui.SetInterfaceScale(2.0F);
  // ⚠ CE QUE LE STYLE SEUL NE FAIT PAS. `ScaleAllSizes` grossit les marges et la
  // police ; la taille d'une fenêtre est en pixels et resterait telle quelle —
  // un texte deux fois plus gros dans un cadre resté petit, donc un réglage qui
  // a l'air de ne rien faire.
  CHECK(window->SizeFull.x == before.x * 2.0F);
  CHECK(window->SizeFull.y == before.y * 2.0F);
  ui.EndFrame();
}

TEST_CASE("la poignée bute sur le bord de l'écran", "[ui][bornes]") {
  ro::ui::UiContext ui;
  ImGuiWindow* window = DragLowerRightGrip(ui, "tiree", ImVec2(900.0F, 500.0F),
                                           ImVec2(300.0F, 180.0F), ImVec2(3000.0F, 2000.0F));
  REQUIRE(window != nullptr);
  // ELLE A GRANDI — sans quoi l'essai ne mesurerait qu'une poignée jamais saisie.
  CHECK(window->SizeFull.x > 300.0F);
  // ET ELLE S'EST ARRÊTÉE AU BORD, dans l'image même. Sans la contrainte, le coin
  // bas-droit suit la souris jusqu'à trois mille pixels, et la passe de position
  // fait alors glisser la fenêtre vers la gauche au lieu de l'arrêter.
  CHECK(window->Pos.x + window->SizeFull.x <= kViewport.width_px);
  CHECK(window->Pos.y + window->SizeFull.y <= kViewport.height_px);
}

TEST_CASE("la poignée bute aussi sur la part d'écran", "[ui][bornes]") {
  ro::ui::UiContext ui;
  ImGuiWindow* window = DragLowerRightGrip(ui, "etiree", ImVec2(0.0F, 0.0F), ImVec2(200.0F, 150.0F),
                                           ImVec2(3000.0F, 2000.0F));
  REQUIRE(window != nullptr);
  // Posée dans le coin, rien ne l'arrête avant nos quatre-vingt-dix pour cent :
  // c'est l'autre borne, et elle vaut pendant le geste aussi.
  CHECK(window->SizeFull.x <= kViewport.width_px * ro::ui::kMaxScreenFraction);
  CHECK(window->SizeFull.y <= kViewport.height_px * ro::ui::kMaxScreenFraction);
}

TEST_CASE("le plancher de l'appelant survit à la borne d'écran", "[ui][bornes]") {
  // ⚠ ON RABAT SON PLAFOND, PAS SON PLANCHER : la chatbox exige quatre cents
  // pixels de large, et une fenêtre tassée sous son minimum ne montre plus rien.
  ro::ui::UiContext ui;
  ImGuiWindow* window =
      DragLowerRightGrip(ui, "plancher", ImVec2(100.0F, 100.0F), ImVec2(500.0F, 300.0F),
                         ImVec2(120.0F, 110.0F), ImVec2(400.0F, 200.0F));
  REQUIRE(window != nullptr);
  CHECK(window->SizeFull.x >= 400.0F);
  CHECK(window->SizeFull.y >= 200.0F);
}
