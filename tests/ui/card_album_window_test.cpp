// L'ALBUM DE CARTES, dessiné hors jeu.
//
// Ce qui se vérifie sans écran : les noms sous les pochettes ; fermée, la
// fenêtre ne dessine rien ; ouverte, elle demande les vignettes de ce qu'elle
// montre ; refermée, elle rend le verrou au serveur UNE fois ; et une carte que
// le sac lui offre se range, se sacrifie ou se refuse selon sa pochette.

#include "ro/ui/card_album_window.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include <imgui.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::CardAlbumShortName;
using ro::ui::CardAlbumVerb;
using ro::ui::CardAlbumView;
using ro::ui::CardAlbumWindowAction;
using ro::ui::CardAlbumWindowState;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1600.0F, .height_px = 900.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr std::uint32_t kPoring = 4001;
constexpr std::uint32_t kFabre = 4002;
constexpr std::uint32_t kPhreeoni = 4121;
constexpr std::uint32_t kOldCardAlbum = 616;
constexpr std::uint16_t kPoringSlot = 3;
constexpr std::uint16_t kPhreeoniSlot = 4;
constexpr std::uint16_t kEnchantSlot = 5;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

struct AlbumProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  CardAlbumWindowState state;
  std::array<ro::ui::CardAlbumPocketView, 3> pockets;
  std::array<ro::ui::CardAlbumSourceView, 2> sources = {
      ro::ui::CardAlbumSourceView{
          .item_id = kOldCardAlbum, .letter = "O", .name = "Old Card Album", .typical_rate = 10},
      ro::ui::CardAlbumSourceView{.item_id = 12246, .letter = "M", .name = "Mystical Card Album"}};
  std::array<ro::ui::CardAlbumHandCard, 3> hand = {
      ro::ui::CardAlbumHandCard{.client_slot = kPoringSlot, .card_id = kPoring, .amount = 4},
      ro::ui::CardAlbumHandCard{.client_slot = kPhreeoniSlot, .card_id = kPhreeoni, .amount = 3},
      // Un enchant est rangé en carte, et le catalogue ne le connaît pas.
      ro::ui::CardAlbumHandCard{.client_slot = kEnchantSlot, .card_id = 4700, .amount = 1}};
  CardAlbumView view;

  AlbumProbe() {
    pockets[0] = {.card_id = kPoring,
                  .reserve = 2,
                  .unlocked = true,
                  .families = ro::ui::kCardFamilyArmor,
                  .name = "Poring Card",
                  .slot_label = "Armor"};
    pockets[0].sources[0] = {.present = true, .percent = 0.3, .weight = 10, .total = 3000};
    pockets[1] = {.card_id = kFabre,
                  .unlocked = true,
                  .families = ro::ui::kCardFamilyArmor,
                  .name = "Fabre Card",
                  .slot_label = "Armor"};
    pockets[2] = {.card_id = kPhreeoni,
                  .families = ro::ui::kCardFamilyWeapon,
                  .dropper_rank = 2,
                  .name = "Phreeoni Card",
                  .slot_label = "Weapon"};
    view.pockets = pockets;
    view.revision = 1;
    view.unlocked_count = 2;
    view.total_reserve = 2;
    view.dropper_known = true;
    view.hand = hand;
    view.sources = sources;
    view.highlight_card = kPhreeoni;
    view.highlight_serial = 1;
    view.notice = "Poring : 2 rangée(s) dans l'album.";
  }

  CardAlbumWindowAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    CardAlbumWindowAction action = ro::ui::DrawCardAlbumWindow(state, view, art);
    ui.EndFrame();
    return action;
  }
};

}  // namespace

TEST_CASE("Album : le nom sous la pochette perd son emplacement et « Card »", "[ui][album]") {
  CHECK(CardAlbumShortName("Poring Card") == "Poring");
  CHECK(CardAlbumShortName("Poring Card [Armor]") == "Poring");
  CHECK(CardAlbumShortName("Orc Lady card") == "Orc Lady");
  // Ce qui n'est pas un suffixe reste.
  CHECK(CardAlbumShortName("Cardinal") == "Cardinal");
}

TEST_CASE("Album : un nom ne devient jamais vide", "[ui][album]") {
  CHECK(CardAlbumShortName("") == "(?)");
  CHECK(CardAlbumShortName("[Armor]") == "[Armor]");
  CHECK(CardAlbumShortName("Card") == "Card");
}

