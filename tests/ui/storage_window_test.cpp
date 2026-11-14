#include "ro/ui/storage_window.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/inventory_window.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::InventoryCellView;
using ro::ui::ItemTab;
using ro::ui::ItemTabPiece;
using ro::ui::ItemTabPieceLying;
using ro::ui::kItemTabCount;
using ro::ui::StorageColumn;
using ro::ui::StorageColumnLabel;
using ro::ui::StorageRowsFingerprint;
using ro::ui::StorageTabs;

namespace {

InventoryCellView CellOf(std::uint16_t slot, std::uint32_t item_id, int amount) {
  InventoryCellView cell;
  cell.client_slot = slot;
  cell.item_id = item_id;
  cell.amount = amount;
  return cell;
}

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

// Les mesures du client, pour les trois familles de pièces dont la HAUTEUR
// décide d'une disposition : la barre de titre, le bouton et le petit bouton.
// Le reste peut être carré — aucune de ces mesures-ci n'en dépend.
constexpr float kTitleBarHeightPixels = 19.0F;
constexpr float kButtonHeightPixels = 20.0F;
constexpr float kSmallButtonHeightPixels = 15.0F;
constexpr float kCapPixels = 6.0F;
constexpr float kPlainPiecePixels = 12.0F;

// UN HABILLAGE COMPLET. Ce n'est pas l'art qu'on mesure mais la DISPOSITION, et
// elle ne se produit que sur le chemin habillé : sans barre de titre, la fenêtre
// retombe sur celle d'ImGui et le pied du coffre ne se dessine plus pareil.
[[nodiscard]] ro::ui::SkinArt WholeArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  const auto set = [&art](SkinPiece piece, float width, float height) {
    art.Set(piece, ro::ui::SkinImage{.id = 1, .width = width, .height = height});
  };
  for (const SkinPiece piece :
       {SkinPiece::kTitleBarLeft, SkinPiece::kTitleBarMiddle, SkinPiece::kTitleBarRight}) {
    set(piece, kCapPixels, kTitleBarHeightPixels);
  }
  for (const SkinPiece piece :
       {SkinPiece::kButtonLeft, SkinPiece::kButtonMiddle, SkinPiece::kButtonRight,
        SkinPiece::kButtonHoverLeft, SkinPiece::kButtonHoverMiddle, SkinPiece::kButtonHoverRight,
        SkinPiece::kButtonPressedLeft, SkinPiece::kButtonPressedMiddle,
        SkinPiece::kButtonPressedRight}) {
    set(piece, kCapPixels, kButtonHeightPixels);
  }
  for (const SkinPiece piece :
       {SkinPiece::kSmallButtonLeft, SkinPiece::kSmallButtonMiddle, SkinPiece::kSmallButtonRight,
        SkinPiece::kSmallButtonHoverLeft, SkinPiece::kSmallButtonHoverMiddle,
        SkinPiece::kSmallButtonHoverRight, SkinPiece::kSmallButtonPressedLeft,
        SkinPiece::kSmallButtonPressedMiddle, SkinPiece::kSmallButtonPressedRight}) {
    set(piece, kCapPixels, kSmallButtonHeightPixels);
  }
  return art;
}

// Un coffre bien rempli : de quoi remplir la table au-delà de ce qu'elle montre.
[[nodiscard]] std::vector<InventoryCellView> WellStocked(int count) {
  std::vector<InventoryCellView> cells;
  cells.reserve(static_cast<std::size_t>(count));
  for (int rank = 0; rank < count; ++rank) {
    InventoryCellView cell =
        CellOf(static_cast<std::uint16_t>(rank + 1), static_cast<std::uint32_t>(500 + rank), 1);
    cell.name = "Objet";
    cells.push_back(cell);
  }
  return cells;
}

}  // namespace

