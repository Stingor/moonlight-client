#include "ro/ui/description_window.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>

#include <imgui_internal.h>

#include "ro/ui/bug_report_window.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "description_odds.h"

namespace ro::ui {
namespace {

// La taille d'ouverture de la FENÊTRE, et son plancher.
//
// ⚠ GRANDE, ET C'EST NÉCESSAIRE : une description n'a qu'un contenu, du TEXTE.
// Une fenêtre étroite y coupe chaque phrase en deux, et l'on passe son temps à
// l'élargir.
constexpr float kFirstWidthPixels = 560.0F;
constexpr float kFirstHeightPixels = 420.0F;
constexpr float kLeastWidthPixels = 320.0F;
// Deux colonnes demandent de la place : en comparaison, la fenêtre s'élargit.
constexpr float kComparingLeastWidthPixels = 500.0F;
constexpr float kLeastHeightPixels = 300.0F;

// LA PLUS GRANDE DIMENSION QU'ON LAISSE À L'ILLUSTRATION.
//
// 🔴 SUR LE PLUS GRAND CÔTÉ, jamais sur la hauteur seule : l'art de collection
// va du portrait au bandeau, et borner la hauteur laisse une image large
// s'étaler sur toute la fenêtre.
constexpr float kWindowPictureSidePixels = 120.0F;
// Dans l'aperçu, elle est bornée par sa HAUTEUR : l'infobulle a une largeur
// fixe, et c'est la hauteur qui décide de la place qu'il reste au texte.
constexpr float kPreviewPictureHeightPixels = 110.0F;
// De combien l'ombre d'un nom cassé se décale, dans l'aperçu.
constexpr float kBrokenShadowPixels = 1.0F;

// LE FORGERON D'UN OBJET : bleu quand le serveur l'a nommé, rouge tant qu'il ne
// l'a pas fait, sur un halo gris clair — les couleurs du client, pour son papier
// blanc.
constexpr ImU32 kCreatorKnownColor = IM_COL32(0x40, 0x40, 0xE0, 255);
constexpr ImU32 kCreatorUnknownColor = IM_COL32(0xE0, 0x40, 0x40, 255);
constexpr ImU32 kCreatorHaloColor = IM_COL32(0xE0, 0xE0, 0xE0, 255);

// L'arrondi de ses coins, celui de la fenêtre de description du client.
constexpr float kDescriptionPreviewRounding = 4.0F;

// Le champ de quantité du staff, boutons compris, en largeurs de police, et le
// pas que Ctrl tenu fait faire.
constexpr float kStaffAmountWidthEm = 7.0F;
constexpr int kStaffAmountFastStep = 10;

// L'image à montrer, à la taille voulue. Rend une largeur nulle quand il n'y en
// a pas.
[[nodiscard]] ImVec2 PictureSize(const SkinImage& picture, float longest_side,
                                 bool bound_height_only) {
  if (!picture.usable() || picture.width <= 0.0F || picture.height <= 0.0F) {
    return ImVec2(0.0F, 0.0F);
  }
  const float biggest =
      bound_height_only ? picture.height : std::max(picture.width, picture.height);
  const float shrink = biggest > longest_side ? longest_side / biggest : 1.0F;
  return ImVec2(picture.width * shrink, picture.height * shrink);
}

// LE POINT DE LA FENÊTRE QUI SE POSE SUR LE CURSEUR, en fractions de sa
// taille : le coin OPPOSÉ à la direction où elle s'étend.
[[nodiscard]] ImVec2 AnchorPivot(DescriptionAnchor anchor) {
  switch (anchor) {
    case DescriptionAnchor::kTopLeft:
      return ImVec2(1.0F, 1.0F);
    case DescriptionAnchor::kTopRight:
      return ImVec2(0.0F, 1.0F);
    case DescriptionAnchor::kBottomLeft:
      return ImVec2(1.0F, 0.0F);
    case DescriptionAnchor::kBottomRight:
      return ImVec2(0.0F, 0.0F);
    case DescriptionAnchor::kCenter:
      return ImVec2(0.5F, 0.5F);
  }
  return ImVec2(0.0F, 0.0F);
}

// LE NOM, MORCEAU PAR MORCEAU, sur une ligne : chacun sa couleur, et le halo
// sous le forgeron. `ordinary` est l'encre des autres morceaux ; `shadow`, non
// nulle, l'ombre rouge d'un objet cassé sous le nom entier.
void DrawNamePieces(std::span<const DescriptionNamePiece> pieces, ImU32 ordinary, ImU32 shadow) {
  // Les couleurs du nom sont celles du client, pour son papier blanc : sur le
  // papier réglé, celles qui ne s'y liraient plus s'éclaircissent.
  const ImU32 paper = PaintingLook().paper;
  const bool light_paper =
      ContrastRatio(paper, IM_COL32_BLACK) > ContrastRatio(paper, IM_COL32_WHITE);
  bool first = true;
  for (const DescriptionNamePiece& piece : pieces) {
    if (piece.text.empty()) {
      continue;
    }
    if (!first) {
      ImGui::SameLine(0.0F, 0.0F);
    }
    first = false;
    const bool creator = piece.tone != DescriptionNameTone::kOrdinary;
    // ⚠ LE HALO DU FORGERON N'EST QU'UN RELIEF SUR UN PAPIER CLAIR : sur un sombre,
    // ce trait clair sous le nom se lirait comme une ombre à l'envers.
    const ImU32 halo = creator ? (light_paper ? kCreatorHaloColor : 0) : shadow;
    const char* const begin = piece.text.data();
    const char* const end = begin + piece.text.size();
    if (halo != 0) {
      const ImVec2 at = ImGui::GetCursorScreenPos();
      ImGui::GetWindowDrawList()->AddText(
          ImVec2(at.x + kBrokenShadowPixels, at.y + kBrokenShadowPixels), halo, begin, end);
    }
    const ImU32 ink = piece.tone == DescriptionNameTone::kCreator          ? kCreatorKnownColor
                      : piece.tone == DescriptionNameTone::kUnknownCreator ? kCreatorUnknownColor
                                                                           : ordinary;
    ImGui::PushStyleColor(ImGuiCol_Text, ReadableOn(ink, paper));
    ImGui::TextUnformatted(begin, end);
    ImGui::PopStyleColor();
  }
}

}  // namespace

// À L'OUVERTURE, sous le curseur si le réglage le veut. `was_open` dit si la
// fenêtre l'était à l'image d'avant ; il est remis à jour ici.
void PlaceDescription(bool& place_at_mouse, bool& was_open, const DescriptionPlacement& placement) {
  const bool opening = !was_open;
  was_open = true;
  if (!place_at_mouse) {
    return;
  }
  place_at_mouse = false;
  if (!opening || !placement.at_cursor) {
    return;
  }
  const ImVec2 mouse = ImGui::GetMousePos();
  ImGui::SetNextWindowPos(ImVec2(mouse.x + placement.offset_x, mouse.y + placement.offset_y),
                          ImGuiCond_Always, AnchorPivot(placement.anchor));
}

// DANS L'ÉCRAN, toujours : un ancrage près d'un bord, ou une fenêtre agrandie,
// la ferait déborder, et une barre de titre hors de l'écran ne se rattrape plus.
void KeepDescriptionOnScreen() {
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  const ImVec2 at = ImGui::GetWindowPos();
  const ImVec2 size = ImGui::GetWindowSize();
  const ImVec2 kept(std::max(0.0F, std::min(at.x, screen.x - size.x)),
                    std::max(0.0F, std::min(at.y, screen.y - size.y)));
  if (kept.x != at.x || kept.y != at.y) {
    ImGui::SetWindowPos(kept);
  }
}

bool IsDatabaseLinkLine(std::string_view line) {
  return line.find("<URL>") != std::string_view::npos;
}

namespace {

// Les variables de style que pose `PushPreviewStyle`.
constexpr int kPreviewStyleVars = 5;

// LE DESSIN D'UNE CARTE EN ENTIER, quand la vue le demande et qu'il est là.
[[nodiscard]] bool ShowsWholePicture(const DescriptionView& view) {
  return view.picture_whole && view.picture_is_card && view.picture.usable();
}

// LE PAPIER DE L'APERÇU, et sa largeur maximale.
//
// ⚠ LE FOND EST LE PAPIER, ARRONDI, PEINT PAR IMGUI, et sa bordure RETIRÉE :
// c'est elle qui ferait le liseré sombre à angles droits. Le seul cadre visible
// doit être l'art du client. Les mêmes réglages valent pour une infobulle et
// pour un panneau : `Window…` pour l'une, `Child…` pour l'autre.
[[nodiscard]] float PushPreviewStyle(const DescriptionView& view, const SkinArt& art) {
  const float edge = DescPanelEdge(art);
  PushPaperStyle(art.look());
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kDescriptionPreviewRounding);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kDescriptionPreviewRounding);
  // ⚠ LA MARGE EST L'ÉPAISSEUR DU CADRE : sans elle, le texte passerait dessous.
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(edge, edge));
  // ⚠ LE DESSIN ENTIER ÉLARGIT L'APERÇU DE SA PROPRE LARGEUR : le texte garde la
  // sienne à côté, au lieu de se tasser dans ce que l'image lui laisse.
  const float picture_room =
      ShowsWholePicture(view) ? view.picture.width + kPictureGapPixels : 0.0F;
  const float widest = kDescriptionPreviewWidest + picture_room;
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(0.0F, 0.0F),
      ImVec2(widest, ImGui::GetIO().DisplaySize.y * kDescriptionPreviewScreenShare));
  return widest;
}

