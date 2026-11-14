// Le contexte d'interface, SANS FENÊTRE NI CARTE GRAPHIQUE.
//
// C'est le premier étage des tests — aucune I/O — appliqué à ce qui n'y était
// jamais : une fenêtre ne se vérifiait qu'à l'œil, et l'audit qui a fondé nos
// règles impute trois bugs critiques à un `OnRenderUI` de 1 702 lignes que
// personne ne relisait. Une fenêtre qui se construit et se mesure ici est une fenêtre dont
// on peut dire ce qu'elle fait.

#include "ro/ui/ui_context.h"

#include <catch2/catch_test_macros.hpp>

#include "imgui.h"
// ⚠ POUR POSER L'ELEMENT ACTIF A LA MAIN : c'est l'etat qu'un bouton enfonce ou
// une source de glisser installe, et rien d'autre ne le simule.
#include "imgui_internal.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

// Une fenêtre POSÉE, à une place et une taille connues : c'est ce qui permet de
// dire où est son intérieur, donc de viser dedans et à côté.
constexpr ImVec2 kWindowOrigin{100.0F, 80.0F};
constexpr ImVec2 kWindowSize{300.0F, 200.0F};

void DrawPlacedWindow() {
  ImGui::SetNextWindowPos(kWindowOrigin);
  ImGui::SetNextWindowSize(kWindowSize);
  ImGui::Begin("essai");
  ImGui::TextUnformatted("quelque chose");
  ImGui::End();
}

}  // namespace

TEST_CASE("une trame s'ouvre et se ferme sans dos", "[ui]") {
  ro::ui::UiContext ui;
  REQUIRE_FALSE(ui.frame_open());

  ui.BeginFrame(kViewport, kFrameSeconds);
  CHECK(ui.frame_open());
  DrawPlacedWindow();
  ui.EndFrame();
  CHECK_FALSE(ui.frame_open());

  // La liste de dessin existe bel et bien : la fenêtre a été COMPOSÉE, et pas
  // seulement acceptée sans erreur.
  ImGui::SetCurrentContext(ui.handle());
  const ImDrawData* const drawn = ImGui::GetDrawData();
  REQUIRE(drawn != nullptr);
  CHECK(drawn->Valid);
  CHECK(drawn->TotalVtxCount > 0);
}

TEST_CASE("deux contextes vivent ensemble sans se mêler", "[ui]") {
  // Ce que cette propriété achète : un test n'a rien à ordonner, et une fuite
  // ne traverse pas jusqu'au test suivant. C'est aussi ce qui manquerait à un
  // singleton — voir CLAUDE.md, et les 789 sites d'appel mesurés ailleurs.
  ro::ui::UiContext first;
  ro::ui::UiContext second;

  first.BeginFrame(kViewport, kFrameSeconds);
  DrawPlacedWindow();
  first.EndFrame();

  second.BeginFrame(kViewport, kFrameSeconds);
  second.EndFrame();

  ImGui::SetCurrentContext(first.handle());
  const int drawn_by_first = ImGui::GetDrawData()->TotalVtxCount;
  ImGui::SetCurrentContext(second.handle());
  const int drawn_by_second = ImGui::GetDrawData()->TotalVtxCount;

  CHECK(drawn_by_first > 0);
  CHECK(drawn_by_second == 0);
}

TEST_CASE("deux contextes gardent chacun leur choix de police", "[ui]") {
  // 🔴 LE CHOIX VIVAIT DANS DES GLOBALES DE FICHIER : poser celui du premier
  // contexte changeait celui du second, et détruire l'un laissait à l'autre des
  // polices d'un atlas libéré.
  ro::ui::UiContext first;
  ro::ui::UiContext second;
  first.ChooseFont(-1);
  CHECK(first.chosen_font() == -1);
  CHECK(second.chosen_font() == 0);
}

TEST_CASE("le laissez-passer se ferme sous une fenêtre et reste ouvert à côté", "[ui]") {
  // LA RAISON D'ÊTRE DE TOUTE LA COUCHE. La boucle décidait jusqu'ici par un
  // ordre écrit à la main — « le banc d'abord, il est dessiné par-dessus » — et
  // son défaut ne se voyait pas : un clic destiné à un panneau faisait aussi
  // marcher le personnage dessous.
  ro::ui::UiContext ui;

  const auto frame_at = [&ui](float x_px, float y_px) {
    ui.SetPointer(x_px, y_px, false);
    ui.BeginFrame(kViewport, kFrameSeconds);
    DrawPlacedWindow();
    ui.EndFrame();
  };

  // DEUX trames par visée : ImGui ne sait ce qu'il y a sous le curseur qu'une
  // fois les fenêtres dessinées avec cette position-là.
  const ImVec2 inside{kWindowOrigin.x + kWindowSize.x / 2.0F,
                      kWindowOrigin.y + kWindowSize.y / 2.0F};
  frame_at(inside.x, inside.y);
  frame_at(inside.x, inside.y);
  CHECK(ui.gate().mouse_to_ui);

  const ImVec2 outside{kWindowOrigin.x + kWindowSize.x + 200.0F,
                       kWindowOrigin.y + kWindowSize.y + 200.0F};
  frame_at(outside.x, outside.y);
  frame_at(outside.x, outside.y);
  CHECK_FALSE(ui.gate().mouse_to_ui);
}

