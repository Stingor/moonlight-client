// LE CLAVIER DES LISTES : le rang où mène chaque touche, la liste de la base de
// données que la fiche suit et qui défile pour garder la ligne en vue, son
// ascenseur habillé, le fil que le clavier ne noie pas, et la liste déroulante
// ouverte qui se parcourt aux mêmes touches.

#include "ro/ui/list_keys.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/database_window.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/ui_context.h"

using ro::ui::DatabaseKind;
using ro::ui::DatabaseTarget;
using ro::ui::kListNoEntry;
using ro::ui::ListKeyMove;
using ro::ui::ListKeyTarget;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 900.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr int kSettleFrames = 4;
constexpr ImTextureID kSkinTexture = 1;
// Plus de lignes que la liste n'en montre : elle doit défiler.
constexpr std::size_t kRowCount = 200;
constexpr std::uint32_t kFirstId = 500;
// Un point sur le champ de la liste déroulante, loin de sa flèche : la fenêtre
// est au coin, et le champ commence sous sa barre de titre.
constexpr float kOnTheFieldX = 200.0F;
constexpr float kOnTheFieldY = 36.0F;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{
                .id = kSkinTexture, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

[[nodiscard]] DatabaseTarget ItemSheet(std::size_t rank) {
  return DatabaseTarget{.kind = DatabaseKind::kItem,
                        .id = kFirstId + static_cast<std::uint32_t>(rank)};
}

[[nodiscard]] ImGuiWindow* WindowNamed(std::string_view part) {
  for (ImGuiWindow* window : GImGui->Windows) {
    if (window->WasActive && std::string_view(window->Name).find(part) != std::string_view::npos) {
      return window;
    }
  }
  return nullptr;
}

// Vrai quand la fenêtre a peint une pièce de l'habillage : l'ascenseur du jeu,
// ImGui n'en dessinant aucune.
[[nodiscard]] bool PaintsSkin(const ImGuiWindow* window) {
  for (const ImDrawCmd& command : window->DrawList->CmdBuffer) {
    // ⚠ PAS `GetTexID` : l'atlas des polices n'est pas chargé hors écran.
    if (command.TexRef._TexData == nullptr && command.TexRef._TexID == kSkinTexture) {
      return true;
    }
  }
  return false;
}

struct ListProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  ro::ui::DatabaseWindowState state;
  ro::ui::MonsterSheetWindowState monster_state;
  std::vector<std::string> names;
  std::vector<ro::ui::DatabaseRowView> rows;
  ro::ui::DatabaseView view;

  ListProbe() {
    names.reserve(kRowCount);
    for (std::size_t rank = 0; rank < kRowCount; ++rank) {
      names.push_back("Objet " + std::to_string(rank));
    }
    for (std::size_t rank = 0; rank < kRowCount; ++rank) {
      rows.push_back(
          ro::ui::DatabaseRowView{.id = ItemSheet(rank).id, .name = names[rank], .note = "Divers"});
    }
    view.rows = rows;
    view.summary = "200 objets";
    ro::ui::OpenDatabaseOn(state, ItemSheet(0));
  }

  void Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawDatabaseWindow(state, view, monster_state, art));
    ui.EndFrame();
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      Frame();
    }
  }

  void Press(ImGuiKey key) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(key, true);
    Frame();
    io.AddKeyEvent(key, false);
    Settle();
  }

  [[nodiscard]] std::uint32_t shown() const { return state.history.current()->id; }
};

}  // namespace