void PopPreviewStyle() {
  ImGui::PopStyleVar(kPreviewStyleVars);
  ImGui::PopStyleColor(kPaperStyleColors);
}

// LE CORPS DE L'APERÇU, dans l'infobulle ou le panneau déjà ouvert.
void DrawPreviewBody(const DescriptionView& view, const SkinArt& art, float widest);

}  // namespace

void DrawDescriptionPreview(const DescriptionView& view, const SkinArt& art) {
  if (view.item_id == 0) {
    return;
  }
  // ── UNE INFOBULLE, ET PAS UNE FENÊTRE ─────────────────────────────────────
  //
  // 🔴 IMGUI ORDONNE LES FENÊTRES PAR FOCUS : une fenêtre ordinaire, même
  // soumise après celle qu'on survole, passerait DERRIÈRE elle. Les infobulles
  // vivent dans la couche avant, et ImGui les recadre lui-même près des bords.
  const float widest = PushPreviewStyle(view, art);
  if (ImGui::BeginTooltip()) {
    DrawPreviewBody(view, art, widest);
    ImGui::EndTooltip();
  }
  PopPreviewStyle();
}

void DrawDescriptionPreviewPanel(const char* id, const DescriptionView& view, const SkinArt& art) {
  if (view.item_id == 0) {
    return;
  }
  const float widest = PushPreviewStyle(view, art);
  // ⚠ LE PANNEAU SE DIMENSIONNE SUR SON CONTENU, comme l'infobulle : une
  // description courte donne un petit papier.
  ImGui::PushStyleColor(ImGuiCol_ChildBg, art.look().paper);
  if (ImGui::BeginChild(id, ImVec2(0.0F, 0.0F),
                        ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY |
                            ImGuiChildFlags_AlwaysUseWindowPadding,
                        ImGuiWindowFlags_NoScrollbar)) {
    DrawPreviewBody(view, art, widest);
  }
  ImGui::EndChild();
  ImGui::PopStyleColor();
  PopPreviewStyle();
}

