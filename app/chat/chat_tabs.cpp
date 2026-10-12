#include "chat/chat_tabs.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <utility>

#include "ro/core/i18n.h"

#include "chat/whisper_desk.h"

namespace moonclient {
namespace {

using ro::sim::ChatType;

// Les étiquettes du diagnostic, une par numéro. ⚠ ÉCRITES ICI ET NON
// COMPOSÉES : la ligne les garde par vue, sans copie, le temps de l'image.
constexpr std::array<std::string_view, ro::sim::kChatTypeCount> kTypeTags = {
    "t00", "t01", "t02", "t03", "t04", "t05", "t06", "t07", "t08", "t09",
    "t10", "t11", "t12", "t13", "t14", "t15", "t16", "t17", "t18", "t19",
    "t20", "t21", "t22", "t23", "t24", "t25", "t26", "t27"};

// Le premier numéro d'un onglet neuf, et le dernier qu'on essaie : au-delà,
// le plafond des onglets est atteint depuis longtemps.
constexpr int kFirstNewTabNumber = 2;
constexpr int kLastNewTabNumber = 99;
constexpr std::size_t kNewTabNameSize = 32;
// Le nom sans numéro, quand tous sont pris.
constexpr std::string_view kNewTabFallbackName = "NewTab";

}  // namespace

std::string_view ChatTypeTag(ChatType type) {
  const auto rank = static_cast<std::size_t>(type);
  return rank < kTypeTags.size() ? kTypeTags[rank] : std::string_view();
}

bool TabHasUnread(const ChatTab& tab, std::span<const ro::sim::ChatLine> chat,
                  const WhisperDesk* desk) {
  // Le compteur n'a rien à lire.
  if (tab.content != ChatTabContent::kLog) {
    return false;
  }
  // ⚠ DEPUIS LA FIN, ET L'ON S'ARRÊTE À CE QUI EST LU : les rangs croissent le
  // long du tampon, et la question se pose pour chaque onglet à chaque image.
  for (auto line = chat.rbegin(); line != chat.rend(); ++line) {
    if (line->seq <= tab.read_through ||
        (tab.cleared_through != 0 && line->seq <= tab.cleared_through)) {
      return false;
    }
    if (!tab.Accepts(line->type)) {
      continue;
    }
    // Ce qu'une conversation 1:1 a pris ne paraît pas dans l'onglet : il n'y a
    // rien à y lire.
    if (desk != nullptr && line->whisper.side != ro::sim::WhisperSide::kNone &&
        desk->ShownApart(line->seq)) {
      continue;
    }
    return true;
  }
  return false;
}

std::optional<ChatType> ChatTypeOf(int number) {
  if (number < 0 || static_cast<std::size_t>(number) >= ro::sim::kChatTypeCount) {
    return std::nullopt;
  }
  return static_cast<ChatType>(number);
}

std::string NewTabName(std::span<const ChatTab> tabs) {
  for (int number = kFirstNewTabNumber; number <= kLastNewTabNumber; ++number) {
    std::array<char, kNewTabNameSize> name{};
    std::snprintf(name.data(), name.size(), ro::core::Tr("NewTab_%d"), number);
    const bool taken = std::any_of(tabs.begin(), tabs.end(),
                                   [&name](const ChatTab& tab) { return tab.name == name.data(); });
    if (!taken) {
      return std::string(name.data());
    }
  }
  return std::string(kNewTabFallbackName);
}

ChatTabs::ChatTabs() {
  // ⚠ UN SEUL ONGLET AU DÉPART, ET IL MONTRE TOUT — c'est ce que fait le client.
  // En poser quatre d'avance déciderait à la place du joueur de la façon dont il
  // range sa conversation.
  //
  // ⚠ ET LA FENÊTRE PRINCIPALE NAÎT FIGÉE : la disposition d'usine la pose en
  // bas à gauche, et un glisser malheureux ne doit pas l'en décoller.
  tabs_.push_back(ChatTab{
      .name = std::string(ro::core::Tr("Tout")), .accepted = kDefaultChatTypes, .locked = true});
}

std::size_t ChatTabs::Add(std::string name) {
  tabs_.push_back(ChatTab{.name = std::move(name), .accepted = kDefaultChatTypes});
  ++revision_;
  return tabs_.size() - 1;
}

void ChatTabs::Rename(std::size_t rank, std::string name) {
  if (rank >= tabs_.size() || tabs_[rank].name == name) {
    return;
  }
  tabs_[rank].name = std::move(name);
  ++revision_;
}

void ChatTabs::Remove(std::size_t rank) {
  // ⚠ LE DERNIER ONGLET DE FIL NE SE RETIRE PAS : il ne resterait rien où lire,
  // ni rien où cliquer pour en refaire un. Le compteur ne compte pas.
  if (rank >= tabs_.size()) {
    return;
  }
  const auto logs = std::count_if(tabs_.begin(), tabs_.end(), [](const ChatTab& tab) {
    return tab.content == ChatTabContent::kLog;
  });
  if (tabs_[rank].content == ChatTabContent::kLog && logs <= 1) {
    return;
  }
  tabs_.erase(tabs_.begin() + static_cast<std::ptrdiff_t>(rank));
  ++revision_;
}

void ChatTabs::ClearThrough(std::size_t rank, std::uint64_t seq) {
  if (rank >= tabs_.size()) {
    return;
  }
  tabs_[rank].cleared_through = seq;
}

void ChatTabs::MarkRead(std::size_t rank, std::uint64_t seq) {
  if (rank >= tabs_.size()) {
    return;
  }
  tabs_[rank].read_through = seq;
}

void ChatTabs::Move(std::size_t rank, std::size_t before) {
  if (rank >= tabs_.size() || before > tabs_.size()) {
    return;
  }
  // Le retrait décale d'un cran tout ce qui suivait : une arrivée située
  // APRÈS le départ se lit donc un rang plus tôt une fois l'onglet sorti.
  const std::size_t landing = before > rank ? before - 1 : before;
  if (landing == rank) {
    return;  // reposé là où il était : rien n'a bougé
  }
  ChatTab moved = std::move(tabs_[rank]);
  tabs_.erase(tabs_.begin() + static_cast<std::ptrdiff_t>(rank));
  tabs_.insert(tabs_.begin() + static_cast<std::ptrdiff_t>(landing), std::move(moved));
  ++revision_;
}

void ChatTabs::SetAccepted(std::size_t rank, ChatTypeMask accepted) {
  if (rank >= tabs_.size() || tabs_[rank].accepted == accepted) {
    return;
  }
  tabs_[rank].accepted = accepted;
  ++revision_;
}

std::uint32_t ChatTabs::Detach(std::size_t rank) {
  if (rank >= tabs_.size()) {
    return 0;
  }
  // Déjà seul dans sa fenêtre : le geste n'aurait rien à faire, et il
  // consommerait un numéro pour rien.
  const std::uint32_t was = tabs_[rank].group;
  if (was != 0 && AloneIn(was)) {
    return was;
  }
  // ⚠ LE NUMÉRO NE SE RÉEMPLOIE JAMAIS : il nomme la fenêtre, donc ce qui lui
  // garde sa position et sa taille.
  tabs_[rank].group = next_group_++;
  ++revision_;
  return tabs_[rank].group;
}

void ChatTabs::Attach(std::size_t rank, std::uint32_t group) {
  if (rank >= tabs_.size() || tabs_[rank].group == group) {
    return;
  }
  // ⚠ ON NE REJOINT PAS UNE FENÊTRE QUI N'EXISTE PAS. Un numéro relu d'un
  // fichier retouché à la main, ou resté d'un onglet retiré, laisserait
  // l'onglet dans une fenêtre que rien ne dessine : il disparaîtrait sans que
  // rien ne le dise.
  if (group != 0 && !Holds(group)) {
    return;
  }
  tabs_[rank].group = group;
  ++revision_;
}

std::vector<std::uint32_t> ChatTabs::Groups() const {
  // ⚠ LA PRINCIPALE D'ABORD ET TOUJOURS, même sans onglet : elle porte la
  // saisie, et une conversation où l'on ne peut plus rien écrire parce qu'on a
  // sorti tous ses onglets serait un piège sans retour.
  std::vector<std::uint32_t> living = {0};
  for (const ChatTab& tab : tabs_) {
    if (tab.group == 0 || std::find(living.begin(), living.end(), tab.group) != living.end()) {
      continue;
    }
    living.push_back(tab.group);
  }
  return living;
}

bool ChatTabs::Holds(std::uint32_t group) const {
  return std::any_of(tabs_.begin(), tabs_.end(),
                     [group](const ChatTab& tab) { return tab.group == group; });
}

std::optional<std::size_t> ChatTabs::MeterRank() const {
  const auto found = std::find_if(tabs_.begin(), tabs_.end(), [](const ChatTab& tab) {
    return tab.content == ChatTabContent::kDamageMeter;
  });
  if (found == tabs_.end()) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(found - tabs_.begin());
}

void ChatTabs::DockMeter(std::string name, std::uint32_t group, std::size_t before) {
  // ⚠ UNE FENÊTRE QUI N'EXISTE PLUS SE LIT COMME LA PRINCIPALE : le compteur
  // doit atterrir quelque part, et c'est elle qui ne disparaît jamais.
  if (group != 0 && !Holds(group)) {
    group = 0;
  }
  std::size_t rank = 0;
  if (const std::optional<std::size_t> kept = MeterRank(); kept.has_value()) {
    rank = *kept;
    if (tabs_[rank].group != group) {
      tabs_[rank].group = group;
      ++revision_;
    }
  } else {
    // 🔴 IL PREND LE VERROU DE SA FENÊTRE, comme tous ses onglets : sinon,
    // montré, il la libérerait ou la figerait à lui seul.
    const auto host = std::find_if(tabs_.begin(), tabs_.end(),
                                   [group](const ChatTab& tab) { return tab.group == group; });
    tabs_.push_back(ChatTab{.name = std::move(name),
                            .content = ChatTabContent::kDamageMeter,
                            .group = group,
                            .locked = host != tabs_.end() && host->locked});
    rank = tabs_.size() - 1;
    ++revision_;
  }
  Move(rank, std::min(before, tabs_.size()));
}

void ChatTabs::UndockMeter() {
  if (const std::optional<std::size_t> kept = MeterRank(); kept.has_value()) {
    meter_home_ = MeterHome{.group = tabs_[*kept].group, .rank = *kept};
    tabs_.erase(tabs_.begin() + static_cast<std::ptrdiff_t>(*kept));
    ++revision_;
  }
}

void ChatTabs::RedockMeter(std::string name) {
  // ⚠ UNE FENÊTRE PARTIE DEPUIS SE LIT COMME LA PRINCIPALE : c'est `DockMeter`
  // qui en décide, et le rang trop grand y tombe au bout.
  const MeterHome home = meter_home_.value_or(MeterHome{.group = 0, .rank = tabs_.size()});
  DockMeter(std::move(name), home.group, home.rank);
}

bool ChatTabs::FollowMeterDock(bool wanted, bool seen, std::string name) {
  const bool docked = MeterRank().has_value();
  if (wanted != seen && wanted && !docked) {
    RedockMeter(std::move(name));
  } else if (wanted != seen && !wanted && docked) {
    UndockMeter();
  }
  return MeterRank().has_value();
}

bool ChatTabs::lone_meter() const {
  const std::optional<std::size_t> kept = MeterRank();
  return kept.has_value() && tabs_[*kept].group != 0 && AloneIn(tabs_[*kept].group);
}

bool ChatTabs::AloneIn(std::uint32_t group) const {
  return std::count_if(tabs_.begin(), tabs_.end(),
                       [group](const ChatTab& tab) { return tab.group == group; }) == 1;
}

void ChatTabs::SetGroupLocked(std::uint32_t group, bool locked) {
  bool moved = false;
  for (ChatTab& tab : tabs_) {
    if (tab.group == group && tab.locked != locked) {
      tab.locked = locked;
      moved = true;
    }
  }
  if (moved) {
    ++revision_;
  }
}

void ChatTabs::SetLook(std::size_t rank, const TabLook& look) {
  if (rank >= tabs_.size()) {
    return;
  }
  tabs_[rank].look = look;
  ++revision_;
}

void ChatTabs::Replace(std::vector<ChatTab> tabs) {
  // Un seul compteur : le premier rangé l'emporte.
  bool meter_seen = false;
  std::erase_if(tabs, [&meter_seen](const ChatTab& tab) {
    if (tab.content != ChatTabContent::kDamageMeter) {
      return false;
    }
    return std::exchange(meter_seen, true);
  });
  if (std::none_of(tabs.begin(), tabs.end(),
                   [](const ChatTab& tab) { return tab.content == ChatTabContent::kLog; })) {
    return;
  }
  tabs_ = std::move(tabs);
  // ⚠ LE COMPTEUR SE RELÈVE AU-DESSUS DE CE QUE LE FICHIER PORTE. Repartir de
  // un rendrait à une fenêtre neuve le numéro d'une fenêtre encore ouverte,
  // donc sa position et sa taille.
  for (const ChatTab& tab : tabs_) {
    next_group_ = std::max(next_group_, tab.group + 1);
  }
  ++revision_;
}

}  // namespace moonclient
