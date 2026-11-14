// La grille de groupe : sa géométrie, ses couleurs, ses textes, et ses gestes.

#include "ro/ui/party_grid.h"

#include <array>

#include <imgui.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

using ro::ui::PartyGridAnswer;
using ro::ui::PartyGridHpCaption;
using ro::ui::PartyGridHpInk;
using ro::ui::PartyGridHpText;
using ro::ui::PartyGridHpTextOf;
using ro::ui::PartyGridLayout;
using ro::ui::PartyGridLayoutOf;
using ro::ui::PartyGridLook;
using ro::ui::PartyGridState;
using ro::ui::PartyGridTileView;
using ro::ui::PartyGridView;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

constexpr std::uint32_t kMyAccount = 2000001;
constexpr std::uint32_t kNearAccount = 2000002;
constexpr std::uint32_t kFarAccount = 2000003;
constexpr std::uint32_t kGoneAccount = 2000004;

// Moi, un membre à l'écran, un membre en jeu mais loin, un déconnecté.
struct Party {
  std::array<ro::ui::StatusCellView, 3> statuses{
      ro::ui::StatusCellView{.abbreviation = "BL", .remaining_ms = 4000, .total_ms = 8000},
      ro::ui::StatusCellView{.abbreviation = "AG"},
      ro::ui::StatusCellView{.abbreviation = "IM", .remaining_ms = 500, .total_ms = 60000},
  };
  std::array<PartyGridTileView, 4> tiles{};

  Party() {
    tiles[0] =
        PartyGridTileView{.account_id = kMyAccount,
                          .name = "Moi",
                          .job_label = "Priest",
                          .map_label = "Prontera",
                          .job_icon = ro::ui::SkinImage{.id = 1, .width = 24.0F, .height = 24.0F},
                          .base_level = 99,
                          .is_me = true,
                          .is_leader = true,
                          .in_sight = true,
                          .same_map = true,
                          .hp_known = true,
                          .hp = 800,
                          .max_hp = 1000,
                          .sp_known = true,
                          .sp = 50,
                          .max_sp = 300,
                          .statuses_known = true,
                          .statuses = statuses};
    tiles[1] = PartyGridTileView{.account_id = kNearAccount,
                                 .name = "Un nom beaucoup trop long pour sa tuile",
                                 .job_label = "Knight",
                                 .base_level = 80,
                                 .is_target = true,
                                 .in_sight = true,
                                 .same_map = true,
                                 .hp_known = true,
                                 .hp = 100,
                                 .max_hp = 1000};
    tiles[2] = PartyGridTileView{.account_id = kFarAccount, .name = "Loin", .base_level = 50};
    tiles[3] = PartyGridTileView{
        .account_id = kGoneAccount, .name = "Parti", .base_level = 10, .offline = true};
  }

  [[nodiscard]] PartyGridView View(bool leader = true) const {
    return PartyGridView{.tiles = tiles, .i_am_leader = leader};
  }
};

// Une image entière : le cadre, puis la fin de l'image.
PartyGridAnswer Frame(ro::ui::UiContext& ui, const PartyGridView& view, PartyGridLook& look,
                      PartyGridState& state, ro::ui::HudGrip& grip) {
  const ro::ui::AlignGrid grid;
  ui.BeginFrame(kViewport, kFrameSeconds);
  PartyGridAnswer answer = ro::ui::DrawPartyGrid(view, look, state, grid, grip);
  ui.EndFrame();
  return answer;
}

}  // namespace

