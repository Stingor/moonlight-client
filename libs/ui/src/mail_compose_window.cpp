#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/ui/mail_window.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "item_grid.h"
#include "mail_parts.h"

namespace ro::ui {
namespace {

using mail::Scale;

// ── LES MESURES ─────────────────────────────────────────────────────────────
//
// Celles de la fenêtre d'écriture.
constexpr float kFirstWidthPixels = 430.0F;
constexpr float kFirstHeightPixels = 470.0F;
constexpr float kRecipientFieldPixels = 200.0F;
constexpr float kMessageHeightPixels = 130.0F;
constexpr float kZenyFieldPixels = 160.0F;
constexpr float kAttachmentsHeightPixels = 92.0F;
constexpr float kSendButtonPixels = 130.0F;
constexpr float kCancelButtonPixels = 110.0F;

// La place d'une phrase composée d'un nom de personnage.
constexpr std::size_t kSentenceBytes = 256;

constexpr std::string_view kSendConfirmId = "rodex_send_confirm";

// Les octets de suite d'un caractère UTF-8 : 10xxxxxx.
constexpr unsigned char kUtf8ContinuationMask = 0xC0;
constexpr unsigned char kUtf8ContinuationBits = 0x80;

// La saisie d'un tampon, jusqu'à son zéro.
[[nodiscard]] std::string_view Typed(std::span<const char> buffer) {
  const auto end = std::ranges::find(buffer, '\0');
  return std::string_view(buffer.data(), static_cast<std::size_t>(end - buffer.begin()));
}

[[nodiscard]] const MailBagView* FindBag(std::span<const MailBagView> bag,
                                         std::uint16_t client_slot) {
  const auto found = std::ranges::find_if(
      bag, [client_slot](const MailBagView& on) { return on.client_slot == client_slot; });
  return found == bag.end() ? nullptr : &*found;
}

// Le plus long début de `text` qui tient en `most` octets sans couper un
// caractère UTF-8.
[[nodiscard]] std::string_view Utf8Prefix(std::string_view text, std::size_t most) {
  if (text.size() <= most) {
    return text;
  }
  std::size_t end = most;
  while (end > 0 &&
         (static_cast<unsigned char>(text[end]) & kUtf8ContinuationMask) == kUtf8ContinuationBits) {
    --end;
  }
  return text.substr(0, end);
}

// « Untel — Archbishop, niveau 99 » : le nom réémis par le serveur — une
// coquille se repère sans relire son champ —, le job et le niveau, qui disent
// si c'est la bonne personne.
[[nodiscard]] std::string WhoLine(const MailComposeWindowState& state,
                                  const MailComposeView& view) {
  const std::string_view who =
      view.checked_name.empty() ? Typed(state.recipient) : view.checked_name;
  std::array<char, kSentenceBytes> line{};
  if (!view.checked_job.empty()) {
    std::snprintf(line.data(), line.size(), core::Tr("%.*s — %.*s, niveau %d"),
                  static_cast<int>(who.size()), who.data(),
                  static_cast<int>(view.checked_job.size()), view.checked_job.data(),
                  view.checked_level);
  } else {
    std::snprintf(line.data(), line.size(), core::Tr("%.*s — niveau %d"),
                  static_cast<int>(who.size()), who.data(), view.checked_level);
  }
  return line.data();
}

// Un compteur d'octets du fil, rouge au-delà de la borne.
void DrawByteCounter(std::size_t used, std::size_t most) {
  std::array<char, kSentenceBytes> counter{};
  std::snprintf(counter.data(), counter.size(), "%zu/%zu", used, most);
  if (used > most) {
    ImGui::TextColored(mail::kRefusalText, "%s", counter.data());
  } else {
    ImGui::TextDisabled("%s", counter.data());
  }
}

void DrawRecipient(MailComposeWindowState& state, const MailComposeView& view, const SkinArt& art,
                   MailComposeAction& action) {
  ImGui::TextColored(kTradeBlackText, "%s", core::Tr("Destinataire"));
  ImGui::SetNextItemWidth(kRecipientFieldPixels * Scale());
  ImGui::InputText("##rodex_to", state.recipient.data(), state.recipient.size());
  // Le nom se vérifie aussi tout seul, quand on quitte le champ après l'avoir
  // changé.
  if (ImGui::IsItemDeactivatedAfterEdit() && !Typed(state.recipient).empty()) {
    action.check = true;
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Vérifier"), art)) {
    action.check = true;
  }

  switch (view.check) {
    case MailRecipientCheck::kPending:
      ImGui::TextDisabled("%s", core::Tr("Vérification en cours..."));
      break;
    case MailRecipientCheck::kUnknown:
      ImGui::TextColored(mail::kRefusalText, "%s", core::Tr("Ce personnage n'existe pas."));
      break;
    case MailRecipientCheck::kFound: {
      const std::string who = WhoLine(state, view);
      ImGui::PushStyleColor(ImGuiCol_Text, kTradeBlackText);
      ImGui::TextUnformatted(who.c_str());
      ImGui::PopStyleColor();
      break;
    }
    case MailRecipientCheck::kNone:
      break;
  }
}

void DrawTexts(MailComposeWindowState& state, const MailComposeView& view) {
  ImGui::TextColored(kTradeBlackText, "%s", core::Tr("Sujet"));
  ImGui::SetNextItemWidth(-1.0F);
  ImGui::InputText("##rodex_subject", state.subject.data(), state.subject.size());
  DrawByteCounter(view.subject_wire_bytes, view.subject_max_bytes);
  ImGui::TextColored(kTradeBlackText, "%s", core::Tr("Message"));
  ImGui::InputTextMultiline("##rodex_body", state.message.data(), state.message.size(),
                            ImVec2(-1.0F, kMessageHeightPixels * Scale()));
  DrawByteCounter(view.message_wire_bytes, view.message_max_bytes);
}

void DrawZeny(MailComposeWindowState& state, const MailComposeView& view) {
  ImGui::TextColored(kTradeBlackText, "%s", core::Tr("Zeny à joindre"));
  ImGui::SetNextItemWidth(kZenyFieldPixels * Scale());
  ImGui::InputScalar("##rodex_zeny", ImGuiDataType_S64, &state.zeny);
  state.zeny = std::max<std::int64_t>(state.zeny, 0);
  // Les frais sont ceux que `app` a calculés : le taux et le prix par pièce
  // jointe sont la configuration du serveur.
  ImGui::SameLine();
  if (view.fee > 0) {
    ImGui::TextDisabled(core::Tr("frais : %lld z"), static_cast<long long>(view.fee));
  } else {
    ImGui::TextDisabled("%s", core::Tr("(des frais d'envoi s'appliquent)"));
  }
  if (view.purse_short) {
    ImGui::SameLine();
    ImGui::TextColored(mail::kRefusalText, core::Tr("total : %lld z"),
                       static_cast<long long>(view.total_debit));
  }
}

// UN ITEM DU SAC LÂCHÉ SUR LES PIÈCES JOINTES : seul le sac en donne, et pas
// ce qu'on porte. Une pile demande sa quantité ; une pièce seule se joint.
void AcceptBagDrop(MailComposeWindowState& state, const MailComposeView& view,
                   MailComposeAction& action) {
  if (!ImGui::BeginDragDropTarget()) {
    return;
  }
  if (const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(kItemDragPayload);
      dropped != nullptr && dropped->DataSize == sizeof(ItemDrag)) {
    ItemDrag carried;
    std::memcpy(&carried, dropped->Data, sizeof(carried));
    const MailBagView* bag = FindBag(view.bag, carried.client_slot);
    if (carried.from == ItemHolder::kInventory && !carried.worn && bag != nullptr &&
        bag->amount > 0) {
      if (bag->amount > 1) {
        state.asking_amount = true;
        state.asking_slot = bag->client_slot;
        state.asking_value = bag->amount;
      } else {
        action.attach = MailAttachmentMove{.client_slot = bag->client_slot, .amount = 1};
      }
    }
  }
  ImGui::EndDragDropTarget();
}

void DrawAttachments(MailComposeWindowState& state, const MailComposeView& view, const SkinArt& art,
                     MailComposeAction& action) {
  ImGui::TextColored(kTradeBlackText, core::Tr("Pièces jointes (%d/%d)"),
                     static_cast<int>(view.attachments.size()), view.attachment_capacity);
  ImGui::BeginChild("rodex_attach", ImVec2(0.0F, kAttachmentsHeightPixels * Scale()),
                    ImGuiChildFlags_Borders);
  for (std::size_t index = 0; index < view.attachments.size(); ++index) {
    const MailAttachmentView& attachment = view.attachments[index];
    ImGui::PushID(static_cast<int>(index));
    const TradeItemGesture gesture = mail::DrawItemRow(attachment.line);
    if (gesture.hovered) {
      action.hovered_item = static_cast<int>(index);
    }
    if (gesture.describe) {
      action.described_item = static_cast<int>(index);
    }
    ImGui::SameLine();
    if (SkinButton(core::Tr("Retirer"), art)) {
      action.remove = MailAttachmentMove{.client_slot = attachment.client_slot,
                                         .amount = attachment.line.amount};
    }
    ImGui::PopID();
  }
  if (view.attachments.empty()) {
    ImGui::TextDisabled("%s", core::Tr("(vide — glisse un item de l'inventory ici)"));
  }
  ImGui::EndChild();
  // ⚠ LA CIBLE EST LE VOLET QU'ON VIENT DE FERMER.
  AcceptBagDrop(state, view, action);
}

void DrawAskAmount(MailComposeWindowState& state, const MailComposeView& view, const SkinArt& art,
                   MailComposeAction& action) {
  const MailBagView* bag = FindBag(view.bag, state.asking_slot);
  // La pile a disparu le temps de répondre : la question n'a plus d'objet.
  if (bag == nullptr || bag->amount <= 0) {
    state.asking_amount = false;
    return;
  }
  const AmountAnswer given =
      DrawCountPrompt(state.asking_value, bag->amount, core::Tr("Joindre au courrier"), art);
  if (given.chosen) {
    action.attach = MailAttachmentMove{.client_slot = bag->client_slot,
                                       .amount = std::clamp(given.amount, 1, bag->amount)};
  }
  if (given.closed) {
    state.asking_amount = false;
  }
}

[[nodiscard]] MailComposeSend SendOf(const MailComposeWindowState& state) {
  return MailComposeSend{.recipient = std::string(Typed(state.recipient)),
                         .subject = std::string(Typed(state.subject)),
                         .message = std::string(Typed(state.message)),
                         .zeny = state.zeny};
}

void DrawSendButton(MailComposeWindowState& state, const MailComposeView& view, const SkinArt& art,
                    MailComposeAction& action) {
  const bool held = view.send_in_flight || view.purse_short || state.confirming_send;
  ImGui::BeginDisabled(held);
  if (SkinButton(core::Tr("Envoyer"), art, ImVec2(kSendButtonPixels * Scale(), 0.0F)) && !held) {
    // Un envoi qui emporte quelque chose passe par le récapitulatif.
    if (MailSendCarries(state, view)) {
      state.confirming_send = true;
    } else {
      action.send = SendOf(state);
    }
  }
  ImGui::EndDisabled();
  // ⚠ `SkinTooltip` REDEMANDE LE SURVOL SANS `AllowWhenDisabled`, et un bouton
  // grisé n'est jamais survolé à ses yeux : la bulle se pose donc ici.
  if (view.purse_short && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltipFormat("%s",
                      core::Tr("Ta bourse ne couvre pas le zeny joint et les frais d'envoi."));
  }
}

// Le récapitulatif, demandé au niveau de la fenêtre : depuis un volet, il se
// nommerait dans une autre pile d'identités et ne paraîtrait jamais.
void DrawSendConfirmation(MailComposeWindowState& state, const MailComposeView& view,
                          const SkinArt& art, MailComposeAction& action) {
  if (state.confirming_send && (view.send_in_flight || view.purse_short)) {
    state.confirming_send = false;
  }
  const std::string name = mail::ConfirmModalName(kSendConfirmId);
  if (!state.confirming_send) {
    mail::CloseModal(name.c_str());
    return;
  }
  if (!ImGui::IsPopupOpen(name.c_str())) {
    AskModal(name.c_str());
  }
  const std::string recap = MailSendRecapText(state, view);
  switch (mail::DrawConfirmModal(name, recap, art)) {
    case ModalAnswer::kYes:
      action.send = SendOf(state);
      state.confirming_send = false;
      break;
    case ModalAnswer::kNo:
      state.confirming_send = false;
      break;
    case ModalAnswer::kWaiting:
      break;
  }
}

void DrawBody(MailComposeWindowState& state, const MailComposeView& view, const SkinArt& art,
              MailComposeAction& action) {
  DrawRecipient(state, view, art, action);
  DrawTexts(state, view);
  DrawZeny(state, view);
  DrawAttachments(state, view, art, action);
  if (state.asking_amount) {
    DrawAskAmount(state, view, art, action);
  }

  ImGui::Separator();
  DrawSendButton(state, view, art, action);
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art, ImVec2(kCancelButtonPixels * Scale(), 0.0F))) {
    state.open = false;
    return;
  }
  if (!view.error.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, mail::kRefusalText);
    ImGui::TextUnformatted(view.error.data(), view.error.data() + view.error.size());
    ImGui::PopStyleColor();
  }
  DrawSendConfirmation(state, view, art, action);
}

