// Le carnet de chasse MVP : les aides pures — durées, heure saisie, textes,
// filtre, tri et hauteur de rang.

#include "mvp_tracker_parts.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <format>
#include <limits>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/core/text.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

namespace ro::ui {

namespace {

using mvp_tracker::kMvpCollapsedExactFreshS;
using mvp_tracker::kMvpCollapsedWindowFreshS;
using mvp_tracker::kMvpSecondsPerHour;
using mvp_tracker::kMvpSecondsPerMinute;
using mvp_tracker::Printed;
using mvp_tracker::Px;

// Une heure saisie tient en quatre chiffres au plus : « hhmm », ou « hmm ».
constexpr std::size_t kKillTimeMostDigits = 4;
constexpr std::size_t kKillTimeLeastDigits = 3;
constexpr int kDecimalBase = 10;
constexpr int kLastHour = 23;
constexpr int kLastMinute = 59;
constexpr std::int64_t kSecondsPerDay = 24 * kMvpSecondsPerHour;

// Les sources d'une observation, dans l'ordre du fil.
enum class MvpSource : std::uint8_t { kManual, kTomb, kKill, kMirror };

// Les réponses du serveur à une commande de groupe, dans l'ordre du fil.
enum class MvpResult : std::uint8_t {
  kOk,
  kNoAccount,
  kAlreadyMember,
  kNotMember,
  kNotOwner,
  kNoSuchUser,
  kSelf,
  kFull,
  kBadName,
  kNoInvite,
  // La cible appartient à un AUTRE groupe.
  kTargetInGroup,
  kSql,
  // La cible est déjà dans le nôtre, par une autre tête de son compte.
  kTargetSameGroup,
  // Elle existe, mais n'est ni de la guilde ni des amis.
  kNotInvitable,
  // Une saisie sans effet : le groupe tient déjà mieux. Pas un refus.
  kObservationKept,
  kUnknownSlot,
  // La cible d'une exclusion n'est pas du groupe.
  kTargetNotMember,
  // Un nom de groupe au-delà de ce que le serveur garde : refusé, pas coupé.
  kNameTooLong,
};

[[nodiscard]] int DigitValue(char digit) { return digit - '0'; }

// Un octet de suite UTF-8, « 10xxxxxx » : jamais le début d'un caractère.
constexpr unsigned kUtf8ContinuationMask = 0xC0U;
constexpr unsigned kUtf8ContinuationBits = 0x80U;

[[nodiscard]] bool IsUtf8Continuation(char byte) {
  return (static_cast<unsigned char>(byte) & kUtf8ContinuationMask) == kUtf8ContinuationBits;
}

// L'heure LOCALE d'un instant. Faux quand la bibliothèque ne sait pas la
// donner.
[[nodiscard]] bool LocalTimeOf(std::time_t instant, std::tm* into) {
#if defined(_WIN32)
  return localtime_s(into, &instant) == 0;
#else
  return localtime_r(&instant, into) != nullptr;
#endif
}

// Le filtre est-il dans ce texte, sans tenir compte de la casse ASCII ?
[[nodiscard]] bool ContainsFolded(std::string_view haystack, std::string_view needle) {
  const auto folded = [](char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) ==
           std::tolower(static_cast<unsigned char>(b));
  };
  return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(), folded) !=
         haystack.end();
}

// La comparaison à trois voies de deux textes, octet par octet, comme `strcmp`.
[[nodiscard]] int CompareBytes(std::string_view a, std::string_view b) {
  const int said = a.compare(b);
  return said < 0 ? -1 : (said > 0 ? 1 : 0);
}

template <typename T>
[[nodiscard]] int CompareValues(T a, T b) {
  return a < b ? -1 : (a > b ? 1 : 0);
}

// La hauteur d'une étoile de favori : son art, ou à défaut le petit bouton
// texte qui la remplace.
[[nodiscard]] float FavoriteHeight(const SkinImage& image, const SkinArt& art) {
  if (!image.usable()) {
    return SkinSmallButtonHeight(art);
  }
  return Px(image.height > 0.0F ? image.height : mvp_tracker::kMvpFavoriteFallbackPixels);
}

