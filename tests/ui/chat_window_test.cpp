// La chatbox : sa grille de lignes, son repli, et ce que son cadre refuse.

#include "ro/ui/chat_window.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "ro/ui/chat_frame.h"
#include "ro/ui/chat_input.h"
#include "ro/ui/chat_log.h"
#include "ro/ui/chat_strip.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
// De quoi loger la rangée de saisie, et une fenêtre assez étroite pour que la
// largeur de l'étiquette pèse sur celle du champ.
constexpr float kRoomForInput = 200.0F;
constexpr float kNarrowWindow = 420.0F;
// Un point qui tombe DANS le champ de saisie de cette rangée-là.
constexpr float kInTheField = 330.0F;
constexpr float kOnTheRow = 32.0F;

constexpr float kFrameSeconds = 1.0F / 60.0F;
// Aucune pièce d'habillage : la rangée doit tenir sur les seuls replis.
const ro::ui::SkinArt kNoArt;
// Une taille de texte franche, pour que les écarts d'un pixel se voient.
constexpr float kFontSize = 20.0F;

[[nodiscard]] ro::ui::ChatLogStyle StyleOf(float line_gap) {
  return ro::ui::ChatLogStyle{.font_size = kFontSize, .line_gap = line_gap, .timestamps = false};
}

}  // namespace

TEST_CASE("Chatbox : le pas d'une ligne est plus serré que son texte", "[ui][chatbox]") {
  // ⚠ C'EST LE CALAGE DU SKIN, ET IL N'EST PAS DÉCORATIF : l'interligne réglable
  // s'ajoute à un pas qui démarre SOUS la hauteur du texte, parce que celle
  // d'ImGui est très aérée pour du chat. Un pas égal à la taille du texte
  // donnerait une chatbox qui montre un tiers de lignes en moins.
  const float tight = ro::ui::ChatLineStep(StyleOf(0.0F));
  CHECK(tight == kFontSize - ro::ui::kChatLineTightening);
  // L'interligne s'y ajoute, pixel pour pixel.
  CHECK(ro::ui::ChatLineStep(StyleOf(5.0F)) == tight + 5.0F);
}

TEST_CASE("Chatbox : le pas ne dégénère jamais", "[ui][chatbox]") {
  // ⚠ UN PAS NUL SERAIT UNE DIVISION PAR ZÉRO À L'ARRONDI DU DÉFILEMENT, pas un
  // défaut d'affichage. Un texte minuscule ne doit pas produire ça.
  const ro::ui::ChatLogStyle crushed{.font_size = 1.0F, .line_gap = 0.0F, .timestamps = false};
  CHECK(ro::ui::ChatLineStep(crushed) == ro::ui::kChatSmallestLineStep);
}

TEST_CASE("Chatbox : le dernier glyphe dépasse sous son pas", "[ui][chatbox]") {
  // ⚠ L'OUBLIER ROGNE LA LIGNE LA PLUS RÉCENTE EN PERMANENCE : le cumul des pas
  // s'arrête au pas de la dernière ligne, pas sous son glyphe, et c'est ce cumul
  // qui borne le défilement.
  CHECK(ro::ui::ChatLineOverhang(StyleOf(0.0F)) > 0.0F);
  // Interligne large : le pas passe devant le texte, et il n'y a plus de débord.
  CHECK(ro::ui::ChatLineOverhang(StyleOf(40.0F)) == 0.0F);
}

TEST_CASE("Chatbox : une ligne trop longue se replie", "[ui][chatbox]") {
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);

  const std::string sentence = "Le lapin bleu traverse la place de Prontera en sifflant.";
  const std::array<ro::ui::ChatLineView, 1> lines = {ro::ui::ChatLineView{.text = sentence}};
  const ro::ui::ChatLogStyle style = StyleOf(2.0F);
  const float step = ro::ui::ChatLineStep(style);

  // Large : tout tient sur une rangée.
  const float roomy = ro::ui::ChatLogHeight(lines, style, 4000.0F);
  CHECK(roomy < 2.0F * step);
  // Étroite : la même phrase en occupe plusieurs.
  const float narrow = ro::ui::ChatLogHeight(lines, style, 120.0F);
  CHECK(narrow > 3.0F * step);

  ui.EndFrame();
}

TEST_CASE("Chatbox : un mot plus large que la zone se coupe", "[ui][chatbox]") {
  // ⚠ SANS LA COUPE AU CARACTÈRE, le repli mot à mot n'a rien à couper : il
  // descend le mot d'une rangée, où il déborde exactement pareil, et la fin du
  // texte sort de la fenêtre. Un « AAAA…A » de trois cents caractères, une
  // adresse sans espace : tous les jours dans un chat public.
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);

  const std::string wall(300, 'A');
  const std::array<ro::ui::ChatLineView, 1> lines = {ro::ui::ChatLineView{.text = wall}};
  const ro::ui::ChatLogStyle style = StyleOf(2.0F);
  CHECK(ro::ui::ChatLogHeight(lines, style, 100.0F) > 4.0F * ro::ui::ChatLineStep(style));

  ui.EndFrame();
}

