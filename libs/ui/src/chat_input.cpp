#include "ro/ui/chat_input.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/core/text.h"
#include "ro/ui/chat_frame.h"
#include "ro/ui/chat_strip.h"
#include "ro/ui/list_keys.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "chat_input_chips.h"
#include "chat_input_commands.h"
#include "chat_whisper_box.h"
#include "ui_input.h"
#include "ui_strings.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// L'identité du champ. ⚠ LA MÊME POUR LA LIGNE UNIQUE ET POUR LE RECTANGLE :
// l'état d'édition, le focus et le rechargement du tampon ne doivent pas voir la
// différence quand le champ gagne sa deuxième ligne.
constexpr const char* kFieldId = "##saisie";
// Ce qui sépare le champ du bord droit du cadre.
constexpr float kFieldRightGap = 4.0F;
// Le plus long libellé que la boîte de destination sache montrer en entier.
// Au-delà — un pseudo à rallonge — la liste déroulante le découpe elle-même.
constexpr std::size_t kTargetShownMax = 64;
// Les trois couleurs de fond du champ — au repos, survolé, actif — qu'un mode
// armé remplace par le fond de sa teinte.
constexpr int kFieldBackdropColors = 3;

// CE QUE LE RAPPEL D'IMGUI RAMÈNE DE L'INTÉRIEUR DU CHAMP. Il ne transporte
// qu'un pointeur, et ces trois nombres ne vivent que le temps de l'appel : la
// rangée où se trouve le curseur, combien il y en a, et sur quelle largeur
// elles se replient.
struct CursorProbe {
  float wrap_width = 1.0F;
  int row = 0;
  int rows = 1;
};

// CE QUE LE RAPPEL DU CHAMP MULTILIGNE TRANSPORTE : la sonde du curseur, et la
// liste des commandes pour Tab.
struct WrappedFieldHooks {
  CursorProbe probe;
  const CommandFieldHooks* commands = nullptr;
};

// COMBIEN DE RANGÉES CE TEXTE OCCUPE, et sur laquelle tombe le curseur.
//
// ⚠ LE DÉCOUPAGE EST CELUI D'IMGUI, À LA FONCTION PRÈS. `InputTextEx` construit
// son index de lignes avec `ImFontCalcWordWrapPositionEx(…, WrapKeepBlanks)` et
// saute un saut de ligne après chaque coupe. Le reproduire plutôt que
// l'approcher : une coupure de différence, et la hauteur réservée n'a plus le
// même nombre de lignes que le widget — une rangée déborde, ou reste vide.
[[nodiscard]] int WrapRows(const char* text, float wrap_width, int cursor, int* cursor_row) {
  ImFont* font = ImGui::GetFont();
  const float font_size = ImGui::GetFontSize();
  const char* const end = text + std::strlen(text);
  int rows = 0;
  if (cursor_row != nullptr) {
    *cursor_row = 0;
  }
  for (const char* at = text; at < end;) {
    // La rangée qui CONTIENT l'octet du curseur — la dernière dont le début ne
    // le dépasse pas. Un curseur en fin de texte tombe donc sur la dernière.
    if (cursor_row != nullptr && cursor >= static_cast<int>(at - text)) {
      *cursor_row = rows;
    }
    ++rows;
    const char* next = ImFontCalcWordWrapPositionEx(font, font_size, at, end, wrap_width,
                                                    ImDrawTextFlags_WrapKeepBlanks);
    if (next <= at) {
      next = at + 1;  // garde-fou : ne jamais rester sur place
    }
    at = (next < end && *next == '\n') ? next + 1 : next;
  }
  return std::max(rows, 1);  // un texte vide fait une rangée, comme chez ImGui
}

// La largeur du CHAMP, une fois l'étiquette de destination servie.
// Ce que la box « Pseudo » prend, son écart compris. ⚠ ELLE EST SEULE : les
// récents s'ouvrent au CLIC DROIT, sans bouton à côté.
[[nodiscard]] float WhisperBoxWidth() {
  const ImGuiStyle& style = ImGui::GetStyle();
  return kChatWhisperBoxWidth * style.FontScaleMain + style.ItemSpacing.x;
}

// CE QUE LE CHAMP AURA, PRÉDIT AVANT QUE LA RANGÉE EXISTE.
//
// 🔴 À LA MESURE SEULE. La hauteur de la rangée se réserve AVANT de
// dessiner quoi que ce soit, donc sur la largeur entière : il faut alors
// retrancher soi-même ce que les deux boîtes prendront. Le DESSIN, lui, lit ce
// qui reste vraiment — y rappeler cette fonction retrancherait tout deux fois.
//
// Les deux résultats coïncident par construction : ce qu'on retranche ici est
// exactement ce que les boîtes consomment.
[[nodiscard]] float FieldWidthFor(float available, float target_width) {
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  return std::max(1.0F, available - (target_width + spacing) - WhisperBoxWidth() - kFieldRightGap);
}

