// Les fenêtres de description : ce que leur titre retient d'une image à
// l'autre.

#include "ro/ui/description_window.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

}  // namespace

TEST_CASE("Description : un objet réparé perd « Broken » de son titre", "[ui][description]") {
  // 🔴 LE TITRE NE SE RECOMPOSAIT QU'AU CHANGEMENT DE NOM OU DE LANGUE, alors
  // qu'il porte aussi l'état abîmé : un objet réparé fenêtre ouverte le
  // gardait jusqu'au changement d'objet.
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  ro::ui::DescriptionWindowState state;
  state.open = true;
  state.item_id = 1201;
  ro::ui::DescriptionView view;
  view.name = "Couteau";
  view.damaged = true;

  const auto frame = [&]() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawDescriptionWindow(state, view, art));
    ui.EndFrame();
  };
  frame();
  REQUIRE(state.title.find("Broken") != std::string::npos);

  view.damaged = false;
  frame();
  CHECK(state.title.find("Broken") == std::string::npos);
}

TEST_CASE("Description : le coût en SP du titre suit le niveau décrit", "[ui][description]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  ro::ui::SkillDescriptionWindowState state;
  state.open = true;
  state.skill_id = 75;
  ro::ui::SkillDescriptionView view;
  view.name = "Gloria";
  view.sp_cost = 20;

  const auto frame = [&]() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawSkillDescriptionWindow(state, view, art));
    ui.EndFrame();
  };
  frame();
  REQUIRE(state.title.find("20") != std::string::npos);

  view.sp_cost = 60;
  frame();
  CHECK(state.title.find("60") != std::string::npos);
}

namespace {

// Une boîte à deux tirages : un objet, un monstre, et le lot garanti.
struct OddsProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  ro::ui::DescriptionWindowState state;
  std::array<ro::ui::DescriptionOddsRow, 3> rows = {
      ro::ui::DescriptionOddsRow{.id = 4001,
                                 .name = "Poring Card",
                                 .percent = 9.0909,
                                 .weight = 1,
                                 .total = 11,
                                 .group = 1},
      ro::ui::DescriptionOddsRow{.id = 1002,
                                 .monster = true,
                                 .name = "Poring",
                                 .percent = 90.9091,
                                 .weight = 10,
                                 .total = 11,
                                 .group = 1},
      ro::ui::DescriptionOddsRow{
          .id = 607, .name = "Yggdrasil Berry", .percent = 100.0, .group = 2, .guaranteed = true}};
  ro::ui::DescriptionView view;

  OddsProbe() {
    state.open = true;
    state.item_id = 616;
    view.item_id = 616;
    view.name = "Old Card Album";
    view.odds = rows;
    view.odds_many_groups = true;
  }

  ro::ui::DescriptionWindowAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ro::ui::DescriptionWindowAction action = ro::ui::DrawDescriptionWindow(state, view, art);
    ui.EndFrame();
    return action;
  }
};

}  // namespace

TEST_CASE("Description : un conteneur porte l'onglet « Probabilités »", "[ui][description]") {
  OddsProbe probe;
  for (int frame = 0; frame < 3; ++frame) {
    const ro::ui::DescriptionWindowAction action = probe.Frame();
    CHECK(action.link.gesture == ro::ui::ChatLinkGesture::kNothing);
  }
  // L'onglet n'a pas été choisi : il n'a encore rien trié, mais il existe.
  CHECK(ro::ui::ItemSheetTabOffered(ro::ui::ItemSheetTab::kOdds, probe.view));
  // Sans table, plus d'onglet — et plus de barre : « Description » serait seul.
  probe.view.odds = {};
  (void)probe.Frame();
  CHECK_FALSE(ro::ui::ItemSheetTabOffered(ro::ui::ItemSheetTab::kOdds, probe.view));
}

TEST_CASE("Description : suivre un lien empile, « Retour » dépile", "[ui][description]") {
  OddsProbe probe;
  (void)probe.Frame();
  ro::ui::NavigateDescription(probe.state, 4001);
  CHECK(probe.state.item_id == 4001);
  REQUIRE(probe.state.back.size() == 1);
  CHECK(probe.state.back.back() == 616);
  // Notre propre saut garde la pile d'une image à l'autre.
  (void)probe.Frame();
  CHECK(probe.state.back.size() == 1);
  // Un objet arrivé d'ailleurs ouvre un autre fil.
  probe.state.item_id = 909;
  (void)probe.Frame();
  CHECK(probe.state.back.empty());
  // Et la fenêtre fermée l'oublie aussi.
  ro::ui::NavigateDescription(probe.state, 4001);
  probe.state.open = false;
  (void)probe.Frame();
  CHECK(probe.state.back.empty());
}

