// LES GRILLES DE L'ÉDITEUR DE STYLE : leur géométrie, et ce qu'elles
// demandent à composer.
//
// ⚠ ÉPROUVÉES SUR DEUX RÈGLES QUI NE SE SIGNALENT PAS. Une grille trop étroite
// laisse son ascenseur mordre la dernière colonne, et le contenu est
// simplement coupé à droite — on croit à un défaut de sprite. Une grille qui
// demanderait toutes ses cases au lieu des visibles composerait deux cent
// cinquante et une vignettes pour en montrer cent soixante, et cela ne se voit
// que sur un profileur.

#include "ro/ui/palette_editor.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/palette_effects.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/ui_context.h"

using ro::ui::PaletteChoiceBounds;
using ro::ui::PaletteEditorPicker;
using ro::ui::PaletteEditorState;
using ro::ui::PaletteEditorView;
using ro::ui::PaletteTab;
using ro::ui::PaletteTabBlock;
using ro::ui::PaletteTabKind;
using ro::ui::ThumbnailGridHeight;
using ro::ui::ThumbnailGridScrolls;
using ro::ui::ThumbnailGridWidth;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

// Les trois grilles, telles que le serveur les borne.
constexpr int kHairStyles = 80;
constexpr int kHairColors = 251;
constexpr int kBodyPalettes = 553;
constexpr int kHairColumns = 8;
constexpr int kHairColorColumns = 16;
constexpr int kVisibleRows = 10;
constexpr float kCell = 34.0F;

[[nodiscard]] PaletteChoiceBounds Bounds() {
  return PaletteChoiceBounds{.first = 1,
                             .hair_style = kHairStyles,
                             .hair_palette = kHairColors,
                             .body_palette = kBodyPalettes};
}

// Ce qu'il faut d'images à la barre d'onglets pour se poser : une sélection
// demandée prend effet à l'image suivante, et un onglet qui n'est plus soumis
// n'est retiré qu'à celle d'après.
constexpr int kSettleFrames = 4;

// Dessine l'éditeur comme le fait `app` : la vue montre l'onglet que la
// fenêtre a retenu à l'image précédente, ou le corps s'il n'est pas réglable.
void DrawFrames(ro::ui::UiContext& ui, PaletteEditorState& state, PaletteEditorView& view,
                const ro::ui::SkinArt& art) {
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    const auto asked = std::ranges::find(view.tabs, state.tab, &PaletteTab::kind);
    view.tab = asked != view.tabs.end() && asked->blocked == PaletteTabBlock::kNone
                   ? state.tab
                   : PaletteTabKind::kBody;
    ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawPaletteEditor(state, view, art));
    ui.EndFrame();
  }
}

}  // namespace

TEST_CASE("Grille : elle défile dès qu'elle déborde de ses lignes", "[ui][palette]") {
  // Quatre-vingts coiffures sur huit colonnes font dix lignes : tout juste ce
  // qui tient, donc pas d'ascenseur.
  CHECK_FALSE(ThumbnailGridScrolls(kHairStyles, kHairColumns, kVisibleRows));
  // Une de plus, et il en faut un.
  CHECK(ThumbnailGridScrolls(kHairStyles + 1, kHairColumns, kVisibleRows));
  // Deux cent cinquante et une couleurs sur seize colonnes font seize lignes.
  CHECK(ThumbnailGridScrolls(kHairColors, kHairColorColumns, kVisibleRows));
}

TEST_CASE("Grille : sa largeur compte l'ascenseur", "[ui][palette]") {
  // 🔴 LA RÈGLE QUI NE SE SIGNALE PAS. Sans cette place, l'ascenseur mord sur
  // la dernière colonne et rogne ses vignettes — rien ne le dit, le contenu
  // est seulement coupé à droite.
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::Begin("essai");
  const float bare = ThumbnailGridWidth(kHairColumns, kCell, /*with_scrollbar=*/false);
  const float scrolling = ThumbnailGridWidth(kHairColumns, kCell, /*with_scrollbar=*/true);
  CHECK(scrolling > bare);
  CHECK(scrolling - bare == ImGui::GetStyle().ScrollbarSize);
  // Et elle tient ses colonnes en entier.
  CHECK(bare >= static_cast<float>(kHairColumns) * kCell);
  ImGui::End();
  ui.EndFrame();
}