TEST_CASE("Clavier des listes : le rang où mène chaque touche", "[ui][list_keys]") {
  constexpr int kCount = 10;
  constexpr int kPage = 4;
  CHECK(ListKeyTarget(3, kCount, kPage, ListKeyMove::kNext) == 4);
  CHECK(ListKeyTarget(3, kCount, kPage, ListKeyMove::kPrevious) == 2);
  CHECK(ListKeyTarget(3, kCount, kPage, ListKeyMove::kPageDown) == 7);
  CHECK(ListKeyTarget(3, kCount, kPage, ListKeyMove::kPageUp) == 0);
  CHECK(ListKeyTarget(3, kCount, kPage, ListKeyMove::kFirst) == 0);
  CHECK(ListKeyTarget(3, kCount, kPage, ListKeyMove::kLast) == 9);
  // Bornée aux deux bouts : elle ne boucle pas.
  CHECK(ListKeyTarget(9, kCount, kPage, ListKeyMove::kNext) == 9);
  CHECK(ListKeyTarget(8, kCount, kPage, ListKeyMove::kPageDown) == 9);
  CHECK(ListKeyTarget(0, kCount, kPage, ListKeyMove::kPrevious) == 0);
  // Sans entrée choisie : la première vers le bas, la dernière vers le haut.
  CHECK(ListKeyTarget(kListNoEntry, kCount, kPage, ListKeyMove::kNext) == 0);
  CHECK(ListKeyTarget(kListNoEntry, kCount, kPage, ListKeyMove::kPageUp) == 9);
  CHECK(ListKeyTarget(kListNoEntry, kCount, kPage, ListKeyMove::kNone) == kListNoEntry);
  // Une page d'au moins une ligne ; une liste vide ne mène nulle part.
  CHECK(ListKeyTarget(3, kCount, 0, ListKeyMove::kPageDown) == 4);
  CHECK(ListKeyTarget(0, 0, kPage, ListKeyMove::kNext) == kListNoEntry);
}

TEST_CASE("Clavier des listes : une entrée qui ne se choisit pas se saute", "[ui][list_keys]") {
  constexpr int kCount = 10;
  constexpr int kPage = 4;
  // Se choisissent : 1, 2, 5, 6, 8. Ni les deux bouts, ni 3, 4, 7, 9.
  const std::function<bool(int)> open = [](int rank) {
    return rank == 1 || rank == 2 || rank == 5 || rank == 6 || rank == 8;
  };
  // Le voisin qui ne se choisit pas cède au suivant dans le sens de la touche.
  CHECK(ListKeyTarget(2, kCount, kPage, ListKeyMove::kNext, open) == 5);
  CHECK(ListKeyTarget(5, kCount, kPage, ListKeyMove::kPrevious, open) == 2);
  // Au bout : plus rien dans ce sens, elle reste où elle est.
  CHECK(ListKeyTarget(8, kCount, kPage, ListKeyMove::kNext, open) == 8);
  CHECK(ListKeyTarget(1, kCount, kPage, ListKeyMove::kPrevious, open) == 1);
  // Une page qui tombe sur une entrée fermée va au-delà, ou revient en deçà.
  CHECK(ListKeyTarget(1, kCount, kPage, ListKeyMove::kPageDown, open) == 5);
  CHECK(ListKeyTarget(6, kCount, kPage, ListKeyMove::kPageDown, open) == 8);
  CHECK(ListKeyTarget(8, kCount, kPage, ListKeyMove::kPageUp, open) == 2);
  CHECK(ListKeyTarget(5, kCount, kPage, ListKeyMove::kPageUp, open) == 1);
  // Le début et la fin : la première et la dernière qui se choisissent.
  CHECK(ListKeyTarget(5, kCount, kPage, ListKeyMove::kFirst, open) == 1);
  CHECK(ListKeyTarget(5, kCount, kPage, ListKeyMove::kLast, open) == 8);
  // Sans entrée choisie, la flèche du bas mène à la première qui se choisit.
  CHECK(ListKeyTarget(kListNoEntry, kCount, kPage, ListKeyMove::kNext, open) == 1);
  CHECK(ListKeyTarget(kListNoEntry, kCount, kPage, ListKeyMove::kPrevious, open) == 8);
  // Sans touche, rien ne bouge, même sur une entrée fermée.
  CHECK(ListKeyTarget(3, kCount, kPage, ListKeyMove::kNone, open) == 3);
  // Aucune entrée ne se choisit : nulle part.
  const std::function<bool(int)> none = [](int) { return false; };
  CHECK(ListKeyTarget(0, kCount, kPage, ListKeyMove::kNext, none) == kListNoEntry);
  // Sans filtre, le rang d'origine.
  CHECK(ListKeyTarget(3, kCount, kPage, ListKeyMove::kPageDown, {}) == 7);
}

