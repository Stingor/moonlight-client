// Le courrier. Voir l'en-tête.

#include "ro/sim/mailbox.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace ro::sim {
namespace {

std::int16_t SaturatedSum(std::int16_t held, std::int32_t added) noexcept {
  const std::int32_t sum = static_cast<std::int32_t>(held) + added;
  return static_cast<std::int16_t>(
      std::clamp(sum, std::int32_t{0}, std::int32_t{std::numeric_limits<std::int16_t>::max()}));
}

}  // namespace

std::int64_t MailFeeFor(std::int64_t zeny, std::size_t attachment_count) noexcept {
  const std::int64_t sent = std::clamp<std::int64_t>(zeny, 0, kMailMostZeny);
  const auto pieces = static_cast<std::int64_t>(std::min(attachment_count, kMailMostItems));
  return pieces * kMailAttachmentFeeZeny + sent * kMailZenyFeePercent / kMailPercentBase;
}

std::int64_t MailSendTotal(std::int64_t zeny, std::size_t attachment_count) noexcept {
  return std::clamp<std::int64_t>(zeny, 0, kMailMostZeny) + MailFeeFor(zeny, attachment_count);
}

std::int64_t MailSecondsLeft(const MailHeader& header, std::int64_t now_unix_s) noexcept {
  return std::max<std::int64_t>(header.expires_at_unix_s - now_unix_s, 0);
}

void Mailbox::Tell(MailNewsKind kind, std::uint8_t code, std::int64_t mail_id, MailClaim claim,
                   const InventoryItem* item) {
  MailNews news;
  news.seq = ++news_seq_;
  news.kind = kind;
  news.code = code;
  news.mail_id = mail_id;
  news.claim = claim;
  if (item != nullptr) {
    news.item = *item;
  }
  if (news_.size() == kMailNewsKept) {
    news_.erase(news_.begin());
  }
  news_.push_back(std::move(news));
}

// ── LA BOÎTE ───────────────────────────────────────────────────────────────

void Mailbox::NoteOpened(std::uint32_t now_ms) {
  open_ = true;
  NoteListRequested(now_ms);
}

void Mailbox::NoteListRequested(std::uint32_t now_ms) noexcept {
  list_requested_ = true;
  list_requested_ms_ = now_ms;
}

bool Mailbox::ListLoading(std::uint32_t now_ms) const noexcept {
  return list_requested_ && now_ms - list_requested_ms_ <= kMailListLoadingMs;
}

void Mailbox::TellList(std::vector<MailHeader> headers) {
  if (headers.size() > kMailMostListed) {
    headers.resize(kMailMostListed);
  }
  std::ranges::sort(headers, [](const MailHeader& left, const MailHeader& right) {
    return left.mail_id > right.mail_id;
  });
  // Un courrier dont on tient le contenu a été lu, quoi qu'en dise une liste
  // demandée avant la lecture.
  for (MailHeader& header : headers) {
    if (FindContent(header.mail_id) != nullptr) {
      header.read = true;
    }
  }
  std::erase_if(contents_, [&headers](const MailContent& content) {
    return std::ranges::none_of(headers, [&content](const MailHeader& header) {
      return header.mail_id == content.mail_id;
    });
  });
  headers_ = std::move(headers);
  list_requested_ = false;
  list_received_ = true;
}

MailHeader* Mailbox::FindHeaderMutable(std::int64_t mail_id) noexcept {
  const auto found = std::ranges::find_if(
      headers_, [mail_id](const MailHeader& header) { return header.mail_id == mail_id; });
  return found == headers_.end() ? nullptr : &*found;
}

const MailHeader* Mailbox::FindHeader(std::int64_t mail_id) const noexcept {
  const auto found = std::ranges::find_if(
      headers_, [mail_id](const MailHeader& header) { return header.mail_id == mail_id; });
  return found == headers_.end() ? nullptr : &*found;
}

std::size_t Mailbox::UnreadCount() const noexcept {
  return static_cast<std::size_t>(
      std::ranges::count_if(headers_, [](const MailHeader& header) { return !header.read; }));
}

std::int64_t Mailbox::NewestIdIn(MailBox box) const noexcept {
  std::int64_t newest = 0;
  for (const MailHeader& header : headers_) {
    if (header.box == box) {
      newest = std::max(newest, header.mail_id);
    }
  }
  return newest;
}