TEST_CASE("Coffre : DIX onglets, et ce ne sont pas les sept du sac", "[ui][coffre]") {
  // 🔴 LE FAVORI DU COFFRE TIENT À UN FICHIER DU JOUEUR. Le serveur n'a aucun
  // drapeau de favori dans un coffre, contrairement au sac : sans un endroit où
  // retenir la liste, l'onglet se viderait à chaque démarrage.
  //
  // ⚠ LA CHARRETTE, ELLE, N'EN A PAS : elle n'a rien où retenir non
  // plus, et surtout aucun onglet où montrer.
  const auto tabs = StorageTabs();
  CHECK(tabs.size() == 10);
  CHECK(std::ranges::find(tabs, ItemTab::kFavorite) != tabs.end());

  // 🔴 LES QUATRE QU'IL EST SEUL À MONTRER. C'est la fenêtre où l'on CHERCHE :
  // « Équipement » y mêlerait les dagues, les armures et les accessoires devant
  // six cents lignes oubliées. Il les sépare, et sort le cash des consommables.
  for (const ItemTab wanted :
       {ItemTab::kWeapon, ItemTab::kArmor, ItemTab::kCostume, ItemTab::kCash}) {
    CHECK(std::ranges::count(tabs, wanted) == 1);
  }
  // ⚠ ET IL N'A PAS D'ONGLET « ÉQUIPEMENT » : les trois qui le remplacent le
  // couvrent en entier, et le garder ferait voir chaque pièce deux fois.
  CHECK(std::ranges::find(tabs, ItemTab::kEquipment) == tabs.end());

  // L'ordre commence par les deux qui ne filtrent
  // pas par type.
  CHECK(tabs.front() == ItemTab::kAll);
  CHECK(tabs[1] == ItemTab::kFavorite);
}

TEST_CASE("Coffre : les onglets couches ne sont pas les onglets debout",
          "[ui][coffre][habillage]") {
  // ⚠ CE N'EST PAS LA MEME IMAGE TOURNEE : l'installation en livre un SECOND jeu,
  // dessine pour une rangee (27x25 la ou le jeu debout fait 23x27), dont le bord
  // ouvert n'est pas du meme cote. Faire pivoter le premier ouvrirait l'onglet
  // sur le vide.
  for (std::size_t rank = 0; rank < kItemTabCount; ++rank) {
    const auto tab = static_cast<ItemTab>(rank);
    for (const bool active : {true, false}) {
      CHECK(ItemTabPiece(tab, active) != ItemTabPieceLying(tab, active));
    }
    // Et l'actif n'est pas l'inactif : le suffixe « 1 » est l'onglet OUVERT.
    CHECK(ItemTabPieceLying(tab, true) != ItemTabPieceLying(tab, false));
  }
}

TEST_CASE("Coffre : chaque onglet couche a SON image, sauf UNE paire", "[ui][coffre][habillage]") {
  // Une table indexee par rang se desaccorde en silence : deux onglets qui
  // partageraient la meme piece se verraient identiques, et le joueur cliquerait
  // sur celui d'a cote sans rien remarquer.
  //
  // 🔴 L'EXCEPTION EST CELLE DU CLIENT, ET ELLE EST NOMMEE ICI : `tab_cos` EST la
  // tuile d'APPARAT, et le client la donne aussi a l'onglet « Equip » du sac. Les
  // deux onglets existent, mais aucune fenetre ne les montre ENSEMBLE — le sac
  // n'a pas « Costumes », le coffre n'a pas « Equip ».
  std::vector<SkinPiece> seen;
  for (std::size_t rank = 0; rank < kItemTabCount; ++rank) {
    const auto tab = static_cast<ItemTab>(rank);
    if (tab == ItemTab::kCostume) {
      continue;
    }
    seen.push_back(ItemTabPieceLying(tab, true));
    seen.push_back(ItemTabPieceLying(tab, false));
  }
  std::ranges::sort(seen);
  CHECK(std::ranges::adjacent_find(seen) == seen.end());

  // ⚠ ET LA PAIRE PARTAGE BIEN, dans les deux dispositions : si l'une des deux
  // tables divergeait, un onglet prendrait la tuile de son voisin.
  for (const bool active : {true, false}) {
    CHECK(ItemTabPiece(ItemTab::kCostume, active) == ItemTabPiece(ItemTab::kEquipment, active));
    CHECK(ItemTabPieceLying(ItemTab::kCostume, active) ==
          ItemTabPieceLying(ItemTab::kEquipment, active));
  }
}

