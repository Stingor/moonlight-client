#include "ro/ui/modal_box.h"

#include <algorithm>
#include <array>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_widgets.h"

namespace ro::ui {
namespace {

// Les huit morceaux du cadre : les quatre coins, et les quatre bords qui
// s'étirent entre eux. Le neuvième — le centre — n'existe pas dans
// l'installation : le corps de la boîte est un aplat, comme celui d'une fenêtre.
constexpr std::array<render::SkinPiece, 8> kFramePieces = {
    render::SkinPiece::kFrameTopLeft,      render::SkinPiece::kFrameTopMiddle,
    render::SkinPiece::kFrameTopRight,     render::SkinPiece::kFrameLeft,
    render::SkinPiece::kFrameRight,        render::SkinPiece::kFrameBottomLeft,
    render::SkinPiece::kFrameBottomMiddle, render::SkinPiece::kFrameBottomRight};

// La largeur au repos d'une boîte, en pixels d'art : assez pour une question
// d'une ligne ou deux, et pas plus. Une boîte large se lit moins bien qu'une
// boîte étroite — le regard doit revenir à la ligne.
constexpr float kBoxWidthPixels = 300.0F;
// Ce qui sépare le texte du cadre, et le texte des boutons.
constexpr float kBoxPaddingPixels = 14.0F;
constexpr float kQuestionGapPixels = 10.0F;

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

[[nodiscard]] bool Paintable(const SkinArt& art) {
  return std::ranges::all_of(kFramePieces,
                             [&art](render::SkinPiece piece) { return art.Get(piece).usable(); });
}

// ⚠ À LA COULEUR DU CORPS QU'IL BORDE : voir `PaintFrameOn`. Sans elle, le
// cadre d'un habillage sombre resterait blanc autour d'un corps sombre.
void Stretch(ImDrawList* into, const SkinArt& art, const SkinImage& image, ImVec2 top_left,
             ImVec2 bottom_right) {
  if (image.usable() && bottom_right.x > top_left.x && bottom_right.y > top_left.y) {
    PaintFrameOn(into, image.id, top_left, bottom_right, art.look().body);
  }
}

// LE CADRE EN NEUF MORCEAUX, posé sur un rectangle.
//
// ⚠ LES COINS GARDENT LEUR TAILLE, les bords s'étirent entre eux. C'est toute
// la raison d'un découpage : un cadre étiré en une seule image déforme ses
// angles, et cela se voit au premier coup d'œil.
void PaintFrame(ImDrawList* into, const SkinArt& art, ImVec2 top_left, ImVec2 bottom_right) {
  const float scale = Scale();
  const SkinImage& corner = art.Get(render::SkinPiece::kFrameTopLeft);
  const SkinImage& bottom = art.Get(render::SkinPiece::kFrameBottomLeft);
  const float wide = corner.width * scale;
  const float high = corner.height * scale;
  const float low = bottom.height * scale;

  const float left = top_left.x;
  const float right = bottom_right.x;
  const float top = top_left.y;
  const float floor_y = bottom_right.y;
  // Une boîte trop petite pour ses coins : ils se partagent la place plutôt que
  // de se chevaucher à l'envers.
  const float inner_left = std::min(left + wide, right);
  const float inner_right = std::max(inner_left, right - wide);
  const float inner_top = std::min(top + high, floor_y);
  const float inner_bottom = std::max(inner_top, floor_y - low);

  // LE CORPS D'ABORD, sous le cadre : les bords ont des bavures transparentes,
  // et un aplat posé par-dessus les mangerait.
  into->AddRectFilled(ImVec2(left, top), ImVec2(right, floor_y), art.look().body);

  Stretch(into, art, art.Get(render::SkinPiece::kFrameTopLeft), ImVec2(left, top),
          ImVec2(inner_left, inner_top));
  Stretch(into, art, art.Get(render::SkinPiece::kFrameTopMiddle), ImVec2(inner_left, top),
          ImVec2(inner_right, inner_top));
  Stretch(into, art, art.Get(render::SkinPiece::kFrameTopRight), ImVec2(inner_right, top),
          ImVec2(right, inner_top));
  Stretch(into, art, art.Get(render::SkinPiece::kFrameLeft), ImVec2(left, inner_top),
          ImVec2(inner_left, inner_bottom));
  Stretch(into, art, art.Get(render::SkinPiece::kFrameRight), ImVec2(inner_right, inner_top),
          ImVec2(right, inner_bottom));
  Stretch(into, art, art.Get(render::SkinPiece::kFrameBottomLeft), ImVec2(left, inner_bottom),
          ImVec2(inner_left, floor_y));
  Stretch(into, art, art.Get(render::SkinPiece::kFrameBottomMiddle),
          ImVec2(inner_left, inner_bottom), ImVec2(inner_right, floor_y));
  Stretch(into, art, art.Get(render::SkinPiece::kFrameBottomRight),
          ImVec2(inner_right, inner_bottom), ImVec2(right, floor_y));
}

// 🔴 LE CORPS CLAIR APPELLE L'ENCRE DE LA PEAU, et c'est la boîte qui la pose.
// Elle peint elle-même son aplat : les couleurs qui s'y lisent lui appartiennent
// donc aussi. Une boîte dessinée hors de toute fenêtre habillée n'hériterait
// d'aucune encre, et écrirait blanc sur clair.
struct BoxStyle {
  bool painted = false;
  SkinStylePushed skin;
};

[[nodiscard]] BoxStyle PushBoxStyle(const SkinArt& art) {
  BoxStyle style{.painted = Paintable(art)};
  if (!style.painted) {
    return style;
  }
  style.skin = PushSkinStyle(art);
  // Le fond d'ImGui s'efface : c'est le cadre qu'on peint qui fait le corps.
  const float scale = Scale();
  ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(kBoxPaddingPixels * scale, kBoxPaddingPixels * scale));
  return style;
}

void PopBoxStyle(BoxStyle style) {
  if (!style.painted) {
    return;
  }
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(2);
  PopSkinStyle(style.skin);
}

}  // namespace

