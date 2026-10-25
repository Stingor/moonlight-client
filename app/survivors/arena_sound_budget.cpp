#include "survivors/arena_sound_budget.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace moonclient {

ArenaSoundVerdict ArenaSoundBudget::Ask(std::string_view name, ArenaSoundRank rank,
                                        std::optional<std::uint64_t> length_ms,
                                        std::uint64_t now_ms) {
  ArenaSoundVerdict verdict;
  if (!length_ms.has_value()) {
    ++refused_total_;
    return verdict;
  }
  Release(now_ms);

  // La cadence passe avant tout rang : un second départ trop tôt ne s'entend
  // pas comme un second coup, il ne vaut aucune voix.
  if (const auto last = last_start_ms_.find(name);
      last != last_start_ms_.end() && now_ms < last->second + kArenaSoundNameCadenceMs) {
    ++refused_total_;
    return verdict;
  }

  if (voices_.size() >= kArenaSoundVoices) {
    const Voice* const yielding = Yielding(rank);
    if (yielding == nullptr) {
      ++refused_total_;
      return verdict;
    }
    verdict.silenced = yielding->name;
    std::erase_if(voices_, [&](const Voice& voice) { return voice.name == verdict.silenced; });
    ++silenced_total_;
  }

  voices_.push_back(Voice{.name = std::string(name), .rank = rank, .ends_ms = now_ms + *length_ms});
  if (const auto last = last_start_ms_.find(name); last != last_start_ms_.end()) {
    last->second = now_ms;
  } else {
    last_start_ms_.emplace(std::string(name), now_ms);
  }
  ++played_total_;
  verdict.play = true;
  return verdict;
}

void ArenaSoundBudget::Clear() {
  voices_.clear();
  last_start_ms_.clear();
}

std::size_t ArenaSoundBudget::busy(std::uint64_t now_ms) const noexcept {
  return static_cast<std::size_t>(std::ranges::count_if(
      voices_, [now_ms](const Voice& voice) { return voice.ends_ms > now_ms; }));
}

void ArenaSoundBudget::Release(std::uint64_t now_ms) {
  std::erase_if(voices_, [now_ms](const Voice& voice) { return voice.ends_ms <= now_ms; });
}

const ArenaSoundBudget::Voice* ArenaSoundBudget::Yielding(ArenaSoundRank rank) const noexcept {
  const Voice* yielding = nullptr;
  for (const Voice& voice : voices_) {
    if (voice.rank >= rank) {
      continue;
    }
    // TOUTES LES VOIX D'UN NOM SE TAISENT ENSEMBLE : un nom qui sonne aussi à
    // un rang égal ou supérieur ne cède pas, il ferait taire plus fort que lui.
    const bool outranked = std::ranges::any_of(voices_, [&voice, rank](const Voice& other) {
      return other.name == voice.name && other.rank >= rank;
    });
    if (outranked) {
      continue;
    }
    if (yielding == nullptr || voice.rank < yielding->rank ||
        (voice.rank == yielding->rank && voice.ends_ms < yielding->ends_ms)) {
      yielding = &voice;
    }
  }
  return yielding;
}

}  // namespace moonclient
