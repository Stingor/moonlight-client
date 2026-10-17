#include "ro/ui/skinned_window.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/window_bounds.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// Ce qu'une fenêtre habillée garde d'un `Begin` à son `End`.
//
// ⚠ UNE SEULE À LA FOIS, ET C'EST TOUT CE QU'IL FAUT : les fenêtres ne
// s'imbriquent pas. Une pile serait du décor.
struct Painting {
  bool skinned = false;
  // Ce que la palette a posé sur le style, et qu'il faudra retirer.
  SkinStylePushed style;
  // ⚠ L'HABILLAGE, POUR LA FERMETURE. `EndSkinnedWindow` ne le reçoit pas, et
  // l'ascenseur ne se peint qu'à la fin — la taille du contenu n'est connue
  // qu'alors. Le pointeur ne vit que le temps de l'image, entre les deux appels.
  const SkinArt* art = nullptr;
  // La marge de cadre posée pour la barre de titre, qui vaut pour tout le corps.
  // Voir le site qui la pousse.
  bool tight = false;
  // ⚠ SON CADRE EST PLUS ÉPAIS, et la poignée doit rentrer d'autant : c'est une
  // boîte `sysbox`, pas la bordure mince d'une fenêtre ordinaire.
  bool description = false;
  // L'épaisseur du bas de ce cadre, où le corps ne descend pas. Nulle ailleurs.
  float frame_bottom = 0.0F;
};
Painting g_painting;

// Peint une bande en trois morceaux : deux bouts à leur taille, un milieu étiré.
// LE CADRE D'UNE DESCRIPTION : les deux côtés et le bas, en pièces `sysbox`.
//
// ⚠ NI LE HAUT NI LES COINS HAUTS : la barre de titre les couvre. Les peindre y
// poserait un second filet, visible sur le clair de la barre.
//
// ⚠ L'ÉPAISSEUR VIENT DU CÔTÉ, jamais d'une constante : c'est l'art qui la donne,
// et un habillage de joueur peut la changer.
void PaintDescriptionFrame(ImDrawList* into, const SkinArt& art, ImVec2 top_left,
                           ImVec2 bottom_right) {
  const SkinImage& side = art.Get(render::SkinPiece::kFrameLeft);
  if (!side.usable()) {
    return;
  }
  const float edge = side.width * Scale();
  const auto blit = [into, &art](const SkinImage& piece, ImVec2 from, ImVec2 to) {
    if (piece.usable()) {
      PaintFrameOn(into, piece.id, from, to, art.look().paper);
    }
  };
  const float bottom = bottom_right.y - edge;
  blit(side, ImVec2(top_left.x, top_left.y), ImVec2(top_left.x + edge, bottom));
  blit(art.Get(render::SkinPiece::kFrameRight), ImVec2(bottom_right.x - edge, top_left.y),
       ImVec2(bottom_right.x, bottom));
  blit(art.Get(render::SkinPiece::kFrameBottomLeft), ImVec2(top_left.x, bottom),
       ImVec2(top_left.x + edge, bottom_right.y));
  blit(art.Get(render::SkinPiece::kFrameBottomRight), ImVec2(bottom_right.x - edge, bottom),
       ImVec2(bottom_right.x, bottom_right.y));
  blit(art.Get(render::SkinPiece::kFrameBottomMiddle), ImVec2(top_left.x + edge, bottom),
       ImVec2(bottom_right.x - edge, bottom_right.y));
}

void PaintStrip(ImDrawList* into, const SkinArt& art, render::SkinPiece left,
                render::SkinPiece middle, render::SkinPiece right, ImVec2 top_left,
                ImVec2 bottom_right) {
  const StripRects parts = HorizontalStrip(top_left, bottom_right, art.Get(left).width * Scale(),
                                           art.Get(right).width * Scale());
  const std::array<std::pair<render::SkinPiece, const ImVec4*>, 3> pieces = {
      std::pair{left, &parts.left}, std::pair{middle, &parts.middle},
      std::pair{right, &parts.right}};
  for (const auto& [piece, where] : pieces) {
    if (where->z <= where->x) {
      continue;
    }
    PaintArt(into, art, art.Get(piece).id, ImVec2(where->x, where->y), ImVec2(where->z, where->w));
  }
}

}  // namespace

std::string_view ShownTitle(std::string_view name) {
  const std::size_t marker = name.find("##");
  return marker == std::string_view::npos ? name : name.substr(0, marker);
}

