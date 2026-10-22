#include "chat/recent_whispers.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>

namespace moonclient {
namespace {

constexpr auto kNamesKept = static_cast<std::size_t>(ro::ui::kChatWhisperNamesKept);

}  // namespace

void RecentWhispers::Note(std::string_view name, ro::ui::ChatWhisperWay way) {
  if (name.empty()) {
    return;
  }
  // ⚠ LE NOM EXACT : deux personnages peuvent ne différer que par la casse.
  std::erase_if(peers_, [name](const WhisperPeer& peer) { return peer.name == name; });
  peers_.push_front(
      WhisperPeer{.name = std::string(name), .incoming = way == ro::ui::ChatWhisperWay::kIncoming});
  while (peers_.size() > kNamesKept) {
    peers_.pop_back();
  }
}

void RecentWhispers::FollowChat(std::span<const ro::sim::ChatLine> chat,
                                std::string_view own_name) {
  // ⚠ DEPUIS LA FIN : seules les dernières lignes sont neuves, et parcourir tout
  // le fil à chaque image coûterait des milliers de lignes pour en trouver zéro.
  auto first_new = chat.end();
  while (first_new != chat.begin() && std::prev(first_new)->seq > heard_through_) {
    --first_new;
  }
  for (const ro::sim::ChatLine& line : std::span(first_new, chat.end())) {
    const ro::sim::WhisperFacts& facts = line.whisper;
    if (facts.peer_name == own_name) {
      continue;
    }
    if (facts.side == ro::sim::WhisperSide::kReceived && facts.sender_id != 0) {
      Note(facts.peer_name, ro::ui::ChatWhisperWay::kIncoming);
    } else if (facts.side == ro::sim::WhisperSide::kSent) {
      Note(facts.peer_name, ro::ui::ChatWhisperWay::kOutgoing);
    }
  }
  if (!chat.empty()) {
    heard_through_ = std::max(heard_through_, chat.back().seq);
  }
}

void RecentWhispers::Restore(std::span<const WhisperPeer> peers) {
  peers_.clear();
  // ⚠ BORNÉE ET SANS DOUBLON À LA RELECTURE AUSSI : un fichier retouché à la
  // main peut en porter cent, ou deux fois le même. Le premier, le plus récent,
  // l'emporte.
  for (const WhisperPeer& peer : peers) {
    if (peers_.size() >= kNamesKept) {
      break;
    }
    if (!peer.name.empty() &&
        std::ranges::find(peers_, peer.name, &WhisperPeer::name) == peers_.end()) {
      peers_.push_back(peer);
    }
  }
}

void RecentWhispers::Forget() {
  peers_.clear();
  views_.clear();
}

std::span<const ro::ui::ChatRecentWhisper> RecentWhispers::Views() {
  views_.clear();
  for (const WhisperPeer& peer : peers_) {
    views_.push_back(ro::ui::ChatRecentWhisper{.name = peer.name,
                                               .way = peer.incoming
                                                          ? ro::ui::ChatWhisperWay::kIncoming
                                                          : ro::ui::ChatWhisperWay::kOutgoing});
  }
  return views_;
}

}  // namespace moonclient