// LE RANG D'UNE ÉCHÉANCE DANS LE TITRE REPLIÉ, le plus petit devant : dans sa
// fenêtre ou revenu à l'instant exact, puis à venir, puis fenêtre close.
enum class CollapsedStanding : std::uint8_t { kOpen, kUpcoming, kPassed };

[[nodiscard]] CollapsedStanding StandingOf(const MvpSlotView& slot, std::int64_t server_now_s) {
  if (slot.window_from_s > server_now_s) {
    return CollapsedStanding::kUpcoming;
  }
  if (!slot.exact && slot.window_to_s <= server_now_s) {
    return CollapsedStanding::kPassed;
  }
  return CollapsedStanding::kOpen;
}

}  // namespace

std::string FormatMvpDuration(std::int64_t seconds) {
  // ⚠ LA PLUS PETITE VALEUR N'A PAS D'OPPOSÉ : elle se lit comme la plus grande.
  if (seconds < 0) {
    seconds = seconds < -std::numeric_limits<std::int64_t>::max()
                  ? std::numeric_limits<std::int64_t>::max()
                  : -seconds;
  }
  if (seconds >= kMvpSecondsPerHour) {
    return std::format("{}h{:02}", seconds / kMvpSecondsPerHour,
                       (seconds % kMvpSecondsPerHour) / kMvpSecondsPerMinute);
  }
  if (seconds >= kMvpSecondsPerMinute) {
    return Printed(core::Tr("%lld min"), static_cast<long long>(seconds / kMvpSecondsPerMinute));
  }
  return Printed(core::Tr("%lld s"), static_cast<long long>(seconds));
}

std::int64_t ParseMvpKillTime(std::string_view text, std::int64_t server_now_s) {
  // « h », « : » et les espaces ne sont que du décor : seuls les chiffres
  // comptent, ce qui accepte 1430, 14h30, 14:30 et « 14 30 » sans les énumérer.
  // Un « - » avant le quatrième chiffre désigne la veille.
  bool yesterday = false;
  std::array<char, kKillTimeMostDigits> digits{};
  std::size_t count = 0;
  for (const char typed : text) {
    if (count >= kKillTimeMostDigits) {
      break;
    }
    if (typed == '-') {
      yesterday = true;
    } else if (typed >= '0' && typed <= '9') {
      digits[count++] = typed;
    }
  }
  if (count != kKillTimeMostDigits && count != kKillTimeLeastDigits) {
    return 0;
  }
  int hour = 0;
  int minute = 0;
  if (count == kKillTimeMostDigits) {
    hour = DigitValue(digits[0]) * kDecimalBase + DigitValue(digits[1]);
    minute = DigitValue(digits[2]) * kDecimalBase + DigitValue(digits[3]);
  } else {
    hour = DigitValue(digits[0]);
    minute = DigitValue(digits[1]) * kDecimalBase + DigitValue(digits[2]);
  }
  if (hour > kLastHour || minute > kLastMinute) {
    return 0;
  }
  std::tm local{};
  if (!LocalTimeOf(static_cast<std::time_t>(server_now_s), &local)) {
    return 0;
  }
  local.tm_hour = hour;
  local.tm_min = minute;
  local.tm_sec = 0;
  const std::time_t made = std::mktime(&local);
  if (made == static_cast<std::time_t>(-1)) {
    return 0;
  }
  auto when = static_cast<std::int64_t>(made);
  // Une mort ne peut pas être dans le futur : une heure « à venir » désigne donc
  // la veille, ce qui rend le « - » facultatif la plupart du temps.
  if (yesterday || when > server_now_s) {
    when -= kSecondsPerDay;
  }
  return when;
}

std::string MvpLineText(const MvpSlotView& slot, std::int64_t server_now_s) {
  return std::format("{} — {}", MvpLineName(slot), MvpReturnText(slot, server_now_s));
}

