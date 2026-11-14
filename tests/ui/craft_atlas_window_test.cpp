// L'ATLAS DES RECETTES : ce que ses lignes font du clic.
//
// 🔴 ICI LE CLIC GAUCHE NAVIGUE, là où il décrit partout ailleurs, et c'est la
// règle qui se perdrait le plus facilement : un appel au geste commun des liens
// suffirait à ouvrir une description à chaque saut. Ce banc tient donc la
// navigation — le clic suit, le retour revient —, le Maj+clic qui pose le
// lien, et la liste des lignes réellement dessinées, dont dépendent les icônes.

#include "ro/ui/craft_atlas_window.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::CraftAtlasAnswer;
using ro::ui::CraftAtlasGroup;
using ro::ui::CraftAtlasRow;
using ro::ui::CraftAtlasView;
using ro::ui::CraftAtlasWindowState;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr float kScanStepY = 3.0F;
constexpr float kScanStepX = 15.0F;
constexpr int kSettleFrames = 4;
constexpr std::uint32_t kSword = 1101;
constexpr std::uint32_t kKnife = 1201;
constexpr int kSmithSword = 99;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

struct AtlasProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  CraftAtlasWindowState state;
  std::array<CraftAtlasRow, 2> products = {
      CraftAtlasRow{.item_id = kSword, .name = "Sword", .craftable = 2},
      CraftAtlasRow{.item_id = kKnife, .name = "Knife"}};
  std::array<CraftAtlasGroup, 1> groups = {
      CraftAtlasGroup{.skill_id = kSmithSword, .label = "Smith Sword", .products = products}};
  CraftAtlasView view{.available = true, .groups = groups, .products = products};

  AtlasProbe() { state.open = true; }

  CraftAtlasAnswer Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    CraftAtlasAnswer answer = ro::ui::DrawCraftAtlasWindow(state, view, art);
    ui.EndFrame();
    return answer;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }

  void Filter(std::string_view text) {
    state.filter.fill('\0');
    std::ranges::copy(text.substr(0, state.filter.size() - 1), state.filter.begin());
  }
};

// Balaie la fenêtre jusqu'à survoler la ligne de l'item voulu.
[[nodiscard]] bool HoverRow(AtlasProbe& probe, std::uint32_t item_id) {
  const ImGuiWindow* window = ImGui::FindWindowByName("###craft_atlas");
  REQUIRE(window != nullptr);
  ImGuiIO& io = ImGui::GetIO();
  for (float y = window->Pos.y; y < window->Pos.y + window->Size.y; y += kScanStepY) {
    for (float x = window->Pos.x; x < window->Pos.x + window->Size.x; x += kScanStepX) {
      io.AddMousePosEvent(x, y);
      if (probe.Frame().item.hovered_id == item_id) {
        return true;
      }
    }
  }
  return false;
}

[[nodiscard]] CraftAtlasAnswer Click(AtlasProbe& probe, bool shift = false) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiMod_Shift, shift);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  CraftAtlasAnswer released = probe.Frame();
  io.AddKeyEvent(ImGuiMod_Shift, false);
  static_cast<void>(probe.Frame());
  return released;
}

}  // namespace

TEST_CASE("Atlas : fermé, il ne dessine rien", "[ui][atlas]") {
  AtlasProbe probe;
  probe.state.open = false;
  const CraftAtlasAnswer answer = probe.Frame();
  CHECK(answer.drawn.empty());
  CHECK(ImGui::FindWindowByName("###craft_atlas") == nullptr);
}

TEST_CASE("Atlas : sans fichier, aucune liste ne se dessine", "[ui][atlas]") {
  AtlasProbe probe;
  probe.view.available = false;
  probe.Filter("Sword");
  probe.Settle();
  CHECK(probe.Frame().drawn.empty());
}

TEST_CASE("Atlas : un filtre ouvre le skill, et ses lignes se disent dessinées", "[ui][atlas]") {
  AtlasProbe probe;
  probe.Filter("sWoRd");
  probe.Settle();
  const CraftAtlasAnswer answer = probe.Frame();
  CHECK(std::ranges::find(answer.drawn, kSword) != answer.drawn.end());
  // « Knife » ne passe pas le filtre : sa ligne n'est pas dessinée.
  CHECK(std::ranges::find(answer.drawn, kKnife) == answer.drawn.end());
}

