#include "ro/ui/inventory_window.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/ui_context.h"

using ro::ui::GroupThousands;
using ro::ui::InventoryWindowAction;
using ro::ui::ItemBadgeFontSize;
using ro::ui::ItemGridColumns;
using ro::ui::ItemTab;
using ro::ui::ItemTabLabel;
using ro::ui::ItemTabPiece;
using ro::ui::kItemBadgeMinimumScale;
using ro::ui::kItemCellPixels;
using ro::ui::kItemTabCount;

namespace {

// Les cases sont jointives : c'est le dessin du client.
constexpr float kNoGap = 0.0F;

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
// Les mesures du client : une barre de titre de dix-sept pixels, l'art d'un
// petit bouton de quatorze, et ses bouts de six de large.
constexpr float kTitleBarPixels = 17.0F;
constexpr float kTitleBarCapPixels = 12.0F;
constexpr float kSmallButtonPixels = 14.0F;
constexpr float kSmallCapPixels = 6.0F;
constexpr float kPlainPiecePixels = 8.0F;
// Le pas du balayage de la barre : plus fin que le plus étroit des boutons.
constexpr float kScanStepPixels = 2.0F;
// Le libellé du bouton du staff, non traduit : aucun catalogue n'est chargé.
constexpr const char* kWipeLabel = "Vider";

// Un habillage complet, aux tailles du client, pour que la barre de titre et
// ses boutons existent. Rien n'est peint : on ne lit que la disposition.
[[nodiscard]] ro::ui::SkinArt WholeArt() {
  using ro::render::SkinPiece;
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  for (const SkinPiece piece :
       {SkinPiece::kTitleBarLeft, SkinPiece::kTitleBarMiddle, SkinPiece::kTitleBarRight}) {
    art.Set(piece,
            ro::ui::SkinImage{.id = 1, .width = kTitleBarCapPixels, .height = kTitleBarPixels});
  }
  for (const SkinPiece piece :
       {SkinPiece::kSmallButtonLeft, SkinPiece::kSmallButtonMiddle, SkinPiece::kSmallButtonRight,
        SkinPiece::kSmallButtonHoverLeft, SkinPiece::kSmallButtonHoverMiddle,
        SkinPiece::kSmallButtonHoverRight, SkinPiece::kSmallButtonPressedLeft,
        SkinPiece::kSmallButtonPressedMiddle, SkinPiece::kSmallButtonPressedRight}) {
    art.Set(piece,
            ro::ui::SkinImage{.id = 1, .width = kSmallCapPixels, .height = kSmallButtonPixels});
  }
  return art;
}

// Un sac vide, ouvert, dessiné image par image.
struct BagProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = WholeArt();
  ro::ui::InventoryWindowState state;
  ro::ui::InventoryWindowView view;

  BagProbe() { state.open = true; }

  InventoryWindowAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    InventoryWindowAction action = ro::ui::DrawInventoryWindow(
        state, view, std::span<const ro::ui::InventoryCellView>(), ro::ui::InventoryTally{}, art);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < 4; ++frame) {
      static_cast<void>(Frame());
    }
  }
};

// Balaie la barre de titre jusqu'à survoler le bouton « Vider ». Faux s'il n'y
// est pas.
[[nodiscard]] bool HoverWipe(BagProbe& probe) {
  ImGuiWindow* const window = ImGui::FindWindowByName("###inventory");
  REQUIRE(window != nullptr);
  const ImGuiID wipe_id = window->GetID(kWipeLabel);
  const float bar_middle = window->Pos.y + window->TitleBarHeight * 0.5F;
  ImGuiIO& io = ImGui::GetIO();
  for (float x = window->Pos.x; x < window->Pos.x + window->Size.x; x += kScanStepPixels) {
    io.AddMousePosEvent(x, bar_middle);
    static_cast<void>(probe.Frame());
    if (ImGui::GetCurrentContext()->HoveredId == wipe_id) {
      return true;
    }
  }
  return false;
}

// Un clic sur place : ce que rendent l'image de l'enfoncement et celle du
// relâchement, réunies.
[[nodiscard]] InventoryWindowAction Click(BagProbe& probe) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  const InventoryWindowAction pressed = probe.Frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  InventoryWindowAction released = probe.Frame();
  released.staff_wipe = released.staff_wipe || pressed.staff_wipe;
  released.toggle_cart = released.toggle_cart || pressed.toggle_cart;
  released.toggle_storage = released.toggle_storage || pressed.toggle_storage;
  return released;
}

}  // namespace