namespace {

void DrawPreviewBody(const DescriptionView& view, const SkinArt& art, float widest) {
  const float edge = DescPanelEdge(art);
  // 🔴 DEUX CANAUX, ET C'EST LE SEUL MOYEN. Le cadre doit être DERRIÈRE le
  // contenu, mais on ne connaît la taille de l'infobulle qu'une fois le contenu
  // écrit. On écrit donc le contenu sur le canal 1, le cadre sur le 0, et la
  // fusion les remet dans l'ordre.
  ImDrawList* const into = ImGui::GetWindowDrawList();
  into->ChannelsSplit(2);
  into->ChannelsSetCurrent(1);
  const float wrap_width = widest - 2.0F * edge;
  // ── LE NOM, DANS LE CORPS ─────────────────────────────────────────────────
  //
  // 🔴 ET C'EST CE QUI SÉPARE L'APERÇU DE LA FENÊTRE : une infobulle n'a pas de
  // barre de titre où le mettre. La fenêtre, elle, l'a — et l'écrire aussi dans
  // son corps le dirait deux fois.
  const std::string shown_name(view.name);
  ImGui::PushTextWrapPos(wrap_width > 0.0F ? ImGui::GetCursorPosX() + wrap_width : 0.0F);
  if (!view.name_pieces.empty()) {
    DrawNamePieces(view.name_pieces, kDescriptionNameColor,
                   view.damaged ? kDescriptionBrokenShadow : 0);
  } else if (view.damaged) {
    const ImVec2 at = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(at.x + kBrokenShadowPixels, at.y + kBrokenShadowPixels));
    ImGui::PushStyleColor(ImGuiCol_Text, kDescriptionBrokenShadow);
    ImGui::TextUnformatted(shown_name.c_str());
    ImGui::PopStyleColor();
    ImGui::SetCursorPos(at);
  }
  if (view.name_pieces.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ReadableOn(kDescriptionNameColor, art.look().paper));
    ImGui::TextUnformatted(shown_name.c_str());
    ImGui::PopStyleColor();
  }
  ImGui::PopTextWrapPos();

  // ⚠ L'IDENTIFIANT EN CLAIR, ET NON LE LIEN : une infobulle ne se clique pas,
  // un lien y serait mort. La fenêtre, qui se clique, fait l'inverse.
  ImGui::TextDisabled(core::Tr("ID : %u"), view.item_id);

  std::span<const std::string> lines = view.lines;
  if (!lines.empty() && IsDatabaseLinkLine(lines.front())) {
    lines = lines.subspan(1);
  }
  const bool has_text = !lines.empty();
  const ImVec2 picture = ShowsWholePicture(view)
                             ? ImVec2(view.picture.width, view.picture.height)
                             : PictureSize(view.picture, kPreviewPictureHeightPixels, true);
  if (picture.x > 0.0F || has_text) {
    ImGui::Separator();
  }

  float text_width = wrap_width > 0.0F ? wrap_width : ImGui::GetContentRegionAvail().x;
  if (picture.x > 0.0F) {
    ImGui::Image(view.picture.id, picture);
    if (has_text) {
      // 🔴 LE TEXTE VA À DROITE DE L'IMAGE, donc sa largeur de repli DIMINUE
      // d'autant. Gardée entière, il déborde de l'infobulle et se coupe.
      ImGui::SameLine(0.0F, kPictureGapPixels);
      text_width -= picture.x + kPictureGapPixels;
    }
  }
  if (has_text) {
    ImGui::BeginGroup();
    RichText laid = LayOutRichTextHere(lines, ImGui::GetColorU32(ImGuiCol_Text), text_width);
    KeepReadable(laid, art.look().paper);
    // ⚠ DESSINÉ, PAS RENDU INTERACTIF : on ne sélectionne rien dans une
    // infobulle, et l'on n'y suit aucun lien. Le résultat se jette.
    static_cast<void>(DrawRichText("##apercu", laid, ImGui::GetTextLineHeightWithSpacing()));
    ImGui::EndGroup();
  } else if (picture.x <= 0.0F) {
    ImGui::TextDisabled("%s", core::Tr("(pas de description)"));
  }
  DrawPreviewInstance(view);

  into->ChannelsSetCurrent(0);
  // ⚠ SANS FOND : ImGui vient d'en peindre un, ARRONDI. Celui du cadre est à
  // angles droits et lui écraserait les coins.
  DrawDescPanelFrameAroundWindow(into, art, /*fill_bg=*/false);
  into->ChannelsMerge();
}

