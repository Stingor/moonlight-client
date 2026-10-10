#include "ro/formats/gr2_arith.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace ro::formats {
namespace {

// Les bornes et le code tiennent sur 31 bits.
constexpr std::uint32_t kCodeBits = 31;
constexpr std::uint32_t kCodeMask = 0x7FFFFFFFU;
// Le bit de tête, et celui qui le suit : quand les bornes s'accordent sur le
// premier, il est acquis ; quand elles l'encadrent de trop près, le second
// sert à les écarter.
constexpr std::uint32_t kTopBit = 0x40000000U;
constexpr std::uint32_t kSecondBit = 0x20000000U;
constexpr std::uint32_t kBelowSecondBit = kSecondBit - 1;

// Le total au-delà duquel un modèle divise ses fréquences.
constexpr std::uint32_t kRescaleTotal = 0x4000;
// Le poids d'un échappement neuf, et celui d'un symbole qu'on vient d'apprendre.
constexpr std::int32_t kInitialEscapeWeight = 3;
constexpr std::int32_t kLearnedWeight = 2;
// Les cases s'allouent par quatre, avec deux de marge au moins.
constexpr std::uint32_t kSlotPadding = 5;
constexpr std::uint32_t kSlotAlignment = ~std::uint32_t{3};
// Un modèle de moins de six symboles ne découpe pas ses cases en seaux : tout
// va au dernier.
constexpr std::uint32_t kFewestBucketedSymbols = 6;
constexpr std::uint32_t kLastBucket = 15;
constexpr std::uint32_t kLargestBucketShift = 15;
constexpr std::uint32_t kBucketShiftCount = 16;

}  // namespace

std::uint32_t Gr2BitReader::Bit() noexcept {
  const std::size_t byte = bit_ / kBitsPerByte;
  const std::uint32_t value =
      byte < data_.size() ? (static_cast<std::uint32_t>(data_[byte]) >> (bit_ % kBitsPerByte)) & 1U
                          : 0U;
  ++bit_;
  return value;
}

std::uint32_t Gr2BitReader::Bits(std::uint32_t count) noexcept {
  std::uint32_t value = 0;
  for (std::uint32_t index = 0; index < count; ++index) {
    value |= Bit() << index;
  }
  return value;
}

Gr2RangeDecoder::Gr2RangeDecoder(std::span<const std::uint8_t> data,
                                 std::size_t start_byte) noexcept
    : bits_(data, start_byte), high_(kCodeMask) {
  for (std::uint32_t index = 0; index < kCodeBits; ++index) {
    code_ = (code_ << 1) | bits_.Bit();
  }
}

std::uint32_t Gr2RangeDecoder::Target(std::uint32_t total) const noexcept {
  const std::uint64_t span = std::uint64_t{high_} - low_ + 1;
  const std::uint64_t offset = std::uint64_t{code_} - low_ + 1;
  if (total == 0 || code_ < low_) {
    return 0;
  }
  return static_cast<std::uint32_t>((std::uint64_t{total} * offset - 1) / span);
}

void Gr2RangeDecoder::Narrow(const Gr2Interval& interval) noexcept {
  if (interval.total == 0) {
    return;
  }
  const std::uint64_t span = std::uint64_t{high_} - low_ + 1;
  std::uint32_t high = static_cast<std::uint32_t>(
      low_ + (std::uint64_t{interval.cumulative} + interval.frequency) * span / interval.total - 1);
  std::uint32_t low =
      static_cast<std::uint32_t>(low_ + std::uint64_t{interval.cumulative} * span / interval.total);
  std::uint32_t code = code_;
  // Le bit de tête est acquis : il sort, et le code en lit un neuf.
  while (((high ^ low) & kTopBit) == 0) {
    low = (low << 1) & kCodeMask;
    high = ((high << 1) | 1U) & kCodeMask;
    code = ((code << 1) | bits_.Bit()) & kCodeMask;
  }
  // Les bornes encadrent le milieu de trop près : on écarte l'intervalle
  // autour de lui.
  while ((low & kSecondBit) != 0 && (high & kSecondBit) == 0) {
    low = (low & kBelowSecondBit) << 1;
    high = ((high << 1) | kTopBit | 1U) & kCodeMask;
    code = (((code ^ kSecondBit) << 1) | bits_.Bit()) & kCodeMask;
  }
  low_ = low;
  high_ = high;
  code_ = code;
}