TEST_CASE("un bouton TENU n'est pas une prise de clavier", "[ui]") {
  // 🔴 IMGUI LEVE SA CAPTURE DES QU'UN ELEMENT EST ACTIF -- un bouton enfonce,
  // une poignee tiree, un objet qu'on GLISSE d'une fenetre a l'autre. Aucun de
  // ces gestes n'attend de touche, et pourtant tous les raccourcis du jeu
  // tombaient : attraper un objet dans le sac eteignait le clavier entier.
  // C'est justement en tenant quelque chose qu'on veut ouvrir un contenant.
  //
  // `typing_to_ui` est ce qui les separe : la capture est la, mais elle ne vient
  // pas d'une saisie -- les touches que le jeu seul comprend lui reviennent.
  ro::ui::UiContext ui;

  // DEUX TRAMES, et il en faut deux : ImGui arrete sa capture au DEBUT d'une
  // image, d'apres ce que la precedente a laisse. L'element pose dans la
  // premiere ne se lit donc qu'a la seconde.
  for (int frame = 0; frame < 2; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::Begin("essai");
    ImGui::Button("tenu");
    // L'element actif, pose a la main : c'est exactement ce que fait un bouton
    // qu'on enfonce, ou une source de glisser.
    ImGui::SetActiveID(ImGui::GetID("tenu"), ImGui::GetCurrentWindow());
    ImGui::End();
    ui.EndFrame();
  }

  const ro::ui::InputGate held = ui.gate();
  // La capture est bien levee -- c'est elle qui eteignait tout.
  CHECK(held.keyboard_to_ui);
  // 🔴 ET VOICI CE QUI CHANGE : l'element tenu se range du cote de la SAISIE et
  // non de la prise TOTALE. Meme liberation ciblee, meme critere -- les touches
  // qu'aucun champ ne peut utiliser reviennent au jeu. Une modale, elle,
  // leverait la capture SANS lever celui-ci, et garderait donc tout.
  CHECK(held.typing_to_ui);
  // Rien ne s'ecrit pour autant : aucun champ n'attend de caractere.
  CHECK_FALSE(held.text_to_ui);
}

TEST_CASE("une trame laissée ouverte se ferme à la destruction", "[ui]") {
  // Sans cela, ImGui garderait de l'état pointant dans un contexte détruit, et
  // ASan le dirait bien plus tard — dans le test suivant, sur une pile qui n'y
  // est pour rien.
  {
    ro::ui::UiContext ui;
    ui.BeginFrame(kViewport, kFrameSeconds);
    DrawPlacedWindow();
  }
  ro::ui::UiContext next;
  next.BeginFrame(kViewport, kFrameSeconds);
  next.EndFrame();
  CHECK_FALSE(next.frame_open());
}

TEST_CASE("un temps nul ne fait pas échouer la trame", "[ui]") {
  // Deux images dans la même milliseconde arrivent sur une machine rapide, et
  // ImGui refuse un temps nul par assertion : le plancher vaut mieux qu'un
  // refus, parce que l'appelant n'a rien fait de faux.
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, 0.0F);
  ui.EndFrame();
  ImGui::SetCurrentContext(ui.handle());
  CHECK(ImGui::GetIO().DeltaTime > 0.0F);
}

// ── L'ÉCRAN QUI DISPARAÎT ───────────────────────────────────────────────────

TEST_CASE("une fenêtre réduite garde le dernier écran connu", "[ui]") {
  // 🔴 CE QUE LE PONT SDL3 ENVOIE quand la fenêtre passe dans la barre des
  // tâches : `w = h = 0`, posé par lui et non par nous. Tout ce qui se mesure
  // en part d'écran vaudrait alors zéro — et un plafond de taille n'est pas un
  // rognage : il ÉCRIT dans la fenêtre, qui ne s'en relève pas au retour.
  constexpr ro::ui::Viewport kMinimized{.width_px = 0.0F, .height_px = 0.0F};

  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  ui.EndFrame();

  ui.BeginFrame(kMinimized, kFrameSeconds);
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  ui.EndFrame();

  CHECK(screen.x == kViewport.width_px);
  CHECK(screen.y == kViewport.height_px);
}

TEST_CASE("un client lancé déjà réduit n'invente pas d'écran", "[ui]") {
  // ⚠ L'AUTRE MOITIÉ DE LA RÈGLE, et elle décide du repli : il n'y a pas de
  // dernier écran connu à rendre. Inventer une taille poserait les fenêtres
  // d'après un écran que le joueur n'a pas ; les bornes, elles, savent déjà se
  // taire devant un écran nul.
  constexpr ro::ui::Viewport kMinimized{.width_px = 0.0F, .height_px = 0.0F};

  ro::ui::UiContext ui;
  ui.BeginFrame(kMinimized, kFrameSeconds);
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  ui.EndFrame();

  CHECK(screen.x == 0.0F);
  CHECK(screen.y == 0.0F);
}
