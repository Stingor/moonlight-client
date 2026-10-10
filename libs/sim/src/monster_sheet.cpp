#include "ro/sim/monster_sheet.h"

#include <utility>

namespace ro::sim {

void MonsterSheetBook::Receive(std::uint32_t asked_id, MonsterSheet sheet) {
  Entry& entry = entries_[asked_id];
  entry.sheet = std::move(sheet);
  entry.unknown = false;
  entry.arrival = ++arrivals_;
}

void MonsterSheetBook::NoteUnknown(std::uint32_t asked_id) {
  Entry& entry = entries_[asked_id];
  entry.sheet.reset();
  entry.unknown = true;
  entry.arrival = ++arrivals_;
}

const MonsterSheet* MonsterSheetBook::Find(std::uint32_t asked_id) const {
  const auto found = entries_.find(asked_id);
  if (found == entries_.end() || !found->second.sheet.has_value()) {
    return nullptr;
  }
  return &*found->second.sheet;
}

bool MonsterSheetBook::IsUnknown(std::uint32_t asked_id) const {
  const auto found = entries_.find(asked_id);
  return found != entries_.end() && found->second.unknown;
}

std::uint32_t MonsterSheetBook::ArrivalOf(std::uint32_t asked_id) const {
  const auto found = entries_.find(asked_id);
  return found == entries_.end() ? 0 : found->second.arrival;
}

void MonsterSheetBook::ReceiveSense(SenseReading reading) {
  sense_ = reading;
  ++sense_readings_;
}

void MonsterSheetBook::Clear() noexcept {
  entries_.clear();
  sense_.reset();
}

}  // namespace ro::sim
