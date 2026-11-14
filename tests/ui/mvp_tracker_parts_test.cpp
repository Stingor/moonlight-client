// LE CARNET DE CHASSE MVP, les aides pures : durées, heure saisie, textes,
// filtre.
//
// Les heures attendues se calculent ICI, par l'heure locale de la machine :
// l'essai ne dépend pas du fuseau de celle qui le lance.

#include <array>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <limits>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/local_time.h"
#include "ro/ui/mvp_tracker_window.h"

using ro::ui::FormatMvpDuration;
using ro::ui::MvpCollapsedTitle;
using ro::ui::MvpGroupHeader;
using ro::ui::MvpKillTimeEcho;
using ro::ui::MvpLineText;
using ro::ui::MvpMatchesFilter;
using ro::ui::MvpResultText;
using ro::ui::MvpReturnText;
using ro::ui::MvpReturnTone;
using ro::ui::MvpReturnToneOf;
using ro::ui::MvpSlotView;
using ro::ui::MvpSourceLabel;
using ro::ui::MvpWireCut;
using ro::ui::MvpWireExcess;
using ro::ui::MvpWireExcessOf;
using ro::ui::ParseMvpKillTime;

namespace {

constexpr std::int64_t kHour = 3600;
constexpr std::int64_t kMinute = 60;
constexpr std::int64_t kDay = 24 * kHour;
// Le 5 octobre 2026, loin de tout changement d'heure.
constexpr int kYearSince1900 = 126;
constexpr int kOctober = 9;
constexpr int kFifth = 5;

[[nodiscard]] std::tm LocalOf(std::int64_t instant) {
  const auto seconds = static_cast<std::time_t>(instant);
  std::tm local{};
  REQUIRE(ro::core::ToLocalTime(seconds, local));
  return local;
}

// L'instant local de ce jour-là, à cette heure.
[[nodiscard]] std::int64_t LocalInstant(int hour, int minute) {
  std::tm local{};
  local.tm_year = kYearSince1900;
  local.tm_mon = kOctober;
  local.tm_mday = kFifth;
  local.tm_hour = hour;
  local.tm_min = minute;
  local.tm_isdst = -1;
  return static_cast<std::int64_t>(std::mktime(&local));
}

// L'heure saisie telle que la règle la lit, le même jour que `now` : la veille
// quand elle est demandée, ou quand elle tomberait dans le futur.
[[nodiscard]] std::int64_t Expected(std::int64_t now, int hour, int minute, bool yesterday) {
  std::tm local = LocalOf(now);
  local.tm_hour = hour;
  local.tm_min = minute;
  local.tm_sec = 0;
  auto when = static_cast<std::int64_t>(std::mktime(&local));
  if (yesterday || when > now) {
    when -= kDay;
  }
  return when;
}

[[nodiscard]] MvpSlotView Slot(std::uint16_t slot_id, std::string_view label,
                               std::string_view map) {
  return MvpSlotView{.slot_id = slot_id, .mob_id = slot_id, .map = map, .label = label};
}

[[nodiscard]] MvpSlotView Window(MvpSlotView slot, std::int64_t from, std::int64_t to,
                                 bool exact = false) {
  slot.window_known = true;
  slot.window_from_s = from;
  slot.window_to_s = to;
  slot.exact = exact;
  return slot;
}

}  // namespace

TEST_CASE("Carnet MVP : une durée s'écrit court, sans signe", "[ui][mvp]") {
  CHECK(FormatMvpDuration(0) == "0 s");
  CHECK(FormatMvpDuration(59) == "59 s");
  CHECK(FormatMvpDuration(60) == "1 min");
  CHECK(FormatMvpDuration(119) == "1 min");
  CHECK(FormatMvpDuration(3599) == "59 min");
  CHECK(FormatMvpDuration(3600) == "1h00");
  CHECK(FormatMvpDuration(8040) == "2h14");
  CHECK(FormatMvpDuration(86400) == "24h00");
  CHECK(FormatMvpDuration(-90) == "1 min");
  // La plus petite valeur n'a pas d'opposé : elle s'écrit comme la plus grande.
  CHECK(FormatMvpDuration(std::numeric_limits<std::int64_t>::min()) ==
        FormatMvpDuration(std::numeric_limits<std::int64_t>::max()));
}

