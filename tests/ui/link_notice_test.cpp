// La boîte de liaison perdue, à un seul bouton : quand elle paraît, et ce qui
// la referme.

#include "ro/ui/link_notice.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/modal_box.h"
#include "ro/ui/ui_context.h"

using ro::ui::DrawLinkNotice;
using ro::ui::DrawNoticeBox;
using ro::ui::LinkNoticeState;
using ro::ui::SkinArt;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr const char* kLost = "Déconnecté du serveur.\nLe serveur ne répond plus.";
constexpr const char* kNoticeId = "##un_avis";

// Sans art : la boîte s'ouvre quand même, en fenêtre ordinaire. C'est la
// mécanique qu'on mesure, pas la peinture.
const SkinArt kNoArt;

void OneFrame(ro::ui::UiContext& ui, LinkNoticeState& state) {
  ui.BeginFrame(kViewport, kFrameSeconds);
  DrawLinkNotice(state, kNoArt);
  ui.EndFrame();
}

// La boîte de perte a-t-elle été dessinée à la dernière image ? Voir
// `modal_box_test.cpp` pour pourquoi on lit `WasActive`.
[[nodiscard]] bool LossBoxShown() {
  const ImGuiWindow* box = ImGui::FindWindowByName(ro::ui::kLinkLossBoxId);
  return box != nullptr && box->WasActive;
}

// Une touche pressée le temps d'une image, puis relâchée.
void PressFor(ro::ui::UiContext& ui, LinkNoticeState& state, ImGuiKey key) {
  ImGui::GetIO().AddKeyEvent(key, true);
  OneFrame(ui, state);
  ImGui::GetIO().AddKeyEvent(key, false);
}

}  // namespace

TEST_CASE("Liaison perdue : sans texte, aucune boîte", "[ui][liaison]") {
  ro::ui::UiContext ui;
  LinkNoticeState state;
  OneFrame(ui, state);
  OneFrame(ui, state);
  CHECK_FALSE(LossBoxShown());
  CHECK_FALSE(state.acknowledged);
}

TEST_CASE("Liaison perdue : le texte posé, la boîte s'ouvre d'elle-même et attend",
          "[ui][liaison]") {
  ro::ui::UiContext ui;
  LinkNoticeState state;
  state.text = kLost;
  OneFrame(ui, state);
  OneFrame(ui, state);
  CHECK(LossBoxShown());
  CHECK_FALSE(state.acknowledged);
}

TEST_CASE("Liaison perdue : Entrée vaut OK", "[ui][liaison]") {
  ro::ui::UiContext ui;
  LinkNoticeState state;
  state.text = kLost;
  OneFrame(ui, state);
  OneFrame(ui, state);
  REQUIRE(LossBoxShown());

  PressFor(ui, state, ImGuiKey_Enter);
  CHECK(state.acknowledged);

  // ⚠ ACQUITTÉE, ELLE NE SE ROUVRE PAS : c'est à `app` de vider le texte.
  OneFrame(ui, state);
  OneFrame(ui, state);
  CHECK_FALSE(LossBoxShown());
}

TEST_CASE("Liaison perdue : Échap vaut OK aussi", "[ui][liaison]") {
  ro::ui::UiContext ui;
  LinkNoticeState state;
  state.text = kLost;
  OneFrame(ui, state);
  OneFrame(ui, state);
  REQUIRE(LossBoxShown());

  PressFor(ui, state, ImGuiKey_Escape);
  CHECK(state.acknowledged);
}

TEST_CASE("Liaison perdue : une autre boîte ouverte ensuite ne la chasse pas", "[ui][liaison]") {
  // 🔴 UNE SECONDE BOÎTE MODALE REMPLACE LA PREMIÈRE dans la pile d'ImGui. Celle
  // de la perte doit revenir : il n'y a plus de serveur pour l'autre.
  //
  // ⚠ LES DEUX SE DESSINENT À CHAQUE IMAGE, l'autre AVANT la nôtre, comme dans
  // l'étage d'interface : c'est ainsi qu'elles se disputeraient la pile.
  constexpr const char* kOtherId = "##une_autre_question";
  constexpr int kFramesAfter = 5;
  ro::ui::UiContext ui;
  LinkNoticeState state;
  state.text = kLost;
  const auto both = [&](bool ask_other) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    if (ask_other) {
      ro::ui::AskModal(kOtherId);
    }
    static_cast<void>(ro::ui::DrawModalBox(kOtherId, "Autre ?", "Oui", "Non", kNoArt));
    DrawLinkNotice(state, kNoArt);
    ui.EndFrame();
  };
  both(false);
  both(true);
  for (int frame = 0; frame < kFramesAfter; ++frame) {
    both(false);
    CHECK(LossBoxShown());
  }
  CHECK_FALSE(state.acknowledged);
}

TEST_CASE("Liaison perdue : une touche frappée à l'ouverture ne la referme pas", "[ui][liaison]") {
  // 🔴 LE JOUEUR QUI MARTÈLE ÉCHAP DEVANT UN JEU FIGÉ ne doit pas refermer la
  // boîte à l'image même où elle paraît : il ne l'aurait jamais vue.
  ro::ui::UiContext ui;
  LinkNoticeState state;
  state.text = kLost;
  PressFor(ui, state, ImGuiKey_Escape);
  CHECK_FALSE(state.acknowledged);
  OneFrame(ui, state);
  CHECK(LossBoxShown());

  // LA FRAPPE SUIVANTE, elle, répond.
  PressFor(ui, state, ImGuiKey_Enter);
  CHECK(state.acknowledged);
}

TEST_CASE("Liaison perdue : une touche tenue depuis avant ne la referme pas, même répétée",
          "[ui][liaison]") {
  // Assez d'images pour que la répétition du clavier se déclenche plusieurs fois.
  constexpr int kHeldFrames = 120;
  ro::ui::UiContext ui;
  LinkNoticeState state;
  state.text = kLost;
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
  for (int frame = 0; frame < kHeldFrames; ++frame) {
    OneFrame(ui, state);
  }
  CHECK_FALSE(state.acknowledged);
  CHECK(LossBoxShown());
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
  OneFrame(ui, state);
}

TEST_CASE("Boîte d'avis : un seul bouton, Entrée et Échap y répondent", "[ui][modale]") {
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  ro::ui::AskModal(kNoticeId);
  CHECK_FALSE(DrawNoticeBox(kNoticeId, kLost, "OK", kNoArt));
  ui.EndFrame();

  ui.BeginFrame(kViewport, kFrameSeconds);
  CHECK_FALSE(DrawNoticeBox(kNoticeId, kLost, "OK", kNoArt));
  ui.EndFrame();

  ImGui::GetIO().AddKeyEvent(ImGuiKey_KeypadEnter, true);
  ui.BeginFrame(kViewport, kFrameSeconds);
  CHECK(DrawNoticeBox(kNoticeId, kLost, "OK", kNoArt));
  ui.EndFrame();
  ImGui::GetIO().AddKeyEvent(ImGuiKey_KeypadEnter, false);

  // REFERMÉE : elle ne répond plus, et ne se rouvre pas d'elle-même.
  ui.BeginFrame(kViewport, kFrameSeconds);
  CHECK_FALSE(DrawNoticeBox(kNoticeId, kLost, "OK", kNoArt));
  ui.EndFrame();
}
