#include "ro/ui/chat_macros.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

#include "ro/ui/modal_box.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_bounds.h"
#include "ro/ui/window_name.h"

#include "imgui.h"
#include "imgui_internal.h"

namespace ro::ui {
namespace {

// La taille au PREMIER affichage seulement : ensuite, c'est celle que le joueur
// lui a donnée. Celle de l'implémentation de référence.
constexpr float kFirstWidth = 470.0F;
constexpr float kFirstHeight = 360.0F;

// Le jeton du glisser-déposer d'une ligne. ⚠ TREIZE CARACTÈRES AU PLUS : ImGui
// borne le type d'une charge utile, et il tronque en silence — deux types dont
// les treize premiers caractères coïncident deviendraient le même.
constexpr const char* kRowPayload = "macro_row";

// La colonne de touche est taillée sur le PLUS LONG libellé possible, pas sur
// celui qu'on affiche : « Alt+1 » et « Ctrl+Maj+F12 » ne font pas la même
// largeur, et une colonne qui suit son contenu bouge à chaque réaffectation.
constexpr const char* kWidestKeyLabel = "Ctrl+Maj+F12";
// De même pour le compte : « 00/00 » est le plus large qu'il puisse valoir.
constexpr const char* kWidestCountLabel = "00/00";

// Combien de paddings encadrent le texte d'une colonne fixe. Nommé plutôt
// qu'écrit : un 4 au milieu d'une multiplication ne dit pas ce qu'il compte.
constexpr float kPaddingsAroundText = 4.0F;

// 🔴 LE COMPTE QUAND LA LIGNE EST PLEINE. La troncature du client est
// SILENCIEUSE : c'est le seul endroit où elle peut se voir arriver.
constexpr ImU32 kFullCountColor = IM_COL32(179, 51, 38, 255);

// La grille d'émotes du menu : six colonnes, et une case taillée sur son
// contenu.
constexpr int kEmotesPerRow = 6;
constexpr float kEmoteSpriteSide = 30.0F;
// La plus longue commande d'émote fait cinq caractères — « /swt2 », « /kis2 ».
// ⚠ MESURÉE, JAMAIS ESTIMÉE : la police est un réglage.
constexpr const char* kWidestEmoteCommand = "/swt2";
constexpr float kEmoteCellPadding = 6.0F;
constexpr float kEmoteTextGap = 1.0F;
constexpr ImU32 kEmoteHoverEdge = IM_COL32(40, 40, 40, 160);

[[nodiscard]] bool RowIsEmpty(const ChatMacroState& state, std::size_t slot) {
  return state.rows[slot][0] == '\0';
}

// Écrit une ligne, tronquée au tampon. ⚠ `snprintf` TERMINE TOUJOURS PAR UN
// ZÉRO, même quand il tronque : c'est ce qui rend la troncature sûre plutôt que
// silencieusement fautive.
void SetRow(ChatMacroState& state, std::size_t slot, const char* text) {
  std::snprintf(state.rows[slot].data(), state.rows[slot].size(), "%s",
                text != nullptr ? text : "");
}

// ÉCHANGE DEUX LIGNES. ⚠ LES TEXTES SEULEMENT, JAMAIS LES TOUCHES : Alt+3 reste
// la troisième ligne. Le joueur réordonne ses macros, il ne remappe pas son
// clavier par mégarde — et une touche qui suivrait son texte ferait de chaque
// glisser une réaffectation muette.
void SwapRows(ChatMacroState& state, std::size_t first, std::size_t second) {
  std::swap(state.rows[first], state.rows[second]);
}

// ── LE MENU DU CLIC DROIT ────────────────────────────────────────────────────
// Ce qui rend la fenêtre utilisable sans connaître la liste des commandes par
// cœur : le champ du client, lui, n'offrait qu'un vide.

// La grille d'émotes du menu. Chaque case porte la COMMANDE sous le sprite :
// c'est elle qui atterrira dans la macro, et beaucoup d'émotes ne se
// reconnaissent pas à leur seule image en trente pixels.
[[nodiscard]] bool DrawEmoteGrid(ChatMacroState& state, std::size_t slot,
                                 std::span<const ChatEmotion> emotions) {
  // 🔴 LA GRILLE EST DIMENSIONNÉE SUR SON CONTENU, et rien d'autre : ni zone
  // défilante, ni rangées réservées pour du vide. Il y a au plus une trentaine
  // d'émotes commandables, donc cela tient toujours en quelques rangées.
  const float text_height = ImGui::GetTextLineHeight();
  const float text_width = ImGui::CalcTextSize(kWidestEmoteCommand).x;
  const float cell_width = std::max(text_width, kEmoteSpriteSide) + kEmoteCellPadding;
  const float cell_height = kEmoteSpriteSide + text_height + kEmoteTextGap;

  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImU32 text_color = ImGui::GetColorU32(ImGuiCol_Text);
  const double now_s = ImGui::GetTime();
  bool changed = false;
  int column = 0;
  for (const ChatEmotion& emotion : emotions) {
    if (emotion.command.empty()) {
      continue;
    }
    if (column != 0) {
      ImGui::SameLine();
    }
    ImGui::PushID(emotion.id);
    const ImVec2 corner = ImGui::GetCursorScreenPos();
    const bool chosen = ImGui::InvisibleButton("##emote", ImVec2(cell_width, cell_height));

    const ImVec2 sprite_at(corner.x + (cell_width - kEmoteSpriteSide) * 0.5F, corner.y);
    PaintEmotion(into, emotion, sprite_at, kEmoteSpriteSide, now_s);
    const ImVec2 measured = ImGui::CalcTextSize(emotion.command.data(),
                                                emotion.command.data() + emotion.command.size());
    into->AddText(ImVec2(corner.x + (cell_width - measured.x) * 0.5F,
                         corner.y + kEmoteSpriteSide + kEmoteTextGap),
                  text_color, emotion.command.data(),
                  emotion.command.data() + emotion.command.size());

    if (ImGui::IsItemHovered()) {
      into->AddRect(corner, ImVec2(corner.x + cell_width, corner.y + cell_height), kEmoteHoverEdge);
      // Le nom du sprite en infobulle : la commande est déjà lisible sous
      // l'image, c'est l'émote elle-même qu'on peut avoir besoin de nommer.
      SkinTooltip(std::string(emotion.name).c_str());
    }
    if (chosen) {
      SetRow(state, slot, std::string(emotion.command).c_str());
      changed = true;
    }
    ImGui::PopID();
    if (++column == kEmotesPerRow) {
      column = 0;
    }
  }
  return changed;
}

// Le menu d'une ligne. Rend vrai si la ligne a été modifiée.
[[nodiscard]] bool DrawPrefillMenu(ChatMacroState& state, std::size_t slot, std::string_view key,
                                   std::span<const ChatMacroCommandGroup> commands,
                                   std::span<const ChatEmotion> emotions) {
  bool changed = false;

  // Rappel de la ligne visée et de ce qu'on va écraser : le menu s'ouvre au clic
  // droit, donc sur une ligne qu'on ne regardait pas forcément.
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::Text("%.*s  %s", static_cast<int>(key.size()), key.data(),
              RowIsEmpty(state, slot) ? core::Tr("(vide)") : state.rows[slot].data());
  ImGui::PopStyleColor();
  ImGui::Separator();

  // Un groupe = un sous-menu. ⚠ LE SECOND ARGUMENT DE `MenuItem` EST LA COLONNE
  // DES RACCOURCIS : grisée et alignée à droite, elle sert ici de colonne
  // d'explication, sans widget de plus.
  for (const ChatMacroCommandGroup& group : commands) {
    if (!ImGui::BeginMenu(std::string(group.label).c_str())) {
      continue;
    }
    for (const ChatMacroCommand& command : group.items) {
      if (ImGui::MenuItem(std::string(command.text).c_str(), std::string(command.help).c_str())) {
        SetRow(state, slot, std::string(command.text).c_str());
        changed = true;
      }
    }
    ImGui::EndMenu();
  }

  if (!emotions.empty() && ImGui::BeginMenu(core::Tr("Émotions"))) {
    changed = DrawEmoteGrid(state, slot, emotions) || changed;
    ImGui::EndMenu();
  }

  ImGui::Separator();
  if (ImGui::MenuItem(core::Tr("Vider la ligne"), nullptr, false, !RowIsEmpty(state, slot))) {
    SetRow(state, slot, "");
    changed = true;
  }
  return changed;
}

// La poignée de gauche : le libellé de touche, et la prise du glisser.
void DrawKeyHandle(ChatMacroState& state, std::size_t slot, std::string_view key,
                   ChatMacroAsk* asked) {
  // ⚠ LE LIBELLÉ PORTE SON IDENTITÉ APRÈS TROIS DIÈSES : une réaffectation
  // change le texte montré, et une prise de glisser dont l'identifiant change
  // sous le doigt lâche ce qu'elle tenait.
  std::array<char, 96> label{};
  std::snprintf(label.data(), label.size(), "%.*s###macro_key_%zu", static_cast<int>(key.size()),
                key.data(), slot);

  // 🔴 UN ÉLÉMENT QUI A UN IDENTIFIANT, et non un simple texte : une source de
  // glisser posée sur un `Text()` n'a rien à quoi s'accrocher, et elle échoue
  // sans un mot. C'est le piège que l'implémentation de référence a payé.
  ImGui::Selectable(label.data(), false, ImGuiSelectableFlags_None,
                    ImVec2(0.0F, ImGui::GetFrameHeight()));

  if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover)) {
    const int from = static_cast<int>(slot);
    ImGui::SetDragDropPayload(kRowPayload, &from, sizeof(from));
    ImGui::TextUnformatted(RowIsEmpty(state, slot) ? core::Tr("(vide)") : state.rows[slot].data());
    ImGui::EndDragDropSource();
  }
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(kRowPayload)) {
      const int from = *static_cast<const int*>(dropped->Data);
      if (from >= 0 && static_cast<std::size_t>(from) < kChatMacroCount &&
          static_cast<std::size_t>(from) != slot) {
        SwapRows(state, static_cast<std::size_t>(from), slot);
        asked->changed = true;
      }
    }
    ImGui::EndDragDropTarget();
  }
  if (ImGui::IsItemHovered()) {
    // ⚠ DEUX ENTRÉES DE CATALOGUE, RECOLLÉES ICI : le catalogue range une
    // traduction par LIGNE, si bien qu'un saut de ligne dans la clé la couperait
    // en deux et en perdrait la moitié. Le saut se pose donc dans le code.
    std::string tip(
        core::Tr("Glisser sur une autre ligne pour échanger les deux macros. La "
                 "touche, elle, reste attachée au rang."));
    tip += '\n';
    tip += core::Tr("Clic droit : commandes du serveur.");
    SkinTooltip(tip.c_str());
  }
}