// La largeur sur laquelle le texte se replie DANS le champ.
//
// ⚠ PAS LA LARGEUR DU CHAMP : `InputTextEx` mesure dans son enfant après un
// décalage de `FramePadding.x`. Et rien à retrancher pour une barre de
// défilement — on la met à zéro le temps du widget (voir plus bas).
[[nodiscard]] float WrapWidthFor(float field_width) {
  return std::max(1.0F, field_width - ImGui::GetStyle().FramePadding.x);
}

}  // namespace

void RememberTypedLine(ChatInputHistory& history, std::string_view line) {
  if (line.empty()) {
    return;
  }
  if (history.lines.empty() || history.lines.back() != line) {
    history.lines.emplace_back(line);
  }
  const int kept = TypedHistoryKept(history.kept);
  while (static_cast<int>(history.lines.size()) > kept) {
    history.lines.erase(history.lines.begin());
  }
  history.at = static_cast<int>(history.lines.size());
  history.draft.clear();
}

namespace {

// Remonte ou redescend le fil. Rend faux quand il n'y a rien à rappeler.
[[nodiscard]] bool Recall(ChatInputHistory& history, int direction, const char* current,
                          std::string* into) {
  const int count = static_cast<int>(history.lines.size());
  if (count == 0) {
    return false;
  }
  if (direction < 0) {  // vers le passé
    if (history.at == count) {
      history.draft = current;  // la phrase commencée, mise de côté
    }
    if (history.at > 0) {
      --history.at;
    }
  } else {  // vers le présent
    if (history.at >= count) {
      return false;
    }
    ++history.at;
  }
  *into = history.at >= count ? history.draft : history.lines[static_cast<std::size_t>(history.at)];
  return true;
}

// Recharge un champ qu'on vient de VIDER, curseur à zéro.
//
// 🔴 `ReloadUserBufAndMoveToEnd` NE CONVIENT PAS ICI, et c'est un plantage
// payé en jeu. Elle pose le curseur à `INT_MAX` en comptant sur un bornage
// fait plus loin dans `InputTextEx` — mais ce bornage vit dans la branche que
// prend un champ ACTIF, et un champ qui vient de rendre sa ligne sur Entrée ne
// l'est plus : `EnterReturnsTrue` le désactive. Le curseur restait donc à
// `INT_MAX`, et la première image qui redessinait le champ franchissait :
//
//   text_end_display >= text_begin && text_end_display <= text_end
//
// c'est-à-dire un curseur au-delà de la fin du texte. Une assertion franchie
// TUE le programme.
//
// ⚠ ET ZÉRO EST LA BONNE RÉPONSE, pas un contournement : le tampon est vide, il
// n'y a pas d'autre place où mettre un curseur.
void ReloadEmptiedField(ImGuiID field) {
  if (ImGuiInputTextState* state = ImGui::GetInputTextState(field)) {
    state->WantReloadUserBuf = true;
    state->ReloadSelectionStart = 0;
    state->ReloadSelectionEnd = 0;
  }
}

// LA MARQUE DISCORD : un disque bleu à droite de la boîte, en part de la taille
// du texte.
constexpr float kDiscordMarkShare = 0.5F;
constexpr float kHalf = 0.5F;

// Un mode au moins part aussi sur Discord, ou la ligne en cours y part : la
// boîte réserve la place de la marque.
[[nodiscard]] bool AnyRelayed(const ChatLinePreview& preview) {
  return preview.relayed || std::ranges::any_of(preview.modes, &ChatSendMode::relayed);
}

// LA PLACE DE LA MARQUE, son écart compris.
//
// ⚠ RÉSERVÉE DÈS QU'UN MODE PEUT ÊTRE RELAYÉ, que la ligne en cours y parte ou
// non : la boîte garde ainsi la même largeur d'un sigil à l'autre, et le champ
// ne se dérobe pas sous le curseur — même règle que pour le plus long libellé.
[[nodiscard]] float DiscordMarkRoom() {
  return ImGui::GetFontSize() * kDiscordMarkShare + ImGui::GetStyle().ItemSpacing.x;
}

void PaintDiscordMark(ImVec2 at, float room, float height) {
  const float radius = ImGui::GetFontSize() * kDiscordMarkShare * kHalf;
  const float gap = room - 2.0F * radius;
  ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(at.x + gap + radius, at.y + height * kHalf),
                                              radius, kDiscordMarkColor);
}

