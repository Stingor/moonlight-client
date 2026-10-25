#include "ro/sim/survivors_week.h"

#include <cstdint>

namespace ro::sim {
namespace {

// LE CALENDRIER GRÉGORIEN, compté depuis sa définition : 365 jours par an, un
// de plus les années bissextiles (divisibles par 4, sauf les siècles que 400
// ne divise pas). Quatre cents ans font donc toujours le même nombre de jours :
// un jour se ramène près de son année sans compter les siècles un à un.
constexpr std::int64_t kEpochYear = 1970;
constexpr std::int64_t kYearsPerCycle = 400;
constexpr std::int64_t kLeapYearsPerCycle = 97;
constexpr std::int64_t kDaysPerYear = 365;
constexpr std::int64_t kDaysPerLeapYear = kDaysPerYear + 1;
constexpr std::int64_t kDaysPerCycle = kYearsPerCycle * kDaysPerYear + kLeapYearsPerCycle;
constexpr std::int64_t kLeapEveryYears = 4;
constexpr std::int64_t kNoLeapEveryYears = 100;
constexpr std::int64_t kMonthsPerYear = 12;

constexpr std::int64_t kDaysPerWeek = 7;
// Le 1970-01-01 est un jeudi : jour 4 de la semaine ISO (lundi = 1).
constexpr std::int64_t kEpochIsoWeekday = 4;
constexpr std::int64_t kThursdayIsoWeekday = 4;

constexpr std::int64_t kWeeksPerSeedYear = 100;
// Le sel de la graine : « SURVWEEK » en ASCII.
constexpr std::uint64_t kChallengeSeedSalt = 0x5355'5256'5745'454BULL;
// Les constantes publiées de SplitMix64 : l'incrément (partie fractionnaire du
// nombre d'or) et les deux multiplicateurs du finaliseur.
constexpr std::uint64_t kSplitMixGamma = 0x9E37'79B9'7F4A'7C15ULL;
constexpr std::uint64_t kSplitMixFirstMultiplier = 0xBF58'476D'1CE4'E5B9ULL;
constexpr std::uint64_t kSplitMixSecondMultiplier = 0x94D0'49BB'1331'11EBULL;
constexpr unsigned kSplitMixFirstShift = 30;
constexpr unsigned kSplitMixSecondShift = 27;
constexpr unsigned kSplitMixLastShift = 31;

[[nodiscard]] constexpr std::int64_t FloorDivide(std::int64_t value, std::int64_t by) noexcept {
  const std::int64_t quotient = value / by;
  return (value % by != 0 && (value < 0) != (by < 0)) ? quotient - 1 : quotient;
}

[[nodiscard]] constexpr bool IsLeapYear(std::int64_t year) noexcept {
  return year % kLeapEveryYears == 0 &&
         (year % kNoLeapEveryYears != 0 || year % kYearsPerCycle == 0);
}

[[nodiscard]] constexpr std::int64_t DaysInMonth(std::int64_t year, std::int64_t month) noexcept {
  constexpr std::int64_t kFebruary = 2;
  constexpr std::int64_t kLongMonth = 31;
  constexpr std::int64_t kShortMonth = 30;
  constexpr std::int64_t kLeapFebruary = 29;
  constexpr std::int64_t kFebruaryDays = 28;
  if (month == kFebruary) return IsLeapYear(year) ? kLeapFebruary : kFebruaryDays;
  // Avril, juin, septembre et novembre ont trente jours.
  constexpr std::int64_t kApril = 4;
  constexpr std::int64_t kJune = 6;
  constexpr std::int64_t kSeptember = 9;
  constexpr std::int64_t kNovember = 11;
  if (month == kApril || month == kJune || month == kSeptember || month == kNovember) {
    return kShortMonth;
  }
  return kLongMonth;
}

// Un compte cumulé des années bissextiles : `LeapYearsThrough(y) −
// LeapYearsThrough(y − 1)` vaut 1 exactement quand `y` est bissextile, y
// compris avant l'an 1 (l'an 0 l'est).
[[nodiscard]] constexpr std::int64_t LeapYearsThrough(std::int64_t year) noexcept {
  return FloorDivide(year, kLeapEveryYears) - FloorDivide(year, kNoLeapEveryYears) +
         FloorDivide(year, kYearsPerCycle);
}

// Les jours du 1970-01-01 au 1er janvier de `year`, négatifs avant 1970.
[[nodiscard]] constexpr std::int64_t DaysBeforeYear(std::int64_t year) noexcept {
  return (year - kEpochYear) * kDaysPerYear + LeapYearsThrough(year - 1) -
         LeapYearsThrough(kEpochYear - 1);
}

}  // namespace

CivilDate CivilDateOfDay(std::int64_t days_since_epoch) noexcept {
  const std::int64_t cycles = FloorDivide(days_since_epoch, kDaysPerCycle);
  const std::int64_t in_cycle = days_since_epoch - cycles * kDaysPerCycle;
  // Aucune année n'a plus de 366 jours : l'estimation tombe sur l'année ou
  // juste avant, et ne monte que d'une ou deux années.
  std::int64_t year = kEpochYear + cycles * kYearsPerCycle + in_cycle / kDaysPerLeapYear;
  while (DaysBeforeYear(year + 1) <= days_since_epoch) ++year;
  std::int64_t day_of_year = days_since_epoch - DaysBeforeYear(year);
  std::int64_t month = 1;
  while (month < kMonthsPerYear && day_of_year >= DaysInMonth(year, month)) {
    day_of_year -= DaysInMonth(year, month);
    ++month;
  }
  return CivilDate{.year = static_cast<std::int32_t>(year),
                   .month = static_cast<std::uint8_t>(month),
                   .day = static_cast<std::uint8_t>(day_of_year + 1)};
}

std::int64_t DayOfCivilDate(const CivilDate& date) noexcept {
  std::int64_t days = DaysBeforeYear(date.year);
  for (std::int64_t month = 1; month < date.month && month <= kMonthsPerYear; ++month) {
    days += DaysInMonth(date.year, month);
  }
  return days + date.day - 1;
}

bool CivilDateExists(const CivilDate& date) noexcept {
  if (date.month < 1 || date.month > kMonthsPerYear || date.day < 1) return false;
  return date.day <= DaysInMonth(date.year, date.month);
}

UtcInstant UtcInstantOf(std::int64_t utc_s) noexcept {
  const std::int64_t day = FloorDivide(utc_s, kSecondsPerDay);
  const std::int64_t of_day = utc_s - day * kSecondsPerDay;
  return UtcInstant{
      .date = CivilDateOfDay(day),
      .hour = static_cast<std::uint8_t>(of_day / kSecondsPerHour),
      .minute = static_cast<std::uint8_t>(of_day % kSecondsPerHour / kSecondsPerMinute),
      .second = static_cast<std::uint8_t>(of_day % kSecondsPerMinute)};
}

IsoWeek IsoWeekOfDay(std::int64_t days_since_epoch) noexcept {
  // Le jeudi de la semaine décide de son année : la semaine 1 est celle du
  // premier jeudi.
  const std::int64_t from_monday = days_since_epoch + kEpochIsoWeekday - 1;
  const std::int64_t weekday =
      from_monday - FloorDivide(from_monday, kDaysPerWeek) * kDaysPerWeek + 1;
  const std::int64_t thursday = days_since_epoch - weekday + kThursdayIsoWeekday;
  const std::int32_t iso_year = CivilDateOfDay(thursday).year;
  const std::int64_t first_day = DayOfCivilDate(CivilDate{.year = iso_year, .month = 1, .day = 1});
  return IsoWeek{.year = iso_year,
                 .week = static_cast<std::uint8_t>((thursday - first_day) / kDaysPerWeek + 1)};
}

std::int64_t MondayOfIsoWeek(const IsoWeek& week) noexcept {
  // Le 4 janvier est toujours dans la semaine 1 : son lundi ouvre l'année ISO.
  constexpr std::uint8_t kAlwaysInFirstWeek = 4;
  const std::int64_t anchor =
      DayOfCivilDate(CivilDate{.year = week.year, .month = 1, .day = kAlwaysInFirstWeek});
  const std::int64_t from_monday = anchor + kEpochIsoWeekday - 1;
  const std::int64_t first_monday =
      anchor - (from_monday - FloorDivide(from_monday, kDaysPerWeek) * kDaysPerWeek);
  return first_monday + (std::int64_t{week.week} - 1) * kDaysPerWeek;
}

std::int64_t IsoWeekOrdinal(const IsoWeek& week) noexcept {
  constexpr std::int64_t kFirstEpochMonday = 1 - kEpochIsoWeekday;
  return FloorDivide(MondayOfIsoWeek(week) - kFirstEpochMonday, kDaysPerWeek);
}

std::uint64_t SeedOfWeek(std::int32_t iso_year, std::uint8_t iso_week) noexcept {
  // Le mélange se fait en arithmétique non signée, modulo 2^64, comme le
  // serveur le refera.
  const std::uint64_t week_number = static_cast<std::uint64_t>(
      std::int64_t{iso_year} * kWeeksPerSeedYear + std::int64_t{iso_week});
  std::uint64_t mixed = week_number + kChallengeSeedSalt + kSplitMixGamma;
  mixed = (mixed ^ (mixed >> kSplitMixFirstShift)) * kSplitMixFirstMultiplier;
  mixed = (mixed ^ (mixed >> kSplitMixSecondShift)) * kSplitMixSecondMultiplier;
  return mixed ^ (mixed >> kSplitMixLastShift);
}

}  // namespace ro::sim
