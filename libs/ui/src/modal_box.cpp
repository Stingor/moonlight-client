#include "ro/ui/modal_box.h"

#include <algorithm>
#include <array>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_widgets.h"

#include "ui_popup.h"
#include "ui_scale.h"

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

// Ni barre de titre, ni poignée de taille, ni barre de défilement, et rien de lu
// ni d'écrit dans le .ini : il n'y a rien à retenir, puisque `BeginBox` recentre
// la boîte à chaque ouverture et repose sa largeur à chaque image.
constexpr ImGuiWindowFlags kBoxFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoScrollbar |
                                       ImGuiWindowFlags_NoSavedSettings;

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
  const SkinImage& corner = art.Get(render::SkinPiece::kFrameTopLeft);
  const SkinImage& bottom = art.Get(render::SkinPiece::kFrameBottomLeft);
  const float wide = Px(corner.width);
  const float high = Px(corner.height);
  const float low = Px(bottom.height);

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
  ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(Px(kBoxPaddingPixels), Px(kBoxPaddingPixels)));
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

// Ce que l'ouverture d'une boîte a posé, et ce qu'elle a trouvé.
struct OpenedBox {
  BoxStyle style;
  // `BeginPopupModal` a rendu vrai : le contenu se dessine.
  bool open = false;
};

// LA COQUE DES BOÎTES : centrée, habillée, à largeur fixe, son cadre peint.
//
// ⚠ `EndBox` S'APPELLE TOUJOURS, ouverte ou non : c'est lui qui dépile
// l'habillage, posé AVANT de savoir si la boîte s'ouvrirait.
[[nodiscard]] OpenedBox BeginBox(const char* id, const SkinArt& art) {
  // ⚠ AU CENTRE, À CHAQUE FOIS. Une boîte qu'on doit chercher a manqué son but :
  // elle interrompt, donc elle tombe sous les yeux.
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));

  const float width = Px(kBoxWidthPixels);
  OpenedBox box{.style = PushBoxStyle(art)};
  ImGui::SetNextWindowSize(ImVec2(width, 0.0F), ImGuiCond_Always);

  box.open = ImGui::BeginPopupModal(id, nullptr, kBoxFlags);
  if (!box.open) {
    return box;
  }
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (box.style.painted) {
    // ⚠ LE CADRE SE PEINT SOUS LE CONTENU, donc AVANT lui : la liste de dessin
    // est chronologique, et le peindre après l'aurait recouvert. La taille est
    // celle de l'image PRÉCÉDENTE — la hauteur s'ajuste au texte —, ce qui ne
    // se voit pas : ImGui la fixe dès la seconde image d'ouverture.
    PaintFrame(window->DrawList, art, window->Pos,
               ImVec2(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y));
  }
  return box;
}

// Termine la coque ouverte par `BeginBox`, et dépile son style dans tous les
// cas. Une réponse tombée retire la boîte de la pile des surgissants ; sans
// réponse, elle reste ouverte pour l'image suivante.
void EndBox(const OpenedBox& box, bool answered) {
  if (box.open) {
    if (answered) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  PopBoxStyle(box.style);
}

// Pose le curseur pour qu'une rangée large de `row_width` tombe au milieu de la
// boîte, sans jamais repasser à gauche de la marge.
void CenterRow(float row_width) {
  ImGui::SetCursorPosX(
      std::max(ImGui::GetCursorPosX(), (ImGui::GetWindowSize().x - row_width) * 0.5F));
}

// LES DEUX BOUTONS, CENTRÉS ET DE MÊME LARGEUR — celle du plus long libellé,
// mesurée dans la langue courante. Deux boutons de largeurs différentes se
// lisent comme deux choses sans rapport, et c'est un choix qu'on présente.
[[nodiscard]] ModalAnswer DrawAnswerButtons(const char* yes, const char* no, const SkinArt& art) {
  const std::array<const char*, 2> labels = {yes, no};
  const float button = SkinWidestButton(labels, art);
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  CenterRow(button * 2.0F + spacing);
  ModalAnswer answer = ModalAnswer::kWaiting;
  if (SkinButton(yes, art, ImVec2(button, 0.0F))) {
    answer = ModalAnswer::kYes;
  }
  ImGui::SameLine();
  if (SkinButton(no, art, ImVec2(button, 0.0F))) {
    answer = ModalAnswer::kNo;
  }
  return answer;
}

}  // namespace

