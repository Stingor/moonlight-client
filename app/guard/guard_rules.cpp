#include "guard/guard_rules.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <utility>

#include "ro/net/cheat_report.h"

namespace moonclient {

namespace {

#if defined(MOONCLIENT_GUARD_SIGNATURES)
// La table hors dépôt : voir `KnownSignatures`.
constexpr GuardSignature kSignatures[] = {
#include "guard/guard_signatures.local.inc"
};
#endif

std::string Lowered(std::string_view text) {
  std::string lowered(text);
  std::ranges::transform(lowered, lowered.begin(), [](char letter) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
  });
  return lowered;
}

bool Matches(const GuardSignature& signature, GuardProbe probe, std::string_view lowered) {
  if (probe == GuardProbe::kProcess) {
    if (signature.probe == GuardProbe::kProcess) {
      return lowered == signature.pattern;
    }
    return signature.probe == GuardProbe::kProcessSubstring &&
           lowered.find(signature.pattern) != std::string_view::npos;
  }
  return signature.probe == probe && lowered.find(signature.pattern) != std::string_view::npos;
}

std::uint32_t Permille(std::uint64_t part, std::uint64_t whole) {
  constexpr std::uint64_t kPermille = 1000;
  return whole == 0 ? 0 : static_cast<std::uint32_t>(part * kPermille / whole);
}

}  // namespace

std::span<const GuardSignature> KnownSignatures() {
#if defined(MOONCLIENT_GUARD_SIGNATURES)
  return kSignatures;
#else
  return {};
#endif
}

std::optional<GuardFinding> MatchSignature(std::span<const GuardSignature> table, GuardProbe probe,
                                           std::string_view name) {
  const std::string lowered = Lowered(name);
  for (const GuardSignature& signature : table) {
    if (Matches(signature, probe, lowered)) {
      return GuardFinding{.tool = std::string(signature.tool), .detail = std::string(name)};
    }
  }
  return std::nullopt;
}

void KeyProvenance::NoteRawKey(bool has_device) {
  if (has_device) {
    ++raw_from_device_;
  } else {
    ++raw_injected_;
  }
}

void KeyProvenance::NoteKeyMessage(bool key_down_in_system) {
  ++messages_;
  if (!key_down_in_system) {
    ++messages_posted_;
  }
}

std::optional<GuardFinding> KeyProvenance::TakeVerdict() {
  if (messages_ < kKeysPerVerdict) {
    return std::nullopt;
  }
  const std::uint32_t raw = raw_from_device_ + raw_injected_;
  const std::uint32_t injected_share = Permille(raw_injected_, raw);
  const std::uint32_t posted_share = Permille(messages_posted_, messages_);
  const std::uint32_t messages = messages_;
  const std::uint32_t posted = messages_posted_;
  const std::uint32_t injected = raw_injected_;
  *this = KeyProvenance();

  if (posted_share >= kSyntheticSharePermille) {
    return GuardFinding{
        .tool = "PostMessage",
        .detail = std::format("{}/{} touches postees a la fenetre", posted, messages)};
  }
  // ⚠ L'entrée brute peut manquer — enregistrement refusé, ou repris par une
  // autre partie du programme : sans elle, aucune part injectée ne se mesure.
  if (raw * 2 >= messages && injected_share >= kSyntheticSharePermille) {
    return GuardFinding{.tool = "SendInput",
                        .detail = std::format("{}/{} touches injectees", injected, raw)};
  }
  return std::nullopt;
}

std::optional<GuardFinding> ClockAgreement::Note(std::uint64_t counter_ns,
                                                 std::uint64_t kernel_ns) {
  if (!started_ || kernel_ns < kernel_start_ns_ || counter_ns < counter_start_ns_) {
    started_ = true;
    counter_start_ns_ = counter_ns;
    kernel_start_ns_ = kernel_ns;
    return std::nullopt;
  }
  const std::uint64_t kernel_elapsed = kernel_ns - kernel_start_ns_;
  if (kernel_elapsed < kClockWindowNs) {
    return std::nullopt;
  }
  const std::uint64_t counter_elapsed = counter_ns - counter_start_ns_;
  counter_start_ns_ = counter_ns;
  kernel_start_ns_ = kernel_ns;

  if (kernel_elapsed > kClockWindowNs * kClockStallFactor) {
    return std::nullopt;
  }
  const std::uint32_t ratio_permille = Permille(counter_elapsed, kernel_elapsed);
  constexpr std::uint32_t kAgreed = 1000;
  const std::uint32_t gap =
      ratio_permille > kAgreed ? ratio_permille - kAgreed : kAgreed - ratio_permille;
  if (gap < kClockDisagreementPermille) {
    strikes_ = 0;
    return std::nullopt;
  }
  if (++strikes_ < kClockStrikes) {
    return std::nullopt;
  }
  return GuardFinding{.tool = "Speedhack",
                      .detail = std::format("compteur a x{}.{:03} du noyau",
                                            ratio_permille / kAgreed, ratio_permille % kAgreed)};
}

bool GuardSpokesman::Note(GuardFinding finding) {
  if (findings_.size() >= kMaxFindings ||
      std::ranges::find(findings_, finding) != findings_.end()) {
    return false;
  }
  findings_.push_back(std::move(finding));
  return true;
}

std::optional<GuardFinding> GuardSpokesman::Due(bool session_verified, std::uint32_t now_ms) {
  if (!session_verified) {
    session_open_ = false;
    return std::nullopt;
  }
  if (!session_open_) {
    session_open_ = true;
    told_ = 0;
  }
  if (told_ >= findings_.size()) {
    return std::nullopt;
  }
  constexpr std::uint32_t kSpacingMs = ro::net::kCheatReportIntervalMs + kReportMarginMs;
  if (has_sent_ && now_ms - last_sent_ms_ < kSpacingMs) {
    return std::nullopt;
  }
  has_sent_ = true;
  last_sent_ms_ = now_ms;
  return findings_[told_++];
}

}  // namespace moonclient