TEST_CASE("Description : l'ouvrir la pose sous le curseur, devant, avec sa provenance",
          "[ui][description]") {
  ro::ui::DescriptionWindowState state;
  constexpr std::uint16_t kBagSlot = 7;
  ro::ui::OpenItemDescription(state, 1201, kBagSlot, ro::ui::ItemHolder::kEquipment);
  CHECK(state.open);
  CHECK(state.item_id == 1201);
  CHECK(state.from_slot == kBagSlot);
  CHECK(state.from_holder == ro::ui::ItemHolder::kEquipment);
  CHECK(state.place_at_mouse);
  CHECK(state.raise);
}

TEST_CASE("Description : les quatre onglets du serveur se dessinent", "[ui][description]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  ro::ui::DescriptionWindowState state;
  state.open = true;
  state.item_id = 909;
  const std::array<ro::ui::DescriptionDropRow, 2> drops = {
      ro::ui::DescriptionDropRow{.monster_id = 1002, .name = "Poring", .rate_hundredths = 7000},
      ro::ui::DescriptionDropRow{.monster_id = 1038,
                                 .name = "Osiris",
                                 .boss_rank = 2,
                                 .mvp_reward = true,
                                 .rate_hundredths = 100}};
  const std::array<ro::ui::DescriptionComboMember, 2> members = {
      ro::ui::DescriptionComboMember{.item_id = 909, .name = "Jellopy"},
      ro::ui::DescriptionComboMember{.item_id = 4001, .name = "Poring Card"}};
  const std::array<ro::ui::DescriptionComboView, 1> combos = {
      ro::ui::DescriptionComboView{.members = members, .script = "bonus bLuk,5; // chance"}};
  ro::ui::DescriptionView view;
  view.item_id = 909;
  view.name = "Jellopy";
  view.server_tabs = true;

  const auto frame = [&]() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ro::ui::DescriptionWindowAction action = ro::ui::DrawDescriptionWindow(state, view, art);
    ui.EndFrame();
    return action;
  };
  // En attendant les réponses : rien ne se grise, rien ne se demande.
  view.drops_ordinary.fetch = ro::ui::DescriptionFetch::kWaiting;
  view.drops_boss.fetch = ro::ui::DescriptionFetch::kWaiting;
  view.scripts.fetch = ro::ui::DescriptionFetch::kWaiting;
  CHECK(frame().link.gesture == ro::ui::ChatLinkGesture::kNothing);
  // Les réponses arrivées.
  view.drops_ordinary = {.fetch = ro::ui::DescriptionFetch::kReady, .rows = drops};
  view.drops_boss = {.fetch = ro::ui::DescriptionFetch::kReady,
                     .rows = drops,
                     .truncated = true,
                     .treasure_excluded = 2};
  view.scripts = {.fetch = ro::ui::DescriptionFetch::kReady,
                  .main = "if (getrefine() > 5) { bonus bStr,1; bonus2 bAddRace,RC_Boss,10; }",
                  .combos = combos};
  for (int rank = 0; rank < 3; ++rank) {
    const ro::ui::DescriptionWindowAction action = frame();
    CHECK_FALSE(action.refresh_ordinary);
    CHECK_FALSE(action.refresh_boss);
  }
  // Une réponse vide : l'onglet se grise, la fenêtre se dessine toujours.
  view.drops_boss = {.fetch = ro::ui::DescriptionFetch::kReady};
  view.scripts = {.fetch = ro::ui::DescriptionFetch::kReady, .unknown = true};
  CHECK(frame().link.gesture == ro::ui::ChatLinkGesture::kNothing);
}

TEST_CASE("Description : un œuf éclos se dit « Éclos », pas « Broken »", "[ui][description]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  ro::ui::DescriptionWindowState state;
  state.open = true;
  state.item_id = 9001;
  ro::ui::DescriptionView view;
  view.item_id = 9001;
  view.name = "Poring Egg";
  view.damaged = true;
  view.pet_egg = true;
  view.pet_inside = true;
  view.pet_intimacy = "Cordial";

  ui.BeginFrame(kViewport, kFrameSeconds);
  static_cast<void>(ro::ui::DrawDescriptionWindow(state, view, art));
  ro::ui::DrawDescriptionPreview(view, art);
  ui.EndFrame();
  CHECK(state.title.find("Broken") == std::string::npos);
  CHECK(state.title.find("clos") != std::string::npos);
}

