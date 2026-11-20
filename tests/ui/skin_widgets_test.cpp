// Le bouton habillé : sa mesure, et ce qu'il fait sans art.

#include "ro/ui/skin_widgets.h"

#include <algorithm>
#include <cmath>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::SkinArt;
using ro::ui::SkinButton;
using ro::ui::SkinButtonWidth;
using ro::ui::SkinImage;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

// De quoi laisser un libellé déborder sans qu'ImGui le coupe au bord de la
// fenêtre : c'est LÀ que se mesure le débordement d'un bouton.
constexpr float kWideEnough = 600.0F;
constexpr float kTallEnough = 200.0F;

// Les mesures du PETIT bouton, telles que l'installation les livre :
// `sbtn_out_left.bmp` fait six pixels sur quatorze.
constexpr float kSmallCapPixels = 6.0F;
constexpr float kSmallButtonPixels = 14.0F;
// Ce que son item réserve AU-DESSUS de l'art, pour le libellé qui dépasse.
constexpr float kSmallDropPixels = 3.0F;

// Les mesures du client : un bout de bouton fait six pixels sur vingt.
constexpr float kCapPixels = 6.0F;
constexpr float kButtonPixels = 20.0F;

[[nodiscard]] SkinArt ButtonArt() {
  SkinArt art;
  for (const SkinPiece piece :
       {SkinPiece::kButtonLeft, SkinPiece::kButtonMiddle, SkinPiece::kButtonRight,
        SkinPiece::kButtonHoverLeft, SkinPiece::kButtonHoverMiddle, SkinPiece::kButtonHoverRight,
        SkinPiece::kButtonPressedLeft, SkinPiece::kButtonPressedMiddle,
        SkinPiece::kButtonPressedRight}) {
    art.Set(piece, SkinImage{.id = 1, .width = kCapPixels, .height = kButtonPixels});
  }
  return art;
}

}  // namespace

TEST_CASE("Bouton : sa largeur suit son libellé", "[ui][widgets]") {
  // ⚠ C'EST CE QUI PERMET DE DIMENSIONNER UN CONTENEUR SUR DES LIBELLÉS TRADUITS.
  // Le même bouton n'a pas la même largeur en français et en anglais, et une mise
  // en page calée sur l'une coupe l'autre.
  ro::ui::UiContext ui;
  const SkinArt art = ButtonArt();

  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::Begin("essai");
  const float court = SkinButtonWidth("Oui", art);
  const float long_label = SkinButtonWidth("Annuler la transaction", art);
  CHECK(court > 0.0F);
  CHECK(long_label > court);
  // La marge de confort s'ajoute au texte, elle ne s'y retranche pas : le
  // bouton est plus large que son libellé nu.
  CHECK(court > ImGui::CalcTextSize("Oui").x);
  ImGui::End();
  ui.EndFrame();
}

TEST_CASE("Bouton : il prend sa place dans la disposition", "[ui][widgets]") {
  // ⚠ L'ART N'EST QUE DE LA PEINTURE. C'est l'élément invisible qui occupe la
  // place et reçoit le clic ; sans lui on verrait un bouton sans pouvoir
  // l'atteindre, et le suivant se dessinerait par-dessus.
  ro::ui::UiContext ui;
  const SkinArt art = ButtonArt();

  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::Begin("essai");
  const float y_before = ImGui::GetCursorPosY();
  static_cast<void>(SkinButton("Suivant", art));
  const float y_after = ImGui::GetCursorPosY();
  CHECK(y_after > y_before);
  // La hauteur vient de l'art, à l'espacement près.
  CHECK(y_after - y_before >= kButtonPixels);
  ImGui::End();
  ui.EndFrame();
}

TEST_CASE("Bouton : sans art, celui d'ImGui", "[ui][widgets]") {
  // Il n'y a pas de demi-bouton à peindre : un état manquant le ferait
  // disparaître au survol. Mieux vaut le bouton ordinaire, partout pareil.
  ro::ui::UiContext ui;
  SkinArt incomplet;
  incomplet.Set(SkinPiece::kButtonLeft,
                SkinImage{.id = 1, .width = kCapPixels, .height = kButtonPixels});

  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::Begin("essai");
  const float y_before = ImGui::GetCursorPosY();
  static_cast<void>(SkinButton("Fermer", incomplet));
  CHECK(ImGui::GetCursorPosY() > y_before);
  ImGui::End();
  ui.EndFrame();
}

