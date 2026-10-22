// La box « Pseudo ». Voir l'en-tête.

#include "chat_whisper_box.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/core/text.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"

#include "ui_input.h"
#include "ui_scale.h"
#include "ui_strings.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// Le menu des derniers correspondants, ouvert au clic droit sur la box.
constexpr const char* kWhisperMenu = "##recents";
// La même liste, déroulée sous la box dès qu'elle a le clavier.
constexpr const char* kListWindow = "##correspondants";
// Le propriétaire des touches que la liste prend à la box.
constexpr const char* kKeysOwner = "##touches_des_correspondants";
// L'écart entre le bas de la box et la liste.
constexpr float kListGap = 2.0F;
// LA FLÈCHE DU SENS, devant chaque nom : sa longueur et sa pointe, en part de
// la taille du texte, et son trait en pixels d'interface.
constexpr float kWayMarkShare = 0.75F;
constexpr float kWayHeadShare = 0.25F;
constexpr float kWayStroke = 1.5F;
constexpr float kHalf = 0.5F;

// La touche pressée cette image, prise à tout le reste : la box la lit par
// `Shortcut`, que le verrou fait taire.
void LockKey(ImGuiKey key) {
  ImGui::SetKeyOwner(key, ImGui::GetID(kKeysOwner), ImGuiInputFlags_LockThisFrame);
}

[[nodiscard]] bool EnterPressed() {
  return ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
         ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
}

[[nodiscard]] float WayMarkWidth() { return ImGui::GetFontSize() * kWayMarkShare; }

// LA FLÈCHE : sortante (→) pour un nom à qui l'on a chuchoté en dernier,
// entrante (←) pour un nom qui nous a chuchoté en dernier. ⚠ DESSINÉE, PAS
// ÉCRITE : elle ne dépend d'aucun glyphe de la police.
void PaintWayMark(ImDrawList* draw, ImVec2 at, ChatWhisperWay way, ImU32 color) {
  const float width = WayMarkWidth();
  const float head = ImGui::GetFontSize() * kWayHeadShare;
  const float middle = at.y + ImGui::GetTextLineHeight() * kHalf;
  const float left = at.x;
  const float right = at.x + width;
  draw->AddLine(ImVec2(left, middle), ImVec2(right, middle), color, Px(kWayStroke));
  if (way == ChatWhisperWay::kOutgoing) {
    draw->AddTriangleFilled(ImVec2(right, middle), ImVec2(right - head, middle - head),
                            ImVec2(right - head, middle + head), color);
  } else {
    draw->AddTriangleFilled(ImVec2(left, middle), ImVec2(left + head, middle - head),
                            ImVec2(left + head, middle + head), color);
  }
}

// La largeur d'une rangée : la flèche, son écart, et le plus long nom.
[[nodiscard]] float RowWidth(std::span<const ChatRecentWhisper> names) {
  float widest = 0.0F;
  for (const ChatRecentWhisper& entry : names) {
    widest = std::max(widest, TextWidth(entry.name));
  }
  return WayMarkWidth() + ImGui::GetStyle().ItemSpacing.x + widest;
}

// UNE RANGÉE : la flèche du sens, puis le nom. Rend vrai au clic.
[[nodiscard]] bool WhisperRow(const ChatRecentWhisper& entry, bool chosen, float width,
                              ImGuiSelectableFlags flags) {
  const ImVec2 at = ImGui::GetCursorScreenPos();
  const bool clicked =
      ImGui::Selectable("##nom", chosen, flags, ImVec2(width, ImGui::GetTextLineHeight()));
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
  PaintWayMark(draw, at, entry.way, color);
  draw->AddText(ImVec2(at.x + WayMarkWidth() + ImGui::GetStyle().ItemSpacing.x, at.y), color,
                entry.name.data(), entry.name.data() + entry.name.size());
  if (ImGui::IsItemHovered()) {
    SkinTooltipFormat("%s", entry.way == ChatWhisperWay::kIncoming
                                ? core::Tr("Il t'a chuchoté en dernier.")
                                : core::Tr("Tu lui as chuchoté en dernier."));
  }
  return clicked;
}