// Une infobulle en deux parties, la seconde facultative. Son fond et son texte
// sont ceux du thème : voir `LendSkinArt`.
void TwoPartTooltip(std::string_view first, std::string_view second) {
  if (first.empty() || second.empty()) {
    const std::string_view only = first.empty() ? second : first;
    SkinTooltipFormat("%.*s", static_cast<int>(only.size()), only.data());
  } else {
    SkinTooltipFormat("%.*s\n\n%.*s", static_cast<int>(first.size()), first.data(),
                      static_cast<int>(second.size()), second.data());
  }
}

// L'ÉTIQUETTE DE DESTINATION, qui est aussi le SÉLECTEUR DE MODE.
//
// Elle MONTRE où la ligne en cours partira — ce que les sigils tapés décident —
// et son clic ARME une destination pour les lignes qui n'en portent pas. Les
// deux chemins existent dans le client, et ils ne se contredisent pas : ce
// qu'on tape l'emporte, le temps d'une ligne.
//
// ⚠ UN SEUL WIDGET POUR LES DEUX, et c'est délibéré : un sélecteur à côté d'une
// étiquette qui le contredit — « Groupe » armé, « Alentour » affiché parce
// qu'aucun sigil n'est tapé — se lirait comme un défaut.
//
// ⚠ C'EST LA LISTE DÉROULÉE DU SKIN, celle de tout le client : une boîte qui
// s'ouvre doit porter la flèche qui le dit. Sans elle, rien à l'écran ne
// distingue la destination d'une étiquette morte.
void DrawTarget(const ChatLinePreview& preview, float width, int* chose_mode, const SkinArt& art) {
  // Le libellé arrive en VUE, et la liste veut une chaîne terminée.
  std::array<char, kTargetShownMax + 1> shown{};
  // ⚠ COUPÉ ENTRE DEUX CARACTÈRES, jamais au milieu d'un : voir
  // `core::WithoutDanglingUtf8`.
  const std::size_t kept =
      core::WithoutDanglingUtf8(
          preview.target.substr(0, std::min(preview.target.size(), shown.size() - 1)))
          .size();
  if (kept > 0) {
    std::memcpy(shown.data(), preview.target.data(), kept);
  }

  const float mark_room = AnyRelayed(preview) ? DiscordMarkRoom() : 0.0F;
  ImGui::SetNextItemWidth(std::max(1.0F, width - mark_room));
  int step = 0;
  const bool open = BeginSkinCombo("##destination", shown.data(), art, &step);
  // Relevé COLLÉ au widget : plus bas, « l'item » désignerait le dernier dessiné,
  // et l'infobulle se poserait sur autre chose.
  const bool hovered = ImGui::IsItemHovered();
  // ⚠ LA DESTINATION RÉSOLUE, PAS LE MODE ARMÉ : voir `ChatLinePreview::relayed`.
  const bool relayed = preview.relayed;
  if (const int to = SkinComboStepped(preview.mode, step, static_cast<int>(preview.modes.size()));
      to != preview.mode) {
    *chose_mode = to;
  }
  if (open) {
    ComboKeys keys(preview.mode, static_cast<int>(preview.modes.size()));
    for (int rank = 0; rank < static_cast<int>(preview.modes.size()); ++rank) {
      const ChatSendMode& mode = preview.modes[static_cast<std::size_t>(rank)];
      // Chacun dans SA couleur : c'est elle qu'on verra ensuite sur le texte, et
      // l'apprendre ici évite d'avoir à la deviner.
      // ⚠ SUR LE FOND OÙ ELLE SE LIRA : la rangée est peinte avant l'entrée, que
      // le survol recouvre encore.
      if (mode.backdrop != 0) {
        const ImVec2 at = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            at, ImVec2(at.x + ImGui::GetContentRegionAvail().x, at.y + ImGui::GetTextLineHeight()),
            mode.backdrop);
      }
      ImGui::PushStyleColor(ImGuiCol_Text, mode.color != 0 ? mode.color : kChatDarkTextColor);
      // CE QUI PART AUSSI SUR DISCORD LE DIT DANS LA LISTE, avant qu'on l'arme.
      std::string label(mode.label);
      if (mode.relayed) {
        label += " · ";
        label += core::Tr("Discord");
      }
      if (keys.Took(rank, ImGui::Selectable(label.c_str(), rank == preview.mode))) {
        *chose_mode = rank;
      }
      ImGui::PopStyleColor();
    }
    EndSkinCombo();
  }
  // LA MARQUE DISCORD, dans la place réservée à droite de la boîte : vide quand
  // la ligne en cours ne part pas là-bas, pour que rien ne bouge d'une ligne à
  // l'autre.
  bool mark_hovered = false;
  if (mark_room > 0.0F) {
    ImGui::SameLine(0.0F, 0.0F);
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float height = ImGui::GetFrameHeight();
    ImGui::Dummy(ImVec2(mark_room, height));
    mark_hovered = relayed && ImGui::IsItemHovered();
    if (relayed) {
      PaintDiscordMark(at, mark_room, height);
    }
  }
  // Pas pendant que la liste est déroulée : elle recouvre déjà ce que l'aide
  // expliquerait. ⚠ LE RELAI D'ABORD : c'est ce qu'on ignore le plus volontiers,
  // et ce qui coûte le plus d'ignorer.
  const std::string_view tip = relayed ? preview.relay_tip : std::string_view();
  if (mark_hovered && !tip.empty()) {
    TwoPartTooltip(tip, {});
  } else if (hovered && !open && (!tip.empty() || !preview.help.empty())) {
    TwoPartTooltip(tip, preview.help);
  }
}

