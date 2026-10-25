#include "survivors/real_world_watch.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>

#include "ro/core/i18n.h"

namespace moonclient {

std::optional<std::uint32_t> NearestCells(GroundCell from, std::span<const GroundCell> hostiles) {
  std::optional<std::uint32_t> nearest_cells;
  for (const GroundCell& hostile : hostiles) {
    const auto gap_x = static_cast<std::uint32_t>(std::abs(std::int64_t{hostile.x} - from.x));
    const auto gap_y = static_cast<std::uint32_t>(std::abs(std::int64_t{hostile.y} - from.y));
    const std::uint32_t cells = std::max(gap_x, gap_y);
    if (!nearest_cells || cells < *nearest_cells) nearest_cells = cells;
  }
  return nearest_cells;
}

std::optional<EntryRefusal> Judge(const EntryGuardFacts& facts) {
  if (facts.link_closed) return EntryRefusal::kLinkClosed;
  if (facts.loading) return EntryRefusal::kLoading;
  if (facts.dead) return EntryRefusal::kDead;
  if (!facts.on_town_map && !facts.on_indoor_map) return EntryRefusal::kNotSheltered;
  if (facts.nearest_hostile_cells && *facts.nearest_hostile_cells < kHostileGuardCells) {
    return EntryRefusal::kHostileNearby;
  }
  if (facts.trade_open) return EntryRefusal::kTradeOpen;
  if (facts.npc_dialogue_open) return EntryRefusal::kNpcDialogueOpen;
  if (facts.shop_open) return EntryRefusal::kShopOpen;
  if (facts.storage_open) return EntryRefusal::kStorageOpen;
  return std::nullopt;
}

const char* RefusalText(EntryRefusal refusal) {
  switch (refusal) {
    case EntryRefusal::kLinkClosed:
      return ro::core::TrText("La liaison avec le serveur est perdue.");
    case EntryRefusal::kLoading:
      return ro::core::TrText("Un chargement de carte est en cours.");
    case EntryRefusal::kDead:
      return ro::core::TrText("Le personnage est mort.");
    case EntryRefusal::kNotSheltered:
      return ro::core::TrText("Il faut être en ville ou dans un intérieur.");
    case EntryRefusal::kHostileNearby:
      return ro::core::TrText("Un monstre hostile est trop proche.");
    case EntryRefusal::kTradeOpen:
      return ro::core::TrText("Un échange est ouvert.");
    case EntryRefusal::kNpcDialogueOpen:
      return ro::core::TrText("Un dialogue avec un PNJ est ouvert.");
    case EntryRefusal::kShopOpen:
      return ro::core::TrText("Une vente est ouverte.");
    case EntryRefusal::kStorageOpen:
      return ro::core::TrText("Le storage est ouvert.");
  }
  return "";
}

bool Pauses(RealWorldAlert alert) noexcept {
  return alert != RealWorldAlert::kWhisper && alert != RealWorldAlert::kLinkLost;
}

const char* BannerText(RealWorldAlert alert) {
  switch (alert) {
    case RealWorldAlert::kDied:
      return ro::core::TrText("Votre personnage est mort dans le jeu.");
    case RealWorldAlert::kHit:
      return ro::core::TrText("Votre personnage est attaqué !");
    case RealWorldAlert::kMoved:
      return ro::core::TrText("Votre personnage a changé de carte.");
    case RealWorldAlert::kTrade:
      return ro::core::TrText("On vous propose un échange.");
    case RealWorldAlert::kInvite:
      return ro::core::TrText("Vous avez reçu une invitation.");
    case RealWorldAlert::kDialogue:
      return ro::core::TrText("Un PNJ vous parle.");
    case RealWorldAlert::kLinkLost:
    case RealWorldAlert::kWhisper:
      return nullptr;
  }
  return nullptr;
}

bool RealWorldReport::pauses() const noexcept {
  return std::ranges::any_of(alerts, [](RealWorldAlert alert) { return Pauses(alert); });
}

bool RealWorldReport::link_lost() const noexcept {
  return std::ranges::find(alerts, RealWorldAlert::kLinkLost) != alerts.end();
}

std::optional<RealWorldAlert> RealWorldReport::banner() const noexcept {
  const auto first =
      std::ranges::find_if(alerts, [](RealWorldAlert alert) { return Pauses(alert); });
  if (first == alerts.end()) return std::nullopt;
  return *first;
}

namespace {

// Un drapeau qui passe à vrai : un état qui dure n'alerte qu'une fois.
[[nodiscard]] bool Rose(bool previous, bool now) noexcept { return !previous && now; }

}  // namespace

RealWorldReport RealWorldWatch::Compare(const RealWorldFacts& previous, const RealWorldFacts& now) {
  RealWorldReport report;
  // Une alerte par énumérateur au plus, poussée dans l'ordre de l'énumération.
  const auto raise_if = [&report](bool happened, RealWorldAlert alert) {
    if (happened) report.alerts.push_back(alert);
  };
  raise_if(previous.link_open && !now.link_open, RealWorldAlert::kLinkLost);
  raise_if(Rose(previous.dead, now.dead), RealWorldAlert::kDied);
  // Un coup reçu se lit à la BAISSE des PV d'un personnage vivant ; leur
  // remontée (régénération, soin) ne dit rien. Un plafond qui baisse (fin d'un
  // bonus de PV max) ramène les PV jusqu'à lui sans que personne ne frappe :
  // seule une baisse sous le nouveau plafond compte alors.
  const std::int32_t unhurt_hp =
      now.max_hp < previous.max_hp ? std::min(previous.hp, now.max_hp) : previous.hp;
  raise_if(!now.dead && now.hp < unhurt_hp, RealWorldAlert::kHit);
  raise_if(now.map_entries > previous.map_entries, RealWorldAlert::kMoved);
  raise_if(Rose(previous.trade_offered, now.trade_offered), RealWorldAlert::kTrade);
  raise_if(Rose(previous.party_invited, now.party_invited) ||
               Rose(previous.guild_invited, now.guild_invited) ||
               Rose(previous.friend_invited, now.friend_invited),
           RealWorldAlert::kInvite);
  raise_if(Rose(previous.server_dialogue_open, now.server_dialogue_open),
           RealWorldAlert::kDialogue);
  if (now.whispers > previous.whispers) {
    report.new_whispers = now.whispers - previous.whispers;
    report.alerts.push_back(RealWorldAlert::kWhisper);
  }
  return report;
}

RealWorldReport RealWorldWatch::Observe(const RealWorldFacts& now) {
  RealWorldReport report = Compare(previous_, now);
  previous_ = now;
  return report;
}

}  // namespace moonclient