std::string MvpCollapsedTitle(std::span<const MvpSlotView> slots, std::int64_t server_now_s) {
  // LA PROCHAINE ÉCHÉANCE : la borne basse la plus proche, en écartant les
  // fenêtres passées depuis plus d'une demi-heure et les instants exacts passés
  // depuis plus d'un quart d'heure — au-delà, le MVP est revenu ou déjà retombé.
  // 🔴 L'ÉTAT D'ABORD, LA BORNE ENSUITE : un créneau dans sa fenêtre passe devant
  // une fenêtre close dont la borne basse est plus ancienne, et une échéance à
  // venir aussi.
  const MvpSlotView* best = nullptr;
  for (const MvpSlotView& slot : slots) {
    if (!slot.window_known) {
      continue;
    }
    if (!slot.exact && slot.window_to_s + kMvpCollapsedWindowFreshS < server_now_s) {
      continue;
    }
    if (slot.exact && slot.window_from_s + kMvpCollapsedExactFreshS < server_now_s) {
      continue;
    }
    if (best == nullptr) {
      best = &slot;
      continue;
    }
    const CollapsedStanding standing = StandingOf(slot, server_now_s);
    const CollapsedStanding best_standing = StandingOf(*best, server_now_s);
    if (standing < best_standing ||
        (standing == best_standing && slot.window_from_s < best->window_from_s)) {
      best = &slot;
    }
  }
  if (best == nullptr) {
    return core::Tr("Carnet de chasse MVP");
  }
  const std::string label(best->label);
  // Le même mot que la table et la ligne : un instant exact passé est « de
  // retour », une fenêtre close est « passée », une fenêtre ouverte est
  // « possible ».
  if (best->exact && best->window_from_s <= server_now_s) {
    return Printed(core::Tr("MVP : %s de retour"), label.c_str());
  }
  if (!best->exact && best->window_to_s <= server_now_s) {
    return Printed(core::Tr("MVP : %s, fenêtre passée"), label.c_str());
  }
  if (best->window_from_s <= server_now_s) {
    return Printed(core::Tr("MVP : %s possible"), label.c_str());
  }
  return Printed(core::Tr("MVP : %s dans %s"), label.c_str(),
                 FormatMvpDuration(best->window_from_s - server_now_s).c_str());
}

const char* MvpResultText(std::uint8_t code) {
  // Un numéro ne dit rien à personne : la raison du refus, en clair.
  switch (static_cast<MvpResult>(code)) {
    case MvpResult::kOk:
      return "";
    case MvpResult::kNoAccount:
      return core::Tr("Ce compte de jeu n'est rattaché à aucun compte Moonlight.");
    case MvpResult::kAlreadyMember:
      return core::Tr("Vous êtes déjà dans un groupe : quittez-le d'abord.");
    case MvpResult::kNotMember:
      return core::Tr("Vous n'êtes dans aucun groupe de chasse.");
    case MvpResult::kNotOwner:
      return core::Tr("Seul le propriétaire du groupe peut faire cela.");
    case MvpResult::kNoSuchUser:
      return core::Tr("Personnage introuvable : vérifiez l'orthographe.");
    case MvpResult::kSelf:
      return core::Tr("Pas sur vous-même.");
    case MvpResult::kFull:
      return core::Tr("Le groupe est complet.");
    case MvpResult::kBadName:
      return core::Tr("Nom de groupe invalide (1 à 31 caractères).");
    case MvpResult::kNoInvite:
      return core::Tr("Aucune invitation en attente.");
    case MvpResult::kTargetInGroup:
      return core::Tr("Cette personne appartient déjà à un autre groupe.");
    case MvpResult::kSql:
      return core::Tr("Erreur de base de données : rien n'a été changé.");
    case MvpResult::kTargetSameGroup:
      return core::Tr(
          "Ce personnage est déjà dans votre groupe : son compte Moonlight y est, avec tous ses "
          "personnages.");
    case MvpResult::kNotInvitable:
      return core::Tr("Ce personnage n'est ni dans votre guilde ni dans vos amis.");
    case MvpResult::kObservationKept:
      return core::Tr("Saisie sans effet : le groupe tient déjà une heure au moins aussi précise.");
    case MvpResult::kUnknownSlot:
      return core::Tr("Ce créneau MVP est inconnu du serveur.");
    case MvpResult::kTargetNotMember:
      return core::Tr("Cette personne n'est pas membre de votre groupe.");
    case MvpResult::kNameTooLong:
      return core::Tr("Nom de groupe trop long : 31 caractères au plus.");
  }
  // Un code que cette version ne connaît pas : le serveur est plus récent. On le
  // montre plutôt que de mentir avec un message voisin.
  return nullptr;
}