// La place restante, calée à droite contre le champ, sur sa DERNIÈRE rangée —
// donc sous l'étiquette de destination, qui n'occupe que la première.
void DrawBudget(const ChatLinePreview& preview, ImVec2 field_at, float field_height) {
  std::array<char, 48> label{};
  if (preview.bytes_left >= 0) {
    std::snprintf(label.data(), label.size(), core::Tr("reste %d"), preview.bytes_left);
  } else {
    std::snprintf(label.data(), label.size(), core::Tr("%d de trop"), -preview.bytes_left);
  }
  const ImGuiStyle& style = ImGui::GetStyle();
  const ImVec2 size = ImGui::CalcTextSize(label.data());
  const ImVec2 at(field_at.x - style.ItemSpacing.x - size.x,
                  field_at.y + field_height - style.FramePadding.y - size.y);
  ImGui::GetWindowDrawList()->AddText(
      at, preview.bytes_left >= 0 ? kChatBudgetColor : kChatBudgetOverColor, label.data());
}

}  // namespace

float ChatTargetBoxWidth(const ChatLinePreview& preview, const SkinArt& art) {
  float widest = kChatTargetLeastText * ImGui::GetStyle().FontScaleMain;
  for (const ChatSendMode& mode : preview.modes) {
    if (mode.label.empty()) {
      continue;
    }
    widest = std::max(widest, TextWidth(mode.label));
  }
  return widest + SkinComboChromeWidth(art) + (AnyRelayed(preview) ? DiscordMarkRoom() : 0.0F);
}

float ChatInputHeight(ChatInputState& state, const ChatLinePreview& preview, float most,
                      const SkinArt& art) {
  const float one = ImGui::GetFrameHeight();
  state.measured_frame = ImGui::GetFrameCount();
  state.rows_wanted = 1;
  state.rows_allowed = 1;
  state.rows_drawn = 1;
  if (!state.grow) {
    return one;
  }
  const float line = ImGui::GetFontSize();
  // ⚠ LA MÊME LARGEUR QUE LE DESSIN, étiquette de destination comprise. Mesurer
  // sur une largeur et dessiner sur une autre, c'est une rangée de différence —
  // donc un champ qui déborde de la place qu'on lui a réservée.
  const float field =
      FieldWidthFor(ImGui::GetContentRegionAvail().x, ChatTargetBoxWidth(preview, art));
  state.rows_wanted = WrapRows(state.text.data(), WrapWidthFor(field), 0, nullptr);
  // Le plafond, c'est ce que l'appelant peut céder ; au-delà le champ défile en
  // interne. Jamais moins d'une rangée.
  state.rows_allowed = std::max(1, 1 + static_cast<int>(std::floor((most - one) / line)));
  state.rows_drawn = std::clamp(state.rows_wanted, 1, state.rows_allowed);
  return one + static_cast<float>(state.rows_drawn - 1) * line;
}

bool AppendPendingInsert(ChatInputState& state, std::string_view piece) {
  const std::size_t typed_bytes = std::strlen(state.text.data());
  // Le zéro terminal compte : c'est la règle de pose de `DrawChatInput`.
  if (typed_bytes + state.pending_insert.size() + piece.size() + 1 > state.text.size()) {
    return false;
  }
  state.pending_insert += piece;
  return true;
}