TEST_CASE("Chatbox : une zone d'un pixel ne fige pas le client", "[ui][chatbox]") {
  // ⚠ C'EST UN GEL, PAS UN DÉFAUT D'AFFICHAGE : toute la largeur et même pas un
  // glyphe, le point d'arrêt ne bouge pas, le curseur non plus, et la boucle
  // tourne pour toujours. On pose un caractère quoi qu'il arrive.
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);

  const std::array<ro::ui::ChatLineView, 1> lines = {ro::ui::ChatLineView{.text = "abcdef"}};
  CHECK(ro::ui::ChatLogHeight(lines, StyleOf(2.0F), 1.0F) > 0.0F);

  ui.EndFrame();
}

TEST_CASE("Chatbox : un saut de ligne descend d'une rangée", "[ui][chatbox]") {
  // ⚠ SANS CE CAS, le mot qui suit le saut est passé tel quel au dessin : la
  // MESURE s'arrête au saut et rend une largeur nulle, tandis que le RENDU
  // l'honore et pose le mot une rangée plus bas, au `x` courant — donc en plein
  // milieu du vide.
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);

  const ro::ui::ChatLogStyle style = StyleOf(2.0F);
  const std::array<ro::ui::ChatLineView, 1> flat = {ro::ui::ChatLineView{.text = "un deux"}};
  const std::array<ro::ui::ChatLineView, 1> broken = {ro::ui::ChatLineView{.text = "un\ndeux"}};
  CHECK(ro::ui::ChatLogHeight(broken, style, 4000.0F) >
        ro::ui::ChatLogHeight(flat, style, 4000.0F));

  ui.EndFrame();
}

TEST_CASE("Chatbox : son cadre refuse le redimensionnement d'ImGui", "[ui][chatbox]") {
  // ⚠ `NoResize` COUPE LES POIGNÉES D'IMGUI, coins compris — aucun drapeau ne
  // sait les désactiver une par une. Ce sont nos quatre bords qui
  // redimensionnent, et eux seuls savent se taire quand ils touchent l'écran.
  // Et pas de barre de titre : la chatbox du client n'en a pas.
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const ro::ui::ChatStyle style;
  static_cast<void>(ro::ui::BeginChatFrame("conversation", style));
  const ImGuiWindow* window = ImGui::GetCurrentWindow();
  REQUIRE(window != nullptr);
  const ImGuiWindowFlags flags = window->Flags;
  ro::ui::EndChatFrame();
  ui.EndFrame();

  CHECK((flags & ImGuiWindowFlags_NoResize) != 0);
  CHECK((flags & ImGuiWindowFlags_NoTitleBar) != 0);
  CHECK((flags & ImGuiWindowFlags_NoScrollbar) != 0);
  // Déplaçable tant qu'on ne l'a pas figée : le verrou est un réglage, pas un
  // état du cadre.
  CHECK((flags & ImGuiWindowFlags_NoMove) == 0);
}

TEST_CASE("Chatbox : figée, elle ne se déplace plus", "[ui][chatbox]") {
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  ro::ui::ChatStyle style;
  style.movable = false;
  static_cast<void>(ro::ui::BeginChatFrame("conversation_figee", style));
  const ImGuiWindowFlags flags = ImGui::GetCurrentWindow()->Flags;
  ro::ui::EndChatFrame();
  ui.EndFrame();

  CHECK((flags & ImGuiWindowFlags_NoMove) != 0);
}

namespace {

// Une fenêtre ordinaire, juste assez large pour que la saisie ait de la place.
// ⚠ `ChatInputHeight` LIT LA RÉGION COURANTE : hors fenêtre, elle ne mesurerait
// rien de ce qu'elle est censée mesurer.
void InWindow(float width, const std::function<void()>& body) {
  ImGui::SetNextWindowSize(ImVec2(width, 200.0F));
  ImGui::Begin("banc_de_saisie");
  body();
  ImGui::End();
}

void Fill(ro::ui::ChatInputState& state, std::string_view text) {
  const std::size_t kept = std::min(text.size(), state.text.size() - 1);
  std::copy_n(text.data(), kept, state.text.data());
  state.text[kept] = '\0';
}

}  // namespace