TEST_CASE("Case à cocher : le libellé fait partie de la cible", "[ui][widgets]") {
  // ⚠ UNE CASE DE ONZE PIXELS EST UNE CIBLE MINUSCULE. Le client, comme ImGui,
  // laisse cliquer le mot à côté ; ne prendre que le carré rendrait l'option
  // pénible à atteindre sans que personne sache pourquoi.
  ro::ui::UiContext ui;
  SkinArt art;
  art.Set(SkinPiece::kCheckBoxEmpty, SkinImage{.id = 1, .width = 11.0F, .height = 11.0F});
  art.Set(SkinPiece::kCheckBoxTicked, SkinImage{.id = 1, .width = 11.0F, .height = 11.0F});

  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::Begin("essai");
  bool coche = false;
  static_cast<void>(ro::ui::SkinCheckBox("Montrer les boîtes", &coche, art));
  const ImVec2 size = ImGui::GetItemRectSize();
  // Plus large que la case seule : le libellé est dedans.
  CHECK(size.x > 11.0F + ImGui::CalcTextSize("Montrer les boîtes").x * 0.5F);
  ImGui::End();
  ui.EndFrame();
}

TEST_CASE("Case à cocher : sans son art, celle d'ImGui", "[ui][widgets]") {
  ro::ui::UiContext ui;
  const SkinArt aucun;

  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::Begin("essai");
  bool coche = true;
  const float y_before = ImGui::GetCursorPosY();
  static_cast<void>(ro::ui::SkinCheckBox("Option", &coche, aucun));
  CHECK(ImGui::GetCursorPosY() > y_before);
  // Et elle n'a pas changé toute seule.
  CHECK(coche);
  ImGui::End();
  ui.EndFrame();
}

TEST_CASE("Bouton d'état : il ne retient rien", "[ui][widgets]") {
  // ⚠ L'ÉTAT APPARTIENT À L'APPELANT. Une brique qui le garderait le garderait
  // pour tout le monde : deux onglets du même libellé, dans deux fenêtres, se
  // partageraient leur état sans que rien ne le dise.
  ro::ui::UiContext ui;
  const SkinArt art = ButtonArt();

  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::Begin("essai");
  // Ni l'un ni l'autre n'est cliqué : les deux rendent faux, quel que soit
  // l'état qu'on leur passe.
  CHECK_FALSE(ro::ui::SkinToggleButton("Actif", true, art));
  CHECK_FALSE(ro::ui::SkinToggleButton("Repos", false, art));
  ImGui::End();
  ui.EndFrame();
}

TEST_CASE("Poignée : rien sur une fenêtre qui ne se redimensionne pas", "[ui][widgets]") {
  // Peindre une poignée sur une fenêtre figée promettrait un geste qui ne
  // répondrait jamais.
  ro::ui::UiContext ui;
  SkinArt art;
  art.Set(SkinPiece::kResizeGrip, SkinImage{.id = 1, .width = 10.0F, .height = 10.0F});

  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::Begin("figee", nullptr, ImGuiWindowFlags_NoResize);
  const int y_before = ImGui::GetWindowDrawList()->VtxBuffer.Size;
  ro::ui::PaintWindowResizeGrip(art);
  CHECK(ImGui::GetWindowDrawList()->VtxBuffer.Size == y_before);
  ImGui::End();

  // Sur une fenêtre ordinaire, elle se peint.
  ImGui::Begin("libre");
  const int sans = ImGui::GetWindowDrawList()->VtxBuffer.Size;
  ro::ui::PaintWindowResizeGrip(art);
  CHECK(ImGui::GetWindowDrawList()->VtxBuffer.Size > sans);
  ImGui::End();
  ui.EndFrame();
}