TEST_CASE("Description : cartes et options se posent sous la fenêtre", "[ui][description]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  ro::ui::DescriptionWindowState state;
  state.open = true;
  state.item_id = 1201;
  const std::array<ro::ui::DescriptionCardRow, 3> cards = {
      ro::ui::DescriptionCardRow{.id = 4001, .name = "Poring Card"}, ro::ui::DescriptionCardRow{},
      ro::ui::DescriptionCardRow{.id = 4700}};
  const std::array<ro::ui::DescriptionOptionRow, 2> options = {
      ro::ui::DescriptionOptionRow{.text = "MaxHP +250", .id = 1, .value = 250},
      ro::ui::DescriptionOptionRow{.id = 99, .value = -3, .param = 1}};
  ro::ui::DescriptionView view;
  view.item_id = 1201;
  view.name = "+7 Knife [3]";
  view.cards = cards;
  view.card_slots = 3;
  view.options = options;

  for (int rank = 0; rank < 3; ++rank) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ro::ui::DescriptionWindowAction action = ro::ui::DrawDescriptionWindow(state, view, art);
    ro::ui::DrawDescriptionPreview(view, art);
    ui.EndFrame();
    // Rien de survolé ni de cliqué : aucun geste ne naît du seul dessin.
    CHECK(action.link.gesture == ro::ui::ChatLinkGesture::kNothing);
  }
  // Un objet sans rien de serti n'ouvre aucun panneau, et la fenêtre reste.
  view.cards = {};
  view.options = {};
  ui.BeginFrame(kViewport, kFrameSeconds);
  static_cast<void>(ro::ui::DrawDescriptionWindow(state, view, art));
  ui.EndFrame();
  CHECK(state.open);
}

TEST_CASE("Description : le forgeron se colore, dans l'aperçu et dans la fenêtre",
          "[ui][description]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  ro::ui::DescriptionWindowState state;
  state.open = true;
  state.item_id = 1201;
  const std::array<ro::ui::DescriptionNamePiece, 3> pieces = {
      ro::ui::DescriptionNamePiece{.text = "Very Strong "},
      ro::ui::DescriptionNamePiece{.text = "Sans nom's ",
                                   .tone = ro::ui::DescriptionNameTone::kUnknownCreator},
      ro::ui::DescriptionNamePiece{.text = "Fire Knife"}};
  ro::ui::DescriptionView view;
  view.item_id = 1201;
  view.name = "Very Strong Sans nom's Fire Knife";
  view.name_pieces = pieces;
  view.damaged = true;

  ui.BeginFrame(kViewport, kFrameSeconds);
  static_cast<void>(ro::ui::DrawDescriptionWindow(state, view, art));
  ro::ui::DrawDescriptionPreview(view, art);
  ui.EndFrame();
  // Le titre garde le nom à plat.
  CHECK(state.title.find("Sans nom's") != std::string::npos);
}

TEST_CASE("Description : le bouton alootid ne paraît qu'avec la liste", "[ui][description]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  ro::ui::DescriptionWindowState state;
  state.open = true;
  state.item_id = 909;
  ro::ui::DescriptionView view;
  view.item_id = 909;
  view.name = "Jellopy";
  for (const ro::ui::DescriptionAutoloot autoloot :
       {ro::ui::DescriptionAutoloot::kUnavailable, ro::ui::DescriptionAutoloot::kOut,
        ro::ui::DescriptionAutoloot::kIn}) {
    view.autoloot = autoloot;
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ro::ui::DescriptionWindowAction action = ro::ui::DrawDescriptionWindow(state, view, art);
    ui.EndFrame();
    // Dessiné, pas cliqué.
    CHECK_FALSE(action.toggle_autoloot);
  }
}

namespace {

// La place de la fenêtre de description, lue dans ImGui.
[[nodiscard]] ImVec2 DescriptionAt() {
  const ImGuiWindow* window = ImGui::FindWindowByName("###description");
  REQUIRE(window != nullptr);
  return window->Pos;
}

}  // namespace