TEST_CASE("Carnet MVP : l'heure saisie ne garde que les chiffres", "[ui][mvp]") {
  const std::int64_t now = LocalInstant(23, 55);
  for (const std::string_view typed : {"1430", "14h30", "14:30", "14 30", "14300", "1:4:3:0"}) {
    INFO(typed);
    CHECK(ParseMvpKillTime(typed, now) == Expected(now, 14, 30, false));
  }
  CHECK(ParseMvpKillTime("930", now) == Expected(now, 9, 30, false));
  CHECK(ParseMvpKillTime("9h30", now) == Expected(now, 9, 30, false));
  // Trois chiffres : une heure d'un chiffre, puis les minutes.
  CHECK(ParseMvpKillTime("14h3", now) == Expected(now, 1, 43, false));
  // Au-delà du quatrième chiffre, rien ne compte.
  CHECK(ParseMvpKillTime("123456", now) == Expected(now, 12, 34, false));
  CHECK(ParseMvpKillTime("1:5:30", now) == Expected(now, 15, 30, false));
}

TEST_CASE("Carnet MVP : un « - » avant le quatrième chiffre désigne la veille", "[ui][mvp]") {
  const std::int64_t now = LocalInstant(23, 55);
  CHECK(ParseMvpKillTime("2350", now) == Expected(now, 23, 50, false));
  CHECK(ParseMvpKillTime("-2350", now) == Expected(now, 23, 50, true));
  CHECK(ParseMvpKillTime("1-430", now) == Expected(now, 14, 30, true));
  // Lu APRÈS le quatrième chiffre, il ne compte plus.
  CHECK(ParseMvpKillTime("2350-", now) == Expected(now, 23, 50, false));
  CHECK(ParseMvpKillTime("2350", now) - ParseMvpKillTime("-2350", now) == kDay);
}

TEST_CASE("Carnet MVP : une heure à venir désigne la veille", "[ui][mvp]") {
  const std::int64_t now = LocalInstant(10, 0);
  const std::int64_t when = ParseMvpKillTime("1430", now);
  CHECK(when == Expected(now, 14, 30, true));
  CHECK(when < now);
  CHECK(now - when == kDay - 4 * kHour - 30 * kMinute);
}

TEST_CASE("Carnet MVP : une heure illisible vaut zéro", "[ui][mvp]") {
  const std::int64_t now = LocalInstant(23, 55);
  for (const std::string_view typed : {"9:3", "24:00", "12:60", "ab", "-", "", "1h"}) {
    INFO(typed);
    CHECK(ParseMvpKillTime(typed, now) == 0);
  }
}

TEST_CASE("Carnet MVP : la ligne détachée dit le nom, la carte et l'échéance", "[ui][mvp]") {
  constexpr std::int64_t kNow = 1'000'000;
  const MvpSlotView bare = Slot(1, "Baphomet", "prt_maze03");
  CHECK(MvpLineText(bare, kNow) == "Baphomet (prt_maze03) — jamais vu");
  CHECK(MvpLineText(Window(bare, kNow - 10, kNow - 10, true), kNow) ==
        "Baphomet (prt_maze03) — de retour");
  CHECK(MvpLineText(Window(bare, kNow + 8040, kNow + 8040, true), kNow) ==
        "Baphomet (prt_maze03) — dans 2h14 (exact)");
  CHECK(MvpLineText(Window(bare, kNow - 600, kNow - 1), kNow) ==
        "Baphomet (prt_maze03) — fenêtre passée");
  CHECK(MvpLineText(Window(bare, kNow - 600, kNow + 600), kNow) ==
        "Baphomet (prt_maze03) — possible (encore 10 min)");
  CHECK(MvpLineText(Window(bare, kNow + kHour, kNow + kHour + 600), kNow) ==
        "Baphomet (prt_maze03) — 1h00 – 1h10");
  // La carte nommée par l'installation remplace le nom interne.
  MvpSlotView named = bare;
  named.map_label = "Labyrinthe de Prontera, 3F";
  CHECK(MvpLineText(named, kNow) == "Baphomet (Labyrinthe de Prontera, 3F) — jamais vu");
}

