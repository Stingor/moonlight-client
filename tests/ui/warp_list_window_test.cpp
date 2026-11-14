// LA FENÊTRE DES DESTINATIONS : sa disposition, et ce qu'elle rend au clic.
//
// 🔴 LES MESURES SONT CELLES DU CLIENT, et l'essai les relit sur la fenêtre
// dessinée, non sur les constantes : une liste posée ailleurs que sous la ligne
// de titre, ou des boutons décalés, se verraient d'abord ici.
//
// ⚠ ET LE CLIC DANS LE VIDE COMPTE : sous la dernière ligne, il désélectionne,
// et « OK » ne part plus. C'est le comportement du client, et il n'a rien
// d'évident.

#include "ro/ui/warp_list_window.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::kWarpListNoRow;
using ro::ui::WarpListRowAt;
using ro::ui::WarpListStep;
using ro::ui::WarpListVerb;
using ro::ui::WarpListView;
using ro::ui::WarpListWindowAction;
using ro::ui::WarpListWindowState;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

// Une question ouverte sur deux cartes, l'annulation en dernier.
struct WarpProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  WarpListWindowState state;
  std::vector<std::string> rows = {"Prontera", "Geffen", "Cancel"};
  WarpListView view{.title = "Select an Area to Warp", .rows = rows, .offer_seq = 1};

  WarpProbe() { state.open = true; }
  explicit WarpProbe(std::vector<std::string> shown) : rows(std::move(shown)) {
    view.rows = rows;
    state.open = true;
  }

  WarpListWindowAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const WarpListWindowAction action = ro::ui::DrawWarpListWindow(state, view, art);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < 4; ++frame) {
      static_cast<void>(Frame());
    }
  }
};

[[nodiscard]] ImGuiWindow* Window() {
  ImGuiWindow* window = ImGui::FindWindowByName("###destinations");
  REQUIRE(window != nullptr);
  return window;
}

[[nodiscard]] ImGuiWindow* Rows() {
  ImGuiWindow* window = Window();
  REQUIRE(window->DC.ChildWindows.Size == 1);
  return window->DC.ChildWindows[0];
}

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

// Les images des deux boutons, chacune sous son propre identifiant.
constexpr ImTextureID kOkOutTexture = 11;
constexpr ImTextureID kOkOverTexture = 12;
constexpr ImTextureID kOkDownTexture = 13;
constexpr ImTextureID kCancelOutTexture = 21;
constexpr ImTextureID kCancelOverTexture = 22;
constexpr ImTextureID kCancelDownTexture = 23;
constexpr ImVec2 kButtonBitmapPixels = ImVec2(42.0F, 20.0F);

void SetButtonArt(ro::ui::SkinArt& art) {
  const auto set = [&art](SkinPiece piece, ImTextureID id) {
    art.Set(piece, ro::ui::SkinImage{
                       .id = id, .width = kButtonBitmapPixels.x, .height = kButtonBitmapPixels.y});
  };
  set(SkinPiece::kOkButtonOut, kOkOutTexture);
  set(SkinPiece::kOkButtonOver, kOkOverTexture);
  set(SkinPiece::kOkButtonDown, kOkDownTexture);
  set(SkinPiece::kCancelButtonOut, kCancelOutTexture);
  set(SkinPiece::kCancelButtonOver, kCancelOverTexture);
  set(SkinPiece::kCancelButtonDown, kCancelDownTexture);
}

// Les coins du rectangle couvert par une image posée par identifiant, ou rien.
//
// ⚠ PAS `GetTexID()` : il assertionne sur la texture de la police, qu'aucun
// moteur n'a chargée dans un banc.
[[nodiscard]] bool ImageBounds(const ImGuiWindow& window, ImTextureID id, ImVec2& low,
                               ImVec2& high) {
  bool found = false;
  const ImDrawList& list = *window.DrawList;
  for (const ImDrawCmd& command : list.CmdBuffer) {
    if (command.TexRef._TexData != nullptr || command.TexRef._TexID != id) {
      continue;
    }
    for (unsigned rank = 0; rank < command.ElemCount; ++rank) {
      const ImDrawVert& vertex =
          list.VtxBuffer[static_cast<int>(command.VtxOffset) +
                         list.IdxBuffer[static_cast<int>(command.IdxOffset + rank)]];
      if (!found) {
        low = vertex.pos;
        high = vertex.pos;
        found = true;
      }
      low = ImVec2(std::min(low.x, vertex.pos.x), std::min(low.y, vertex.pos.y));
      high = ImVec2(std::max(high.x, vertex.pos.x), std::max(high.y, vertex.pos.y));
    }
  }
  return found;
}

