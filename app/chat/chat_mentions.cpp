#include "chat/chat_mentions.h"

#include <algorithm>

namespace moonclient {

void ChatMentionWatch::StartSession(std::uint64_t last_seq) noexcept {
  seen_through_ = last_seq;
  rang_ = false;
  rang_at_ms_ = 0;
  // Ce qui s'est dit avant d'entrer ne sonne pas, mais se signale encore.
  rejudge_ = true;
}

bool ChatMentionWatch::Advance(std::span<const ro::sim::ChatLine> chat,
                               const ro::sim::MentionRules& rules, std::uint32_t now_ms) {
  if (rejudge_) {
    rejudge_ = false;
    naming_seqs_.clear();
    for (const ro::sim::ChatLine& line : chat) {
      if (line.seq <= seen_through_ && ro::sim::MentionsMe(line, rules)) {
        naming_seqs_.push_back(line.seq);
      }
    }
  }
  bool named = false;
  std::uint64_t newest = seen_through_;
  for (const ro::sim::ChatLine& line : chat) {
    if (line.seq <= seen_through_) {
      continue;
    }
    newest = std::max(newest, line.seq);
    // ⚠ CHAQUE LIGNE NEUVE SE JUGE, même après une première mention : le fil
    // signale chacune.
    if (ro::sim::MentionsMe(line, rules)) {
      naming_seqs_.push_back(line.seq);
      named = true;
    }
  }
  seen_through_ = newest;
  if (!chat.empty()) {
    const std::uint64_t oldest = chat.front().seq;
    std::erase_if(naming_seqs_, [oldest](std::uint64_t seq) { return seq < oldest; });
  }
  // ⚠ L'ÉCART SE COMPTE EN ARITHMÉTIQUE NON SIGNÉE : l'horloge de la boucle
  // peut faire le tour, et la différence reste juste.
  if (!named || (rang_ && now_ms - rang_at_ms_ < kMentionRingGapMs)) {
    return false;
  }
  rang_ = true;
  rang_at_ms_ = now_ms;
  return true;
}

const ro::sim::MentionRules& ChatMentionWatch::RulesFor(std::string_view own_name,
                                                        std::uint32_t own_id,
                                                        std::string_view typed_words) {
  if (typed_words != typed_words_) {
    typed_words_ = typed_words;
    words_ = ro::sim::SplitMentionWords(typed_words_);
    rejudge_ = true;
  }
  if (own_name != own_name_ || own_id != own_id_) {
    own_name_ = own_name;
    own_id_ = own_id;
    rejudge_ = true;
  }
  rules_ = ro::sim::MentionRules{.own_name = own_name_, .own_id = own_id_, .words = words_};
  return rules_;
}

}  // namespace moonclient