// LE NOM PRIS : il remplit la box, et le clavier part à la phrase.
void TakeName(ChatInputState& state, std::string_view name, ImGuiID box) {
  // ⚠ UNE COPIE BORNÉE, ET QUI NE COUPE PAS UN CARACTÈRE : un nom plus long que
  // la box — d'un fichier, d'un serveur modifié — déborderait le tampon.
  CopyBounded(state.whisper_to, core::Utf8Prefix(name, state.whisper_to.size() - 1));
  // 🔴 ET ON PRÉVIENT LE CHAMP. Écrire dans le tampon d'un `InputText` sans le
  // lui dire laisse son curseur posé sur l'ancien texte ; s'il était plus long,
  // le curseur dépasse la fin et la première image qui redessine le champ
  // franchit une assertion d'ImGui — ce qui TUE le programme.
  ReloadInputField(box);
  // Le nom est choisi : on veut ÉCRIRE la phrase — c'est à elle que le clavier
  // va, pas à la box qu'on vient de remplir.
  state.focus_on_whisper = false;
  state.wants_focus = true;
  CloseWhisperList(state.whisper_list);
}

// AVANT LA BOX : les touches que la liste déroulée lui prend. Rend le rang,
// dans `names`, du nom qu'Entrée ou Tab vient de prendre.
//
// 🔴 PRISES AVANT LE WIDGET, ET VERROUILLÉES pour l'image : sans verrou, Échap
// fermerait la liste ET quitterait la box — et replierait la barre en battle
// mode —, Entrée prendrait le nom ET enverrait la ligne.
[[nodiscard]] std::optional<std::size_t> TakeListKeys(ChatInputState& state,
                                                      const ChatWhisperListView& view) {
  ChatWhisperListState& list = state.whisper_list;
  if (!view.open || !list.box_was_active) {
    return std::nullopt;
  }
  const std::string_view typed = state.whisper_to.data();
  const auto press = [&](ChatWhisperKey key) {
    return PressWhisperListKey(list, view, typed, key);
  };
  if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && press(ChatWhisperKey::kEscape).consumed) {
    LockKey(ImGuiKey_Escape);
    return std::nullopt;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && press(ChatWhisperKey::kDown).consumed) {
    LockKey(ImGuiKey_DownArrow);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && press(ChatWhisperKey::kUp).consumed) {
    LockKey(ImGuiKey_UpArrow);
  }
  // ⚠ ENTRÉE ET TAB NE SONT À LA LISTE QU'AVEC UN NOM SURLIGNÉ : sans lui,
  // `PressWhisperListKey` les laisse, et elles gardent leur rôle.
  if (EnterPressed()) {
    const ChatWhisperKeyAnswer answer = press(ChatWhisperKey::kEnter);
    if (answer.picked.has_value()) {
      LockKey(ImGuiKey_Enter);
      LockKey(ImGuiKey_KeypadEnter);
      return answer.picked;
    }
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
    const ChatWhisperKeyAnswer answer = press(ChatWhisperKey::kTab);
    if (answer.picked.has_value()) {
      LockKey(ImGuiKey_Tab);
      return answer.picked;
    }
  }
  return std::nullopt;
}