// LA LIGNE DU STAFF : « Staff : [N] [@item] ». La quantité est celle de l'état
// de la fenêtre, qui la retient d'un item à l'autre.
void DrawStaffItemRow(const DescriptionView& view, const SkinArt& art, int& amount,
                      DescriptionWindowAction& action) {
  amount = std::clamp(amount, 1, kStaffItemMostAmount);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(core::Tr("Staff :"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * kStaffAmountWidthEm);
  static_cast<void>(SkinInputInt("##staff_item_amount", &amount, 1, kStaffItemMostAmount, 1,
                                 kStaffAmountFastStep, art));
  // 🔴 LE CHAMP GARDE SA QUANTITÉ, la demande seule est bornée : le champ est
  // retenu d'un item à l'autre, et une armure ne doit pas réduire la quantité
  // de la potion décrite ensuite.
  const int granted = std::min(amount, StaffItemMostFor(view.stackable, view.free_bag_slots));
  const StaffItemRoom room = StaffItemRoomFor(view.stackable, view.free_bag_slots, amount);
  ImGui::SameLine();
  ImGui::BeginDisabled(granted <= 0);
  if (SkinSmallButton(core::Tr("@item"), art)) {
    action.staff_item = StaffItemGrant{.item_id = view.item_id, .amount = granted};
  }
  ImGui::EndDisabled();
  const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
  if (room == StaffItemRoom::kBounded) {
    ImGui::SameLine();
    ImGui::TextDisabled(core::Tr("(%d au plus)"), granted);
  }
  if (!hovered) {
    return;
  }
  switch (room) {
    case StaffItemRoom::kFull:
      SkinTooltipFormat("%s", core::Tr("L'inventory est plein : cet item ne s'empile pas, chaque "
                                       "exemplaire prend une case."));
      break;
    case StaffItemRoom::kBounded:
      SkinTooltipFormat(core::Tr("Cet item ne s'empile pas : chaque exemplaire prend une case, "
                                 "et la demande est ramenée aux %d cases libres de l'inventory."),
                        granted);
      break;
    case StaffItemRoom::kFits:
      SkinTooltipFormat("%s", core::Tr("Envoie « @item » au serveur : l'item décrit, à cette "
                                       "quantité. Un équipement ne s'empile pas : chaque "
                                       "exemplaire prend sa case."));
      break;
  }
}

// LE CORPS D'UNE DESCRIPTION : l'illustration, le lien de la base de données, et
// le texte. Il est tout l'onglet « Description » d'un conteneur, et toute la
// fenêtre des autres objets.
//
// `staff_amount` est la quantité du staff, nulle pour la pièce portée d'une
// comparaison : sans elle, pas de ligne du staff.
void DrawItemBody(DescriptionDollState* doll, int* staff_amount, const DescriptionView& view,
                  const SkinArt& art, RichTextSelect select, DescriptionWindowAction& action) {
  // ── L'EN-TÊTE : L'ILLUSTRATION, ET LE LIEN À SA DROITE ────────────────────
  std::span<const std::string> lines = view.lines;
  const bool has_link = !lines.empty() && IsDatabaseLinkLine(lines.front());
  const ImVec2 picture = PictureSize(view.picture, kWindowPictureSidePixels, false);
  if (picture.x > 0.0F) {
    ImGui::Image(view.picture.id, picture);
    // LE DESSIN D'UNE CARTE SE REGARDE EN ENTIER AU SURVOL, comme au client.
    //
    // ⚠ UNE INFOBULLE SANS RIEN AUTOUR : ni fond ni bordure. Le dessin porte son
    // propre cadre — c'est une carte à jouer — et un second cadre autour ferait
    // deux bords qui ne s'alignent pas.
    //
    // ⚠ SEULEMENT POUR UNE CARTE. L'illustration de collection est déjà ce que
    // la vignette montre à sa taille, et l'icône de vingt-quatre pixels agrandie
    // ne serait qu'une bouillie.
    if (view.picture_is_card && ImGui::IsItemHovered()) {
      ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32_BLACK_TRANS);
      ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32_BLACK_TRANS);
      if (ImGui::BeginTooltip()) {
        ImGui::Image(view.picture.id, ImVec2(view.picture.width, view.picture.height));
        ImGui::EndTooltip();
      }
      ImGui::PopStyleColor(2);
    }
    // L'ILLUSTRATION D'UNE PIÈCE QUI SE PORTE montre le pantin qui la porte,
    // comme sa ligne « ViewID » — et celle d'un costume qui n'a qu'un effet de
    // chapeau, qui n'a pas de ligne à survoler.
    if (!view.picture_is_card && doll != nullptr && (view.doll_possible || view.doll_effect) &&
        ImGui::IsItemHovered()) {
      // ⚠ UNE IMAGE N'A PAS D'IDENTIFIANT : la molette se nomme ici.
      ShowDollOver(*doll, ImGui::GetID("##pantin_illustration"));
    }
    ImGui::SameLine(0.0F, kPictureGapPixels);
  }
  ImGui::BeginGroup();
  if (!view.name_pieces.empty()) {
    DrawNamePieces(view.name_pieces, ImGui::GetColorU32(ImGuiCol_Text),
                   view.damaged ? kDescriptionBrokenShadow : 0);
  }
  if (has_link) {
    // 🔴 LA LIGNE DE LA BASE DE DONNÉES REMPLACE « ID : N », elle ne s'ajoute pas
    // à elle. Elle PORTE déjà l'identifiant — « ItemID : 12103 » — et elle est
    // cliquable ; écrire les deux, c'est le même nombre sur deux lignes.
    RichText header = LayOutRichTextHere(lines.subspan(0, 1), ImGui::GetColorU32(ImGuiCol_Text),
                                         ImGui::GetContentRegionAvail().x);
    KeepReadable(header, art.look().paper);
    const RichTextAction followed =
        DrawRichText("##lien_base", header, ImGui::GetTextLineHeightWithSpacing(), select);
    if (followed.followed >= 0 && followed.followed < static_cast<int>(header.links.size())) {
      const RichLink& link = header.links[static_cast<std::size_t>(followed.followed)];
      action.followed = link.payload;
      action.followed_kind = link.kind;
    }
    lines = lines.subspan(1);
  } else {
    // ⚠ REPLI QUAND LA TABLE N'A PAS DE LIEN : l'identifiant reste la seule façon
    // de retrouver un objet ailleurs, et il ne doit pas disparaître.
    ImGui::TextDisabled(core::Tr("ID : %u"), view.item_id);
  }
  // « VIEWID : N », REMONTÉ SOUS L'IDENTIFIANT et rendu vivant : la ligne que
  // la table écrit au même endroit se retire, elle dirait le même nombre.
  if (doll != nullptr && view.view_id != 0 && view.doll_possible) {
    DrawViewIdLine(*doll, view);
    if (!lines.empty() && lines.front().find("ViewID") != std::string::npos) {
      lines = lines.subspan(1);
    }
  }
  // « + ALOOTID » : ramasser automatiquement cet item, d'un clic. ⚠ LE LIBELLÉ
  // PORTE L'ÉTAT — « + » hors de la liste, « - » dedans — : l'habillage du
  // bouton ignore les couleurs, un fond rouge ou vert ne se verrait pas.
  bool action_row = false;
  if (view.autoloot != DescriptionAutoloot::kUnavailable) {
    const bool listed = view.autoloot == DescriptionAutoloot::kIn;
    if (SkinSmallButton(listed ? core::Tr("- alootid") : core::Tr("+ alootid"), art)) {
      action.toggle_autoloot = true;
    }
    SkinTooltip(listed ? core::Tr("Cet item est dans ta liste de ramassage automatique. "
                                  "Clique pour l'en retirer.")
                       : core::Tr("Ramasser automatiquement cet item."));
    // « MARCHÉ », à côté et aux mêmes conditions : le Marché s'ouvre sur
    // l'identifiant de l'item. « BASE DE DONNÉES », juste après : la fiche
    // entière de l'item. ⚠ DANS LA BASE, son en-tête porte déjà « Marché ».
    if (!view.in_database) {
      ImGui::SameLine();
      if (SkinSmallButton(core::Tr("Marché"), art)) {
        action.search_market = true;
      }
      SkinTooltip(core::Tr("Ouvre le Marché sur cet item : qui le vend, qui l'achète."));
      ImGui::SameLine();
      if (SkinSmallButton(core::Tr("Base de données"), art)) {
        action.open_database = true;
      }
      SkinTooltip(core::Tr("Ouvre la fiche de cet item dans la base de données."));
    }
    action_row = true;
  }
  // « LIRE » ET « LECTURE AUTO », pour un livre qu'on possède.
  if (view.readable_book) {
    if (action_row) {
      ImGui::SameLine();
    }
    if (SkinSmallButton(core::Tr("Lire"), art)) {
      action.read_book = true;
    }
    ImGui::SameLine();
    if (SkinSmallButton(core::Tr("Lecture auto"), art)) {
      action.recite_book = true;
    }
    SkinTooltip(
        core::Tr("Le personnage lit le livre à voix haute, ligne par ligne, dans le chat."));
    action_row = true;
  }
  // « VOTE SHOP : N PTS », sous l'identifiant : l'item s'achète, et voici son
  // prix. Le clic ouvre la boutique avec l'item au panier.
  if (view.vote_shop_label != nullptr) {
    if (action_row) {
      ImGui::SameLine();
    }
    if (SkinSmallButton(view.vote_shop_label, art)) {
      action.open_vote_shop = true;
    }
    SkinTooltip(core::Tr("Ouvre le Vote Shop avec cet item dans le panier."));
    action_row = true;
  }
  // L'ÉTIQUETTE « CRAFT », sous l'identifiant : elle informe autant qu'elle
  // agit, son libellé portant déjà le skill ou le nombre de recettes.
  if (view.craft_label != nullptr) {
    if (action_row) {
      ImGui::SameLine();
    }
    if (SkinSmallButton(view.craft_label, art)) {
      action.open_craft_atlas = true;
    }
    if (view.craft_tip != nullptr) {
      SkinTooltip(view.craft_tip);
    }
  }
  if (view.staff_item && staff_amount != nullptr) {
    DrawStaffItemRow(view, art, *staff_amount, action);
  }
  ImGui::EndGroup();

  // ── LE CORPS, PLEINE LARGEUR ──────────────────────────────────────────────
  //
  // 🔴 SOUS L'ILLUSTRATION, PAS À CÔTÉ. C'est l'autre chose qui sépare la
  // fenêtre de l'aperçu : une infobulle est étroite et serre son texte contre
  // l'image, une fenêtre est large et lui donne toute sa largeur. Un texte
  // coincé dans une colonne de deux cents pixels au milieu d'une fenêtre qui en
  // fait cinq cents est ce qu'on remarque en premier.
  ImGui::Separator();
  if (lines.empty()) {
    ImGui::TextDisabled("%s", core::Tr("(pas de description)"));
  } else {
    RichText laid = LayOutRichTextHere(lines, ImGui::GetColorU32(ImGuiCol_Text),
                                       ImGui::GetContentRegionAvail().x);
    KeepReadable(laid, art.look().paper);
    const RichTextAction followed =
        DrawRichText("##description", laid, ImGui::GetTextLineHeightWithSpacing(), select);
    if (followed.followed >= 0 && followed.followed < static_cast<int>(laid.links.size())) {
      // ⚠ LA CHARGE SE RECOPIE : la disposition meurt à la fin de cette image, et
      // l'appelant lira l'action après.
      const RichLink& link = laid.links[static_cast<std::size_t>(followed.followed)];
      action.followed = link.payload;
      action.followed_kind = link.kind;
    }
  }
}

