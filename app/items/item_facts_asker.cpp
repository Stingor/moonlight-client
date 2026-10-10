#include "items/item_facts_asker.h"

#include "ro/net/item_facts.h"

namespace moonclient {
namespace {

[[nodiscard]] ro::sim::ItemSourceScope ScopeOf(ItemFactsAsker::Kind kind) {
  return kind == ItemFactsAsker::Kind::kBoss ? ro::sim::ItemSourceScope::kBoss
                                             : ro::sim::ItemSourceScope::kOrdinary;
}

}  // namespace

bool ItemFactsAsker::Asked(std::uint32_t item_id, Kind kind) const {
  return sent_.contains({item_id, kind});
}

void ItemFactsAsker::Forget(std::uint32_t item_id, Kind kind) {
  if (kind != Kind::kScripts) {
    facts_.ForgetSources(item_id, ScopeOf(kind));
  }
  sent_.erase({item_id, kind});
}

void ItemFactsAsker::AskOne(ro::net::Connection& link, std::uint32_t item_id, Kind kind,
                            std::uint32_t now_ms) {
  const bool scripts = kind == Kind::kScripts;
  const bool known = scripts ? facts_.ScriptsOf(item_id) != nullptr
                             : facts_.SourcesOf(item_id, ScopeOf(kind)) != nullptr;
  const auto found = sent_.find({item_id, kind});
  if (known && found != sent_.end() && !found->second.answered) {
    // La réponse vient d'arriver : c'est d'elle que la fraîcheur se compte.
    found->second.answered = true;
    found->second.at_ms = now_ms;
  }
  if (known && found != sent_.end()) {
    if (scripts || now_ms - found->second.at_ms < kItemSourcesFreshMs) {
      return;
    }
    facts_.ForgetSources(item_id, ScopeOf(kind));
  } else if (known) {
    // Une réponse arrivée sans qu'on l'ait demandée : elle vaut comme fraîche.
    sent_[{item_id, kind}] = Sent{.at_ms = now_ms, .answered = true};
    return;
  } else if (found != sent_.end() && !found->second.answered &&
             now_ms - found->second.at_ms < kItemFactsAnswerWaitMs) {
    return;  // la demande est en route
  }
  sent_[{item_id, kind}] = Sent{.at_ms = now_ms, .answered = false};
  if (scripts) {
    static_cast<void>(ro::net::RequestItemScripts(link, item_id));
  } else {
    static_cast<void>(ro::net::RequestItemSources(link, item_id, ScopeOf(kind)));
  }
}

void ItemFactsAsker::WantCreator(std::uint32_t character_id) {
  if (character_id != 0 && facts_.CreatorOf(character_id) == nullptr) {
    wanted_creators_.insert(character_id);
  }
}

void ItemFactsAsker::AskCreators(ro::net::Connection& link, std::uint32_t now_ms) {
  for (const std::uint32_t character_id : wanted_creators_) {
    if (facts_.CreatorOf(character_id) != nullptr) {
      continue;
    }
    const auto asked = creators_asked_at_ms_.find(character_id);
    if (asked != creators_asked_at_ms_.end() && now_ms - asked->second < kItemFactsAnswerWaitMs) {
      continue;  // la demande est en route
    }
    creators_asked_at_ms_[character_id] = now_ms;
    static_cast<void>(ro::net::RequestCreatorName(link, character_id));
  }
  wanted_creators_.clear();
}

void ItemFactsAsker::Ask(ro::net::Connection& link, bool window_open, std::uint32_t item_id,
                         std::uint32_t now_ms) {
  if (link.is_open()) {
    AskCreators(link, now_ms);
  }
  if (window_open && !window_was_open_ && item_id != 0) {
    Forget(item_id, Kind::kOrdinary);
    Forget(item_id, Kind::kBoss);
  }
  window_was_open_ = window_open;
  if (!window_open || item_id == 0 || !link.is_open()) {
    return;
  }
  AskOne(link, item_id, Kind::kOrdinary, now_ms);
  AskOne(link, item_id, Kind::kBoss, now_ms);
  AskOne(link, item_id, Kind::kScripts, now_ms);
}

}  // namespace moonclient