TEST_CASE("Grille de groupe : la taille du cadre se déduit des tuiles", "[ui][party_grid]") {
  PartyGridLook look;
  look.columns = 2;
  const PartyGridLayout layout = PartyGridLayoutOf(look, 5, 1.0F);
  CHECK(layout.columns == 2);
  CHECK(layout.rows == 3);
  // Deux tuiles de 180, un écart de 2, une marge de 3 de chaque côté.
  CHECK(layout.frame_width == 2.0F * 180.0F + 2.0F + 6.0F);
  CHECK(layout.frame_height == 3.0F * 34.0F + 2.0F * 2.0F + 6.0F);

  // La tuile de rang 3 tombe en deuxième colonne, deuxième rangée.
  const ImVec2 at = ro::ui::PartyGridTileAt(layout, ImVec2(100.0F, 50.0F), 3);
  CHECK(at.x == 100.0F + 3.0F + 182.0F);
  CHECK(at.y == 50.0F + 3.0F + 36.0F);

  // Jamais plus de colonnes que de tuiles.
  look.columns = 3;
  CHECK(PartyGridLayoutOf(look, 2, 1.0F).columns == 2);

  // L'échelle d'interface multiplie tout.
  look.columns = 1;
  const PartyGridLayout doubled = PartyGridLayoutOf(look, 1, 2.0F);
  CHECK(doubled.frame_width == 2.0F * (180.0F + 6.0F));
  CHECK(doubled.frame_height == 2.0F * (34.0F + 6.0F));
}

TEST_CASE("Grille de groupe : un réglage hors bornes est ramené dedans", "[ui][party_grid]") {
  PartyGridLook look;
  look.columns = 40;
  look.tile_width_px = 1;
  look.tile_height_px = 5000;
  look.gap_px = -3;
  const PartyGridLayout layout = PartyGridLayoutOf(look, 12, 1.0F);
  CHECK(layout.columns == ro::ui::kPartyGridMostColumns);
  CHECK(layout.tile_width == static_cast<float>(ro::ui::kPartyGridNarrowestTilePixels));
  CHECK(layout.tile_height == static_cast<float>(ro::ui::kPartyGridTallestTilePixels));
  CHECK(layout.gap == 0.0F);
}

TEST_CASE("Grille de groupe : l'opacité des icônes lue du fichier est bornée", "[ui][party_grid]") {
  // Un fichier écrit à la main ne rend pas les icônes invisibles, ni plus
  // qu'opaques. Les deux surfaces du groupe lisent la même borne.
  using ro::ui::StatusOpacityOf;
  CHECK(StatusOpacityOf(0) == StatusOpacityOf(ro::ui::kFaintestStatusOpacityPercent));
  CHECK(StatusOpacityOf(ro::ui::kFaintestStatusOpacityPercent) > 0.0F);
  CHECK(StatusOpacityOf(ro::ui::kFullStatusOpacityPercent * 2) == 1.0F);
  CHECK(StatusOpacityOf(ro::ui::kFullStatusOpacityPercent) == 1.0F);
  CHECK(StatusOpacityOf(ro::ui::kFullStatusOpacityPercent / 2) == 0.5F);
  // Par défaut, rien ne change à l'écran.
  CHECK(PartyGridLook{}.status_opacity_percent == ro::ui::kFullStatusOpacityPercent);
  CHECK(PartyGridLook{}.status_countdown_auto);
  // Le fond d'icône par défaut est noir et opaque ; le voile,
  // celui de la barre du joueur.
  CHECK(PartyGridLook{}.status_icon_background == ro::ui::kDefaultStatusIconBackground);
  CHECK(PartyGridLook{}.status_sweep_color == ro::ui::kDefaultStatusSweepColor);
}

TEST_CASE("Grille de groupe : tirer le cadre change le nombre de colonnes", "[ui][party_grid]") {
  PartyGridLook look;
  look.columns = 2;
  const PartyGridLayout layout = PartyGridLayoutOf(look, 6, 1.0F);
  CHECK(ro::ui::PartyGridColumnsForWidth(layout, layout.frame_width) == 2);
  CHECK(ro::ui::PartyGridColumnsForWidth(layout, layout.frame_width + 182.0F) == 3);
  // Au-delà de la moitié d'une tuile, on arrondit à la colonne suivante.
  CHECK(ro::ui::PartyGridColumnsForWidth(layout, layout.frame_width + 100.0F) == 3);
  CHECK(ro::ui::PartyGridColumnsForWidth(layout, layout.frame_width + 80.0F) == 2);
  CHECK(ro::ui::PartyGridColumnsForWidth(layout, 5000.0F) == ro::ui::kPartyGridMostColumns);
  CHECK(ro::ui::PartyGridColumnsForWidth(layout, 10.0F) == ro::ui::kPartyGridFewestColumns);
}