// Ouvre le menu d'une ligne depuis n'importe lequel de ses éléments.
//
// 🔴 `ClearActiveID()` AVANT D'OUVRIR, ET CE N'EST PAS DÉCORATIF. `InputText`
// garde son propre tampon d'édition tant qu'il est actif, et ne le relit du
// nôtre qu'à la prise de focus : sur un champ resté actif, le préremplissage
// serait invisible — et la frappe suivante réécrirait l'ancien texte par-dessus.
void OpenMenuOnRightClick(const char* id) {
  if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    ImGui::ClearActiveID();
    ImGui::OpenPopup(id);
  }
}

void DrawRow(ChatMacroState& state, std::size_t slot, std::string_view key,
             std::span<const ChatMacroCommandGroup> commands, std::span<const ChatEmotion> emotions,
             const SkinArt& art, ChatMacroAsk* asked) {
  ImGui::TableNextRow();
  std::array<char, 32> menu{};
  std::snprintf(menu.data(), menu.size(), "##macro_menu_%zu", slot);

  ImGui::TableSetColumnIndex(0);
  DrawKeyHandle(state, slot, key, asked);
  OpenMenuOnRightClick(menu.data());

  ImGui::TableSetColumnIndex(1);
  ImGui::SetNextItemWidth(-1.0F);
  std::array<char, 32> field{};
  std::snprintf(field.data(), field.size(), "##macro_text_%zu", slot);
  if (ImGui::InputTextWithHint(field.data(), core::Tr("Texte, /commande ou @commande..."),
                               state.rows[slot].data(), state.rows[slot].size())) {
    asked->changed = true;
  }
  OpenMenuOnRightClick(menu.data());

  // ── LA LONGUEUR ────────────────────────────────────────────────────────────
  // Le champ du client tronque à cinquante octets sans rien dire ; ici la limite
  // se voit arriver.
  ImGui::TableSetColumnIndex(2);
  const std::size_t written = std::strlen(state.rows[slot].data());
  const bool full = written >= kChatMacroBytes;
  ImGui::PushStyleColor(ImGuiCol_Text, full ? ImGui::ColorConvertU32ToFloat4(kFullCountColor)
                                            : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::Text("%zu/%zu", written, kChatMacroBytes);
  ImGui::PopStyleColor();

  ImGui::TableSetColumnIndex(3);
  // ⚠ GRISÉ ET NON RETIRÉ : un bouton qui disparaît fait sauter la mise en page
  // de la ligne sous les yeux de qui vient d'effacer son texte.
  ImGui::BeginDisabled(RowIsEmpty(state, slot));
  std::array<char, 64> send{};
  std::snprintf(send.data(), send.size(), "%s###macro_send_%zu", core::Tr("Essayer"), slot);
  if (SkinButton(send.data(), art)) {
    asked->send = static_cast<int>(slot);
  }
  ImGui::EndDisabled();
  if (!RowIsEmpty(state, slot) && ImGui::IsItemHovered()) {
    SkinTooltip(core::Tr("Envoie la macro tout de suite, sans fermer la fenêtre."));
  }

  // Le menu, rendu EN DERNIER : un popup se dessine là où il est appelé, pas là
  // où il est ouvert, et ses deux points d'ouverture sont au-dessus.
  if (BeginSkinPopup(menu.data())) {
    if (DrawPrefillMenu(state, slot, key, commands, emotions)) {
      asked->changed = true;
      ImGui::CloseCurrentPopup();
    }
    EndSkinPopup();
  }
}

}  // namespace