// « Libellé : valeur » — le libellé traduit, la valeur telle quelle.
void AppendField(std::string& text, const char* label, std::string_view value) {
  text.append(label);
  text.push_back(' ');
  text.append(value);
}

}  // namespace

void OpenMailCompose(MailComposeWindowState& state, std::string_view recipient) {
  state = MailComposeWindowState{};
  state.open = true;
  const std::string_view kept = Utf8Prefix(recipient, state.recipient.size() - 1);
  kept.copy(state.recipient.data(), kept.size());
}

bool MailSendCarries(const MailComposeWindowState& state, const MailComposeView& view) {
  return !view.attachments.empty() || state.zeny > 0;
}

std::string MailSendRecapText(const MailComposeWindowState& state, const MailComposeView& view) {
  std::string text;
  if (view.check == MailRecipientCheck::kFound) {
    AppendField(text, core::Tr("Destinataire :"), WhoLine(state, view));
  } else {
    AppendField(text, core::Tr("Destinataire :"), Typed(state.recipient));
    text.append("\n");
    text.append(core::Tr("(!) Destinataire non vérifié."));
  }
  if (!view.attachments.empty()) {
    text.append("\n\n");
    text.append(core::Tr("Items joints :"));
    for (const MailAttachmentView& attachment : view.attachments) {
      const std::string_view name = attachment.line.item.name;
      text.append("\n   ");
      text.append(name.empty() ? std::string_view("?") : name);
      text.append(" x");
      text.append(TradeAmountText(attachment.line.amount, {}));
    }
  }
  text.append("\n\n");
  if (state.zeny > 0) {
    AppendField(text, core::Tr("Zeny joint :"), TradeAmountText(state.zeny, "z"));
    text.append("\n");
  }
  AppendField(text, core::Tr("Frais d'envoi :"), TradeAmountText(view.fee, "z"));
  text.append("\n");
  AppendField(text, core::Tr("Total débité :"), TradeAmountText(view.total_debit, "z"));
  text.append("\n\n");
  text.append(core::Tr("Envoyer ce courrier ?"));
  return text;
}

