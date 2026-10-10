#include "items/warp_memo_report.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace moonclient {
namespace {

// Les trois noms de la commande, en minuscules. Le dernier est en UTF-8.
constexpr std::array<std::string_view, 3> kMemoCommandNames = {"/memo", "/rldjr", "/기억"};

// Le premier octet qui n'est plus de l'ASCII : au-delà, la casse ne compte pas.
constexpr unsigned char kFirstNonAscii = 0x80;

[[nodiscard]] char Folded(char letter) {
  const auto byte = static_cast<unsigned char>(letter);
  return byte < kFirstNonAscii ? static_cast<char>(std::tolower(byte)) : letter;
}

}  // namespace

bool IsWarpMemoCommand(std::string_view line) {
  const std::string_view word = line.substr(0, line.find(' '));
  return std::ranges::any_of(kMemoCommandNames, [word](std::string_view name) {
    return std::ranges::equal(name, word, [](char a, char b) { return Folded(a) == Folded(b); });
  });
}

std::optional<WarpMemoLine> WarpMemoLineOfAck(std::uint8_t code) {
  using ro::vfs::WarpMemoMessage;
  switch (static_cast<WarpMemoAck>(code)) {
    case WarpMemoAck::kSaved:
      return WarpMemoLine{WarpMemoMessage::kSaved, kWarpMemoCyan};
    case WarpMemoAck::kSkillTooLow:
      return WarpMemoLine{WarpMemoMessage::kSkillTooLow, kWarpMemoRed};
    case WarpMemoAck::kSkillNotLearned:
      return WarpMemoLine{WarpMemoMessage::kSkillNotLearned, kWarpMemoRed};
    case WarpMemoAck::kCannotSaveHere:
      return WarpMemoLine{WarpMemoMessage::kCannotSaveHere, kWarpMemoRed};
  }
  return std::nullopt;
}

std::optional<WarpMemoLine> WarpMemoLineOfMapInfo(std::int16_t code) {
  using ro::vfs::WarpMemoMessage;
  switch (static_cast<MapInfoNotice>(code)) {
    case MapInfoNotice::kTeleportForbidden:
      return WarpMemoLine{WarpMemoMessage::kTeleportForbidden, kWarpMemoRed};
    case MapInfoNotice::kMemoForbidden:
      return WarpMemoLine{WarpMemoMessage::kMemoForbidden, kWarpMemoCyan};
    case MapInfoNotice::kSkillForbidden:
      return WarpMemoLine{WarpMemoMessage::kSkillForbiddenHere, kWarpMemoCyan};
    case MapInfoNotice::kItemForbidden:
      return WarpMemoLine{WarpMemoMessage::kItemForbiddenHere, kWarpMemoCyan};
  }
  return std::nullopt;
}

}  // namespace moonclient