TEST_CASE("Sac : sans le droit du staff, la barre n'a pas de « Vider »", "[ui][sac][staff]") {
  BagProbe probe;
  REQUIRE(probe.art.dresses_windows());
  probe.state.storage_button = true;
  probe.Settle();
  CHECK_FALSE(HoverWipe(probe));
}

TEST_CASE("Sac : le « Vider » du staff émet l'intention, et rien d'autre", "[ui][sac][staff]") {
  // 🔴 SON RANG SUIT LES RACCOURCIS PRÉSENTS. Sans eux, il prend le rang zéro,
  // celui de « Cart » : lu sans garde, le clic ouvrirait aussi la charrette.
  const bool with_shortcuts = GENERATE(false, true);
  BagProbe probe;
  probe.view.staff_tools = true;
  probe.state.storage_button = with_shortcuts;
  probe.Settle();
  REQUIRE(HoverWipe(probe));
  const InventoryWindowAction clicked = Click(probe);
  CHECK(clicked.staff_wipe);
  CHECK_FALSE(clicked.toggle_cart);
  CHECK_FALSE(clicked.toggle_storage);
}

TEST_CASE("Grille d'objets : les colonnes se CALCULENT, elles ne sont pas fixées", "[ui][sac]") {
  // 🔴 C'EST LA RÈGLE QUE CET ESSAI TIENT. Une largeur de colonnes figée ferait
  // une fenêtre qui ne sert à rien à l'agrandir, ou qui coupe ses cases au bord.
  CHECK(ItemGridColumns(6 * kItemCellPixels, kItemCellPixels, kNoGap) == 6);
  CHECK(ItemGridColumns(10 * kItemCellPixels, kItemCellPixels, kNoGap) == 10);
  // Une case entamée ne compte pas : mieux vaut une bande vide qu'une case
  // tronquée.
  CHECK(ItemGridColumns(6 * kItemCellPixels + 31.0F, kItemCellPixels, kNoGap) == 6);
}

TEST_CASE("Grille d'objets : jamais moins d'une colonne", "[ui][sac]") {
  // Zéro colonne ne dessinerait rien ET diviserait par zéro au placement.
  CHECK(ItemGridColumns(0.0F, kItemCellPixels, kNoGap) == 1);
  CHECK(ItemGridColumns(-100.0F, kItemCellPixels, kNoGap) == 1);
  // Et une case de côté nul ne doit pas faire boucler l'appelant à l'infini.
  CHECK(ItemGridColumns(200.0F, 0.0F, kNoGap) == 1);
}

TEST_CASE("Grille d'objets : un écart entre cases se compte une fois de moins", "[ui][sac]") {
  // La grille n'en emploie pas, mais la formule doit rester juste : N cases
  // portent N-1 écarts, pas N.
  constexpr float kGap = 2.0F;
  CHECK(ItemGridColumns(4 * kItemCellPixels + 3 * kGap, kItemCellPixels, kGap) == 4);
}

TEST_CASE("Badge : il garde son corps tant que le nombre y tient", "[ui][sac]") {
  constexpr float kBase = 10.0F;
  CHECK(ItemBadgeFontSize(kBase, 12.0F, 30.0F) == kBase);
  CHECK(ItemBadgeFontSize(kBase, 30.0F, 30.0F) == kBase);
}

TEST_CASE("Badge : il rétrécit quand le nombre déborde, et pas en dessous du plancher",
          "[ui][sac]") {
  // 🔴 CE PLANCHER EST CE QUI ÉVITE UN CHIFFRE ILLISIBLE. Sans lui, un total à
  // six chiffres se réduirait jusqu'à n'être qu'une tache — et le client, lui,
  // coupe plutôt que de descendre là.
  constexpr float kBase = 10.0F;
  const float floor_size = kBase * kItemBadgeMinimumScale;

  const float shrunk = ItemBadgeFontSize(kBase, 40.0F, 30.0F);
  CHECK(shrunk < kBase);
  CHECK(shrunk >= floor_size);

  // Un nombre démesuré s'arrête au plancher et n'en descend pas.
  CHECK(ItemBadgeFontSize(kBase, 1000.0F, 30.0F) >= floor_size);
}

