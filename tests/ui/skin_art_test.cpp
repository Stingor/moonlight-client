// Les découpages du skin et la teinte de ses pièces, sans fenêtre ni pilote.

#include "ro/ui/skin_art.h"

#include <set>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"

using ro::render::SkinPiece;
using ro::ui::HorizontalStrip;
using ro::ui::NineSlice;
using ro::ui::SkinArt;
using ro::ui::SkinImage;

TEST_CASE("Skin : chaque pièce a son chemin, et aucun n'est partagé", "[ui][skin]") {
  // 🔴 UN `std::array` SOUS-REMPLI EST LÉGAL. Ajouter une pièce à l'énumération
  // sans ajouter son chemin à la table compile parfaitement : la pièce cherche
  // alors un fichier au nom vide, ne le trouve pas, et manque en silence — on
  // croit l'installation incomplète.
  //
  // ⚠ ET AUCUN DOUBLON : deux pièces sur le même bitmap, c'est une ligne
  // insérée au mauvais rang, qui décale toute la fin de la table.
  std::set<std::string_view> seen;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    const std::string_view path =
        ro::render::SkinPiecePath(static_cast<ro::render::SkinPiece>(rank));
    INFO("pièce numéro " << rank);
    CHECK_FALSE(path.empty());
    CHECK(seen.insert(path).second);
  }
}

TEST_CASE("Skin : une bande en trois garde ses bouts à leur taille", "[ui][skin]") {
  const ro::ui::StripRects strip =
      HorizontalStrip(ImVec2(100.0F, 10.0F), ImVec2(400.0F, 27.0F), 12.0F, 12.0F);

  // Les bouts font leur largeur, le milieu prend tout le reste.
  CHECK(strip.left.x == 100.0F);
  CHECK(strip.left.z == 112.0F);
  CHECK(strip.middle.x == 112.0F);
  CHECK(strip.middle.z == 388.0F);
  CHECK(strip.right.x == 388.0F);
  CHECK(strip.right.z == 400.0F);
  // La hauteur est celle qu'on a donnée, partout.
  CHECK(strip.left.y == 10.0F);
  CHECK(strip.middle.w == 27.0F);
}

TEST_CASE("Skin : une bande plus étroite que ses bouts ne s'inverse pas", "[ui][skin]") {
  // ⚠ C'EST LE CAS QUI CASSE. Une barre de vingt pixels entre deux bouts de
  // douze donne un milieu de largeur NÉGATIVE, qui se peindrait à l'envers —
  // texture retournée, bord droit à gauche. Les bouts se partagent alors la
  // place, et le milieu disparaît.
  const ro::ui::StripRects tight =
      HorizontalStrip(ImVec2(0.0F, 0.0F), ImVec2(20.0F, 17.0F), 12.0F, 12.0F);

  CHECK(tight.left.x <= tight.left.z);
  CHECK(tight.middle.x <= tight.middle.z);
  CHECK(tight.right.x <= tight.right.z);
  // Rien ne déborde de la bande.
  CHECK(tight.left.x == 0.0F);
  CHECK(tight.right.z == 20.0F);
  // Et les trois se suivent sans trou.
  CHECK(tight.left.z == tight.middle.x);
  CHECK(tight.middle.z == tight.right.x);
}

TEST_CASE("Skin : un cadre en neuf couvre tout, sans trou ni recouvrement", "[ui][skin]") {
  const ro::ui::NineSliceRects nine_slice =
      NineSlice(ImVec2(50.0F, 60.0F), ImVec2(250.0F, 200.0F), ImVec2(8.0F, 6.0F));

  // Les coins font leur taille.
  CHECK(nine_slice[0].x == 50.0F);
  CHECK(nine_slice[0].z == 58.0F);
  CHECK(nine_slice[0].w == 66.0F);
  CHECK(nine_slice[8].x == 242.0F);
  CHECK(nine_slice[8].z == 250.0F);
  CHECK(nine_slice[8].y == 194.0F);

  // ⚠ L'ORDRE EST CELUI DE LA LECTURE, et s'il changeait les bords iraient au
  // mauvais côté sans qu'aucune dimension ne s'en aperçoive.
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 3; ++column) {
      const ImVec4& part = nine_slice[row * 3 + column];
      // Chaque case se suit horizontalement...
      if (column > 0) {
        CHECK(part.x == nine_slice[row * 3 + column - 1].z);
      }
      // ... et verticalement.
      if (row > 0) {
        CHECK(part.y == nine_slice[(row - 1) * 3 + column].w);
      }
    }
  }
  // Les quatre bords du tout sont ceux qu'on a demandés.
  CHECK(nine_slice[0].x == 50.0F);
  CHECK(nine_slice[0].y == 60.0F);
  CHECK(nine_slice[8].z == 250.0F);
  CHECK(nine_slice[8].w == 200.0F);
}