TEST_CASE("Saisie : une phrase longue fait grandir le champ", "[ui][chatbox]") {
  // ⚠ UN CHAMP D'UNE LIGNE QUI DÉFILE CACHE LE DÉBUT DE CE QU'ON ÉCRIT. Pour un
  // message dont la longueur est bornée et dont le dépassement est refusé par le
  // serveur sans un mot, ne pas voir ce qu'on a écrit est un piège.
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);

  ro::ui::ChatInputState state;
  const ro::ui::ChatLinePreview preview{.target = "Alentour"};
  float one = 0.0F;
  float many = 0.0F;
  InWindow(300.0F, [&] {
    one = ro::ui::ChatInputHeight(state, preview, 400.0F, kNoArt);
    Fill(state, std::string(400, 'a'));
    many = ro::ui::ChatInputHeight(state, preview, 400.0F, kNoArt);
  });
  ui.EndFrame();

  CHECK(state.rows_wanted > 1);
  CHECK(many > one);
}

TEST_CASE("Menu d'onglet : la principale garde toujours un onglet", "[ui][chatbox]") {
  // 🔴 LES TROIS RÈGLES SE TIENNENT. C'est parce que la principale ne peut
  // ni fermer ni sortir son dernier onglet qu'une flottante a le droit de fermer
  // le sien : la conversation ne peut alors jamais disparaître en entier.
  constexpr std::uint32_t kMain = 0;
  constexpr std::uint32_t kFloating = 7;

  const ro::ui::ChatTabPowers alone = ro::ui::ChatTabMenuPowers(kMain, 1, 1);
  CHECK_FALSE(alone.can_close);
  CHECK_FALSE(alone.can_detach);

  const ro::ui::ChatTabPowers among = ro::ui::ChatTabMenuPowers(kMain, 3, 3);
  CHECK(among.can_close);
  CHECK(among.can_detach);

  // Une flottante à un onglet le ferme — elle s'efface avec lui — mais ne le
  // sort nulle part : il est déjà seul dans sa fenêtre.
  const ro::ui::ChatTabPowers floating = ro::ui::ChatTabMenuPowers(kFloating, 1, 4);
  CHECK(floating.can_close);
  CHECK_FALSE(floating.can_detach);
}

TEST_CASE("Menu d'onglet : le plafond compte TOUTES les fenêtres", "[ui][chatbox]") {
  // 🔴 C'EST LE CHARGEUR DU CLIENT qui refuse au-delà, pas notre bande. Un
  // plafond compté par fenêtre en laisserait créer trois fois trop, et les
  // surnuméraires disparaîtraient à la reconnexion suivante, sans un mot.
  constexpr std::uint32_t kFloating = 7;
  // Deux onglets ici, mais dix en tout : la bande a de la place, le client non.
  CHECK_FALSE(ro::ui::ChatTabMenuPowers(kFloating, 2, ro::ui::kMostTabs).can_add);
  CHECK(ro::ui::ChatTabMenuPowers(kFloating, 2, ro::ui::kMostTabs - 1).can_add);
}

TEST_CASE("Saisie : la boîte de destination garde sa largeur", "[ui][chatbox]") {
  // 🔴 ELLE MONTRE AUSSI LE PSEUDO QU'ON CHUCHOTE. Taillée sur son texte,
  // elle grandissait à chaque lettre tapée dans la box « Pseudo » — et la box,
  // poussée vers la droite, se dérobait sous le curseur en même temps que le
  // champ. La place du plus long libellé est la seule qui ne bouge jamais.
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);

  const std::array<ro::ui::ChatSendMode, 3> modes = {ro::ui::ChatSendMode{.label = "Alentour"},
                                                     ro::ui::ChatSendMode{.label = "Groupe"},
                                                     ro::ui::ChatSendMode{.label = "Alliés"}};
  ro::ui::ChatLinePreview preview{.target = "Alentour"};
  preview.modes = modes;

  float on_public = 0.0F;
  float on_party = 0.0F;
  float on_long_name = 0.0F;
  float widest_label = 0.0F;
  InWindow(300.0F, [&] {
    on_public = ro::ui::ChatTargetBoxWidth(preview, kNoArt);
    preview.target = "Groupe";
    on_party = ro::ui::ChatTargetBoxWidth(preview, kNoArt);
    preview.target = "UnPseudoBeaucoupTropLong";
    on_long_name = ro::ui::ChatTargetBoxWidth(preview, kNoArt);
    widest_label = ImGui::CalcTextSize("Alentour").x;
  });
  ui.EndFrame();

  CHECK(on_public == on_party);
  CHECK(on_public == on_long_name);
  // Et elle montre le plus long des libellés EN ENTIER : c'est ce qu'elle achète
  // en échange de sa largeur fixe.
  CHECK(on_public > widest_label);
}