std::optional<std::string> DrawChatInput(ChatInputState& state, const ChatLinePreview& preview,
                                         ChatInputAsk* asked, const SkinArt& art,
                                         std::span<const ChatInputLink> links,
                                         ItemLinkFacts link_facts) {
  // ⚠ UN APPELANT QUI N'ÉCOUTE PAS REÇOIT UNE RÉPONSE QU'ON JETTE : la boîte des
  // chuchotements et la destination écrivent dedans sans tester, et le reste
  // de la fonction testait. Un seul point de garde vaut mieux que deux règles.
  ChatInputAsk ignored;
  if (asked == nullptr) {
    asked = &ignored;
  }
  std::optional<std::string> said;
  const float row_height = ImGui::GetFrameHeight();

  // Ce que la bande a déposé : un emoji pioché dans le sélecteur.
  // ⚠ RIEN N'EST TRONQUÉ : poser la moitié d'un emoji laisserait des octets qui
  // ne forment plus un caractère, et le champ les rendrait en losanges.
  // ⚠ UNE COMMANDE COMPLÉTÉE À LA SOURIS SE RECHARGE COMME LUI : voir
  // `DrawCommandsOverField`.
  const bool completed = std::exchange(state.commands.reload_field, false);
  const bool appended =
      !state.pending_insert.empty() &&
      std::strlen(state.text.data()) + state.pending_insert.size() + 1 <= state.text.size();
  const bool inserted = appended || completed;
  if (appended) {
    std::memcpy(state.text.data() + std::strlen(state.text.data()), state.pending_insert.data(),
                state.pending_insert.size() + 1);
  }
  state.pending_insert.clear();
  const float target_width = ChatTargetBoxWidth(preview, art);
  // ⚠ L'ORDRE EST CELUI DU CLIENT : à QUI d'abord, OÙ ensuite, QUOI enfin. Le
  // destinataire est le plus précis des trois chemins — un nom désigne quelqu'un,
  // un mode désigne un groupe — et c'est celui qu'on relit avant d'envoyer.
  ImGui::PushStyleColor(ImGuiCol_Text, kChatDarkTextColor);
  const WhisperBoxGesture whisper = DrawWhisperBox(state, preview, asked);
  ImGui::PopStyleColor();
  ImGui::SameLine();
  DrawTarget(preview, target_width, &asked->chose_mode, art);
  ImGui::SameLine();

  // ⚠ LE TEXTE REPASSE EN SOMBRE LE TEMPS DU CHAMP : celui-ci est CLAIR, comme
  // les champs du chat natif, et le cadre pousse un texte clair pour son fond
  // sombre. Clair sur clair, on ne lirait rien.
  //
  // ⚠ SAUF QUAND UN MODE EST ARMÉ : le texte prend alors SA couleur, comme dans
  // le client. Parler à sa guilde en croyant parler à voix haute est la faute
  // qu'on ne fait qu'une fois, et c'est cette teinte qui l'évite.
  //
  // ⚠ ET LE CHAMP PREND ALORS LE FOND OÙ CETTE TEINTE SE LIT : celles du fil sont
  // claires, et sur le gris du champ elles disparaîtraient.
  const bool known_mode =
      preview.mode >= 0 && preview.mode < static_cast<int>(preview.modes.size());
  const ChatSendMode armed_mode =
      known_mode ? preview.modes[static_cast<std::size_t>(preview.mode)] : ChatSendMode{};
  const ImU32 armed = armed_mode.color;
  const int field_colors = armed != 0 && armed_mode.backdrop != 0 ? kFieldBackdropColors : 0;
  if (field_colors != 0) {
    ImGui::PushStyleColor(ImGuiCol_FrameBg, armed_mode.backdrop);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, armed_mode.backdrop);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, armed_mode.backdrop);
  }
  ImGui::PushStyleColor(ImGuiCol_Text, armed != 0 ? armed : kChatDarkTextColor);
  // 🔴 CE QUI RESTE DE LA LIGNE, ET RIEN D'AUTRE. `GetContentRegionAvail`
  // est lue APRÈS les deux boîtes : elle a déjà déduit ce qu'elles prennent. En
  // repasser par `FieldWidthFor` le retranchait une SECONDE fois, et le champ
  // tombait à une poignée de pixels — huit caractères par ligne, trois rangées
  // pour un mot. Cette fonction-là sert à PRÉDIRE la largeur avant que la rangée
  // existe, et elle n'a qu'un appelant : `ChatInputHeight`.
  const float field_width = std::max(1.0F, ImGui::GetContentRegionAvail().x - kFieldRightGap);
  ImGui::SetNextItemWidth(field_width);
  // Relevée AVANT la soumission : au retour, le curseur aura avancé.
  const ImVec2 field_at = ImGui::GetCursorScreenPos();
  // Le nombre de rangées est celui que la mise en page a RÉSERVÉ cette image.
  // Le remesurer ici, sur un texte qui a pu changer, ferait déborder la rangée.
  const int rows = state.measured_frame == ImGui::GetFrameCount() ? state.rows_drawn : 1;
  const float field_height = row_height + static_cast<float>(rows - 1) * ImGui::GetFontSize();
  const float wrap_width = state.grow ? WrapWidthFor(field_width) : 0.0F;

  // ⚠ ET SEULEMENT SI C'EST ICI QUE LE CLAVIER DOIT ALLER : la box « Pseudo »
  // a déjà pris la demande pour elle si le curseur y était.
  if (state.wants_focus && !state.focus_on_whisper) {
    ImGui::SetKeyboardFocusHere();
    // ⚠ LA DEMANDE N'EST CONSOMMÉE QU'UNE FOIS LE GESTE TERMINÉ.
    // `SetKeyboardFocusHere` refuse SEC tant qu'un glisser est en cours ou qu'une
    // fenêtre est déplacée. Or la demande est parfois posée EN PLEIN GESTE — un
    // Maj + clic qui pose un lien part d'une cellule d'inventaire, laquelle est
    // une source de glisser. L'effacer dans la foulée, c'est un champ qui n'a
    // jamais le focus et une PREMIÈRE Entrée qui sert à l'ouvrir au lieu
    // d'envoyer. La re-poser ne peut pas boucler : le relâchement arrive toujours.
    if (!ImGui::IsAnyMouseDown() && !ImGui::IsDragDropActive()) {
      state.wants_focus = false;
    }
  }

  // LA LISTE DES COMMANDES prend ses touches AVANT le champ : voir
  // `chat_input_commands.h`.
  const ChatCommandListView commands =
      TakeCommandKeysBeforeField(state, preview.commands, ImGui::GetID(kFieldId));
  CommandFieldHooks hooks{.state = &state, .view = &commands, .hints = preview.commands};

  bool submitted = false;
  int cursor_row_before = -1;
  int cursor_rows_before = 1;
  if (wrap_width <= 0.0F) {
    // ↑/↓ rappellent l'historique. ImGui ne l'expose QUE par rappel : le tampon
    // appartient au widget pendant l'édition, l'écrire à côté serait écrasé par
    // son état interne à l'image suivante.
    // ⚠ SAUF LISTE DES COMMANDES OUVERTE : elle les prend pour choisir.
    const auto recall = [](ImGuiInputTextCallbackData* data) -> int {
      const auto* field_hooks = static_cast<const CommandFieldHooks*>(data->UserData);
      if (CompleteOnTab(*field_hooks, data)) {
        return 0;
      }
      if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        const bool up = data->EventKey == ImGuiKey_UpArrow;
        if (NavigateCommands(*field_hooks, up)) {
          return 0;
        }
        std::string text;
        if (Recall(field_hooks->state->history, up ? -1 : 1, data->Buf, &text)) {
          data->DeleteChars(0, data->BufTextLen);
          data->InsertChars(0, text.c_str());
          DismissChatCommandList(field_hooks->state->commands, text);
        }
      }
      return 0;
    };
    submitted =
        ImGui::InputText(kFieldId, state.text.data(), state.text.size(),
                         ImGuiInputTextFlags_EnterReturnsTrue |
                             ImGuiInputTextFlags_CallbackHistory | CommandFieldFlags(commands),
                         recall, &hooks);
  } else {
    // ── Le rectangle à retour à la ligne ─────────────────────────────────────
    // ⚠ PAS DE BARRE DE DÉFILEMENT. L'enfant du champ multiligne est créé en dur
    // par ImGui, sans `NoScrollbar` possible ; la seule prise est la LARGEUR de
    // la barre, mise à zéro — la barre se replie sur un cadre de largeur nulle et
    // le texte garde toute la largeur du champ, ce sur quoi `WrapWidthFor`
    // compte. Le champ montre TOUT ce qu'on écrit : la fenêtre grandit pour ça.
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0F);
    // ⚠ REFUSER LE RETOUR À LA LIGNE. Un message de chat n'en a pas : Ctrl+Entrée
    // et Maj+Entrée, qu'ImGui traduit en saut en multiligne, ne doivent RIEN
    // insérer, et un collage y perd les siens comme en ligne unique.
    const auto refuse_newline = [](ImGuiInputTextCallbackData* data) -> int {
      if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
        return data->EventChar == '\n' ? 1 : 0;
      }
      auto* wrapped = static_cast<WrappedFieldHooks*>(data->UserData);
      if (CompleteOnTab(*wrapped->commands, data)) {
        return 0;
      }
      if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways) {
        CursorProbe& probe = wrapped->probe;
        probe.rows = WrapRows(data->Buf, probe.wrap_width, data->CursorPos, &probe.row);
      }
      return 0;
    };
    // ⚠ LA RANGÉE DU CURSEUR EST CELLE DE L'IMAGE PRÉCÉDENTE, et il le faut :
    // le rappel tourne APRÈS le traitement des touches de cette image, donc lire
    // sa valeur ensuite ne dirait pas si la flèche a trouvé une rangée au-dessus
    // ou n'a rien pu faire.
    WrappedFieldHooks wrapped{.commands = &hooks};
    CursorProbe& probe = wrapped.probe;
    probe.wrap_width = wrap_width;
    probe.row = state.cursor_row;
    probe.rows = state.cursor_rows;
    cursor_row_before = state.cursor_row;
    cursor_rows_before = state.cursor_rows;
    submitted = ImGui::InputTextMultiline(
        kFieldId, state.text.data(), state.text.size(), ImVec2(field_width, field_height),
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine |
            ImGuiInputTextFlags_WordWrap | ImGuiInputTextFlags_CallbackCharFilter |
            ImGuiInputTextFlags_CallbackAlways | CommandFieldFlags(commands),
        refuse_newline, &wrapped);
    ImGui::PopStyleVar();
    state.cursor_row = probe.row;
    state.cursor_rows = probe.rows;
  }
  const ImGuiID field = ImGui::GetItemID();
  const bool active = ImGui::IsItemActive();
  // ⚠ RELEVÉ COLLÉ AU WIDGET, comme pour la box : plus bas, « l'item » ne
  // désignerait plus le champ.
  const bool field_escaped =
      ImGui::IsItemDeactivated() && ImGui::IsKeyPressed(ImGuiKey_Escape, false);
  // ⚠ RELEVÉS AVANT TOUT AUTRE ÉLÉMENT : `GetItemRect*` parle du DERNIER posé, et
  // la rangée en pose d'autres juste après.
  const ImVec2 field_corner = ImGui::GetItemRectMin();
  const ImVec2 field_extent = ImGui::GetItemRectSize();
  const bool field_hovered = ImGui::IsItemHovered();
  if (asked != nullptr) {
    asked->link = PaintInputLinkChips(state, links, field, field_corner, field_extent, wrap_width,
                                      active, field_hovered, link_facts);
  }
  if (inserted) {
    // 🔴 ON PRÉVIENT LE CHAMP. Écrire dans son tampon sans le lui dire
    // laisse son curseur posé sur l'ancien texte ; s'il était plus long, le
    // curseur dépasse la fin et la première image qui redessine le champ
    // franchit une assertion d'ImGui — ce qui TUE le programme.
    ReloadInputField(field);
  }

  // ALT+ENTRÉE VALIDE, dans les deux formes du champ : ImGui ne lit Entrée
  // qu'avec Ctrl, Maj ou rien.
  const ImGuiIO& keys_io = ImGui::GetIO();
  const bool enter_down = ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                          ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
  submitted = submitted || (active && keys_io.KeyAlt && enter_down);
  if (wrap_width > 0.0F && active) {
    const ImGuiIO& io = ImGui::GetIO();
    // Ctrl+Entrée et Maj+Entrée VALIDENT aussi, comme en ligne unique. En
    // multiligne ImGui en fait une insertion que le filtre vient de refuser : la
    // touche n'a donc rien fait et le champ est encore actif — c'est ici qu'on la
    // lit.
    if (!submitted && (io.KeyCtrl || io.KeyShift) &&
        (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
         ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
      submitted = true;
    }
    // L'historique, sans le rappel qu'ImGui refuse en multiligne. La règle : ↑
    // rappelle depuis la PREMIÈRE rangée, ↓ depuis la DERNIÈRE — ailleurs les
    // flèches déplacent le curseur, et ImGui l'a déjà fait. Une phrase d'une
    // rangée se comporte donc exactement comme en ligne unique.
    // 🔴 SEULEMENT SI CE CHAMP-CI A LE CLAVIER, et c'est un plantage payé en
    // jeu. La barre porte un SECOND champ — la box « Pseudo » ; une flèche
    // pressée là réécrivait quand même le texte d'ici, DANS LE DOS D'IMGUI :
    // l'état de saisie est unique, il appartenait alors à la box, et le
    // rechargement ne trouvait personne à prévenir. Le curseur restait posé sur
    // l'ancien texte, plus long, et la première image qui redessinait le champ
    // franchissait son assertion :
    //
    //   text_end_display >= text_begin && text_end_display <= text_end
    //
    // ⚠ ET C'EST AUSSI CE QU'ON ATTEND : une flèche dans la box « Pseudo » ne
    // doit pas remonter l'historique de l'autre champ.
    std::string recalled;
    const bool mine = ImGui::GetActiveID() == field;
    const bool arrow_up = mine && ImGui::IsKeyPressed(ImGuiKey_UpArrow);
    const bool arrow_down = mine && ImGui::IsKeyPressed(ImGuiKey_DownArrow);
    // ⚠ LA LISTE DES COMMANDES OUVERTE PREND LES FLÈCHES, et le curseur qu'ImGui
    // vient de déplacer revient en fin de ligne : on choisit une commande, on
    // n'édite pas.
    if ((arrow_up || arrow_down) && NavigateCommands(hooks, arrow_up)) {
      ReloadInputField(field);
    } else {
      const bool up = arrow_up && cursor_row_before == 0;
      const bool down = arrow_down && cursor_row_before >= cursor_rows_before - 1;
      if ((up || down) && Recall(state.history, up ? -1 : 1, state.text.data(), &recalled)) {
        CopyBounded(state.text, recalled);
        ReloadInputField(field);
        DismissChatCommandList(state.commands, recalled);
      }
    }
  }
  ImGui::PopStyleColor(1 + field_colors);
  DrawCommandsOverField(state, preview.commands, active, field_corner, field_extent.x, art);

  // La place restante, dès que le champ a une deuxième rangée — c'est aussi le
  // moment où l'étiquette laisse, sous elle, la place de l'écrire.
  if (preview.budget_known && rows >= 2) {
    DrawBudget(preview, field_at, field_height);
  }

  // 🔴 LA PERTE DE FOCUS NE REPLIE RIEN. Deux sorties, toutes deux explicites :
  // Entrée sur une phrase VIDE, et Échap. Ni la liste des destinataires, ni une
  // languette, ni un clic dans le décor : ouvrir un menu désactive le champ
  // exactement comme un clic dehors, et la barre se refermerait sous le doigt à
  // l'instant où la liste s'ouvre.
  //
  // 🔴 ET LA DÉCISION NE REGARDE QUE LA PHRASE, jamais la boîte d'où vient la
  // touche : c'est la règle du client d'origine, et c'est aussi la seule qui se
  // retienne.
  if (submitted || whisper.submitted) {
    if (state.text[0] != '\0') {
      said = std::string(state.text.data());
      RememberTypedLine(state.history, *said);
      if (asked != nullptr) {
        asked->keys = ChatSubmitKeys{.ctrl = keys_io.KeyCtrl, .alt = keys_io.KeyAlt};
      }
      state.text[0] = '\0';
      ReloadEmptiedField(field);
      // ⚠ LE CLAVIER RESTE À LA BARRE APRÈS ENVOI, comme dans le jeu : on dit
      // souvent deux phrases de suite, et rendre la touche au monde entre les
      // deux ferait marcher le personnage à la première lettre de la seconde.
      // Et il revient à la boîte d'où l'on a validé — le client ne déplace pas
      // le curseur du joueur pour lui.
      state.focus_on_whisper = whisper.submitted && !submitted;
      state.wants_focus = true;
    } else if (state.battle_mode) {
      // Rien à envoyer : c'est la SORTIE, en une frappe.
      state.unfolded = false;
      if (asked != nullptr) {
        asked->folded = true;
      }
    } else {
      state.wants_focus = true;
    }
  } else if ((field_escaped || whisper.escaped) &&
             ChatBarFoldsOnEscape(state.battle_mode, state.unfolded)) {
    // L'AUTRE SORTIE. La désactivation ET la touche, jamais la touche seule :
    // c'est bien une des deux boîtes qu'Échap vient de quitter — ImGui la
    // désactive et restaure son texte dans la même image —, pas un Échap tapé
    // ailleurs, qui appartient au jeu.
    state.unfolded = false;
    if (asked != nullptr) {
      asked->folded = true;
    }
  }

  // Laquelle des deux boîtes portait le clavier en dernier. ⚠ ELLE SURVIT À LA
  // PERTE DU FOCUS, et c'est tout son intérêt : un emoji pioché ou un lien posé
  // rendent le clavier là où le joueur avait laissé son curseur.
  if (whisper.active) {
    state.focus_on_whisper = true;
  }
  if (active) {
    state.focus_on_whisper = false;
  }
  state.was_active = active;
  return said;
}

}  // namespace ro::ui