TEST_CASE("Coffre : chaque colonne a son libelle, et aucun n'est vide", "[ui][coffre]") {
  for (const StorageColumn column :
       {StorageColumn::kSlot, StorageColumn::kItem, StorageColumn::kItemId,
        StorageColumn::kCardSlots, StorageColumn::kAmount}) {
    const std::string_view label = StorageColumnLabel(column);
    CHECK_FALSE(label.empty());
  }
}

TEST_CASE("Coffre : l'empreinte du tri suit la QUANTITE", "[ui][coffre]") {
  // 🔴 SANS ELLE, UN TRI PAR QUANTITE RESTERAIT CELUI D'AVANT. Un retrait
  // partiel change la quantite SANS retirer la ligne : ni le compte de lignes ni
  // les identifiants ne bougent, et une empreinte qui ne lirait qu'eux dirait
  // « rien n'a change ».
  std::array<InventoryCellView, 2> before = {CellOf(1, 501, 10), CellOf(2, 502, 3)};
  std::array<InventoryCellView, 2> after = {CellOf(1, 501, 4), CellOf(2, 502, 3)};
  CHECK(StorageRowsFingerprint(before) != StorageRowsFingerprint(after));

  // Et elle ne bouge pas toute seule : deux listes identiques se valent.
  std::array<InventoryCellView, 2> same = {CellOf(1, 501, 10), CellOf(2, 502, 3)};
  CHECK(StorageRowsFingerprint(before) == StorageRowsFingerprint(same));
}

// ── Le geste de description, conduit a la souris ────────────────────────────
//
// La rangee ne se calcule pas : elle se TOUCHE. La table se dispose sur l'image
// suivante, et sa hauteur depend de la police comme du pied — predire un `y`
// figerait ici une mesure qui n'appartient pas a cet essai.
struct StorageProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = WholeArt();
  ro::ui::StorageWindowState state;
  std::vector<InventoryCellView> cells = WellStocked(6);
  ro::ui::StorageTally tally{.item_count = 6, .capacity = 600, .shown_count = 6};

  StorageProbe() { state.open = true; }

  // Une image, et ce que la fenetre y a repondu.
  ro::ui::StorageWindowAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ro::ui::StorageWindowAction action = ro::ui::DrawStorageWindow(state, {}, cells, tally, art);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < 4; ++frame) {
      static_cast<void>(Frame());
    }
  }
};

// Ou tomber pour toucher le NOM d'une rangee. On tate la fenetre -- son
// rectangle, qu'ImGui sait dire -- et on garde le premier point que la fenetre
// declare survole. Rend un point hors ecran si rien ne repond, ce qui fait
// echouer l'essai plutot que de le rendre creux.
[[nodiscard]] ImVec2 FindRowPoint(StorageProbe& probe) {
  const ImGuiWindow* window = ImGui::FindWindowByName("###storage");
  if (window == nullptr) {
    return ImVec2(-1.0F, -1.0F);
  }
  ImGuiIO& io = ImGui::GetIO();
  for (float y = window->Pos.y; y < window->Pos.y + window->Size.y; y += 3.0F) {
    for (float x = window->Pos.x; x < window->Pos.x + window->Size.x; x += 15.0F) {
      io.AddMousePosEvent(x, y);
      if (probe.Frame().hovered_id != 0) {
        return ImVec2(x, y);
      }
    }
  }
  return ImVec2(-1.0F, -1.0F);
}