// Un sommet d'aplat — dessiné sur la texture de la police — de cette couleur, à
// ce point exact.
[[nodiscard]] bool FlatVertexAt(const ImGuiWindow& window, ImU32 color, ImVec2 at) {
  const ImDrawList& list = *window.DrawList;
  for (const ImDrawCmd& command : list.CmdBuffer) {
    if (command.TexRef._TexData == nullptr) {
      continue;
    }
    for (unsigned rank = 0; rank < command.ElemCount; ++rank) {
      const ImDrawVert& vertex =
          list.VtxBuffer[static_cast<int>(command.VtxOffset) +
                         list.IdxBuffer[static_cast<int>(command.IdxOffset + rank)]];
      if (vertex.col == color && vertex.pos.x == at.x && vertex.pos.y == at.y) {
        return true;
      }
    }
  }
  return false;
}

// Un clic complet — enfoncé puis relâché — au point donné.
WarpListWindowAction ClickAt(WarpProbe& probe, ImVec2 at) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(at.x, at.y);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  WarpListWindowAction pressed = probe.Frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  const WarpListWindowAction released = probe.Frame();
  return pressed.verb != WarpListVerb::kNothing ? pressed : released;
}

// Le milieu d'une ligne de la liste.
[[nodiscard]] ImVec2 RowCenter(int row) {
  const ImGuiWindow* rows = Rows();
  const float height = ro::ui::kWarpListRowPixels * Scale();
  return ImVec2(rows->Pos.x + rows->Size.x * 0.5F,
                rows->Pos.y + (static_cast<float>(row) + 0.5F) * height);
}

// Le milieu d'un bouton, compté depuis le coin bas-droit.
[[nodiscard]] ImVec2 ButtonCenter(float from_right) {
  const ImGuiWindow* window = Window();
  const float scale = Scale();
  return ImVec2(window->Pos.x + window->Size.x - from_right * scale +
                    ro::ui::kWarpListButtonPixels.x * scale * 0.5F,
                window->Pos.y + window->Size.y - ro::ui::kWarpListButtonsFromBottomPixels * scale +
                    ro::ui::kWarpListButtonPixels.y * scale * 0.5F);
}

}  // namespace

TEST_CASE("Destinations : la ligne sous un point, et les flèches", "[ui][destinations]") {
  CHECK(WarpListRowAt(0.0F, 16.0F, 3) == 0);
  CHECK(WarpListRowAt(31.0F, 16.0F, 3) == 1);
  CHECK(WarpListRowAt(47.9F, 16.0F, 3) == 2);
  // Sous la dernière ligne : aucune.
  CHECK(WarpListRowAt(48.0F, 16.0F, 3) == kWarpListNoRow);
  CHECK(WarpListRowAt(-1.0F, 16.0F, 3) == kWarpListNoRow);

  CHECK(WarpListStep(0, -1, 3) == 0);
  CHECK(WarpListStep(2, 1, 3) == 2);
  CHECK(WarpListStep(1, 1, 3) == 2);
  // Sans ligne choisie, les deux flèches reprennent à la première.
  CHECK(WarpListStep(kWarpListNoRow, -1, 3) == 0);
  CHECK(WarpListStep(kWarpListNoRow, 1, 3) == 0);
}

TEST_CASE("Destinations : fermée, la fenêtre ne dessine rien", "[ui][destinations]") {
  WarpProbe probe;
  probe.state.open = false;
  CHECK(probe.Frame().verb == WarpListVerb::kNothing);
  CHECK(ImGui::FindWindowByName("###destinations") == nullptr);
}

TEST_CASE("Destinations : la fenêtre a les mesures du client", "[ui][destinations]") {
  WarpProbe probe;
  probe.Settle();
  const float scale = Scale();
  const ImGuiWindow* window = Window();
  CHECK(window->Size.x == ro::ui::kWarpListSizePixels.x * scale);
  CHECK(window->Size.y == ro::ui::kWarpListSizePixels.y * scale);
  // La liste : douze pixels du bord gauche, vingt-six du haut, et ce que la
  // fenêtre lui laisse — 256 sur 65.
  const ImGuiWindow* rows = Rows();
  CHECK(rows->Pos.x - window->Pos.x == 12.0F * scale);
  CHECK(rows->Pos.y - window->Pos.y == 26.0F * scale);
  CHECK(rows->Size.x == 256.0F * scale);
  CHECK(rows->Size.y == 65.0F * scale);
  // La première ligne est choisie d'emblée.
  CHECK(probe.state.selected == 0);
}

