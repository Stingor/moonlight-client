#include "chat/whisper_desk.h"

#include <algorithm>

namespace moonclient {

using ro::sim::WhisperRelation;
using ro::sim::WhisperSide;

bool OpensConversation(const FriendSetup& setup, WhisperRelation relation) {
  return relation == WhisperRelation::kFriend ? setup.open_from_friend : setup.open_from_stranger;
}

Conversation* WhisperDesk::Open(std::string_view peer) {
  Conversation* shown = Find(peer);
  if (shown == nullptr) {
    shown = Admit(peer);
  }
  if (shown != nullptr) {
    shown->active_rank = Stir();
    shown->raise = true;
  }
  return shown;
}

void WhisperDesk::Close(std::string_view peer) {
  std::erase_if(conversations_,
                [peer](const Conversation& conversation) { return conversation.peer == peer; });
}

Conversation* WhisperDesk::Find(std::string_view peer) {
  const auto found = std::ranges::find(conversations_, peer, &Conversation::peer);
  return found == conversations_.end() ? nullptr : &*found;
}

const Conversation* WhisperDesk::Find(std::string_view peer) const {
  const auto found = std::ranges::find(conversations_, peer, &Conversation::peer);
  return found == conversations_.end() ? nullptr : &*found;
}

Conversation* WhisperDesk::Admit(std::string_view peer) {
  if (conversations_.size() >= kMostConversations) {
    // ⚠ LA MOINS ACTIVE, et non la première ouverte : celle qu'on vient de
    // ramener devant a servi plus récemment que son rang ne le dit. ⚠ ET
    // SEULEMENT PARMI CELLES QUI PEUVENT CÉDER — voir `kMostConversations`.
    auto oldest = conversations_.end();
    for (auto at = conversations_.begin(); at != conversations_.end(); ++at) {
      if (at->unread || at->drafting) {
        continue;
      }
      if (oldest == conversations_.end() || at->active_rank < oldest->active_rank) {
        oldest = at;
      }
    }
    if (oldest == conversations_.end()) {
      return nullptr;
    }
    conversations_.erase(oldest);
  }
  conversations_.push_back(Conversation{.peer = std::string(peer), .active_rank = Stir()});
  return &conversations_.back();
}

WhisperDesk::Absorbed WhisperDesk::Absorb(std::span<const ro::sim::ChatLine> chat,
                                          const FriendSetup& setup, std::string_view own_name) {
  Absorbed absorbed;
  for (const ro::sim::ChatLine& line : chat) {
    if (line.seq <= absorbed_through_) {
      continue;
    }
    absorbed_through_ = line.seq;
    const ro::sim::WhisperFacts& facts = line.whisper;
    if (facts.side == WhisperSide::kNone || facts.peer_name.empty()) {
      continue;
    }
    Conversation* shown = Find(facts.peer_name);
    // ⚠ LE REFUS N'OUVRE RIEN ET NE SE PREND PAS : le fil l'écrit toujours, et
    // la conversation qui existe déjà le montre en plus.
    if (facts.side == WhisperSide::kUndelivered) {
      if (shown != nullptr) {
        shown->active_rank = Stir();
      }
      continue;
    }
    if (facts.peer_name == own_name) {
      continue;
    }
    // L'ÉCHO DE NOTRE ENVOI OUVRE AUSSI : le client fait passer les deux sens
    // par la même porte, gouvernée par les mêmes cases.
    if (shown == nullptr && OpensConversation(setup, facts.relation)) {
      shown = Admit(facts.peer_name);
      if (shown != nullptr) {
        ++absorbed.opened;
      }
    }
    if (shown == nullptr) {
      continue;
    }
    shown->active_rank = Stir();
    if (facts.side == WhisperSide::kReceived) {
      shown->unread = true;
    }
    apart_.push_back(line.seq);
  }
  // ⚠ CE QUE LE MONDE A OUBLIÉ NE SE RETIENT PLUS : sans cela, la liste
  // grandirait toute la partie.
  if (!chat.empty()) {
    const std::uint64_t oldest = chat.front().seq;
    std::erase_if(apart_, [oldest](std::uint64_t seq) { return seq < oldest; });
  }
  return absorbed;
}

bool WhisperDesk::ShownApart(std::uint64_t seq) const {
  return std::ranges::binary_search(apart_, seq);
}

void WhisperDesk::NoteDraft(std::string_view peer, bool drafting) {
  if (Conversation* shown = Find(peer); shown != nullptr) {
    shown->drafting = drafting;
  }
}

void WhisperDesk::Seen(std::string_view peer) {
  if (Conversation* shown = Find(peer); shown != nullptr) {
    shown->unread = false;
  }
}

void WhisperDesk::Clear(std::uint64_t last_seq) {
  conversations_.clear();
  apart_.clear();
  absorbed_through_ = last_seq;
}

}  // namespace moonclient