namespace {
// UN BOUTON DE LA BARRE DE TITRE, TESTÉ À LA MAIN.
//
// ⚠ PAS UN `InvisibleButton`, ET C'EST LA RAISON DE TOUT CE QUI SUIT. Une
// fenêtre REPLIÉE a `SkipItems` : aucun élément ne s'y ajoute, donc aucun
// bouton n'y répondrait — et l'on ne pourrait plus la déplier. La barre de
// titre doit rester vivante quand le reste ne l'est plus, ce qui oblige à lire
// la souris directement.
//
// ⚠ ET LE CLIC PREND L'IDENTIFIANT ACTIF. Sans cela, ImGui — qui a vu un clic
// sur sa barre de titre — se met à DÉPLACER la fenêtre pendant qu'on appuie sur
// un de ses boutons.
[[nodiscard]] bool SystemButton(ImDrawList* into, const SkinArt& art, render::SkinPiece off,
                                render::SkinPiece on, ImVec2 top_left, ImGuiID id) {
  const SkinImage& resting = art.Get(off);
  if (!resting.usable()) {
    return false;
  }
  const float scale = Scale();
  const ImVec2 bottom_right(top_left.x + resting.width * scale,
                            top_left.y + resting.height * scale);
  const bool over =
      ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(top_left, bottom_right, false);
  const SkinImage& shown = over && art.Get(on).usable() ? art.Get(on) : resting;
  PaintArt(into, art, shown.id, top_left, bottom_right);

  bool clicked = false;
  if (over) {
    // LA MAIN QUI POINTE : c'est ce que le client montre au-dessus de tout ce
    // qui se clique.
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      clicked = true;
      ImGui::SetActiveID(id, ImGui::GetCurrentWindow());
    }
  }
  if (ImGui::GetCurrentContext()->ActiveId == id && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    ImGui::ClearActiveID();
  }
  return clicked;
}

// UN BOUTON SYSTÈME À DEUX ÉTATS : son image dit ce qu'il EST, pas ce que la
// souris fait. Même lecture manuelle de la souris que `SystemButton`, et pour la
// même raison — une fenêtre repliée n'accepte aucun élément.
[[nodiscard]] bool SystemToggle(ImDrawList* into, const SkinArt& art, render::SkinPiece off,
                                render::SkinPiece on, bool state, ImVec2 top_left, ImGuiID id,
                                bool* hovered) {
  const SkinImage& resting = art.Get(off);
  const SkinImage& driven = art.Get(on);
  const SkinImage& shown = state && driven.usable() ? driven : resting;
  if (!shown.usable()) {
    return false;
  }
  const float scale = Scale();
  const ImVec2 bottom_right(top_left.x + shown.width * scale, top_left.y + shown.height * scale);
  const bool over =
      ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(top_left, bottom_right, false);
  PaintArt(into, art, shown.id, top_left, bottom_right);
  if (hovered != nullptr) {
    *hovered = over;
  }

  bool clicked = false;
  if (over) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      clicked = true;
      ImGui::SetActiveID(id, ImGui::GetCurrentWindow());
    }
  }
  if (ImGui::GetCurrentContext()->ActiveId == id && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    ImGui::ClearActiveID();
  }
  return clicked;
}

// Ce que la prochaine fenêtre fera de sa puce, et ce que la courante en a
// obtenu. ⚠ UNE SEULE À LA FOIS, comme la peinture : les fenêtres ne
// s'imbriquent pas.
const char* g_next_bullet_tooltip = nullptr;
bool g_bullet_clicked = false;

// Les boutons demandés pour la prochaine barre de titre, et ce qu'ils ont rendu.
struct TitleAction {
  const char* label = nullptr;
  const char* tooltip = nullptr;
  bool enabled = true;
};
std::array<TitleAction, kTitleActionCount> g_next_title_actions;
int g_next_title_action_count = 0;
std::array<bool, kTitleActionCount> g_title_action_clicked{};

// L'épingle demandée pour la prochaine barre de titre. ⚠ C'EST L'ADRESSE DE
// L'APPELANT : la fenêtre la retourne, elle ne la garde pas.
bool* g_next_pinned = nullptr;

// L'ombre demandée sous le titre. Zéro = aucune.
ImU32 g_next_title_shadow = 0;

// L'arrondi demandé pour la prochaine fenêtre, avant l'échelle. Zéro = celui de
// la palette.
float g_next_rounding = 0.0F;

// Le filet d'une description : gris clair, comme dans le client ; son corps
// est le papier de l'habillage.
constexpr ImU32 kDescriptionBorderColor = IM_COL32(194, 194, 194, 255);
// Ce qui sépare le titre d'une description de son bord gauche, et sa croix du
// bord droit, en pixels d'art.
constexpr float kDescriptionTitleInsetPixels = 8.0F;
constexpr float kDescriptionCloseInsetPixels = 5.0F;
// De combien le titre d'une description se relève. ⚠ UN PIXEL, pas un et demi :
// sa barre est claire et basse, et l'œil n'y cale pas le texte comme sur le
// dégradé sombre d'une fenêtre ordinaire.
constexpr float kDescriptionTitleRaisePixels = 1.0F;

// CE QU'ON LAISSE AU TITRE quand le bouton demande de la place. En deçà, le
// bouton ne se dessine pas : la puce occupe déjà le début de la barre, et un
// titre entièrement recouvert vaut moins qu'un raccourci absent.
constexpr float kTitleKeptPixels = 24.0F;

