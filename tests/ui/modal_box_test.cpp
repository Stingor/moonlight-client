// La boîte de confirmation : quand elle paraît, et ce qu'elle rend.

#include "ro/ui/modal_box.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/ui_context.h"

using ro::ui::AskModal;
using ro::ui::DrawModalBox;
using ro::ui::ModalAnswer;
using ro::ui::SkinArt;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

constexpr const char* kBoxId = "##une_question";
constexpr const char* kQuestion = "Tout effacer ?";

// Sans art : la boîte s'ouvre quand même, en fenêtre ordinaire. C'est ce qu'on
// mesure ici — la mécanique, pas la peinture.
const SkinArt kNoArt;

// Une image de jeu : la boîte est dessinée, et l'on rend ce qu'elle répond.
[[nodiscard]] ModalAnswer OneFrame(ro::ui::UiContext& ui) {
  ui.BeginFrame(kViewport, kFrameSeconds);
  const ModalAnswer answer = DrawModalBox(kBoxId, kQuestion, "Oui", "Non", kNoArt);
  ui.EndFrame();
  return answer;
}

constexpr float kPiecePixels = 8.0F;

// Tout l'habillage présent : la boîte peint son cadre et son corps clair.
[[nodiscard]] SkinArt PaintedArt() {
  SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPiecePixels, .height = kPiecePixels});
  }
  return art;
}

// La boîte a-t-elle été dessinée à la dernière image ? ⚠ ON LIT `WasActive` :
// `Active` ne vaut que PENDANT l'image, et se retrouve toujours faux une fois
// la trame close.
[[nodiscard]] bool BoxShown() {
  const ImGuiWindow* box = ImGui::FindWindowByName(kBoxId);
  return box != nullptr && box->WasActive;
}

}  // namespace

TEST_CASE("Boîte modale : elle ne paraît pas tant qu'on ne l'a pas demandée", "[ui][modale]") {
  ro::ui::UiContext ui;
  CHECK(OneFrame(ui) == ModalAnswer::kWaiting);
  // ⚠ ET RIEN N'EST DESSINÉ : une boîte qui existerait en permanence, même
  // invisible, retiendrait le clic et le clavier du jeu.
  CHECK_FALSE(BoxShown());
}

TEST_CASE("Boîte modale : demandée, elle s'ouvre et attend", "[ui][modale]") {
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  AskModal(kBoxId);
  static_cast<void>(DrawModalBox(kBoxId, kQuestion, "Oui", "Non", kNoArt));
  ui.EndFrame();

  // Elle est là, et elle attend : personne n'a répondu.
  CHECK(OneFrame(ui) == ModalAnswer::kWaiting);
  CHECK(BoxShown());
}

TEST_CASE("Boîte modale : peinte, elle écrit à l'encre de la peau, même hors d'une fenêtre",
          "[ui][modale]") {
  // 🔴 LE CORPS EST CLAIR, DONC L'ENCRE DOIT ÊTRE SOMBRE, et personne d'autre
  // que la boîte ne le garantit : une invitation reçue en pleine carte n'est
  // dans aucune fenêtre habillée, et héritait de l'encre blanche d'ImGui.
  ro::ui::UiContext ui;
  const SkinArt art = PaintedArt();
  ui.BeginFrame(kViewport, kFrameSeconds);
  AskModal(kBoxId);
  static_cast<void>(DrawModalBox(kBoxId, kQuestion, "Oui", "Non", art));
  ui.EndFrame();

  ui.BeginFrame(kViewport, kFrameSeconds);
  static_cast<void>(DrawModalBox(kBoxId, kQuestion, "Oui", "Non", art));
  const ImGuiWindow* box = ImGui::FindWindowByName(kBoxId);
  REQUIRE(box != nullptr);
  bool inked = false;
  for (const ImDrawVert& vertex : box->DrawList->VtxBuffer) {
    inked = inked || vertex.col == ro::ui::kSkinTextColor;
  }
  ui.EndFrame();
  CHECK(inked);
}

TEST_CASE("Boîte modale : Échap vaut non, et la referme", "[ui][modale]") {
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  AskModal(kBoxId);
  static_cast<void>(DrawModalBox(kBoxId, kQuestion, "Oui", "Non", kNoArt));
  ui.EndFrame();
  REQUIRE(OneFrame(ui) == ModalAnswer::kWaiting);

  // ⚠ UNE BOÎTE MODALE QU'ON NE PEUT PAS CONGÉDIER AU CLAVIER RETIENT LE JEU
  // ENTIER : elle prend le clic et les touches tant qu'elle est là.
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
  CHECK(OneFrame(ui) == ModalAnswer::kNo);

  // Et elle est refermée : la question ne se repose pas toute seule.
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
  CHECK(OneFrame(ui) == ModalAnswer::kWaiting);
  // ⚠ UNE IMAGE DE PLUS AVANT DE CONSTATER SA DISPARITION. `WasActive` porte
  // l'image PRÉCÉDENTE, et la boîte était encore dessinée sur celle où elle a
  // répondu — c'est dans son corps qu'on a lu la touche. Regarder trop tôt
  // mesurerait la boîte au moment où elle se ferme.
  CHECK(OneFrame(ui) == ModalAnswer::kWaiting);
  CHECK_FALSE(BoxShown());
}