TEST_CASE("Coffre : la description part au RELACHEMENT, pas a l'enfoncement",
          "[ui][coffre][description]") {
  // 🔴 C'EST LA REGLE QUE CET ESSAI TIENT, et elle se paie en z-ordre. La fenetre
  // de description nait a l'image SUIVANTE : partie de l'enfoncement, elle
  // naissait pendant que le bouton etait encore tenu sur le coffre, qui reprenait
  // le premier plan -- et elle s'ouvrait DERRIERE lui.
  //
  // ⚠ C'EST AUSSI LE BORD DU MENU. `BeginPopupContextItem` s'ouvre lui aussi au
  // relachement : sur l'enfoncement, les deux gestes ne se departageaient plus
  // par leur touche mais par leur instant.
  StorageProbe probe;
  probe.Settle();

  const ImVec2 point = FindRowPoint(probe);
  REQUIRE(point.x >= 0.0F);

  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(point.x, point.y);
  static_cast<void>(probe.Frame());
  io.AddKeyEvent(ImGuiMod_Ctrl, true);

  io.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
  const ro::ui::StorageWindowAction pressed = probe.Frame();
  // RIEN N'EST DEMANDE TANT QUE LE BOUTON EST TENU.
  CHECK(pressed.describe_id == 0);

  io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
  const ro::ui::StorageWindowAction released = probe.Frame();
  CHECK(released.describe_id != 0);
  // ⚠ ET SA PLACE PART AVEC LUI : sans elle le nom de la description perdrait son
  // raffinement et ses cartes. Elle manquait ici comme dans la charrette.
  CHECK(released.describe_slot != 0);
  io.AddKeyEvent(ImGuiMod_Ctrl, false);
  static_cast<void>(probe.Frame());
}

TEST_CASE("Coffre : sans bande de categories, la vue repasse a Tout", "[ui][coffre]") {
  // 🔴 UN FILTRE QUI NE SE MONTRE PLUS CACHE DES LIGNES SANS RAISON VISIBLE. Le
  // joueur qui retire la bande alors qu'il est sur « Cartes » ne verrait plus
  // qu'une poignee d'objets dans un coffre de six cents, et rien a l'ecran ne le
  // lui dirait. C'est la FENETRE qui remet la vue a plat : elle est seule a
  // savoir qu'elle n'a pas dessine la bande.
  StorageProbe probe;
  probe.state.tab = ItemTab::kCard;
  probe.Settle();
  CHECK(probe.state.tab == ItemTab::kCard);

  probe.state.type_tabs = false;
  probe.Settle();
  CHECK(probe.state.tab == ItemTab::kAll);
}

TEST_CASE("Coffre : son pied ne lui donne pas d'ascenseur", "[ui][coffre][habillage]") {
  // 🔴 IL N'Y A RIEN A FAIRE DEFILER SOUS LA TABLE, et un ascenseur de fenetre
  // n'y serait pas seulement inutile : il mange la poignee de redimensionnement
  // du coin bas droit. Il apparaissait parce que le bouton « Quitter » du pied
  // se pose en coordonnees ABSOLUES, plus bas que la fin de la zone de contenu,
  // sans rendre a la fenetre sa marque de contenu -- la meme regle que
  // `FooterToggle` et `FooterHelp` tiennent depuis toujours.
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = WholeArt();
  ro::ui::StorageWindowState state;
  state.open = true;
  const std::vector<InventoryCellView> cells = WellStocked(31);
  const ro::ui::StorageTally tally{.item_count = 31, .capacity = 600, .shown_count = 31};

  // Plusieurs images : une table se dispose sur la SUIVANTE, et l'ascenseur
  // d'une fenetre se decide sur ce que l'image precedente a mesure.
  for (int frame = 0; frame < 4; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawStorageWindow(state, {}, cells, tally, art));
    ui.EndFrame();
  }

  const ImGuiWindow* const window = ImGui::FindWindowByName(state.title.c_str());
  REQUIRE(window != nullptr);
  CHECK_FALSE(window->ScrollbarY);
}

