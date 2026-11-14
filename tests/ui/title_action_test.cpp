// Le raccourci de barre de titre, sans fenêtre ni carte graphique.

#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/skin_art.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

// Les mesures du client : la barre de titre fait dix-sept pixels de haut,
// l'art d'un petit bouton quatorze, et son item en réserve trois de plus
// au-dessus pour le libellé qui dépasse.
constexpr float kTitleBarPixels = 17.0F;
constexpr float kSmallButtonPixels = 14.0F;
constexpr float kSmallCapPixels = 6.0F;
constexpr float kSmallDropPixels = 3.0F;

// Un habillage complet, aux tailles du client. Les identifiants ne désignent
// aucune texture : rien n'est peint ici, on ne mesure que la disposition.
[[nodiscard]] ro::ui::SkinArt WholeArt() {
  using ro::render::SkinPiece;
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = 8.0F, .height = 8.0F});
  }
  for (const SkinPiece piece :
       {SkinPiece::kTitleBarLeft, SkinPiece::kTitleBarMiddle, SkinPiece::kTitleBarRight}) {
    art.Set(piece, ro::ui::SkinImage{.id = 1, .width = 12.0F, .height = kTitleBarPixels});
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

}  // namespace

TEST_CASE("Barre de titre : le raccourci tient DANS la barre", "[ui][titre]") {
  // 🔴 CE QUI SE CENTRE EST L'ART, PAS L'ITEM. Un petit bouton réserve trois
  // pixels AU-DESSUS de son art pour le libellé qui dépasse ; cette chute est du
  // VIDE. Centrer l'item la compte comme du dessin, l'art descend d'autant, et
  // sur une barre de dix-sept pixels pour un art de quatorze il en sort par le
  // bas — c'est ce que le joueur a vu.
  //
  // ⚠ ET LA HAUTEUR NE S'IMPOSE PAS : `SkinSmallButtonHeight` rend la place
  // PRISE, que la barre passait en `size.y` — l'art s'y trouvait étiré des trois
  // mêmes pixels, par-dessus le marché.
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = WholeArt();
  REQUIRE(art.dresses_windows());

  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::SetNextWindowPos(ImVec2(100.0F, 100.0F));
  ImGui::SetNextWindowSize(ImVec2(300.0F, 200.0F));
  ro::ui::SetNextWindowTitleAction("Cart", "Ouvrir", true);
  bool open = true;
  REQUIRE(ro::ui::BeginSkinnedWindow("Inventory###essai", &open, art));
  const ImGuiWindow* const window = ImGui::GetCurrentWindow();
  const float bar_top = window->Pos.y;
  const float bar_height = window->TitleBarHeight;
  // ⚠ LE DERNIER ITEM SOUMIS EST LE RACCOURCI : la barre le pose en dernier, et
  // ni le dépilage du découpage ni la remise en place du curseur n'y touchent.
  // C'est la seule prise qu'on ait sur sa géométrie depuis le dehors.
  const float item_top = ImGui::GetItemRectMin().y;
  const float item_bottom = ImGui::GetItemRectMax().y;
  ro::ui::EndSkinnedWindow();
  ui.EndFrame();

  REQUIRE(bar_height == kTitleBarPixels);
  // L'art occupe le BAS de l'item : la chute est au-dessus de lui.
  const float art_top = item_top + kSmallDropPixels;
  const float art_bottom = item_bottom;
  // Il n'est pas étiré…
  CHECK(art_bottom - art_top == kSmallButtonPixels);
  // …il ne sort de la barre ni par le haut ni par le bas…
  CHECK(art_top >= bar_top);
  CHECK(art_bottom <= bar_top + bar_height);
  // …et il y est CENTRÉ, au pixel entier : une barre de dix-sept pixels pour un
  // art de quatorze ne se partage pas en deux, et l'arrondi va au plancher pour
  // que l'art tombe sur des texels entiers.
  CHECK(art_top == bar_top + std::floor((bar_height - kSmallButtonPixels) * 0.5F));
}