// Le coin bas-droit du DERNIER quadrilatere pose dans la liste de dessin. Une
// image ImGui en pose quatre sommets ; le plus a droite et le plus en bas dit ou
// la poignee s'arrete.
[[nodiscard]] ImVec2 LastQuadCorner(const ImDrawList* into) {
  ImVec2 corner(-FLT_MAX, -FLT_MAX);
  const int count = into->VtxBuffer.Size;
  for (int rank = count - 4; rank < count; ++rank) {
    if (rank < 0) {
      continue;
    }
    corner.x = (std::max)(corner.x, into->VtxBuffer[rank].pos.x);
    corner.y = (std::max)(corner.y, into->VtxBuffer[rank].pos.y);
  }
  return corner;
}

TEST_CASE("Poignée : elle RENTRE dans le cadre, elle ne s'y pose pas dessus", "[ui][widgets]") {
  // 🔴 C'EST LA RÈGLE QUE CET ESSAI TIENT, et elle se voit tout de suite quand
  // elle manque : le cadre d'une fenêtre du jeu est un neuf-pièces, ses bords ont
  // une épaisseur, et une poignée collée au coin se pose PAR-DESSUS eux. Le coin
  // arrondi passe derrière elle.
  //
  // ⚠ ON MESURE LES SOMMETS, pas un pixel : c'est la seule preuve du placement
  // qui ne dépende d'aucune texture.
  ro::ui::UiContext ui;
  SkinArt art;
  art.Set(SkinPiece::kResizeGrip, SkinImage{.id = 1, .width = 10.0F, .height = 10.0F});

  constexpr ImVec2 kWhere(40.0F, 30.0F);
  constexpr ImVec2 kHowBig(300.0F, 200.0F);

  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::SetNextWindowPos(kWhere);
  ImGui::SetNextWindowSize(kHowBig);
  ImGui::Begin("libre");
  ro::ui::PaintWindowResizeGrip(art);
  const ImVec2 ordinary = LastQuadCorner(ImGui::GetWindowDrawList());
  ImGui::End();
  ui.EndFrame();

  // L'échelle vaut un dans un essai : la mesure est donc celle du client.
  CHECK(ordinary.x == kWhere.x + kHowBig.x - ro::ui::kResizeGripInsetPixels);
  CHECK(ordinary.y == kWhere.y + kHowBig.y - ro::ui::kResizeGripInsetPixels);

  // ⚠ ET LA DESCRIPTION RENTRE PLUS LOIN : son cadre est une boîte `sysbox`,
  // plus épaisse que la bordure mince d'une fenêtre ordinaire.
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::SetNextWindowPos(kWhere);
  ImGui::SetNextWindowSize(kHowBig);
  ImGui::Begin("desc");
  ro::ui::PaintWindowResizeGrip(art, ro::ui::kDescriptionResizeGripInsetPixels);
  const ImVec2 described = LastQuadCorner(ImGui::GetWindowDrawList());
  ImGui::End();
  ui.EndFrame();

  CHECK(described.x == kWhere.x + kHowBig.x - ro::ui::kDescriptionResizeGripInsetPixels);
  CHECK(described.y == kWhere.y + kHowBig.y - ro::ui::kDescriptionResizeGripInsetPixels);
  CHECK(described.x < ordinary.x);
}

TEST_CASE("Bouton : survolé, il demande la main qui pointe", "[ui][widgets]") {
  // ⚠ C'EST SOUVENT LE SEUL INDICE QU'UNE CHOSE SE CLIQUE. Sur un panneau du
  // jeu, un bouton ne s'annonce ni par un cadre qui s'allume ni par un
  // soulignement : la forme du curseur est ce qui le dit.
  //
  // ⚠ ET LE VERDICT SE LIT APRÈS L'IMAGE, comme le reste du laissez-passer :
  // ImGui remet le curseur à la flèche au DÉBUT de chaque image, et la boucle le
  // consulte avant de dessiner l'interface suivante.
  ro::ui::UiContext ui;
  const SkinArt art = ButtonArt();
  const ImVec2 where(200.0F, 200.0F);

  // Une image pour poser le bouton là où la souris ira.
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::SetNextWindowPos(where);
  ImGui::Begin("essai");
  static_cast<void>(SkinButton("Suivant", art));
  ImGui::End();
  ui.EndFrame();

  // Ailleurs : rien à pointer.
  ImGui::GetIO().MousePos = ImVec2(where.x - 50.0F, where.y - 50.0F);
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::SetNextWindowPos(where);
  ImGui::Begin("essai");
  static_cast<void>(SkinButton("Suivant", art));
  ImGui::End();
  ui.EndFrame();
  CHECK_FALSE(ui.gate().pointing);

  // Sur le bouton : la main.
  ImGui::GetIO().MousePos = ImVec2(where.x + 20.0F, where.y + 30.0F);
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::SetNextWindowPos(where);
  ImGui::Begin("essai");
  static_cast<void>(SkinButton("Suivant", art));
  ImGui::End();
  ui.EndFrame();
  CHECK(ui.gate().pointing);
}