TEST_CASE("Grille : sa hauteur se borne aux lignes visibles", "[ui][palette]") {
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::Begin("essai");
  // Deux cent cinquante et une couleurs feraient seize lignes ; on en montre
  // dix, et la hauteur ne dépasse pas celle de dix.
  const float capped = ThumbnailGridHeight(kHairColors, kHairColorColumns, kCell, kVisibleRows);
  const float ten_rows =
      ThumbnailGridHeight(kVisibleRows * kHairColorColumns, kHairColorColumns, kCell, kVisibleRows);
  CHECK(capped == ten_rows);
  // Une grille plus courte que le plafond, elle, garde sa taille.
  const float three_rows =
      ThumbnailGridHeight(3 * kHairColorColumns, kHairColorColumns, kCell, kVisibleRows);
  CHECK(three_rows < capped);
  ImGui::End();
  ui.EndFrame();
}

TEST_CASE("Éditeur : sans grille ouverte, rien n'est demandé à composer", "[ui][palette]") {
  // ⚠ LA REMISE À « AUCUNE » EST CE QUI REND LES TEXTURES : une grille
  // refermée qui continuerait de se déclarer ouverte garderait ses vignettes
  // pour toute la session.
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  PaletteEditorState state;
  state.open = true;
  state.picker.open = PaletteEditorPicker::kHairStyle;
  PaletteEditorView view;
  view.bounds = Bounds();

  ui.BeginFrame(kViewport, kFrameSeconds);
  static_cast<void>(ro::ui::DrawPaletteEditor(state, view, art));
  ui.EndFrame();

  CHECK(state.picker.open == PaletteEditorPicker::kNone);
}

TEST_CASE("Éditeur : un onglet dont le libellé change reste ouvert", "[ui][palette]") {
  // 🔴 LE LIBELLÉ D'UN ONGLET BOUGE PENDANT QU'ON Y TRAVAILLE : une étoile dès
  // la première retouche, un autre nom quand on change de chapeau. Si la barre
  // y voyait un nouvel onglet, elle quitterait celui qu'on règle au premier
  // clic sur une couleur.
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  std::array<PaletteTab, 3> tabs{
      PaletteTab{.kind = PaletteTabKind::kBody}, PaletteTab{.kind = PaletteTabKind::kHair},
      PaletteTab{.kind = PaletteTabKind::kHeadgearTop, .name = "Valkyrie", .item = true}};
  PaletteEditorState state;
  state.open = true;
  PaletteEditorView view;
  view.bounds = Bounds();
  view.tabs = tabs;

  SECTION("l'étoile d'une retouche") {
    state.tab = PaletteTabKind::kHair;
    state.tab_asked = true;
    DrawFrames(ui, state, view, art);
    REQUIRE(state.tab == PaletteTabKind::kHair);

    tabs[1].edited = true;
    DrawFrames(ui, state, view, art);
    CHECK(state.tab == PaletteTabKind::kHair);

    // Et l'étoile qui s'en va, une fois la retouche validée.
    tabs[1].edited = false;
    DrawFrames(ui, state, view, art);
    CHECK(state.tab == PaletteTabKind::kHair);
  }

  SECTION("un autre objet à la même place") {
    state.tab = PaletteTabKind::kHeadgearTop;
    state.tab_asked = true;
    DrawFrames(ui, state, view, art);
    REQUIRE(state.tab == PaletteTabKind::kHeadgearTop);

    tabs[2].name = "Chapeau de paille";
    DrawFrames(ui, state, view, art);
    CHECK(state.tab == PaletteTabKind::kHeadgearTop);
  }
}

