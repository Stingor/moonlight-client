#include "ro/ui/mail_window.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "mail_parts.h"

namespace ro::ui {
namespace {

using mail::Scale;

// ── LES MESURES ─────────────────────────────────────────────────────────────
//
// Celles de la boîte.
constexpr float kFirstLeftPixels = 380.0F;
constexpr float kFirstTopPixels = 150.0F;
// La hauteur de départ est celle de la liste seule.
constexpr float kFirstWidthPixels = 560.0F;
constexpr float kFirstHeightPixels = 268.0F;
// Le bloc « courrier ouvert », ajouté ou retiré EN DELTA.
constexpr float kDetailHeightPixels = 232.0F;
// La liste à hauteur fixe pendant la lecture, et le corps du courrier.
constexpr float kListWhileReadingPixels = 190.0F;
constexpr float kBodyHeightPixels = 86.0F;
constexpr float kClaimButtonPixels = 150.0F;
constexpr float kActionButtonPixels = 110.0F;
// Les colonnes : la marque, deux colonnes étirées à ces poids, l'expiration.
constexpr float kMarkColumnPixels = 26.0F;
constexpr float kSenderColumnWeight = 0.34F;
constexpr float kTitleColumnWeight = 0.50F;
constexpr float kExpiryColumnPixels = 52.0F;
constexpr int kTableColumns = 4;

constexpr std::int64_t kSecondsPerHour = std::int64_t{60} * 60;
constexpr std::int64_t kSecondsPerDay = kSecondsPerHour * 24;

// L'insigne d'un courrier du serveur : discret, mais distinct du noir d'un
// joueur.
constexpr ImVec4 kSystemBadgeText = ImVec4(0.20F, 0.40F, 0.70F, 1.0F);

// La place d'un titre ou d'un libellé composé.
constexpr std::size_t kLabelBytes = 96;

constexpr std::string_view kConfirmId = "rodex_confirm";
constexpr const char* kItemMenuId = "##rodex_item_menu";
// Les boutons de la confirmation.
constexpr float kConfirmButtonPixels = 110.0F;

[[nodiscard]] char Folded(char letter) {
  return static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
}

[[nodiscard]] bool ContainsFolded(std::string_view haystack, std::string_view needle) {
  const auto found = std::ranges::search(
      haystack, needle, [](char left, char right) { return Folded(left) == Folded(right); });
  return !found.empty() || needle.empty();
}

[[nodiscard]] std::string_view FilterOf(const MailboxWindowState& state) {
  return std::string_view(state.filter.data());
}

[[nodiscard]] const MailRowView* FindRow(std::span<const MailRowView> rows, const MailRef& ref) {
  const auto found =
      std::ranges::find_if(rows, [&ref](const MailRowView& row) { return row.ref == ref; });
  return found == rows.end() ? nullptr : &*found;
}

void Unformatted(std::string_view text) {
  ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

void Colored(ImVec4 color, std::string_view text) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  Unformatted(text);
  ImGui::PopStyleColor();
}

void Disabled(std::string_view text) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  Unformatted(text);
  ImGui::PopStyleColor();
}

void SystemBadge(const MailRowView& row) {
  if ((row.carried & kMailFromSystem) == 0) {
    return;
  }
  ImGui::SameLine();
  ImGui::TextColored(kSystemBadgeText, "%s", core::Tr("Système"));
}

[[nodiscard]] const char* TabName(MailboxTab tab) {
  switch (tab) {
    case MailboxTab::kNormal:
      return core::Tr("Reçus");
    case MailboxTab::kAccount:
      return core::Tr("Compte");
    case MailboxTab::kReturned:
      return core::Tr("Retournés");
    case MailboxTab::kAll:
      return core::Tr("Tous");
  }
  return core::Tr("Tous");
}

void Forget(MailboxWindowState& state) {
  state.selected.reset();
  state.confirm = MailConfirm::kNone;
}

// ── LA LISTE ────────────────────────────────────────────────────────────────

void DrawCarried(const MailRowView& row, const MailCarriedIcons& icons) {
  const SkinImage* icon = nullptr;
  // Le repère quand l'image de l'installation manque.
  const char* mark = "-";
  const char* what = nullptr;
  const bool zeny = (row.carried & kMailCarriesZeny) != 0;
  const bool items = (row.carried & kMailCarriesItems) != 0;
  if (zeny && items) {
    icon = &icons.zeny_and_items;
    mark = "#";
    what = core::Tr("Items et zeny à récupérer");
  } else if (items) {
    icon = &icons.items;
    mark = "#";
    what = core::Tr("Items à récupérer");
  } else if (zeny) {
    icon = &icons.zeny;
    mark = "z";
    what = core::Tr("Zeny à récupérer");
  }
  if (icon != nullptr && icon->usable()) {
    // Hauteur alignée sur la ligne de texte, largeur au ratio : ces images ne
    // font pas toutes la même taille, et un carré forcé les déformerait.
    const float height = ImGui::GetTextLineHeight();
    const float width = height * icon->width / icon->height;
    ImGui::Image(icon->id, ImVec2(width, height));
  } else if (what != nullptr) {
    ImGui::TextColored(kTradeBlackText, "%s", mark);
  } else {
    ImGui::TextDisabled("%s", mark);
  }
  if (what != nullptr && ImGui::IsItemHovered()) {
    SkinTooltip(what);
  }
}

void DrawRow(MailboxWindowState& state, const MailRowView& row, const MailCarriedIcons& icons,
             MailboxAction& action) {
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  DrawCarried(row, icons);

  ImGui::TableNextColumn();
  // ⚠ L'EXPÉDITEUR N'EST PAS LE LIBELLÉ DE LA LIGNE : un « ## » dans un nom en
  // changerait l'identité et en couperait l'affichage. La ligne cliquable est
  // anonyme, le nom s'écrit par-dessus.
  const bool selected = state.selected.has_value() && *state.selected == row.ref;
  const float left = ImGui::GetCursorPosX();
  if (ImGui::Selectable("##mail", selected,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
    state.selected = row.ref;
    state.confirm = MailConfirm::kNone;
    // Le contenu n'arrive qu'à la demande, et il ne survit pas à la session :
    // il se réclame dès qu'il n'est pas là, pas seulement quand il est non lu.
    if (!row.content_ready) {
      action.read = row.ref;
    }
  }
  ImGui::SameLine();
  ImGui::SetCursorPosX(left);
  if (row.sender.empty()) {
    Unformatted(core::Tr("(inconnu)"));
  } else {
    Unformatted(row.sender);
  }
  SystemBadge(row);

  ImGui::TableNextColumn();
  // Sujet grisé : déjà lu, selon le serveur.
  if (row.read) {
    Disabled(row.title);
  } else {
    Colored(kTradeBlackText, row.title);
  }

  ImGui::TableNextColumn();
  const std::string expiry = MailExpiryLabel(row.expire_seconds_left);
  if (MailExpiresSoon(row)) {
    Colored(mail::kRefusalText, expiry);
  } else {
    Disabled(expiry);
  }
}

void DrawCountLine(std::size_t count) {
  const int shown = static_cast<int>(count);
  if (shown > 1) {
    ImGui::TextDisabled(core::Tr("%d courriers"), shown);
  } else {
    ImGui::TextDisabled(core::Tr("%d courrier"), shown);
  }
}

void DrawTabs(MailboxWindowState& state, const MailboxTabs& tabs, const SkinArt& art) {
  if (!BeginSkinTabBar("rodex_tabs", art, ImGuiTabBarFlags_FittingPolicyScroll)) {
    return;
  }
  for (std::size_t tab = 0; tab < kMailboxTabCount; ++tab) {
    if (static_cast<MailboxTab>(tab) == MailboxTab::kAccount && !tabs.show_account) {
      continue;
    }
    // ### : le compteur change à chaque courrier, l'identité de l'onglet ne
    // doit pas changer avec lui.
    std::array<char, kLabelBytes> label{};
    std::snprintf(label.data(), label.size(), core::Tr("%s (%d)###rodex_tab%d"),
                  TabName(static_cast<MailboxTab>(tab)), tabs.counts[tab], static_cast<int>(tab));
    if (ImGui::BeginTabItem(label.data())) {
      if (state.tab != static_cast<MailboxTab>(tab)) {
        state.tab = static_cast<MailboxTab>(tab);
        Forget(state);
      }
      ImGui::EndTabItem();
    }
  }
  EndSkinTabBar();
}

void DrawList(MailboxWindowState& state, const MailboxView& view, const SkinArt& art,
              MailboxAction& action) {
  const float scale = Scale();
  // Les actions de BOÎTE d'abord : elles ne dépendent pas de l'onglet.
  if (SkinButton(core::Tr("Rafraîchir"), art)) {
    action.refresh = true;
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Écrire"), art)) {
    action.compose = true;
  }
  ImGui::SameLine();
  DrawCountLine(view.rows.size());

  const MailboxTabs tabs = CountMailboxTabs(view.rows);
  SettleMailboxTab(state, tabs);
  DrawTabs(state, tabs, art);

  ImGui::SetNextItemWidth(-1.0F);
  ImGui::InputTextWithHint("##rodex_filter", core::Tr("Filtrer par expéditeur ou sujet..."),
                           state.filter.data(), state.filter.size());
  const std::string_view filter = FilterOf(state);

  // Courrier ouvert : la liste garde une hauteur fixe, le bloc de lecture prend
  // la place gagnée par l'agrandissement. Sinon elle prend tout, moins le pied.
  const ImGuiStyle& style = ImGui::GetStyle();
  const float footer =
      SkinButtonHeight(art) + ImGui::GetTextLineHeight() + style.ItemSpacing.y * 2.0F;
  const float height = state.selected.has_value() ? kListWhileReadingPixels * scale : -footer;
  ImGui::BeginChild("rodex_list", ImVec2(0.0F, height), ImGuiChildFlags_Borders);
  if (view.rows.empty()) {
    ImGui::TextDisabled(
        "%s", view.list_loading ? core::Tr("Chargement...") : core::Tr("Aucun courrier."));
  } else if (ImGui::BeginTable("rodex_table", kTableColumns,
                               ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                   ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, kMarkColumnPixels * scale);
    ImGui::TableSetupColumn(core::Tr("Expéditeur"), ImGuiTableColumnFlags_WidthStretch,
                            kSenderColumnWeight);
    ImGui::TableSetupColumn(core::Tr("Sujet"), ImGuiTableColumnFlags_WidthStretch,
                            kTitleColumnWeight);
    ImGui::TableSetupColumn(core::Tr("Expire"), ImGuiTableColumnFlags_WidthFixed,
                            kExpiryColumnPixels * scale);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    int shown = 0;
    for (std::size_t index = 0; index < view.rows.size(); ++index) {
      const MailRowView& row = view.rows[index];
      if (!MailRowShown(row, state.tab, filter)) {
        continue;
      }
      ++shown;
      ImGui::PushID(static_cast<int>(index));
      DrawRow(state, row, view.icons, action);
      ImGui::PopID();
    }
    ImGui::EndTable();
    // Une table vide alors que la boîte ne l'est pas : on distingue « le filtre
    // ne trouve rien » de « cet onglet est vide ».
    if (shown == 0) {
      ImGui::TextDisabled("%s", filter.empty() ? core::Tr("Aucun courrier dans cet onglet.")
                                               : core::Tr("Aucun courrier ne correspond."));
    }
  }
  ImGui::EndChild();

  // « Récupérer tout » vise l'onglet tel qu'il se montre.
  const bool none_to_claim = MailClaimTargets(view.rows, state).empty();
  ImGui::BeginDisabled(none_to_claim);
  if (SkinButton(core::Tr("Récupérer tout"), art) && !none_to_claim) {
    state.confirm = MailConfirm::kClaimAll;
  }
  ImGui::EndDisabled();
  // « Supprimer tout » aussi, sur ce qui ne porte plus rien.
  ImGui::SameLine();
  const bool none_to_delete = MailDeleteTargets(view.rows, state).empty();
  ImGui::BeginDisabled(none_to_delete);
  if (SkinButton(core::Tr("Supprimer tout"), art) && !none_to_delete) {
    state.confirm = MailConfirm::kDeleteAll;
  }
  ImGui::EndDisabled();
}

// ── LA LECTURE ──────────────────────────────────────────────────────────────

void DrawAttachments(const MailRowView& row, const MailContentView* content, const SkinArt& art,
                     ItemLinkMenu& menu, MailboxAction& action) {
  const bool has_zeny = (row.carried & kMailCarriesZeny) != 0;
  const bool has_items = (row.carried & kMailCarriesItems) != 0;
  if (!has_zeny && !has_items) {
    return;
  }
  // Le DÉTAIL n'existe qu'une fois le contenu reçu : avant, seul le masque dit
  // qu'il transporte quelque chose, et aucune valeur ne s'invente.
  if (has_zeny && content != nullptr) {
    ImGui::TextColored(kTradeBlackText, core::Tr("Zeny joint : %lld z"),
                       static_cast<long long>(content->zeny));
  } else if (has_zeny) {
    ImGui::TextDisabled("%s", core::Tr("Zeny joint (ouvre le courrier pour voir le montant)"));
  }
  if (content != nullptr) {
    for (std::size_t index = 0; index < content->items.size(); ++index) {
      ImGui::PushID(static_cast<int>(index));
      const TradeItemGesture gesture = mail::DrawItemRow(content->items[index]);
      if (gesture.hovered) {
        action.hovered_item = static_cast<int>(index);
      }
      if (NoteTradeItemLink(gesture, content->items[index].item, static_cast<std::uint32_t>(index),
                            menu)) {
        action.described_item = static_cast<int>(index);
        action.item_gesture = ChatLinkGesture::kRepost;
      }
      ImGui::PopID();
    }
  }
  if (has_items && content == nullptr) {
    ImGui::TextDisabled("%s", core::Tr("Items joints (ouvre le courrier pour les voir)"));
  }
  if (SkinButton(core::Tr("Tout récupérer"), art, ImVec2(kClaimButtonPixels * Scale(), 0.0F))) {
    action.claim = row.ref;
  }
}

void DrawDetail(MailboxWindowState& state, const MailboxView& view, const SkinArt& art,
                MailboxAction& action) {
  const MailRowView* row = FindRow(view.rows, *state.selected);
  if (row == nullptr) {
    // Le courrier a disparu — supprimé, retourné : plus de cible pour la
    // question, et la fenêtre se rétracte d'elle-même à l'image suivante.
    Forget(state);
    return;
  }
  const MailContentView* content =
      view.content != nullptr && view.content->ref == row->ref ? view.content : nullptr;
  const float scale = Scale();

  const std::string_view sender = row->sender;
  ImGui::TextColored(kTradeBlackText, core::Tr("De : %.*s"), static_cast<int>(sender.size()),
                     sender.data());
  SystemBadge(*row);
  const std::string expiry = MailExpiryLabel(row->expire_seconds_left);
  if (!expiry.empty()) {
    ImGui::SameLine();
    const ImVec4 color = MailExpiresSoon(*row) ? mail::kRefusalText
                                               : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    ImGui::TextColored(color, core::Tr("(expire : %s)"), expiry.c_str());
  }
  const std::string_view title = row->title;
  ImGui::TextColored(kTradeBlackText, core::Tr("Sujet : %.*s"), static_cast<int>(title.size()),
                     title.data());

  ImGui::BeginChild("rodex_body", ImVec2(0.0F, kBodyHeightPixels * scale), ImGuiChildFlags_Borders);
  if (content == nullptr && row->content_unanswered) {
    // La lecture s'est perdue : cliquer de nouveau sur la ligne la redemande.
    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextDisabled("%s", core::Tr("Le message n'a pas pu être chargé. Clique de nouveau sur "
                                       "le courrier pour réessayer."));
    ImGui::PopTextWrapPos();
  } else if (content == nullptr) {
    ImGui::TextDisabled("%s", core::Tr("Chargement du message..."));
  } else if (content->body.empty()) {
    ImGui::TextDisabled("%s", core::Tr("(message vide)"));
  } else {
    ImGui::PushTextWrapPos(0.0F);
    Unformatted(content->body);
    ImGui::PopTextWrapPos();
  }
  ImGui::EndChild();

  DrawAttachments(*row, content, art, state.item_menu, action);

  const ImVec2 button(kActionButtonPixels * scale, 0.0F);
  if (SkinButton(core::Tr("Répondre"), art, button)) {
    action.reply = row->ref;
  }
  ImGui::SameLine();
  const bool can_return = MailCanReturn(*row);
  ImGui::BeginDisabled(!can_return);
  if (SkinButton(core::Tr("Retourner"), art, button) && can_return) {
    state.confirm = MailConfirm::kReturn;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  // Le serveur refuse de supprimer un courrier qui porte encore quelque chose.
  const bool can_delete = (row->carried & (kMailCarriesZeny | kMailCarriesItems)) == 0;
  ImGui::BeginDisabled(!can_delete);
  if (SkinButton(core::Tr("Supprimer"), art, button) && can_delete) {
    state.confirm = MailConfirm::kDelete;
  }
  ImGui::EndDisabled();
  if (!can_delete) {
    ImGui::TextDisabled("%s", core::Tr("Récupère d'abord les pièces jointes."));
  }
}

// ── LA CONFIRMATION ─────────────────────────────────────────────────────────

[[nodiscard]] std::string ClaimAllQuestion(std::size_t count) {
  std::array<char, kLabelBytes> question{};
  const int shown = static_cast<int>(count);
  if (shown > 1) {
    std::snprintf(question.data(), question.size(),
                  core::Tr("Récupérer les pièces jointes de %d courriers ?"), shown);
  } else {
    std::snprintf(question.data(), question.size(),
                  core::Tr("Récupérer les pièces jointes de %d courrier ?"), shown);
  }
  return question.data();
}

// « Supprimer définitivement 12 courriers ? », suivi, quand l'onglet en garde,
// de combien restent et pourquoi.
[[nodiscard]] std::string DeleteAllQuestion(std::size_t count, std::size_t kept) {
  std::array<char, kLabelBytes> part{};
  const int erased = static_cast<int>(count);
  std::snprintf(part.data(), part.size(),
                erased > 1 ? core::Tr("Supprimer définitivement %d courriers ?")
                           : core::Tr("Supprimer définitivement %d courrier ?"),
                erased);
  std::string question = part.data();
  if (kept > 0) {
    const int left = static_cast<int>(kept);
    std::snprintf(part.data(), part.size(),
                  left > 1 ? core::Tr("(%d gardés : il leur reste des pièces jointes)")
                           : core::Tr("(%d gardé : il lui reste des pièces jointes)"),
                  left);
    question += '\n';
    question += part.data();
  }
  return question;
}

// La question partagée par « Supprimer », « Retourner », « Récupérer tout » et
// « Supprimer tout »,
// demandée au niveau de la fenêtre : depuis un volet, elle se nommerait dans
// une autre pile d'identités et ne paraîtrait jamais.
void DrawConfirmation(MailboxWindowState& state, const MailboxView& view, const SkinArt& art,
                      MailboxAction& action) {
  const MailRowView* row =
      state.selected.has_value() ? FindRow(view.rows, *state.selected) : nullptr;
  std::vector<MailRef> targets;
  std::string question;
  switch (state.confirm) {
    case MailConfirm::kDelete:
      question = core::Tr("Supprimer définitivement ce courrier ?");
      break;
    case MailConfirm::kReturn:
      question = core::Tr("Retourner ce courrier à son expéditeur ?");
      break;
    case MailConfirm::kClaimAll:
      targets = MailClaimTargets(view.rows, state);
      question = ClaimAllQuestion(targets.size());
      break;
    case MailConfirm::kDeleteAll:
      targets = MailDeleteTargets(view.rows, state);
      question = DeleteAllQuestion(targets.size(), MailClaimTargets(view.rows, state).size());
      break;
    case MailConfirm::kNone:
      break;
  }
  const bool needs_row =
      state.confirm == MailConfirm::kDelete || state.confirm == MailConfirm::kReturn;
  const bool needs_targets =
      state.confirm == MailConfirm::kClaimAll || state.confirm == MailConfirm::kDeleteAll;
  if ((needs_row && row == nullptr) || (needs_targets && targets.empty())) {
    state.confirm = MailConfirm::kNone;
  }
  const std::string name = mail::ConfirmModalName(kConfirmId);
  if (state.confirm == MailConfirm::kNone) {
    mail::CloseModal(name.c_str());
    return;
  }
  if (!ImGui::IsPopupOpen(name.c_str())) {
    AskModal(name.c_str());
  }
  switch (mail::DrawConfirmModal(name, question, art)) {
    case ModalAnswer::kYes:
      if (state.confirm == MailConfirm::kDelete) {
        action.erase = row->ref;
      } else if (state.confirm == MailConfirm::kReturn) {
        action.return_to_sender = row->ref;
      } else if (state.confirm == MailConfirm::kDeleteAll) {
        action.erase_all = std::move(targets);
      } else {
        action.claim_all = std::move(targets);
      }
      state.confirm = MailConfirm::kNone;
      break;
    case ModalAnswer::kNo:
      state.confirm = MailConfirm::kNone;
      break;
    case ModalAnswer::kWaiting:
      break;
  }
}

[[nodiscard]] std::string MailboxTitle(int unread) {
  std::array<char, kLabelBytes> title{};
  if (unread > 1) {
    std::snprintf(title.data(), title.size(), core::Tr("Courrier (%d non lus)"), unread);
  } else if (unread == 1) {
    std::snprintf(title.data(), title.size(), core::Tr("Courrier (%d non lu)"), unread);
  } else {
    std::snprintf(title.data(), title.size(), "%s", core::Tr("Courrier"));
  }
  return std::string(title.data()) + "###" + kMailboxWindowId;
}

}  // namespace

namespace mail {

TradeItemGesture DrawItemRow(const MailItemView& line) {
  ImGui::BeginGroup();
  ImGui::PushStyleColor(ImGuiCol_Text, kTradeBlackText);
  TradeItemGesture gesture = DrawTradeItem(line.item);
  ImGui::PopStyleColor();
  if (line.amount > 1) {
    ImGui::SameLine();
    ImGui::TextDisabled("x%d", line.amount);
  }
  ImGui::EndGroup();
  AddLastItemGesture(gesture);
  return gesture;
}

void CloseModal(const char* name) {
  const ImGuiContext& context = *GImGui;
  const ImGuiID popup_id = ImGui::GetID(name);
  for (int level = 0; level < context.OpenPopupStack.Size; ++level) {
    if (context.OpenPopupStack[level].PopupId == popup_id) {
      ImGui::ClosePopupToLevel(level, true);
      return;
    }
  }
}

std::string ConfirmModalName(std::string_view id) {
  std::string name(core::Tr("Confirmation"));
  name += "###";
  name += id;
  return name;
}

ModalAnswer DrawConfirmModal(const std::string& name, std::string_view question,
                             const SkinArt& art) {
  if (!BeginSkinnedModal(name.c_str(), art)) {
    return ModalAnswer::kWaiting;
  }
  ModalAnswer answer = ModalAnswer::kWaiting;
  Unformatted(question);
  ImGui::Spacing();
  const ImVec2 button(kConfirmButtonPixels * Scale(), 0.0F);
  if (SkinButton(core::Tr("Confirmer"), art, button)) {
    answer = ModalAnswer::kYes;
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art, button)) {
    answer = ModalAnswer::kNo;
  }
  // ⚠ ÉCHAP VAUT « NON » : la boîte se referme d'elle-même, et la question doit
  // tomber avec elle, sans quoi elle se reposerait à l'image suivante.
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    answer = ModalAnswer::kNo;
  }
  if (answer != ModalAnswer::kWaiting) {
    ImGui::CloseCurrentPopup();
  }
  EndSkinnedModal();
  return answer;
}

}  // namespace mail