// UNE COLONNE DE LA COMPARAISON : son rôle, le nom en couleurs, le corps.
//
// ⚠ LES GESTES DE LA PIÈCE PORTÉE NE VALENT PAS POUR LA DÉCRITE : ses boutons
// n'en ont pas, et seul un lien suivi dans son texte remonte.
void DrawComparedColumn(DescriptionDollState* doll, int* staff_amount, const DescriptionView& view,
                        const char* role, const SkinArt& art, RichTextSelect select,
                        DescriptionWindowAction& action, bool owns_gestures) {
  ImGui::TextDisabled("%s", role);
  const DescriptionNamePiece whole{.text = view.name};
  const std::span<const DescriptionNamePiece> pieces =
      view.name_pieces.empty() ? std::span<const DescriptionNamePiece>(&whole, 1)
                               : view.name_pieces;
  ImGui::PushTextWrapPos(0.0F);
  DrawNamePieces(pieces, kDescriptionNameColor, view.damaged ? kDescriptionBrokenShadow : 0);
  ImGui::PopTextWrapPos();
  if (owns_gestures) {
    DrawItemBody(doll, staff_amount, view, art, select, action);
    return;
  }
  DescriptionWindowAction borrowed;
  DrawItemBody(nullptr, nullptr, view, art, select, borrowed);
  if (!borrowed.followed.empty()) {
    action.followed = std::move(borrowed.followed);
    action.followed_kind = borrowed.followed_kind;
  }
}

}  // namespace

