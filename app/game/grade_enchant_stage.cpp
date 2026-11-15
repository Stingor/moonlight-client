// L'amélioration de grade, dans la boucle. Voir l'en-tête.

#include "game/grade_enchant_stage.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "ro/core/log.h"
#include "ro/net/grade_enchant.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

// L'ARGENT DU JOUEUR, ou « assez » tant que le serveur ne l'a pas dit : le serveur
// reste alors seul à juger.
[[nodiscard]] std::int64_t PurseOf(const TradeContext& context) {
  const std::int64_t zeny = context.world.player_status().zeny;
  return zeny == ro::sim::kUnknownStat ? std::numeric_limits<std::int64_t>::max() : zeny;
}

[[nodiscard]] ro::sim::GradeEnchantAttempt AttemptOf(const ro::ui::GradeEnchantWindowState& state) {
  return ro::sim::GradeEnchantAttempt{
      .stake = static_cast<std::uint32_t>(std::max(state.stake, 0)),
      .blessing_steps = static_cast<std::uint32_t>(std::max(state.blessing_steps, 0))};
}

}  // namespace

void GradeEnchantStage::StartSession() {
  grade_.CloseSilently();
  result_read_ = grade_.result_seq();
  notice_.clear();
  notice_good_ = false;
  hovered_ = {};
  described_.reset();
  view_ = nullptr;
}

std::optional<TradeItemFocus> GradeEnchantStage::TakeDescribed() noexcept {
  return std::exchange(described_, std::nullopt);
}

void GradeEnchantStage::Enchant(const ro::ui::GradeEnchantWindowState& state,
                                const ro::ui::GradeEnchantWindowAction& answer,
                                const TradeContext& context) {
  // 🔴 CE QUI PART EST CE QUE LE JOUEUR A CONFIRMÉ : si le choix ou la proposition
  // ont changé depuis, rien ne part.
  if (answer.choice_seq != grade_.choice_seq()) {
    return;
  }
  notice_.clear();
  // ⚠ REJUGÉ ICI : la fenêtre a montré un verdict d'une image de retard.
  if (auto sent = ro::net::RequestGradeEnchant(context.link, grade_, AttemptOf(state),
                                               context.world.inventory(), PurseOf(context));
      !sent) {
    ro::core::LogError("grade : {}", ro::core::Describe(sent.error()));
  }
}

void GradeEnchantStage::Follow(ro::ui::GradeEnchantWindowState& state,
                               const ro::ui::GradeEnchantWindowAction& answer,
                               const TradeContext& context) {
  const ro::sim::Inventory& bag = context.world.inventory();
  hovered_ = {};
  if (answer.hovered_slot != 0) {
    if (const ro::sim::InventoryItem* item = bag.Find(answer.hovered_slot); item != nullptr) {
      hovered_ = TradeItemFocus{.item_id = item->item_id, .bag_slot = answer.hovered_slot};
    }
  } else if (answer.hovered_id != 0) {
    hovered_ = TradeItemFocus{.item_id = answer.hovered_id};
  }
  if (answer.describe_slot != 0) {
    if (const ro::sim::InventoryItem* item = bag.Find(answer.describe_slot); item != nullptr) {
      described_ = TradeItemFocus{.item_id = item->item_id,
                                  .bag_slot = answer.describe_slot,
                                  .gesture = answer.item_gesture};
    }
  } else if (answer.describe_id != 0) {
    described_ = TradeItemFocus{.item_id = answer.describe_id, .gesture = answer.item_gesture};
  }
  if (!grade_.open()) {
    return;
  }
  // 🔴 UNE FENÊTRE BAISSÉE EST UNE FERMETURE, quel que soit le geste. ⚠ MAIS
  // SEULEMENT UNE FENÊTRE QUI MONTRAIT DÉJÀ CETTE OUVERTURE.
  const bool shown = state.opening_seen == grade_.opening_seq();
  if (answer.close || (!state.open && shown)) {
    grade_.Close();
    return;
  }
  if (!shown) {
    return;
  }
  if (answer.choose_slot != 0) {
    notice_.clear();
    static_cast<void>(grade_.Choose(answer.choose_slot));
  }
  if (answer.enchant) {
    Enchant(state, answer, context);
  }
}

void GradeEnchantStage::ReadResult() {
  if (grade_.result_seq() == result_read_) {
    return;
  }
  result_read_ = grade_.result_seq();
  notice_ = GradeEnchantResultText(grade_.result());
  notice_good_ = grade_.result() == ro::sim::GradeEnchantResult::kSucceeded;
}

void GradeEnchantStage::Advance(ro::ui::GradeEnchantWindowState& state,
                                const ro::ui::GradeEnchantWindowAction& answer,
                                const TradeContext& context) {
  Follow(state, answer, context);
  ReadResult();
  // LA PIÈCE CHOISIE A QUITTÉ LE SAC — détruite, vendue, rangée : le choix tombe.
  // ⚠ UNE PIÈCE QUE LE SERVEUR N'ADMET PLUS — passée au grade A — ne se redemande
  // pas : il ne répondrait pas, et la fenêtre attendrait pour rien.
  if (const std::optional<std::uint16_t> chosen = grade_.chosen_slot();
      chosen.has_value() && !grade_.waiting()) {
    const ro::sim::InventoryItem* item = context.world.inventory().Find(*chosen);
    if (item == nullptr) {
      grade_.Forget();
    } else if (ro::sim::JudgeGradePiece(*item) != ro::sim::GradeEnchantRefusal::kNone) {
      static_cast<void>(grade_.TakeSelection());
    }
  }
  ro::net::AskGradeEnchantOffer(context.link, grade_);
  ro::net::SayGradeEnchantFarewell(context.link, grade_);

  view_ = nullptr;
  if (!grade_.open()) {
    state.open = false;
    state.asking = false;
    return;
  }
  if (grade_.opening_seq() != state.opening_seen) {
    ro::ui::OpenGradeEnchantOn(state, grade_.opening_seq());
    notice_.clear();
    notice_good_ = false;
  }
  const GradeEnchantLooks looks{
      .name_of_held =
          [&context](const ro::sim::InventoryItem& item) {
            return context.items.NameOf(item, context.creators);
          },
      .name_of_id = [&context](std::uint32_t item_id) { return context.items.NameOf(item_id); },
      .icon_of = [&context](std::uint32_t item_id) { return context.items.IconOf(item_id); }};
  view_ = &composer_.Compose(GradeEnchantScene{.grade = grade_,
                                               .inventory = context.world.inventory(),
                                               .looks = looks,
                                               .attempt = AttemptOf(state),
                                               .zeny = PurseOf(context),
                                               .notice = notice_,
                                               .notice_good = notice_good_});
}

}  // namespace moonclient