TEST_CASE("Curseur : la poignée qui redimensionne demande la main", "[ui][widgets]") {
  // ImGui réclame une flèche de redimensionnement sur la poignée d'une fenêtre ;
  // le jeu ne dessine pas les siennes, et la poignée doit dire qu'elle se saisit.
  ro::ui::UiContext ui;
  const ImVec2 where(200.0F, 200.0F);
  const ImVec2 size(200.0F, 150.0F);
  // Juste à l'intérieur du coin bas-droit, sur la poignée.
  constexpr float kInsideGripPixels = 3.0F;
  ImGui::GetIO().MousePos =
      ImVec2(where.x + size.x - kInsideGripPixels, where.y + size.y - kInsideGripPixels);
  constexpr int kFramesToSettle = 3;
  for (int frame = 0; frame < kFramesToSettle; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::SetNextWindowPos(where);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("redimensionnable");
    ImGui::End();
    ui.EndFrame();
  }
  CHECK(ui.gate().pointing);
}

TEST_CASE("Alignement : un texte se centre sur la RANGÉE, pas sur la pièce", "[ui][skin]") {
  // 🔴 LA CASE DU CLIENT FAIT ONZE PIXELS, moins qu'une ligne de texte : un
  // libellé centré sur LA CASE serait posé au-dessus du haut de rangée — et le
  // « (?) » qu'un `SameLine` ajoute ensuite, lui posé au haut de rangée,
  // paraîtrait décalé SOUS le mot.
  //
  // ⚠ LE LIBELLÉ LUI-MÊME N'EST OBSERVABLE PAR AUCUN ESSAI : il est PEINT dans
  // la liste de dessin, ce n'est pas un élément d'ImGui, et aucun rectangle ne
  // l'enregistre. C'est la règle par laquelle il passe qui se vérifie ici, et
  // c'est pourquoi elle est publique plutôt qu'écrite trois fois à la main.
  constexpr float kRowTop = 100.0F;
  constexpr float kTinyBoxPixels = 11.0F;
  constexpr float kTextPixels = 16.0F;

  // La rangée fait la hauteur du plus grand des deux : le texte, ici.
  const float row = std::max(kTinyBoxPixels, kTextPixels);
  CHECK(ro::ui::RowTextTop(kRowTop, row, kTextPixels) == kRowTop);
  // Et la petite pièce descend au milieu, plutôt que le texte ne monte.
  CHECK(ro::ui::RowTextTop(kRowTop, row, kTinyBoxPixels) > kRowTop);
  CHECK(ro::ui::RowTextTop(kRowTop, row, kTinyBoxPixels) + kTinyBoxPixels <= kRowTop + row);

  // Passer la hauteur de la PIÈCE au lieu de celle de la rangée est ce qui
  // sortait le texte par le haut. La fonction ne protège pas de cet appel-là :
  // elle le rend VISIBLE, en nommant son paramètre.
  CHECK(ro::ui::RowTextTop(kRowTop, kTinyBoxPixels, kTextPixels) < kRowTop);

  // Arrondi à l'entier : un demi-pixel fait flotter le texte entre deux lignes.
  CHECK(ro::ui::RowTextTop(0.0F, 15.0F, 10.0F) == 2.0F);
}