// ── LE CONTENU ─────────────────────────────────────────────────────────────

void Mailbox::NoteReadSent(std::int64_t mail_id, std::uint32_t now_ms) {
  std::erase(unanswered_reads_, mail_id);
  std::erase_if(pending_reads_,
                [mail_id](const PendingRead& pending) { return pending.mail_id == mail_id; });
  // Une boîte n'en tient pas plus : des lectures sans fin ne s'accumulent pas.
  if (pending_reads_.size() >= kMailMostListed) {
    pending_reads_.erase(pending_reads_.begin());
  }
  pending_reads_.push_back(PendingRead{.mail_id = mail_id, .sent_ms = now_ms});
}

void Mailbox::MarkReadUnanswered(std::int64_t mail_id) {
  if (ReadUnanswered(mail_id)) {
    return;
  }
  if (unanswered_reads_.size() >= kMailMostListed) {
    unanswered_reads_.erase(unanswered_reads_.begin());
  }
  unanswered_reads_.push_back(mail_id);
}

bool Mailbox::ReadInFlight(std::int64_t mail_id) const noexcept {
  return std::ranges::any_of(
      pending_reads_, [mail_id](const PendingRead& pending) { return pending.mail_id == mail_id; });
}

bool Mailbox::ReadUnanswered(std::int64_t mail_id) const noexcept {
  return std::ranges::find(unanswered_reads_, mail_id) != unanswered_reads_.end();
}

MailContent* Mailbox::FindContentMutable(std::int64_t mail_id) noexcept {
  const auto found = std::ranges::find_if(
      contents_, [mail_id](const MailContent& content) { return content.mail_id == mail_id; });
  return found == contents_.end() ? nullptr : &*found;
}

const MailContent* Mailbox::FindContent(std::int64_t mail_id) const noexcept {
  const auto found = std::ranges::find_if(
      contents_, [mail_id](const MailContent& content) { return content.mail_id == mail_id; });
  return found == contents_.end() ? nullptr : &*found;
}

void Mailbox::TellContent(MailContent content) {
  if (content.items.size() > kMailMostItems) {
    content.items.resize(kMailMostItems);
  }
  std::erase_if(pending_reads_, [&content](const PendingRead& pending) {
    return pending.mail_id == content.mail_id;
  });
  std::erase(unanswered_reads_, content.mail_id);
  if (MailHeader* header = FindHeaderMutable(content.mail_id); header != nullptr) {
    header->read = true;
  }
  if (MailContent* held = FindContentMutable(content.mail_id); held != nullptr) {
    *held = std::move(content);
    return;
  }
  // Une boîte n'en tient pas plus : un contenu sans en-tête ne s'accumule pas.
  if (contents_.size() >= kMailMostListed) {
    contents_.erase(contents_.begin());
  }
  contents_.push_back(std::move(content));
}

// ── LES RÉCUPÉRATIONS ──────────────────────────────────────────────────────

void Mailbox::NoteClaimSent(std::int64_t mail_id, MailClaim claim, std::uint32_t now_ms) {
  if (ClaimInFlight(mail_id, claim)) {
    return;
  }
  pending_claims_.push_back(PendingClaim{.mail_id = mail_id, .claim = claim, .sent_ms = now_ms});
}

bool Mailbox::ClaimInFlight(std::int64_t mail_id, MailClaim claim) const noexcept {
  return std::ranges::any_of(pending_claims_, [mail_id, claim](const PendingClaim& pending) {
    return pending.mail_id == mail_id && pending.claim == claim;
  });
}

void Mailbox::TellClaimResult(std::int64_t mail_id, MailClaim claim, std::uint8_t code) {
  std::erase_if(pending_claims_, [mail_id, claim](const PendingClaim& pending) {
    return pending.mail_id == mail_id && pending.claim == claim;
  });
  if (code != kMailClaimSucceeded) {
    Tell(MailNewsKind::kClaimRefused, code, mail_id, claim);
    return;
  }
  MailHeader* header = FindHeaderMutable(mail_id);
  MailContent* content = FindContentMutable(mail_id);
  if (claim == MailClaim::kZeny) {
    if (header != nullptr) {
      header->has_zeny = false;
    }
    if (content != nullptr) {
      content->zeny = 0;
    }
  } else {
    if (header != nullptr) {
      header->has_items = false;
    }
    if (content != nullptr) {
      content->items.clear();
    }
  }
  Tell(MailNewsKind::kClaimed, code, mail_id, claim);
}

