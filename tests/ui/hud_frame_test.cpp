// La géométrie d'un cadre de HUD : sa prise, son aimant, son bloc, sa retaille.
//
// 🔴 CE SONT LES QUATRE RÈGLES QUI SE TROMPENT SANS RIEN DIRE. Un bord qui
// s'aimante du mauvais côté, un bloc qui oublie un frère, une retaille qui fait
// glisser le cadre au lieu de le retailler : tout cela donne un résultat
// plausible, et ne se remarque qu'en reposant son interface pour la dixième
// fois. Aucune ne demande d'ImGui, donc aucune n'a d'excuse.

#include "ro/ui/hud_frame.h"

#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/window_places.h"

using ro::ui::HudEdgesAt;
using ro::ui::HudFrameLook;
using ro::ui::HudRect;
using ro::ui::HudSiblings;
using ro::ui::ResizedTo;
using ro::ui::SnappedToSiblings;
using ro::ui::TouchingFrames;

namespace {

// Un cadre net, posé loin de l'origine : un rectangle oublié à zéro ne passerait
// pas pour le bon.
[[nodiscard]] HudRect Frame(float x, float y, float width, float height) {
  HudRect rect;
  rect.x = x;
  rect.y = y;
  rect.width = width;
  rect.height = height;
  return rect;
}

constexpr ImVec2 kScreen(1920.0F, 1080.0F);

}  // namespace

TEST_CASE("Cadre de HUD : l'endroit saisi décide du geste", "[ui][hud]") {
  const HudRect rect = Frame(100.0F, 200.0F, 300.0F, 80.0F);

  // Le plein milieu ne tient aucun bord : c'est un déplacement.
  CHECK(HudEdgesAt(rect, ImVec2(250.0F, 240.0F)) == ro::ui::kHudEdgeNone);

  CHECK(HudEdgesAt(rect, ImVec2(102.0F, 240.0F)) == ro::ui::kHudEdgeLeft);
  CHECK(HudEdgesAt(rect, ImVec2(398.0F, 240.0F)) == ro::ui::kHudEdgeRight);
  CHECK(HudEdgesAt(rect, ImVec2(250.0F, 202.0F)) == ro::ui::kHudEdgeTop);
  CHECK(HudEdgesAt(rect, ImVec2(250.0F, 278.0F)) == ro::ui::kHudEdgeBottom);

  // ⚠ LE COIN BAS-DROIT EN ALLUME DEUX, ET SA PRISE EST PLUS LARGE. À huit
  // pixels du coin on est hors de la prise d'un BORD, et pourtant dans celle du
  // coin : c'est exactement la zone que le joueur vise pour retailler.
  const int corner = HudEdgesAt(rect, ImVec2(392.0F, 272.0F));
  CHECK((corner & ro::ui::kHudEdgeRight) != 0);
  CHECK((corner & ro::ui::kHudEdgeBottom) != 0);
}

TEST_CASE("Cadre de HUD : l'aimant connaît les quatre façons", "[ui][hud]") {
  std::vector<HudRect> frames{Frame(0.0F, 0.0F, 50.0F, 20.0F),
                              Frame(500.0F, 300.0F, 100.0F, 40.0F)};
  HudSiblings siblings;
  siblings.frames = frames;
  siblings.mine = 0;

  // Même bord gauche.
  CHECK(SnappedToSiblings(503.0F, 50.0F, siblings, false) == 500.0F);
  // Bord opposé : mon bord droit sur le sien.
  CHECK(SnappedToSiblings(548.0F, 50.0F, siblings, false) == 550.0F);
  // Juste après lui.
  CHECK(SnappedToSiblings(598.0F, 50.0F, siblings, false) == 600.0F);
  // Juste avant lui.
  CHECK(SnappedToSiblings(452.0F, 50.0F, siblings, false) == 450.0F);

  // Hors de portée : rien ne bouge.
  CHECK(SnappedToSiblings(300.0F, 50.0F, siblings, false) == 300.0F);

  // ⚠ ET L'AXE COMPTE : sur la verticale, ce sont y et height qui parlent.
  CHECK(SnappedToSiblings(303.0F, 20.0F, siblings, true) == 300.0F);
}