MailboxTabs CountMailboxTabs(std::span<const MailRowView> rows) {
  MailboxTabs tabs;
  for (const MailRowView& row : rows) {
    const auto box = static_cast<std::size_t>(row.ref.box);
    if (box < static_cast<std::size_t>(MailboxTab::kAll)) {
      ++tabs.counts[box];
    }
    ++tabs.counts[static_cast<std::size_t>(MailboxTab::kAll)];
  }
  tabs.show_account = tabs.counts[static_cast<std::size_t>(MailboxTab::kAccount)] > 0;
  return tabs;
}

void SettleMailboxTab(MailboxWindowState& state, const MailboxTabs& tabs) {
  if (!tabs.show_account && state.tab == MailboxTab::kAccount) {
    state.tab = MailboxTab::kNormal;
    Forget(state);
  }
}

bool MailRowShown(const MailRowView& row, MailboxTab tab, std::string_view filter) {
  if (tab != MailboxTab::kAll && static_cast<MailboxTab>(row.ref.box) != tab) {
    return false;
  }
  return filter.empty() || ContainsFolded(row.sender, filter) || ContainsFolded(row.title, filter);
}

std::string MailExpiryLabel(std::optional<std::int64_t> seconds_left) {
  if (!seconds_left.has_value()) {
    return {};
  }
  if (*seconds_left <= 0) {
    return core::Tr("expiré");
  }
  std::array<char, kLabelBytes> label{};
  const std::int64_t days = *seconds_left / kSecondsPerDay;
  if (days >= 1) {
    std::snprintf(label.data(), label.size(), core::Tr("J-%lld"), static_cast<long long>(days));
  } else {
    const std::int64_t hours = std::max<std::int64_t>(*seconds_left / kSecondsPerHour, 1);
    std::snprintf(label.data(), label.size(), core::Tr("%lld h"), static_cast<long long>(hours));
  }
  return label.data();
}