TEST_CASE("Album : fermé, il ne dessine rien et ne demande rien", "[ui][album]") {
  AlbumProbe probe;
  const CardAlbumWindowAction action = probe.Frame();
  CHECK(action.verb == CardAlbumVerb::kNothing);
  CHECK(probe.state.wanted.empty());
}

TEST_CASE("Album : ouvert, il demande les vignettes de ce qu'il montre", "[ui][album]") {
  AlbumProbe probe;
  probe.state.open = true;
  for (int frame = 0; frame < 3; ++frame) {
    const CardAlbumWindowAction action = probe.Frame();
    CHECK(action.verb == CardAlbumVerb::kNothing);
  }
  REQUIRE(probe.state.wanted.size() == 3);
  // La pochette scellée demande sa SILHOUETTE, les ouvertes leur couleur.
  for (const ro::ui::CardAlbumArtAsk& asked : probe.state.wanted) {
    CHECK(asked.silhouette == (asked.card_id == kPhreeoni));
  }
  CHECK(probe.state.wanted_width > 0);
  CHECK(probe.state.title.find("2 / 3") != std::string::npos);
  CHECK(probe.state.family_total == 3);
}

TEST_CASE("Album : refermé d'ailleurs, il rend le verrou une fois", "[ui][album]") {
  AlbumProbe probe;
  probe.state.open = true;
  (void)probe.Frame();
  probe.state.open = false;  // Échap, ou le raccourci
  CHECK(probe.Frame().verb == CardAlbumVerb::kClose);
  CHECK(probe.Frame().verb == CardAlbumVerb::kNothing);
}

// ⚠ PAS L'INTERCALAIRE : l'onglet choisi appartient à la barre d'ImGui, qui le
// réécrit à chaque image. La nature et la recherche, elles, sont à la fenêtre.
TEST_CASE("Album : la nature et la recherche filtrent la collection", "[ui][album]") {
  AlbumProbe probe;
  probe.state.open = true;
  probe.state.nature_mask = 1U << 2U;  // MVP
  probe.state.order_dirty = true;
  (void)probe.Frame();
  REQUIRE(probe.state.order.size() == 1);
  CHECK(probe.pockets[static_cast<std::size_t>(probe.state.order[0])].card_id == kPhreeoni);
  // La recherche porte aussi sur l'identifiant.
  probe.state.nature_mask = 0;
  probe.state.filter[0] = '4';
  probe.state.filter[1] = '0';
  probe.state.filter[2] = '0';
  probe.state.filter[3] = '2';
  probe.state.order_dirty = true;
  (void)probe.Frame();
  REQUIRE(probe.state.order.size() == 1);
  CHECK(probe.pockets[static_cast<std::size_t>(probe.state.order[0])].card_id == kFabre);
}

TEST_CASE("Album : la pochette demandée lève les filtres qui la cachent", "[ui][album]") {
  AlbumProbe probe;
  probe.state.open = true;
  probe.view.highlight_serial = 0;
  (void)probe.Frame();
  // Les ouvertes seulement, et une recherche : Phreeoni, scellée, est cachée.
  probe.state.show_filter = 1;
  probe.state.filter[0] = 'P';
  probe.state.order_dirty = true;
  (void)probe.Frame();
  REQUIRE(probe.state.order.size() == 1);
  // « Voir dans l'album » : un nouveau numéro de série, et la demande de voir.
  probe.view.highlight_card = kPhreeoni;
  probe.view.highlight_serial = 1;
  probe.view.highlight_reveal = true;
  (void)probe.Frame();
  CHECK(probe.state.show_filter == 0);
  CHECK(probe.state.filter[0] == '\0');
  CHECK(probe.state.order.size() == 3);
  CHECK(probe.state.highlight_seen == 1);
}

TEST_CASE("Album : la pochette demandée ramène l'intercalaire « Tous »", "[ui][album]") {
  constexpr int kWeaponTab = 2;
  AlbumProbe probe;
  probe.state.open = true;
  probe.view.highlight_serial = 0;
  (void)probe.Frame();
  // L'intercalaire des armes, choisi comme la barre le choisirait.
  probe.state.slot_filter = kWeaponTab;
  probe.state.slot_tab_asked = true;
  probe.state.order_dirty = true;
  for (int frame = 0; frame < 3; ++frame) {
    (void)probe.Frame();
  }
  REQUIRE(probe.state.slot_filter == kWeaponTab);
  REQUIRE(probe.state.order.size() == 1);
  probe.view.highlight_card = kPoring;
  probe.view.highlight_serial = 1;
  probe.view.highlight_reveal = true;
  // ⚠ L'ONGLET DES ARMES, encore montré une image, ne reprend pas le filtre.
  for (int frame = 0; frame < 3; ++frame) {
    (void)probe.Frame();
  }
  CHECK(probe.state.slot_filter == 0);
  CHECK(probe.state.order.size() == 3);
}

