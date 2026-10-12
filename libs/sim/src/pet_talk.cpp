// Voir l'en-tête.

#include "ro/sim/pet_talk.h"

#include <array>
#include <cstddef>
#include <cstdio>

#include "ro/core/i18n.h"

namespace ro::sim {
namespace {

// La base des chiffres de l'entier, et le poids de chacun.
constexpr std::int32_t kDigitBase = 10;
constexpr std::int32_t kHungerWeight = 10;
constexpr std::int32_t kOccasionWeight = 100;
constexpr std::int32_t kClassWeight = 1000;
// Le reste d'une réplique, et celui d'une émoticône.
constexpr std::int32_t kLineRest = 0;
constexpr std::int32_t kEmotionRest = 2;

// Les émoticônes du jeu de base que le familier montre, par leur numéro.
constexpr std::int32_t kSurprise = 0;
constexpr std::int32_t kDelight = 2;
constexpr std::int32_t kThrob = 3;
constexpr std::int32_t kSweat = 4;
constexpr std::int32_t kAha = 5;
constexpr std::int32_t kFret = 6;
constexpr std::int32_t kThink = 9;
constexpr std::int32_t kBigThrob = 14;
constexpr std::int32_t kThanks = 15;
constexpr std::int32_t kSmile = 18;
constexpr std::int32_t kProfuselySweat = 19;
constexpr std::int32_t kScratch = 20;
constexpr std::int32_t kBest = 21;
constexpr std::int32_t kStareAbout = 22;
constexpr std::int32_t kHuk = 23;
constexpr std::int32_t kHelp = 26;
constexpr std::int32_t kCry = 28;
constexpr std::int32_t kKik = 29;
constexpr std::int32_t kChup = 30;
constexpr std::int32_t kHng = 32;
constexpr std::int32_t kOk = 33;
// Pas d'émoticône.
constexpr std::int32_t kNone = -1;

constexpr std::size_t kRanks = 5;
// Les occasions qui ont une émoticône : du repas au niveau pris.
constexpr std::size_t kEmotedOccasions = 7;

// L'ÉMOTICÔNE, par rang de faim, puis par rang d'intimité, puis par occasion
// (repas, chasse, danger, mort, repos, spectacle spécial, niveau pris).
//
// ⚠ LE NIVEAU PRIS D'UN FAMILIER DISTANT (intimité au rang zéro) est une case
// que le client ne remplit jamais : elle garde le zéro de sa mémoire, qui est
// la surprise.
constexpr std::array<std::array<std::array<std::int32_t, kEmotedOccasions>, kRanks>, kRanks>
    kEmotions = {{
        {{
            {kHng, kHng, kKik, kKik, kFret, kHuk, kSurprise},
            {kHng, kHng, kKik, kKik, kFret, kFret, kSweat},
            {kScratch, kHng, kNone, kHng, kSweat, kScratch, kThink},
            {kOk, kNone, kNone, kThink, kScratch, kThink, kAha},
            {kSmile, kNone, kNone, kScratch, kOk, kDelight, kAha},
        }},
        {{
            {kScratch, kHng, kKik, kKik, kHng, kHng, kSurprise},
            {kScratch, kHng, kNone, kHng, kKik, kHng, kHng},
            {kOk, kNone, kNone, kThink, kScratch, kThink, kAha},
            {kSmile, kNone, kNone, kScratch, kOk, kDelight, kAha},
            {kThanks, kNone, kNone, kProfuselySweat, kThrob, kDelight, kBest},
        }},
        {{
            {kHng, kHng, kNone, kHng, kKik, kSweat, kSurprise},
            {kScratch, kNone, kNone, kThink, kThink, kThink, kScratch},
            {kOk, kNone, kNone, kScratch, kOk, kDelight, kAha},
            {kSmile, kNone, kNone, kProfuselySweat, kSmile, kDelight, kBest},
            {kThanks, kBest, kNone, kHuk, kBigThrob, kOk, kBest},
        }},
        {{
            {kHng, kNone, kNone, kThink, kThink, kThink, kSurprise},
            {kHng, kNone, kNone, kScratch, kScratch, kScratch, kThink},
            {kScratch, kNone, kNone, kProfuselySweat, kBest, kDelight, kAha},
            {kOk, kBest, kProfuselySweat, kHuk, kThrob, kOk, kBest},
            {kSmile, kBest, kHelp, kCry, kChup, kStareAbout, kSmile},
        }},
        {{
            {kHuk, kNone, kNone, kScratch, kScratch, kScratch, kSurprise},
            {kHng, kNone, kNone, kProfuselySweat, kScratch, kSurprise, kSurprise},
            {kHng, kBest, kProfuselySweat, kHuk, kSmile, kOk, kBest},
            {kScratch, kBest, kHelp, kCry, kThrob, kSmile, kSmile},
            {kScratch, kBest, kHelp, kCry, kChup, kSmile, kChup},
        }},
    }};

// Les spectacles que le serveur tire, et le spécial.
constexpr std::int32_t kPerformanceFirst = 1;
constexpr std::int32_t kPerformanceSecond = 2;
constexpr std::int32_t kPerformanceThird = 3;
constexpr std::int32_t kPerformanceSpecial = 4;
// L'entier qui part pour un spectacle au numéro inconnu.
constexpr std::int32_t kUnknownPerformanceCode = 0;

constexpr std::int64_t kWholePercent = 100;

[[nodiscard]] bool InRank(int rank) noexcept {
  return rank >= 0 && static_cast<std::size_t>(rank) < kRanks;
}

// La réplique, puis l'émoticône, chacune si elle part.
[[nodiscard]] std::vector<std::int32_t> Spoken(const Pet& pet, PetTalkOccasion occasion,
                                               int hunger) {
  const int hunger_rank = PetHungerRank(hunger);
  std::vector<std::int32_t> codes;
  const std::int32_t line = PetLineCode(pet.pet_class(), occasion, hunger_rank);
  if (PetActLeaves(line, pet.intimacy())) {
    codes.push_back(line);
  }
  const std::optional<std::int32_t> emotion =
      PetEmotionCode(hunger_rank, PetIntimacyRank(pet.intimacy()), occasion);
  if (emotion.has_value() && PetActLeaves(*emotion, pet.intimacy())) {
    codes.push_back(*emotion);
  }
  return codes;
}

}  // namespace

std::int32_t PetLineCode(std::uint32_t pet_class, PetTalkOccasion occasion,
                         int hunger_rank) noexcept {
  // Le client ne fait que l'arithmétique : une classe trop grande déborde, et
  // le serveur relaie l'entier tel quel.
  const auto wide = static_cast<std::int64_t>(pet_class) * kClassWeight +
                    static_cast<std::int64_t>(occasion) * kOccasionWeight +
                    static_cast<std::int64_t>(hunger_rank) * kHungerWeight + kLineRest;
  return static_cast<std::int32_t>(wide);
}

std::optional<std::int32_t> PetEmotionCode(int hunger_rank, int intimacy_rank,
                                           PetTalkOccasion occasion) noexcept {
  const auto column = static_cast<std::size_t>(occasion);
  if (!InRank(hunger_rank) || !InRank(intimacy_rank) || column >= kEmotedOccasions) {
    return std::nullopt;
  }
  const std::int32_t emotion = kEmotions.at(static_cast<std::size_t>(hunger_rank))
                                   .at(static_cast<std::size_t>(intimacy_rank))
                                   .at(column);
  if (emotion == kNone) {
    return std::nullopt;
  }
  return emotion * kDigitBase + kEmotionRest;
}

bool PetActLeaves(std::int32_t code, int intimacy) noexcept {
  if (code % kDigitBase >= kEmotionRest) {
    return true;
  }
  return intimacy >= kPetTalkIntimacyAtLeast;
}

PetActHeard ReadPetActCode(std::int32_t code) noexcept {
  if (code % kDigitBase == kEmotionRest) {
    return PetEmotionHeard{.emotion = code / kDigitBase};
  }
  const bool greeting = code % kDigitBase == kPetConnectRest;
  return PetLineHeard{
      .pet_class = static_cast<std::uint32_t>(code / kClassWeight),
      .hunger_digit = static_cast<int>((code / kHungerWeight) % kDigitBase),
      .occasion_digit = greeting ? kPetConnectOccasionDigit
                                 : static_cast<int>((code / kOccasionWeight) % kDigitBase)};
}

std::string PetLineShown(const std::string& speaker_name, std::string_view line) {
  const int needed = std::snprintf(nullptr, 0, core::Tr("[Pet] %s :"), speaker_name.c_str());
  if (needed < 0) {
    return {};
  }
  std::string shown(static_cast<std::size_t>(needed), '\0');
  std::snprintf(shown.data(), shown.size() + 1, core::Tr("[Pet] %s :"), speaker_name.c_str());
  shown += ' ';
  shown += line;
  return shown;
}

std::optional<PetTalkOccasion> PerformanceOccasion(std::int32_t performance) noexcept {
  switch (performance) {
    case kPerformanceFirst:
      return PetTalkOccasion::kPerformance1;
    case kPerformanceSecond:
      return PetTalkOccasion::kPerformance2;
    case kPerformanceThird:
      return PetTalkOccasion::kPerformance3;
    case kPerformanceSpecial:
      return PetTalkOccasion::kSpecialPerformance;
    default:
      return std::nullopt;
  }
}

std::vector<std::int32_t> PetChatter::Fed(const Pet& pet) const {
  return Spoken(pet, PetTalkOccasion::kFeeding, pet.previous_hunger());
}

std::vector<std::int32_t> PetChatter::OwnerDied(const Pet& pet) const {
  return Spoken(pet, PetTalkOccasion::kDead, pet.hunger());
}

std::vector<std::int32_t> PetChatter::OwnerLevelledUp(const Pet& pet) const {
  return Spoken(pet, PetTalkOccasion::kLevelUp, pet.hunger());
}

std::vector<std::int32_t> PetChatter::Performed(const Pet& pet, std::int32_t performance) const {
  // Zéro n'est pas un spectacle : rien ne se joue.
  if (performance == 0) {
    return {};
  }
  const std::optional<PetTalkOccasion> occasion = PerformanceOccasion(performance);
  const std::int32_t code =
      occasion.has_value() ? PetLineCode(pet.pet_class(), *occasion, PetHungerRank(pet.hunger()))
                           : kUnknownPerformanceCode;
  if (!PetActLeaves(code, pet.intimacy())) {
    return {};
  }
  return {code};
}

bool PetChatter::QuietFor(std::uint32_t quiet_ms, std::uint32_t now_ms) {
  if (last_spoken_ms_.has_value() && now_ms - *last_spoken_ms_ < quiet_ms) {
    return false;
  }
  last_spoken_ms_ = now_ms;
  return true;
}

std::vector<std::int32_t> PetChatter::OwnerEngaged(const Pet& pet, std::uint32_t now_ms,
                                                   int roll_percent) {
  // Le tirage d'abord : un tirage perdu ne remet pas le silence à zéro.
  if (roll_percent >= kPetHuntingChancePercent || !QuietFor(kPetHuntingQuietMs, now_ms)) {
    return {};
  }
  return Spoken(pet, PetTalkOccasion::kHunting, pet.hunger());
}

std::vector<std::int32_t> PetChatter::OwnerHurt(const Pet& pet, std::int64_t health,
                                                std::int64_t max_health, std::uint32_t now_ms,
                                                int roll_percent) {
  if (max_health <= 0 || kWholePercent * health / max_health >= kPetDangerHealthBelowPercent) {
    return {};
  }
  if (roll_percent >= kPetDangerChancePercent || !QuietFor(kPetDangerQuietMs, now_ms)) {
    return {};
  }
  return Spoken(pet, PetTalkOccasion::kDanger, pet.hunger());
}

std::vector<std::int32_t> PetChatter::ObserveRest(const Pet& pet, bool walking,
                                                  std::uint32_t now_ms, int roll_percent) {
  // L'ARRÊT se date : c'est de là que le repos se compte, une fois.
  if (was_walking_ && !walking) {
    resting_since_ms_ = now_ms;
  }
  was_walking_ = walking;
  if (walking || !resting_since_ms_.has_value() ||
      now_ms - *resting_since_ms_ <= kPetStandAfterMs) {
    return {};
  }
  resting_since_ms_.reset();
  if (roll_percent >= kPetStandChancePercent) {
    return {};
  }
  return Spoken(pet, PetTalkOccasion::kStand, pet.hunger());
}

}  // namespace ro::sim