bool MvpResultRefuses(std::uint8_t code) {
  return code != static_cast<std::uint8_t>(MvpResult::kOk) &&
         code != static_cast<std::uint8_t>(MvpResult::kObservationKept);
}

const char* MvpLinkPendingText() {
  return core::Tr("Liaison avec le serveur en cours de vérification : réessayez dans un instant.");
}

const char* MvpTrackerOffText() { return core::Tr("Le carnet de chasse est éteint."); }

const char* MvpSourceLabel(std::uint8_t source_raw) {
  switch (static_cast<MvpSource>(source_raw)) {
    case MvpSource::kManual:
      return core::Tr("saisie");
    case MvpSource::kTomb:
      return core::Tr("tombe");
    case MvpSource::kKill:
      return core::Tr("tué");
    case MvpSource::kMirror:
      return core::Tr("miroir");
  }
  return "?";
}

std::string MvpSortWord(int sort_key) {
  using mvp_tracker::kMvpSortKeysPerColumn;
  if (sort_key < kMvpSortKeysPerColumn) {
    return "none";
  }
  const char* column = "?";
  switch (sort_key / kMvpSortKeysPerColumn - 1) {
    case mvp_tracker::kMvpColumnFavorite:
      column = "favorite";
      break;
    case mvp_tracker::kMvpColumnName:
      column = "name";
      break;
    case mvp_tracker::kMvpColumnMap:
      column = "map";
      break;
    case mvp_tracker::kMvpColumnDelay:
      column = "delay";
      break;
    case mvp_tracker::kMvpColumnReturn:
      column = "return";
      break;
    case mvp_tracker::kMvpColumnAge:
      column = "age";
      break;
    case mvp_tracker::kMvpColumnSource:
      column = "source";
      break;
    default:
      break;
  }
  return std::string(column) + (sort_key % kMvpSortKeysPerColumn != 0 ? "_asc" : "_desc");
}

bool MvpMatchesFilter(const MvpSlotView& slot, std::string_view filter) {
  // Sur le NOM et la CARTE, lisible comme interne : c'est par l'un ou par
  // l'autre qu'on cherche un MVP, et exiger de savoir lequel avant de taper
  // serait pénible pour rien.
  if (filter.empty()) {
    return true;
  }
  return ContainsFolded(slot.label, filter) || ContainsFolded(slot.map, filter) ||
         ContainsFolded(slot.map_label, filter);
}

float MvpTableRowHeight(bool show_sprites, const MvpTrackerView& view, const SkinArt& art) {
  const float padding = ImGui::GetStyle().CellPadding.y * 2.0F;
  if (show_sprites) {
    return Px(kMvpSpritePixels) + padding;
  }
  // Sans sprite, le rang a la hauteur de sa plus haute cellule : le texte, le
  // petit bouton de la source, l'étoile.
  const float star =
      std::max(FavoriteHeight(view.favorite_off, art), FavoriteHeight(view.favorite_on, art));
  return std::max({ImGui::GetTextLineHeight(), SkinSmallButtonHeight(art), star}) + padding;
}

std::string_view MvpMapShown(const MvpSlotView& slot) {
  return slot.map_label.empty() ? slot.map : slot.map_label;
}