TEST_CASE("Saisie : le champ qui ne grandit pas garde UNE rangée", "[ui][chatbox]") {
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);

  ro::ui::ChatInputState state;
  state.grow = false;
  const ro::ui::ChatLinePreview preview{.target = "Alentour"};
  float height = 0.0F;
  float one_row = 0.0F;
  InWindow(300.0F, [&] {
    Fill(state, std::string(400, 'a'));
    one_row = ImGui::GetFrameHeight();
    height = ro::ui::ChatInputHeight(state, preview, 400.0F, kNoArt);
  });
  ui.EndFrame();

  CHECK(state.rows_drawn == 1);
  CHECK(state.rows_wanted == 1);  // on ne lui a même pas demandé de compter
  CHECK(height == one_row);
}

TEST_CASE("Saisie : le champ ne dépasse pas ce qu'on lui cède", "[ui][chatbox]") {
  // ⚠ LE PLAFOND EST CE QUE L'APPELANT PEUT CÉDER, et il doit être TENU : une
  // rangée de plus que la place réservée, et le champ mord sur le fil.
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);

  ro::ui::ChatInputState state;
  const ro::ui::ChatLinePreview preview{.target = "Alentour"};
  float height = 0.0F;
  float ceiling = 0.0F;
  InWindow(300.0F, [&] {
    Fill(state, std::string(2000, 'a'));
    ceiling = ImGui::GetFrameHeight() + 2.0F * ImGui::GetFontSize();
    height = ro::ui::ChatInputHeight(state, preview, ceiling, kNoArt);
  });
  ui.EndFrame();

  CHECK(state.rows_wanted > state.rows_allowed);  // il en voudrait plus
  CHECK(state.rows_drawn == state.rows_allowed);  // il n'en prend pas plus
  CHECK(height <= ceiling);
}

TEST_CASE("Chatbox : l'étiquette de diagnostic pousse le texte", "[ui][chatbox]") {
  // ⚠ PEINTE MAIS NON MESURÉE, une étiquette recouvrirait le début de la phrase :
  // c'est le défaut classique d'un préfixe ajouté au dessin seul. Elle avance la
  // plume comme l'horodatage, donc elle fait replier plus tôt.
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);

  const std::string sentence = "Le lapin bleu traverse la place de Prontera en sifflant.";
  const std::array<ro::ui::ChatLineView, 1> bare = {ro::ui::ChatLineView{.text = sentence}};
  const std::array<ro::ui::ChatLineView, 1> tagged = {
      ro::ui::ChatLineView{.text = sentence, .tag = "chuchotement"}};
  const ro::ui::ChatLogStyle style = StyleOf(2.0F);

  constexpr float kNarrow = 200.0F;
  CHECK(ro::ui::ChatLogHeight(tagged, style, kNarrow) >
        ro::ui::ChatLogHeight(bare, style, kNarrow));
  // Et sans étiquette, rien ne change : hors diagnostic, aucune ligne n'en porte.
  const std::array<ro::ui::ChatLineView, 1> empty_tag = {
      ro::ui::ChatLineView{.text = sentence, .tag = {}}};
  CHECK(ro::ui::ChatLogHeight(empty_tag, style, kNarrow) ==
        ro::ui::ChatLogHeight(bare, style, kNarrow));

  ui.EndFrame();
}

// ── LE DÉPÔT D'UNE LANGUETTE ────────────────────────────────
//
// Ce verdict est de la logique PURE : des rectangles, un point, et des milieux.
// Il s'essaie donc sans fenêtre ni souris — ce qui est tout l'intérêt de l'avoir
// sorti de la bande qui a vu le lâcher.

namespace {

// Une bande de deux languettes, quelque part à l'écran.
[[nodiscard]] ro::ui::ChatDragBoard::Strip StripAt(std::uint32_t group, float left, float top) {
  ro::ui::ChatDragBoard::Strip strip;
  strip.group = group;
  strip.min = ImVec2(left, top);
  strip.max = ImVec2(left + 200.0F, top + 20.0F);
  strip.window_min = ImVec2(left, top);
  strip.window_max = ImVec2(left + 200.0F, top + 300.0F);
  strip.count = 2;
  strip.middles[0] = left + 25.0F;
  strip.middles[1] = left + 75.0F;
  return strip;
}

}  // namespace