TEST_CASE("Destinations : un clic choisit, « OK » part vers la ligne choisie",
          "[ui][destinations]") {
  WarpProbe probe;
  probe.Settle();
  static_cast<void>(ClickAt(probe, RowCenter(1)));
  CHECK(probe.state.selected == 1);
  const WarpListWindowAction ok = ClickAt(probe, ButtonCenter(ro::ui::kWarpListOkFromRightPixels));
  REQUIRE(ok.verb == WarpListVerb::kChoose);
  CHECK(ok.row == 1);
}

TEST_CASE("Destinations : un clic sous la dernière ligne désélectionne, et « OK » se tait",
          "[ui][destinations]") {
  WarpProbe probe;
  probe.Settle();
  // Trois lignes de seize pixels dans une liste de soixante-cinq : la
  // quatrième bande est vide.
  static_cast<void>(ClickAt(probe, RowCenter(3)));
  CHECK(probe.state.selected == kWarpListNoRow);
  const WarpListWindowAction ok = ClickAt(probe, ButtonCenter(ro::ui::kWarpListOkFromRightPixels));
  CHECK(ok.verb == WarpListVerb::kNothing);
}

TEST_CASE("Destinations : un double-clic sur une ligne vaut « OK »", "[ui][destinations]") {
  WarpProbe probe;
  probe.Settle();
  static_cast<void>(ClickAt(probe, RowCenter(2)));
  // LE SECOND CLIC, dans le délai : ImGui rend le double-clic à l'ENFONCEMENT.
  ImGuiIO& io = ImGui::GetIO();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  const WarpListWindowAction pressed = probe.Frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  static_cast<void>(probe.Frame());
  REQUIRE(pressed.verb == WarpListVerb::kChoose);
  // La dernière ligne est l'annulation : c'est à `app` d'en faire le mot.
  CHECK(pressed.row == 2);
}

TEST_CASE("Destinations : « Annuler » rend l'annulation", "[ui][destinations]") {
  WarpProbe probe;
  probe.Settle();
  const WarpListWindowAction cancel =
      ClickAt(probe, ButtonCenter(ro::ui::kWarpListCancelFromRightPixels));
  CHECK(cancel.verb == WarpListVerb::kCancel);
}

TEST_CASE("Destinations : une liste nouvelle remet la sélection en tête", "[ui][destinations]") {
  WarpProbe probe;
  probe.Settle();
  probe.state.selected = 2;
  static_cast<void>(probe.Frame());
  CHECK(probe.state.selected == 2);
  probe.view.offer_seq = 2;
  static_cast<void>(probe.Frame());
  CHECK(probe.state.selected == 0);
}

TEST_CASE("Destinations : les coins du cadre restent arrondis", "[ui][destinations]") {
  // 🔴 LE CORPS BLANC S'ARRÊTE AU BORD INTÉRIEUR DES PIÈCES. Étendu jusqu'au bord
  // de la fenêtre, il remplissait l'extérieur transparent des coins, et la
  // fenêtre avait des angles droits là où le client les arrondit.
  WarpProbe probe;
  probe.Settle();
  const ImGuiWindow* window = Window();
  const float edge = kPlainPiecePixels * Scale();
  constexpr ImU32 kBody = IM_COL32(255, 255, 255, 255);
  CHECK_FALSE(FlatVertexAt(*window, kBody, window->Pos));
  CHECK(FlatVertexAt(*window, kBody, ImVec2(window->Pos.x + edge, window->Pos.y + edge)));
}

TEST_CASE("Destinations : les couleurs de la liste sont celles de l'écran du client",
          "[ui][destinations]") {
  // Un canal ne garde que ses cinq bits de poids fort, étendus par recopie.
  CHECK(ro::ui::ShownOnFiveBits(240) == 247);
  CHECK(ro::ui::ShownOnFiveBits(202) == 206);
  CHECK(ro::ui::ShownOnFiveBits(223) == 222);
  CHECK(ro::ui::ShownOnFiveBits(251) == 255);
  CHECK(ro::ui::ShownOnFiveBits(0) == 0);
  CHECK(ro::ui::ShownOnFiveBits(255) == 255);
  CHECK(ro::ui::kWarpListRowsColor == IM_COL32(247, 247, 247, 255));
  CHECK(ro::ui::kWarpListSelectionColor == IM_COL32(206, 222, 255, 255));
  CHECK(ro::ui::kWarpListTextColor == IM_COL32(0, 0, 0, 255));
}