TEST_CASE("Cadre de HUD : on ne s'aimante pas sur ce qu'on ne voit pas", "[ui][hud]") {
  // 🔴 UN FRÈRE MASQUÉ N'AIMANTE RIEN. Sans cela, le cadre collerait à une
  // position que rien à l'écran ne justifie — le pire des défauts de pose, parce
  // qu'il a l'air d'un bug de souris.
  std::vector<HudRect> frames{Frame(0.0F, 0.0F, 50.0F, 20.0F),
                              Frame(500.0F, 300.0F, 100.0F, 40.0F)};
  // ⚠ UN TABLEAU D'OCTETS, et non un `std::vector<bool>` : celui-ci ne range pas
  // ses données de façon contiguë, donc aucun `span` ne peut le viser.
  const bool shown[] = {true, false};
  HudSiblings siblings;
  siblings.frames = frames;
  siblings.shown = std::span<const bool>(shown, 2);
  siblings.mine = 0;

  CHECK(SnappedToSiblings(503.0F, 50.0F, siblings, false) == 503.0F);
}

TEST_CASE("Cadre de HUD : le bloc se propage de proche en proche", "[ui][hud]") {
  // 🔴 TRANSITIVEMENT. A touche B, B touche C : les trois partent ensemble. Une
  // passe unique laisserait C derrière, et le bloc se disloquerait au geste — ce
  // qui ne se voit qu'en déplaçant, donc trop tard.
  std::vector<HudRect> frames{
      Frame(0.0F, 0.0F, 100.0F, 20.0F),    // A
      Frame(100.0F, 0.0F, 100.0F, 20.0F),  // B, collé à A
      Frame(200.0F, 0.0F, 100.0F, 20.0F),  // C, collé à B seulement
      Frame(900.0F, 900.0F, 50.0F, 50.0F)  // le solitaire
  };
  HudSiblings siblings;
  siblings.frames = frames;
  siblings.mine = 0;

  const std::uint32_t block = TouchingFrames(siblings, 0);
  CHECK((block & 1U) != 0);
  CHECK((block & 2U) != 0);
  CHECK((block & 4U) != 0);
  CHECK((block & 8U) == 0);
}

TEST_CASE("Cadre de HUD : un frère masqué ne part pas en bloc", "[ui][hud]") {
  std::vector<HudRect> frames{Frame(0.0F, 0.0F, 100.0F, 20.0F), Frame(100.0F, 0.0F, 100.0F, 20.0F)};
  const bool shown[] = {true, false};
  HudSiblings siblings;
  siblings.frames = frames;
  siblings.shown = std::span<const bool>(shown, 2);
  siblings.mine = 0;

  CHECK(TouchingFrames(siblings, 0) == 1U);
}

TEST_CASE("Cadre de HUD : le cadre d'un autre HUD aimante sans partir en bloc", "[ui][hud]") {
  // A et C sont du même HUD ; B, entre eux, appartient à un autre. Le bloc ne
  // l'emporte pas, et ne passe pas par lui pour atteindre C.
  std::vector<HudRect> frames{
      Frame(0.0F, 0.0F, 100.0F, 20.0F),    // A
      Frame(200.0F, 0.0F, 100.0F, 20.0F),  // C, loin de A
      Frame(100.0F, 0.0F, 100.0F, 20.0F),  // B, l'autre HUD, collé aux deux
  };
  HudSiblings siblings;
  siblings.frames = frames;
  siblings.mine = 0;
  siblings.movable = 2;
  CHECK(TouchingFrames(siblings, 0) == 1U);

  // Mais ses bords aimantent : A, qui chevauche B de trois pixels, vient
  // coller son bord droit au bord gauche de B.
  CHECK(SnappedToSiblings(3.0F, 100.0F, siblings, false) == 0.0F);
}