TEST_CASE("Dépôt : une languette lâchée dans une autre bande la rejoint", "[ui][chatbox]") {
  ro::ui::ChatDragBoard board;
  board.strips = {StripAt(0, 0.0F, 0.0F), StripAt(7, 500.0F, 0.0F)};
  board.from_group = 0;
  board.from_tab = 1;
  board.released = true;
  // Entre les deux milieux de la bande d'en face : elle se range donc en second.
  board.at = ImVec2(560.0F, 10.0F);

  const ro::ui::ChatDrop drop = ro::ui::DecideChatDrop(board);
  REQUIRE(drop.asked);
  CHECK_FALSE(drop.to_new_window);
  CHECK(drop.to_group == 7);
  CHECK(drop.to_rank == 1);
  CHECK(drop.from_group == 0);
  CHECK(drop.from_tab == 1);
  // ⚠ LE PLATEAU SE VIDE DE SON GESTE : sans cela le même lâcher se rejouerait à
  // chaque image, et l'onglet repartirait en boucle.
  CHECK_FALSE(board.released);
  CHECK_FALSE(ro::ui::DecideChatDrop(board).asked);
}

TEST_CASE("Dépôt : au-delà du dernier milieu, elle va au bout", "[ui][chatbox]") {
  ro::ui::ChatDragBoard board;
  board.strips = {StripAt(0, 0.0F, 0.0F), StripAt(7, 500.0F, 0.0F)};
  board.from_group = 0;
  board.from_tab = 0;
  board.released = true;
  board.at = ImVec2(690.0F, 10.0F);

  const ro::ui::ChatDrop drop = ro::ui::DecideChatDrop(board);
  REQUIRE(drop.asked);
  CHECK(drop.to_group == 7);
  CHECK(drop.to_rank == 2);
}

TEST_CASE("Dépôt : lâchée loin de tout, elle fonde sa fenêtre", "[ui][chatbox]") {
  ro::ui::ChatDragBoard board;
  board.strips = {StripAt(0, 0.0F, 0.0F)};
  board.from_group = 0;
  board.from_tab = 1;
  board.released = true;
  board.at = ImVec2(900.0F, 600.0F);

  const ro::ui::ChatDrop drop = ro::ui::DecideChatDrop(board);
  REQUIRE(drop.asked);
  CHECK(drop.to_new_window);
  CHECK(drop.from_tab == 1);
}

TEST_CASE("Dépôt : lâchée chez elle hors de la bande, rien ne bouge", "[ui][chatbox]") {
  // 🔴 C'EST LA GARDE CONTRE L'ARRACHEMENT ACCIDENTEL. Six pixels vers le bas
  // suffisent à déclencher un glisser ; sans cette règle, un clic un peu traînant
  // sur une languette sortirait l'onglet dans une fenêtre que personne n'a
  // demandée. Le menu de l'onglet, lui, le fait sans ambiguïté.
  ro::ui::ChatDragBoard board;
  board.strips = {StripAt(0, 0.0F, 0.0F)};
  board.from_group = 0;
  board.from_tab = 0;
  board.released = true;
  // Dans le FIL de sa propre fenêtre : sous la bande, mais dedans.
  board.at = ImVec2(100.0F, 150.0F);

  CHECK_FALSE(ro::ui::DecideChatDrop(board).asked);
}

TEST_CASE("Saisie : Entrée avec un destinataire armé", "[ui][chatbox]") {
  // 🔴 LE GESTE EXACT D'UN PLANTAGE VU EN JEU : remplir la box Â« Pseudo Â»,
  // taper une phrase, valider. L'étiquette affiche alors le pseudo ; la ligne
  // part, le champ se vide, et l'étiquette repasse à Â« Alentour Â» — donc la
  // largeur du champ change à l'image où son tampon vient d'être rechargé.
  ro::ui::UiContext ui;
  ro::ui::ChatInputState state;
  std::snprintf(state.text.data(), state.text.size(), "%s", "qsdf");
  std::snprintf(state.whisper_to.data(), state.whisper_to.size(), "%s", "test");
  state.wants_focus = true;

  ro::ui::ChatLinePreview preview;
  preview.target = "test";
  std::optional<std::string> said;

  const auto draw = [&state, &preview, &said]() {
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kNarrowWindow, kViewport.height_px));
    ImGui::Begin("essai");
    static_cast<void>(ro::ui::ChatInputHeight(state, preview, kRoomForInput, kNoArt));
    ro::ui::ChatInputAsk asked;
    said = ro::ui::DrawChatInput(state, preview, &asked, kNoArt);
    ImGui::End();
  };

  ImGuiIO& io = ImGui::GetIO();
  // Une image pour poser la rangée, puis un CLIC dans le champ : c'est la seule
  // façon de lui donner le clavier pour de bon.
  ui.BeginFrame(kViewport, kFrameSeconds);
  draw();
  ui.EndFrame();

  io.AddMousePosEvent(kInTheField, kOnTheRow);
  io.AddMouseButtonEvent(0, true);
  ui.BeginFrame(kViewport, kFrameSeconds);
  draw();
  ui.EndFrame();
  io.AddMouseButtonEvent(0, false);
  ui.BeginFrame(kViewport, kFrameSeconds);
  draw();
  ui.EndFrame();

  // Entrée : la ligne part, le champ se vide.
  io.AddKeyEvent(ImGuiKey_Enter, true);
  ui.BeginFrame(kViewport, kFrameSeconds);
  draw();
  ui.EndFrame();
  io.AddKeyEvent(ImGuiKey_Enter, false);
  CHECK(said.has_value());

  // ⚠ ET L'ÉTIQUETTE CHANGE : la ligne est vide, elle ne vise plus personne.
  preview.target = "Alentour";
  ui.BeginFrame(kViewport, kFrameSeconds);
  draw();
  ui.EndFrame();

  CHECK(state.text[0] == 0);
}