TEST_CASE("Album : un ordre abouti laisse les filtres du joueur", "[ui][album]") {
  AlbumProbe probe;
  probe.state.open = true;
  probe.view.highlight_serial = 0;
  (void)probe.Frame();
  probe.state.show_filter = 1;
  probe.state.order_dirty = true;
  (void)probe.Frame();
  probe.view.highlight_serial = 1;
  (void)probe.Frame();
  CHECK(probe.state.show_filter == 1);
  CHECK(probe.state.order.size() == 2);
}

TEST_CASE("Album : une carte offerte se range dans sa pochette ouverte", "[ui][album]") {
  AlbumProbe probe;
  probe.state.open = true;
  (void)probe.Frame();
  probe.state.offered_slot = kPoringSlot;
  const CardAlbumWindowAction action = probe.Frame();
  // LA PILE ENTIÈRE, sans demander.
  CHECK(action.verb == CardAlbumVerb::kDeposit);
  CHECK(action.slot == kPoringSlot);
  CHECK(action.amount == 4);
}

TEST_CASE("Album : une pochette scellée demande la confirmation du sacrifice", "[ui][album]") {
  AlbumProbe probe;
  probe.state.open = true;
  (void)probe.Frame();
  probe.state.offered_slot = kPhreeoniSlot;
  CHECK(probe.Frame().verb == CardAlbumVerb::kNothing);
  CHECK(probe.state.confirm_card == kPhreeoni);
  CHECK(probe.state.confirm_rest == 2);
  // La boîte se dessine quelques images sans rien envoyer d'elle-même.
  for (int frame = 0; frame < 3; ++frame) {
    CHECK(probe.Frame().verb == CardAlbumVerb::kNothing);
  }
}

TEST_CASE("Album : sans confirmation, le sacrifice part aussitôt", "[ui][album]") {
  AlbumProbe probe;
  probe.state.open = true;
  probe.state.look.auto_sacrifice = true;
  (void)probe.Frame();
  probe.state.offered_slot = kPhreeoniSlot;
  const CardAlbumWindowAction action = probe.Frame();
  CHECK(action.verb == CardAlbumVerb::kSacrifice);
  CHECK(action.amount == 1);
  CHECK(action.rest == 2);
}

TEST_CASE("Album : ce qui n'est pas au catalogue se refuse sans voyage", "[ui][album]") {
  AlbumProbe probe;
  probe.state.open = true;
  (void)probe.Frame();
  probe.state.offered_slot = kEnchantSlot;
  const CardAlbumWindowAction action = probe.Frame();
  CHECK(action.verb == CardAlbumVerb::kNothing);
  CHECK(action.refused_not_a_card);
}

TEST_CASE("Album : une pochette lâchée sur le sac se reprend", "[ui][album]") {
  AlbumProbe probe;
  probe.state.open = true;
  (void)probe.Frame();
  // Deux copies : la quantité se demande, rien ne part encore.
  probe.state.withdraw_asked = kPoring;
  CHECK(probe.Frame().verb == CardAlbumVerb::kNothing);
  CHECK(probe.state.quantity_asked);
  CHECK(probe.state.quantity.most == 2);
}

TEST_CASE("Album : les macarons du staff se dessinent sous Ctrl et Maj", "[ui][album]") {
  AlbumProbe probe;
  probe.state.open = true;
  probe.view.staff = true;
  probe.pockets[1].sources[1].intent = ro::ui::CardAlbumIntent::kAdd;
  probe.pockets[1].sources[1].intent_rate = 3;
  (void)probe.Frame();
  ImGuiIO& io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiMod_Ctrl, true);
  CHECK(probe.Frame().verb == CardAlbumVerb::kNothing);
  io.AddKeyEvent(ImGuiMod_Ctrl, false);
  io.AddKeyEvent(ImGuiMod_Shift, true);
  CHECK(probe.Frame().verb == CardAlbumVerb::kNothing);
  io.AddKeyEvent(ImGuiMod_Shift, false);
  CHECK(probe.Frame().verb == CardAlbumVerb::kNothing);
}
