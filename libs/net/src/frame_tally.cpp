#include "ro/net/frame_tally.h"

#include <algorithm>
#include <string_view>

#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

// Un opcode s'écrit sur quatre chiffres hexadécimaux de quatre bits.
constexpr int kOpcodeDigits = 4;
constexpr int kBitsPerDigit = 4;
constexpr std::uint16_t kDigitMask = 0xF;

// Ce qui sépare deux opcodes sur une même ligne : assez pour que l'œil les
// sépare, sans tabulation — le journal du chat n'en connaît pas.
constexpr std::string_view kOpcodeSeparator = "  ";

}  // namespace

std::string HexOpcode(std::uint16_t opcode) {
  constexpr std::string_view kHexDigits = "0123456789abcdef";
  std::string text = "0x";
  for (int digit = kOpcodeDigits - 1; digit >= 0; --digit) {
    text.push_back(kHexDigits[(opcode >> (digit * kBitsPerDigit)) & kDigitMask]);
  }
  return text;
}

void FrameTally::Record(const Frame& frame, FrameOutcome outcome) {
  // L'AUTEUR ATTACHÉ COMPTE POUR LUI : la liaison l'a lu, et le traducteur de
  // la trame qu'il décrit l'a pris avec elle.
  if (!frame.preamble.empty()) {
    Record(proto::ZcMoonlightChatAuthor::kOpcode, FrameOutcome::kApplied);
  }
  Record(frame.opcode, outcome);
  const std::size_t kept = std::min(frame.bytes.size(), kRecentHeadBytes);
  recent_.push_back(
      Recent{.serial = frames_,
             .opcode = frame.opcode,
             .outcome = outcome,
             .size = static_cast<std::uint32_t>(frame.bytes.size()),
             .head = std::vector<std::uint8_t>(
                 frame.bytes.begin(), frame.bytes.begin() + static_cast<std::ptrdiff_t>(kept))});
  if (recent_.size() > kRecentKept) {
    recent_.pop_front();
  }
}

void FrameTally::Record(std::uint16_t opcode, FrameOutcome outcome) {
  Entry& entry = by_opcode_[opcode];
  entry.opcode = opcode;
  ++frames_;
  switch (outcome) {
    case FrameOutcome::kApplied:
      ++entry.applied;
      return;
    case FrameOutcome::kIgnored:
      ++entry.ignored;
      return;
    case FrameOutcome::kUnhandled:
      ++entry.unhandled;
      ++missing_frames_;
      return;
    case FrameOutcome::kUndecodable:
      ++entry.undecodable;
      ++missing_frames_;
      return;
  }
}

std::vector<FrameTally::Entry> FrameTally::All() const {
  std::vector<Entry> entries;
  entries.reserve(by_opcode_.size());
  for (const auto& [opcode, entry] : by_opcode_) {
    entries.push_back(entry);
  }
  return entries;
}

std::vector<FrameTally::Entry> FrameTally::Missing() const {
  std::vector<Entry> entries;
  for (const auto& [opcode, entry] : by_opcode_) {
    if (entry.missing() > 0) {
      entries.push_back(entry);
    }
  }
  // Tri STABLE sur le seul nombre de trames perdues : la source vient déjà
  // triée par opcode, et un tri stable conserve cet ordre à égalité. Comparer
  // aussi l'opcode donnerait le même résultat en disant deux fois la règle.
  std::stable_sort(entries.begin(), entries.end(), [](const Entry& left, const Entry& right) {
    return left.missing() > right.missing();
  });
  return entries;
}

void FrameTally::Clear() {
  by_opcode_.clear();
  frames_ = 0;
  missing_frames_ = 0;
  recent_.clear();
}

namespace {

// Le pluriel français d'un décompte : rien à un, « s » au-delà. Zéro prend le
// singulier.
[[nodiscard]] std::string_view Plural(std::size_t count) { return count > 1 ? "s" : ""; }

// La première ligne : ce qui est arrivé, et ce qui s'en est perdu.
[[nodiscard]] std::string SummaryLine(const FrameTally& tally,
                                      const std::vector<FrameTally::Entry>& missing) {
  if (tally.frames() == 0) {
    return "trames : aucune reçue";
  }
  const std::string received =
      "trames : " + std::to_string(tally.frames()) + " reçue" + std::string(Plural(tally.frames()));
  if (missing.empty()) {
    return received + ", toutes traduites";
  }
  std::uint32_t undecodable = 0;
  for (const FrameTally::Entry& entry : missing) {
    undecodable += entry.undecodable;
  }
  std::string line = received + ", " + std::to_string(tally.missing_frames()) + " perdue" +
                     std::string(Plural(tally.missing_frames())) + " sur " +
                     std::to_string(missing.size()) + " opcode" +
                     std::string(Plural(missing.size()));
  // Les indéchiffrables ne se réparent pas au même endroit que les non traités :
  // l'un demande d'écrire un cas, l'autre de corriger une disposition. Les
  // confondre enverrait chercher au mauvais endroit.
  if (undecodable > 0) {
    line += " (dont " + std::to_string(undecodable) + " indéchiffrable" +
            std::string(Plural(undecodable)) + ")";
  }
  return line;
}

}  // namespace

std::vector<std::string> DescribedFrameTally(const FrameTally& tally, std::size_t max_lines) {
  std::vector<std::string> lines;
  if (max_lines == 0) {
    return lines;
  }
  const std::vector<FrameTally::Entry> missing = tally.Missing();
  lines.push_back(SummaryLine(tally, missing));
  if (missing.empty() || lines.size() == max_lines) {
    return lines;
  }

  // Ce que les lignes restantes peuvent nommer. Au-delà, le reste se compte
  // plutôt que de se taire.
  const std::size_t room = (max_lines - lines.size()) * kTallyOpcodesPerLine;
  const std::size_t shown = std::min(missing.size(), room);
  for (std::size_t first = 0; first < shown; first += kTallyOpcodesPerLine) {
    const std::size_t last = std::min(first + kTallyOpcodesPerLine, shown);
    std::string line;
    for (std::size_t index = first; index < last; ++index) {
      if (index > first) {
        line += kOpcodeSeparator;
      }
      line += HexOpcode(missing[index].opcode) + " ×" + std::to_string(missing[index].missing());
    }
    lines.push_back(line);
  }
  if (shown < missing.size()) {
    lines.back() += std::string(kOpcodeSeparator) + "… +" + std::to_string(missing.size() - shown);
  }
  return lines;
}

}  // namespace ro::net