TEST_CASE("Saisie : la borne de l'historique tapé est ramenée dans ses limites", "[ui][chatbox]") {
  // 🔴 CETTE BORNE VIENT D'UN FICHIER QUE LE JOUEUR PEUT OUVRIR, et c'est ce qui
  // la distingue d'un curseur de panneau : celui-ci ne sait pas sortir de ses
  // bornes, un éditeur de texte si. Un zéro écrit à la main viderait
  // l'historique à chaque envoi, et la flèche ↑ ne rendrait plus rien — sans
  // que rien n'explique pourquoi.
  CHECK(ro::ui::TypedHistoryKept(0) == ro::ui::kSmallestTypedHistory);
  CHECK(ro::ui::TypedHistoryKept(-5) == ro::ui::kSmallestTypedHistory);
  CHECK(ro::ui::TypedHistoryKept(1000000) == ro::ui::kLargestTypedHistory);

  // ⚠ ET UNE VALEUR ORDINAIRE PASSE TELLE QUELLE : une borne qui « corrigerait »
  // ce qui est déjà bon ferait mentir le curseur du panneau.
  CHECK(ro::ui::TypedHistoryKept(ro::ui::kChatHistoryKept) == ro::ui::kChatHistoryKept);
  CHECK(ro::ui::TypedHistoryKept(ro::ui::kSmallestTypedHistory) == ro::ui::kSmallestTypedHistory);
  CHECK(ro::ui::TypedHistoryKept(ro::ui::kLargestTypedHistory) == ro::ui::kLargestTypedHistory);
}

// ── LA FENÊTRE DU JEU RÉDUITE DANS LA BARRE DES TÂCHES ──────────────────────
//
// 🔴 IMGUI REÇOIT ALORS UN ÉCRAN DE TAILLE NULLE, et ce n'est pas une
// supposition : le pont SDL3 pose lui-même `w = h = 0` quand la fenêtre porte
// `SDL_WINDOW_MINIMIZED`. Tout ce qui se calcule en part d'écran vaut donc zéro
// le temps que le joueur regarde ailleurs.
//
// ⚠ ET LE DÉGÂT SURVIT AU RETOUR, parce qu'un plafond de taille ne rogne pas
// une image : il ÉCRIT dans la fenêtre. Rendue à son plancher pendant qu'elle
// est cachée, la chatbox revient au plancher — et part ainsi dans les réglages.

TEST_CASE("Chatbox : réduite dans la barre des tâches, elle garde sa taille", "[ui][chatbox]") {
  // L'écran d'une fenêtre réduite, tel que le pont SDL3 le rend.
  constexpr ro::ui::Viewport kMinimized{.width_px = 0.0F, .height_px = 0.0F};
  // Plus grande que le plancher, et bien plus petite que la part d'écran : sa
  // taille ne doit rien à une borne, elle est celle que le joueur a tirée.
  constexpr float kChosenWidth = 700.0F;
  constexpr float kChosenHeight = 400.0F;

  ro::ui::UiContext ui;
  const ro::ui::ChatStyle style;
  const auto draw = [&style]() {
    static_cast<void>(ro::ui::BeginChatFrame("conversation_reduite", style));
    ro::ui::EndChatFrame();
  };

  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::SetNextWindowSize(ImVec2(kChosenWidth, kChosenHeight));
  draw();
  ui.EndFrame();

  const ImGuiWindow* window = ImGui::FindWindowByName("conversation_reduite");
  REQUIRE(window != nullptr);
  REQUIRE(window->SizeFull.x == kChosenWidth);
  REQUIRE(window->SizeFull.y == kChosenHeight);

  // Réduite : deux images, le temps qu'un joueur consulte autre chose.
  for (int frame = 0; frame < 2; ++frame) {
    ui.BeginFrame(kMinimized, kFrameSeconds);
    draw();
    ui.EndFrame();
  }

  // Rendue : rien n'a changé, ni pendant ni après.
  ui.BeginFrame(kViewport, kFrameSeconds);
  draw();
  ui.EndFrame();

  CHECK(window->SizeFull.x == kChosenWidth);
  CHECK(window->SizeFull.y == kChosenHeight);
}