// L'ONGLET « DESCRIPTION » : le corps seul, ou la comparaison en deux colonnes,
// et la case qui passe de l'un à l'autre quand une pièce portée existe.
void DrawDescriptionTab(DescriptionWindowState& state, const DescriptionView& view,
                        const SkinArt& art, DescriptionWindowAction& action) {
  if (view.compared == nullptr) {
    DrawItemBody(&state.doll, &state.staff_item_amount, view, art, state.text_select, action);
    return;
  }
  bool shown = state.compare;
  if (SkinCheckBox(core::Tr("Comparer"), &shown, art)) {
    state.compare = shown;
    action.compare_changed = true;
    action.compare = shown;
  }
  ImGui::Separator();
  if (!state.compare) {
    DrawItemBody(&state.doll, &state.staff_item_amount, view, art, state.text_select, action);
    return;
  }
  constexpr ImGuiTableFlags kFlags =
      ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame;
  // ⚠ LES DEUX COLONNES DESSINENT LE MÊME CORPS, donc les mêmes widgets : chacune
  // a sa propre portée d'identifiants, sans quoi ImGui les confond au survol.
  enum ComparedColumnId : int { kWornColumnId, kDescribedColumnId };
  if (ImGui::BeginTable("##comparaison", 2, kFlags)) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::PushID(kWornColumnId);
    DrawComparedColumn(nullptr, nullptr, *view.compared, core::Tr("Équipé"), art, state.text_select,
                       action, false);
    ImGui::PopID();
    ImGui::TableNextColumn();
    ImGui::PushID(kDescribedColumnId);
    DrawComparedColumn(&state.doll, &state.staff_item_amount, view, core::Tr("Item"), art,
                       state.text_select, action, true);
    ImGui::PopID();
    ImGui::EndTable();
  }
}