TEST_CASE("Barre de titre : la demande est CONSOMMEE, meme sans habillage", "[ui][titre]") {
  // 🔴 LE PIEGE QUE LA PUCE A DEJA PAYE. Une demande qui survivrait a la fenetre
  // pour laquelle elle a ete posee ferait apparaitre le bouton sur la fenetre
  // SUIVANTE -- et son clic partirait a la mauvaise.
  //
  // ⚠ ET CE N'EST PAS UN CAS DE BORD : une installation sans art prend la
  // branche nue de BeginSkinnedWindow, qui ne dessine aucune barre. Si la
  // consommation vivait avec le dessin, elle n'aurait jamais lieu la.
  ro::ui::UiContext ui;
  const ro::ui::SkinArt bare;
  REQUIRE_FALSE(bare.dresses_windows());

  ui.BeginFrame(kViewport, kFrameSeconds);
  ro::ui::SetNextWindowTitleAction("Storage", "Ouvrir", true);
  bool first_open = true;
  if (ro::ui::BeginSkinnedWindow("premiere###premiere", &first_open, bare)) {
    CHECK_FALSE(ro::ui::TitleActionClicked(0));
  }
  ro::ui::EndSkinnedWindow();

  // La seconde n'a RIEN demande : elle ne doit hériter de rien.
  bool second_open = true;
  if (ro::ui::BeginSkinnedWindow("seconde###seconde", &second_open, bare)) {
    CHECK_FALSE(ro::ui::TitleActionClicked(0));
  }
  ro::ui::EndSkinnedWindow();
  ui.EndFrame();
}

TEST_CASE("Barre de titre : le raccourci ne pousse pas le corps vers le bas", "[ui][titre]") {
  // 🔴 LE BOUTON EST POSE PAR-DESSUS LA BARRE, donc en DEPLACANT le curseur de
  // disposition. Le laisser la ferait commencer le corps de la fenetre dans sa
  // barre de titre -- un decalage qu'on attribuerait a la marge, pas au bouton.
  ro::ui::UiContext ui;
  const ro::ui::SkinArt bare;

  ui.BeginFrame(kViewport, kFrameSeconds);
  bool plain_open = true;
  float plain_y = 0.0F;
  if (ro::ui::BeginSkinnedWindow("nue###temoin", &plain_open, bare)) {
    plain_y = ImGui::GetCursorScreenPos().y - ImGui::GetWindowPos().y;
  }
  ro::ui::EndSkinnedWindow();

  ro::ui::SetNextWindowTitleAction("Storage", "Ouvrir", true);
  bool action_open = true;
  float action_y = 0.0F;
  if (ro::ui::BeginSkinnedWindow("avec###essai", &action_open, bare)) {
    action_y = ImGui::GetCursorScreenPos().y - ImGui::GetWindowPos().y;
  }
  ro::ui::EndSkinnedWindow();
  ui.EndFrame();

  CHECK(plain_y == action_y);
}

TEST_CASE("Barre de titre : deux raccourcis ne se confondent pas", "[ui][titre]") {
  // 🔴 LE RANG EST CELUI DE LA POSE, pas celui de l'affichage. Les deux sont
  // inverses -- chaque bouton devient la butee du suivant, qui se place a sa
  // GAUCHE --, et lire le mauvais rang ouvrirait l'autre fenetre.
  //
  // ⚠ ET LE TROP-PLEIN EST JETE : une barre de titre n'a pas la place d'un
  // troisieme raccourci, et un client ne tombe pas pour si peu.
  ro::ui::UiContext ui;
  const ro::ui::SkinArt bare;

  ui.BeginFrame(kViewport, kFrameSeconds);
  ro::ui::SetNextWindowTitleAction("Cart", "Ouvrir", true);
  ro::ui::SetNextWindowTitleAction("Storage", "Ouvrir", true);
  ro::ui::SetNextWindowTitleAction("Trop", "Jete", true);
  bool open = true;
  if (ro::ui::BeginSkinnedWindow("deux###deux", &open, bare)) {
    CHECK_FALSE(ro::ui::TitleActionClicked(0));
    CHECK_FALSE(ro::ui::TitleActionClicked(1));
    // Hors bornes des deux cotes : jamais vrai, jamais une lecture hors tableau.
    CHECK_FALSE(ro::ui::TitleActionClicked(-1));
    CHECK_FALSE(ro::ui::TitleActionClicked(ro::ui::kTitleActionCount));
  }
  ro::ui::EndSkinnedWindow();

  // Les TROIS demandes sont consommees, y compris celle qu'on a jetee : une
  // seule survivante ferait apparaitre un bouton sur la fenetre suivante.
  bool next_open = true;
  if (ro::ui::BeginSkinnedWindow("suivante###suivante", &next_open, bare)) {
    CHECK_FALSE(ro::ui::TitleActionClicked(0));
    CHECK_FALSE(ro::ui::TitleActionClicked(1));
  }
  ro::ui::EndSkinnedWindow();
  ui.EndFrame();
}