TEST_CASE("Destinations : « OK » et « cancel » sont les images du client, à leur taille",
          "[ui][destinations]") {
  // 🔴 LE MOT EST DANS L'IMAGE. Un bouton habillé à libellé écrivait le sien en
  // blanc sur un fond presque blanc : deux boutons vides à l'écran.
  WarpProbe probe;
  SetButtonArt(probe.art);
  probe.Settle();
  const ImGuiWindow* window = Window();
  const float scale = Scale();
  const float top =
      window->Pos.y + window->Size.y - ro::ui::kWarpListButtonsFromBottomPixels * scale;

  ImVec2 low;
  ImVec2 high;
  REQUIRE(ImageBounds(*window, kOkOutTexture, low, high));
  CHECK(low.x == window->Pos.x + window->Size.x - ro::ui::kWarpListOkFromRightPixels * scale);
  CHECK(low.y == top);
  CHECK(high.x - low.x == kButtonBitmapPixels.x * scale);
  CHECK(high.y - low.y == kButtonBitmapPixels.y * scale);
  REQUIRE(ImageBounds(*window, kCancelOutTexture, low, high));
  CHECK(low.x == window->Pos.x + window->Size.x - ro::ui::kWarpListCancelFromRightPixels * scale);
  CHECK(low.y == top);
}

TEST_CASE("Destinations : un bouton-image se montre survolé, puis enfoncé", "[ui][destinations]") {
  WarpProbe probe;
  SetButtonArt(probe.art);
  probe.Settle();
  ImVec2 low;
  ImVec2 high;
  ImGuiIO& io = ImGui::GetIO();
  const ImVec2 ok = ButtonCenter(ro::ui::kWarpListOkFromRightPixels);
  io.AddMousePosEvent(ok.x, ok.y);
  static_cast<void>(probe.Frame());
  static_cast<void>(probe.Frame());
  CHECK(ImageBounds(*Window(), kOkOverTexture, low, high));
  CHECK_FALSE(ImageBounds(*Window(), kOkOutTexture, low, high));
  // L'autre bouton, non survolé, reste au repos.
  CHECK(ImageBounds(*Window(), kCancelOutTexture, low, high));

  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  static_cast<void>(probe.Frame());
  static_cast<void>(probe.Frame());
  CHECK(ImageBounds(*Window(), kOkDownTexture, low, high));
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  static_cast<void>(probe.Frame());
}

namespace {

// Les pièces de la barre, chacune sous son identifiant et à la taille des
// bitmaps du client : treize pixels de côté pour la piste et les flèches,
// quatre de haut pour les carreaux de la poignée.
constexpr ImTextureID kScrollUpTexture = 31;
constexpr ImTextureID kScrollMiddleTexture = 32;
constexpr ImTextureID kScrollDownTexture = 33;
constexpr ImTextureID kGrabTopTexture = 34;
constexpr ImTextureID kGrabMiddleTexture = 35;
constexpr ImTextureID kGrabBottomTexture = 36;
constexpr float kScrollPiecePixels = 13.0F;
constexpr float kGrabTilePixels = 4.0F;

void SetScrollArt(ro::ui::SkinArt& art) {
  const auto set = [&art](SkinPiece piece, ImTextureID id, float height) {
    art.Set(piece, ro::ui::SkinImage{.id = id, .width = kScrollPiecePixels, .height = height});
  };
  set(SkinPiece::kScrollUp, kScrollUpTexture, kScrollPiecePixels);
  set(SkinPiece::kScrollMiddle, kScrollMiddleTexture, kScrollPiecePixels);
  set(SkinPiece::kScrollDown, kScrollDownTexture, kScrollPiecePixels);
  set(SkinPiece::kScrollGrabTop, kGrabTopTexture, kGrabTilePixels);
  set(SkinPiece::kScrollGrabMiddle, kGrabMiddleTexture, kGrabTilePixels);
  set(SkinPiece::kScrollGrabBottom, kGrabBottomTexture, kGrabTilePixels);
}

// La liste d'un Warp Portal de niveau 4 : le point de sauvegarde, trois mémos,
// et l'annulation que le client ajoute — cinq lignes pour quatre places.
[[nodiscard]] std::vector<std::string> FiveRows() {
  return {"Random Area", "Port City Alberta", "Gonryun, the Hermit Land (Kunlun)", "Random Area",
          "Cancel"};
}

// Le milieu d'un point de la barre, `y_pixels` sous son haut.
[[nodiscard]] ImVec2 BarPoint(float y_pixels) {
  const ImGuiWindow* rows = Rows();
  const float scale = Scale();
  return ImVec2(
      rows->Pos.x + rows->Size.x - static_cast<float>(ro::ui::kWarpListBarPixels) * scale * 0.5F,
      rows->Pos.y + y_pixels * scale);
}

}  // namespace