TEST_CASE("Description : elle s'ouvre au curseur, décalée, et y reste", "[ui][description]") {
  constexpr float kMouseX = 200.0F;
  constexpr float kMouseY = 150.0F;
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  ro::ui::DescriptionWindowState state;
  ro::ui::DescriptionView view;
  view.item_id = 909;
  view.name = "Jellopy";
  const auto frame = [&](float mouse_x, float mouse_y) {
    ImGui::GetIO().AddMousePosEvent(mouse_x, mouse_y);
    ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawDescriptionWindow(state, view, art));
    ui.EndFrame();
  };
  // L'ouverture : le coin haut-gauche au curseur, plus le décalage.
  state.open = true;
  state.item_id = 909;
  state.place_at_mouse = true;
  frame(kMouseX, kMouseY);
  frame(kMouseX, kMouseY);
  const ImVec2 opened = DescriptionAt();
  CHECK(opened.x == kMouseX + static_cast<float>(ro::ui::kDescriptionCursorGapPixels));
  CHECK(opened.y == kMouseY + static_cast<float>(ro::ui::kDescriptionCursorGapPixels));
  // Un autre item décrit fenêtre ouverte : le contenu change, pas la place.
  state.item_id = 512;
  view.item_id = 512;
  state.place_at_mouse = true;
  frame(kMouseX + 100.0F, kMouseY + 100.0F);
  frame(kMouseX + 100.0F, kMouseY + 100.0F);
  CHECK(DescriptionAt().x == opened.x);
  CHECK(DescriptionAt().y == opened.y);
}

TEST_CASE("Description : ouverte près d'un bord, elle reste dans l'écran", "[ui][description]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  ro::ui::DescriptionWindowState state;
  state.open = true;
  state.item_id = 909;
  state.place_at_mouse = true;
  ro::ui::DescriptionView view;
  view.item_id = 909;
  view.name = "Jellopy";
  for (int rank = 0; rank < 3; ++rank) {
    ImGui::GetIO().AddMousePosEvent(kViewport.width_px - 5.0F, kViewport.height_px - 5.0F);
    ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawDescriptionWindow(state, view, art));
    ui.EndFrame();
  }
  const ImGuiWindow* window = ImGui::FindWindowByName("###description");
  REQUIRE(window != nullptr);
  CHECK(window->Pos.x + window->Size.x <= kViewport.width_px);
  CHECK(window->Pos.y + window->Size.y <= kViewport.height_px);
  CHECK(window->Pos.x >= 0.0F);
  CHECK(window->Pos.y >= 0.0F);
}

TEST_CASE("Description : la comparaison titre « Comparaison » et se replie", "[ui][description]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  ro::ui::DescriptionWindowState state;
  state.open = true;
  state.item_id = 2302;
  ro::ui::DescriptionView worn;
  worn.item_id = 2301;
  worn.name = "Cotton Shirt";
  ro::ui::DescriptionView view;
  view.item_id = 2302;
  view.name = "Jacket";
  view.damaged = true;
  view.compared = &worn;
  const auto frame = [&]() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ro::ui::DescriptionWindowAction action = ro::ui::DrawDescriptionWindow(state, view, art);
    ui.EndFrame();
    return action;
  };
  CHECK_FALSE(frame().compare_changed);
  CHECK(state.title.find("Jacket") == std::string::npos);
  // Repliée : le nom revient, et « Broken » avec lui.
  state.compare = false;
  static_cast<void>(frame());
  CHECK(state.title.find("Jacket") != std::string::npos);
  CHECK(state.title.find("Broken") != std::string::npos);
}

TEST_CASE("Description : « ViewID » ne demande le pantin qu'au survol", "[ui][description]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  ro::ui::DescriptionWindowState state;
  state.open = true;
  state.item_id = 5001;
  const std::array<std::string, 2> lines = {"<URL>ItemID : 5001<INFO>https://exemple</INFO></URL>",
                                            "ViewID : 17"};
  ro::ui::DescriptionView view;
  view.item_id = 5001;
  view.name = "Hat";
  view.lines = lines;
  view.view_id = 17;
  view.doll_possible = true;
  state.doll.wanted = true;
  // La souris loin de la ligne : rien n'est demandé, la demande d'avant tombe.
  ImGui::GetIO().AddMousePosEvent(-100.0F, -100.0F);
  ui.BeginFrame(kViewport, kFrameSeconds);
  static_cast<void>(ro::ui::DrawDescriptionWindow(state, view, art));
  ui.EndFrame();
  CHECK_FALSE(state.doll.wanted);
}