// De combien le titre se relève, en pixels d'art. Voir le site qui l'emploie.
constexpr float kTitleRaisePixels = 1.5F;
// Ce qui sépare le bouton du premier bouton système.
constexpr float kTitleActionGapPixels = 5.0F;

}  // namespace

void SetNextWindowBulletClickable(const char* tooltip) { g_next_bullet_tooltip = tooltip; }

bool TitleBulletClicked() { return g_bullet_clicked; }

void SetNextWindowTitleAction(const char* label, const char* tooltip, bool enabled) {
  // ⚠ LE TROP-PLEIN EST JETÉ EN SILENCE : une barre de titre n'a pas la place
  // d'un troisième raccourci, et refuser bruyamment ferait tomber un client que
  // rien n'empêche de tourner.
  if (g_next_title_action_count >= kTitleActionCount) {
    return;
  }
  g_next_title_actions[static_cast<std::size_t>(g_next_title_action_count)] =
      TitleAction{.label = label, .tooltip = tooltip, .enabled = enabled};
  g_next_title_action_count += 1;
}

bool TitleActionClicked(int rank) {
  return rank >= 0 && rank < kTitleActionCount &&
         g_title_action_clicked[static_cast<std::size_t>(rank)];
}

void SetNextWindowPinnable(bool* pinned) { g_next_pinned = pinned; }

void SetNextWindowTitleShadow(ImU32 color) { g_next_title_shadow = color; }

void SetNextWindowRounding(float pixels) { g_next_rounding = pixels; }