void AskModal(const char* id) { ImGui::OpenPopup(id); }

namespace {
// Y avait-il un surgissant ouvert au DÉBUT de l'image ?
//
// 🔴 LE RELEVÉ SE FAIT AU DÉBUT, ET C'EST TOUT L'OBJET. La touche Échap arrive
// AVANT que l'image ne se dessine : ce qui comptait, c'est ce que le joueur
// avait sous les yeux à ce moment-là. Lire l'état à la FIN de l'image donne
// faux, parce que le surgissant vient justement de se fermer sur cette touche —
// et la pile d'Échap, croyant la touche libre, fermait la fenêtre derrière. Une
// seule touche, deux fermetures.
//
// ⚠ UNE STATIQUE DE FICHIER, comme la peinture de la barre de titre : il n'y a
// qu'un contexte ImGui, qu'une image à la fois, et qu'une pile de surgissants.
bool g_popup_at_frame_start = false;
}  // namespace

void NotePopupsAtFrameStart() {
  g_popup_at_frame_start =
      ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
}

bool PopupTakesEscape() {
  return g_popup_at_frame_start ||
         ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
}

ModalAnswer DrawModalBox(const char* id, const char* question, const char* yes, const char* no,
                         const SkinArt& art, ModalEnter enter) {
  // ⚠ AU CENTRE, À CHAQUE FOIS. Une boîte qu'on doit chercher a manqué son but :
  // elle interrompt, donc elle tombe sous les yeux.
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));

  const float scale = Scale();
  const BoxStyle style = PushBoxStyle(art);
  ImGui::SetNextWindowSize(ImVec2(kBoxWidthPixels * scale, 0.0F), ImGuiCond_Always);

  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoSavedSettings;
  ModalAnswer answer = ModalAnswer::kWaiting;
  if (ImGui::BeginPopupModal(id, nullptr, kFlags)) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (style.painted) {
      // ⚠ LE CADRE SE PEINT SOUS LE CONTENU, donc AVANT lui : la liste de dessin
      // est chronologique, et le peindre après l'aurait recouvert. La taille est
      // celle de l'image PRÉCÉDENTE — la hauteur s'ajuste au texte —, ce qui ne
      // se voit pas : ImGui la fixe dès la seconde image d'ouverture.
      PaintFrame(window->DrawList, art, window->Pos,
                 ImVec2(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y));
    }

    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextUnformatted(question);
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0.0F, kQuestionGapPixels * scale));

    // LES DEUX BOUTONS, CENTRÉS ET DE MÊME LARGEUR — celle du plus long libellé,
    // mesurée dans la langue courante. Deux boutons de largeurs différentes se
    // lisent comme deux choses sans rapport, et c'est un choix qu'on présente.
    const std::array<const char*, 2> labels = {yes, no};
    const float button = SkinWidestButton(labels, art);
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float total = button * 2.0F + spacing;
    ImGui::SetCursorPosX(
        std::max(ImGui::GetCursorPosX(), (ImGui::GetWindowSize().x - total) * 0.5F));
    if (SkinButton(yes, art, ImVec2(button, 0.0F))) {
      answer = ModalAnswer::kYes;
    }
    ImGui::SameLine();
    if (SkinButton(no, art, ImVec2(button, 0.0F))) {
      answer = ModalAnswer::kNo;
    }
    // ⚠ ÉCHAP VAUT « NON », jamais « rien » : une boîte modale qu'on ne peut pas
    // congédier au clavier retient le jeu entier.
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      answer = ModalAnswer::kNo;
    }
    if (enter == ModalEnter::kYes && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                                      ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
      answer = ModalAnswer::kYes;
    }
    if (answer != ModalAnswer::kWaiting) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  PopBoxStyle(style);
  return answer;
}