std::string MvpLineName(const MvpSlotView& slot) {
  // 🔴 LA CARTE FAIT PARTIE DU NOM : plusieurs créneaux portent le même MVP, et
  // une ligne détachée n'a que ce texte pour dire lequel elle surveille.
  return std::format("{} ({})", slot.label, MvpMapShown(slot));
}

std::string MvpReturnText(const MvpSlotView& slot, std::int64_t server_now_s) {
  // Rien d'observé : rien d'écrit, même si le serveur connaît le tirage — c'est
  // la règle de non-triche, visible à l'œil nu.
  const std::int64_t from = slot.window_from_s;
  const std::int64_t to = slot.window_to_s;
  if (!slot.window_known) {
    return core::Tr("jamais vu");
  }
  if (slot.exact) {
    if (from <= server_now_s) {
      return core::Tr("de retour");
    }
    return Printed(core::Tr("dans %s (exact)"), FormatMvpDuration(from - server_now_s).c_str());
  }
  if (to <= server_now_s) {
    return core::Tr("fenêtre passée");
  }
  if (from <= server_now_s) {
    return Printed(core::Tr("possible (encore %s)"), FormatMvpDuration(to - server_now_s).c_str());
  }
  return std::format("{} – {}", FormatMvpDuration(from - server_now_s),
                     FormatMvpDuration(to - server_now_s));
}

MvpReturnTone MvpReturnToneOf(const MvpSlotView& slot, std::int64_t server_now_s) {
  if (!slot.window_known) {
    return MvpReturnTone::kDim;
  }
  if (!slot.exact && slot.window_from_s <= server_now_s && server_now_s < slot.window_to_s) {
    return MvpReturnTone::kPossible;
  }
  return MvpReturnTone::kPlain;
}

std::string MvpGroupHeader(std::string_view group_name, int accounts) {
  // ⚠ LES DEUX FORMES PARTAGENT L'IDENTITÉ « ###mvp_group » : l'en-tête garde
  // son pli quand le groupe passe d'un à deux membres.
  const std::string name(group_name);
  if (accounts == 1) {
    return Printed(core::Tr("Groupe « %s » — 1 membre###mvp_group"), name.c_str());
  }
  return Printed(core::Tr("Groupe « %s » — %d membres###mvp_group"), name.c_str(), accounts);
}

std::string MvpKillTimeEcho(std::int64_t kill_time_s, std::int64_t server_now_s) {
  // La saisie ne garde que les chiffres et devine la veille : dire ce qu'elle a
  // compris évite d'envoyer au groupe une mort décalée d'un jour.
  std::tm kill{};
  std::tm now{};
  if (!LocalTimeOf(static_cast<std::time_t>(kill_time_s), &kill) ||
      !LocalTimeOf(static_cast<std::time_t>(server_now_s), &now)) {
    return {};
  }
  const std::string clock = std::format("{:02}:{:02}", kill.tm_hour, kill.tm_min);
  if (kill.tm_year == now.tm_year && kill.tm_yday == now.tm_yday) {
    return Printed(core::Tr("%s aujourd'hui"), clock.c_str());
  }
  return Printed(core::Tr("%s hier"), clock.c_str());
}

std::size_t MvpWireCut(std::string_view utf8, std::size_t most_wire_bytes) {
  // Caractère par caractère : chacun prend sa place sur le fil APRÈS
  // conversion, et la coupe tombe toujours avant un octet de tête.
  std::size_t cut = 0;
  std::size_t wire_bytes = 0;
  std::size_t at = 0;
  while (at < utf8.size()) {
    std::size_t next = at + 1;
    while (next < utf8.size() && IsUtf8Continuation(utf8[next])) {
      ++next;
    }
    wire_bytes += core::Cp1252FromUtf8(utf8.substr(at, next - at)).size();
    if (wire_bytes > most_wire_bytes) {
      break;
    }
    cut = next;
    at = next;
  }
  return cut;
}

