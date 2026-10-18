// Le courrier, dans la boucle. Voir l'en-tête.

#include "game/mail_stage.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/text.h"
#include "ro/net/mail.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"

#include "chat/banned_input.h"
#include "game/braces_filled.h"
#include "game/mail_box_move.h"
#include "game/mail_bug_report.h"
#include "items/inventory_view.h"

namespace moonclient {
namespace {

void Report(const char* what, const ro::core::Result<void>& sent) {
  if (!sent) {
    ro::core::LogWarning("courrier : {} non émis — {}", what, ro::core::Describe(sent.error()));
  }
}

// Le texte d'un tampon de saisie, jusqu'à son premier zéro.
template <std::size_t kBytes>
std::string_view Typed(const std::array<char, kBytes>& buffer) {
  const auto end = std::ranges::find(buffer, '\0');
  return {buffer.data(), static_cast<std::size_t>(end - buffer.begin())};
}

std::optional<std::int64_t> PurseOf(const ro::sim::World& world) {
  const std::int64_t zeny = world.player_status().zeny;
  return zeny == ro::sim::kUnknownStat ? std::nullopt : std::optional<std::int64_t>(zeny);
}

const char* AttachFaultText(ro::sim::MailAttachFault fault) {
  switch (fault) {
    case ro::sim::MailAttachFault::kNone:
      return "";
    case ro::sim::MailAttachFault::kNotWriting:
      return ro::core::Tr("Aucun courrier n'est en cours d'écriture.");
    case ro::sim::MailAttachFault::kSendInFlight:
      return ro::core::Tr("Un envoi attend encore la réponse du serveur.");
    case ro::sim::MailAttachFault::kReopenDue:
      return ro::core::Tr("L'écriture se rouvre après l'envoi précédent : patiente un instant.");
    case ro::sim::MailAttachFault::kAttachInFlight:
      return ro::core::Tr("Un ajout attend encore la réponse du serveur.");
    case ro::sim::MailAttachFault::kBadSlot:
      return ro::core::Tr("Cet item ne peut pas être joint.");
    case ro::sim::MailAttachFault::kEquipped:
      return ro::core::Tr("Porté : déséquipe-le d'abord.");
    case ro::sim::MailAttachFault::kRented:
      return ro::core::Tr("Un item loué ne s'envoie pas.");
    case ro::sim::MailAttachFault::kAllAttached:
      return ro::core::Tr("Toute la pile est déjà jointe.");
    case ro::sim::MailAttachFault::kBadAmount:
      return ro::core::Tr("Quantité hors de la pile.");
    case ro::sim::MailAttachFault::kAttachmentsFull:
      return ro::core::Tr("Cinq items différents au plus par courrier.");
  }
  return "";
}

const char* SendFaultText(ro::sim::MailSendFault fault) {
  switch (fault) {
    case ro::sim::MailSendFault::kNone:
      return "";
    case ro::sim::MailSendFault::kNotWriting:
      return ro::core::Tr("L'écriture n'est pas ouverte chez le serveur : patiente un instant.");
    case ro::sim::MailSendFault::kSendInFlight:
      return ro::core::Tr("Un envoi attend encore la réponse du serveur.");
    case ro::sim::MailSendFault::kReopenDue:
      return ro::core::Tr("L'écriture se rouvre après l'envoi précédent : patiente un instant.");
    case ro::sim::MailSendFault::kAttachInFlight:
      return ro::core::Tr("Un ajout attend encore la réponse du serveur.");
    case ro::sim::MailSendFault::kRecipientEmpty:
      return ro::core::Tr("Indique un destinataire.");
    case ro::sim::MailSendFault::kRecipientTooLong:
      return ro::core::Tr("Nom du destinataire trop long (23 caractères maximum).");
    case ro::sim::MailSendFault::kTitleEmpty:
      return ro::core::Tr("Le sujet ne peut pas être vide.");
    case ro::sim::MailSendFault::kTitleTooLong:
      return ro::core::Tr("Sujet trop long (39 caractères maximum).");
    case ro::sim::MailSendFault::kBodyTooLong:
      return ro::core::Tr("Message trop long (499 caractères maximum).");
    case ro::sim::MailSendFault::kEmbeddedNul:
      return ro::core::Tr("Le sujet ou le message contient un caractère nul.");
    case ro::sim::MailSendFault::kZenyNegative:
      return ro::core::Tr("Le zeny joint ne peut pas être négatif.");
    case ro::sim::MailSendFault::kZenyAboveMost:
      return ro::core::Tr("Le zeny joint dépasse ce qu'une bourse peut tenir.");
    case ro::sim::MailSendFault::kPurseUnknown:
      return ro::core::Tr("Ta bourse n'est pas encore connue : réessaie dans un instant.");
    case ro::sim::MailSendFault::kPurseShort:
      return ro::core::Tr("Ta bourse ne couvre pas le zeny joint et les frais d'envoi.");
  }
  return "";
}

std::string SendResultText(std::uint8_t code) {
  switch (code) {
    case ro::sim::kMailSendSucceeded:
      return ro::core::Tr("Courrier envoyé.");
    case ro::sim::kMailSendDailyQuota:
      return ro::core::Tr("Envoi refusé : le nombre de courriers du jour est atteint.");
    case ro::sim::kMailSendNoRecipient:
      return ro::core::Tr("Envoi refusé : ce destinataire n'existe pas.");
    case ro::sim::kMailSendBlocked:
      return ro::core::Tr("Envoi refusé par le destinataire.");
    case ro::sim::kMailSilent:
      return ro::core::Tr("Le serveur n'a pas répondu à l'envoi.");
    // ⚠ CE SERVEUR RÉPOND AUSSI PAR CE CODE À UN DESTINATAIRE INTROUVABLE : le
    // code qui lui est propre n'y est jamais émis.
    case ro::sim::kMailSendFailed:
      return std::string(ro::core::Tr("Le serveur a refusé l'envoi.")) + " " +
             ro::core::Tr(
                 "Vérifie aussi le nom du destinataire : un nom introuvable reçoit la "
                 "même réponse.");
    case ro::sim::kMailSendItemFailed:
    default:
      return ro::core::Tr("Le serveur a refusé l'envoi.");
  }
}

std::string AttachRefusalText(std::uint8_t code, std::string_view item) {
  switch (code) {
    case ro::sim::kMailAttachOverWeight:
      return BracesFilled(
          ro::core::Tr("{} ne peut pas être joint : le courrier serait trop lourd."), {item});
    case ro::sim::kMailAttachNoRoom:
      return ro::core::Tr("Plus de place pour une pièce jointe.");
    case ro::sim::kMailAttachNotTradable:
      return BracesFilled(ro::core::Tr("{} ne peut pas être envoyé par courrier."), {item});
    case ro::sim::kMailSilent:
      return BracesFilled(
          ro::core::Tr("Le serveur n'a pas répondu à l'ajout de {} : il est tenu pour refusé."),
          {item});
    case ro::sim::kMailAttachFailed:
    default:
      return BracesFilled(ro::core::Tr("Le serveur a refusé de joindre {}."), {item});
  }
}

std::string ClaimText(ro::sim::MailClaim claim) {
  return claim == ro::sim::MailClaim::kZeny ? ro::core::Tr("Zeny du courrier récupéré.")
                                            : ro::core::Tr("Items du courrier récupérés.");
}

std::string ClaimRefusalText(ro::sim::MailClaim claim, std::uint8_t code) {
  switch (code) {
    case ro::sim::kMailClaimOverWeight:
      return ro::core::Tr("Récupération refusée : poids ou place insuffisants dans l'inventory.");
    case ro::sim::kMailSilent:
      return ro::core::Tr("Le serveur n'a pas répondu à la récupération.");
    case ro::sim::kMailClaimFailed:
    default:
      return claim == ro::sim::MailClaim::kZeny
                 ? ro::core::Tr("Récupération refusée : ta bourse ne peut pas tenir ce zeny.")
                 : ro::core::Tr("Récupération des items refusée.");
  }
}

}  // namespace

std::int64_t MailNowUnixSeconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

void MailStage::See(const ro::vfs::ItemTable* items, ro::render::ItemIcons* icons,
                    const ro::vfs::ItemWords* words, const ro::vfs::JobNames* jobs,
                    ro::render::GameImages* images) noexcept {
  items_.See(items, icons, words);
  jobs_ = jobs;
  images_ = images;
  icons_loaded_ = false;
  icons_ = {};
}

void MailStage::StartSession() {
  mailbox_ = ro::sim::Mailbox();
  flow_ = MailWriteFlow();
  news_said_ = mailbox_.last_news_seq();
  write_seq_seen_ = mailbox_.write_seq();
  granted_recipient_.clear();
  granted_recipient_wire_.clear();
  box_shown_ = false;
  toggle_asked_ = false;
  compose_asked_.reset();
  queued_attaches_.clear();
  send_carried_ = false;
  item_use_held_ = false;
  deal_held_ = false;
  compose_error_.clear();
  hovered_ = {};
  described_.reset();
  bug_report_.reset();
}

ro::net::FrameOutcome MailStage::ApplyFrame(const ro::net::Frame& frame, std::int64_t now_unix_s) {
  return ro::net::ApplyMailFrame(frame, mailbox_, now_unix_s);
}

void MailStage::QueueAttach(std::uint16_t client_slot, int amount) {
  queued_attaches_.push_back(QueuedAttach{.client_slot = client_slot, .amount = amount});
}

bool MailStage::HoldsWorld() const noexcept {
  return flow_.shown() || mailbox_.writing() || mailbox_.cancel_due();
}

bool MailStage::HoldsItemUse() {
  if (!HoldsWorld()) {
    return false;
  }
  item_use_held_ = true;
  return true;
}

bool MailStage::HoldsDealProposal() {
  if (!HoldsWorld()) {
    return false;
  }
  deal_held_ = true;
  return true;
}

std::int32_t MailStage::WithheldAt(std::uint16_t client_slot) const noexcept {
  return mailbox_.WithheldAt(client_slot);
}

std::optional<std::string> MailStage::RecipientWire(std::string_view typed_utf8) const {
  if (!granted_recipient_wire_.empty() && typed_utf8 == granted_recipient_) {
    return granted_recipient_wire_;
  }
  return ro::net::MailRecipientWire(typed_utf8);
}

std::optional<TradeItemFocus> MailStage::TakeDescribed() noexcept {
  return std::exchange(described_, std::nullopt);
}

std::optional<BugReportContext> MailStage::TakeBugReport() noexcept {
  return std::exchange(bug_report_, std::nullopt);
}

void MailStage::Note(std::span<const ro::sim::InventoryItem> items, int index, bool hovered,
                     bool described, ro::ui::ChatLinkGesture gesture) {
  if (index < 0 || static_cast<std::size_t>(index) >= items.size()) {
    return;
  }
  const ro::sim::InventoryItem& item = items[static_cast<std::size_t>(index)];
  // UNE PIÈCE JOINTE SE DÉCRIT PAR CE QU'ELLE PORTE, comme une offre d'échange :
  // reçue, elle n'est pas dans le sac.
  const TradeItemFocus focus{.item_id = item.item_id, .link = ItemLinkOf(item), .gesture = gesture};
  if (hovered) {
    hovered_ = focus;
  }
  if (described) {
    described_ = focus;
  }
}

// ── LES DEMANDES ───────────────────────────────────────────────────────────

void MailStage::RequestList(const TradeContext& context) {
  Report("demande de liste",
         ro::net::RequestMailList(context.link, mailbox_.NewestIdIn(ro::sim::MailBox::kNormal),
                                  mailbox_.NewestIdIn(ro::sim::MailBox::kReturned),
                                  mailbox_.NewestIdIn(ro::sim::MailBox::kAccount)));
  mailbox_.NoteListRequested(context.now_ms);
}

void MailStage::AskWrite(std::string_view recipient_wire, const TradeContext& context) {
  if (flow_.shown()) {
    Say(context, ro::core::Tr("Un courrier est déjà en cours d'écriture."), kTradeWarningColor);
    return;
  }
  // ⚠ UNE SEULE DEMANDE À LA FOIS : la seconde trouverait l'écriture déjà
  // ouverte, et son refus se lirait comme un coffre ouvert.
  if (flow_.asking()) {
    return;
  }
  if (auto sent = ro::net::RequestOpenWrite(context.link, recipient_wire); !sent) {
    Report("ouverture d'écriture", sent);
    Say(context,
        ro::core::Tr("Ce nom contient un caractère que le serveur ne connaît pas, ou dépasse 23 "
                     "caractères."),
        kTradeFailedColor);
    return;
  }
  flow_.NoteAsked(context.now_ms);
}

void MailStage::Claim(std::int64_t mail_id, const TradeContext& context) {
  const ro::sim::MailHeader* header = mailbox_.FindHeader(mail_id);
  if (header == nullptr) {
    return;
  }
  const bool items = header->has_items;
  const bool zeny = header->has_zeny;
  // Les items d'abord, puis le zeny.
  if (items && !mailbox_.ClaimInFlight(mail_id, ro::sim::MailClaim::kItems)) {
    Report("récupération des items",
           ro::net::RequestClaimItems(context.link, mailbox_, mail_id, context.now_ms));
  }
  if (zeny && !mailbox_.ClaimInFlight(mail_id, ro::sim::MailClaim::kZeny)) {
    Report("récupération du zeny",
           ro::net::RequestClaimZeny(context.link, mailbox_, mail_id, context.now_ms));
  }
}

void MailStage::Attach(std::uint16_t client_slot, int amount, const TradeContext& context) {
  // ⚠ LE GESTE SE REJUGE : le verdict dessiné a une image de retard, et la case
  // a pu changer de main entre-temps.
  const ro::sim::InventoryItem* held = context.world.inventory().Find(client_slot);
  if (held == nullptr) {
    return;
  }
  if (const ro::sim::MailAttachFault fault = mailbox_.CheckAttach(*held, amount);
      fault != ro::sim::MailAttachFault::kNone) {
    compose_error_ = AttachFaultText(fault);
    Say(context, compose_error_, kTradeFailedColor);
    return;
  }
  if (auto sent = ro::net::RequestAttachItem(context.link, mailbox_, *held, amount, context.now_ms);
      !sent) {
    Report("ajout de pièce jointe", sent);
    return;
  }
  compose_error_.clear();
}

void MailStage::Send(const ro::ui::MailComposeSend& send, const TradeContext& context) {
  compose_error_.clear();
  if (send.recipient.empty()) {
    compose_error_ = ro::core::Tr("Indique un destinataire.");
    return;
  }
  std::optional<std::string> recipient = RecipientWire(send.recipient);
  if (!recipient.has_value()) {
    compose_error_ =
        ro::core::Tr("Le nom du destinataire contient un caractère que le serveur ne connaît pas.");
    return;
  }
  const ro::net::MailWireFields fields{.recipient = std::move(*recipient),
                                       .title = ro::core::WireFromUtf8Text(send.subject),
                                       .body = ro::core::WireFromUtf8Text(send.message)};
  const std::optional<std::int64_t> purse = PurseOf(context.world);
  if (const ro::sim::MailSendFault fault =
          mailbox_.CheckSend(fields.recipient, fields.title, fields.body, send.zeny, purse);
      fault != ro::sim::MailSendFault::kNone) {
    compose_error_ = SendFaultText(fault);
    return;
  }
  // 🔴 L'OBJET PUIS LE CORPS : l'un ou l'autre qui porte un mot interdit, et
  // le courrier ne part pas.
  if (context.banned != nullptr) {
    for (const std::string& typed : {send.subject, send.message}) {
      if (std::optional<std::string> refused =
              BannedInputRefusal(*context.banned, typed, BannedInput::kMail);
          refused.has_value()) {
        compose_error_ = std::move(*refused);
        return;
      }
    }
  }
  const bool carried = mailbox_.attachment_count() > 0;
  if (auto sent = ro::net::RequestSendMail(context.link, mailbox_, fields, send.zeny, purse,
                                           context.now_ms);
      !sent) {
    Report("envoi", sent);
    compose_error_ = ro::core::Tr("Le serveur a refusé l'envoi.");
    return;
  }
  send_carried_ = carried;
}

// ── LES FENÊTRES ───────────────────────────────────────────────────────────

void MailStage::FollowAsks(MailWindows& windows, const TradeContext& context) {
  switch (MailBoxMoveFor(mailbox_.open(), std::exchange(toggle_asked_, false),
                         mailbox_.TakeWindowOrder())) {
    case MailBoxMove::kNone:
      break;
    case MailBoxMove::kOpen:
      mailbox_.NoteOpened(context.now_ms);
      windows.box.open = true;
      RequestList(context);
      break;
    case MailBoxMove::kClose:
      // ⚠ REFERMER N'ÉMET RIEN : le serveur ne tient aucune session de boîte.
      mailbox_.Close();
      break;
  }
  if (compose_asked_.has_value()) {
    const std::string recipient = std::move(*compose_asked_);
    compose_asked_.reset();
    // ⚠ CE NOM N'A QUE SON UTF-8 : il part dans l'encodage strict du fil, et un
    // caractère que ce dernier ne porte pas le rend inexprimable.
    if (const std::optional<std::string> wire = ro::net::MailRecipientWire(recipient);
        wire.has_value()) {
      AskWrite(*wire, context);
    } else {
      Say(context,
          ro::core::Tr("Ce nom contient un caractère que le serveur ne connaît pas, ou dépasse 23 "
                       "caractères."),
          kTradeFailedColor);
    }
  }
  for (const QueuedAttach& queued : std::exchange(queued_attaches_, {})) {
    Attach(queued.client_slot, queued.amount, context);
  }
}

void MailStage::FollowBox(MailWindows& windows, const TradeContext& context) {
  ro::ui::MailboxAction answer = std::exchange(windows.box_answer, {});
  // ⚠ AVANT TOUT CHANGEMENT : le rapport décrit l'image que le joueur a vue.
  if (answer.report_bug && box_shown_) {
    bug_report_ = MailboxBugReport(windows.box, box_view_);
  }
  // LA CROIX OU ÉCHAP : la boîte se ferme, sans un octet.
  if (box_shown_ && (answer.close || !windows.box.open)) {
    mailbox_.Close();
    return;
  }
  if (!mailbox_.open()) {
    return;
  }
  if (box_view_.content != nullptr) {
    Note(content_items_, answer.hovered_item, true, false);
    Note(content_items_, answer.described_item, false, true, answer.item_gesture);
  }
  if (answer.refresh) {
    RequestList(context);
  }
  if (answer.compose) {
    AskWrite("", context);
  }
  if (answer.reply.has_value()) {
    // L'EXPÉDITEUR REPART PAR SES OCTETS DU FIL : un nom se cherche octet par
    // octet, et l'aller-retour par l'UTF-8 ne le rend pas toujours tel quel.
    if (const ro::sim::MailHeader* header = mailbox_.FindHeader(answer.reply->mail_id);
        header != nullptr) {
      AskWrite(header->sender_wire, context);
    }
  }
  if (answer.read.has_value()) {
    Report("lecture",
           ro::net::RequestReadMail(context.link, mailbox_, answer.read->mail_id, context.now_ms));
  }
  if (answer.claim.has_value()) {
    Claim(answer.claim->mail_id, context);
  }
  for (const ro::ui::MailRef& ref : answer.claim_all) {
    Claim(ref.mail_id, context);
  }
  if (answer.return_to_sender.has_value()) {
    if (auto sent =
            ro::net::RequestReturnMail(context.link, mailbox_, answer.return_to_sender->mail_id);
        !sent) {
      Report("retour", sent);
      Say(context, ro::core::Tr("Ce courrier ne peut pas être retourné."), kTradeFailedColor);
    }
  }
  if (answer.erase.has_value()) {
    // ⚠ LE SERVEUR REFUSE EN SILENCE la suppression d'un courrier qui porte
    // encore quelque chose : le verbe la retient, et le refus se dit ici.
    if (auto sent = ro::net::RequestDeleteMail(context.link, mailbox_, answer.erase->mail_id);
        !sent) {
      Report("suppression", sent);
      Say(context, ro::core::Tr("Suppression refusée : récupère d'abord les pièces jointes."),
          kTradeFailedColor);
    }
  }
  // « SUPPRIMER TOUT » : une demande par courrier, le serveur n'en a pas de
  // groupée. La fenêtre n'a visé que des courriers vides ; le verbe le revérifie.
  for (const ro::ui::MailRef& ref : answer.erase_all) {
    Report("suppression", ro::net::RequestDeleteMail(context.link, mailbox_, ref.mail_id));
  }
}

void MailStage::FollowCompose(MailWindows& windows, const TradeContext& context) {
  const ro::ui::MailComposeAction answer = std::exchange(windows.compose_answer, {});
  if (!flow_.shown()) {
    return;
  }
  // ⚠ AVANT TOUT CHANGEMENT, comme celui de la boîte.
  if (answer.report_bug) {
    bug_report_ = MailComposeBugReport(windows.compose, compose_view_);
  }
  Note(attached_items_, answer.hovered_item, true, false);
  Note(attached_items_, answer.described_item, false, true, answer.item_gesture);
  // LA CROIX, ÉCHAP OU « ANNULER » : l'écriture s'annule chez le serveur, qui
  // rend les pièces jointes.
  if (answer.cancel || !windows.compose.open) {
    Obey(flow_.NoteClosedByPlayer(), windows, context);
    return;
  }
  if (answer.check) {
    if (const std::optional<std::string> wire = RecipientWire(Typed(windows.compose.recipient));
        wire.has_value()) {
      Report("vérification du destinataire",
             ro::net::RequestCheckRecipient(context.link, mailbox_, *wire, context.now_ms));
    } else {
      compose_error_ = ro::core::Tr(
          "Le nom du destinataire contient un caractère que le serveur ne connaît pas.");
    }
  }
  if (answer.attach.has_value()) {
    Attach(answer.attach->client_slot, answer.attach->amount, context);
  }
  if (answer.remove.has_value()) {
    Report("retrait de pièce jointe",
           ro::net::RequestDetachItem(context.link, mailbox_, answer.remove->client_slot,
                                      answer.remove->amount));
  }
  if (answer.send.has_value()) {
    Send(*answer.send, context);
  }
}

void MailStage::Obey(const MailWriteOrders& orders, MailWindows& windows,
                     const TradeContext& context) {
  if (orders.emit_cancel) {
    Report("annulation d'écriture", ro::net::RequestCancelWrite(context.link, mailbox_));
  }
  if (orders.emit_reopen) {
    // La réouverture s'adresse au destinataire saisi ; un nom que le fil ne
    // porte pas rouvre une écriture vierge — la saisie, elle, reste.
    const std::optional<std::string> wire = RecipientWire(Typed(windows.compose.recipient));
    if (auto sent = ro::net::RequestOpenWrite(context.link, wire.value_or(std::string())); !sent) {
      Report("réouverture d'écriture", ro::net::RequestOpenWrite(context.link, ""));
    }
  }
  if (orders.open_fresh || orders.keep_typing) {
    granted_recipient_ = mailbox_.write_recipient();
    granted_recipient_wire_ = mailbox_.write_recipient_wire();
  }
  if (orders.open_fresh) {
    ro::ui::OpenMailCompose(windows.compose, mailbox_.write_recipient());
    compose_error_.clear();
  }
  if (orders.keep_typing) {
    windows.compose.confirming_send = false;
    windows.compose.asking_amount = false;
    if (orders.outcome_unknown) {
      // 🔴 LE PREMIER ENVOI A PU PARTIR : le zeny ne repart pas de lui-même.
      windows.compose.zeny = 0;
      const std::string text = ro::core::Tr(
          "Sans réponse du serveur, le courrier a pu partir : vérifie ton inventory et ta bourse "
          "avant de renvoyer. Le zeny joint a été remis à zéro.");
      Say(context, text, kTradeWarningColor);
      compose_error_ += compose_error_.empty() ? text : "\n" + text;
    } else if (orders.redo_attachments) {
      const std::string text = ro::core::Tr(
          "Les items joints sont revenus dans l'inventory : joins-les de nouveau avant de "
          "renvoyer.");
      Say(context, text, kTradeWarningColor);
      compose_error_ += compose_error_.empty() ? text : "\n" + text;
    }
  }
  if (orders.close_window) {
    windows.compose.open = false;
    windows.compose.confirming_send = false;
    windows.compose.asking_amount = false;
  }
  if (orders.reopen_failed) {
    Say(context,
        ro::core::Tr("Le serveur n'a pas rouvert l'écriture : le courrier n'est pas parti."),
        kTradeFailedColor);
  }
}

void MailStage::FollowWriteState(MailWindows& windows, const TradeContext& context) {
  // L'ACCORD DU SERVEUR : une écriture neuve, ou la réouverture qui suit un
  // échec.
  if (mailbox_.write_seq() != write_seq_seen_) {
    write_seq_seen_ = mailbox_.write_seq();
    Obey(flow_.TellGranted(), windows, context);
  }
  // L'ÉCRITURE FERMÉE PAR LE SERVEUR LUI-MÊME — un changement de carte.
  if (!mailbox_.writing()) {
    Obey(flow_.NoteServerDropped(), windows, context);
  }
  Obey(flow_.Tick(context.now_ms), windows, context);
  // 🔴 L'ANNULATION DUE PART QUOI QU'IL ARRIVE : sans elle, le serveur garde
  // l'écriture ouverte et le joueur ne peut plus ni marcher ni utiliser d'item.
  if (mailbox_.cancel_due()) {
    Report("annulation d'écriture", ro::net::RequestCancelWrite(context.link, mailbox_));
  }
}

// ── LES NOUVELLES ──────────────────────────────────────────────────────────

void MailStage::SayNews(MailWindows& windows, const TradeContext& context) {
  for (const ro::sim::MailNews& news : mailbox_.news()) {
    if (news.seq <= news_said_) {
      continue;
    }
    news_said_ = news.seq;
    switch (news.kind) {
      case ro::sim::MailNewsKind::kSendResult: {
        const bool succeeded = news.code == ro::sim::kMailSendSucceeded;
        Say(context, SendResultText(news.code),
            succeeded ? kTradeSucceededColor : kTradeFailedColor);
        if (!succeeded) {
          compose_error_ = SendResultText(news.code);
        }
        Obey(flow_.TellSendResult(news.code, std::exchange(send_carried_, false), context.now_ms),
             windows, context);
        break;
      }
      // La réponse tardive d'un envoi tenu pour silencieux : elle dit seulement
      // si le courrier est parti, sans toucher à l'écriture en cours.
      case ro::sim::MailNewsKind::kSendResultLate: {
        const bool succeeded = news.code == ro::sim::kMailSendSucceeded;
        Say(context, SendResultText(news.code),
            succeeded ? kTradeSucceededColor : kTradeFailedColor);
        break;
      }
      case ro::sim::MailNewsKind::kAttachRefused: {
        const std::string item = context.items.NameOf(news.item, context.creators);
        compose_error_ = AttachRefusalText(news.code, item);
        Say(context, compose_error_, kTradeFailedColor);
        break;
      }
      case ro::sim::MailNewsKind::kWriteRefused: {
        const bool reopening = flow_.reopening();
        const MailWriteOrders orders = flow_.TellRefused();
        if (!reopening) {
          Say(context,
              ro::core::Tr("Impossible d'écrire un courrier maintenant : ferme d'abord le "
                           "storage ou l'échange en cours."),
              kTradeFailedColor);
        }
        Obey(orders, windows, context);
        break;
      }
      case ro::sim::MailNewsKind::kClaimed:
        Say(context, ClaimText(news.claim), kTradeSucceededColor);
        break;
      case ro::sim::MailNewsKind::kClaimRefused:
        Say(context, ClaimRefusalText(news.claim, news.code), kTradeFailedColor);
        break;
      case ro::sim::MailNewsKind::kDeleted:
        Say(context, ro::core::Tr("Courrier supprimé."), kTradeSucceededColor);
        break;
      case ro::sim::MailNewsKind::kReturned:
        Say(context, ro::core::Tr("Courrier retourné à son expéditeur."), kTradeSucceededColor);
        break;
      case ro::sim::MailNewsKind::kReturnRefused:
        Say(context, ro::core::Tr("Le serveur a refusé de retourner ce courrier."),
            kTradeFailedColor);
        break;
    }
  }
}

void MailStage::SayHeld(const TradeContext& context) {
  if (std::exchange(item_use_held_, false)) {
    Say(context, ro::core::Tr("Impossible d'utiliser un item pendant l'écriture d'un courrier."),
        kTradeFailedColor);
  }
  if (std::exchange(deal_held_, false)) {
    Say(context,
        ro::core::Tr("Pas d'échange pendant l'écriture d'un courrier : envoie-le ou annule-le "
                     "d'abord."),
        kTradeFailedColor);
  }
}

// ── L'IMAGE ────────────────────────────────────────────────────────────────

void MailStage::Advance(MailWindows& windows, ro::net::Connection& link, ro::sim::World& world,
                        const ro::vfs::ItemWords& words, const CreatorNames& creators,
                        std::uint32_t now_ms) {
  const TradeContext context{.link = link,
                             .world = world,
                             .words = words,
                             .creators = creators,
                             .items = items_,
                             .now_ms = now_ms};
  hovered_ = {};
  mailbox_.ExpireSilent(now_ms);
  FollowAsks(windows, context);
  FollowBox(windows, context);
  FollowCompose(windows, context);
  SayNews(windows, context);
  FollowWriteState(windows, context);
  SayHeld(context);

  LoadIcons();
  ComposeBox(windows, context);
  ComposeWrite(windows, context);
  box_shown_ = mailbox_.open();
  windows.box.open = mailbox_.open();
  windows.box_view = mailbox_.open() ? &box_view_ : nullptr;
  windows.compose.open = flow_.shown();
  windows.compose_view = flow_.shown() ? &compose_view_ : nullptr;
  windows.news_lit = mailbox_.unread_badge() || mailbox_.UnreadCount() > 0;
}

}  // namespace moonclient