TEST_CASE("Carnet MVP : le retour s'écrit pareil partout, et sa teinte le suit", "[ui][mvp]") {
  constexpr std::int64_t kNow = 1'000'000;
  const MvpSlotView bare = Slot(1, "Baphomet", "prt_maze03");
  CHECK(MvpReturnText(bare, kNow) == "jamais vu");
  CHECK(MvpReturnToneOf(bare, kNow) == MvpReturnTone::kDim);
  // Un instant exact passé est « de retour », à venir « dans … (exact) ».
  CHECK(MvpReturnText(Window(bare, kNow, kNow, true), kNow) == "de retour");
  CHECK(MvpReturnText(Window(bare, kNow + 60, kNow + 60, true), kNow) == "dans 1 min (exact)");
  CHECK(MvpReturnToneOf(Window(bare, kNow, kNow, true), kNow) == MvpReturnTone::kPlain);
  // Une fenêtre ouverte dit le temps qui lui reste, en vert ; à sa fin pile,
  // elle est passée.
  const MvpSlotView open = Window(bare, kNow - 60, kNow + kHour);
  CHECK(MvpReturnText(open, kNow) == "possible (encore 1h00)");
  CHECK(MvpReturnToneOf(open, kNow) == MvpReturnTone::kPossible);
  const MvpSlotView closed = Window(bare, kNow - 60, kNow);
  CHECK(MvpReturnText(closed, kNow) == "fenêtre passée");
  CHECK(MvpReturnToneOf(closed, kNow) == MvpReturnTone::kPlain);
  CHECK(MvpReturnToneOf(Window(bare, kNow + 60, kNow + 120), kNow) == MvpReturnTone::kPlain);
}

TEST_CASE("Carnet MVP : replié, le titre dit la prochaine échéance", "[ui][mvp]") {
  constexpr std::int64_t kNow = 1'000'000;
  std::array<MvpSlotView, 4> slots = {
      Slot(1, "Atroce", "ra_fild02"), Slot(2, "Baphomet", "prt_maze03"),
      Slot(3, "Drake", "treasure02"), Slot(4, "Eddga", "pay_fild11")};
  // Aucun créneau observé : le titre complet.
  CHECK(MvpCollapsedTitle(slots, kNow) == "Carnet de chasse MVP");

  // Le plus proche gagne.
  slots[1] = Window(slots[1], kNow + 8040, kNow + 9000);
  slots[2] = Window(slots[2], kNow + 600, kNow + 1200);
  CHECK(MvpCollapsedTitle(slots, kNow) == "MVP : Drake dans 10 min");

  // À égalité, l'ordre du catalogue départage.
  slots[3] = Window(slots[3], kNow + 600, kNow + 1200);
  CHECK(MvpCollapsedTitle(slots, kNow) == "MVP : Drake dans 10 min");

  // Un créneau déjà dans sa fenêtre passe devant.
  slots[0] = Window(slots[0], kNow - 60, kNow + 60);
  CHECK(MvpCollapsedTitle(slots, kNow) == "MVP : Atroce possible");
}

TEST_CASE("Carnet MVP : le titre replié écarte ce qui est passé depuis trop longtemps",
          "[ui][mvp]") {
  constexpr std::int64_t kNow = 1'000'000;
  std::array<MvpSlotView, 3> slots = {Slot(1, "Atroce", "ra_fild02"),
                                      Slot(2, "Baphomet", "prt_maze03"),
                                      Slot(3, "Drake", "treasure02")};
  slots[2] = Window(slots[2], kNow + kHour, kNow + 2 * kHour);
  // Une fenêtre close depuis plus d'une demi-heure s'efface ; à la demi-heure
  // pile, elle compte encore.
  slots[0] = Window(slots[0], kNow - 2 * kHour, kNow - 1801);
  CHECK(MvpCollapsedTitle(slots, kNow) == "MVP : Drake dans 1h00");
  // À la demi-heure pile, elle compte encore — mais seule : une échéance à
  // venir passe devant une fenêtre close.
  const MvpSlotView drake = slots[2];
  slots[2] = Slot(3, "Drake", "treasure02");
  slots[0] = Window(slots[0], kNow - 2 * kHour, kNow - 1800);
  // Close, elle se dit « passée », comme dans la table et la ligne — et non
  // « possible ».
  CHECK(MvpCollapsedTitle(slots, kNow) == "MVP : Atroce, fenêtre passée");
  slots[0] = Window(slots[0], kNow - 2 * kHour, kNow);
  CHECK(MvpCollapsedTitle(slots, kNow) == "MVP : Atroce, fenêtre passée");
  slots[2] = drake;
  CHECK(MvpCollapsedTitle(slots, kNow) == "MVP : Drake dans 1h00");
  slots[0] = Window(slots[0], kNow - 2 * kHour, kNow + 1);
  CHECK(MvpCollapsedTitle(slots, kNow) == "MVP : Atroce possible");
  // Un instant exact passé depuis plus d'un quart d'heure aussi.
  slots[0] = Slot(1, "Atroce", "ra_fild02");
  slots[1] = Window(slots[1], kNow - 901, kNow - 901, true);
  CHECK(MvpCollapsedTitle(slots, kNow) == "MVP : Drake dans 1h00");
  slots[1] = Window(slots[1], kNow - 900, kNow - 900, true);
  // Un instant exact passé est « de retour », comme dans la table.
  CHECK(MvpCollapsedTitle(slots, kNow) == "MVP : Baphomet de retour");
}