MailComposeAction DrawMailComposeWindow(MailComposeWindowState& state, const MailComposeView& view,
                                        const SkinArt& art) {
  MailComposeAction action;
  if (!state.open) {
    // Une question laissée en suspens ne survit pas à la fenêtre qui la posait.
    state.confirming_send = false;
    state.asking_amount = false;
    return action;
  }
  const float scale = Scale();
  // Au centre à chaque ouverture, à la taille de départ la première fois.
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels * scale, kFirstHeightPixels * scale),
                           ImGuiCond_FirstUseEver);
  const std::string title =
      std::string(core::Tr("Écrire un courrier")) + "###" + kMailComposeWindowId;
  const bool body_shown = BeginSkinnedWindow(title.c_str(), &state.open, art, SkinnedTitle::kBar,
                                             ImGuiWindowFlags_NoCollapse);
  if (body_shown && state.open) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, mail::kChildRoundingPixels * scale);
    DrawBody(state, view, art, action);
    ImGui::PopStyleVar();
  }
  EndSkinnedWindow();
  if (!state.open) {
    // 🔴 FERMER, C'EST ANNULER : rien d'autre ne part avec.
    action = MailComposeAction{};
    action.cancel = true;
    state.confirming_send = false;
    state.asking_amount = false;
  }
  return action;
}

}  // namespace ro::ui
