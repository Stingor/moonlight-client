#include "chat_input_chips.h"

#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <imgui_internal.h>

#include "ro/ui/chat_frame.h"

namespace ro::ui {
namespace {

// ── LES PASTILLES DE LIEN ────────────────────────────────────────────────────
//
// 🔴 LE NATIF N'ÉCRIT PAS DU TEXTE DANS SA LIGNE DE SAISIE : il y accroche un
// vrai bouton par lien (`UIItemTagButton`), coloré et cliquable avant même
// l'envoi. Un `InputText` d'ImGui n'a pas de texte riche ; la seule façon d'y
// colorer un fragment est de le RECOUVRIR du fond du champ puis de le réécrire
// dans la couleur des liens — d'où le passage APRÈS la soumission du widget, et
// le fond OPAQUE : sans opacité, le texte que le champ vient d'écrire
// transparaîtrait dessous.
//
// ⚠ LE CHAMP DÉFILE quand la ligne dépasse sa largeur, et ce décalage n'est nulle
// part ailleurs que dans l'état interne du widget — qui n'existe QUE tant qu'il
// est actif, d'où le zéro par défaut, qui est justement le décalage d'un champ
// inactif. L'ignorer collerait les pastilles sur du texte qui a glissé, d'autant
// plus visiblement que la saisie est longue : c'est-à-dire précisément quand on
// pose plusieurs liens.
//
// ⚠ ET UN LIEN PEUT CHEVAUCHER DEUX RANGÉES dans le champ à repli : sa pastille
// se peint alors en deux morceaux, sur les MÊMES coupures que le widget — celles
// de `WrapRows`, à la fonction près.
struct FieldRow {
  const char* begin;
  const char* end;
};

[[nodiscard]] std::vector<FieldRow> FieldRows(const char* text, float wrap_width) {
  const char* const end = text + std::strlen(text);
  std::vector<FieldRow> rows;
  if (wrap_width <= 0.0F) {
    rows.push_back(FieldRow{.begin = text, .end = end});
    return rows;
  }
  ImFont* font = ImGui::GetFont();
  const float font_size = ImGui::GetFontSize();
  for (const char* at = text; at < end;) {
    const char* next = ImFontCalcWordWrapPositionEx(font, font_size, at, end, wrap_width,
                                                    ImDrawTextFlags_WrapKeepBlanks);
    if (next <= at) {
      next = at + 1;
    }
    rows.push_back(FieldRow{.begin = at, .end = next});
    at = (next < end && *next == '\n') ? next + 1 : next;
  }
  return rows;
}

// De combien la pastille déborde du texte qu'elle recouvre : il faut effacer les
// glyphes du champ, qui montent et descendent d'un pixel au-delà du pas.
constexpr float kChipBleedPixels = 1.0F;
// L'identifiant du menu d'une pastille, et la case où sa cible attend l'image
// suivante. ⚠ DANS LE MAGASIN DE LA FENÊTRE, pour la même raison que celui du
// fil : deux chatbox détachées ont chacune sa rangée.
constexpr const char* kChipMenuId = "lien_saisie";
constexpr const char* kChipMenuTargetKey = "##cible_pastille";
// Ce qui borne le nom d'une émoticone dans le texte : `:smile:`.
constexpr char kEmoteTokenMark = ':';

}  // namespace

ChatLinkAsk PaintInputLinkChips(const ChatInputState& state, std::span<const ChatInputLink> links,
                                ImGuiID field, ImVec2 field_at, ImVec2 field_size, float wrap_width,
                                bool active, bool hovered, ItemLinkFacts link_facts) {
  ChatLinkAsk asked;
  // ⚠ D'ICI, TOUJOURS : c'est la surface qui le dit, pas `app` qui le devine.
  asked.from_input = true;
  ImGuiStorage* kept = ImGui::GetStateStorage();
  const ImGuiID target_key = ImGui::GetID(kChipMenuTargetKey);
  const char* const text = state.text.data();
  if (links.empty() || text[0] == '\0') {
    // ⚠ LE MENU SE DESSINE QUAND MÊME : fermé, il ne coûte rien, et sauter son
    // appel le jour où la saisie se vide le laisserait ouvert sur du vide.
    return DrawChatLinkMenu(kChipMenuId, ChatLinkKind::kItem, 0,
                            static_cast<std::uint32_t>(kept->GetInt(target_key, 0)), {},
                            link_facts);
  }
  const ImGuiStyle& style = ImGui::GetStyle();
  const float font_size = ImGui::GetFontSize();
  ImVec2 scrolled(0.0F, 0.0F);
  if (const ImGuiInputTextState* typing = ImGui::GetInputTextState(field); typing != nullptr) {
    scrolled.x = wrap_width <= 0.0F ? typing->Scroll.x : 0.0F;
  }
  const ImU32 behind = ImGui::GetColorU32(active    ? ImGuiCol_FrameBgActive
                                          : hovered ? ImGuiCol_FrameBgHovered
                                                    : ImGuiCol_FrameBg);
  const ImVec2 origin(field_at.x + style.FramePadding.x - scrolled.x,
                      field_at.y + style.FramePadding.y - scrolled.y);
  const ImVec2 clip_min(field_at.x + kChipBleedPixels, field_at.y + kChipBleedPixels);
  const ImVec2 clip_max(field_at.x + field_size.x - kChipBleedPixels,
                        field_at.y + field_size.y - kChipBleedPixels);
  const bool over_field = ImGui::IsMouseHoveringRect(clip_min, clip_max);

  const std::string_view whole(text);
  const std::vector<FieldRow> rows = FieldRows(text, wrap_width);
  ImDrawList* into = ImGui::GetWindowDrawList();
  into->PushClipRect(clip_min, clip_max, true);
  std::size_t from = 0;
  std::string_view menu_label;
  ChatLinkKind menu_kind = ChatLinkKind::kItem;
  std::uint32_t menu_item = 0;
  const MvpLinkMenuFacts* menu_mvp = nullptr;
  NaviSearchMenuFacts menu_navi;
  const auto wanted = static_cast<std::uint32_t>(kept->GetInt(target_key, 0));
  for (const ChatInputLink& link : links) {
    if (link.label.empty()) {
      continue;
    }
    // ⚠ MÊME BALAYAGE QUE LA SUBSTITUTION À L'ENVOI — de gauche à droite, un lien
    // consommé à la fois : deux exemplaires du même objet gardent chacun leur
    // pastille, et la première n'est pas peinte deux fois.
    const std::size_t at = whole.find(link.label, from);
    if (at == std::string_view::npos) {
      continue;
    }
    from = at + link.label.size();
    if (link.token == wanted) {
      menu_label = link.label;
      menu_kind = link.kind;
      menu_item = link.item_id;
      menu_mvp = link.mvp;
      menu_navi = link.navi;
    }
    const char* const link_begin = text + at;
    const char* const link_end = text + from;
    bool touched = false;
    for (std::size_t rank = 0; rank < rows.size(); ++rank) {
      // La part du lien qui tombe sur CETTE rangée — rien, tout, ou un morceau.
      const char* const piece_begin = (std::max)(rows[rank].begin, link_begin);
      const char* const piece_end = (std::min)(rows[rank].end, link_end);
      if (piece_begin >= piece_end) {
        continue;
      }
      const float before = ImGui::CalcTextSize(rows[rank].begin, piece_begin).x;
      const float wide = ImGui::CalcTextSize(piece_begin, piece_end).x;
      const ImVec2 where(origin.x + before, origin.y + static_cast<float>(rank) * font_size);
      const ImVec2 top_left(where.x - kChipBleedPixels, where.y - kChipBleedPixels);
      const ImVec2 bottom_right(where.x + wide + kChipBleedPixels,
                                where.y + font_size + kChipBleedPixels);
      into->AddRectFilled(top_left, bottom_right, behind);
      into->AddText(where, kChatLinkColor, piece_begin, piece_end);
      // ⚠ LE SURVOL EST À NOUS : ces pastilles sont peintes à la main par-dessus
      // un champ de texte, ce ne sont pas des éléments d'ImGui.
      if (over_field && ImGui::IsMouseHoveringRect(top_left, bottom_right)) {
        touched = true;
      }
    }
    // ⚠ UNE FOIS PAR LIEN, PAS PAR MORCEAU : un lien coupé en deux reste UN lien.
    if (!touched) {
      continue;
    }
    asked.hovered_id = link.item_id;
    asked.hovered_kind = link.kind;
    asked.hovered_token = link.token;
    asked.hovering = true;
    const ChatLinkGesture gesture = HitChatLink(true);
    if (gesture == ChatLinkGesture::kMenu) {
      kept->SetInt(target_key, static_cast<int>(link.token));
      ImGui::OpenPopup(kChipMenuId);
    } else if (gesture != ChatLinkGesture::kNothing) {
      asked.gesture = gesture;
      asked.kind = link.kind;
      asked.item_id = link.item_id;
      asked.token = link.token;
    }
  }
  into->PopClipRect();

  if (const ChatLinkAsk chosen =
          DrawChatLinkMenu(kChipMenuId, menu_kind, menu_item, wanted, menu_label, link_facts,
                           nullptr, menu_mvp, menu_navi);
      chosen.gesture != ChatLinkGesture::kNothing) {
    asked.gesture = chosen.gesture;
    asked.kind = chosen.kind;
    asked.item_id = chosen.item_id;
    asked.token = chosen.token;
  }
  return asked;
}

std::string EmoteTokenText(std::string_view token) {
  std::string text(1, kEmoteTokenMark);
  text += token;
  text += kEmoteTokenMark;
  return text;
}

bool AppendPickedToChatInput(ChatInputState& state, std::string_view piece) {
  // UN NOM `:nom:` SE RECONNAÎT À SES DEUX-POINTS : aucun emoji n'en porte.
  const bool word =
      piece.size() > 2 && piece.front() == kEmoteTokenMark && piece.back() == kEmoteTokenMark;
  if (!word) {
    return AppendPendingInsert(state, piece);
  }
  // CE QUE LA SAISIE MONTRERA AVANT LUI : le texte, puis ce qui attend déjà au
  // dépôt — c'est au bout des deux qu'il se pose.
  const std::string_view typed(state.text.data());
  const std::string_view before = state.pending_insert.empty() ? typed : state.pending_insert;
  if (before.empty() || before.back() == ' ') {
    return AppendPendingInsert(state, piece);
  }
  return AppendPendingInsert(state, " " + std::string(piece));
}

}  // namespace ro::ui
