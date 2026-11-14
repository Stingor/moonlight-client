// LA PETITE FENÊTRE DE STATUT : sa largeur, la règle de ses flèches, et ce
// qu'un clic sur l'une d'elles demande.
//
// 🔴 LA FLÈCHE SE MONTRE OU DISPARAÎT, ELLE NE GRISE PAS. Une flèche visible
// sur une caractéristique au plafond enverrait au serveur une montée qu'il
// refuse en silence ; une flèche absente là où les points suffisent laisserait
// le joueur sans geste. Les deux se voient mal à l'œil et s'éprouvent ici.

#include "ro/ui/status_window.h"

#include <cmath>
#include <cstddef>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::CharacterAttributeView;
using ro::ui::StatusVerb;
using ro::ui::StatusWindowAction;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr int kSettleFrames = 4;
constexpr const char* kWindowName = "###status";

// LE FOND ET LA FLÈCHE, À LEUR TAILLE RÉELLE : c'est le fond qui commande la
// largeur, et l'essai ne vaut que s'il la porte.
constexpr float kBackgroundWidthPixels = 302.0F;
constexpr float kBackgroundHeightPixels = 115.0F;
constexpr float kArrowPixels = 11.0F;
constexpr float kPlainPiecePixels = 8.0F;
// Une échelle d'interface autre que un : une largeur prise au fond sans
// l'échelle passerait à un.
constexpr float kLargeScale = 1.5F;

[[nodiscard]] ro::ui::SkinArt StatusArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  art.Set(SkinPiece::kStatusBackground,
          ro::ui::SkinImage{
              .id = 1, .width = kBackgroundWidthPixels, .height = kBackgroundHeightPixels});
  art.Set(SkinPiece::kStatusRaiseArrow,
          ro::ui::SkinImage{.id = 1, .width = kArrowPixels, .height = kArrowPixels});
  return art;
}

// La fenêtre ouverte, dessinée image par image. Les six caractéristiques
// coûtent deux points, et le joueur en a trois : toutes les flèches se
// montrent.
struct StatusProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = StatusArt();
  ro::ui::StatusWindowState state;
  ro::ui::StatusWindowView view;

  StatusProbe() {
    state.open = true;
    for (CharacterAttributeView& attribute : view.attributes) {
      attribute = CharacterAttributeView{.base = 50, .bonus = 0, .cost = 2};
    }
    view.status_point = 3;
  }

  StatusWindowAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    StatusWindowAction action = ro::ui::DrawStatusWindow(state, view, art);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }

  // Le milieu de la flèche de la rangée `rank`, en pixels d'écran.
  [[nodiscard]] static ImVec2 ArrowCenter(std::size_t rank) {
    const ImGuiWindow* window = ImGui::FindWindowByName(kWindowName);
    REQUIRE(window != nullptr);
    const float scale = ImGui::GetStyle().FontScaleMain;
    const float half = ro::ui::kStatusArrowPixels * 0.5F;
    return {window->Pos.x + (ro::ui::kStatusArrowLeftPixels + half) * scale,
            window->Pos.y + window->TitleBarHeight +
                (ro::ui::StatusArrowTopPixels(rank) + half) * scale};
  }

  // Un clic complet en ce point : appui, puis relâché. Rend ce que l'une des
  // images a demandé.
  StatusWindowAction ClickAt(ImVec2 at) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(at.x, at.y);
    StatusWindowAction seen = Frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    const StatusWindowAction pressed = Frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    const StatusWindowAction released = Frame();
    for (const StatusWindowAction& asked : {pressed, released}) {
      if (asked.verb != StatusVerb::kNothing) {
        seen = asked;
      }
    }
    return seen;
  }
};

}  // namespace

TEST_CASE("Statut : la fenêtre prend la largeur de son fond", "[ui][statut]") {
  StatusProbe probe;
  probe.Settle();
  const ImGuiWindow* window = ImGui::FindWindowByName(kWindowName);
  REQUIRE(window != nullptr);
  CHECK(window->Size.x == kBackgroundWidthPixels);
}

TEST_CASE("Statut : la largeur du fond suit l'échelle de l'interface", "[ui][statut]") {
  StatusProbe probe;
  probe.ui.SetInterfaceScale(kLargeScale);
  probe.Settle();
  const ImGuiWindow* window = ImGui::FindWindowByName(kWindowName);
  REQUIRE(window != nullptr);
  CHECK(window->Size.x == kBackgroundWidthPixels * kLargeScale);
}