namespace {

// La fenêtre de ce titre, ou nulle. L'illustration tombe au coin de son contenu.
[[nodiscard]] const ImGuiWindow* WindowTitled(const std::string& title) {
  for (const ImGuiWindow* window : ImGui::GetCurrentContext()->Windows) {
    if (std::string(window->Name).starts_with(title)) {
      return window;
    }
  }
  return nullptr;
}

// Survole l'illustration une image, et dit si le pantin a été demandé.
[[nodiscard]] bool DollWantedOverPicture(const ro::ui::DescriptionView& view) {
  // La souris juste à l'intérieur de l'illustration.
  constexpr float kInside = 4.0F;
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  ro::ui::DescriptionWindowState state;
  state.open = true;
  state.item_id = view.item_id;
  ui.BeginFrame(kViewport, kFrameSeconds);
  static_cast<void>(ro::ui::DrawDescriptionWindow(state, view, art));
  ui.EndFrame();
  const ImGuiWindow* window = WindowTitled(state.title);
  REQUIRE(window != nullptr);
  const ImVec2 corner = window->WorkRect.Min;
  ImGui::GetIO().AddMousePosEvent(corner.x + kInside, corner.y + kInside);
  for (int frame = 0; frame < 2; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawDescriptionWindow(state, view, art));
    ui.EndFrame();
  }
  return state.doll.wanted;
}

[[nodiscard]] ro::ui::DescriptionView CostumeView() {
  // Une icône de collection quelconque, à sa taille.
  constexpr float kPictureSide = 48.0F;
  ro::ui::DescriptionView view;
  view.item_id = 20500;
  view.name = "Costume";
  view.picture = ro::ui::SkinImage{.id = 3, .width = kPictureSide, .height = kPictureSide};
  return view;
}

}  // namespace

TEST_CASE("Description : les deux colonnes de la comparaison ont chacune leurs identifiants",
          "[ui][description]") {
  // 🔴 LES DEUX COLONNES DESSINENT LE MÊME CORPS : sans portée propre, leurs
  // widgets portent les mêmes identifiants, et ImGui le signale au survol.
  ro::ui::UiContext ui;
  ImGui::GetIO().ConfigDebugHighlightIdConflicts = true;
  const ro::ui::SkinArt art = PlainArt();
  ro::ui::DescriptionWindowState state;
  state.open = true;
  state.item_id = 1404;
  const std::array<std::string, 2> worn_lines = {"A rough-hewn wand.", "ViewID : 10"};
  const std::array<std::string, 2> lines = {"A basic spear.", "ViewID : 4"};
  ro::ui::DescriptionView worn;
  worn.item_id = 1604;
  worn.name = "Wand";
  worn.lines = worn_lines;
  ro::ui::DescriptionView view;
  view.item_id = 1404;
  view.name = "Triple Drowsy Spear";
  view.lines = lines;
  view.compared = &worn;
  const auto frame = [&]() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawDescriptionWindow(state, view, art));
    ui.EndFrame();
  };
  frame();
  const ImGuiWindow* window = WindowTitled(state.title);
  REQUIRE(window != nullptr);
  // Le curseur balaie le contenu, de haut en bas, dans les deux colonnes.
  constexpr int kSweepSteps = 24;
  const ImVec2 low = window->WorkRect.Min;
  const ImVec2 high = window->WorkRect.Max;
  int conflicts = 0;
  for (int column = 0; column < 2; ++column) {
    const float x = low.x + (high.x - low.x) * (0.25F + 0.5F * static_cast<float>(column));
    for (int step = 0; step < kSweepSteps; ++step) {
      const float y =
          low.y + (high.y - low.y) * static_cast<float>(step) / static_cast<float>(kSweepSteps);
      ImGui::GetIO().AddMousePosEvent(x, y);
      frame();
      conflicts += ImGui::GetCurrentContext()->DebugDrawIdConflictsId != 0 ? 1 : 0;
    }
  }
  CHECK(conflicts == 0);
}

TEST_CASE("Description : l'illustration d'une coiffe montre le pantin au survol",
          "[ui][description]") {
  ro::ui::DescriptionView view = CostumeView();
  view.view_id = 42;
  view.doll_possible = true;
  CHECK(DollWantedOverPicture(view));
}

