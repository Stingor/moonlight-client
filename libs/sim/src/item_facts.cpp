#include "ro/sim/item_facts.h"

#include <algorithm>
#include <utility>

namespace ro::sim {

void ItemFacts::TellSources(std::uint32_t item_id, ItemSourceScope scope, ItemDropSources told) {
  sources_[{item_id, scope}] = std::move(told);
}

const ItemDropSources* ItemFacts::SourcesOf(std::uint32_t item_id, ItemSourceScope scope) const {
  const auto found = sources_.find({item_id, scope});
  return found == sources_.end() ? nullptr : &found->second;
}

void ItemFacts::ForgetSources(std::uint32_t item_id, ItemSourceScope scope) {
  sources_.erase({item_id, scope});
}

void ItemFacts::TellScripts(std::uint32_t item_id, ItemScripts told) {
  scripts_[item_id] = std::move(told);
}

const ItemScripts* ItemFacts::ScriptsOf(std::uint32_t item_id) const {
  const auto found = scripts_.find(item_id);
  return found == scripts_.end() ? nullptr : &found->second;
}

void ItemFacts::TellCreator(std::uint32_t character_id, std::string name) {
  creators_.insert_or_assign(character_id, std::move(name));
}

const std::string* ItemFacts::CreatorOf(std::uint32_t character_id) const {
  const auto found = creators_.find(character_id);
  return found != creators_.end() ? &found->second : nullptr;
}

void ItemFacts::TellAutoloot(std::vector<std::uint32_t> item_ids) {
  autoloot_ = std::move(item_ids);
  autoloot_told_ = true;
}

void ItemFacts::NoteAutoloot(std::uint32_t item_id, bool listed) {
  const auto found = std::ranges::find(autoloot_, item_id);
  if (listed && found == autoloot_.end()) {
    autoloot_.push_back(item_id);
  } else if (!listed && found != autoloot_.end()) {
    autoloot_.erase(found);
  }
}

bool ItemFacts::Autoloots(std::uint32_t item_id) const {
  return std::ranges::find(autoloot_, item_id) != autoloot_.end();
}

}  // namespace ro::sim
