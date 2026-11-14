// LA FENÊTRE DE DIALOGUE, DESSINÉE : ce que le clavier y fait, et ce qu'un
// filtre change au choix qui part.
//
// 🔴 C'EST LE CHOIX SOUS FILTRE QUI COMPTE LE PLUS ICI. Les chiffres désignent la
// N-ième option VISIBLE, et la position qu'ils rendent est lue plus loin par la
// couche qui la traduit en rang de protocole. Se tromper d'un cran n'affiche pas
// une erreur : cela répond autre chose au script, ou fait ÉJECTER le joueur.

#include "ro/ui/npc_dialog_window.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::ui::NpcDialogAnswer;
using ro::ui::NpcDialogArt;
using ro::ui::NpcDialogIntent;
using ro::ui::NpcDialogPrompt;
using ro::ui::NpcDialogView;
using ro::ui::NpcDialogWindowState;

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

// Une conversation ouverte, dessinée image par image.
struct DialogProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  NpcDialogArt media;
  NpcDialogWindowState state;
  std::vector<std::string> lines = {"Bonjour, aventurier."};
  std::vector<std::string_view> options;
  NpcDialogView view;

  NpcDialogAnswer Frame() {
    view.lines = lines;
    // ⚠ LE SPAN SE PREND APRÈS LE REMPLISSAGE, à chaque image : un span pris sur
    // un vecteur qu'on remplit ensuite meurt à la première réallocation.
    view.options = options;
    ui.BeginFrame(kViewport, kFrameSeconds);
    NpcDialogAnswer answer = ro::ui::DrawNpcDialogWindow(state, view, art, media);
    ui.EndFrame();
    return answer;
  }

  void Settle() {
    for (int frame = 0; frame < 4; ++frame) {
      static_cast<void>(Frame());
    }
  }

  // Une frappe : enfoncée à une image, relâchée à la suivante — c'est ainsi
  // qu'ImGui reconnaît un appui.
  [[nodiscard]] NpcDialogAnswer Press(ImGuiKey key) {
    ImGui::GetIO().AddKeyEvent(key, true);
    NpcDialogAnswer answer = Frame();
    ImGui::GetIO().AddKeyEvent(key, false);
    static_cast<void>(Frame());
    return answer;
  }
};

}  // namespace

TEST_CASE("Dialogue : la barre de titre porte le nom du PNJ", "[ui][npc]") {
  // ⚠ LE CLIENT D'ORIGINE NE LE DIT NULLE PART, et c'est ce qu'on y
  // ajoute : l'identité reste la même quel que soit le nom, sinon la fenêtre
  // perdrait sa place et sa taille à chaque PNJ.
  DialogProbe probe;
  probe.view.name = "Kafra";
  probe.view.prompt = NpcDialogPrompt::kNext;
  probe.Settle();
  const ImGuiWindow* window = ImGui::FindWindowByName("Kafra###npc_dialog");
  REQUIRE(window != nullptr);
}

TEST_CASE("Dialogue : Entrée et Espace valent « Suivant »", "[ui][npc]") {
  DialogProbe probe;
  probe.view.prompt = NpcDialogPrompt::kNext;
  probe.Settle();
  CHECK(probe.Press(ImGuiKey_Enter).intent == NpcDialogIntent::kNext);
  CHECK(probe.Press(ImGuiKey_Space).intent == NpcDialogIntent::kNext);
}

TEST_CASE("Dialogue : en attente de réponse, le clavier ne répond plus", "[ui][npc]") {
  // 🔴 SANS CELA, UN SECOND APPUI FAIT AVANCER LE SCRIPT D'UNE PAGE DE PLUS. La
  // page reste affichée pendant l'aller-retour, et `BeginDisabled` ne couvre PAS
  // le clavier : il faut l'exclure en toutes lettres.
  DialogProbe probe;
  probe.view.prompt = NpcDialogPrompt::kNext;
  probe.view.awaiting_reply = true;
  probe.Settle();
  CHECK(probe.Press(ImGuiKey_Enter).intent == NpcDialogIntent::kNothing);
}