TEST_CASE("Description : l'illustration d'un costume à effet seul montre le pantin",
          "[ui][description]") {
  // 🔴 IL N'A PAS DE LIGNE « VIEWID » : l'illustration est sa seule porte.
  ro::ui::DescriptionView view = CostumeView();
  view.doll_effect = true;
  CHECK(DollWantedOverPicture(view));
}

TEST_CASE("Description : ni une carte ni un objet qui ne se porte ne montrent le pantin",
          "[ui][description]") {
  ro::ui::DescriptionView plain = CostumeView();
  CHECK_FALSE(DollWantedOverPicture(plain));
  ro::ui::DescriptionView card = CostumeView();
  card.doll_effect = true;
  card.picture_is_card = true;
  CHECK_FALSE(DollWantedOverPicture(card));
}

// ── LA LIGNE « @item » DU STAFF ─────────────────────────────────────────────

namespace {

constexpr std::uint32_t kStaffSword = 1101;
constexpr int kStaffAmount = 7;
constexpr float kStaffScanStepX = 15.0F;
constexpr float kStaffScanStepY = 4.0F;
constexpr int kStaffSettleFrames = 3;

struct StaffItemProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  ro::ui::DescriptionWindowState state;
  ro::ui::DescriptionView view;

  StaffItemProbe() {
    state.open = true;
    state.item_id = kStaffSword;
    view.item_id = kStaffSword;
    view.name = "Sword";
  }

  ro::ui::DescriptionWindowAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ro::ui::DescriptionWindowAction action = ro::ui::DrawDescriptionWindow(state, view, art);
    ui.EndFrame();
    return action;
  }

  // Balaie la fenêtre jusqu'à survoler le bouton « @item ». Faux : il n'y est
  // pas.
  [[nodiscard]] bool HoverItemButton() {
    for (int frame = 0; frame < kStaffSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
    ImGuiWindow* window = ImGui::FindWindowByName("###description");
    REQUIRE(window != nullptr);
    const ImGuiID wanted = window->GetID("@item");
    for (float y = window->Pos.y; y < window->Pos.y + window->Size.y; y += kStaffScanStepY) {
      for (float x = window->Pos.x; x < window->Pos.x + window->Size.x; x += kStaffScanStepX) {
        ImGui::GetIO().AddMousePosEvent(x, y);
        static_cast<void>(Frame());
        if (GImGui->HoveredId == wanted) {
          return true;
        }
      }
    }
    return false;
  }

  // Un clic complet là où la souris se trouve ; rend l'image du relâché.
  ro::ui::DescriptionWindowAction Click() {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    static_cast<void>(Frame());
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    return Frame();
  }
};

}  // namespace

TEST_CASE("Description : sans le droit du staff, pas de ligne « @item »",
          "[ui][description][staff]") {
  StaffItemProbe probe;
  probe.view.staff_item = false;
  CHECK_FALSE(probe.HoverItemButton());
}

TEST_CASE("Description : « @item » demande l'item décrit, à la quantité retenue",
          "[ui][description][staff]") {
  StaffItemProbe probe;
  probe.view.staff_item = true;
  probe.state.staff_item_amount = kStaffAmount;
  REQUIRE(probe.HoverItemButton());
  const ro::ui::DescriptionWindowAction released = probe.Click();
  REQUIRE(released.staff_item.has_value());
  CHECK(*released.staff_item ==
        ro::ui::StaffItemGrant{.item_id = kStaffSword, .amount = kStaffAmount});
  // La quantité reste pour l'item suivant.
  CHECK(probe.state.staff_item_amount == kStaffAmount);
}

TEST_CASE("Description : la quantité du staff est bornée", "[ui][description][staff]") {
  StaffItemProbe probe;
  probe.view.staff_item = true;
  probe.state.staff_item_amount = 0;
  static_cast<void>(probe.Frame());
  CHECK(probe.state.staff_item_amount == 1);
  probe.state.staff_item_amount = ro::ui::kStaffItemMostAmount + 1;
  static_cast<void>(probe.Frame());
  CHECK(probe.state.staff_item_amount == ro::ui::kStaffItemMostAmount);
}

// ── UN ITEM NON EMPILABLE : AUTANT QUE DE CASES LIBRES ──────────────────────