TEST_CASE("Atlas : l'identifiant filtre aussi", "[ui][atlas]") {
  AtlasProbe probe;
  probe.Filter("1201");
  probe.Settle();
  const CraftAtlasAnswer answer = probe.Frame();
  CHECK(std::ranges::find(answer.drawn, kKnife) != answer.drawn.end());
  CHECK(std::ranges::find(answer.drawn, kSword) == answer.drawn.end());
}

TEST_CASE("Atlas : le clic gauche SUIT l'item, il ne le décrit pas", "[ui][atlas]") {
  AtlasProbe probe;
  probe.Filter("Sword");
  probe.Settle();
  REQUIRE(HoverRow(probe, kSword));
  const CraftAtlasAnswer released = Click(probe);
  CHECK(probe.state.selected_id == kSword);
  CHECK(released.item.gesture == ro::ui::ChatLinkGesture::kNothing);
  CHECK(probe.state.back.empty());
}

TEST_CASE("Atlas : Maj + clic pose le lien, sans naviguer", "[ui][atlas]") {
  AtlasProbe probe;
  probe.Filter("Sword");
  probe.Settle();
  REQUIRE(HoverRow(probe, kSword));
  const CraftAtlasAnswer released = Click(probe, /*shift=*/true);
  CHECK(released.item.gesture == ro::ui::ChatLinkGesture::kRepost);
  CHECK(released.item.item_id == kSword);
  CHECK(probe.state.selected_id == 0);
}

TEST_CASE("Atlas : suivre un second item empile le premier", "[ui][atlas]") {
  AtlasProbe probe;
  probe.state.selected_id = kKnife;
  probe.Filter("Sword");
  probe.Settle();
  REQUIRE(HoverRow(probe, kSword));
  static_cast<void>(Click(probe));
  CHECK(probe.state.selected_id == kSword);
  REQUIRE(probe.state.back.size() == 1);
  CHECK(probe.state.back.front() == kKnife);
}

TEST_CASE("Atlas : ouvrir sur un item repart d'un historique vide", "[ui][atlas]") {
  CraftAtlasWindowState state;
  state.back = {kKnife, kSword};
  ro::ui::OpenCraftAtlasOn(state, kKnife);
  CHECK(state.open);
  CHECK(state.raise);
  CHECK(state.selected_id == kKnife);
  CHECK(state.back.empty());
}

// ── LA LIGNE DU STAFF : CE QU'ELLE DEMANDE, ET À QUI ELLE SE MONTRE ─────────

namespace {

using ro::ui::CraftAtlasGrantKind;
using ro::ui::StaffItemGrant;

constexpr std::uint32_t kIronOre = 1002;
constexpr std::uint32_t kCoal = 1003;
constexpr std::uint32_t kAnvil = 986;

// Une recette de trois lignes : deux matériaux consommés, dont l'un déjà en
// quantité, et un guide non consommé qu'on ne possède pas.
[[nodiscard]] std::array<CraftAtlasRow, 3> SwordIngredients() {
  return {CraftAtlasRow{.item_id = kIronOre, .name = "Iron Ore", .owned = 3, .quantity = 2},
          CraftAtlasRow{.item_id = kCoal, .name = "Coal", .owned = 50, .quantity = 1},
          CraftAtlasRow{.item_id = kAnvil, .name = "Anvil", .owned = 0, .kept = true}};
}

// La fiche d'une épée, avec sa recette, choisie dans l'Atlas.
struct StaffProbe : AtlasProbe {
  std::array<CraftAtlasRow, 3> ingredients = SwordIngredients();

