// QUITTER LE JEU DEPUIS LE LOBBY : le bouton du formulaire de connexion sous
// chacune de ses formes, la confirmation d'Échap, et la sortie de la tablée qui
// reste entière dans l'écran.

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/character_select_window.h"
#include "ro/ui/escape_stack.h"
#include "ro/ui/login_window.h"
#include "ro/ui/options_window.h"
#include "ro/ui/seat_layout.h"
#include "ro/ui/ui_context.h"

using ro::ui::LoginStage;
using ro::ui::LoginVerb;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr int kSettleFrames = 4;
constexpr float kSweepStepPixels = 4.0F;
constexpr float kEdgeMarginPixels = 2.0F;

// Le formulaire, ses réglages et la pile d'Échap, dans l'ordre où l'étage
// d'interface les passe.
struct LoginProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art;
  ro::ui::LoginWindowState state;
  ro::ui::LoginWindowView view;
  ro::ui::OptionsWindowState settings;
  ro::ui::EscapeStack escape;
  // Le formulaire a-t-il demandé une connexion, à une image quelconque ?
  bool submitted = false;

  LoginVerb Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ro::ui::LoginWindowAction action = ro::ui::DrawLoginWindow(state, view, art);
    submitted = submitted || action.verb == LoginVerb::kSubmit;
    static_cast<void>(ro::ui::DrawLoginSettingsWindow(
        settings, std::span<const ro::ui::OptionSectionView>(), art));
    if (ro::ui::FollowLoginEscape(escape, &settings.open, art)) {
      action.verb = LoginVerb::kQuit;
    }
    ui.EndFrame();
    return action.verb;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }

  void MoveTo(ImVec2 where) {
    ImGui::GetIO().AddMousePosEvent(where.x, where.y);
    static_cast<void>(Frame());
  }

  // Un clic complet, et le verbe de l'image de la pression ou du relâchement.
  LoginVerb Click() {
    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    const LoginVerb pressed = Frame();
    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    const LoginVerb released = Frame();
    return pressed != LoginVerb::kNothing ? pressed : released;
  }

  // Une touche frappée puis relâchée ; rend le verbe de l'image de la frappe.
  LoginVerb Key(ImGuiKey key) {
    ImGui::GetIO().AddKeyEvent(key, true);
    const LoginVerb pressed = Frame();
    ImGui::GetIO().AddKeyEvent(key, false);
    const LoginVerb released = Frame();
    return pressed != LoginVerb::kNothing ? pressed : released;
  }
};

// Le premier élément interactif rencontré depuis le coin bas-droit d'une
// fenêtre, rangée par rangée en remontant.
[[nodiscard]] ImVec2 LowestRightmostControl(LoginProbe& probe, const char* window_name) {
  const ImGuiWindow* window = ImGui::FindWindowByName(window_name);
  REQUIRE(window != nullptr);
  const ImVec2 corner(window->Pos.x + window->Size.x - kEdgeMarginPixels,
                      window->Pos.y + window->Size.y - kEdgeMarginPixels);
  for (float y = corner.y; y > window->Pos.y; y -= kSweepStepPixels) {
    for (float x = corner.x; x > window->Pos.x; x -= kSweepStepPixels) {
      probe.MoveTo(ImVec2(x, y));
      if (GImGui->HoveredId != 0) {
        return ImVec2(x, y);
      }
    }
  }
  return ImVec2(-1.0F, -1.0F);
}

// Le bouton « oui » de la confirmation : le premier rencontré de gauche à
// droite, sur la plus basse rangée qui en porte.
[[nodiscard]] ImVec2 YesButton(LoginProbe& probe) {
  const ImGuiWindow* box = ImGui::FindWindowByName(ro::ui::kLoginQuitConfirmId);
  REQUIRE(box != nullptr);
  for (float y = box->Pos.y + box->Size.y - kEdgeMarginPixels; y > box->Pos.y;
       y -= kSweepStepPixels) {
    for (float x = box->Pos.x + kEdgeMarginPixels; x < box->Pos.x + box->Size.x;
         x += kSweepStepPixels) {
      probe.MoveTo(ImVec2(x, y));
      if (GImGui->HoveredId != 0) {
        return ImVec2(x, y);
      }
    }
  }
  return ImVec2(-1.0F, -1.0F);
}

// La confirmation est-elle à l'écran ? ⚠ `WasActive` porte l'image PRÉCÉDENTE
// — `Active` ne vaut que pendant l'image — : on en passe donc une de plus avant
// de regarder.
[[nodiscard]] bool ConfirmShown(LoginProbe& probe) {
  static_cast<void>(probe.Frame());
  const ImGuiWindow* box = ImGui::FindWindowByName(ro::ui::kLoginQuitConfirmId);
  return box != nullptr && box->WasActive;
}

[[nodiscard]] bool SettingsShown() {
  const ImGuiWindow* window = ImGui::FindWindowByName(ro::ui::kLoginSettingsWindowId);
  return window != nullptr && window->WasActive;
}

void Fill(std::span<char> field, std::string_view text) {
  std::ranges::copy(text.substr(0, field.size() - 1), field.begin());
}

// Une forme de la fenêtre de connexion.
struct LoginShape {
  const char* name;
  bool web_door;
  LoginStage stage;
};

constexpr std::array<LoginShape, 4> kShapes = {
    LoginShape{.name = "formulaire", .web_door = false, .stage = LoginStage::kForm},
    LoginShape{.name = "connexion en cours", .web_door = false, .stage = LoginStage::kConnecting},
    LoginShape{.name = "code PIN", .web_door = false, .stage = LoginStage::kPincode},
    LoginShape{.name = "porte Moonlight", .web_door = true, .stage = LoginStage::kForm},
};

}  // namespace