TEST_CASE("Ascenseur : sa course est CELLE d'ImGui", "[ui][widgets]") {
  // 🔴 CE QU'ON PEINT DOIT TOMBER OU CE QU'IMGUI ECOUTE SE TROUVE. La formule de
  // la poignee est recopiee d'ImGui et eprouvee a part ; ce qu'on lui DONNE ne
  // l'etait pas, et c'est la que la copie s'etait defaite : `content` valait
  // `ContentSize.y` plus la hauteur VISIBLE au lieu de deux marges de fenetre.
  // La poignee sortait plus courte que la vraie et sa course n'atteignait jamais
  // le bas.
  //
  // L'invariant tient en une ligne : ce qui depasse du visible est exactement ce
  // qu'ImGui fait defiler.
  ro::ui::UiContext ui;
  ro::ui::ScrollExtent extent;
  bool scrollbar = false;
  float scroll_max = 0.0F;
  float scroll = 0.0F;
  // Trois images : une fenetre n'ouvre son ascenseur qu'a partir de ce que
  // l'image PRECEDENTE a mesure, et la course se lit donc une image plus tard.
  for (int frame = 0; frame < 3; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::SetNextWindowSize(ImVec2(200.0F, 120.0F));
    ImGui::Begin("essai");
    for (int line = 0; line < 60; ++line) {
      ImGui::Text("ligne %d", line);
    }
    ImGui::SetScrollY(20.0F);
    extent = ro::ui::WindowScrollExtent();
    const ImGuiWindow* const window = ImGui::GetCurrentWindow();
    scrollbar = window->ScrollbarY;
    scroll_max = window->ScrollMax.y;
    scroll = window->Scroll.y;
    // ⚠ LE `End` AVANT TOUTE VERIFICATION : une assertion qui echoue sort de la
    // fonction, et ImGui tombe alors sur un `Begin` sans `End`.
    ImGui::End();
    ui.EndFrame();
  }

  REQUIRE(scrollbar);
  CHECK(extent.content - extent.visible == scroll_max);
  CHECK(extent.scroll == scroll);
}