TEST_CASE("Dépôt de la saisie : on y ajoute, jamais on ne l'écrase", "[ui][chatbox]") {
  ro::ui::ChatInputState state;
  state.pending_insert = "ligne retenue";
  REQUIRE(ro::ui::AppendPendingInsert(state, ":)"));
  CHECK(state.pending_insert == "ligne retenue:)");

  // Ce qui ne tiendrait plus avec la saisie et le dépôt est refusé, et le dépôt
  // reste tel quel : posé en bloc, il emporterait la ligne retenue avec lui.
  const std::string typed(ro::ui::kChatInputSize - state.pending_insert.size() - 2, 'a');
  std::ranges::copy(typed, state.text.begin());
  state.text[typed.size()] = '\0';
  CHECK_FALSE(ro::ui::AppendPendingInsert(state, "xy"));
  CHECK(state.pending_insert == "ligne retenue:)");
  // Le zéro terminal compte, et la dernière place se prend.
  CHECK(ro::ui::AppendPendingInsert(state, "x"));
  CHECK(state.pending_insert == "ligne retenue:)x");
}

// ── LES MARQUES : Discord sur la destination, non-lu sur une languette ──────

namespace {

// Un sommet de cette teinte a-t-il été peint, dans n'importe quelle liste ?
[[nodiscard]] bool Painted(ImU32 tint) {
  const ImDrawData* drawn = ImGui::GetDrawData();
  REQUIRE(drawn != nullptr);
  for (const ImDrawList* list : drawn->CmdLists) {
    for (const ImDrawVert& vertex : list->VtxBuffer) {
      if (vertex.col == tint) {
        return true;
      }
    }
  }
  return false;
}

// Une image de la rangée de saisie, sur ces modes, ce rang armé, et une ligne
// qui part ou non sur Discord.
[[nodiscard]] bool MarkPainted(std::span<const ro::ui::ChatSendMode> modes, int mode,
                               bool relayed) {
  ro::ui::UiContext ui;
  ro::ui::ChatInputState state;
  ro::ui::ChatLinePreview preview{.target = "Alentour"};
  preview.modes = modes;
  preview.mode = mode;
  preview.relayed = relayed;
  ui.BeginFrame(kViewport, kFrameSeconds);
  InWindow(kNarrowWindow, [&] {
    static_cast<void>(ro::ui::ChatInputHeight(state, preview, kRoomForInput, kNoArt));
    ro::ui::ChatInputAsk asked;
    static_cast<void>(ro::ui::DrawChatInput(state, preview, &asked, kNoArt));
  });
  ui.EndFrame();
  return Painted(ro::ui::kDiscordMarkColor);
}

}  // namespace

TEST_CASE("Saisie : la marque Discord suit la destination résolue, pas le mode armé",
          "[ui][chatbox]") {
  const std::array<ro::ui::ChatSendMode, 2> modes = {
      ro::ui::ChatSendMode{.label = "Alentour", .relayed = true},
      ro::ui::ChatSendMode{.label = "Groupe"}};
  CHECK(MarkPainted(modes, 0, true));
  // 🔴 « ALENTOUR » ARMÉ, MAIS UN SIGIL DE GROUPE TAPÉ : la ligne ne part pas
  // sur Discord, et la marque ne le prétend pas.
  CHECK_FALSE(MarkPainted(modes, 0, false));
  // L'inverse : un sigil de voix haute tapé, « Groupe » armé.
  CHECK(MarkPainted(modes, 1, true));
  const std::array<ro::ui::ChatSendMode, 1> quiet = {ro::ui::ChatSendMode{.label = "Alentour"}};
  CHECK_FALSE(MarkPainted(quiet, 0, false));
}

TEST_CASE("Saisie : la place de la marque est réservée d'un mode à l'autre", "[ui][chatbox]") {
  // 🔴 LA BOÎTE NE BOUGE PAS QUAND ON CHANGE DE MODE : le champ se déroberait
  // sous le curseur. La place se réserve dès qu'UN mode est relayé.
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const std::array<ro::ui::ChatSendMode, 2> relayed = {
      ro::ui::ChatSendMode{.label = "Alentour", .relayed = true},
      ro::ui::ChatSendMode{.label = "Groupe"}};
  const std::array<ro::ui::ChatSendMode, 2> quiet = {ro::ui::ChatSendMode{.label = "Alentour"},
                                                     ro::ui::ChatSendMode{.label = "Groupe"}};
  ro::ui::ChatLinePreview preview{.target = "Alentour"};
  float on_relayed = 0.0F;
  float on_party = 0.0F;
  float without = 0.0F;
  InWindow(300.0F, [&] {
    preview.modes = relayed;
    preview.mode = 0;
    on_relayed = ro::ui::ChatTargetBoxWidth(preview, kNoArt);
    preview.mode = 1;
    on_party = ro::ui::ChatTargetBoxWidth(preview, kNoArt);
    preview.modes = quiet;
    without = ro::ui::ChatTargetBoxWidth(preview, kNoArt);
  });
  ui.EndFrame();
  CHECK(on_relayed == on_party);
  CHECK(on_relayed > without);
}