std::uint32_t Gr2RangeDecoder::Uniform(std::uint32_t count) noexcept {
  if (count == 0) {
    return 0;
  }
  const std::uint32_t value = std::min(Target(count), count - 1);
  Narrow(Gr2Interval{value, 1, count});
  return value;
}

Gr2AdaptiveModel::Gr2AdaptiveModel(std::uint32_t capacity)
    : capacity_(capacity),
      frequency_((capacity + kSlotPadding) & kSlotAlignment),
      value_((capacity + kSlotPadding) & kSlotAlignment) {
  ChooseBuckets(capacity + 1);
  Add(0, kInitialEscapeWeight);
}

std::size_t Gr2AdaptiveModel::BucketOf(std::size_t slot) const noexcept {
  return slot >= bucket_limit_ ? kLastBucket : slot >> bucket_shift_;
}

void Gr2AdaptiveModel::Add(std::size_t slot, std::int32_t delta) noexcept {
  for (std::size_t bucket = BucketOf(slot); bucket < kBuckets; ++bucket) {
    cumulative_[bucket] = static_cast<std::uint16_t>(cumulative_[bucket] + delta);
  }
  frequency_[slot] = static_cast<std::uint16_t>(frequency_[slot] + delta);
}

// Le découpage en seaux qui équilibre le mieux `symbol_count` cases : quinze
// seaux d'une même largeur, puissance de deux, et un dernier qui prend le
// reste. On garde la largeur dont le dernier seau est le plus petit.
void Gr2AdaptiveModel::ChooseBuckets(std::uint32_t symbol_count) noexcept {
  if (symbol_count < kFewestBucketedSymbols) {
    bucket_shift_ = kLargestBucketShift;
    bucket_limit_ = 0;
    return;
  }
  std::uint32_t best_rest = UINT32_MAX;
  std::uint32_t best_shift = 0;
  for (std::uint32_t shift = 0; shift < kBucketShiftCount; ++shift) {
    const std::uint32_t width = 1U << shift;
    const std::uint32_t groups =
        std::min<std::uint32_t>((width + symbol_count - 1) / width, kBucketShiftCount);
    const std::uint32_t rest = std::max(symbol_count - width * (groups - 1), width);
    if (rest < best_rest) {
      best_shift = shift;
      best_rest = rest;
    }
    if (width > symbol_count) {
      break;
    }
  }
  bucket_shift_ = best_shift;
  bucket_limit_ = kLastBucket << best_shift;
}

void Gr2AdaptiveModel::Rescale() {
  ChooseBuckets(used_ + 1);
  std::array<std::uint32_t, kBuckets> sums{};
  const std::size_t escape_bucket = bucket_limit_ != 0 ? 0 : kLastBucket;
  frequency_[0] = static_cast<std::uint16_t>(frequency_[0] >> 1);
  sums[escape_bucket] = frequency_[0];
  std::uint32_t best_frequency = 0;
  std::size_t best_slot = 0;
  std::size_t slot = 1;
  while (slot <= used_) {
    if (frequency_[slot] > 1) {
      frequency_[slot] = static_cast<std::uint16_t>(frequency_[slot] >> 1);
      if (frequency_[slot] > best_frequency) {
        best_frequency = frequency_[slot];
        best_slot = slot;
      }
      sums[BucketOf(slot)] += frequency_[slot];
      ++slot;
      continue;
    }
    // Une case qui retomberait à zéro s'efface : la dernière prend sa place.
    if (slot >= used_) {
      frequency_[slot] = 0;
      --used_;
      break;
    }
    frequency_[slot] = frequency_[used_];
    frequency_[used_] = 0;
    value_[slot] = value_[used_];
    --used_;
  }
  if (best_frequency != 0) {
    std::size_t front =
        used_ >= bucket_limit_ ? bucket_limit_ : (used_ >> bucket_shift_) << bucket_shift_;
    front = std::max<std::size_t>(front, 1);
    if (best_slot != front) {
      const std::uint16_t best = frequency_[best_slot];
      const std::uint16_t displaced = frequency_[front];
      frequency_[front] = best;
      sums[BucketOf(front)] += best - displaced;
      sums[BucketOf(best_slot)] += displaced - best;
      frequency_[best_slot] = displaced;
      std::swap(value_[front], value_[best_slot]);
    }
  }
  if (used_ != capacity_ && frequency_[0] == 0) {
    frequency_[0] = kLearnedWeight;
    sums[escape_bucket] += kLearnedWeight;
  }
  std::uint32_t running = 0;
  for (std::size_t bucket = 0; bucket < kBuckets; ++bucket) {
    running += sums[bucket];
    cumulative_[bucket] = static_cast<std::uint16_t>(running);
  }
}