// LA LISTE DÉROULÉE, collée sous la box. Rend le rang, dans `names`, du nom
// cliqué.
//
// ⚠ LE CLIC SE PREND À L'ENFONCEMENT : cliquer hors de la box la désactive
// dans la même image, et la liste, qui ne se montre qu'à la box active,
// disparaîtrait avant le relâchement.
[[nodiscard]] std::optional<std::size_t> DrawDropDown(ChatInputState& state,
                                                      std::span<const ChatRecentWhisper> names,
                                                      const ChatWhisperListView& view,
                                                      ImVec2 box_corner, float box_width,
                                                      const SkinArt& art) {
  std::optional<std::size_t> picked;
  const SkinLook& look = art.look();
  const ImGuiStyle& style = ImGui::GetStyle();
  const float width = std::max(box_width, RowWidth(names) + 2.0F * style.WindowPadding.x);
  const float row_height = ImGui::GetTextLineHeightWithSpacing();
  const float height = static_cast<float>(view.rows.size()) * row_height - style.ItemSpacing.y +
                       2.0F * style.WindowPadding.y;
  ImGui::SetNextWindowPos(ImVec2(box_corner.x, box_corner.y + kListGap), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, look.popup);
  ImGui::PushStyleColor(ImGuiCol_Border, look.border);
  ImGui::PushStyleColor(ImGuiCol_Header, look.selection);
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, look.SelectionHover());
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, look.SelectionActive());
  ImGui::PushStyleColor(ImGuiCol_Text, look.text);
  constexpr int kPushedColors = 6;
  // 🔴 NI FOCUS À L'APPARITION, NI NAVIGATION : la liste se déroule pendant
  // qu'on écrit dans la box, et le clavier doit y rester.
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration |
                                      ImGuiWindowFlags_NoSavedSettings |
                                      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse;
  if (ImGui::Begin(kListWindow, nullptr, kFlags)) {
    // DEVANT LA FENÊTRE DE CONVERSATION, à chaque image : celle-ci passe au
    // premier plan dès qu'on clique dans sa box, et couvrirait la liste.
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    const float row_width = width - 2.0F * style.WindowPadding.x;
    for (std::size_t row = 0; row < view.rows.size(); ++row) {
      ImGui::PushID(static_cast<int>(row));
      if (WhisperRow(names[view.rows[row]], static_cast<int>(row) == state.whisper_list.chosen,
                     row_width, ImGuiSelectableFlags_SelectOnClick)) {
        picked = view.rows[row];
      }
      ImGui::PopID();
    }
  }
  ImGui::End();
  ImGui::PopStyleColor(kPushedColors);
  return picked;
}

}  // namespace