TEST_CASE("Grille de groupe : la couleur de la vie suit les deux seuils", "[ui][party_grid]") {
  const PartyGridLook look;
  CHECK(PartyGridHpInk(look, 1000, 1000) == look.hp_high);
  CHECK(PartyGridHpInk(look, 560, 1000) == look.hp_high);
  // Le seuil est inclusif : à 55 % pile, la tuile passe au jaune.
  CHECK(PartyGridHpInk(look, 550, 1000) == look.hp_mid);
  CHECK(PartyGridHpInk(look, 260, 1000) == look.hp_mid);
  CHECK(PartyGridHpInk(look, 250, 1000) == look.hp_low);
  CHECK(PartyGridHpInk(look, 0, 1000) == look.hp_low);
}

TEST_CASE("Grille de groupe : les PV s'écrivent selon le mode", "[ui][party_grid]") {
  CHECK(PartyGridHpCaption(PartyGridHpText::kNone, 450, 1000).empty());
  CHECK(PartyGridHpCaption(PartyGridHpText::kNumbers, 450, 1000) == "450/1000");
  CHECK(PartyGridHpCaption(PartyGridHpText::kPercent, 450, 1000) == "45 %");
  CHECK(PartyGridHpCaption(PartyGridHpText::kBoth, 450, 1000) == "450/1000 (45 %)");
  // Un point de vie sur mille : 1 %, jamais 0 % sur un vivant.
  CHECK(PartyGridHpCaption(PartyGridHpText::kPercent, 1, 1000) == "1 %");
  CHECK(PartyGridHpCaption(PartyGridHpText::kPercent, 0, 1000) == "0 %");

  CHECK(PartyGridHpTextOf(-1) == PartyGridHpText::kNone);
  CHECK(PartyGridHpTextOf(2) == PartyGridHpText::kPercent);
  CHECK(PartyGridHpTextOf(9) == PartyGridHpText::kBoth);
}

TEST_CASE("Grille de groupe : éteinte ou vide, elle ne dessine rien", "[ui][party_grid]") {
  ro::ui::UiContext ui;
  PartyGridState state;
  ro::ui::HudGrip grip;
  const Party party;

  PartyGridLook look;
  const PartyGridAnswer off = Frame(ui, party.View(), look, state, grip);
  CHECK_FALSE(off.moved);
  CHECK(off.hovered_account_id == 0);

  look.shown = true;
  const PartyGridAnswer empty = Frame(ui, PartyGridView{}, look, state, grip);
  CHECK_FALSE(empty.moved);
  CHECK(empty.verb == ro::ui::PartyGridVerb::kNone);
}

TEST_CASE("Grille de groupe : pleine, le cadre prend la taille de ses tuiles", "[ui][party_grid]") {
  ro::ui::UiContext ui;
  PartyGridState state;
  ro::ui::HudGrip grip;
  const Party party;

  PartyGridLook look;
  look.shown = true;
  look.columns = 2;
  look.show_level = true;
  look.show_tooltip = true;
  look.hp_text = static_cast<int>(PartyGridHpText::kBoth);
  static_cast<void>(Frame(ui, party.View(), look, state, grip));
  PartyGridLayout layout = PartyGridLayoutOf(look, party.tiles.size(), 1.0F);
  CHECK(look.rect.width == layout.frame_width);
  CHECK(look.rect.height == layout.frame_height);

  // Sans moi ni les déconnectés, deux tuiles : une seule rangée.
  look.show_self = false;
  look.show_offline = false;
  static_cast<void>(Frame(ui, party.View(), look, state, grip));
  layout = PartyGridLayoutOf(look, 2, 1.0F);
  CHECK(layout.rows == 1);
  CHECK(look.rect.height == layout.frame_height);
}