std::vector<std::int64_t> Mailbox::ClaimableIn(MailBox box) const {
  std::vector<std::int64_t> claimable;
  for (const MailHeader& header : headers_) {
    if (header.box != box) {
      continue;
    }
    const bool items_left = header.has_items && !ClaimInFlight(header.mail_id, MailClaim::kItems);
    const bool zeny_left = header.has_zeny && !ClaimInFlight(header.mail_id, MailClaim::kZeny);
    if (items_left || zeny_left) {
      claimable.push_back(header.mail_id);
    }
  }
  return claimable;
}

// ── SUPPRESSION ET RETOUR ──────────────────────────────────────────────────

void Mailbox::Forget(std::int64_t mail_id) {
  std::erase_if(headers_,
                [mail_id](const MailHeader& header) { return header.mail_id == mail_id; });
  std::erase_if(contents_,
                [mail_id](const MailContent& content) { return content.mail_id == mail_id; });
  std::erase_if(pending_claims_,
                [mail_id](const PendingClaim& pending) { return pending.mail_id == mail_id; });
  std::erase(pending_returns_, mail_id);
  std::erase_if(pending_reads_,
                [mail_id](const PendingRead& pending) { return pending.mail_id == mail_id; });
  std::erase(unanswered_reads_, mail_id);
}

void Mailbox::TellDeleted(std::int64_t mail_id) {
  Forget(mail_id);
  Tell(MailNewsKind::kDeleted, 0, mail_id);
}

void Mailbox::NoteReturnSent(std::int64_t mail_id) {
  if (ReturnInFlight(mail_id)) {
    return;
  }
  if (pending_returns_.size() >= kMailMostListed) {
    pending_returns_.erase(pending_returns_.begin());
  }
  pending_returns_.push_back(mail_id);
}

bool Mailbox::ReturnInFlight(std::int64_t mail_id) const noexcept {
  return std::ranges::find(pending_returns_, mail_id) != pending_returns_.end();
}

void Mailbox::TellReturnResult(std::uint32_t mail_id_low, std::uint32_t result) {
  const auto low_matches = [mail_id_low](std::int64_t mail_id) {
    return static_cast<std::uint32_t>(mail_id) == mail_id_low;
  };
  const auto returned = std::ranges::find_if(pending_returns_, low_matches);
  const bool asked = returned != pending_returns_.end();
  const std::int64_t mail_id = asked ? *returned : static_cast<std::int64_t>(mail_id_low);
  if (asked) {
    pending_returns_.erase(returned);
  }
  if (result != kMailReturnSucceeded) {
    if (asked) {
      const auto code = static_cast<std::uint8_t>(
          std::min<std::uint32_t>(result, std::numeric_limits<std::uint8_t>::max()));
      Tell(MailNewsKind::kReturnRefused, code, mail_id);
      return;
    }
    // Sans retour demandé, c'est la réponse à la lecture d'un courrier que le
    // serveur ne connaît pas : cette lecture ne viendra pas.
    if (const auto read = std::ranges::find_if(
            pending_reads_,
            [&low_matches](const PendingRead& pending) { return low_matches(pending.mail_id); });
        read != pending_reads_.end()) {
      const std::int64_t unanswered = read->mail_id;
      pending_reads_.erase(read);
      MarkReadUnanswered(unanswered);
    }
    return;
  }
  // Sans retour demandé, l'identifiant complet se retrouve dans la liste.
  std::int64_t forgotten = mail_id;
  if (!asked) {
    const auto found = std::ranges::find_if(headers_, [mail_id_low](const MailHeader& header) {
      return static_cast<std::uint32_t>(header.mail_id) == mail_id_low;
    });
    if (found == headers_.end()) {
      return;
    }
    forgotten = found->mail_id;
  }
  Forget(forgotten);
  Tell(MailNewsKind::kReturned, 0, forgotten);
}