TEST_CASE("Éditeur : un onglet ouvert qui se bloque rend la main au corps", "[ui][palette]") {
  // La vue retombe sur le corps dès que le calque n'est plus réglable ; la
  // barre doit la suivre, sans quoi elle désigne un calque pendant que la
  // liste est celle d'un autre.
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  std::array<PaletteTab, 2> tabs{PaletteTab{.kind = PaletteTabKind::kBody},
                                 PaletteTab{.kind = PaletteTabKind::kHair}};
  PaletteEditorState state;
  state.open = true;
  state.tab = PaletteTabKind::kHair;
  state.tab_asked = true;
  PaletteEditorView view;
  view.bounds = Bounds();
  view.tabs = tabs;
  DrawFrames(ui, state, view, art);
  REQUIRE(state.tab == PaletteTabKind::kHair);

  tabs[1].blocked = PaletteTabBlock::kOrcHead;
  DrawFrames(ui, state, view, art);
  CHECK(state.tab == PaletteTabKind::kBody);

  // ⚠ ET LA BARRE A VRAIMENT CHANGÉ D'ONGLET : débloqué, celui des cheveux ne
  // se rouvre pas tout seul.
  tabs[1].blocked = PaletteTabBlock::kNone;
  DrawFrames(ui, state, view, art);
  CHECK(state.tab == PaletteTabKind::kBody);
}

TEST_CASE("Éditeur : les compagnons ont leur onglet, en queue, sous leur nom", "[ui][palette]") {
  // ⚠ LES NUMÉROS VOYAGENT : l'homoncule et le familier viennent APRÈS la cape,
  // et le styliste les désigne par 8 et 9.
  CHECK(static_cast<int>(PaletteTabKind::kHomunculus) > static_cast<int>(PaletteTabKind::kGarment));
  CHECK(ro::ui::PaletteTabOfNumber(8) == PaletteTabKind::kHomunculus);
  CHECK(ro::ui::PaletteTabOfNumber(9) == PaletteTabKind::kPet);
  CHECK(std::string_view(ro::ui::PaletteTabName(PaletteTabKind::kPet)) == "Familier");

  // « Couleurs » de la fiche du familier ouvre son onglet, et il y reste quand
  // son nom change.
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  std::array<PaletteTab, 3> tabs{
      PaletteTab{.kind = PaletteTabKind::kBody},
      PaletteTab{.kind = PaletteTabKind::kHomunculus, .name = "Lif", .companion = true},
      PaletteTab{
          .kind = PaletteTabKind::kPet, .name = "Poring", .companion = true, .mostly_grey = true}};
  PaletteEditorState state;
  state.open = true;
  state.tab = PaletteTabKind::kPet;
  state.tab_asked = true;
  PaletteEditorView view;
  view.bounds = Bounds();
  view.tabs = tabs;
  DrawFrames(ui, state, view, art);
  REQUIRE(state.tab == PaletteTabKind::kPet);
  tabs[2].name = "Gluant";
  DrawFrames(ui, state, view, art);
  CHECK(state.tab == PaletteTabKind::kPet);

  // Un serveur qui ne range pas les compagnons bloque l'onglet : la main
  // revient au corps.
  tabs[2].blocked = PaletteTabBlock::kCompanionNotServed;
  DrawFrames(ui, state, view, art);
  CHECK(state.tab == PaletteTabKind::kBody);
}

TEST_CASE("Éditeur : sans bornes, aucune grille n'est proposée", "[ui][palette]") {
  // Les bornes viennent du serveur, par `app`. Tant qu'on ne les a pas, un
  // bouton « Coiffure (0) » n'ouvrirait qu'une grille vide.
  PaletteChoiceBounds none;
  CHECK_FALSE(none.usable());
  CHECK(Bounds().usable());
}

TEST_CASE("Éditeur : une pièce se nomme par sa couleur d'origine", "[ui][palette]") {
  using ro::ui::PieceColorName;
  CHECK(std::string_view(PieceColorName(IM_COL32(200, 30, 30, 255))) == "rouge");
  CHECK(std::string_view(PieceColorName(IM_COL32(40, 60, 210, 255))) == "bleu");
  CHECK(std::string_view(PieceColorName(IM_COL32(40, 170, 60, 255))) == "vert");
  CHECK(std::string_view(PieceColorName(IM_COL32(120, 70, 30, 255))) == "brun");
  CHECK(std::string_view(PieceColorName(IM_COL32(128, 128, 128, 255))) == "gris");
  CHECK(std::string_view(PieceColorName(IM_COL32(250, 250, 248, 255))) == "blanc");
  CHECK(std::string_view(PieceColorName(IM_COL32(15, 12, 20, 255))) == "noir");
}