// 🔴 LE FAUX GRAS RESTE À UN PIXEL ENTRE DEUX ÉCHELLES ENTIÈRES. À 125 %, la
// seconde passe d'un texte d'une quinzaine de pixels tombée deux pixels plus
// loin bouche les creux des lettres : « Aventure » ne se lit plus.
TEST_CASE("Faux gras : un nombre entier de pixels d'écran, jamais moins d'un", "[ui][widgets]") {
  using ro::ui::kFakeBoldPixels;
  using ro::ui::ScreenOffsetPixels;
  CHECK(ScreenOffsetPixels(kFakeBoldPixels, 1.0F) == 1.0F);
  CHECK(ScreenOffsetPixels(kFakeBoldPixels, 1.25F) == 1.0F);
  CHECK(ScreenOffsetPixels(kFakeBoldPixels, 1.5F) == 1.0F);
  CHECK(ScreenOffsetPixels(kFakeBoldPixels, 2.0F) == 2.0F);
  // Réduite, l'interface garde un gras visible.
  CHECK(ScreenOffsetPixels(kFakeBoldPixels, 0.75F) == 1.0F);
}

// 🔴 LA DENSITÉ DE L'INTERFACE : un cadre vaut la barre de titre.
//
// Un champ de saisie, une jauge ou une liste déroulante valent la police plus
// deux fois le remplissage vertical. Laissé au défaut d'ImGui, ce remplissage
// donne un cadre de vingt et un pixels là où le jeu en fait DIX-SEPT — la
// hauteur de son art de barre de titre. Quatre pixels par cadre, sur chaque
// ligne de chaque fenêtre : c'est tout l'écart entre une interface qui
// ressemble au jeu et une qui y ressemble presque, et il s'est vu d'abord sur
// la banque, dont le fond peint attendait la densité du client.
TEST_CASE("Skin : un cadre fait la hauteur de la barre de titre", "[ui][skin]") {
  ro::ui::UiContext ui;
  SkinArt art;
  constexpr float kTitleBarPixels = 17.0F;
  art.Set(SkinPiece::kTitleBarLeft, SkinImage{.id = 1, .width = 12.0F, .height = kTitleBarPixels});

  ui.BeginFrame(kViewport, kFrameSeconds);
  const float padding = ro::ui::SkinFramePaddingY(art);
  const float frame = ImGui::GetFontSize() + 2.0F * padding;
  ui.EndFrame();

  CHECK(frame == kTitleBarPixels);
}

// ⚠ SANS ART, ON NE TOUCHE À RIEN : l'interface est alors celle d'ImGui, et
// c'est ce qu'on veut d'une installation qui n'a pas ses bitmaps.
TEST_CASE("Skin : sans barre de titre, la densité reste celle d'ImGui", "[ui][skin]") {
  ro::ui::UiContext ui;
  const SkinArt bare;

  ui.BeginFrame(kViewport, kFrameSeconds);
  const float padding = ro::ui::SkinFramePaddingY(bare);
  const float untouched = ImGui::GetStyle().FramePadding.y;
  ui.EndFrame();

  CHECK(padding == untouched);
}