bool BeginSkinnedWindow(const char* name, bool* open, const SkinArt& art, SkinnedTitle title,
                        ImGuiWindowFlags flags) {
  g_painting = Painting{};
  // ⚠ CONSOMMÉE MÊME SANS ART, et même si la fenêtre n'ouvre pas : une demande
  // qui survivrait ferait cliquer la puce de la fenêtre SUIVANTE.
  const char* const bullet_tooltip = g_next_bullet_tooltip;
  g_next_bullet_tooltip = nullptr;
  g_bullet_clicked = false;
  const std::array<TitleAction, kTitleActionCount> title_actions = g_next_title_actions;
  const int title_action_count = g_next_title_action_count;
  g_next_title_actions = {};
  g_next_title_action_count = 0;
  g_title_action_clicked = {};
  bool* const pinned = g_next_pinned;
  g_next_pinned = nullptr;
  const ImU32 title_shadow = g_next_title_shadow;
  g_next_title_shadow = 0;
  const float rounding = g_next_rounding;
  g_next_rounding = 0.0F;
  // ⚠ AVANT LE `Begin`, ET AVANT LA BIFURCATION : habillée ou nue, la fenêtre se
  // redimensionne, et c'est ici seulement que la borne entre dans le glisser.
  ConstrainNextWindowToScreen(name);
  g_painting.skinned = art.dresses_windows();

  if (!g_painting.skinned) {
    // Installation sans art : la fenêtre d'ImGui, telle quelle. Elle reste
    // jouable, et c'est tout ce qu'on lui demande.
    return ImGui::Begin(name, open, flags);
  }

  // ⚠ LA PALETTE, ET PAS SEULEMENT LES PIÈCES. C'est elle qui fait le corps
  // clair, le texte sombre, les onglets et les champs du jeu ; sans elle, l'art
  // de la barre de titre se pose au-dessus d'une fenêtre d'ImGui. C'est elle
  // aussi qui rend la barre native TRANSPARENTE, pour qu'on peigne dessus.
  g_painting.style = PushSkinStyle(art);
  g_painting.art = &art;

  const float scale = Scale();
  // ⚠ APRÈS LA PALETTE, qui pose le sien, et AVANT le `Begin`, qui le lit. Il se
  // retire avec elle : c'est une mesure de plus sur la même pile.
  if (rounding > 0.0F) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, rounding * scale);
    g_painting.style.vars += 1;
  }
  const bool description = title == SkinnedTitle::kDescription;
  g_painting.description = description;
  if (description) {
    // 🔴 LE CORPS D'UNE DESCRIPTION EST LE PAPIER, pas celui des fenêtres : les
    // couleurs du client — le bleu des étiquettes, le rouge des interdits — y
    // sont calculées pour un fond clair, et celles qui ne se liraient plus sur
    // le papier réglé s'éclaircissent. Voir `PushPaperStyle`.
    PushPaperStyle(art.look());
    ImGui::PushStyleColor(ImGuiCol_Border, kDescriptionBorderColor);
    g_painting.style.colors += kPaperStyleColors + 1;
  }
  const bool with_bar = title != SkinnedTitle::kNone;
  if (!with_bar) {
    // ⚠ SANS BARRE, LE DÉPLACEMENT REVIENT À IMGUI, et c'est voulu : une boîte
    // de dialogue se traîne en l'attrapant là où il n'y a rien à cliquer, comme
    // dans le client. Il n'y a rien à peindre non plus — le corps clair et son
    // filet d'un pixel sont ceux de la palette.
    return ImGui::Begin(name, open, flags | ImGuiWindowFlags_NoTitleBar);
  }

  // ── LA BARRE EST CELLE D'IMGUI, ET C'EST TOUT CE QUI CHANGE ────────────────
  // La peindre DANS la zone de contenu, la barre native retirée, coûterait trois
  // choses qui ne se rattrapent pas : la fenêtre ne pourrait plus se REPLIER
  // (ImGui replie vers une barre qui n'existerait pas), son ascenseur se
  // placerait à partir du haut — donc par-dessus la barre peinte et son bouton
  // de fermeture —, et le début du contenu se reposerait à la main. En gardant
  // la barre native, ImGui connaît sa hauteur : tout le reste retombe juste
  // tout seul.
  //
  // ⚠ SA HAUTEUR SE RÈGLE PAR `FramePadding.y`, la seule prise qu'ImGui donne :
  // elle vaut `FontSize + FramePadding.y * 2`. Sans cela, l'art serait étiré sur
  // une barre plus haute que celle du client, et son dégradé déformé.
  // ⚠ CHAQUE BARRE A SA HAUTEUR, et celle d'une description est plus basse : son
  // art est `skill_upbar`, que le catalogue range sous `kModalBar`.
  const render::SkinPiece bar_left =
      description ? render::SkinPiece::kModalBarLeft : render::SkinPiece::kTitleBarLeft;
  const render::SkinPiece bar_middle =
      description ? render::SkinPiece::kModalBarMiddle : render::SkinPiece::kTitleBarMiddle;
  const render::SkinPiece bar_right =
      description ? render::SkinPiece::kModalBarRight : render::SkinPiece::kTitleBarRight;
  const float bar_height = art.Get(bar_left).height * scale;
  const float padding_y = std::max(0.0F, (bar_height - ImGui::GetFontSize()) * 0.5F);
  // 🔴 ET ELLE VAUT POUR TOUT LE CORPS, pas seulement pour la barre. C'est le
  // dessin du client, et il tient à ce report : dépilée juste après `Begin`, la
  // marge redevient celle d'ImGui — trois pixels au lieu d'un —, et CHAQUE cadre
  // de la fenêtre grandit de quatre. Un champ de filtre passe de dix-sept à
  // vingt et un, et la fenêtre entière paraît lâche à côté de celle du jeu.
  //
  // ⚠ LA VALEUR VIENT DONC DE L'ART DE LA BARRE DE TITRE, ce qui est une drôle
  // de source pour la marge d'un champ de saisie. C'est assumé : le client la
  // prend là, et c'est cette dimension-là qu'un joueur reconnaît.
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      ImVec2(ImGui::GetStyle().FramePadding.x, padding_y));
  g_painting.tight = true;
  // ⚠ LE BOUTON DE REPLI D'IMGUI S'EFFACE : il se pose exactement là où va notre
  // puce, et capterait son clic. On le retire par sa POSITION et non par
  // `NoCollapse`, qui forcerait `Collapsed` à faux à chaque image — le bouton
  // de minimisation qu'on peint ne servirait alors plus à rien.
  const ImGuiDir menu_button = ImGui::GetStyle().WindowMenuButtonPosition;
  ImGui::GetStyle().WindowMenuButtonPosition = ImGuiDir_None;
  // ⚠ `nullptr` ET NON `open` : on ne veut pas de la croix d'ImGui, qui se
  // peindrait sous la nôtre et resterait cliquable là où l'on ne voit rien.
  const bool visible = ImGui::Begin(name, nullptr, flags);
  ImGui::GetStyle().WindowMenuButtonPosition = menu_button;

  ImGuiWindow* window = ImGui::GetCurrentWindow();
  ImDrawList* into = window->DrawList;
  // Repliée, la fenêtre N'EST PLUS QUE sa barre : c'est son rectangle entier.
  const ImRect bar = window->Collapsed ? window->Rect() : window->TitleBarRect();
  // ⚠ LA BARRE DÉBORDE DE LA ZONE DE CONTENU, où `Begin` a réduit le rectangle
  // de découpe. Sans cet élargissement, elle serait rognée.
  into->PushClipRect(window->Pos,
                     ImVec2(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y), false);
  PaintStrip(into, art, bar_left, bar_middle, bar_right, bar.Min, bar.Max);
  // ── LE CADRE D'UNE DESCRIPTION : LES CÔTÉS ET LE BAS ──────────────────────
  //
  // ⚠ PAS LE HAUT : la barre de titre le couvre déjà, et un cadre complet y
  // poserait un second filet par-dessus elle.
  if (description && !window->Collapsed) {
    PaintDescriptionFrame(into, art, ImVec2(window->Pos.x, bar.Max.y),
                          ImVec2(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y));
    // ⚠ SEULEMENT POUR UNE FENÊTRE QUI DÉFILE : c'est son texte qui glissait
    // sous le cadre. Le dialogue d'un PNJ porte le même cadre et pose ses
    // boutons au ras du bas, sans défiler — la découpe les rognait.
    //
    // 🔴 LE DRAPEAU D'ABORD, LE DÉBORDEMENT ENSUITE : une fenêtre qui refuse son
    // ascenseur déborde quand même une image pendant qu'on la rétrécit. La
    // découpe allongerait alors son contenu de l'épaisseur du cadre, et ce
    // débordement s'entretiendrait seul : les boutons resteraient rognés.
    if ((window->Flags & ImGuiWindowFlags_NoScrollbar) == 0 && window->ScrollMax.y > 0.0F) {
      g_painting.frame_bottom = DescPanelEdge(art);
    }
  }

  // LA PUCE, puis le titre à sa droite : c'est la disposition du client.
  // L'infobulle de la puce, si la souris est dessus. Voir plus bas.
  const char* hovered_bullet = nullptr;
  const SkinImage& bullet = art.Get(render::SkinPiece::kBulletOff);
  // ⛔ PAS DE PUCE SUR UNE DESCRIPTION, et son titre part donc de plus près du
  // bord. Le client n'en met pas : le nom d'un objet décoré est long, et les
  // trois boutons d'une fenêtre ordinaire lui prendraient sa place.
  float text_x =
      bar.Min.x + (description ? kDescriptionTitleInsetPixels : kBulletMarginPixels) * scale;
  if (bullet.usable() && !description) {
    const float side = bullet.width * scale;
    const ImVec2 at(text_x, bar.Min.y + (bar.GetHeight() - side) * 0.5F);
    // ⚠ DÉCORATIVE TANT QU'ON NE L'A PAS RÉCLAMÉE. Voir
    // `SetNextWindowBulletClickable` : une puce cliquable sur toute fenêtre
    // promettrait à chacune une action qu'elle n'a pas.
    if (bullet_tooltip != nullptr) {
      g_bullet_clicked = SystemButton(into, art, render::SkinPiece::kBulletOff,
                                      render::SkinPiece::kBulletOn, at, window->GetID("##puce"));
      // ⚠ RETENUE, PAS POSÉE ICI. `SetTooltip` ouvre une AUTRE fenêtre, et nous
      // sommes sous le découpage de la barre de titre : elle y serait rognée.
      // Elle sort donc après le `PopClipRect`, plus bas.
      if (ImGui::IsMouseHoveringRect(at, ImVec2(at.x + side, at.y + side), false) &&
          ImGui::IsWindowHovered()) {
        hovered_bullet = bullet_tooltip;
      }
    } else {
      PaintArt(into, art, bullet.id, at, ImVec2(at.x + side, at.y + side));
    }
    text_x += side + kBulletTextGapPixels * scale;
  }
  const std::string_view shown = ShownTitle(name);
  // ⚠ REMONTÉ D'UN POIL. Centré à la lettre, le titre PARAÎT trop bas : la barre
  // porte un dégradé dont le haut est clair, et l'œil cale le texte sur la
  // partie sombre.
  //
  // ⚠ D'UN PIXEL SEULEMENT SUR UNE DESCRIPTION : sa barre est claire et basse,
  // et l'œil n'y cale pas le texte de la même façon.
  const float raise = description ? kDescriptionTitleRaisePixels : kTitleRaisePixels;
  const ImVec2 title_at(
      text_x, bar.Min.y + (bar.GetHeight() - ImGui::GetTextLineHeight()) * 0.5F - raise * scale);
  // 🔴 L'OMBRE SOUS LE TITRE, ET AVANT LUI : c'est ainsi que le client dit qu'un
  // équipement est CASSÉ. Posée après, elle recouvrirait le nom qu'elle souligne.
  if (title_shadow != 0) {
    into->AddText(ImVec2(title_at.x + scale, title_at.y + scale), title_shadow, shown.data(),
                  shown.data() + shown.size());
  }
  into->AddText(title_at, art.look().title_text, shown.data(), shown.data() + shown.size());

  // LES BOUTONS SYSTÈME, POSÉS DEPUIS LE BORD DROIT : la croix d'abord, puis
  // l'épingle à sa gauche, puis le minimiser. À l'écran on lit donc
  // [minimiser][épingle][croix], et c'est l'ordre du client.
  //
  // ⚠ L'INFOBULLE DE L'ÉPINGLE EST RETENUE, comme celle de la puce : elle sort
  // après le `PopClipRect`, une infobulle étant une fenêtre à elle.
  const char* hovered_pin = nullptr;
  float right =
      bar.Max.x - (description ? kDescriptionCloseInsetPixels : kSystemEdgePixels) * scale;
  if (open != nullptr) {
    const SkinImage& close = art.Get(render::SkinPiece::kCloseOff);
    if (close.usable()) {
      const float side = close.width * scale;
      const ImVec2 at(right - side, bar.Min.y + (bar.GetHeight() - side) * 0.5F);
      if (SystemButton(into, art, render::SkinPiece::kCloseOff, render::SkinPiece::kCloseOn, at,
                       window->GetID("##fermer"))) {
        *open = false;
      }
      right = at.x - kSystemButtonGapPixels * scale;
    }
  }
  if (pinned != nullptr) {
    const SkinImage& pin = art.Get(render::SkinPiece::kPinLoose);
    if (pin.usable()) {
      const float side = pin.width * scale;
      const ImVec2 at(right - side, bar.Min.y + (bar.GetHeight() - side) * 0.5F);
      bool over = false;
      if (SystemToggle(into, art, render::SkinPiece::kPinLoose, render::SkinPiece::kPinDriven,
                       *pinned, at, window->GetID("##epingle"), &over)) {
        *pinned = !*pinned;
      }
      if (over) {
        hovered_pin = *pinned ? core::Tr("Détacher : Échap referme la fenêtre")
                              : core::Tr("Épingler : Échap ne ferme plus");
      }
      right = at.x - kSystemButtonGapPixels * scale;
    }
  }
  // ⚠ PAS DE MINIMISER SUR UNE FENÊTRE QUI REFUSE DE SE REPLIER : le bouton
  // serait là, cliquable, et ne ferait rien.
  if ((flags & ImGuiWindowFlags_NoCollapse) == 0) {
    const SkinImage& mini = art.Get(render::SkinPiece::kCollapseOff);
    if (mini.usable()) {
      const float side = mini.width * scale;
      const ImVec2 at(right - side, bar.Min.y + (bar.GetHeight() - side) * 0.5F);
      if (SystemButton(into, art, render::SkinPiece::kCollapseOff, render::SkinPiece::kCollapseOn,
                       at, window->GetID("##minimiser"))) {
        ImGui::SetWindowCollapsed(window, !window->Collapsed);
      }
      // 🔴 LE CURSEUR AVANCE ICI AUSSI. Sans cette ligne, le raccourci de barre
      // de titre se posait par-dessus le minimiser et le mangeait : il partait
      // du bord gauche de la CROIX, comme s'il n'y avait pas de minimiser.
      right = at.x - kSystemButtonGapPixels * scale;
    }
  }
  // ── LE RACCOURCI VERS UNE AUTRE FENÊTRE, à gauche des boutons système ─────
  //
  // 🔴 UN VRAI ÉLÉMENT IMGUI, et non un rectangle lu à la souris comme les
  // boutons système. Il porte un LIBELLÉ, donc l'art du petit bouton, donc la
  // largeur que ce libellé traduit demande — et cela, seul le widget habillé
  // sait le faire.
  //
  // 🔴 ET IL RESTE SUR UNE FENÊTRE REPLIÉE, où la barre est tout ce qu'on voit :
  // ImGui y lève `SkipItems`, qui écarte tout élément, et le raccourci
  // disparaîtrait avec le corps. Le drapeau se baisse le temps de la barre,
  // puis se rend ; une fenêtre CACHÉE, elle, garde le sien.
  //
  // ⚠ LE CURSEUR SE REND. Poser un élément par-dessus la barre déplace le
  // curseur de disposition ; le laisser là ferait commencer le CORPS de la
  // fenêtre dans sa barre de titre.
  const bool skipped_items = window->SkipItems;
  const bool bar_shown = !skipped_items || (window->Collapsed && window->Active);
  window->SkipItems = !bar_shown;
  for (int rank = 0; rank < title_action_count && bar_shown; ++rank) {
    const TitleAction& asked = title_actions[static_cast<std::size_t>(rank)];
    if (asked.label == nullptr) {
      continue;
    }
    // 🔴 ARRONDI AU PIXEL, POUR LA MÊME RAISON QUE L'ORDONNÉE : l'art du jeu est
    // échantillonné au plus proche, et une abscisse fractionnaire fait baver ses
    // bordures d'un pixel. Or `CalcTextSize` rend presque toujours des
    // fractions, et elles se propagent jusqu'ici.
    //
    // ⚠ LA LARGEUR AU PLAFOND, la position au plancher : rogner la largeur sous
    // celle du texte ferait rapetisser le libellé pour rien.
    const float action_width = std::ceil(SkinSmallButtonWidth(asked.label, art));
    const float action_x = std::floor(right - kTitleActionGapPixels * scale - action_width);
    if (action_x <= bar.Min.x + kTitleKeptPixels * scale) {
      break;
    }
    const ImVec2 cursor = window->DC.CursorPos;
    const ImVec2 cursor_max = window->DC.CursorMaxPos;
    // ⚠ `ImGui::PushClipRect` ET NON CELUI DE LA LISTE : c'est lui qui élargit
    // AUSSI le rectangle de découpe de la FENÊTRE, sans quoi l'élément posé
    // au-dessus de la zone de contenu serait écarté avant d'être vu.
    ImGui::PushClipRect(
        window->Pos, ImVec2(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y), false);
    // 🔴 C'EST SON ART QUI SE CENTRE DANS LA BARRE, pas son item : la chute
    // qu'un petit bouton réserve au-dessus de son art est du vide, et centrer
    // l'item la compterait comme du dessin — l'art descendrait d'autant et
    // sortirait de la barre par le bas.
    ImGui::SetCursorScreenPos(
        ImVec2(action_x, SkinSmallButtonTopCenteredOn(art, bar.Min.y + bar.GetHeight() * 0.5F)));
    ImGui::BeginDisabled(!asked.enabled);
    // ⚠ HAUTEUR AUTOMATIQUE : la barre n'impose rien, c'est l'art qui donne sa
    // taille, et le centrage ci-dessus l'a déjà prise en compte.
    g_title_action_clicked[static_cast<std::size_t>(rank)] =
        SkinSmallButton(asked.label, art, ImVec2(action_width, 0.0F));
    ImGui::EndDisabled();
    // ⚠ `AllowWhenDisabled` : sans ce drapeau, un élément grisé n'est jamais
    // survolé, et l'infobulle qui EXPLIQUE le grisé ne s'afficherait jamais.
    if (asked.tooltip != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      SkinTooltipFormat("%s", asked.tooltip);
    }
    ImGui::PopClipRect();
    window->DC.CursorPos = cursor;
    window->DC.CursorMaxPos = cursor_max;
    // Le suivant se pose à sa GAUCHE.
    right = action_x;
  }
  window->SkipItems = skipped_items;

  into->PopClipRect();
  // 🔴 LE CORPS D'UNE DESCRIPTION S'ARRÊTE AU-DESSUS DU BAS DE SON CADRE. ImGui
  // découpe jusqu'au filet d'un pixel d'une fenêtre ordinaire : la ligne qui
  // défile passerait sur le cadre, plus épais. La découpe de la fenêtre remonte
  // donc d'autant — après celle de la barre, qui se pose par-dessus —, et la
  // fermeture allonge le contenu du même écart : la dernière ligne reste
  // atteignable.
  if (g_painting.frame_bottom > 0.0F) {
    ImRect body = window->InnerClipRect;
    body.Max.y = std::max(
        body.Min.y, std::min(body.Max.y, window->Pos.y + window->Size.y - g_painting.frame_bottom));
    window->InnerClipRect = body;
    ImGui::PopClipRect();
    ImGui::PushClipRect(body.Min, body.Max, false);
  }
  // 🔴 APRÈS LE DÉCOUPAGE, jamais dedans : une infobulle est une fenêtre à elle,
  // et celle de la barre de titre l'aurait rognée à quelques pixels de haut.
  if (hovered_bullet != nullptr) {
    SkinTooltipFormat("%s", hovered_bullet);
  } else if (hovered_pin != nullptr) {
    SkinTooltipFormat("%s", hovered_pin);
  }

  return visible;
}

