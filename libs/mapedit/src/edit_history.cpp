#include "ro/mapedit/edit_history.h"

#include <utility>

namespace ro::mapedit {

void EditHistory::Record(std::string label, MapEdit before, MapEdit after) {
  future_.clear();
  past_.push_back(Step{std::move(label), std::move(before), std::move(after)});
  if (past_.size() > kMaxSteps) {
    past_.erase(past_.begin());
  }
}

std::string EditHistory::UndoLabel() const {
  return past_.empty() ? std::string() : past_.back().label;
}

std::string EditHistory::RedoLabel() const {
  return future_.empty() ? std::string() : future_.back().label;
}

std::optional<MapEdit> EditHistory::Undo() {
  if (past_.empty()) {
    return std::nullopt;
  }
  future_.push_back(std::move(past_.back()));
  past_.pop_back();
  return future_.back().before;
}

std::optional<MapEdit> EditHistory::Redo() {
  if (future_.empty()) {
    return std::nullopt;
  }
  past_.push_back(std::move(future_.back()));
  future_.pop_back();
  return past_.back().after;
}

void EditHistory::Clear() noexcept {
  past_.clear();
  future_.clear();
}

}  // namespace ro::mapedit
