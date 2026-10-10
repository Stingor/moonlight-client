#include "ro/sim/card_album.h"

#include <algorithm>
#include <utility>

namespace ro::sim {

void CardAlbum::Receive(CardAlbumResult result, std::vector<CardAlbumCard> cards,
                        bool dropper_known) {
  last_result_ = result;
  ++arrival_;
  unlocked_count_ = 0;
  total_reserve_ = 0;
  if (result == CardAlbumResult::kInUse) {
    cards_.clear();
    dropper_known_ = false;
    return;
  }
  cards_ = std::move(cards);
  dropper_known_ = dropper_known;
  for (const CardAlbumCard& card : cards_) {
    if (card.unlocked) {
      ++unlocked_count_;
      total_reserve_ += card.reserve;
    }
  }
}

void CardAlbum::Clear() noexcept { *this = CardAlbum(); }

const CardAlbumCard* CardAlbum::Find(std::uint32_t card_id) const {
  // Le serveur envoie le catalogue TRIÉ par identifiant ; on ne s'y fie pas
  // pour autant — une recherche linéaire sur neuf cents entrées ne coûte rien,
  // et un catalogue mal trié ne doit pas faire disparaître une carte.
  const auto found = std::find_if(cards_.begin(), cards_.end(), [card_id](const CardAlbumCard& c) {
    return c.card_id == card_id;
  });
  return found == cards_.end() ? nullptr : &*found;
}

}  // namespace ro::sim