// 🔴 UN LIBELLÉ TROP LONG NE DÉBORDE PAS DE SON BOUTON.
//
// « Tout déposer » et « +100M », à la banque, sont posés dans des cases
// taillées au plus juste : écrits à taille pleine, ils mordraient sur l'art, là
// où le client les RAPETISSE.
//
// ⚠ CE BANC MESURE CE QUE LE BOUTON DESSINE, et non ce qu'il calcule : le
// libellé part à la main dans la liste d'affichage, qu'ImGui ne clippe pas, et
// aucune fonction pure n'aurait attrapé le débordement. On relit donc les
// sommets ajoutés pendant l'appel et l'on vérifie qu'aucun ne sort du cadre.
TEST_CASE("Bouton : un libellé trop long rapetisse au lieu de déborder", "[ui][widgets]") {
  ro::ui::UiContext ui;
  const SkinArt art = ButtonArt();

  // 🔴 LA FENÊTRE EST LARGE, ET C'EST INDISPENSABLE. ImGui coupe le texte au
  // bord de la FENÊTRE — pas à celui du bouton : dans une fenêtre étroite, le
  // débordement se fait rogner et le banc ne voit rien. C'est ce qui l'a rendu
  // creux une première fois.
  ImGui::SetNextWindowSize(ImVec2(kWideEnough, kTallEnough));
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::Begin("essai");
  // Une case étroite, et un libellé qui n'y tient pas de loin.
  constexpr float kNarrow = 60.0F;
  const ImVec2 at = ImGui::GetCursorScreenPos();
  ImDrawList* const into = ImGui::GetWindowDrawList();
  const int before = into->VtxBuffer.Size;
  static_cast<void>(SkinButton("Annuler la transaction", art, ImVec2(kNarrow, 0.0F)));
  const int after = into->VtxBuffer.Size;
  REQUIRE(after > before);

  // ⚠ ON REGARDE LES DEUX BORDS. L'art occupe exactement la case, donc il ne
  // dit rien ; ce qui sort, à gauche comme à droite, ne peut être que le
  // libellé — et un libellé centré qui déborde sort des DEUX côtés.
  float leftmost = at.x;
  float widest = at.x;
  for (int rank = before; rank < after; ++rank) {
    leftmost = std::min(leftmost, into->VtxBuffer[rank].pos.x);
    widest = std::max(widest, into->VtxBuffer[rank].pos.x);
  }
  ImGui::End();
  ui.EndFrame();

  // ⚠ UN DEMI-PIXEL DE TOLÉRANCE : le centrage tombe sur des fractions, et le
  // dernier glyphe peut mordre d'autant sans que cela se voie.
  CHECK(leftmost >= at.x - 0.5F);
  CHECK(widest <= at.x + kNarrow + 0.5F);
}

// ⚠ ET IL NE RAPETISSE PAS QUAND IL N'Y A PAS DE RAISON : un bouton large écrit
// son libellé à la taille de l'interface. Sans cette moitié-ci, un rapetissement
// posé partout passerait pour juste.
TEST_CASE("Bouton : un libellé qui tient garde sa taille", "[ui][widgets]") {
  ro::ui::UiContext ui;
  const SkinArt art = ButtonArt();

  ImGui::SetNextWindowSize(ImVec2(kWideEnough, kTallEnough));
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::Begin("essai");
  const ImVec2 at = ImGui::GetCursorScreenPos();
  ImDrawList* const into = ImGui::GetWindowDrawList();
  const int before = into->VtxBuffer.Size;
  static_cast<void>(SkinButton("Oui", art, ImVec2(300.0F, 0.0F)));
  const int after = into->VtxBuffer.Size;

  float leftmost = at.x + 300.0F;
  float widest = at.x;
  for (int rank = before; rank < after; ++rank) {
    leftmost = std::min(leftmost, into->VtxBuffer[rank].pos.x);
    widest = std::max(widest, into->VtxBuffer[rank].pos.x);
  }
  ImGui::End();
  ui.EndFrame();

  // Le dessin tient dans le cadre, et le cadre est bien celui qu'on a demandé.
  CHECK(leftmost >= at.x - 0.5F);
  CHECK(widest <= at.x + 300.0F + 0.5F);
}

[[nodiscard]] SkinArt SmallButtonArt() {
  SkinArt art;
  for (const SkinPiece piece :
       {SkinPiece::kSmallButtonLeft, SkinPiece::kSmallButtonMiddle, SkinPiece::kSmallButtonRight,
        SkinPiece::kSmallButtonHoverLeft, SkinPiece::kSmallButtonHoverMiddle,
        SkinPiece::kSmallButtonHoverRight, SkinPiece::kSmallButtonPressedLeft,
        SkinPiece::kSmallButtonPressedMiddle, SkinPiece::kSmallButtonPressedRight}) {
    art.Set(piece, SkinImage{.id = 1, .width = kSmallCapPixels, .height = kSmallButtonPixels});
  }
  return art;
}