TEST_CASE("Clavier des listes : le fil retient d'où le clavier est parti", "[ui][list_keys]") {
  ro::ui::DatabaseHistory history;
  history.Visit(ItemSheet(0));
  history.Step(ItemSheet(1));
  history.Step(ItemSheet(2));
  history.Step(ItemSheet(3));
  CHECK(history.current() == ItemSheet(3));
  // Les pas du clavier se remplacent : ◀ ramène au départ.
  REQUIRE(history.Back());
  CHECK(history.current() == ItemSheet(0));
  CHECK_FALSE(history.can_go_back());
  // Après ◀, le clavier repart d'une fiche nouvelle.
  history.Step(ItemSheet(5));
  REQUIRE(history.Back());
  CHECK(history.current() == ItemSheet(0));
  // Une visite à la souris coupe la suite du clavier.
  REQUIRE(history.Forward());
  history.Visit(ItemSheet(7));
  history.Step(ItemSheet(8));
  REQUIRE(history.Back());
  CHECK(history.current() == ItemSheet(7));
}

TEST_CASE("Clavier des listes : la liste de la base se parcourt, la fiche suit",
          "[ui][list_keys]") {
  ListProbe probe;
  probe.Settle();
  probe.Press(ImGuiKey_DownArrow);
  CHECK(probe.shown() == ItemSheet(1).id);
  probe.Press(ImGuiKey_UpArrow);
  CHECK(probe.shown() == ItemSheet(0).id);

  // La fin : la dernière ligne, que la liste a rejointe.
  probe.Press(ImGuiKey_End);
  CHECK(probe.shown() == ItemSheet(kRowCount - 1).id);
  const ImGuiWindow* const rows = WindowNamed("##db_rows");
  REQUIRE(rows != nullptr);
  CHECK(rows->ScrollMax.y > 0.0F);
  CHECK(rows->Scroll.y == rows->ScrollMax.y);

  // Le début : la première, et la liste remonte.
  probe.Press(ImGuiKey_Home);
  CHECK(probe.shown() == ItemSheet(0).id);
  CHECK(rows->Scroll.y == 0.0F);

  // Une page : plus d'une ligne, moins que la liste entière ; la page d'avant
  // ramène au même point.
  probe.Press(ImGuiKey_PageDown);
  const std::uint32_t paged = probe.shown() - kFirstId;
  CHECK(paged > 1);
  CHECK(paged < kRowCount - 1);
  probe.Press(ImGuiKey_PageUp);
  CHECK(probe.shown() == ItemSheet(0).id);

  // Le fil ne garde qu'une fiche pour tout ce parcours.
  CHECK_FALSE(probe.state.history.can_go_back());
}

TEST_CASE("Clavier des listes : la liste de la base porte l'ascenseur du jeu", "[ui][list_keys]") {
  ListProbe probe;
  probe.Settle();
  const ImGuiWindow* const rows = WindowNamed("##db_rows");
  REQUIRE(rows != nullptr);
  REQUIRE(rows->ScrollbarY);
  CHECK(PaintsSkin(rows));
}

TEST_CASE("Clavier des listes : une liste déroulante ouverte se parcourt aux touches",
          "[ui][list_keys][combo]") {
  ro::ui::UiContext ui;
  // Sans art : le champ a la hauteur d'un champ d'ImGui, où vise la souris.
  const ro::ui::SkinArt art;
  constexpr auto kItems = std::to_array<const char*>({"un", "deux", "trois", "quatre", "cinq"});
  int chosen = 0;
  bool changed = false;
  const auto frame = [&] {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kViewport.width_px, kViewport.height_px));
    ImGui::Begin("essai");
    changed = ro::ui::SkinCombo("##liste", &chosen, kItems, art) || changed;
    ImGui::End();
    ui.EndFrame();
  };
  const auto press = [&](ImGuiKey key) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(key, true);
    frame();
    io.AddKeyEvent(key, false);
    frame();
  };
  frame();
  // Fermée, la liste ne prend pas les touches.
  press(ImGuiKey_DownArrow);
  CHECK(chosen == 0);
  CHECK_FALSE(changed);

  // Un clic sur le champ la déroule.
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(kOnTheFieldX, kOnTheFieldY);
  frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  frame();
  frame();
  REQUIRE(ImGui::GetCurrentContext()->OpenPopupStack.Size > 0);
  press(ImGuiKey_DownArrow);
  CHECK(chosen == 1);
  CHECK(changed);
  press(ImGuiKey_End);
  CHECK(chosen == 4);
  press(ImGuiKey_Home);
  CHECK(chosen == 0);
  press(ImGuiKey_PageDown);
  CHECK(chosen > 0);
  // Entrée referme.
  press(ImGuiKey_Enter);
  CHECK(ImGui::GetCurrentContext()->OpenPopupStack.Size == 0);
}