TEST_CASE("Destinations : la liste montre quatre lignes entières, et sa poignée",
          "[ui][destinations]") {
  using ro::ui::WarpListBarZone;
  // 65 pixels de haut, des lignes de 16 : quatre lignes.
  CHECK(ro::ui::WarpListVisibleRows(65) == 4);
  CHECK(ro::ui::WarpListVisibleRows(64) == 4);
  CHECK(ro::ui::WarpListVisibleRows(63) == 3);

  // Tout tient : pas de poignée, et rien ne défile.
  CHECK(ro::ui::WarpListGrabOf(65, 4, 4, 0).length == 0);
  CHECK(ro::ui::WarpListScrolled(0, 1, 4, 4) == 0);
  // Une ligne cachée : la course de la piste est de 39 pixels, la poignée en
  // prend quatre cinquièmes, tronqués — 31 —, et descend de 8 pour une ligne.
  const ro::ui::WarpListGrab top = ro::ui::WarpListGrabOf(65, 5, 4, 0);
  CHECK(top.top == 13);
  CHECK(top.length == 31);
  const ro::ui::WarpListGrab bottom = ro::ui::WarpListGrabOf(65, 5, 4, 1);
  CHECK(bottom.top == 21);
  CHECK(bottom.length == 31);
  // Beaucoup de lignes : la poignée ne descend pas sous huit pixels.
  CHECK(ro::ui::WarpListGrabOf(65, 100, 4, 0).length == 8);

  // Le défilement ne passe ni avant la première ligne, ni au-delà du vide.
  CHECK(ro::ui::WarpListScrolled(0, -1, 5, 4) == 0);
  CHECK(ro::ui::WarpListScrolled(0, 1, 5, 4) == 1);
  CHECK(ro::ui::WarpListScrolled(1, 1, 5, 4) == 1);
  CHECK(ro::ui::WarpListScrolled(0, 3, 10, 4) == 3);

  // Les zones, de haut en bas, poignée en tête de piste.
  CHECK(ro::ui::WarpListBarZoneAt(0, 65, top) == WarpListBarZone::kUpArrow);
  CHECK(ro::ui::WarpListBarZoneAt(12, 65, top) == WarpListBarZone::kUpArrow);
  CHECK(ro::ui::WarpListBarZoneAt(13, 65, top) == WarpListBarZone::kGrab);
  CHECK(ro::ui::WarpListBarZoneAt(43, 65, top) == WarpListBarZone::kGrab);
  CHECK(ro::ui::WarpListBarZoneAt(44, 65, top) == WarpListBarZone::kPageDown);
  CHECK(ro::ui::WarpListBarZoneAt(52, 65, top) == WarpListBarZone::kDownArrow);
  CHECK(ro::ui::WarpListBarZoneAt(15, 65, bottom) == WarpListBarZone::kPageUp);

  // Le glisser : une course de 8 pixels couvre deux lignes, et tronque.
  CHECK(ro::ui::WarpListDraggedFirstRow(0, 3, 65, 5, 4) == 0);
  CHECK(ro::ui::WarpListDraggedFirstRow(0, 4, 65, 5, 4) == 1);
  CHECK(ro::ui::WarpListDraggedFirstRow(1, -4, 65, 5, 4) == 0);
  CHECK(ro::ui::WarpListDraggedFirstRow(0, 40, 65, 5, 4) == 1);
}