TEST_CASE("Carnet MVP : le titre replié classe par état, puis par borne", "[ui][mvp]") {
  constexpr std::int64_t kNow = 1'000'000;
  // Atroce s'est ouverte la première, mais sa fenêtre est close depuis dix
  // minutes ; Drake est dans la sienne, Eddga vient ensuite.
  std::array<MvpSlotView, 3> slots = {
      Window(Slot(1, "Atroce", "ra_fild02"), kNow - 2 * kHour, kNow - 10 * kMinute),
      Window(Slot(2, "Drake", "treasure02"), kNow - 5 * kMinute, kNow + 5 * kMinute),
      Window(Slot(3, "Eddga", "pay_fild11"), kNow + 20 * kMinute, kNow + 30 * kMinute)};
  CHECK(MvpCollapsedTitle(slots, kNow) == "MVP : Drake possible");
  // Sans fenêtre ouverte, l'échéance à venir passe devant la fenêtre close.
  slots[1] = Slot(2, "Drake", "treasure02");
  CHECK(MvpCollapsedTitle(slots, kNow) == "MVP : Eddga dans 20 min");
  // Un instant exact revenu compte comme une fenêtre ouverte, et la plus
  // ancienne borne départage deux ouvertes.
  slots[1] = Window(slots[1], kNow - 5 * kMinute, kNow - 5 * kMinute, true);
  slots[2] = Window(slots[2], kNow - 30 * kMinute, kNow + 30 * kMinute);
  CHECK(MvpCollapsedTitle(slots, kNow) == "MVP : Eddga possible");
}

TEST_CASE("Carnet MVP : chaque refus du serveur a sa phrase", "[ui][mvp]") {
  REQUIRE(MvpResultText(0) != nullptr);
  CHECK(std::string_view(MvpResultText(0)).empty());
  // 14 à 17 ne viennent que d'un serveur qui sait nos queues.
  constexpr std::uint8_t kLastKnownCode = 17;
  for (std::uint8_t code = 1; code <= kLastKnownCode; ++code) {
    INFO(static_cast<int>(code));
    REQUIRE(MvpResultText(code) != nullptr);
    CHECK_FALSE(std::string_view(MvpResultText(code)).empty());
  }
  CHECK(std::string_view(MvpResultText(7)) == "Le groupe est complet.");
  CHECK(std::string_view(MvpResultText(14)) ==
        "Saisie sans effet : le groupe tient déjà une heure au moins aussi précise.");
  CHECK(std::string_view(MvpResultText(16)) == "Cette personne n'est pas membre de votre groupe.");
  // Un code d'un serveur plus récent : le numéro se montre à la place.
  CHECK(MvpResultText(kLastKnownCode + 1) == nullptr);
}

TEST_CASE("Carnet MVP : la source d'une observation se nomme", "[ui][mvp]") {
  // « saisie », la source ; « saisir » reste le geste du bouton.
  CHECK(std::string_view(MvpSourceLabel(0)) == "saisie");
  CHECK(std::string_view(MvpSourceLabel(1)) == "tombe");
  CHECK(std::string_view(MvpSourceLabel(2)) == "tué");
  CHECK(std::string_view(MvpSourceLabel(3)) == "miroir");
  CHECK(std::string_view(MvpSourceLabel(9)) == "?");
}

TEST_CASE("Carnet MVP : le filtre cherche le nom ou la carte, sans casse", "[ui][mvp]") {
  const MvpSlotView slot = Slot(1, "Golden Thief Bug", "prt_sewb4");
  CHECK(MvpMatchesFilter(slot, ""));
  CHECK(MvpMatchesFilter(slot, "thief"));
  CHECK(MvpMatchesFilter(slot, "GOLDEN"));
  CHECK(MvpMatchesFilter(slot, "SEWB"));
  CHECK_FALSE(MvpMatchesFilter(slot, "bapho"));
  CHECK_FALSE(MvpMatchesFilter(slot, "prt_maze"));
  // Le nom lisible de la carte se cherche aussi, et l'interne reste trouvable.
  MvpSlotView named = slot;
  named.map_label = "Egouts de Prontera";
  CHECK(MvpMatchesFilter(named, "egouts"));
  CHECK(MvpMatchesFilter(named, "sewb"));
}

