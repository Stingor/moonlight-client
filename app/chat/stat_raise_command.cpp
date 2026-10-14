// Les montées de caractéristique tapées dans la conversation. Voir l'en-tête.

#include "chat/stat_raise_command.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

#include "ro/core/ascii.h"
#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/net/character.h"
#include "ro/net/connection.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

struct StatRaiseName {
  std::string_view command;
  StatRaiseKind kind = StatRaiseKind::kNone;
  std::uint16_t parameter = 0;
};

// Les numéros de l'`enum _sp` du serveur.
constexpr std::array<StatRaiseName, 12> kStatRaiseNames = {{
    {"/str+", StatRaiseKind::kBase, 13},
    {"/agi+", StatRaiseKind::kBase, 14},
    {"/vit+", StatRaiseKind::kBase, 15},
    {"/int+", StatRaiseKind::kBase, 16},
    {"/dex+", StatRaiseKind::kBase, 17},
    {"/luk+", StatRaiseKind::kBase, 18},
    {"/pow+", StatRaiseKind::kTrait, 219},
    {"/sta+", StatRaiseKind::kTrait, 220},
    {"/wis+", StatRaiseKind::kTrait, 221},
    {"/spl+", StatRaiseKind::kTrait, 222},
    {"/con+", StatRaiseKind::kTrait, 223},
    {"/crt+", StatRaiseKind::kTrait, 224},
}};

constexpr int kDecimalBase = 10;

// Les blancs qui séparent deux mots. ⚠ UN OCTET NON ASCII N'EN EST JAMAIS UN :
// le second octet d'un caractère coréen ne coupe pas la ligne.
[[nodiscard]] bool IsBlank(char letter) {
  switch (letter) {
    case ' ':
    case '\t':
    case '\n':
    case '\v':
    case '\f':
    case '\r':
      return true;
    default:
      return false;
  }
}

// Le mot de rang `wanted`, s'il existe. ⚠ UN MOT QUI COMMENCE PAR « // » CLÔT
// LA LIGNE : ni lui ni ce qui suit ne compte.
[[nodiscard]] std::optional<std::string_view> WordAt(std::string_view line, std::size_t wanted) {
  std::size_t rank = 0;
  std::size_t at = 0;
  while (at < line.size()) {
    if (IsBlank(line[at])) {
      ++at;
      continue;
    }
    if (line.substr(at).starts_with("//")) {
      return std::nullopt;
    }
    std::size_t end = at;
    while (end < line.size() && !IsBlank(line[end])) {
      ++end;
    }
    if (rank == wanted) {
      return line.substr(at, end - at);
    }
    ++rank;
    at = end;
  }
  return std::nullopt;
}

}  // namespace

std::int32_t LeadingInteger(std::string_view word) {
  std::size_t at = 0;
  bool negative = false;
  if (at < word.size() && (word[at] == '+' || word[at] == '-')) {
    negative = word[at] == '-';
    ++at;
  }
  // Compté en 64 bits, puis ramené à la borne : un nombre trop long ne
  // déborde jamais.
  constexpr std::int64_t kLargest = std::numeric_limits<std::int32_t>::max();
  constexpr std::int64_t kSmallest = std::numeric_limits<std::int32_t>::min();
  std::int64_t value = 0;
  for (; at < word.size() && word[at] >= '0' && word[at] <= '9'; ++at) {
    value = value * kDecimalBase + (word[at] - '0');
    if (value > -kSmallest) {
      value = -kSmallest;
    }
  }
  value = negative ? -value : value;
  if (value > kLargest) {
    return std::numeric_limits<std::int32_t>::max();
  }
  return static_cast<std::int32_t>(value);
}

StatRaiseCommand ReadStatRaiseCommand(std::string_view line) {
  const std::optional<std::string_view> name = WordAt(line, 0);
  if (!name.has_value()) {
    return {};
  }
  for (const StatRaiseName& known : kStatRaiseNames) {
    if (!ro::core::AsciiEqualsFolded(*name, known.command)) {
      continue;
    }
    const std::optional<std::string_view> number = WordAt(line, 1);
    if (!number.has_value()) {
      return {};
    }
    // RÉDUITE À LA LARGEUR DU CHAMP, comme le jeu la tronque : l'octet bas,
    // ou les deux octets bas.
    const auto typed = static_cast<std::uint32_t>(LeadingInteger(*number));
    return {.kind = known.kind,
            .parameter = known.parameter,
            .amount = known.kind == StatRaiseKind::kBase
                          ? static_cast<std::uint16_t>(static_cast<std::uint8_t>(typed))
                          : static_cast<std::uint16_t>(typed)};
  }
  return {};
}

bool RunStatRaiseCommand(ro::net::Connection& link, ro::sim::World& world, std::string_view line) {
  const StatRaiseCommand command = ReadStatRaiseCommand(line);
  if (command.kind == StatRaiseKind::kNone) {
    return false;
  }
  if (!link.is_open()) {
    return true;
  }
  const ro::core::Result<void> sent =
      command.kind == StatRaiseKind::kBase
          ? ro::net::RequestStatUp(link, world, command.parameter,
                                   static_cast<std::uint8_t>(command.amount))
          : ro::net::RequestTraitStatUp(link, world, command.parameter, command.amount);
  if (!sent) {
    ro::core::LogWarning("montée de caractéristique non émise — {}",
                         ro::core::Describe(sent.error()));
  }
  return true;
}

}  // namespace moonclient