TEST_CASE("Destinations : cinq lignes, la barre du client et non celle d'ImGui",
          "[ui][destinations]") {
  // 🔴 LA BARRE GRISE D'IMGUI N'EST PAS CELLE DU CLIENT. La liste débordait
  // d'une ligne — l'annulation —, et ImGui y posait la sienne.
  WarpProbe probe(FiveRows());
  SetScrollArt(probe.art);
  probe.Settle();
  const ImGuiWindow* rows = Rows();
  const float scale = Scale();
  CHECK_FALSE(rows->ScrollbarY);
  CHECK(rows->Size.y == 65.0F * scale);

  // La barre : treize pixels contre le bord droit de la liste, la flèche du haut
  // un pixel sous le bord, celle du bas un pixel au-dessus de sa place.
  const float bar_left = rows->Pos.x + rows->Size.x - 13.0F * scale;
  ImVec2 low;
  ImVec2 high;
  REQUIRE(ImageBounds(*rows, kScrollUpTexture, low, high));
  CHECK(low.x == bar_left);
  CHECK(low.y == rows->Pos.y + 1.0F * scale);
  REQUIRE(ImageBounds(*rows, kScrollDownTexture, low, high));
  CHECK(low.y == rows->Pos.y + 51.0F * scale);
  CHECK(high.y == rows->Pos.y + 64.0F * scale);
  REQUIRE(ImageBounds(*rows, kScrollMiddleTexture, low, high));
  CHECK(low.y == rows->Pos.y + 13.0F * scale);
  CHECK(high.y == rows->Pos.y + 52.0F * scale);
  // La poignée : 31 pixels, en tête de piste.
  REQUIRE(ImageBounds(*rows, kGrabTopTexture, low, high));
  CHECK(low.y == rows->Pos.y + 13.0F * scale);
  REQUIRE(ImageBounds(*rows, kGrabBottomTexture, low, high));
  CHECK(high.y == rows->Pos.y + 44.0F * scale);
  CHECK(ImageBounds(*rows, kGrabMiddleTexture, low, high));
}

TEST_CASE("Destinations : sans ligne cachée, pas de barre", "[ui][destinations]") {
  WarpProbe probe;
  SetScrollArt(probe.art);
  probe.Settle();
  const ImGuiWindow* rows = Rows();
  ImVec2 low;
  ImVec2 high;
  CHECK_FALSE(rows->ScrollbarY);
  CHECK_FALSE(ImageBounds(*rows, kScrollUpTexture, low, high));
  CHECK_FALSE(ImageBounds(*rows, kGrabTopTexture, low, high));
}

TEST_CASE("Destinations : la flèche du bas montre l'annulation", "[ui][destinations]") {
  WarpProbe probe(FiveRows());
  SetScrollArt(probe.art);
  probe.Settle();
  CHECK(probe.state.first_row == 0);
  // La cinquième ligne est cachée : la quatrième place montre la quatrième.
  static_cast<void>(ClickAt(probe, RowCenter(3)));
  CHECK(probe.state.selected == 3);

  static_cast<void>(ClickAt(probe, BarPoint(58.0F)));
  CHECK(probe.state.first_row == 1);
  // Un second clic ne va pas plus loin : il n'y a plus rien dessous.
  static_cast<void>(ClickAt(probe, BarPoint(58.0F)));
  CHECK(probe.state.first_row == 1);
  // La quatrième place montre désormais l'annulation.
  static_cast<void>(ClickAt(probe, RowCenter(3)));
  CHECK(probe.state.selected == 4);
  const WarpListWindowAction ok = ClickAt(probe, ButtonCenter(ro::ui::kWarpListOkFromRightPixels));
  REQUIRE(ok.verb == WarpListVerb::kChoose);
  CHECK(ok.row == 4);

  // La flèche du haut remonte.
  static_cast<void>(ClickAt(probe, BarPoint(5.0F)));
  CHECK(probe.state.first_row == 0);
}

TEST_CASE("Destinations : la flèche du clavier fait suivre la liste", "[ui][destinations]") {
  WarpProbe probe(FiveRows());
  probe.Settle();
  probe.state.selected = 3;
  ImGuiIO& io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiKey_DownArrow, true);
  static_cast<void>(probe.Frame());
  io.AddKeyEvent(ImGuiKey_DownArrow, false);
  static_cast<void>(probe.Frame());
  CHECK(probe.state.selected == 4);
  CHECK(probe.state.first_row == 1);
  // Une liste nouvelle revient en haut.
  probe.view.offer_seq = 2;
  static_cast<void>(probe.Frame());
  CHECK(probe.state.first_row == 0);
}