TEST_CASE("Statut : le corps vaut le fond, quelle que soit la barre de titre", "[ui][statut]") {
  // 🔴 LA BARRE DE CE BANC EST PLUS HAUTE QUE SON ART : ses pièces font huit
  // pixels, la police davantage, et c'est la police qui gagne. Une hauteur
  // totale imposée à « 17 + 115 » rognerait le bas du fond dans ce cas-là.
  for (const float scale : {1.0F, kLargeScale}) {
    StatusProbe probe;
    probe.ui.SetInterfaceScale(scale);
    probe.Settle();
    const ImGuiWindow* window = ImGui::FindWindowByName(kWindowName);
    REQUIRE(window != nullptr);
    CHECK(window->TitleBarHeight > kPlainPiecePixels * scale);
    // Au pixel entier supérieur : à une échelle d'un et demi, le fond fait
    // 172,5 pixels, et la taille d'une fenêtre est entière.
    CHECK(window->Size.y - window->TitleBarHeight == std::ceil(kBackgroundHeightPixels * scale));
  }
}

TEST_CASE("Statut : fermée, elle ne dessine rien", "[ui][statut]") {
  StatusProbe probe;
  probe.state.open = false;
  CHECK(probe.Frame().verb == StatusVerb::kNothing);
  CHECK(ImGui::FindWindowByName(kWindowName) == nullptr);
}

TEST_CASE("Statut : la flèche se cache sans coût, ou quand les points manquent", "[ui][statut]") {
  // ⚠ UN COÛT NUL EST LE PLAFOND, que le serveur annonce ainsi.
  CHECK_FALSE(ro::ui::StatusArrowShown(CharacterAttributeView{.base = 99, .cost = 0}, 50));
  CHECK_FALSE(ro::ui::StatusArrowShown(CharacterAttributeView{.base = 40, .cost = 5}, 4));
}

TEST_CASE("Statut : la flèche se montre dès que les points paient le cran", "[ui][statut]") {
  CHECK(ro::ui::StatusArrowShown(CharacterAttributeView{.base = 40, .cost = 5}, 5));
  CHECK(ro::ui::StatusArrowShown(CharacterAttributeView{.base = 40, .cost = 5}, 6));
}

TEST_CASE("Statut : un clic sur une flèche monte SA caractéristique d'un cran", "[ui][statut]") {
  StatusProbe probe;
  probe.Settle();
  // La quatrième, l'intelligence : un rang qui n'est ni le premier ni le
  // dernier, pour qu'un décalage d'une rangée se voie.
  constexpr std::size_t kIntelligence = 3;
  const StatusWindowAction asked = probe.ClickAt(StatusProbe::ArrowCenter(kIntelligence));
  CHECK(asked.verb == StatusVerb::kRaiseAttribute);
  CHECK(asked.attribute == kIntelligence);
  CHECK(asked.amount == 1);
}

TEST_CASE("Statut : Maj+clic sur une flèche monte jusqu'au palier", "[ui][statut]") {
  StatusProbe probe;
  // 47 + 5 = 52 : le palier suivant est 60, huit crans plus loin.
  constexpr std::size_t kIntelligence = 3;
  probe.view.attributes[kIntelligence] = CharacterAttributeView{.base = 47, .bonus = 5, .cost = 5};
  probe.view.status_point = 50;
  probe.Settle();
  ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, true);
  const StatusWindowAction asked = probe.ClickAt(StatusProbe::ArrowCenter(kIntelligence));
  ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, false);
  CHECK(asked.verb == StatusVerb::kRaiseAttribute);
  CHECK(asked.attribute == kIntelligence);
  CHECK(asked.amount == 8);
}

TEST_CASE("Montée : un cran, ou le palier compté sur le total", "[ui][statut]") {
  const CharacterAttributeView attribute{.base = 47, .bonus = 5, .cost = 5};
  CHECK(ro::ui::AttributeRaiseAmount(attribute, false) == 1);
  CHECK(ro::ui::AttributeRaiseAmount(attribute, true) == 8);
  // Pile sur un palier, Maj monte jusqu'au suivant, pas zéro cran.
  CHECK(ro::ui::AttributeRaiseAmount(CharacterAttributeView{.base = 50}, true) == 10);
}

TEST_CASE("Statut : là où la flèche est cachée, le clic ne demande rien", "[ui][statut]") {
  StatusProbe probe;
  // La dextérité au plafond, et l'agilité trop chère.
  constexpr std::size_t kAgility = 1;
  constexpr std::size_t kDexterity = 4;
  probe.view.attributes[kAgility].cost = 4;
  probe.view.attributes[kDexterity].cost = 0;
  probe.Settle();
  CHECK(probe.ClickAt(StatusProbe::ArrowCenter(kAgility)).verb == StatusVerb::kNothing);
  CHECK(probe.ClickAt(StatusProbe::ArrowCenter(kDexterity)).verb == StatusVerb::kNothing);
}