// 🔴 LE PETIT BOUTON ÉCRIT DANS LA POLICE DE L'INTERFACE, pas dans une plus
// petite. C'est l'écart que le joueur a fini par nommer : son art fait quatorze
// pixels de haut, nous rabattions le texte dessus — douze pixels — quand le
// client l'écrit en quinze et le laisse dépasser.
//
// ⚠ ON LE MESURE PAR LA LARGEUR AUTOMATIQUE, qui est la seule sortie observable
// où la taille du texte entre : elle vaut PILE le libellé plus les deux bouts
// d'art. Un texte rapetissé la rendrait plus courte.
TEST_CASE("Petit bouton : son libellé est écrit dans la police de l'interface", "[ui][widgets]") {
  ro::ui::UiContext ui;
  const SkinArt art = SmallButtonArt();

  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::Begin("essai");
  const float measured = ro::ui::SkinSmallButtonWidth("Tout deposer", art);
  const float at_interface_size = ImGui::CalcTextSize("Tout deposer").x + kSmallCapPixels * 2.0F;
  ImGui::End();
  ui.EndFrame();

  CHECK(measured == at_interface_size);
}

// ⚠ ET SA PLACE EST CELLE DE L'ART PLUS LA CHUTE. Un appelant qui réserverait
// la seule hauteur de l'art verrait le libellé mordre sur la ligne du dessus.
TEST_CASE("Petit bouton : il réserve la chute au-dessus de son art", "[ui][widgets]") {
  ro::ui::UiContext ui;
  const SkinArt art = SmallButtonArt();

  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::Begin("essai");
  const float height = ro::ui::SkinSmallButtonHeight(art);
  ImGui::End();
  ui.EndFrame();

  CHECK(height == kSmallButtonPixels + kSmallDropPixels);
}

// 🔴 ET LA HAUTEUR QU'ON LUI DONNE EST CETTE PLACE-LÀ, pas celle de son art.
//
// La mesure rend l'item, et `size.y` doit dire la même chose. S'il lisait
// l'art, un appelant qui passe l'une à l'autre — la barre de titre, le pied du
// coffre — recevrait un art ÉTIRÉ de la chute dans un item plus haut de trois
// pixels que celui qu'il a réservé.
//
// ⚠ LE LIBELLÉ EST VIDE, ET C'EST CE QUI REND LA MESURE POSSIBLE : `##rien`
// n'écrit aucun glyphe, si bien que les sommets ajoutés pendant l'appel sont
// CEUX DE L'ART et rien d'autre.
TEST_CASE("Petit bouton : la hauteur qu'on lui donne est sa place, pas son art", "[ui][widgets]") {
  ro::ui::UiContext ui;
  const SkinArt art = SmallButtonArt();

  ImGui::SetNextWindowSize(ImVec2(kWideEnough, kTallEnough));
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::Begin("essai");
  const float asked = ro::ui::SkinSmallButtonHeight(art);
  ImDrawList* const into = ImGui::GetWindowDrawList();
  const int before = into->VtxBuffer.Size;
  static_cast<void>(ro::ui::SkinSmallButton("##rien", art, ImVec2(60.0F, asked)));
  const int after = into->VtxBuffer.Size;
  const ImVec2 taken = ImGui::GetItemRectSize();

  float highest = FLT_MAX;
  float lowest = -FLT_MAX;
  for (int rank = before; rank < after; ++rank) {
    highest = std::min(highest, into->VtxBuffer[rank].pos.y);
    lowest = std::max(lowest, into->VtxBuffer[rank].pos.y);
  }
  ImGui::End();
  ui.EndFrame();

  REQUIRE(after > before);
  // La place prise est CELLE QU'ON A DEMANDÉE…
  CHECK(taken.y == asked);
  // …et l'art n'y est pas étiré : il garde la hauteur de son bitmap.
  CHECK(lowest - highest == kSmallButtonPixels);
}