[[nodiscard]] WhisperBoxGesture DrawWhisperBox(ChatInputState& state,
                                               const ChatLinePreview& preview, ChatInputAsk* asked,
                                               const SkinArt& art) {
  WhisperBoxGesture gesture;
  ChatWhisperListState& list = state.whisper_list;
  const std::span<const ChatRecentWhisper> names = preview.recent_whispers;
  // La liste d'après le texte de l'image PRÉCÉDENTE : c'est elle que le joueur
  // a sous les yeux quand il presse une touche.
  const std::optional<std::size_t> keyed =
      TakeListKeys(state, FollowWhisperList(list, names, state.whisper_to.data()));

  ImGui::SetNextItemWidth(Px(kChatWhisperBoxWidth));
  // ⚠ LE CLAVIER PEUT VENIR ICI, et pas seulement à la phrase : c'est le joueur
  // qui décide où il laisse son curseur, et un envoi le lui rend là où il
  // l'avait mis.
  if (state.wants_focus && state.focus_on_whisper) {
    ImGui::SetKeyboardFocusHere();
    // Consommée seulement le geste fini — même raison que pour la phrase, plus
    // bas : `SetKeyboardFocusHere` refuse sec tant qu'un bouton est enfoncé.
    if (!ImGui::IsAnyMouseDown() && !ImGui::IsDragDropActive()) {
      state.wants_focus = false;
    }
  }
  // 🔴 ENTRÉE VAUT DEPUIS ICI AUSSI. Le client d'origine ne demande pas au
  // joueur de savoir dans laquelle des deux boîtes il a laissé son curseur :
  // Entrée envoie, ou referme si la phrase est vide, que le pseudo soit rempli
  // ou non. Sans `EnterReturnsTrue`, la touche se perdait — et en battle mode
  // la barre restait ouverte, sourde.
  gesture.submitted =
      ImGui::InputTextWithHint("##pseudo", core::Tr("Pseudo"), state.whisper_to.data(),
                               state.whisper_to.size(), ImGuiInputTextFlags_EnterReturnsTrue);
  // Relevé ICI : la liste écrit dans CE tampon, et il faudra le lui dire — le
  // widget tient sa propre copie pendant l'édition.
  const ImGuiID box = ImGui::GetItemID();
  gesture.active = ImGui::IsItemActive();
  const bool activated = ImGui::IsItemActivated();
  const ImVec2 box_corner(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y);
  const float box_width = ImGui::GetItemRectSize().x;
  // ⚠ RELEVÉ COLLÉ AU WIDGET : plus bas, « l'item » désignerait le dernier
  // dessiné — une infobulle, un menu — et le test d'Échap porterait sur autre
  // chose.
  gesture.escaped = ImGui::IsItemDeactivated() && ImGui::IsKeyPressed(ImGuiKey_Escape, false);
  // TABULATION → LA PHRASE. Elle est dessinée APRÈS, dans la même image : le
  // clavier y bascule tout de suite, sans le battement d'une image.
  if (gesture.active && ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
    state.focus_on_whisper = false;
    state.wants_focus = true;
  }
  // 🔴 LA LISTE SE DÉROULE À LA PRISE DU CLAVIER, quel que soit le chemin : un
  // clic, Tab, ou le clavier que la barre rend à la box.
  if (activated) {
    OpenWhisperList(list, state.whisper_to.data());
  }

  // 🔴 CLIC DROIT = LES RÉCENTS, et c'est ce que fait le bouton « Select
  // Receiver » du client avec sa liste. Le geste est le même que sur une
  // languette, et il ne coûte pas un pixel d'une rangée déjà serrée.
  const bool right_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
  if (right_clicked) {
    ImGui::OpenPopup(kWhisperMenu);
  }
  const bool menu_open = ImGui::IsPopupOpen(kWhisperMenu);
  // ⚠ PAS PENDANT QU'UNE LISTE EST DÉROULÉE : elle se tient juste sous la box,
  // et l'infobulle viendrait recouvrir les noms qu'on cherche à lire.
  if (ImGui::IsItemHovered() && !menu_open && (!gesture.active || names.empty())) {
    SkinTooltipFormat("%s\n%s",
                      core::Tr("À qui chuchoter. Rempli, tout ce que tu écris lui part "
                               "— sauf ce qui porte déjà un sigil."),
                      core::Tr("Tes derniers correspondants se déroulent dessous dès que la "
                               "case a le clavier : flèches pour choisir, Entrée ou Tab pour "
                               "prendre, Échap pour fermer. Clic droit : la même liste."));
  }

  std::optional<std::size_t> picked = keyed;
  // LA LISTE DÉROULÉE, sur le texte de la FIN de l'image : elle suit la frappe
  // sans une image de retard. ⚠ ELLE SE MONTRE AUSSI À L'IMAGE OÙ LA BOX VIENT
  // DE PERDRE LE CLAVIER, et c'est ce qui laisse un clic la toucher.
  const ChatWhisperListView view = FollowWhisperList(list, names, state.whisper_to.data());
  if (!picked.has_value() && view.open && (gesture.active || list.box_was_active) && !menu_open &&
      !right_clicked) {
    picked = DrawDropDown(state, names, view, box_corner, box_width, art);
  }

  if (BeginSkinPopup(kWhisperMenu)) {
    if (names.empty()) {
      // ⚠ UN MOT PLUTÔT QU'UNE LISTE VIDE : un menu qui s'ouvre sur rien se lit
      // comme un défaut, alors que c'est simplement que personne n'a chuchoté.
      ImGui::TextUnformatted(core::Tr("Aucun destinataire récent"));
    } else {
      const float row_width = RowWidth(names);
      for (std::size_t rank = 0; rank < names.size(); ++rank) {
        ImGui::PushID(static_cast<int>(rank));
        if (WhisperRow(names[rank], false, row_width, ImGuiSelectableFlags_None)) {
          picked = rank;
        }
        ImGui::PopID();
      }
      ImGui::Separator();
      if (ImGui::Selectable(core::Tr("Vider la liste"))) {
        asked->forget_whispers = true;
      }
    }
    EndSkinPopup();
  }

  if (picked.has_value() && *picked < names.size()) {
    TakeName(state, names[*picked].name, box);
  } else if (!gesture.active) {
    // La box a perdu le clavier : clic ailleurs, Tab, envoi. La liste se replie.
    CloseWhisperList(list);
  }
  list.box_was_active = gesture.active;
  return gesture;
}

}  // namespace ro::ui