TEST_CASE("Bande : un onglet qui a du neuf porte son point", "[ui][chatbox]") {
  const auto strip_painted = [](bool unread) {
    ro::ui::UiContext ui;
    ro::ui::ChatStripState state;
    ro::ui::ChatDragBoard board;
    const std::array<ro::ui::ChatStripTab, 2> tabs = {
        ro::ui::ChatStripTab{.name = "Tout"},
        ro::ui::ChatStripTab{.name = "Guilde", .unread = unread}};
    ui.BeginFrame(kViewport, kFrameSeconds);
    InWindow(kNarrowWindow, [&] {
      ro::ui::ChatStripAction asked;
      static_cast<void>(ro::ui::DrawChatStrip(state, tabs, 0, ro::ui::kChatTabColor, false, 0, {},
                                              {}, board, &asked));
      // ⚠ LA BANDE LAISSE LE CURSEUR SOUS ELLE, pour ce qui suit : sans un
      // élément après, ImGui refuse une fenêtre qu'on n'a agrandie qu'en
      // déplaçant le curseur.
      ImGui::Dummy(ImVec2(0.0F, 0.0F));
    });
    ui.EndFrame();
    return Painted(ro::ui::kChatUnreadDotColor);
  };
  CHECK(strip_painted(true));
  CHECK_FALSE(strip_painted(false));
}

TEST_CASE("Saisie : ↑ remonte l'historique même depuis une coupure de ligne", "[ui][chatbox]") {
  // 🔴 LE CURSEUR QUE ↑ POSE EN BOUT DE PREMIÈRE RANGÉE est aussi au début de la
  // seconde. ImGui le montre en haut — il n'a plus rien au-dessus — et la
  // flèche suivante doit donc rappeler la ligne d'avant.
  ro::ui::UiContext ui;
  ro::ui::ChatInputState state;
  state.wants_focus = true;
  const ro::ui::ChatLinePreview preview{.target = "Alentour"};

  const auto open = []() {
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kNarrowWindow, kViewport.height_px));
    ImGui::Begin("essai");
  };
  const auto frame = [&]() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    open();
    static_cast<void>(ro::ui::ChatInputHeight(state, preview, kRoomForInput, kNoArt));
    ro::ui::ChatInputAsk asked;
    static_cast<void>(ro::ui::DrawChatInput(state, preview, &asked, kNoArt));
    ImGui::End();
    ui.EndFrame();
  };

  // Le plus long mot qui tient seul sur une rangée : précédé d'un mot court, il
  // passe à la seconde, et sa fin y est plus à droite que celle de la première.
  std::string longest;
  ui.BeginFrame(kViewport, kFrameSeconds);
  open();
  for (std::string word = "b";; word += 'b') {
    Fill(state, word);
    static_cast<void>(ro::ui::ChatInputHeight(state, preview, kRoomForInput, kNoArt));
    if (state.rows_wanted > 1) {
      break;
    }
    longest = word;
  }
  ImGui::End();
  ui.EndFrame();
  REQUIRE_FALSE(longest.empty());
  const std::string wrapped = "aaa " + longest;
  Fill(state, "");
  state.history.lines = {"premier", wrapped};
  state.history.at = static_cast<int>(state.history.lines.size());

  ImGuiIO& io = ImGui::GetIO();
  frame();
  io.AddMousePosEvent(kInTheField, kOnTheRow);
  io.AddMouseButtonEvent(0, true);
  frame();
  io.AddMouseButtonEvent(0, false);
  frame();

  const auto press_up = [&io, &frame]() {
    io.AddKeyEvent(ImGuiKey_UpArrow, true);
    frame();
    io.AddKeyEvent(ImGuiKey_UpArrow, false);
    frame();
  };
  press_up();  // la ligne repliée, curseur en fin de seconde rangée
  REQUIRE(std::string(state.text.data()) == wrapped);
  REQUIRE(state.rows_drawn == 2);
  press_up();  // sur la première rangée, en butée sur la coupure
  CHECK(std::string(state.text.data()) == wrapped);
  press_up();  // plus rien au-dessus : la ligne d'avant
  CHECK(std::string(state.text.data()) == "premier");
}