// L'ART SE CENTRE SUR LA LIGNE, ET L'ITEM DÉBORDE PAR LE HAUT. C'est ce que
// demande une barre de titre : la chute est du vide, et centrer l'item y
// descendrait l'art de sa moitié.
TEST_CASE("Petit bouton : centré sur une ligne, c'est son ART qui l'est", "[ui][widgets]") {
  ro::ui::UiContext ui;
  const SkinArt art = SmallButtonArt();

  ImGui::SetNextWindowSize(ImVec2(kWideEnough, kTallEnough));
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::Begin("essai");
  // ⚠ UNE LIGNE À L'ENTIER : la pose s'arrondit au pixel — l'art du jeu est
  // échantillonné au plus proche — et une ligne fractionnaire ferait porter le
  // banc sur cet arrondi plutôt que sur le centrage.
  const float middle = std::floor(ImGui::GetCursorScreenPos().y) + kTallEnough * 0.5F;
  ImGui::SetCursorScreenPos(
      ImVec2(ImGui::GetCursorScreenPos().x, ro::ui::SkinSmallButtonTopCenteredOn(art, middle)));
  static_cast<void>(ro::ui::SkinSmallButton("##rien", art, ImVec2(60.0F, 0.0F)));
  // L'art occupe le BAS de l'item : la chute est au-dessus de lui.
  const float art_top = ImGui::GetItemRectMin().y + kSmallDropPixels;
  const float art_bottom = ImGui::GetItemRectMax().y;
  ImGui::End();
  ui.EndFrame();

  CHECK(art_bottom - art_top == kSmallButtonPixels);
  CHECK((art_top + art_bottom) * 0.5F == middle);
}

// ── LE MARQUEUR D'AIDE QUI MET EN GARDE ─────────────────────────────────────

namespace {

// Ce que la bulle d'un marqueur survolé a écrit, et si le signe y est peint.
struct HelpBubble {
  std::string written;
  bool mark_painted = false;
};

// DEUX IMAGES POUR POSER LE MARQUEUR — une fenêtre neuve ne prend sa taille, et
// la souris, qu'à la seconde —, UNE TROISIÈME SOUS LA SOURIS. Le texte se
// relève par le journal d'ImGui, qui recopie tout ce qui s'écrit.
[[nodiscard]] HelpBubble HoverHelpMarker(const char* help, const char* warning) {
  ro::ui::UiContext ui;
  const ImVec2 where(100.0F, 100.0F);
  ImVec2 marker;
  for (int frame = 0; frame < 2; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::SetNextWindowPos(where);
    ImGui::Begin("aide");
    ro::ui::SkinHelpMarker(help, warning);
    marker = ImVec2((ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5F,
                    (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5F);
    ImGui::End();
    ui.EndFrame();
  }

  ImGui::GetIO().MousePos = marker;
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::SetNextWindowPos(where);
  ImGui::Begin("aide");
  ImGui::LogToBuffer();
  ro::ui::SkinHelpMarker(help, warning);
  HelpBubble seen{.written = ImGui::GetCurrentContext()->LogBuffer.c_str()};
  ImGui::LogFinish();
  ImGui::End();
  for (const ImGuiWindow* window : ImGui::GetCurrentContext()->Windows) {
    for (const ImDrawVert& vertex : window->DrawList->VtxBuffer) {
      seen.mark_painted = seen.mark_painted || vertex.col == ro::ui::kSkinWarningFill;
    }
  }
  ui.EndFrame();
  return seen;
}

}  // namespace

TEST_CASE("Marqueur d'aide : la mise en garde vient après l'aide, derrière le signe peint",
          "[ui][widgets]") {
  const HelpBubble seen = HoverHelpMarker("Ce que fait le réglage.", "Ce qu'il coûte.");
  const std::size_t help = seen.written.find("Ce que fait le réglage.");
  const std::size_t warning = seen.written.find("Ce qu'il coûte.");
  REQUIRE(help != std::string::npos);
  REQUIRE(warning != std::string::npos);
  CHECK(help < warning);
  CHECK(seen.mark_painted);
}

TEST_CASE("Marqueur d'aide : sans mise en garde, l'aide seule et aucun signe", "[ui][widgets]") {
  const HelpBubble seen = HoverHelpMarker("Ce que fait le réglage.", nullptr);
  INFO(seen.written);
  CHECK(seen.written.find("Ce que fait le réglage.") != std::string::npos);
  CHECK_FALSE(seen.mark_painted);
}