TEST_CASE("Cadre de HUD : retailler ne déplace pas le bord opposé", "[ui][hud]") {
  // 🔴 L'ERREUR CLASSIQUE : retailler en changeant la LARGEUR fait glisser le
  // cadre quand on tire son bord gauche. La géométrie doit retomber des quatre
  // positions de bord, et les bords non saisis rester épinglés.
  const HudRect rect = Frame(100.0F, 200.0F, 300.0F, 80.0F);
  HudFrameLook look;

  // On tire le bord gauche vers la gauche : le droit ne bouge pas.
  const HudRect wider = ResizedTo(rect, ro::ui::kHudEdgeLeft, ImVec2(50.0F, 240.0F),
                                  ImVec2(0.0F, 0.0F), look, kScreen);
  CHECK(wider.x == 50.0F);
  CHECK(wider.x + wider.width == 400.0F);
  CHECK(wider.y == 200.0F);
  CHECK(wider.height == 80.0F);

  // Et le bord bas seul ne touche ni x, ni y, ni la largeur.
  const HudRect taller = ResizedTo(rect, ro::ui::kHudEdgeBottom, ImVec2(250.0F, 400.0F),
                                   ImVec2(0.0F, 0.0F), look, kScreen);
  CHECK(taller.x == 100.0F);
  CHECK(taller.y == 200.0F);
  CHECK(taller.width == 300.0F);
  CHECK(taller.height == 200.0F);
}

TEST_CASE("Cadre de HUD : la taille minimale arrête la retaille", "[ui][hud]") {
  const HudRect rect = Frame(100.0F, 200.0F, 300.0F, 80.0F);
  HudFrameLook look;
  look.least_width = 20.0F;

  // On pousse le bord gauche au-delà du droit : il s'arrête au minimum, et le
  // cadre ne se retourne pas.
  const HudRect pinched = ResizedTo(rect, ro::ui::kHudEdgeLeft, ImVec2(900.0F, 240.0F),
                                    ImVec2(0.0F, 0.0F), look, kScreen);
  CHECK(pinched.width == 20.0F);
  CHECK(pinched.x + pinched.width == 400.0F);
}

TEST_CASE("cadre : posé à droite, il suit le bord droit", "[ui][hud]") {
  const ImVec2 wide(1920.0F, 1080.0F);
  const ImVec2 narrow(1024.0F, 768.0F);
  // Collé en bas à droite de l'écran large, à 10 et 20 pixels des bords.
  const HudRect placed = Frame(1920.0F - 10.0F - 200.0F, 1080.0F - 20.0F - 30.0F, 200.0F, 30.0F);
  const HudRect stored = ro::ui::HudAnchored(placed, wide);
  CHECK(stored.across == static_cast<int>(ro::ui::AnchorSide::kEnd));
  CHECK(stored.down == static_cast<int>(ro::ui::AnchorSide::kEnd));

  const HudRect small = ro::ui::HudOnScreen(stored, narrow);
  CHECK(small.x == 1024.0F - 10.0F - 200.0F);
  CHECK(small.y == 768.0F - 20.0F - 30.0F);
  // Et revenir à l'écran large rend la place d'origine.
  const HudRect back = ro::ui::HudOnScreen(stored, wide);
  CHECK(back.x == placed.x);
  CHECK(back.y == placed.y);
}

TEST_CASE("cadre : une taille relue plus grande que l'écran se montre à sa taille", "[ui][hud]") {
  // Une hauteur éditée à la main dans le fichier ne commande pas une surface
  // démesurée à qui dessine dans le cadre.
  const HudRect stored = Frame(20.0F, 30.0F, 10.0F, 1.0e7F);
  const HudRect shown = ro::ui::HudOnScreen(stored, ImVec2(1024.0F, 768.0F));
  CHECK(shown.width == 10.0F);
  CHECK(shown.height == 768.0F);
  CHECK(shown.y == 0.0F);
}

TEST_CASE("cadre : sans ancre, la place se lit depuis le coin haut-gauche", "[ui][hud]") {
  // C'est le sens de ce qui a été retenu avant l'ancrage : il ne change pas.
  const HudRect stored = Frame(300.0F, 400.0F, 50.0F, 20.0F);
  const HudRect shown = ro::ui::HudOnScreen(stored, ImVec2(1024.0F, 768.0F));
  CHECK(shown.x == 300.0F);
  CHECK(shown.y == 400.0F);
}
