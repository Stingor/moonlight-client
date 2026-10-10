#include "ro/ui/whisper_window.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/ui/chat_frame.h"
#include "ro/ui/skinned_window.h"

#include "chat_input_chips.h"

namespace ro::ui {
namespace {

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

// LE BORD SAUTE PAR PAS, entre ses deux bornes : c'est la conduite de la
// fenêtre du client, et c'est ce qui garde le fil aligné sur ses rangées.
void SnapToSteps(ImGuiSizeCallbackData* data) {
  const float scale = *static_cast<const float*>(data->UserData);
  const auto snapped = [scale](float desired, float least, float most) {
    const float step = kWhisperSizeStepPixels * scale;
    const float steps = std::round((desired - least * scale) / step);
    return std::clamp(least * scale + steps * step, least * scale, most * scale);
  };
  data->DesiredSize.x =
      snapped(data->DesiredSize.x, kWhisperFirstWidthPixels, kWhisperMostWidthPixels);
  data->DesiredSize.y =
      snapped(data->DesiredSize.y, kWhisperFirstHeightPixels, kWhisperMostHeightPixels);
}

// UN NOM D'AUTRE JOUEUR DANS UN TITRE, sans que ses « ## » deviennent des
// identifiants d'ImGui : ils couperaient le titre.
[[nodiscard]] std::string Unhashed(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char letter : text) {
    if (letter == '#' && !out.empty() && out.back() == '#') {
      out.push_back(' ');
    }
    out.push_back(letter);
  }
  return out;
}

// La saisie, jusqu'à son zéro.
[[nodiscard]] std::string_view Typed(const std::array<char, kChatInputSize>& buffer) {
  const auto end = std::ranges::find(buffer, '\0');
  return std::string_view(buffer.data(), static_cast<std::size_t>(end - buffer.begin()));
}

// Ce que le dépôt a laissé — un emoji, un `:nom:`, un lien posé — écrit au
// bout du texte. Rien n'est tronqué : la moitié d'un emoji s'afficherait en
// losanges. Rend vrai quand le texte a changé.
[[nodiscard]] bool TakePendingInsert(ChatInputState& input) {
  const std::size_t typed_bytes = std::strlen(input.text.data());
  const bool fits = !input.pending_insert.empty() &&
                    typed_bytes + input.pending_insert.size() + 1 <= input.text.size();
  if (fits) {
    std::memcpy(input.text.data() + typed_bytes, input.pending_insert.data(),
                input.pending_insert.size() + 1);
  }
  input.pending_insert.clear();
  return fits;
}

// 🔴 ÉCRIRE DANS LE TAMPON D'UN CHAMP SANS LE LUI DIRE laisse son curseur posé
// sur l'ancien texte ; et le champ actif tient sa propre copie, qui écraserait
// la nôtre à l'image suivante.
void ReloadField(ImGuiID field) {
  if (ImGuiInputTextState* edited = ImGui::GetInputTextState(field)) {
    edited->ReloadUserBufAndMoveToEnd();
  }
}

// CE QUE LA PALETTE A RENDU : l'emoji et le `:nom:` s'écrivent dans la saisie,
// et le clavier revient au champ quand elle se referme.
//
// 🔴 RIEN NE PART COMME ÉMOTION DU JEU : celle-ci se joue au-dessus de la tête,
// sur la carte, et seuls les voisins la verraient — jamais le correspondant.
// La palette de la 1:1 écrit donc le `:nom:` (voir `ChatEmoteClick::kWrite`).
void TakePicked(WhisperWindowState& state, const ChatStripAction& picked) {
  if (!picked.chose_emoji.empty()) {
    static_cast<void>(AppendPickedToChatInput(state.input, picked.chose_emoji));
  }
  if (!picked.chose_emote_token.empty()) {
    static_cast<void>(
        AppendPickedToChatInput(state.input, EmoteTokenText(picked.chose_emote_token)));
  }
  if (picked.give_back_keyboard) {
    state.wants_focus = true;
  }
}

// LA RANGÉE DE SAISIE : le champ, ses pastilles de lien, et la palette à droite.
//
// ⚠ LA MÊME ESPÈCE DE SAISIE QUE LA BARRE DE LA CONVERSATION : un lien et un
// emoji s'y posent par le même dépôt, et ses pastilles se peignent et se
// cliquent de la même façon.
void DrawInputRow(WhisperWindowState& state, const WhisperWindowView& view,
                  WhisperWindowAction& action) {
  const bool inserted = TakePendingInsert(state.input);
  if (state.wants_focus) {
    ImGui::SetKeyboardFocusHere();
    // ⚠ CONSOMMÉE SEULEMENT LE GESTE TERMINÉ, comme dans la barre : la demande
    // arrive souvent en plein clic — une émoticone, une pastille, un Maj+clic
    // venu du sac — et `SetKeyboardFocusHere` refuse sec tant qu'un bouton est
    // enfoncé. L'effacer tout de suite laisserait le champ sans clavier.
    if (!ImGui::IsAnyMouseDown() && !ImGui::IsDragDropActive()) {
      state.wants_focus = false;
    }
  }
  // La palette prend un carré de la hauteur du champ, à sa droite ; sans
  // émoticones, elle ne paraît pas et le champ prend tout.
  const float side = ImGui::GetFrameHeight();
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const bool picker = !view.emotions.empty();
  const float field_width =
      std::max(1.0F, ImGui::GetContentRegionAvail().x - (picker ? side + spacing : 0.0F));
  ImGui::SetNextItemWidth(field_width);
  const bool submitted =
      ImGui::InputText("##whisper_input", state.input.text.data(), state.input.text.size(),
                       ImGuiInputTextFlags_EnterReturnsTrue);
  // ⚠ RELEVÉS COLLÉS AU CHAMP : plus bas, « l'item » désignerait la palette.
  const ImGuiID field = ImGui::GetItemID();
  const bool active = ImGui::IsItemActive();
  const bool hovered = ImGui::IsItemHovered();
  const ImVec2 field_at = ImGui::GetItemRectMin();
  const ImVec2 field_size = ImGui::GetItemRectSize();
  action.input_active = active;
  state.input.was_active = active;
  action.input_link = PaintInputLinkChips(state.input, view.input_links, field, field_at,
                                          field_size, 0.0F, active, hovered, view.vote_shop);
  // ⚠ DE QUELLE SAISIE : chaque conversation a ses propres liens posés.
  action.input_link.input_peer = view.peer;
  if (inserted) {
    ReloadField(field);
  }
  if (submitted) {
    const std::string_view typed = Typed(state.input.text);
    if (!typed.empty()) {
      action.said = std::string(typed);
    }
    state.input.text.fill('\0');
    // ⚠ ENTRÉE REND LE CHAMP : on chuchote rarement une seule phrase.
    state.wants_focus = true;
  }
  if (picker) {
    ChatStripAction picked;
    DrawChatEmotePicker(state.picker, ImVec2(field_at.x + field_width + spacing, field_at.y), side,
                        kChatTabColor, view.emotions, view.emojis, &picked, ChatEmoteClick::kWrite);
    TakePicked(state, picked);
  }
}

}  // namespace