bool MailExpiresSoon(const MailRowView& row) {
  return (row.carried & (kMailCarriesZeny | kMailCarriesItems)) != 0 &&
         row.expire_seconds_left.has_value() && *row.expire_seconds_left < kMailExpirySoonSeconds;
}

bool MailCanReturn(const MailRowView& row) {
  return row.ref.box == MailBox::kNormal && (row.carried & kMailFromSystem) == 0;
}

std::vector<MailRef> MailClaimTargets(std::span<const MailRowView> rows,
                                      const MailboxWindowState& state) {
  std::vector<MailRef> targets;
  const std::string_view filter = FilterOf(state);
  for (const MailRowView& row : rows) {
    if ((row.carried & (kMailCarriesZeny | kMailCarriesItems)) != 0 &&
        MailRowShown(row, state.tab, filter)) {
      targets.push_back(row.ref);
    }
  }
  return targets;
}

std::vector<MailRef> MailDeleteTargets(std::span<const MailRowView> rows,
                                       const MailboxWindowState& state) {
  std::vector<MailRef> targets;
  const std::string_view filter = FilterOf(state);
  for (const MailRowView& row : rows) {
    if ((row.carried & (kMailCarriesZeny | kMailCarriesItems)) == 0 &&
        MailRowShown(row, state.tab, filter)) {
      targets.push_back(row.ref);
    }
  }
  return targets;
}