DescriptionWindowAction DrawDescriptionWindow(DescriptionWindowState& state,
                                              const DescriptionView& view, const SkinArt& art) {
  DescriptionWindowAction action;
  NoteDescriptionNavigation(state);
  // Le pantin n'est voulu que tant que sa ligne ou l'illustration est survolée,
  // et elles le redisent.
  state.doll.wanted = false;
  if (!state.open || state.item_id == 0) {
    state.was_open = false;
    return action;
  }

  // ── LE TITRE EST LE NOM DE L'OBJET ────────────────────────────────────────
  //
  // 🔴 DANS LA BARRE, ET NULLE PART AILLEURS. C'est là que le client le met, et
  // c'est ce qui laisse la fenêtre reconnaissable quand on en ouvre trois.
  //
  // ⚠ RECOMPOSÉ SEULEMENT QUAND IL LE FAUT : refait à chaque image, il coûterait
  // une allocation par image pour un texte qui ne bouge qu'au changement d'objet
  // ou de langue.
  // 🔴 EN COMPARAISON, LE TITRE DIT « COMPARAISON » : deux noms ne tiennent pas
  // dans une barre, et chaque colonne écrit le sien.
  const bool comparing = view.compared != nullptr && state.compare;
  const std::size_t language = core::LanguageSerial();
  if (state.title.empty() || view.name != state.title_for || language != state.title_language ||
      view.damaged != state.title_damaged || comparing != state.title_comparing) {
    state.title_for = std::string(view.name);
    state.title_language = language;
    state.title_damaged = view.damaged;
    state.title_comparing = comparing;
    // ⚠ LE REPLI EST ÉCRIT EN TOUTES LETTRES : le relevé des traductions ne lit
    // qu'un appel porté sur un LITTÉRAL.
    state.title = comparing           ? std::string(core::Tr("Comparaison"))
                  : view.name.empty() ? std::string(core::Tr("Description"))
                                      : state.title_for;
    if (view.damaged && !comparing) {
      // ⚠ ÉCRIT EN TOUTES LETTRES ICI, et en ANGLAIS : le relevé des traductions
      // ne lit qu'un appel porté sur un LITTÉRAL, et « Broken » est le mot que
      // les joueurs lisent au client. L'espace de tête est posée à part : le
      // catalogue rognerait une espace de BORD.
      //
      // 🔴 UN ŒUF « CASSÉ » EST ÉCLOS : son familier est dehors, et l'œuf n'a
      // rien d'abîmé.
      state.title += ' ';
      state.title += view.pet_egg ? core::Tr("- Éclos") : core::Tr("- Broken");
    }
    state.title += "###";
    state.title += kDescriptionWindowId;
  }

  PlaceDescription(state.place_at_mouse, state.was_open, state.placement);
  if (state.raise) {
    ImGui::SetNextWindowFocus();
    state.raise = false;
  }
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels, kFirstHeightPixels), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(comparing ? kComparingLeastWidthPixels : kLeastWidthPixels, kLeastHeightPixels),
      ImVec2(FLT_MAX, FLT_MAX));
  // 🔴 L'OMBRE ROUGE DIT QUE L'ÉQUIPEMENT EST CASSÉ, dans la barre de titre comme
  // au client — pas en comparaison, où le titre ne porte aucun nom.
  if (view.damaged && !comparing) {
    SetNextWindowTitleShadow(kDescriptionBrokenShadow);
  }
  PoseBugReportButton(state.bug_report);
  // ⛔ NI PUCE NI ÉPINGLE, et `NoCollapse` retire le minimiser : le client ne
  // donne à sa description que sa croix. Trois boutons de plus prendraient la
  // place d'un nom d'objet décoré, qui est long.
  const bool body_shown =
      BeginSkinnedWindow(state.title.c_str(), &state.open, art, SkinnedTitle::kDescription,
                         ImGuiWindowFlags_NoCollapse);
  KeepDescriptionOnScreen();
  action.report_bug = state.bug_report && TitleActionClicked(0);
  if (!body_shown) {
    EndSkinnedWindow();
    return action;
  }

  DrawItemSheetBody(state, view, art, ItemSheetHost::kDescriptionWindow, action);

  // Les panneaux se posent sous la fenêtre ; ⚠ LE MENU D'UN LIEN S'OUVRE APRÈS
  // EUX, pour qu'une carte cliquée y navigue comme une ligne de table.
  const ImVec2 window_at = ImGui::GetWindowPos();
  const ImVec2 window_size = ImGui::GetWindowSize();
  const bool in_front = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
  EndSkinnedWindow();
  const float bottom = window_at.y + window_size.y;
  if (comparing) {
    // Chaque jeu de panneaux sous sa colonne : la pièce portée à gauche.
    const float half = window_size.x * 0.5F;
    DrawDescriptionPanels(state, *view.compared, art, ImVec2(window_at.x, bottom), half, in_front,
                          "porte", action);
    DrawDescriptionPanels(state, view, art, ImVec2(window_at.x + half, bottom), half, in_front,
                          "decrit", action);
  } else {
    DrawDescriptionPanels(state, view, art, ImVec2(window_at.x, bottom), window_size.x, in_front,
                          "decrit", action);
  }
  FinishItemSheetLinks(state, view, ItemSheetHost::kDescriptionWindow, action);
  return action;
}
}  // namespace ro::ui