TEST_CASE("Badge : une case sans place ne fait pas divergir le calcul", "[ui][sac]") {
  // Largeur nulle ou négative : on rend le corps demandé plutôt qu'un zéro, une
  // division par zéro ou un corps négatif.
  constexpr float kBase = 10.0F;
  CHECK(ItemBadgeFontSize(kBase, 40.0F, 0.0F) == kBase);
  CHECK(ItemBadgeFontSize(kBase, 0.0F, 30.0F) == kBase);
}

TEST_CASE("Onglets : les onze ont un libellé et deux images distinctes", "[ui][sac]") {
  for (std::size_t rank = 0; rank < kItemTabCount; ++rank) {
    const ItemTab tab = static_cast<ItemTab>(rank);
    const char* label = ItemTabLabel(tab);
    REQUIRE(label != nullptr);
    CHECK(label[0] != '\0');
    // L'onglet ouvert et l'onglet fermé ne se dessinent pas pareil : c'est la
    // seule chose qui dise lequel est choisi.
    CHECK(ItemTabPiece(tab, true) != ItemTabPiece(tab, false));
  }
}

TEST_CASE("Onglets : aucun n'emprunte l'image d'un autre, sauf l'apparat", "[ui][sac]") {
  // Deux onglets à la même image seraient indiscernables, et l'erreur se lit
  // mal dans une table de vingt-deux pièces.
  //
  // 🔴 L'EXCEPTION VIENT DU CLIENT : `tab_cos` EST la tuile d'apparat, et c'est
  // celle qu'il donne à l'onglet « Equip » du sac. Les deux onglets la portent
  // donc, et aucune fenêtre ne les montre ensemble — le sac n'a pas
  // « Costumes », le coffre n'a pas « Equip ».
  for (std::size_t first = 0; first < kItemTabCount; ++first) {
    for (std::size_t second = first + 1; second < kItemTabCount; ++second) {
      const auto one = static_cast<ItemTab>(first);
      const auto other = static_cast<ItemTab>(second);
      if (one == ItemTab::kEquipment && other == ItemTab::kCostume) {
        continue;
      }
      CHECK(ItemTabPiece(one, true) != ItemTabPiece(other, true));
      CHECK(ItemTabPiece(one, false) != ItemTabPiece(other, false));
    }
  }
}

TEST_CASE("Zenys : un montant se groupe par milliers, comme chez le client", "[ui][sac]") {
  // 🔴 LA VIRGULE, ET NON L'ESPACE DU FRANÇAIS : c'est ce qu'écrit le client
  // d'origine. Un joueur compare deux
  // fenêtres, pas deux typographies.
  CHECK(GroupThousands(10'900'000) == "10,900,000");
  CHECK(GroupThousands(0) == "0");
  CHECK(GroupThousands(1) == "1");
  CHECK(GroupThousands(999) == "999");
  // La première virgule tombe au quatrième chiffre, pas au troisième.
  CHECK(GroupThousands(1000) == "1,000");
  CHECK(GroupThousands(999'999) == "999,999");
  CHECK(GroupThousands(1'000'000) == "1,000,000");
}

TEST_CASE("Zenys : les groupes ne glissent pas d'un cran", "[ui][sac]") {
  // ⚠ CE QUE CET ESSAI ATTRAPE. Compter les CARACTÈRES écrits plutôt que les
  // CHIFFRES inclut les virgules déjà posées, et chaque groupe se décale d'un de
  // plus que le précédent : « 1,234,5678 » au lieu de « 12,345,678 ». La faute
  // ne se voit qu'à partir du deuxième séparateur.
  CHECK(GroupThousands(12'345'678) == "12,345,678");
  CHECK(GroupThousands(123'456'789'012) == "123,456,789,012");
}

TEST_CASE("Zenys : le plus petit entier ne fait pas déborder le calcul", "[ui][sac]") {
  // ⚠ IL N'A PAS D'OPPOSÉ, et le nier est un comportement indéfini. Le montant
  // ne sera jamais celui-là, mais la fonction ne doit pas être le maillon qui
  // cède si un décodage rate.
  CHECK(GroupThousands(-1) == "-1");
  CHECK(GroupThousands(-1'234) == "-1,234");
  const std::string smallest = GroupThousands(std::numeric_limits<std::int64_t>::min());
  CHECK(smallest == "-9,223,372,036,854,775,808");
}