TEST_CASE("Skin : un cadre plus petit que ses coins tient encore", "[ui][skin]") {
  const ro::ui::NineSliceRects minuscule =
      NineSlice(ImVec2(0.0F, 0.0F), ImVec2(6.0F, 4.0F), ImVec2(8.0F, 6.0F));
  for (const ImVec4& part : minuscule) {
    CHECK(part.x <= part.z);
    CHECK(part.y <= part.w);
  }
  CHECK(minuscule[8].z == 6.0F);
  CHECK(minuscule[8].w == 4.0F);
}

TEST_CASE("Skin : sans les trois morceaux de la barre, on ne peint rien", "[ui][skin]") {
  // ⚠ IL N'Y A PAS DE DEMI-BARRE À PEINDRE. Deux morceaux sur trois seraient
  // pires que pas de barre : le joueur croirait à un défaut de rendu là où il ne
  // manque qu'un bitmap à son installation.
  SkinArt art;
  CHECK_FALSE(art.dresses_windows());

  constexpr SkinImage kPresente{.id = 1, .width = 8.0F, .height = 6.0F};
  art.Set(SkinPiece::kTitleBarLeft, kPresente);
  art.Set(SkinPiece::kTitleBarMiddle, kPresente);
  CHECK_FALSE(art.dresses_windows());

  art.Set(SkinPiece::kTitleBarRight, kPresente);
  CHECK(art.dresses_windows());

  // ⚠ LES HUIT MORCEAUX DE `sysbox` NE DÉCIDENT DE RIEN ICI : ils appartiennent
  // aux boîtes modales, pas au corps d'une fenêtre, qui est un aplat.
  SkinArt boxed;
  for (const SkinPiece piece :
       {SkinPiece::kFrameTopLeft, SkinPiece::kFrameTopMiddle, SkinPiece::kFrameTopRight,
        SkinPiece::kFrameLeft, SkinPiece::kFrameRight, SkinPiece::kFrameBottomLeft,
        SkinPiece::kFrameBottomMiddle, SkinPiece::kFrameBottomRight}) {
    boxed.Set(piece, kPresente);
  }
  CHECK_FALSE(boxed.dresses_windows());
}

TEST_CASE("Skin : une pièce de cadre prend la couleur du fond qu'elle borde", "[ui][skin]") {
  // 🔴 SOUS LA TEINTE DES IMAGES, le blanc des pièces `sysbox` cernait un papier
  // sombre d'une bande ardoise. Teinte du fond, il devient ce fond.
  constexpr ImU32 kDarkPaper = IM_COL32(40, 42, 50, 255);
  ImGuiContext* const context = ImGui::CreateContext();
  ImDrawListSharedData shared;
  shared.ClipRectFullscreen = ImVec4(0.0F, 0.0F, 4096.0F, 4096.0F);
  ImDrawList into(&shared);
  into._ResetForNewFrame();
  into.PushClipRect(ImVec2(0.0F, 0.0F), ImVec2(4096.0F, 4096.0F), false);
  into.PushTextureID(ImTextureRef());
  ro::ui::PaintFrameOn(&into, ImTextureID{1}, ImVec2(0.0F, 0.0F), ImVec2(14.0F, 14.0F), kDarkPaper);
  REQUIRE_FALSE(into.VtxBuffer.empty());
  for (const ImDrawVert& corner : into.VtxBuffer) {
    CHECK(corner.col == kDarkPaper);
  }
  ImGui::DestroyContext(context);
}
