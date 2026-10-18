#include "items/card_drop.h"

#include <algorithm>
#include <cstddef>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::sim::InventoryItem;
using ro::sim::ItemCategory;
using ro::ui::CardDropAim;

[[nodiscard]] bool IsGear(const InventoryItem& item) noexcept {
  const ItemCategory family = ro::sim::ItemCategoryOf(item);
  return family == ItemCategory::kWeapon || family == ItemCategory::kArmor;
}

// LA PAIRE CONSERVÉE PAR L'UNE DES DEUX FENÊTRES : un seul curseur, donc une
// seule cible à la fois.
[[nodiscard]] CardDropAim EitherAim(const CardDropAim& first, const CardDropAim& second) noexcept {
  return first.aimed() ? first : second;
}

}  // namespace

CardDropVerdict JudgeCardDrop(const CardDropAim& aim, const CardDropFacts& facts) {
  if (facts.bag == nullptr || !aim.aimed()) {
    return CardDropVerdict::kVanished;
  }
  const InventoryItem* const card = facts.bag->Find(aim.card_slot);
  const InventoryItem* const equip = facts.bag->Find(aim.equip_slot);
  if (card == nullptr || equip == nullptr ||
      ro::sim::ItemCategoryOf(*card) != ItemCategory::kCard || !IsGear(*equip)) {
    return CardDropVerdict::kVanished;
  }
  if (facts.dealing) {
    return CardDropVerdict::kDealing;
  }
  if (equip->equipped()) {
    return CardDropVerdict::kWorn;
  }
  if (!equip->identified) {
    return CardDropVerdict::kUnidentified;
  }
  if (ro::sim::ItemIsForged(*equip)) {
    return CardDropVerdict::kForged;
  }
  const int total = facts.card_slots_of ? facts.card_slots_of(equip->item_id) : 0;
  if (total <= 0) {
    return CardDropVerdict::kNoSocket;
  }
  if (ro::sim::ItemCardsInSlots(*equip, static_cast<std::size_t>(total)) >=
      static_cast<std::size_t>(total)) {
    return CardDropVerdict::kFull;
  }
  // ⚠ UNE RÉPONSE NE VAUT QUE POUR SA PIÈCE : celle d'une autre pièce, ou une
  // question encore sans réponse, ne dit rien de celle-ci.
  const ro::sim::CompatibleCards* const compatible = facts.compatible;
  if (compatible == nullptr || compatible->equip_slot != aim.equip_slot || !compatible->answered) {
    return CardDropVerdict::kPending;
  }
  const bool listed =
      std::ranges::find(compatible->card_slots, aim.card_slot) != compatible->card_slots.end();
  return listed ? CardDropVerdict::kAccepted : CardDropVerdict::kIncompatible;
}

const char* CardDropRefusalText(CardDropVerdict verdict) {
  switch (verdict) {
    case CardDropVerdict::kAccepted:
    case CardDropVerdict::kPending:
    case CardDropVerdict::kVanished:
      return nullptr;
    case CardDropVerdict::kDealing:
      return ro::core::Tr("Impossible de sertir une carte pendant un échange.");
    case CardDropVerdict::kWorn:
      return ro::core::Tr("Retirez d'abord cette pièce : une pièce portée ne reçoit pas de carte.");
    case CardDropVerdict::kUnidentified:
      return ro::core::Tr("Cet item n'est pas identifié : il ne reçoit pas de carte.");
    case CardDropVerdict::kForged:
      return ro::core::Tr("Un item forgé ou signé ne reçoit pas de carte.");
    case CardDropVerdict::kNoSocket:
      return ro::core::Tr("Cet item n'a aucun emplacement de carte.");
    case CardDropVerdict::kFull:
      return ro::core::Tr("Tous les emplacements de cet item sont occupés.");
    case CardDropVerdict::kIncompatible:
      return ro::core::Tr("Cette carte ne se sertit pas sur ce type d'équipement.");
  }
  return nullptr;
}