void EndSkinnedWindow() {
  // L'ASCENSEUR EN DERNIER, et il le faut : la taille de ce que la fenêtre
  // contient n'est connue qu'une fois son contenu écrit.
  if (g_painting.art != nullptr) {
    // Le bas du cadre s'ajoute sous le contenu, moins la marge qu'ImGui y met
    // déjà : c'est ce qui laisse la dernière ligne défiler au-dessus du cadre.
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    window->DC.CursorMaxPos.y += std::max(0.0F, g_painting.frame_bottom - window->WindowPadding.y);
    PaintWindowScrollbar(*g_painting.art);
    PaintWindowResizeGrip(*g_painting.art, g_painting.description
                                               ? kDescriptionResizeGripInsetPixels
                                               : kResizeGripInsetPixels);
  }
  ImGui::End();
  if (g_painting.tight) {
    ImGui::PopStyleVar();
  }
  if (g_painting.skinned) {
    PopSkinStyle(g_painting.style);
  }
  g_painting = Painting{};
}

namespace {
// Ce que la boîte modale a posé sur le style, et qu'il faudra retirer. ⚠ UNE
// SEULE À LA FOIS, comme les fenêtres : deux modales ne s'imbriquent pas ici.
struct ModalPainting {
  bool skinned = false;
  SkinStylePushed style;
  bool tight = false;
  // L'habillage, pour les bulles qui naissent dans la modale.
  const SkinArt* art = nullptr;
};
ModalPainting g_modal;
// L'habillage qu'un cadre non habillé prête à ses surgissants. Voir `LendSkinArt`.
const SkinArt* g_lent_art = nullptr;
}  // namespace