std::string WhisperWindowId(std::string_view peer) { return "###whisper_" + std::string(peer); }

WhisperWindowAction DrawWhisperWindow(WhisperWindowState& state, const WhisperWindowView& view,
                                      const SkinArt& art) {
  WhisperWindowAction action;
  // ÉCHAP A BAISSÉ L'INTERRUPTEUR : c'est une fermeture, comme la croix.
  if (!state.open) {
    action.closed = true;
    return action;
  }
  // ⚠ LUE PAR LE RAPPEL DE TAILLE, qui tourne PENDANT `Begin` : une variable
  // locale vit assez longtemps.
  float scale = Scale();
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  const float cascade = kWhisperCascadePixels * scale * static_cast<float>(view.cascade);
  ImGui::SetNextWindowPos(ImVec2(screen->GetCenter().x + cascade, screen->GetCenter().y + cascade),
                          ImGuiCond_FirstUseEver, ImVec2(0.5F, 0.5F));
  ImGui::SetNextWindowSize(
      ImVec2(kWhisperFirstWidthPixels * scale, kWhisperFirstHeightPixels * scale),
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(kWhisperFirstWidthPixels * scale, kWhisperFirstHeightPixels * scale),
      ImVec2(kWhisperMostWidthPixels * scale, kWhisperMostHeightPixels * scale), SnapToSteps,
      &scale);
  if (state.raise) {
    ImGui::SetNextWindowFocus();
    state.wants_focus = true;
    state.raise = false;
  }

  // Le titre change — la guilde arrive, le non-lu s'efface —, l'identité
  // jamais : la place de la fenêtre est celle de ce correspondant.
  const std::string name = Unhashed(view.title) + WhisperWindowId(view.peer);
  bool open = true;
  const bool body =
      BeginSkinnedWindow(name.c_str(), &open, art, SkinnedTitle::kBar, ImGuiWindowFlags_NoCollapse);
  if (body && open) {
    action.focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const float log_height =
        std::max(ImGui::GetFrameHeight(),
                 ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeight() - spacing);

    // ── LE FIL : le moteur de la conversation, sur son fond sombre ─────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kChatBodyColor);
    action.link = DrawChatLog(view.lines, view.log_style, log_height, art, view.layout).link;
    ImGui::PopStyleColor();
    // LE PSEUDO DU CORRESPONDANT RAMÈNE LE CLAVIER ICI : c'est déjà à lui
    // qu'on écrit.
    if (action.link.kind == ChatLinkKind::kPlayer &&
        action.link.gesture == ChatLinkGesture::kDescribe) {
      state.wants_focus = true;
      action.link.gesture = ChatLinkGesture::kNothing;
    }

    // ── SA SAISIE, à cible figée ────────────────────────────────────────────
    ImGui::BeginDisabled(!view.can_send);
    DrawInputRow(state, view, action);
    ImGui::EndDisabled();
  }
  EndSkinnedWindow();
  if (!open) {
    action.closed = true;
  }
  return action;
}

}  // namespace ro::ui