// ── Seules les lignes visibles se posent ─────────────────────────────────────
//
// 🔴 CE QUE LE DECOUPEUR NE DOIT PAS CHANGER, c'est ce que le joueur voit : une
// ligne sous la souris est celle qu'elle serait sans lui, au pixel pres, et la
// course de l'ascenseur mene jusqu'a la derniere. On le TOUCHE ici, comme le
// geste de description : chaque hauteur de la table est survolee, et la ligne
// qui repond est notee.
namespace {

// La fenetre interieure de la table : c'est elle qui defile. ImGui la nomme
// d'apres la fenetre et l'identite de la table.
[[nodiscard]] ImGuiWindow* RowsWindow(const ro::ui::StorageWindowState& state) {
  const ImGuiWindow* const outer = ImGui::FindWindowByName(state.title.c_str());
  if (outer == nullptr) {
    return nullptr;
  }
  for (ImGuiWindow* const window : ImGui::GetCurrentContext()->Windows) {
    if (window->ParentWindow == outer &&
        std::string_view(window->Name).find("/lignes_") != std::string_view::npos) {
      return window;
    }
  }
  return nullptr;
}

// La place qui repond a chaque hauteur de la table, zero pour l'en-tete et les
// marges. Une image par pixel : c'est le survol qui dit ou tombe une ligne.
[[nodiscard]] std::vector<std::uint16_t> SweepRows(StorageProbe& probe, float x) {
  const ImGuiWindow* const rows = RowsWindow(probe.state);
  if (rows == nullptr) {
    return {};
  }
  const float top = rows->InnerClipRect.Min.y;
  const float bottom = rows->InnerClipRect.Max.y;
  std::vector<std::uint16_t> slots;
  ImGuiIO& io = ImGui::GetIO();
  for (float y = top; y < bottom; y += 1.0F) {
    io.AddMousePosEvent(x, y);
    slots.push_back(probe.Frame().hovered_slot);
  }
  return slots;
}

// Les places survolees se suivent, sans trou ni retour : la table est triee par
// nom, tous egaux, donc dans l'ordre des places.
[[nodiscard]] bool Contiguous(const std::vector<std::uint16_t>& swept) {
  std::uint16_t last = 0;
  for (const std::uint16_t slot : swept) {
    if (slot == 0 || slot == last) {
      continue;
    }
    if (last != 0 && slot != last + 1) {
      return false;
    }
    last = slot;
  }
  return last != 0;
}

void ScrollRowsTo(const ro::ui::StorageWindowState& state, float scroll_y) {
  ImGuiWindow* const rows = RowsWindow(state);
  if (rows != nullptr) {
    ImGui::SetScrollY(rows, scroll_y);
  }
}

// La hauteur d'une ligne, lue sur le balayage : l'ecart entre les premiers
// pixels de deux lignes voisines.
[[nodiscard]] int RowPitch(const std::vector<std::uint16_t>& swept) {
  std::vector<int> starts;
  std::uint16_t last = 0;
  for (std::size_t y = 0; y < swept.size(); ++y) {
    if (swept[y] != 0 && swept[y] != last) {
      starts.push_back(static_cast<int>(y));
      last = swept[y];
    }
  }
  return starts.size() < 3 ? 0 : starts[2] - starts[1];
}

constexpr int kManyRows = 400;
constexpr int kRowsScrolled = 10;
constexpr std::uint16_t kMenuSlot = 5;
// Plus bas que toute table : ImGui borne le défilement à sa course. ⚠ PAS
// `FLT_MAX`, qui veut dire pour lui « aucune cible ».
constexpr float kFarBelowPixels = 1.0e7F;

}  // namespace

TEST_CASE("Coffre : une ligne survolee est celle qu'elle serait sans decoupeur",
          "[ui][coffre][defilement]") {
  StorageProbe probe;
  probe.cells = WellStocked(kManyRows);
  probe.tally = {.item_count = kManyRows, .capacity = 600, .shown_count = kManyRows};
  probe.Settle();
  const ImVec2 point = FindRowPoint(probe);
  REQUIRE(point.x >= 0.0F);

  ScrollRowsTo(probe.state, 0.0F);
  probe.Settle();
  const std::vector<std::uint16_t> at_top = SweepRows(probe, point.x);
  REQUIRE(Contiguous(at_top));
  const int pitch = RowPitch(at_top);
  REQUIRE(pitch > 0);

  // ⚠ DIX LIGNES PLUS BAS, CHAQUE PIXEL REPOND DIX PLACES PLUS LOIN : ni une
  // ligne de trop, ni un decalage d'un pixel. C'est le defilement d'une table
  // dont toutes les lignes seraient posees.
  ScrollRowsTo(probe.state, static_cast<float>(pitch * kRowsScrolled));
  probe.Settle();
  const std::vector<std::uint16_t> scrolled = SweepRows(probe, point.x);
  REQUIRE(scrolled.size() == at_top.size());
  REQUIRE(Contiguous(scrolled));
  int compared = 0;
  for (std::size_t y = 0; y < at_top.size(); ++y) {
    if (at_top[y] == 0) {
      // L'en-tete reste en place : il est gele en haut de la table.
      CHECK(scrolled[y] == 0);
      continue;
    }
    // ⚠ LA PREMIERE LIGNE TOUCHE L'EN-TETE GELE, dont ImGui recule le bord d'un
    // pixel des que des lignes passent dessous — table entierement posee
    // comprise. Ce pixel-la n'est pas celui du decoupeur.
    if (at_top[y] == 1) {
      continue;
    }
    CAPTURE(y);
    CHECK(scrolled[y] == at_top[y] + kRowsScrolled);
    ++compared;
  }
  CHECK(compared > pitch);
}