const SkinArt* PaintingSkinArt() noexcept {
  // ⚠ LA MODALE D'ABORD : elle s'ouvre PAR-DESSUS une fenêtre, et c'est en
  // elle que la bulle naît. Le prêt en dernier : une fenêtre habillée a le sien.
  const SkinArt* const window_art = g_painting.art != nullptr ? g_painting.art : g_lent_art;
  return g_modal.art != nullptr ? g_modal.art : window_art;
}

void LendSkinArt(const SkinArt* art) noexcept { g_lent_art = art; }

bool BeginSkinnedModal(const char* name, const SkinArt& art, ImGuiWindowFlags flags,
                       ModalEscape escape) {
  g_modal = ModalPainting{};
  // ⚠ AU CENTRE, À L'APPARITION : elle interrompt, donc elle tombe sous les
  // yeux, et le joueur peut ensuite la déplacer.
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
  g_modal.skinned = art.dresses_windows();
  if (g_modal.skinned) {
    g_modal.style = PushSkinStyle(art);
    g_modal.art = &art;
    // LA BARRE À LA HAUTEUR DE SON ART, et la marge des cadres avec elle : voir
    // `BeginSkinnedWindow`, qui fait la même chose pour la même raison.
    const float bar_height = art.Get(render::SkinPiece::kTitleBarLeft).height * Scale();
    const float padding_y = std::max(0.0F, (bar_height - ImGui::GetFontSize()) * 0.5F);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(ImGui::GetStyle().FramePadding.x, padding_y));
    g_modal.tight = true;
  }
  if (!ImGui::BeginPopupModal(name, nullptr, flags)) {
    if (g_modal.tight) {
      ImGui::PopStyleVar();
    }
    if (g_modal.skinned) {
      PopSkinStyle(g_modal.style);
    }
    g_modal = ModalPainting{};
    return false;
  }
  if (g_modal.skinned) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* into = window->DrawList;
    const ImRect bar = window->TitleBarRect();
    const float scale = Scale();
    into->PushClipRect(bar.Min, bar.Max, false);
    PaintStrip(into, art, render::SkinPiece::kTitleBarLeft, render::SkinPiece::kTitleBarMiddle,
               render::SkinPiece::kTitleBarRight, bar.Min, bar.Max);
    float text_x = bar.Min.x + kBulletMarginPixels * scale;
    const SkinImage& bullet = art.Get(render::SkinPiece::kBulletOff);
    if (bullet.usable()) {
      const float side = bullet.width * scale;
      const ImVec2 at(text_x, bar.Min.y + (bar.GetHeight() - side) * 0.5F);
      PaintArt(into, art, bullet.id, at, ImVec2(at.x + side, at.y + side));
      text_x += side + kBulletTextGapPixels * scale;
    }
    const std::string_view shown = ShownTitle(name);
    into->AddText(ImVec2(text_x, bar.Min.y + (bar.GetHeight() - ImGui::GetTextLineHeight()) * 0.5F -
                                     kTitleRaisePixels * scale),
                  art.look().title_text, shown.data(), shown.data() + shown.size());
    into->PopClipRect();
  }
  if (escape == ModalEscape::kCloses && ImGui::IsKeyPressed(ImGuiKey_Escape) &&
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
    ImGui::CloseCurrentPopup();
  }
  return true;
}