MvpWireExcess MvpWireExcessOf(std::string_view utf8, std::size_t cursor,
                              std::size_t most_wire_bytes) {
  const std::size_t fits = MvpWireCut(utf8, most_wire_bytes);
  if (fits == utf8.size()) {
    return {};
  }
  const std::size_t excess = core::Cp1252FromUtf8(utf8).size() - most_wire_bytes;
  // On remonte du curseur, caractère par caractère, jusqu'à rendre au fil ce
  // qu'il a de trop.
  std::size_t end = std::min(cursor, utf8.size());
  while (end > 0 && end < utf8.size() && IsUtf8Continuation(utf8[end])) {
    --end;
  }
  std::size_t start = end;
  std::size_t removed = 0;
  while (start > 0 && removed < excess) {
    std::size_t lead = start - 1;
    while (lead > 0 && IsUtf8Continuation(utf8[lead])) {
      --lead;
    }
    removed += core::Cp1252FromUtf8(utf8.substr(lead, start - lead)).size();
    start = lead;
  }
  if (removed < excess) {
    return {.at = fits, .bytes = utf8.size() - fits};
  }
  return {.at = start, .bytes = end - start};
}

namespace mvp_tracker {

std::string Printed(const char* format, ...) {
  std::array<char, kMvpPrintedBytes> text{};
  std::va_list arguments;
  va_start(arguments, format);
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
  std::vsnprintf(text.data(), text.size(), format, arguments);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  va_end(arguments);
  return text.data();
}

void Unformatted(std::string_view text) {
  ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

void TellLinkPending(const MvpTrackerView& view) {
  if (!view.link_ready && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltipFormat("%s", MvpLinkPendingText());
  }
}

const MvpSlotView* FindSlotView(std::span<const MvpSlotView> slots, std::uint16_t slot_id) {
  for (const MvpSlotView& slot : slots) {
    if (slot.slot_id == slot_id) {
      return &slot;
    }
  }
  return nullptr;
}

MvpRowKey RowKeyOf(const MvpSlotView& slot) {
  return MvpRowKey{.slot_id = slot.slot_id,
                   .delay1_ms = slot.delay1_ms,
                   .favorite = slot.favorite,
                   .window_from_s = slot.window_known ? slot.window_from_s : 0,
                   .reported_at_s = slot.observed ? slot.reported_at_s : 0,
                   .label = std::string(slot.label),
                   .map = std::string(slot.map),
                   .map_label = std::string(slot.map_label)};
}

bool RowBefore(const MvpSlotView& a, const MvpSlotView& b, int column, bool ascending) {
  int said = 0;
  switch (column) {
    case kMvpColumnFavorite:
      // Ascendant = favoris EN TÊTE : c'est le sens qu'on attend d'un premier
      // clic. Le nom départage.
      if (a.favorite != b.favorite) {
        return ascending ? a.favorite : b.favorite;
      }
      said = CompareBytes(a.label, b.label);
      break;
    case kMvpColumnMap:
      // Le nom MONTRÉ : un tri sur le nom interne paraîtrait désordonné.
      said = CompareBytes(MvpMapShown(a), MvpMapShown(b));
      break;
    case kMvpColumnDelay:
      // La borne BASSE, celle qui dit « au plus tôt ».
      said = CompareValues(a.delay1_ms, b.delay1_ms);
      break;
    case kMvpColumnReturn:
      if (!a.window_known || !b.window_known) {
        return a.window_known && !b.window_known;
      }
      said = CompareValues(a.window_from_s, b.window_from_s);
      break;
    case kMvpColumnAge:
      if (!a.observed || !b.observed) {
        return a.observed && !b.observed;
      }
      // Le plus récent d'abord, en croissant.
      said = CompareValues(b.reported_at_s, a.reported_at_s);
      break;
    default:
      said = CompareBytes(a.label, b.label);
      break;
  }
  return ascending ? said < 0 : said > 0;
}

}  // namespace mvp_tracker

}  // namespace ro::ui
