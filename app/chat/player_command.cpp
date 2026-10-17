#include "chat/player_command.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "ro/core/ascii.h"

namespace moonclient {
namespace {

// Comment la ligne porte son argument : voir l'en-tête.
enum class Takes : std::uint8_t {
  kNothing,
  kOneName,
  kRestOfLine,
  kVolume,
};

struct Word {
  std::string_view word;
  PlayerCommandKind kind;
  Takes takes;
};

// LES MOTS ET CE QU'ILS ATTENDENT. Le premier de chaque commande est son nom
// dans le client du joueur ; ceux qui suivent sont ses doubles.
//
// ⚠ « /w » SEUL DEMANDE LE COMPTE DES JOUEURS : suivi d'un nom, c'est un
// chuchotement, que `ReadChatIntent` a déjà pris avant d'arriver ici.
constexpr auto kWords = std::to_array<Word>({
    {"/sit", PlayerCommandKind::kSitOrStand, Takes::kNothing},
    {"/stand", PlayerCommandKind::kSitOrStand, Takes::kNothing},
    {"/dkswrl", PlayerCommandKind::kSitOrStand, Takes::kNothing},
    {"/tjrl", PlayerCommandKind::kSitOrStand, Takes::kNothing},
    {"/deal", PlayerCommandKind::kDeal, Takes::kOneName},
    {"/rjfo", PlayerCommandKind::kDeal, Takes::kOneName},
    {"/ryghks", PlayerCommandKind::kDeal, Takes::kOneName},
    {"/organize", PlayerCommandKind::kOrganizeParty, Takes::kOneName},
    {"/ruftjd", PlayerCommandKind::kOrganizeParty, Takes::kOneName},
    {"/invite", PlayerCommandKind::kInviteToParty, Takes::kRestOfLine},
    {"/cheo", PlayerCommandKind::kInviteToParty, Takes::kRestOfLine},
    {"/leave", PlayerCommandKind::kLeaveParty, Takes::kNothing},
    {"/xkfxhl", PlayerCommandKind::kLeaveParty, Takes::kNothing},
    {"/expel", PlayerCommandKind::kExpelFromParty, Takes::kOneName},
    {"/wpaud", PlayerCommandKind::kExpelFromParty, Takes::kOneName},
    {"/guild", PlayerCommandKind::kCreateGuild, Takes::kOneName},
    {"/rlfem", PlayerCommandKind::kCreateGuild, Takes::kOneName},
    {"/guildinvite", PlayerCommandKind::kInviteToGuild, Takes::kRestOfLine},
    {"/rlfemcheo", PlayerCommandKind::kInviteToGuild, Takes::kRestOfLine},
    {"/breakguild", PlayerCommandKind::kBreakGuild, Takes::kOneName},
    {"/gocp", PlayerCommandKind::kBreakGuild, Takes::kOneName},
    {"/accept", PlayerCommandKind::kAllowPartyInvites, Takes::kNothing},
    {"/refuse", PlayerCommandKind::kRefusePartyInvites, Takes::kNothing},
    {"/notrade", PlayerCommandKind::kToggleDealRefusal, Takes::kNothing},
    {"/nt", PlayerCommandKind::kToggleDealRefusal, Takes::kNothing},
    {"/who", PlayerCommandKind::kUserCount, Takes::kNothing},
    {"/w", PlayerCommandKind::kUserCount, Takes::kNothing},
    {"/wjqthrwk", PlayerCommandKind::kUserCount, Takes::kNothing},
    {"/snrn", PlayerCommandKind::kUserCount, Takes::kNothing},
    {"/help", PlayerCommandKind::kHelp, Takes::kNothing},
    {"/h", PlayerCommandKind::kHelp, Takes::kNothing},
    {"/emotion", PlayerCommandKind::kEmotionList, Takes::kNothing},
    {"/v", PlayerCommandKind::kSoundVolume, Takes::kVolume},
    {"/bv", PlayerCommandKind::kMusicVolume, Takes::kVolume},
});

constexpr char kSpace = ' ';
constexpr int kDecimalBase = 10;
// Le nombre de mots d'une commande à un nom : le mot, puis le nom.
constexpr std::size_t kWordsWithOneName = 2;

// Les mots de la ligne, séparés par des espaces — plusieurs comptent pour une.
[[nodiscard]] std::vector<std::string_view> SplitOnSpaces(std::string_view line) {
  std::vector<std::string_view> words;
  while (!line.empty()) {
    const std::size_t start = line.find_first_not_of(kSpace);
    if (start == std::string_view::npos) {
      break;
    }
    line.remove_prefix(start);
    const std::size_t end = line.find(kSpace);
    words.push_back(line.substr(0, end));
    line.remove_prefix(end == std::string_view::npos ? line.size() : end);
  }
  return words;
}

// UN ENTIER LU COMME `atoi` : un signe facultatif, des chiffres, et rien de ce
// qui suit ; sans chiffre, zéro. Borné à l'échelle, sans jamais déborder.
[[nodiscard]] int CommandVolumeFrom(std::string_view digits) {
  bool negative = false;
  if (!digits.empty() && (digits.front() == '-' || digits.front() == '+')) {
    negative = digits.front() == '-';
    digits.remove_prefix(1);
  }
  int value = 0;
  for (const char digit : digits) {
    if (digit < '0' || digit > '9') {
      break;
    }
    value = std::min(value * kDecimalBase + (digit - '0'), kLoudestCommandVolume);
  }
  return negative ? 0 : value;
}

}  // namespace

PlayerCommand ReadPlayerCommand(std::string_view line) {
  const std::vector<std::string_view> words = SplitOnSpaces(line);
  if (words.empty()) {
    return {};
  }
  const auto found = std::ranges::find_if(kWords, [&](const Word& known) {
    return ro::core::AsciiEqualsFolded(words.front(), known.word);
  });
  if (found == kWords.end()) {
    return {};
  }
  PlayerCommand command{.kind = found->kind};
  switch (found->takes) {
    case Takes::kNothing:
      // « /w nom » est un chuchotement, jamais une question.
      if (found->kind == PlayerCommandKind::kUserCount && words.size() > 1) {
        return {};
      }
      break;
    case Takes::kOneName:
      if (words.size() != kWordsWithOneName) {
        command.malformed = true;
        break;
      }
      command.name = std::string(words[1]);
      break;
    case Takes::kRestOfLine: {
      const std::size_t space = line.find(kSpace, line.find_first_not_of(kSpace));
      if (space == std::string_view::npos || words.size() < kWordsWithOneName) {
        command.malformed = true;
        break;
      }
      command.name = std::string(line.substr(space + 1));
      break;
    }
    case Takes::kVolume:
      if (words.size() > 1) {
        command.volume = CommandVolumeFrom(words[1]);
      }
      break;
  }
  return command;
}

float VolumeFractionOf(int command_volume) {
  return static_cast<float>(std::clamp(command_volume, 0, kLoudestCommandVolume)) /
         static_cast<float>(kLoudestCommandVolume);
}

int CommandVolumeOf(float fraction) {
  return static_cast<int>(
      std::lround(std::clamp(fraction, 0.0F, 1.0F) * static_cast<float>(kLoudestCommandVolume)));
}

std::string_view PlayerCommandWord(PlayerCommandKind kind) {
  const auto found =
      std::ranges::find_if(kWords, [kind](const Word& known) { return known.kind == kind; });
  return found == kWords.end() ? std::string_view() : found->word;
}

}  // namespace moonclient