TEST_CASE("Carnet MVP : un nom se coupe au fil, jamais au milieu d'un caractère", "[ui][mvp]") {
  constexpr std::size_t kGroupMost = ro::ui::kMvpGroupNameMostWireChars;
  const auto repeated = [](std::string_view piece, int times) {
    std::string text;
    for (int count = 0; count < times; ++count) {
      text += piece;
    }
    return text;
  };
  CHECK(MvpWireCut("", kGroupMost) == 0);
  CHECK(MvpWireCut("Bapho", kGroupMost) == 5);
  CHECK(MvpWireCut(repeated("a", 40), kGroupMost) == 31);
  // Un octet du fil par caractère CP1252, quelle que soit sa taille en UTF-8.
  CHECK(MvpWireCut(repeated("é", 40), kGroupMost) == 62);
  CHECK(MvpWireCut(repeated("€", 40), kGroupMost) == 93);
  // Hors CP1252, le caractère part en « ? » : un octet du fil, quatre ici.
  CHECK(MvpWireCut(repeated("😀", 40), kGroupMost) == 124);
  // Le nom d'un personnage : 23.
  CHECK(MvpWireCut(repeated("é", 40), ro::ui::kMvpNameMostWireChars) == 46);
  // Une coupe qui tomberait dans un caractère s'arrête avant lui.
  const std::string mixed = repeated("a", 30) + "€€";
  CHECK(MvpWireCut(mixed, kGroupMost) == 33);
  for (std::size_t most = 0; most <= kGroupMost; ++most) {
    const std::size_t cut = MvpWireCut(mixed, most);
    INFO(most);
    CHECK((cut == mixed.size() || (static_cast<unsigned char>(mixed[cut]) & 0xC0U) != 0x80U));
  }
}

TEST_CASE("Carnet MVP : un champ trop long perd ce qu'on vient d'y écrire", "[ui][mvp]") {
  constexpr std::size_t kMost = 5;
  // Le champ tient : rien à retirer.
  CHECK(MvpWireExcessOf("abcde", 5, kMost).bytes == 0);
  // Écrit en fin : la queue.
  const MvpWireExcess at_end = MvpWireExcessOf("abcdefg", 7, kMost);
  CHECK(at_end.at == 5);
  CHECK(at_end.bytes == 2);
  // 🔴 ÉCRIT AU DÉBUT : ce qui précède le curseur, et la fin du nom reste.
  const MvpWireExcess at_start = MvpWireExcessOf("XYabcde", 2, kMost);
  CHECK(at_start.at == 0);
  CHECK(at_start.bytes == 2);
  // Au milieu, en caractères entiers : « é » fait deux octets UTF-8, un du fil.
  const MvpWireExcess within = MvpWireExcessOf(
      "ab\xC3\xA9\xC3\xA9"
      "cde",
      6, kMost);
  CHECK(within.at == 2);
  CHECK(within.bytes == 4);
  // Le curseur trop tôt pour suffire : la queue.
  const MvpWireExcess too_early = MvpWireExcessOf("Xabcdefg", 1, kMost);
  CHECK(too_early.at == 5);
  CHECK(too_early.bytes == 3);
}

TEST_CASE("Carnet MVP : « 1 membre » au singulier", "[ui][mvp]") {
  CHECK(MvpGroupHeader("Chasseurs", 1) == "Groupe « Chasseurs » — 1 membre###mvp_group");
  CHECK(MvpGroupHeader("Chasseurs", 3) == "Groupe « Chasseurs » — 3 membres###mvp_group");
}

TEST_CASE("Carnet MVP : l'heure saisie se relit « aujourd'hui » ou « hier »", "[ui][mvp]") {
  const std::int64_t now = LocalInstant(12, 0);
  CHECK(MvpKillTimeEcho(ParseMvpKillTime("1000", now), now) == "10:00 aujourd'hui");
  // Une heure à venir désigne la veille, et la relecture le dit.
  CHECK(MvpKillTimeEcho(ParseMvpKillTime("1300", now), now) == "13:00 hier");
  CHECK(MvpKillTimeEcho(ParseMvpKillTime("-0930", now), now) == "09:30 hier");
}
