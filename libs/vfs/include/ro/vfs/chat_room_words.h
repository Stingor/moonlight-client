#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace ro::vfs {

struct ItemWords;

// LES MOTS DES SALONS DE DISCUSSION : des textes de l'interface, posés par
// `FillChatRoomWords` (ADR 0090). Le montant manquant et le salon pendant le
// vending sont des mots des commerces : voir `trade_words.h`.
enum class ChatRoomWord : std::uint8_t {
  kEnterTitle,
  kEnterPassword,
  kPasswordTooShort,  // le mot de passe de moins de quatre caractères
  kRoomMade,
  kTooManyRooms,
  kSameTitle,
  kRoomFull,
  kKickedOut,
  kPrivatePrefix,  // devant le titre
  kPublicPrefix,
  kMakingRoom,  // titre du formulaire
  kRoomSetup,
  kKick,
  kGiveOwner,
  kPeopleUnit,
  kMemberEntered,  // un %s
  kMemberLeft,     // un %s
  kMemberKicked,   // un %s
  kEnglishOnly,
  kPrivate,
  kPublic,
  kLevelTooLow,
  kLevelTooHigh,
  kWrongJob,
  kTitleLabel,
  kLimitLabel,
  kPasswordLabel,
  kCount,
};

inline constexpr std::size_t kChatRoomWordCount = static_cast<std::size_t>(ChatRoomWord::kCount);

// Les textes posés, rangés par `ChatRoomWord`.
struct ChatRoomWords {
  std::array<std::string, kChatRoomWordCount> texts;
};

// Le texte d'un mot, sans conversion. ⚠ UN GABARIT SE LIT PAR
// `ChatRoomTextWith` : le montrer tel quel écrirait sa conversion à l'écran.
[[nodiscard]] std::string_view ChatRoomText(const ItemWords& words, ChatRoomWord word);

// LE GABARIT IMPRIMÉ AVEC UN NOM. 🔴 IL VIENT D'UN CATALOGUE DE LANGUE : sa
// seule conversion doit être un « %s », sans quoi le texte est vide.
[[nodiscard]] std::string ChatRoomTextWith(const ItemWords& words, ChatRoomWord word,
                                           std::string_view name);

// Pose ces mots dans la langue courante.
void FillChatRoomWords(ChatRoomWords& words);

}  // namespace ro::vfs