bool DrawNoticeBox(const char* id, const char* text, const char* ok, const SkinArt& art) {
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));

  const float scale = Scale();
  const BoxStyle style = PushBoxStyle(art);
  ImGui::SetNextWindowSize(ImVec2(kBoxWidthPixels * scale, 0.0F), ImGuiCond_Always);

  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoSavedSettings;
  bool acknowledged = false;
  if (ImGui::BeginPopupModal(id, nullptr, kFlags)) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (style.painted) {
      PaintFrame(window->DrawList, art, window->Pos,
                 ImVec2(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y));
    }

    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0.0F, kQuestionGapPixels * scale));

    // LE BOUTON SEUL, CENTRÉ, à la largeur que prendrait chacun de deux.
    const std::array<const char*, 1> labels = {ok};
    const float button = SkinWidestButton(labels, art);
    ImGui::SetCursorPosX(
        std::max(ImGui::GetCursorPosX(), (ImGui::GetWindowSize().x - button) * 0.5F));
    if (SkinButton(ok, art, ImVec2(button, 0.0F))) {
      acknowledged = true;
    }
    // 🔴 PAS À L'IMAGE OÙ ELLE PARAÎT, ET JAMAIS SUR UNE RÉPÉTITION : une
    // touche frappée ou tenue AVANT qu'elle ne s'ouvre — Échap martelé devant
    // un jeu figé — la refermerait sans qu'elle ait jamais été vue.
    const bool keys_count = !ImGui::IsWindowAppearing();
    if (keys_count && (ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                       ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                       ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
      acknowledged = true;
    }
    if (acknowledged) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  PopBoxStyle(style);
  return acknowledged;
}

ModalAnswer DrawRevertBox(const char* id, const char* question, const char* note, const char* yes,
                          const char* no, int seconds_left, const SkinArt& art) {
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));

  const float scale = Scale();
  const BoxStyle style = PushBoxStyle(art);
  ImGui::SetNextWindowSize(ImVec2(kBoxWidthPixels * scale, 0.0F), ImGuiCond_Always);

  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoSavedSettings;
  ModalAnswer answer = ModalAnswer::kWaiting;
  if (ImGui::BeginPopupModal(id, nullptr, kFlags)) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (style.painted) {
      PaintFrame(window->DrawList, art, window->Pos,
                 ImVec2(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y));
    }

    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextUnformatted(question);
    // ⚠ CE QUE LE GESTE A DÉJÀ FAIT, entre la question et le délai. Le joueur
    // ne peut arbitrer qu'en sachant ce qu'il perd d'un côté ou de l'autre, et
    // ce que la disposition a subi ne se devine pas : elle est sous ses yeux,
    // mais rien ne lui dit qu'elle ne reviendra plus.
    if (note != nullptr && note[0] != '\0') {
      ImGui::Dummy(ImVec2(0.0F, kQuestionGapPixels * scale));
      ImGui::TextUnformatted(note);
    }
    // ⚠ LE DÉLAI EST DIT, pas seulement compté. Un nombre qui descend sans rien
    // annoncer se lit comme un décompte avant que quelque chose n'arrive ; ici
    // c'est l'inverse — il annonce que rien n'arrivera.
    ImGui::TextDisabled(core::Tr("Retour au réglage précédent dans %d s."),
                        seconds_left < 0 ? 0 : seconds_left);
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0.0F, kQuestionGapPixels * scale));

    const std::array<const char*, 2> labels = {yes, no};
    const float button = SkinWidestButton(labels, art);
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float total = button * 2.0F + spacing;
    ImGui::SetCursorPosX(
        std::max(ImGui::GetCursorPosX(), (ImGui::GetWindowSize().x - total) * 0.5F));
    if (SkinButton(yes, art, ImVec2(button, 0.0F))) {
      answer = ModalAnswer::kYes;
    }
    ImGui::SameLine();
    if (SkinButton(no, art, ImVec2(button, 0.0F))) {
      answer = ModalAnswer::kNo;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      answer = ModalAnswer::kNo;
    }
    // 🔴 LE TEMPS ÉCOULÉ VAUT « NON ». C'est toute la raison d'être de cette
    // boîte : celui qui ne la voit pas ne peut pas répondre, et son silence doit
    // le ramener là où il voyait quelque chose.
    if (seconds_left <= 0) {
      answer = ModalAnswer::kNo;
    }
    if (answer != ModalAnswer::kWaiting) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  PopBoxStyle(style);
  return answer;
}

}  // namespace ro::ui