void EndSkinnedModal() {
  ImGui::EndPopup();
  if (g_modal.tight) {
    ImGui::PopStyleVar();
  }
  if (g_modal.skinned) {
    PopSkinStyle(g_modal.style);
  }
  g_modal = ModalPainting{};
}

bool BeginSkinnedTooltip(const SkinArt& art, float widest) {
  // ⚠ LE FOND EST CELUI DE L'HABILLAGE, ARRONDI, PEINT PAR IMGUI, et sa bordure
  // RETIRÉE : c'est elle qui ferait un liseré sombre à angles droits. Le seul
  // cadre visible doit être l'art du client.
  const float edge = DescPanelEdge(art);
  PushPaperStyle(art.look());
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kSkinnedTooltipRounding);
  // ⚠ LA MARGE EST L'ÉPAISSEUR DU CADRE : sans elle, le texte passerait dessous.
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(edge, edge));
  const float most_wide = widest > 0.0F ? widest : kSkinnedTooltipWidest;
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(0.0F, 0.0F),
      ImVec2(most_wide, ImGui::GetIO().DisplaySize.y * kSkinnedTooltipScreenShare));
  if (!ImGui::BeginTooltip()) {
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(kPaperStyleColors);
    return false;
  }
  // 🔴 DEUX CANAUX, ET C'EST LE SEUL MOYEN : voir l'en-tête.
  ImDrawList* const into = ImGui::GetWindowDrawList();
  into->ChannelsSplit(2);
  into->ChannelsSetCurrent(1);
  return true;
}

void EndSkinnedTooltip(const SkinArt& art) {
  ImDrawList* const into = ImGui::GetWindowDrawList();
  into->ChannelsSetCurrent(0);
  // ⚠ SANS FOND : ImGui vient d'en peindre un, ARRONDI. Celui du cadre est à
  // angles droits et lui écraserait les coins.
  DrawDescPanelFrameAroundWindow(into, art, /*fill_bg=*/false);
  into->ChannelsMerge();
  ImGui::EndTooltip();
  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(kPaperStyleColors);
}

}  // namespace ro::ui