// ── L'ÉCRITURE ─────────────────────────────────────────────────────────────

void Mailbox::ForgetAttachments() noexcept {
  attachments_.clear();
  pending_attaches_.clear();
  attached_weight_ = 0;
}

void Mailbox::ResetWrite() {
  writing_ = false;
  write_recipient_.clear();
  write_recipient_wire_.clear();
  ForgetAttachments();
  send_in_flight_ = false;
  send_sent_ms_ = 0;
  recipient_ = MailRecipient();
  recipient_sent_ms_ = 0;
}

void Mailbox::TellWriteOpened(bool granted, std::string recipient, std::string recipient_wire) {
  if (!granted) {
    Tell(MailNewsKind::kWriteRefused);
    return;
  }
  ResetWrite();
  writing_ = true;
  write_recipient_ = std::move(recipient);
  write_recipient_wire_ = std::move(recipient_wire);
  standing_ = MailDraftStanding::kUsable;
  ++write_seq_;
}

void Mailbox::NoteWriteCancelled() {
  ResetWrite();
  // L'annulation ferme l'écriture chez le serveur ; un brouillon à rouvrir le
  // reste, et l'ouverture suivante le dira.
  if (standing_ == MailDraftStanding::kCancelDue) {
    standing_ = MailDraftStanding::kUsable;
  }
}

void Mailbox::CloseWriteForMapChange() {
  ResetWrite();
  standing_ = MailDraftStanding::kUsable;
  send_silenced_ = false;
}

std::int32_t Mailbox::WithheldAt(std::uint16_t client_slot) const noexcept {
  if (!writing_) {
    return 0;
  }
  return AttachedAmountAt(client_slot) + InFlightAt(client_slot);
}

std::int32_t Mailbox::AttachedAmountAt(std::uint16_t client_slot) const noexcept {
  const auto found =
      std::ranges::find_if(attachments_, [client_slot](const MailAttachment& attachment) {
        return attachment.item.client_slot == client_slot;
      });
  return found == attachments_.end() ? 0 : std::max<std::int32_t>(found->item.amount, 0);
}

std::int32_t Mailbox::InFlightAt(std::uint16_t client_slot) const noexcept {
  std::int32_t total = 0;
  for (const MailPendingAttach& pending : pending_attaches_) {
    if (pending.item.client_slot == client_slot) {
      total += pending.amount;
    }
  }
  return total;
}

bool Mailbox::Engages(std::uint16_t client_slot) const noexcept {
  return AttachedAmountAt(client_slot) > 0 || InFlightAt(client_slot) > 0;
}

std::size_t Mailbox::DistinctEngaged() const noexcept {
  std::vector<std::uint16_t> slots;
  slots.reserve(attachments_.size() + pending_attaches_.size());
  for (const MailAttachment& attachment : attachments_) {
    slots.push_back(attachment.item.client_slot);
  }
  for (const MailPendingAttach& pending : pending_attaches_) {
    slots.push_back(pending.item.client_slot);
  }
  std::ranges::sort(slots);
  return static_cast<std::size_t>(
      std::ranges::distance(slots.begin(), std::ranges::unique(slots).begin()));
}

MailAttachFault Mailbox::CheckAttach(const InventoryItem& bag_item,
                                     std::int32_t amount) const noexcept {
  if (!writing_) {
    return MailAttachFault::kNotWriting;
  }
  if (send_in_flight_) {
    return MailAttachFault::kSendInFlight;
  }
  if (reopen_due()) {
    return MailAttachFault::kReopenDue;
  }
  if (!pending_attaches_.empty()) {
    return MailAttachFault::kAttachInFlight;
  }
  if (bag_item.client_slot < kMailFirstBagSlot) {
    return MailAttachFault::kBadSlot;
  }
  if (bag_item.equipped()) {
    return MailAttachFault::kEquipped;
  }
  if (bag_item.rented()) {
    return MailAttachFault::kRented;
  }
  const std::int32_t left = static_cast<std::int32_t>(bag_item.amount) -
                            AttachedAmountAt(bag_item.client_slot) -
                            InFlightAt(bag_item.client_slot);
  if (left < 1) {
    return MailAttachFault::kAllAttached;
  }
  if (amount < 1 || amount > left || amount > kMailMostAttachAmount) {
    return MailAttachFault::kBadAmount;
  }
  if (!Engages(bag_item.client_slot) && DistinctEngaged() >= kMailMostItems) {
    return MailAttachFault::kAttachmentsFull;
  }
  return MailAttachFault::kNone;
}