TEST_CASE("Grille de groupe : la tuile survolée est proposée au sort de soutien",
          "[ui][party_grid]") {
  ro::ui::UiContext ui;
  PartyGridState state;
  ro::ui::HudGrip grip;
  const Party party;

  PartyGridLook look;
  look.shown = true;
  look.locked = true;
  look.rect.x = 100.0F;
  look.rect.y = 100.0F;
  const PartyGridLayout layout = PartyGridLayoutOf(look, party.tiles.size(), 1.0F);
  const auto middle_of = [&layout, &look](std::size_t rank) {
    const ImVec2 at = ro::ui::PartyGridTileAt(layout, ImVec2(look.rect.x, look.rect.y), rank);
    return ImVec2(at.x + layout.tile_width / 2.0F, at.y + layout.tile_height / 2.0F);
  };

  ImVec2 point = middle_of(1);
  ui.SetPointer(point.x, point.y, false);
  PartyGridAnswer answer = Frame(ui, party.View(), look, state, grip);
  CHECK(answer.hovered_account_id == kNearAccount);
  CHECK(answer.cast_account_id == kNearAccount);

  // Un membre sans acteur à l'écran : survolé, mais aucun sort ne partirait sur
  // personne.
  point = middle_of(2);
  ui.SetPointer(point.x, point.y, false);
  answer = Frame(ui, party.View(), look, state, grip);
  CHECK(answer.hovered_account_id == kFarAccount);
  CHECK(answer.cast_account_id == 0);

  look.cast_on_tile = false;
  point = middle_of(0);
  ui.SetPointer(point.x, point.y, false);
  answer = Frame(ui, party.View(), look, state, grip);
  CHECK(answer.hovered_account_id == kMyAccount);
  CHECK(answer.cast_account_id == 0);
}

TEST_CASE("Grille de groupe : cliquable, le clic gauche cible et le droit ouvre un menu",
          "[ui][party_grid]") {
  ro::ui::UiContext ui;
  PartyGridState state;
  ro::ui::HudGrip grip;
  const Party party;

  PartyGridLook look;
  look.shown = true;
  look.locked = true;
  look.clickable = true;
  look.rect.x = 100.0F;
  look.rect.y = 100.0F;
  const PartyGridLayout layout = PartyGridLayoutOf(look, party.tiles.size(), 1.0F);
  const auto middle_of = [&layout, &look](std::size_t rank) {
    const ImVec2 at = ro::ui::PartyGridTileAt(layout, ImVec2(look.rect.x, look.rect.y), rank);
    return ImVec2(at.x + layout.tile_width / 2.0F, at.y + layout.tile_height / 2.0F);
  };

  // Deux images pour que la fenêtre existe et soit survolée, puis l'appui.
  ImVec2 point = middle_of(1);
  ui.SetPointer(point.x, point.y, false);
  static_cast<void>(Frame(ui, party.View(), look, state, grip));
  static_cast<void>(Frame(ui, party.View(), look, state, grip));
  ui.SetPointer(point.x, point.y, true);
  PartyGridAnswer answer = Frame(ui, party.View(), look, state, grip);
  CHECK(answer.target_account_id == kNearAccount);
  ui.SetPointer(point.x, point.y, false);
  static_cast<void>(Frame(ui, party.View(), look, state, grip));

  // Clic droit sur un membre à l'écran : le menu d'entité, par `app`.
  ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Right, true);
  answer = Frame(ui, party.View(), look, state, grip);
  CHECK(answer.verb == ro::ui::PartyGridVerb::kEntityMenu);
  CHECK(answer.verb_account_id == kNearAccount);
  ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Right, false);
  static_cast<void>(Frame(ui, party.View(), look, state, grip));

  // Sur ma propre tuile, aucun geste n'est offert : pas de menu vide.
  point = middle_of(0);
  ui.SetPointer(point.x, point.y, false);
  static_cast<void>(Frame(ui, party.View(), look, state, grip));
  ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Right, true);
  answer = Frame(ui, party.View(), look, state, grip);
  CHECK(answer.verb == ro::ui::PartyGridVerb::kNone);
  CHECK_FALSE(state.open_menu);
  CHECK(state.menu_account_id == 0);
  ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Right, false);
  static_cast<void>(Frame(ui, party.View(), look, state, grip));

  // Sur un déconnecté, le menu de repli, figé au clic ; le chef peut expulser.
  point = middle_of(3);
  ui.SetPointer(point.x, point.y, false);
  static_cast<void>(Frame(ui, party.View(), look, state, grip));
  ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Right, true);
  static_cast<void>(Frame(ui, party.View(), look, state, grip));
  CHECK(state.menu_account_id == kGoneAccount);
  CHECK(state.menu_offline);
  CHECK(state.menu_name == "Parti");
}