void DrawChatMacros(ChatMacroState& state, std::span<const std::string> keys,
                    std::string_view target, std::span<const ChatMacroCommandGroup> commands,
                    std::span<const ChatEmotion> emotions, const SkinArt& art,
                    ChatMacroAsk* asked) {
  if (!state.open) {
    return;
  }
  static WindowName name(kChatMacroTitle, kChatMacroId);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidth, kFirstHeight), ImGuiCond_FirstUseEver);
  ConstrainNextWindowToScreen(name.Get());
  if (!BeginSkinnedWindow(name.Get(), &state.open, art)) {
    EndSkinnedWindow();
    return;
  }

  // ── OÙ PART UNE MACRO ──────────────────────────────────────────────────────
  // Le renseignement que la fenêtre du client ne donne nulle part : la même
  // ligne part alentour, au groupe ou à la guilde selon ce que la barre a armé.
  //
  // ⚠ LE TEXTE SECONDAIRE DE L'HABILLAGE, que la palette de la fenêtre pose en
  // `TextDisabled` — et non le gris éteint d'ImGui, illisible sur un corps clair.
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextUnformatted(core::Tr("Ces macros partent sur :"));
  ImGui::PopStyleColor();
  ImGui::SameLine();
  ImGui::TextUnformatted(target.data(), target.data() + target.size());
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("La destination suit la barre de saisie. Une macro qui commence par / "
                 "ou @ n'est pas concernée : c'est une commande."));
  }
  ImGui::Separator();

  const float footer = SkinButtonHeight(art) + ImGui::GetStyle().ItemSpacing.y * 2.0F;
  const ImVec2 table(0.0F,
                     std::max(ImGui::GetFrameHeight(), ImGui::GetContentRegionAvail().y - footer));
  const float padding = ImGui::GetStyle().FramePadding.x * kPaddingsAroundText;
  const float key_width = ImGui::CalcTextSize(kWidestKeyLabel).x + padding;
  const float count_width = ImGui::CalcTextSize(kWidestCountLabel).x + padding;
  const float send_width = SkinButtonWidth(core::Tr("Essayer"), art);

  if (ImGui::BeginTable("##macro_rows", 4,
                        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY, table)) {
    ImGui::TableSetupColumn(core::Tr("Touche"), ImGuiTableColumnFlags_WidthFixed, key_width);
    ImGui::TableSetupColumn(core::Tr("Macro"), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("##compte", ImGuiTableColumnFlags_WidthFixed, count_width);
    ImGui::TableSetupColumn("##essayer", ImGuiTableColumnFlags_WidthFixed, send_width);
    ImGui::TableHeadersRow();
    for (std::size_t slot = 0; slot < kChatMacroCount; ++slot) {
      // ⚠ UNE TOUCHE MANQUANTE NE RETIRE PAS LA LIGNE : la macro existe même
      // sans raccourci, et le bouton « Essayer » l'envoie quand même. C'est ce
      // qui distingue « pas de touche » de « pas de macro ».
      const std::string_view key =
          slot < keys.size() ? std::string_view(keys[slot]) : std::string_view();
      DrawRow(state, slot, key.empty() ? core::Tr("(aucune)") : key, commands, emotions, art,
              asked);
    }
    ImGui::EndTable();
  }

  const float keys_width = SkinButtonWidth(core::Tr("Modifier les touches..."), art);
  if (SkinButton(core::Tr("Modifier les touches..."), art, ImVec2(keys_width, 0.0F))) {
    asked->open_hotkeys = true;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(core::Tr("Ouvre l'écran des raccourcis, où ces dix touches se réaffectent."));
  }

  ImGui::SameLine();
  const float reset_width = SkinButtonWidth(core::Tr("Valeurs par défaut..."), art);
  if (SkinButton(core::Tr("Valeurs par défaut..."), art, ImVec2(reset_width, 0.0F))) {
    AskModal(kChatMacroResetId);
  }

  const float close_width = SkinButtonWidth(core::Tr("Fermer"), art);
  ImGui::SameLine();
  ImGui::SetCursorPosX(
      std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - close_width));
  if (SkinButton(core::Tr("Fermer"), art, ImVec2(close_width, 0.0F))) {
    state.open = false;
  }

  // ⚠ LA QUESTION DIT CE QU'ON PERD, et non seulement ce qu'on gagne : les dix
  // lignes reprennent les valeurs d'origine, donc ce qui est écrit s'en va.
  switch (DrawModalBox(kChatMacroResetId,
                       core::Tr("Les dix macros vont reprendre leurs valeurs d'origine. Ce que "
                                "vous avez écrit sera perdu."),
                       core::Tr("OK"), core::Tr("Annuler"), art)) {
    case ModalAnswer::kYes:
      asked->reset = true;
      break;
    case ModalAnswer::kNo:
    case ModalAnswer::kWaiting:
      break;
  }

  EndSkinnedWindow();
}

}  // namespace ro::ui