  StaffProbe() {
    view.sheet.item = CraftAtlasRow{.item_id = kSword, .name = "Sword"};
    view.sheet.recipe = ro::ui::CraftAtlasRecipeView{.skill_id = kSmithSword,
                                                     .skill_label = "Smith Sword",
                                                     .skill_level = 1,
                                                     .ingredients = ingredients};
    state.selected_id = kSword;
  }
};

[[nodiscard]] ImGuiWindow* SheetWindow() {
  for (ImGuiWindow* window : GImGui->Windows) {
    if (window->WasActive &&
        std::string_view(window->Name).find("##atlas_sheet") != std::string_view::npos) {
      return window;
    }
  }
  return nullptr;
}

// Balaie la fiche jusqu'à survoler le bouton de ce libellé. Faux : il n'y est
// pas.
[[nodiscard]] bool HoverSheetButton(StaffProbe& probe, const char* label) {
  ImGuiWindow* sheet = SheetWindow();
  REQUIRE(sheet != nullptr);
  const ImGuiID wanted = sheet->GetID(label);
  ImGuiIO& io = ImGui::GetIO();
  for (float y = sheet->Pos.y; y < sheet->Pos.y + sheet->Size.y; y += kScanStepY) {
    for (float x = sheet->Pos.x; x < sheet->Pos.x + sheet->Size.x; x += kScanStepX) {
      io.AddMousePosEvent(x, y);
      static_cast<void>(probe.Frame());
      if (GImGui->HoveredId == wanted) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

TEST_CASE("Atlas : ce qu'exige une recette, et ce qui manque", "[ui][atlas][staff]") {
  const std::array<CraftAtlasRow, 3> rows = SwordIngredients();
  // Un matériau consommé : la quantité, fois le compte.
  CHECK(ro::ui::CraftIngredientNeed(rows[0], 1) == 2);
  CHECK(ro::ui::CraftIngredientNeed(rows[0], 5) == 10);
  CHECK(ro::ui::CraftIngredientShortfall(rows[0], 1) == 0);
  CHECK(ro::ui::CraftIngredientShortfall(rows[0], 5) == 7);
  // Un guide : UN, quel que soit le compte.
  CHECK(ro::ui::CraftIngredientNeed(rows[2], 1) == 1);
  CHECK(ro::ui::CraftIngredientNeed(rows[2], 5) == 1);
  CHECK(ro::ui::CraftIngredientShortfall(rows[2], 5) == 1);
  CraftAtlasRow held_guide = rows[2];
  held_guide.owned = 1;
  CHECK(ro::ui::CraftIngredientShortfall(held_guide, 5) == 0);
}

TEST_CASE("Atlas : « Obtenir tout » demande tout, multiplié", "[ui][atlas][staff]") {
  const std::array<CraftAtlasRow, 3> rows = SwordIngredients();
  CHECK(ro::ui::CraftAtlasGrants(rows, 1, CraftAtlasGrantKind::kAll, std::nullopt).grants ==
        std::vector<StaffItemGrant>{{kIronOre, 2}, {kCoal, 1}, {kAnvil, 1}});
  CHECK(ro::ui::CraftAtlasGrants(rows, 4, CraftAtlasGrantKind::kAll, std::nullopt).grants ==
        std::vector<StaffItemGrant>{{kIronOre, 8}, {kCoal, 4}, {kAnvil, 1}});
}

TEST_CASE("Atlas : « Obtenir le manque » tait l'ingrédient suffisant", "[ui][atlas][staff]") {
  const std::array<CraftAtlasRow, 3> rows = SwordIngredients();
  // Une fois : le minerai et le charbon suffisent, seul le guide manque.
  CHECK(ro::ui::CraftAtlasGrants(rows, 1, CraftAtlasGrantKind::kMissing, std::nullopt).grants ==
        std::vector<StaffItemGrant>{{kAnvil, 1}});
  // Dix fois : 20 minerais exigés, 3 tenus ; le charbon suffit encore.
  CHECK(ro::ui::CraftAtlasGrants(rows, 10, CraftAtlasGrantKind::kMissing, std::nullopt).grants ==
        std::vector<StaffItemGrant>{{kIronOre, 17}, {kAnvil, 1}});
}

TEST_CASE("Atlas : le compte et les quantités sont bornés", "[ui][atlas][staff]") {
  constexpr int kHeavyQuantity = 1000;
  const std::array<CraftAtlasRow, 1> heavy = {
      CraftAtlasRow{.item_id = kIronOre, .name = "Iron Ore", .quantity = kHeavyQuantity}};
  // Le compte ramené à [1, plafond] : zéro vaut une fabrication.
  CHECK(ro::ui::CraftAtlasGrants(heavy, 0, CraftAtlasGrantKind::kAll, std::nullopt).grants ==
        std::vector<StaffItemGrant>{{kIronOre, kHeavyQuantity}});
  // Une pile pleine, pas davantage.
  CHECK(ro::ui::CraftAtlasGrants(heavy, ro::ui::kCraftAtlasMostCrafts, CraftAtlasGrantKind::kAll,
                                 std::nullopt)
            .grants == std::vector<StaffItemGrant>{{kIronOre, ro::ui::kStaffItemMostAmount}});
}

TEST_CASE("Atlas : sans le droit du staff, la ligne n'existe pas", "[ui][atlas][staff]") {
  StaffProbe probe;
  probe.view.staff_items = false;
  probe.Settle();
  CHECK_FALSE(HoverSheetButton(probe, "Obtenir tout"));
  CHECK_FALSE(HoverSheetButton(probe, "Obtenir le manque"));
}

TEST_CASE("Atlas : le staff obtient le manque de N fabrications", "[ui][atlas][staff]") {
  StaffProbe probe;
  probe.view.staff_items = true;
  probe.state.staff_crafts = 10;
  probe.Settle();
  REQUIRE(HoverSheetButton(probe, "Obtenir le manque"));
  const CraftAtlasAnswer released = Click(probe);
  CHECK(released.staff_items == std::vector<StaffItemGrant>{{kIronOre, 17}, {kAnvil, 1}});
  // Le geste n'a pas navigué : la fiche reste celle de l'épée.
  CHECK(probe.state.selected_id == kSword);
}

TEST_CASE("Atlas : le staff obtient tout", "[ui][atlas][staff]") {
  StaffProbe probe;
  probe.view.staff_items = true;
  probe.state.staff_crafts = 2;
  probe.Settle();
  REQUIRE(HoverSheetButton(probe, "Obtenir tout"));
  const CraftAtlasAnswer released = Click(probe);
  CHECK(released.staff_items ==
        std::vector<StaffItemGrant>{{kIronOre, 4}, {kCoal, 2}, {kAnvil, 1}});
}

// ── LE MANQUE AFFICHÉ SUIT LES FABRICATIONS DU STAFF ────────────────────────

namespace {

// Le texte qu'une image de l'Atlas a rendu, capté par le journal d'ImGui.
[[nodiscard]] std::string RenderedText(StaffProbe& probe) {
  probe.ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::LogToBuffer();
  static_cast<void>(ro::ui::DrawCraftAtlasWindow(probe.state, probe.view, probe.art));
  std::string text = GImGui->LogBuffer.c_str();
  ImGui::LogFinish();
  probe.ui.EndFrame();
  return text;
}

[[nodiscard]] bool Shows(std::string_view text, std::string_view piece) {
  return text.find(piece) != std::string_view::npos;
}

}  // namespace

TEST_CASE("Atlas : hors vue staff, le manque vaut pour une fabrication", "[ui][atlas][staff]") {
  StaffProbe probe;
  probe.view.staff_items = false;
  // Un compte resté d'une session staff ne compte plus.
  probe.state.staff_crafts = 10;
  probe.Settle();
  const std::string text = RenderedText(probe);
  CHECK(Shows(text, "Iron Ore x2  (3)"));
  CHECK(Shows(text, "Coal x1  (50)"));
  CHECK_FALSE(Shows(text, "fabrications :"));
}

TEST_CASE("Atlas : en vue staff, le manque vaut pour N fabrications", "[ui][atlas][staff]") {
  StaffProbe probe;
  probe.view.staff_items = true;
  probe.state.staff_crafts = 10;
  probe.Settle();
  const std::string text = RenderedText(probe);
  CHECK(Shows(text, "Quantités pour 10 fabrications :"));
  // 20 minerais exigés, 3 tenus : il en manque 17 — ce que « Obtenir le
  // manque » demanderait.
  CHECK(Shows(text, "Iron Ore x20  (3 — il en manque 17)"));
  CHECK(Shows(text, "Coal x10  (50)"));
  // Le guide n'en exige toujours qu'un.
  CHECK(Shows(text, "Anvil (requis, non consommé — inventory : 0)"));
}

TEST_CASE("Atlas : une seule fabrication ne s'annonce pas", "[ui][atlas][staff]") {
  StaffProbe probe;
  probe.view.staff_items = true;
  probe.state.staff_crafts = 1;
  probe.Settle();
  const std::string text = RenderedText(probe);
  CHECK(Shows(text, "Iron Ore x2  (3)"));
  CHECK_FALSE(Shows(text, "fabrications :"));
}

// ── LES CASES DU SAC BORNENT UN INGRÉDIENT NON EMPILABLE ────────────────────

namespace {

using ro::ui::CraftAtlasGrantPlan;
using ro::ui::StaffItemRoom;

constexpr std::uint32_t kShield = 2101;
constexpr int kKnivesPerCraft = 3;
constexpr int kCoalPerCraft = 5;

// Deux couteaux qui ne s'empilent pas, du charbon qu'on n'a pas, un bouclier.
[[nodiscard]] std::array<CraftAtlasRow, 3> ArmouryIngredients() {
  return {
      CraftAtlasRow{
          .item_id = kKnife, .name = "Knife", .quantity = kKnivesPerCraft, .stackable = false},
      CraftAtlasRow{.item_id = kCoal, .name = "Coal", .quantity = kCoalPerCraft, .stackable = true},
      CraftAtlasRow{.item_id = kShield, .name = "Shield", .quantity = 1, .stackable = false}};
}

}  // namespace

TEST_CASE("Atlas : un ingrédient non empilable est ramené aux cases libres", "[ui][atlas][staff]") {
  const std::array<CraftAtlasRow, 3> rows = ArmouryIngredients();
  // Cinq cases : trois couteaux, une pile de charbon, un bouclier.
  const CraftAtlasGrantPlan roomy = ro::ui::CraftAtlasGrants(rows, 1, CraftAtlasGrantKind::kAll, 5);
  CHECK(roomy.room == StaffItemRoom::kFits);
  CHECK(roomy.grants == std::vector<StaffItemGrant>{
                            {kKnife, kKnivesPerCraft}, {kCoal, kCoalPerCraft}, {kShield, 1}});
  // Deux cases : deux couteaux sur trois ; le charbon, empilable, n'est pas
  // borné ; le bouclier ne trouve plus de case et n'est pas demandé.
  const CraftAtlasGrantPlan tight = ro::ui::CraftAtlasGrants(rows, 1, CraftAtlasGrantKind::kAll, 2);
  CHECK(tight.room == StaffItemRoom::kFull);
  CHECK(tight.grants == std::vector<StaffItemGrant>{{kKnife, 2}, {kCoal, kCoalPerCraft}});
}

TEST_CASE("Atlas : une pile nouvelle prend sa case avant le suivant", "[ui][atlas][staff]") {
  const std::array<CraftAtlasRow, 2> rows = {
      CraftAtlasRow{.item_id = kCoal, .name = "Coal", .quantity = 1, .stackable = true},
      CraftAtlasRow{.item_id = kKnife, .name = "Knife", .quantity = 2, .stackable = false}};
  const CraftAtlasGrantPlan plan = ro::ui::CraftAtlasGrants(rows, 1, CraftAtlasGrantKind::kAll, 2);
  CHECK(plan.room == StaffItemRoom::kBounded);
  CHECK(plan.grants == std::vector<StaffItemGrant>{{kCoal, 1}, {kKnife, 1}});
}

TEST_CASE("Atlas : empilable, inconnu ou sac non dit, rien n'est borné", "[ui][atlas][staff]") {
  std::array<CraftAtlasRow, 1> rows = {CraftAtlasRow{
      .item_id = kCoal, .name = "Coal", .quantity = kCoalPerCraft, .stackable = true}};
  // Empilable, sac plein : la pile entière.
  CHECK(ro::ui::CraftAtlasGrants(rows, 1, CraftAtlasGrantKind::kAll, 0).grants ==
        std::vector<StaffItemGrant>{{kCoal, kCoalPerCraft}});
  // Empilement inconnu : le comportement d'avant la table.
  rows[0].stackable.reset();
  CHECK(ro::ui::CraftAtlasGrants(rows, 1, CraftAtlasGrantKind::kAll, 0).grants ==
        std::vector<StaffItemGrant>{{kCoal, kCoalPerCraft}});
  // Non empilable, capacité du sac pas encore dite.
  rows[0].stackable = false;
  const CraftAtlasGrantPlan unknown_bag =
      ro::ui::CraftAtlasGrants(rows, 1, CraftAtlasGrantKind::kAll, std::nullopt);
  CHECK(unknown_bag.room == StaffItemRoom::kFits);
  CHECK(unknown_bag.grants == std::vector<StaffItemGrant>{{kCoal, kCoalPerCraft}});
}

TEST_CASE("Atlas : sac plein, « Obtenir tout » se grise et dit pourquoi", "[ui][atlas][staff]") {
  StaffProbe probe;
  probe.view.staff_items = true;
  for (CraftAtlasRow& ingredient : probe.ingredients) {
    ingredient.stackable = false;
  }
  probe.view.free_bag_slots = 0;
  probe.Settle();
  REQUIRE(HoverSheetButton(probe, "Obtenir tout"));
  CHECK(GImGui->HoveredIdIsDisabled);
  CHECK(Shows(RenderedText(probe), "L'inventory est plein"));
  const CraftAtlasAnswer released = Click(probe);
  CHECK(released.staff_items.empty());
}