void Gr2AdaptiveModel::Commit(std::size_t slot) noexcept {
  frequency_[slot] = static_cast<std::uint16_t>(frequency_[slot] + 1);
  if (slot != 0 || used_ >= capacity_) {
    return;
  }
  ++used_;
  Add(used_, kLearnedWeight);
  // Le dernier symbole est appris : l'échappement ne sert plus.
  if (used_ == capacity_) {
    Add(0, -static_cast<std::int32_t>(frequency_[0]));
  }
}

Gr2AdaptiveModel::Decoded Gr2AdaptiveModel::Decode(Gr2RangeDecoder& decoder) {
  if (cumulative_[kLastBucket] >= kRescaleTotal) {
    Rescale();
  }
  const std::uint32_t total = cumulative_[kLastBucket];
  const std::uint32_t target = decoder.Target(total);
  std::size_t bucket = 0;
  while (bucket < kLastBucket && target >= cumulative_[bucket]) {
    ++bucket;
  }
  std::uint32_t base = bucket == 0 ? 0 : cumulative_[bucket - 1];
  std::size_t slot = bucket == kLastBucket ? bucket_limit_ : bucket << bucket_shift_;
  for (std::size_t later = bucket; later < kBuckets; ++later) {
    cumulative_[later] = static_cast<std::uint16_t>(cumulative_[later] + 1);
  }
  const std::size_t last_slot = std::min<std::size_t>(used_, frequency_.size() - 1);
  while (slot < last_slot && target >= base + frequency_[slot]) {
    base += frequency_[slot];
    ++slot;
  }
  slot = std::min(slot, last_slot);
  decoder.Narrow(Gr2Interval{base, frequency_[slot], total});
  const bool escaped = slot == 0;
  Commit(slot);
  return Decoded{escaped, escaped ? std::uint16_t{0} : value_[slot]};
}

void Gr2AdaptiveModel::Learn(std::uint16_t value) noexcept { value_[used_] = value; }

Gr2AdaptiveModel::Encoded Gr2AdaptiveModel::Encode(std::uint16_t value) {
  if (cumulative_[kLastBucket] >= kRescaleTotal) {
    Rescale();
  }
  const std::uint32_t total = cumulative_[kLastBucket];
  std::size_t slot = 0;
  for (std::size_t candidate = 1; candidate <= used_; ++candidate) {
    if (value_[candidate] == value) {
      slot = candidate;
      break;
    }
  }
  std::uint32_t base = 0;
  for (std::size_t before = 0; before < slot; ++before) {
    base += frequency_[before];
  }
  for (std::size_t later = BucketOf(slot); later < kBuckets; ++later) {
    cumulative_[later] = static_cast<std::uint16_t>(cumulative_[later] + 1);
  }
  const Gr2Interval interval{base, frequency_[slot], total};
  Commit(slot);
  if (slot == 0) {
    Learn(value);
  }
  return Encoded{interval, slot == 0};
}

}  // namespace ro::formats