TEST_CASE("Connexion : « Quitter le jeu » sort sous chaque forme de la fenêtre",
          "[ui][login][quit]") {
  for (const LoginShape& shape : kShapes) {
    DYNAMIC_SECTION(shape.name) {
      LoginProbe probe;
      probe.view.web_door_offered = shape.web_door;
      probe.view.stage = shape.stage;
      probe.Settle();
      // ⚠ LE DERNIER BOUTON, EN BAS À DROITE : c'est là que le joueur le cherche,
      // et sous aucune forme rien ne doit se glisser dessous.
      const ImVec2 button = LowestRightmostControl(probe, "###login");
      REQUIRE(button.x >= 0.0F);
      // 🔴 VIF MÊME PENDANT LA CONNEXION : c'est justement quand le serveur ne
      // répond pas qu'on cherche la sortie.
      CHECK_FALSE(GImGui->HoveredIdIsDisabled);
      CHECK(probe.Click() == LoginVerb::kQuit);
    }
  }
}

TEST_CASE("Connexion : Échap demande, et Échap de nouveau annule", "[ui][login][quit]") {
  LoginProbe probe;
  probe.Settle();
  CHECK(probe.Key(ImGuiKey_Escape) == LoginVerb::kNothing);
  REQUIRE(ConfirmShown(probe));

  // Échap vaut « non » : rien ne part, et la boîte se ferme sans se reposer.
  CHECK(probe.Key(ImGuiKey_Escape) == LoginVerb::kNothing);
  probe.Settle();
  CHECK_FALSE(ConfirmShown(probe));
}

TEST_CASE("Connexion : Entrée confirme la sortie, sans envoyer le formulaire",
          "[ui][login][quit]") {
  LoginProbe probe;
  // 🔴 LES DEUX CHAMPS REMPLIS : Entrée connecte depuis n'importe où sur ce
  // formulaire, et ne doit pas partir en même temps que la sortie.
  Fill(probe.state.account, "compte");
  Fill(probe.state.password, "secret");
  probe.Settle();
  static_cast<void>(probe.Key(ImGuiKey_Escape));
  REQUIRE(ConfirmShown(probe));
  CHECK(probe.Key(ImGuiKey_Enter) == LoginVerb::kQuit);
  CHECK_FALSE(probe.submitted);
}

TEST_CASE("Connexion : le bouton « Quitter » de la confirmation sort", "[ui][login][quit]") {
  LoginProbe probe;
  probe.Settle();
  static_cast<void>(probe.Key(ImGuiKey_Escape));
  REQUIRE(ConfirmShown(probe));
  REQUIRE(YesButton(probe).x >= 0.0F);
  CHECK(probe.Click() == LoginVerb::kQuit);
}

TEST_CASE("Connexion : Réglages ouverts, Échap les ferme sans rien demander", "[ui][login][quit]") {
  LoginProbe probe;
  probe.settings.open = true;
  probe.Settle();
  REQUIRE(SettingsShown());

  CHECK(probe.Key(ImGuiKey_Escape) == LoginVerb::kNothing);
  CHECK_FALSE(probe.settings.open);
  probe.Settle();
  CHECK_FALSE(ConfirmShown(probe));

  // Les Réglages fermés, l'Échap suivant pose la question.
  static_cast<void>(probe.Key(ImGuiKey_Escape));
  CHECK(ConfirmShown(probe));
}

TEST_CASE("Tablée : « Quitter le jeu » reste dans l'écran quand l'ancre le pousserait dehors",
          "[ui][seats][quit]") {
  // 🔴 UNE ANCRE FIXÉE SUR UN ÉCRAN PLUS LARGE : sa fraction, reportée sur
  // celui-ci, ferait commencer la barre de sortie au bord droit, et son second
  // bouton — la sortie du jeu — tomberait entièrement hors champ.
  constexpr float kWideScreenAnchorX = 0.95F;
  constexpr float kBarsBottomPixels = 52.0F;
  // Les barres vivent dans la moitié basse de l'écran : la recherche s'y borne.
  constexpr float kLowerHalf = 0.5F;
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ro::ui::CharacterSelectState state;
  state.layout.anchors.push_back(
      ro::ui::SeatAnchor{.name = std::string(ro::ui::kExitBarAnchor),
                         .x = kWideScreenAnchorX,
                         .y = (kViewport.height_px - kBarsBottomPixels) / kViewport.height_px});
  ro::ui::CharacterSelectView view;
  view.creatable_slots = 1;

  const auto frame = [&] {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ro::ui::CharacterSelectAction action = ro::ui::DrawCharacterSelect(state, view, art);
    ui.EndFrame();
    return action.verb;
  };
  for (int settle = 0; settle < kSettleFrames; ++settle) {
    static_cast<void>(frame());
  }

  // LE BOUTON LE PLUS À DROITE DE L'ÉCRAN, cherché par colonnes depuis le bord.
  bool found = false;
  for (float x = kViewport.width_px - kEdgeMarginPixels; x > 0.0F && !found;
       x -= kSweepStepPixels) {
    for (float y = kViewport.height_px - kEdgeMarginPixels; y > kViewport.height_px * kLowerHalf;
         y -= kSweepStepPixels) {
      ImGui::GetIO().AddMousePosEvent(x, y);
      static_cast<void>(frame());
      if (GImGui->HoveredId != 0) {
        found = true;
        break;
      }
    }
  }
  REQUIRE(found);
  ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  const ro::ui::CharacterSelectVerb pressed = frame();
  ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  const ro::ui::CharacterSelectVerb released = frame();
  CHECK((pressed == ro::ui::CharacterSelectVerb::kQuit ||
         released == ro::ui::CharacterSelectVerb::kQuit));
}