void Mailbox::NoteAttachSent(const InventoryItem& bag_item, std::int16_t amount,
                             std::uint32_t now_ms) {
  pending_attaches_.push_back(
      MailPendingAttach{.item = bag_item, .amount = amount, .sent_ms = now_ms});
}

void Mailbox::TellAttachResult(std::uint8_t code, const InventoryItem& item, std::int16_t count,
                               std::int32_t weight) {
  if (code != kMailAttachSucceeded) {
    InventoryItem refused = item;
    if (!pending_attaches_.empty()) {
      refused = pending_attaches_.front().item;
      pending_attaches_.erase(pending_attaches_.begin());
    }
    Tell(MailNewsKind::kAttachRefused, code, 0, MailClaim::kItems, &refused);
    return;
  }
  if (const auto pending = std::ranges::find_if(pending_attaches_,
                                                [&item](const MailPendingAttach& sent) {
                                                  return sent.item.client_slot == item.client_slot;
                                                });
      pending != pending_attaches_.end()) {
    pending_attaches_.erase(pending);
  }
  // Un accusé hors brouillon ne vise rien, ni dans un brouillon à rouvrir ; une
  // case du zeny non plus.
  if (!writing_ || reopen_due() || item.client_slot < kMailFirstBagSlot || count <= 0) {
    return;
  }
  attached_weight_ = weight;
  const auto held = std::ranges::find_if(attachments_, [&item](const MailAttachment& attachment) {
    return attachment.item.client_slot == item.client_slot;
  });
  if (held != attachments_.end()) {
    held->item.amount = SaturatedSum(held->item.amount, count);
    return;
  }
  MailAttachment attachment{.item = item};
  attachment.item.amount = count;
  attachments_.push_back(std::move(attachment));
}

bool Mailbox::CanDetach(std::uint16_t client_slot, std::int32_t amount) const noexcept {
  return writing_ && !send_in_flight_ && !reopen_due() && amount > 0 &&
         amount <= AttachedAmountAt(client_slot);
}

void Mailbox::TellDetached(std::uint16_t client_slot, std::int32_t amount, std::int32_t weight) {
  if (!writing_) {
    return;
  }
  const auto held = std::ranges::find_if(attachments_, [client_slot](const MailAttachment& entry) {
    return entry.item.client_slot == client_slot;
  });
  if (held == attachments_.end()) {
    return;
  }
  attached_weight_ = weight;
  held->item.amount = SaturatedSum(held->item.amount, -amount);
  if (held->item.amount <= 0) {
    attachments_.erase(held);
  }
}

MailSendFault Mailbox::CheckSend(std::string_view recipient, std::string_view title,
                                 std::string_view body, std::int64_t zeny,
                                 std::optional<std::int64_t> purse) const noexcept {
  if (!writing_) {
    return MailSendFault::kNotWriting;
  }
  if (send_in_flight_) {
    return MailSendFault::kSendInFlight;
  }
  if (reopen_due()) {
    return MailSendFault::kReopenDue;
  }
  if (!pending_attaches_.empty()) {
    return MailSendFault::kAttachInFlight;
  }
  if (recipient.empty()) {
    return MailSendFault::kRecipientEmpty;
  }
  if (recipient.size() > kMailMostNameBytes) {
    return MailSendFault::kRecipientTooLong;
  }
  if (title.empty()) {
    return MailSendFault::kTitleEmpty;
  }
  if (title.size() > kMailMostTitleBytes) {
    return MailSendFault::kTitleTooLong;
  }
  if (body.size() > kMailMostBodyBytes) {
    return MailSendFault::kBodyTooLong;
  }
  constexpr char kNul = '\0';
  if (recipient.find(kNul) != std::string_view::npos ||
      title.find(kNul) != std::string_view::npos || body.find(kNul) != std::string_view::npos) {
    return MailSendFault::kEmbeddedNul;
  }
  if (zeny < 0) {
    return MailSendFault::kZenyNegative;
  }
  if (zeny > kMailMostZeny) {
    return MailSendFault::kZenyAboveMost;
  }
  if (!purse.has_value()) {
    return MailSendFault::kPurseUnknown;
  }
  if (MailSendTotal(zeny, attachments_.size()) > *purse) {
    return MailSendFault::kPurseShort;
  }
  return MailSendFault::kNone;
}