// ── LES DEUX PEAUX ───────────────────────────────────────────────────────────

namespace {

// Le seul surgissant que ces essais peuvent ouvrir : le nuancier de la peau.
[[nodiscard]] bool SkinPickerOpen() { return !ImGui::GetCurrentContext()->OpenPopupStack.empty(); }

}  // namespace

TEST_CASE("Éditeur : plus d'onglet Peau, la Tête le remplace", "[ui][palette][skin]") {
  using ro::ui::PaletteTabName;
  using ro::ui::PaletteTabOfNumber;
  for (std::uint8_t number = 1; number <= ro::ui::kPaletteTabKinds; ++number) {
    const std::optional<PaletteTabKind> tab = PaletteTabOfNumber(number);
    REQUIRE(tab.has_value());
    CHECK(std::string_view(PaletteTabName(*tab)) != "Peau");
    CHECK_FALSE(std::string_view(PaletteTabName(*tab)).empty());
  }
  // Le styliste qui demande l'ancien onglet de la peau ouvre la Tête.
  constexpr std::uint8_t kRetiredSkinNumber = 2;
  constexpr std::uint8_t kHairNumber = 3;
  CHECK(PaletteTabOfNumber(kRetiredSkinNumber) == PaletteTabKind::kHair);
  CHECK(PaletteTabOfNumber(kHairNumber) == PaletteTabKind::kHair);
  CHECK(std::string_view(PaletteTabName(PaletteTabKind::kHair)) == "Tête");
  CHECK_FALSE(PaletteTabOfNumber(0).has_value());
  CHECK_FALSE(PaletteTabOfNumber(ro::ui::kPaletteTabKinds + 1).has_value());
}

TEST_CASE("Éditeur : la pipette ouvre le nuancier de la ligne de peau de son onglet",
          "[ui][palette][skin]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const std::array<PaletteTab, 2> tabs{PaletteTab{.kind = PaletteTabKind::kBody},
                                       PaletteTab{.kind = PaletteTabKind::kHair}};
  PaletteEditorState state;
  state.open = true;
  PaletteEditorView view;
  view.bounds = Bounds();
  view.tabs = tabs;
  view.skin.shown = true;
  view.skin.color = IM_COL32(230, 180, 150, 255);

  SECTION("le visage, sous la Tête") {
    state.tab = PaletteTabKind::kHair;
    state.tab_asked = true;
    state.skin_open = PaletteTabKind::kHair;
    DrawFrames(ui, state, view, art);
    CHECK(SkinPickerOpen());
    CHECK_FALSE(state.skin_open.has_value());
    // Le pantin montre alors visage et bras ensemble.
    CHECK(state.skin_focus);
  }
  SECTION("les bras, sous le Corps") {
    view.skin.body = true;
    state.skin_open = PaletteTabKind::kBody;
    DrawFrames(ui, state, view, art);
    CHECK(SkinPickerOpen());
    CHECK(state.skin_focus);
  }
  SECTION("la demande attend la vue de son onglet") {
    // La pipette a visé le visage ; la vue est encore celle du Corps.
    view.skin.body = true;
    state.skin_open = PaletteTabKind::kHair;
    DrawFrames(ui, state, view, art);
    CHECK_FALSE(SkinPickerOpen());
    CHECK(state.skin_open == PaletteTabKind::kHair);
  }
  SECTION("une ligne bloquée ne s'ouvre pas, et la demande tombe") {
    view.skin.blocked = PaletteTabBlock::kSkinOnMount;
    state.tab = PaletteTabKind::kHair;
    state.tab_asked = true;
    state.skin_open = PaletteTabKind::kHair;
    DrawFrames(ui, state, view, art);
    CHECK_FALSE(SkinPickerOpen());
    CHECK_FALSE(state.skin_open.has_value());
  }
  SECTION("un Doram n'a pas de ligne") {
    view.skin.shown = false;
    state.tab = PaletteTabKind::kHair;
    state.tab_asked = true;
    state.skin_open = PaletteTabKind::kHair;
    DrawFrames(ui, state, view, art);
    CHECK_FALSE(SkinPickerOpen());
    CHECK_FALSE(state.skin_focus);
  }
}