namespace {

// UNE LISTE DÉROULANTE REMPLIE À LA MAIN, comme les fenêtres le font : un
// intitulé de groupe, puis cinq entrées dont la troisième est grisée.
struct HandComboProbe {
  static constexpr int kEntries = 5;
  static constexpr int kGreyed = 2;

  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ro::ui::ComboKeyChoice choice = ro::ui::ComboKeyChoice::kFollowsKey;
  int chosen = 0;
  int taken = 0;

  void Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kViewport.width_px, kViewport.height_px));
    ImGui::Begin("essai");
    if (ro::ui::BeginSkinCombo("##main", "?", art)) {
      ImGui::TextDisabled("groupe");
      ro::ui::ComboKeys keys(chosen, kEntries, [](int rank) { return rank != kGreyed; }, choice);
      for (int rank = 0; rank < kEntries; ++rank) {
        ImGui::PushID(rank);
        ImGui::BeginDisabled(rank == kGreyed);
        if (keys.Took(rank, ImGui::Selectable("entrée", rank == chosen))) {
          chosen = rank;
          ++taken;
        }
        ImGui::EndDisabled();
        ImGui::PopID();
      }
      ro::ui::EndSkinCombo();
    }
    ImGui::End();
    ui.EndFrame();
  }

  void Press(ImGuiKey key) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(key, true);
    Frame();
    io.AddKeyEvent(key, false);
    Frame();
  }

  void Open() {
    Frame();
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(kOnTheFieldX, kOnTheFieldY);
    Frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    Frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    Frame();
    Frame();
  }

  [[nodiscard]] static bool open() { return ImGui::GetCurrentContext()->OpenPopupStack.Size > 0; }
};

}  // namespace

TEST_CASE("Clavier des listes : une liste déroulante remplie à la main prend les touches",
          "[ui][list_keys][combo]") {
  HandComboProbe probe;
  probe.Frame();
  // Fermée, rien.
  probe.Press(ImGuiKey_DownArrow);
  CHECK(probe.chosen == 0);
  CHECK(probe.taken == 0);

  probe.Open();
  REQUIRE(HandComboProbe::open());
  // Chaque pas est une prise, comme un clic.
  probe.Press(ImGuiKey_DownArrow);
  CHECK(probe.chosen == 1);
  CHECK(probe.taken == 1);
  // L'entrée grisée se saute, à l'aller comme au retour.
  probe.Press(ImGuiKey_DownArrow);
  CHECK(probe.chosen == 3);
  probe.Press(ImGuiKey_UpArrow);
  CHECK(probe.chosen == 1);
  probe.Press(ImGuiKey_End);
  CHECK(probe.chosen == 4);
  // En butée, pas de prise.
  const int before_stop = probe.taken;
  probe.Press(ImGuiKey_DownArrow);
  CHECK(probe.chosen == 4);
  CHECK(probe.taken == before_stop);
  probe.Press(ImGuiKey_Home);
  CHECK(probe.chosen == 0);
  probe.Press(ImGuiKey_PageDown);
  CHECK(probe.chosen > 0);
  CHECK(probe.chosen != HandComboProbe::kGreyed);
  // Entrée referme, sans prendre.
  const int before_enter = probe.taken;
  probe.Press(ImGuiKey_Enter);
  CHECK_FALSE(HandComboProbe::open());
  CHECK(probe.taken == before_enter);
}

TEST_CASE("Clavier des listes : sous « Entrée prend », la touche ne fait que désigner",
          "[ui][list_keys][combo]") {
  HandComboProbe probe;
  probe.choice = ro::ui::ComboKeyChoice::kOnEnter;
  probe.Open();
  REQUIRE(HandComboProbe::open());
  probe.Press(ImGuiKey_DownArrow);
  probe.Press(ImGuiKey_DownArrow);
  // Le curseur a passé l'entrée grisée ; rien n'est pris.
  CHECK(probe.chosen == 0);
  CHECK(probe.taken == 0);
  probe.Press(ImGuiKey_Enter);
  CHECK(probe.chosen == 3);
  CHECK(probe.taken == 1);
  CHECK_FALSE(HandComboProbe::open());

  // Rouverte, le curseur repart de l'entrée choisie.
  probe.Open();
  REQUIRE(HandComboProbe::open());
  probe.Press(ImGuiKey_UpArrow);
  probe.Press(ImGuiKey_Enter);
  CHECK(probe.chosen == 1);
  CHECK(probe.taken == 2);
}