void Mailbox::NoteSendSent(std::uint32_t now_ms) noexcept {
  send_in_flight_ = true;
  send_sent_ms_ = now_ms;
  // Un résultat qui viendrait maintenant répond à CET envoi.
  send_silenced_ = false;
}

void Mailbox::TellSendResult(std::uint8_t code) {
  if (!send_in_flight_) {
    if (std::exchange(send_silenced_, false)) {
      Tell(MailNewsKind::kSendResultLate, code);
    }
    return;
  }
  send_in_flight_ = false;
  Tell(MailNewsKind::kSendResult, code);
  if (code == kMailSendSucceeded) {
    ResetWrite();
    standing_ = MailDraftStanding::kCancelDue;
    return;
  }
  // Selon le refus, le serveur a rendu les pièces jointes au sac sans le dire,
  // ou les a gardées jointes : le brouillon ne sait plus, et l'oublie.
  ForgetAttachments();
  standing_ = MailDraftStanding::kReopenDue;
}

void Mailbox::NoteRecipientCheckSent(std::uint32_t now_ms, std::string queried_wire) {
  recipient_ =
      MailRecipient{.state = MailRecipientState::kPending, .queried_wire = std::move(queried_wire)};
  recipient_sent_ms_ = now_ms;
}

void Mailbox::TellRecipient(std::uint32_t char_id, std::uint16_t job, std::uint16_t base_level,
                            std::string name) {
  if (recipient_.state != MailRecipientState::kPending) {
    return;
  }
  if (char_id == 0) {
    recipient_.state = MailRecipientState::kNotFound;
    return;
  }
  recipient_.state = MailRecipientState::kFound;
  recipient_.char_id = char_id;
  recipient_.job = job;
  recipient_.base_level = base_level;
  recipient_.name = std::move(name);
}

// ── LES SILENCES ───────────────────────────────────────────────────────────

void Mailbox::ExpireSilent(std::uint32_t now_ms) {
  for (auto pending = pending_attaches_.begin(); pending != pending_attaches_.end();) {
    if (now_ms - pending->sent_ms < kMailAttachSilenceMs) {
      ++pending;
      continue;
    }
    Tell(MailNewsKind::kAttachRefused, kMailSilent, 0, MailClaim::kItems, &pending->item);
    pending = pending_attaches_.erase(pending);
  }
  if (send_in_flight_ && now_ms - send_sent_ms_ >= kMailSendSilenceMs) {
    // 🔴 LE COURRIER A PU PARTIR, et le serveur tient encore ses pièces jointes
    // et son écriture : renvoyer tel quel pourrait l'envoyer deux fois.
    send_in_flight_ = false;
    send_silenced_ = true;
    ForgetAttachments();
    standing_ = MailDraftStanding::kOutcomeUnknown;
    Tell(MailNewsKind::kSendResult, kMailSilent);
  }
  for (auto pending = pending_reads_.begin(); pending != pending_reads_.end();) {
    if (now_ms - pending->sent_ms < kMailReadSilenceMs) {
      ++pending;
      continue;
    }
    const std::int64_t unanswered = pending->mail_id;
    pending = pending_reads_.erase(pending);
    MarkReadUnanswered(unanswered);
  }
  for (auto pending = pending_claims_.begin(); pending != pending_claims_.end();) {
    if (now_ms - pending->sent_ms < kMailClaimSilenceMs) {
      ++pending;
      continue;
    }
    Tell(MailNewsKind::kClaimRefused, kMailSilent, pending->mail_id, pending->claim);
    pending = pending_claims_.erase(pending);
  }
  if (recipient_.state == MailRecipientState::kPending &&
      now_ms - recipient_sent_ms_ >= kMailCheckSilenceMs) {
    recipient_.state = MailRecipientState::kSilent;
  }
}

}  // namespace ro::sim
