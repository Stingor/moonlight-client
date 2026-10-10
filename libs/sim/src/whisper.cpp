#include "ro/sim/whisper.h"

#include <algorithm>
#include <utility>

namespace ro::sim {

void WhisperOutbox::Note(std::string recipient, std::string text, std::uint32_t sent_ms) {
  // ⚠ LE PLUS ANCIEN CÈDE : s'il en est là, son accusé ne viendra plus, et
  // c'est le plus récent que le joueur attend de voir paraître.
  if (pending_.size() >= kMostPendingWhispers) {
    pending_.pop_front();
  }
  pending_.push_back(PendingWhisper{
      .recipient = std::move(recipient), .text = std::move(text), .sent_ms = sent_ms});
}

std::optional<PendingWhisper> WhisperOutbox::Settle(std::uint32_t now_ms) {
  // ⚠ LA DIFFÉRENCE EN ARITHMÉTIQUE NON SIGNÉE : l'horloge de la boucle peut
  // faire le tour, et l'écart reste juste.
  while (!pending_.empty() && now_ms - pending_.front().sent_ms > kWhisperAnswerWaitMs) {
    pending_.pop_front();
  }
  if (pending_.empty()) {
    return std::nullopt;
  }
  PendingWhisper settled = std::move(pending_.front());
  pending_.pop_front();
  return settled;
}

bool WhisperOutbox::Forget(std::string_view recipient) {
  const auto found = std::ranges::find(pending_, recipient, &PendingWhisper::recipient);
  if (found == pending_.end()) {
    return false;
  }
  pending_.erase(found);
  return true;
}

}  // namespace ro::sim