ro::ui::InventoryWindowAction CardDropAsQuickInsert(const CardDropAim& aim) {
  ro::ui::InventoryWindowAction asked;
  asked.verb = ro::ui::ItemVerb::kQuickInsert;
  asked.slot = aim.equip_slot;
  asked.insert_card_slot = aim.card_slot;
  return asked;
}

bool CardDropFlow::Settle(const CardDropAim& aim, CardDropVerdict verdict, CardDropStep& step) {
  switch (verdict) {
    case CardDropVerdict::kAccepted:
      step.order = aim;
      return false;
    case CardDropVerdict::kPending:
      return true;
    case CardDropVerdict::kVanished:
      return false;
    case CardDropVerdict::kDealing:
    case CardDropVerdict::kWorn:
    case CardDropVerdict::kUnidentified:
    case CardDropVerdict::kForged:
    case CardDropVerdict::kNoSocket:
    case CardDropVerdict::kFull:
    case CardDropVerdict::kIncompatible:
      step.refusal = CardDropRefusalText(verdict);
      return false;
  }
  return false;
}

CardDropStep CardDropFlow::Step(const ro::ui::CardDropSeen& bag_seen,
                                const ro::ui::CardDropSeen& doll_seen, const CardDropFacts& facts,
                                std::uint32_t now_ms) {
  CardDropStep step;

  // ── LE DÉPÔT ──────────────────────────────────────────────────────────────
  //
  // ⚠ UN DÉPÔT NEUF REMPLACE CELUI QUI ATTENDAIT : c'est le dernier geste du
  // joueur qui compte.
  const CardDropAim dropped = EitherAim(bag_seen.dropped, doll_seen.dropped);
  if (dropped.aimed()) {
    waiting_ = CardDropAim{};
    if (Settle(dropped, JudgeCardDrop(dropped, facts), step)) {
      waiting_ = dropped;
      waiting_since_ms_ = now_ms;
      step.ask_compatible = dropped.equip_slot;
    }
  } else if (waiting_.aimed()) {
    const CardDropVerdict verdict = JudgeCardDrop(waiting_, facts);
    if (verdict != CardDropVerdict::kPending) {
      static_cast<void>(Settle(waiting_, verdict, step));
      waiting_ = CardDropAim{};
    } else if (now_ms - waiting_since_ms_ >= kCardDropWaitMs) {
      step.order = waiting_;
      waiting_ = CardDropAim{};
    }
  }

  // ── LE SURVOL ─────────────────────────────────────────────────────────────
  const CardDropAim hovering = EitherAim(bag_seen.hovering, doll_seen.hovering);
  if (!hovering.aimed()) {
    return step;
  }
  const CardDropVerdict verdict = JudgeCardDrop(hovering, facts);
  step.hint.card_slot = hovering.card_slot;
  step.hint.equip_slot = hovering.equip_slot;
  switch (verdict) {
    case CardDropVerdict::kAccepted:
      step.hint.look = ro::ui::CardDropLook::kAccepted;
      break;
    case CardDropVerdict::kPending:
    case CardDropVerdict::kVanished:
      step.hint.look = ro::ui::CardDropLook::kUnknown;
      break;
    case CardDropVerdict::kDealing:
    case CardDropVerdict::kWorn:
    case CardDropVerdict::kUnidentified:
    case CardDropVerdict::kForged:
    case CardDropVerdict::kNoSocket:
    case CardDropVerdict::kFull:
    case CardDropVerdict::kIncompatible:
      step.hint.look = ro::ui::CardDropLook::kRefused;
      step.hint.refusal = CardDropRefusalText(verdict);
      break;
  }
  // ⚠ LA QUESTION DU SURVOL NE CHASSE PAS CELLE D'UN DÉPÔT EN ATTENTE : le
  // serveur ne garde qu'une question, et sa réponse n'en tranche qu'une.
  if (verdict == CardDropVerdict::kPending && !waiting_.aimed() && step.ask_compatible == 0) {
    step.ask_compatible = hovering.equip_slot;
  }
  return step;
}

}  // namespace moonclient