TEST_CASE("Coffre : la course de l'ascenseur mene a la derniere ligne",
          "[ui][coffre][defilement]") {
  // 🔴 LES LIGNES SAUTEES GARDENT LEUR HAUTEUR : sans elle, la table croirait
  // n'en porter que vingt, et la fin du coffre serait hors d'atteinte.
  StorageProbe probe;
  probe.cells = WellStocked(kManyRows);
  probe.tally = {.item_count = kManyRows, .capacity = 600, .shown_count = kManyRows};
  probe.Settle();
  const ImVec2 point = FindRowPoint(probe);
  REQUIRE(point.x >= 0.0F);

  ScrollRowsTo(probe.state, kFarBelowPixels);
  probe.Settle();
  const std::vector<std::uint16_t> at_bottom = SweepRows(probe, point.x);
  REQUIRE(Contiguous(at_bottom));
  CHECK(std::ranges::max(at_bottom) == kManyRows);
}

TEST_CASE("Coffre : un menu ouvert survit a sa ligne qui sort de la vue",
          "[ui][coffre][defilement]") {
  // 🔴 SEULES LES LIGNES VISIBLES SE POSENT, et le menu d'une ligne se pose AVEC
  // elle. Faire defiler la table menu ouvert sortirait sa ligne de la vue : la
  // ligne tenue se pose donc quand meme, et le menu reste.
  StorageProbe probe;
  probe.cells = WellStocked(kManyRows);
  for (InventoryCellView& cell : probe.cells) {
    // Une pile : le clic droit ouvre le menu, sans retrait direct.
    cell.amount = 5;
  }
  probe.tally = {.item_count = kManyRows, .capacity = 600, .shown_count = kManyRows};
  probe.Settle();
  const ImVec2 point = FindRowPoint(probe);
  REQUIRE(point.x >= 0.0F);

  // ⚠ PAS LA PREMIERE LIGNE : le decoupeur la pose toujours, pour mesurer la
  // hauteur des autres. Son menu survivrait sans rien prouver.
  const std::vector<std::uint16_t> swept = SweepRows(probe, point.x);
  const auto fifth = std::ranges::find(swept, kMenuSlot);
  REQUIRE(fifth != swept.end());
  const float menu_y =
      RowsWindow(probe.state)->InnerClipRect.Min.y + static_cast<float>(fifth - swept.begin());

  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(point.x, menu_y);
  const std::uint16_t menu_slot = probe.Frame().hovered_slot;
  REQUIRE(menu_slot == kMenuSlot);
  io.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
  static_cast<void>(probe.Frame());
  static_cast<void>(probe.Frame());
  const ImGuiContext& g = *ImGui::GetCurrentContext();
  REQUIRE(g.OpenPopupStack.Size == 1);
  CHECK(probe.state.held_slot == menu_slot);

  ScrollRowsTo(probe.state, kFarBelowPixels);
  probe.Settle();
  REQUIRE(g.OpenPopupStack.Size == 1);
  REQUIRE(g.OpenPopupStack[0].Window != nullptr);
  CHECK(g.OpenPopupStack[0].Window->Active);
  CHECK(probe.state.held_slot == menu_slot);
}

TEST_CASE("Coffre : deux lignes echangees ne rendent pas la meme empreinte", "[ui][coffre]") {
  // L'ordre d'arrivee fait partie de ce que le tri lit : le serveur peut relister
  // le coffre dans un autre ordre, et l'ordre affiche doit alors se refaire.
  std::array<InventoryCellView, 2> forward = {CellOf(1, 501, 10), CellOf(2, 502, 3)};
  std::array<InventoryCellView, 2> backward = {CellOf(2, 502, 3), CellOf(1, 501, 10)};
  CHECK(StorageRowsFingerprint(forward) != StorageRowsFingerprint(backward));
}