TEST_CASE("Éditeur : l'onglet des effets se dessine, en queue, et y reste",
          "[ui][palette][effects]") {
  // ⚠ IL VIENT APRÈS LE FAMILIER, et le styliste le désigne par 10.
  CHECK(static_cast<int>(PaletteTabKind::kEffects) > static_cast<int>(PaletteTabKind::kPet));
  CHECK(ro::ui::PaletteTabOfNumber(10) == PaletteTabKind::kEffects);
  CHECK(std::string_view(ro::ui::PaletteTabName(PaletteTabKind::kEffects)) == "Effets");

  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const std::array<PaletteTab, 2> tabs{PaletteTab{.kind = PaletteTabKind::kBody},
                                       PaletteTab{.kind = PaletteTabKind::kEffects}};
  constexpr std::uint16_t kGlow = 20;
  constexpr std::uint16_t kShrink = 17;
  const std::array<ro::ui::PaletteEffectRow, 2> rows{
      ro::ui::PaletteEffectRow{
          .number = kShrink, .name = "Shrink", .blocked = ro::ui::PaletteEffectBlock::kChangesBody},
      ro::ui::PaletteEffectRow{.number = kGlow,
                               .name = "Glow",
                               .additive = true,
                               .base_color = std::array<std::uint8_t, 3>{230, 200, 40}}};
  const ro::ui::PaletteEffectsView effects{
      .rows = rows, .connected = true, .served = true, .atelier_open = true, .can_send = true};
  PaletteEditorState state;
  state.open = true;
  state.tab = PaletteTabKind::kEffects;
  state.tab_asked = true;
  state.effects.drafts[kGlow] = ro::ui::PaletteEffectSetting{.hue = 120, .darken = true};
  PaletteEditorView view;
  view.bounds = Bounds();
  view.tabs = tabs;
  view.effects = &effects;
  DrawFrames(ui, state, view, art);
  CHECK(state.tab == PaletteTabKind::kEffects);
  CHECK(view.tab == PaletteTabKind::kEffects);
  // Le réglage en cours survit au dessin, et rien ne part sans clic.
  CHECK(state.effects.drafts.at(kGlow).hue == 120);
  CHECK(state.effects.drafts.at(kGlow).darken);
  // Le pantin se dessine à côté de l'onglet, et sa pipette n'y désigne aucune pièce.
  CHECK(state.doll.box_width > 0.0F);
  CHECK(state.hovered_ramp == -1);
}

TEST_CASE("Éditeur : une fermeture demandée d'ailleurs attend un style validé", "[ui][palette]") {
  // 🔴 LA FICHE ET LE STYLISTE FERMENT COMME LA CROIX : un style non validé
  // demande confirmation au lieu de se perdre.
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  PaletteEditorState state;
  PaletteEditorView view;
  view.bounds = Bounds();

  ro::ui::TogglePaletteEditor(state);
  CHECK(state.open);
  view.has_something_to_send = true;
  DrawFrames(ui, state, view, art);
  ro::ui::TogglePaletteEditor(state);
  DrawFrames(ui, state, view, art);
  CHECK(state.open);
  CHECK_FALSE(state.close_asked);

  // Rien à valider : la fermeture se fait tout de suite.
  view.has_something_to_send = false;
  ro::ui::ClosePaletteEditor(state);
  DrawFrames(ui, state, view, art);
  CHECK_FALSE(state.open);

  // Fermer un éditeur déjà fermé ne laisse aucune demande en suspens.
  ro::ui::ClosePaletteEditor(state);
  CHECK_FALSE(state.close_asked);
}