void AskModal(const char* id) { ImGui::OpenPopup(id); }

void CloseModal(const char* id) {
  const ImGuiContext& context = *GImGui;
  const ImGuiID popup_id = ImGui::GetID(id);
  for (int level = 0; level < context.OpenPopupStack.Size; ++level) {
    if (context.OpenPopupStack[level].PopupId == popup_id) {
      ImGui::ClosePopupToLevel(level, true);
      return;
    }
  }
}

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

void NotePopupsAtFrameStart() { g_popup_at_frame_start = AnyPopupOpen(); }

bool PopupTakesEscape() { return g_popup_at_frame_start || AnyPopupOpen(); }

ModalAnswer DrawModalBox(const char* id, const char* question, const char* yes, const char* no,
                         const SkinArt& art, ModalEnter enter) {
  const OpenedBox box = BeginBox(id, art);
  ModalAnswer answer = ModalAnswer::kWaiting;
  if (box.open) {
    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextUnformatted(question);
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0.0F, Px(kQuestionGapPixels)));

    answer = DrawAnswerButtons(yes, no, art);
    // ⚠ ÉCHAP VAUT « NON », jamais « rien » : une boîte modale qu'on ne peut pas
    // congédier au clavier retient le jeu entier.
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      answer = ModalAnswer::kNo;
    }
    if (enter == ModalEnter::kYes && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                                      ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
      answer = ModalAnswer::kYes;
    }
  }
  EndBox(box, answer != ModalAnswer::kWaiting);
  return answer;
}

bool DrawNoticeBox(const char* id, const char* text, const char* ok, const SkinArt& art) {
  const OpenedBox box = BeginBox(id, art);
  bool acknowledged = false;
  if (box.open) {
    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0.0F, Px(kQuestionGapPixels)));

    // LE BOUTON SEUL, CENTRÉ, à la largeur que prendrait chacun de deux.
    const std::array<const char*, 1> labels = {ok};
    const float button = SkinWidestButton(labels, art);
    CenterRow(button);
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
  }
  EndBox(box, acknowledged);
  return acknowledged;
}

ModalAnswer DrawRevertBox(const char* id, const char* question, const char* note, const char* yes,
                          const char* no, int seconds_left, const SkinArt& art) {
  const OpenedBox box = BeginBox(id, art);
  ModalAnswer answer = ModalAnswer::kWaiting;
  if (box.open) {
    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextUnformatted(question);
    // ⚠ CE QUE LE GESTE A DÉJÀ FAIT, entre la question et le délai. Le joueur
    // ne peut arbitrer qu'en sachant ce qu'il perd d'un côté ou de l'autre, et
    // ce que la disposition a subi ne se devine pas : elle est sous ses yeux,
    // mais rien ne lui dit qu'elle ne reviendra plus.
    if (note != nullptr && note[0] != '\0') {
      ImGui::Dummy(ImVec2(0.0F, Px(kQuestionGapPixels)));
      ImGui::TextUnformatted(note);
    }
    // ⚠ LE DÉLAI EST DIT, pas seulement compté. Un nombre qui descend sans rien
    // annoncer se lit comme un décompte avant que quelque chose n'arrive ; ici
    // c'est l'inverse — il annonce que rien n'arrivera.
    ImGui::TextDisabled(core::Tr("Retour au réglage précédent dans %d s."),
                        seconds_left < 0 ? 0 : seconds_left);
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0.0F, Px(kQuestionGapPixels)));

    answer = DrawAnswerButtons(yes, no, art);
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      answer = ModalAnswer::kNo;
    }
    // 🔴 LE TEMPS ÉCOULÉ VAUT « NON ». C'est toute la raison d'être de cette
    // boîte : celui qui ne la voit pas ne peut pas répondre, et son silence doit
    // le ramener là où il voyait quelque chose.
    if (seconds_left <= 0) {
      answer = ModalAnswer::kNo;
    }
  }
  EndBox(box, answer != ModalAnswer::kWaiting);
  return answer;
}

}  // namespace ro::ui