TEST_CASE("Dialogue : en attente de réponse, un chiffre ne choisit plus", "[ui][npc]") {
  // Le même piège sur le MENU : les chiffres et Entrée émettaient un second
  // choix pendant l'aller-retour, que le serveur refuse.
  DialogProbe probe;
  probe.view.prompt = NpcDialogPrompt::kMenu;
  probe.options = {"Prontera", "Geffen", "Payon"};
  probe.view.awaiting_reply = true;
  probe.Settle();
  CHECK(probe.Press(ImGuiKey_2).intent == NpcDialogIntent::kNothing);
  CHECK(probe.Press(ImGuiKey_Enter).intent == NpcDialogIntent::kNothing);
}

TEST_CASE("Dialogue : le nombre tapé est borné à 32 bits", "[ui][npc]") {
  // Seize chiffres dépassent un entier 32 bits : `atoi` y est indéfini.
  using ro::ui::ParsedNumber;
  CHECK(ParsedNumber("1234") == 1234);
  CHECK(ParsedNumber("-7") == -7);
  CHECK(ParsedNumber("") == 0);
  CHECK(ParsedNumber("9999999999999999") == 2'147'483'647);
  CHECK(ParsedNumber("-9999999999999999") == -2'147'483'647 - 1);
}

TEST_CASE("Dialogue : un chiffre choisit la N-ième option VISIBLE", "[ui][npc]") {
  DialogProbe probe;
  probe.view.prompt = NpcDialogPrompt::kMenu;
  probe.options = {"Prontera", "Geffen", "Payon"};
  probe.Settle();

  const NpcDialogAnswer second = probe.Press(ImGuiKey_2);
  REQUIRE(second.intent == NpcDialogIntent::kChoose);
  CHECK(second.option_index == 1);
}

TEST_CASE("Dialogue : sous filtre, le chiffre suit l'AFFICHAGE", "[ui][npc]") {
  // 🔴 LE CAS QUI SE TROMPE TOUT SEUL. Le filtre ne laisse que « Payon » : la
  // touche 1 doit rendre SA position — la troisième — et non la première option
  // de la liste entière.
  DialogProbe probe;
  probe.view.prompt = NpcDialogPrompt::kMenu;
  probe.options = {"Prontera", "Geffen", "Payon"};
  probe.Settle();
  const std::string_view wanted = "payon";
  std::copy(wanted.begin(), wanted.end(), probe.state.menu_filter.begin());
  probe.state.menu_filter[wanted.size()] = '\0';
  probe.Settle();

  const NpcDialogAnswer chosen = probe.Press(ImGuiKey_1);
  REQUIRE(chosen.intent == NpcDialogIntent::kChoose);
  CHECK(chosen.option_index == 2);
}

TEST_CASE("Dialogue : une page neuve vide le filtre du menu précédent", "[ui][npc]") {
  // ⚠ SINON LE FILTRE D'UN MENU MASQUE LES OPTIONS DU SUIVANT, et le joueur voit
  // une liste vide sans savoir pourquoi.
  DialogProbe probe;
  probe.view.prompt = NpcDialogPrompt::kMenu;
  probe.options = {"Prontera", "Geffen"};
  probe.Settle();
  probe.state.menu_filter[0] = 'x';
  probe.state.menu_filter[1] = '\0';
  probe.state.menu_hot = 1;

  ++probe.view.page;
  probe.Settle();
  CHECK(probe.state.menu_filter[0] == '\0');
  CHECK(probe.state.menu_hot == 0);
}

TEST_CASE("Dialogue : un menu renvoyé réécrit se relit, même page", "[ui][npc]") {
  // 🔴 LE RÉGLAGE QUI BASCULE : le script renvoie la même liste, un libellé
  // changé, sans une ligne de texte. Ni la page ni le nombre d'options ne bougent.
  DialogProbe probe;
  probe.view.prompt = NpcDialogPrompt::kMenu;
  probe.options = {"Villes", "Detail (ON)"};
  probe.Settle();
  REQUIRE(probe.state.menu_lowered.size() == 2);
  CHECK(probe.state.menu_lowered[1] == "detail (on)");

  probe.options = {"Villes", "Detail (OFF)"};
  ++probe.view.menu;
  probe.Settle();
  CHECK(probe.state.menu_lowered[1] == "detail (off)");
}

TEST_CASE("Dialogue : une image de débordement ne rogne pas le pied", "[ui][npc]") {
  // 🔴 LE PIED NE DOIT JAMAIS RESTER ROGNÉ. Pendant qu'on tire un bord, la
  // fenêtre déborde une image : son contenu a été posé pour la taille d'avant.
  // Si ce débordement faisait poser la découpe d'une fenêtre qui défile, il
  // allongerait le contenu de l'épaisseur du cadre et s'entretiendrait seul.
  DialogProbe probe;
  // ⚠ UN CADRE PLUS ÉPAIS QUE LA MARGE, comme celui du jeu : sinon la découpe
  // ne retranche rien et le défaut ne se voit pas.
  constexpr float kThickFramePixels = 20.0F;
  probe.art.Set(
      ro::render::SkinPiece::kFrameLeft,
      ro::ui::SkinImage{.id = 1, .width = kThickFramePixels, .height = kThickFramePixels});
  probe.view.name = "Warp";
  probe.view.prompt = NpcDialogPrompt::kMenu;
  probe.options = {"Un", "Deux", "Trois", "Quatre", "Cinq", "Six", "Sept", "Huit", "Neuf", "Dix"};
  probe.Settle();
  ImGuiWindow* window = ImGui::FindWindowByName("Warp###npc_dialog");
  REQUIRE(window != nullptr);
  REQUIRE(window->ScrollMax.y == 0.0F);

  // L'image de trop : ce que la précédente a posé dépasse la fenêtre.
  constexpr float kOverflowPixels = 40.0F;
  window->DC.CursorMaxPos.y += kOverflowPixels;
  probe.Settle();
  CHECK(window->ScrollMax.y == 0.0F);
}

TEST_CASE("Dialogue : un « suivant » ne se ferme pas", "[ui][npc]") {
  // 🔴 PAS DE CROIX PENDANT UN `next`, et ce n'est pas un oubli : y envoyer une
  // fermeture ferait AVANCER le script au lieu de le quitter. Le client
  // d'origine n'offre d'ailleurs que « Suivant » à ce moment.
  //
  // ⚠ CE BANC BAISSE L'INTERRUPTEUR LUI-MÊME : c'est la pile d'Échap qui le fait
  // en jeu, après le dessin, et elle vit chez l'étage. L'étage, lui, ne l'inscrit
  // que sur une page fermable — ce qui est l'autre moitié de cette règle.
  DialogProbe probe;
  probe.view.prompt = NpcDialogPrompt::kNext;
  probe.Settle();
  probe.state.open = false;
  const NpcDialogAnswer pendant_next = probe.Frame();
  CHECK(pendant_next.intent == NpcDialogIntent::kNothing);
  // Le drapeau se relève : la fenêtre suit le monde, pas un interrupteur.
  CHECK(probe.state.open);

  // Un « fermer », lui, se ferme.
  probe.view.prompt = NpcDialogPrompt::kClose;
  probe.Settle();
  probe.state.open = false;
  CHECK(probe.Frame().intent == NpcDialogIntent::kClose);
  CHECK(probe.state.open);
}

TEST_CASE("Dialogue : la mise en page ne se refait qu'au changement de page", "[ui][npc]") {
  // 🔴 CALCULÉE UNE FOIS PAR PAGE ET PAR LARGEUR, jamais par image : une page de
  // deux cents lignes payait son propre découpage soixante fois par seconde
  // pendant qu'elle arrivait.
  DialogProbe probe;
  probe.lines = {"Une premiere ligne", "Une seconde ligne"};
  probe.view.prompt = NpcDialogPrompt::kClose;
  probe.Settle();
  const std::size_t placed = probe.state.laid.fragments.size();
  REQUIRE(placed > 0);
  CHECK(probe.state.laid_page == probe.view.page);

  probe.lines.push_back("Une troisieme");
  probe.Settle();
  // La page n'a pas changé de génération : ce qu'on ajoute derrière ne se voit
  // pas encore, et c'est voulu — la page se publie d'un seul tenant.
  CHECK(probe.state.laid.fragments.size() == placed);

  ++probe.view.page;
  probe.Settle();
  CHECK(probe.state.laid.fragments.size() > placed);
}