namespace {

constexpr int kFreeSlots = 3;

// Le texte qu'une image de la description a rendu, bulles comprises.
[[nodiscard]] std::string RenderedText(StaffItemProbe& probe) {
  probe.ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::LogToBuffer();
  static_cast<void>(ro::ui::DrawDescriptionWindow(probe.state, probe.view, probe.art));
  std::string text = GImGui->LogBuffer.c_str();
  ImGui::LogFinish();
  probe.ui.EndFrame();
  return text;
}

// La quantité que « @item » demande, la souris déjà sur le bouton.
[[nodiscard]] int GrantedAmount(StaffItemProbe& probe) {
  REQUIRE(probe.HoverItemButton());
  const ro::ui::DescriptionWindowAction released = probe.Click();
  REQUIRE(released.staff_item.has_value());
  return released.staff_item->amount;
}

}  // namespace

TEST_CASE("Description : un item non empilable est ramené aux cases libres",
          "[ui][description][staff]") {
  StaffItemProbe probe;
  probe.view.staff_item = true;
  probe.view.stackable = false;
  probe.view.free_bag_slots = kFreeSlots;
  probe.state.staff_item_amount = kStaffAmount;
  CHECK(GrantedAmount(probe) == kFreeSlots);
  CHECK(RenderedText(probe).find("(3 au plus)") != std::string::npos);
  // Le champ garde sa quantité pour l'item suivant.
  CHECK(probe.state.staff_item_amount == kStaffAmount);
}

TEST_CASE("Description : empilable ou inconnu, la quantité passe entière",
          "[ui][description][staff]") {
  StaffItemProbe stackable;
  stackable.view.staff_item = true;
  stackable.view.stackable = true;
  stackable.view.free_bag_slots = kFreeSlots;
  stackable.state.staff_item_amount = kStaffAmount;
  CHECK(GrantedAmount(stackable) == kStaffAmount);

  StaffItemProbe unknown;
  unknown.view.staff_item = true;
  unknown.view.free_bag_slots = kFreeSlots;
  unknown.state.staff_item_amount = kStaffAmount;
  CHECK(GrantedAmount(unknown) == kStaffAmount);

  // La capacité du sac pas encore dite : rien ne borne non plus.
  StaffItemProbe unknown_bag;
  unknown_bag.view.staff_item = true;
  unknown_bag.view.stackable = false;
  unknown_bag.state.staff_item_amount = kStaffAmount;
  CHECK(GrantedAmount(unknown_bag) == kStaffAmount);
}

TEST_CASE("Description : sac plein, « @item » se grise et dit pourquoi",
          "[ui][description][staff]") {
  StaffItemProbe probe;
  probe.view.staff_item = true;
  probe.view.stackable = false;
  probe.view.free_bag_slots = 0;
  probe.state.staff_item_amount = kStaffAmount;
  REQUIRE(probe.HoverItemButton());
  CHECK(GImGui->HoveredIdIsDisabled);
  CHECK(RenderedText(probe).find("L'inventory est plein") != std::string::npos);
  CHECK_FALSE(probe.Click().staff_item.has_value());
}

TEST_CASE("Description : redemander la compétence montrée referme sa fenêtre",
          "[ui][description]") {
  // 🔴 LE GESTE QUI OUVRE EST AUSSI CELUI QUI FERME — clic droit sur la barre,
  // Ctrl + clic droit dans le grimoire. Sans la bascule, il fallait la croix.
  constexpr std::uint16_t kHealSkillId = 28;
  constexpr std::uint16_t kBlessingSkillId = 34;
  ro::ui::SkillDescriptionWindowState state;

  ro::ui::AskSkillDescription(state, kHealSkillId);
  REQUIRE(state.open);
  CHECK(state.skill_id == kHealSkillId);
  CHECK(state.place_at_mouse);

  SECTION("la même la referme") {
    ro::ui::AskSkillDescription(state, kHealSkillId);
    CHECK_FALSE(state.open);
    ro::ui::AskSkillDescription(state, kHealSkillId);
    CHECK(state.open);
  }
  SECTION("une autre la remplace et la garde ouverte") {
    state.raise = false;
    ro::ui::AskSkillDescription(state, kBlessingSkillId);
    CHECK(state.open);
    CHECK(state.skill_id == kBlessingSkillId);
    CHECK(state.raise);
  }
  SECTION("zéro ne demande rien") {
    ro::ui::AskSkillDescription(state, 0);
    CHECK(state.open);
    CHECK(state.skill_id == kHealSkillId);
  }
}