MailboxAction DrawMailboxWindow(MailboxWindowState& state, const MailboxView& view,
                                const SkinArt& art, ItemLinkFacts link_facts) {
  MailboxAction action;
  if (!state.open) {
    Forget(state);
    return action;
  }
  const float scale = Scale();
  ImGui::SetNextWindowPos(ImVec2(kFirstLeftPixels, kFirstTopPixels), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels * scale, kFirstHeightPixels * scale),
                           ImGuiCond_FirstUseEver);

  // 🔴 LE BLOC DE LECTURE S'AJOUTE EN DELTA à la hauteur mesurée, et se retire
  // d'autant : un joueur qui a redimensionné sa fenêtre garde sa taille. La
  // largeur se repasse telle quelle — une composante nulle demanderait à ImGui
  // d'ajuster cet axe, et écraserait la largeur choisie.
  const bool want_detail = state.selected.has_value();
  if (want_detail != state.detail_shown) {
    const float delta = kDetailHeightPixels * scale;
    const float height = want_detail ? state.last_height + delta : state.last_height - delta;
    if (state.last_height > 0.0F && state.last_width > 0.0F && height > 0.0F) {
      ImGui::SetNextWindowSize(ImVec2(state.last_width, height), ImGuiCond_Always);
    }
    state.detail_shown = want_detail;
  }

  const std::string title = MailboxTitle(view.unread);
  PoseBugReportButton(state.bug_report);
  const bool body_shown = BeginSkinnedWindow(title.c_str(), &state.open, art, SkinnedTitle::kBar,
                                             ImGuiWindowFlags_NoCollapse);
  action.report_bug = state.bug_report && TitleActionClicked(0);
  if (body_shown && state.open) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, mail::kChildRoundingPixels * scale);
    DrawList(state, view, art, action);
    if (state.selected.has_value()) {
      ImGui::Separator();
      DrawDetail(state, view, art, action);
    } else {
      ImGui::TextDisabled("%s", core::Tr("Clique sur un courrier pour le lire."));
    }
    DrawConfirmation(state, view, art, action);
    if (const ChatLinkAsk chosen = DrawTradeItemMenu(kItemMenuId, state.item_menu, link_facts);
        chosen.gesture != ChatLinkGesture::kNothing) {
      action.described_item = static_cast<int>(chosen.token);
      action.item_gesture = chosen.gesture;
    }
    ImGui::PopStyleVar();
    // La mesure de fin d'image sert de base à l'agrandissement, et suit donc
    // les redimensionnements du joueur.
    state.last_width = ImGui::GetWindowWidth();
    state.last_height = ImGui::GetWindowHeight();
  }
  EndSkinnedWindow();
  if (!state.open) {
    action = MailboxAction{};
    action.close = true;
    Forget(state);
  }
  return action;
}

}  // namespace ro::ui
