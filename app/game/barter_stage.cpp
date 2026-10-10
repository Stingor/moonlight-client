// Le troc, dans la boucle. Voir l'en-tête.

#include "game/barter_stage.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/barter.h"
#include "ro/net/npc_dialog.h"
#include "ro/sim/npc_dialog.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

// Les couleurs des lignes de conversation, celles du client : cyan pour ce qui
// a réussi, rouge pour ce qui a échoué.
constexpr std::uint32_t kSucceededColor = 0x00FFFF;
constexpr std::uint32_t kFailedColor = 0xFF0000;

// Le nom montré d'un PNJ : ce qui précède le dièse.
std::string_view ShownNpcName(std::string_view name) { return name.substr(0, name.find('#')); }

void Say(const TradeContext& context, const std::string& text, std::uint32_t color) {
  if (!text.empty()) {
    context.world.RecordChat(ro::sim::ChatKind::kServer, 0, text, context.now_ms, color);
  }
}

}  // namespace

void BarterStage::StartSession() {
  barter_.CloseSilently();
  result_said_ = barter_.result_seq();
  hovered_ = {};
  described_.reset();
  view_ = nullptr;
}

std::optional<TradeItemFocus> BarterStage::TakeDescribed() noexcept {
  return std::exchange(described_, std::nullopt);
}

void BarterStage::Refuse(const ro::sim::BarterCartCheck& check, const TradeContext& context) {
  Say(context, BarterRefusalText(check.refusal, context.items.NameOf(check.item_id), context.words),
      kFailedColor);
}

void BarterStage::Edit(const ro::ui::BarterCartEdit& edit, const TradeContext& context) {
  const ro::sim::Inventory& inventory = context.world.inventory();
  const ro::sim::PlayerStatus& status = context.world.player_status();
  const std::uint32_t index = edit.line.shop_index;
  switch (edit.kind) {
    case ro::ui::BarterCartEditKind::kNone:
      return;
    case ro::ui::BarterCartEditKind::kClear:
      barter_.ClearCart();
      return;
    case ro::ui::BarterCartEditKind::kAdd:
    case ro::ui::BarterCartEditKind::kSet:
      break;
  }
  if (edit.line.amount < 0) {
    return;
  }
  const auto wanted = static_cast<std::uint32_t>(edit.line.amount);
  const std::uint32_t carted = barter_.InCart(index);
  // CE QUI S'AJOUTE SE JUGE, comme le client d'origine le juge avant de mettre au
  // panier ; ce qui se retire passe toujours.
  const std::uint32_t added = edit.kind == ro::ui::BarterCartEditKind::kAdd ? wanted
                              : wanted > carted                             ? wanted - carted
                                                                            : 0;
  if (added > 0) {
    const ro::sim::BarterCartCheck check =
        ro::sim::CheckBarterAddition(barter_, index, added, inventory, status);
    if (!check.passed()) {
      Refuse(check, context);
      return;
    }
  }
  if (edit.kind == ro::ui::BarterCartEditKind::kAdd) {
    static_cast<void>(barter_.PutInCart(index, wanted));
  } else {
    static_cast<void>(barter_.SetCartAmount(index, wanted));
  }
}

void BarterStage::Exchange(const ro::ui::BarterWindowAction& answer, const TradeContext& context) {
  const ro::sim::Inventory& inventory = context.world.inventory();
  const ro::sim::PlayerStatus& status = context.world.player_status();
  if (answer.immediate) {
    // LE CTRL + CLIC ÉCHANGE CETTE LIGNE SEULE. Elle se juge seule, sur un troc
    // d'essai : le panier du joueur n'est touché que si elle part.
    if (answer.exchange.size() != 1 || answer.exchange.front().amount <= 0) {
      return;
    }
    const ro::ui::BarterLine& line = answer.exchange.front();
    ro::sim::Barter trial = barter_;
    trial.ClearCart();
    static_cast<void>(trial.PutInCart(line.shop_index, static_cast<std::uint32_t>(line.amount)));
    if (const ro::sim::BarterCartCheck check = ro::sim::CheckBarterCart(trial, inventory, status);
        !check.passed()) {
      Refuse(check, context);
      return;
    }
    const std::vector<ro::sim::BarterCartLine> kept(barter_.cart().begin(), barter_.cart().end());
    barter_.ClearCart();
    static_cast<void>(barter_.PutInCart(line.shop_index, static_cast<std::uint32_t>(line.amount)));
    if (auto sent = ro::net::RequestBarterPurchase(context.link, barter_); !sent) {
      ro::core::LogError("troc : {}", ro::core::Describe(sent.error()));
      barter_.ClearCart();
      for (const ro::sim::BarterCartLine& back : kept) {
        static_cast<void>(barter_.PutInCart(back.shop_index, back.amount));
      }
    }
    return;
  }
  // 🔴 LE PANIER QUI PART EST CELUI QUE LE JOUEUR A CONFIRMÉ : s'il a changé
  // depuis, rien ne part, et il le lit.
  if (answer.cart_seq != barter_.cart_seq()) {
    Say(context, ro::core::Tr("Le panier a changé avant l'échange : rien n'est parti."),
        kFailedColor);
    return;
  }
  // ⚠ REJUGÉ ICI : la fenêtre a montré un verdict d'une image de retard.
  if (const ro::sim::BarterCartCheck check = ro::sim::CheckBarterCart(barter_, inventory, status);
      !check.passed()) {
    Refuse(check, context);
    return;
  }
  if (auto sent = ro::net::RequestBarterPurchase(context.link, barter_); !sent) {
    ro::core::LogError("troc : {}", ro::core::Describe(sent.error()));
  }
}

void BarterStage::Follow(ro::ui::BarterWindowState& state, const ro::ui::BarterWindowAction& answer,
                         const TradeContext& context) {
  hovered_ = TradeItemFocus{.item_id = answer.hovered_id};
  if (answer.describe_id != 0) {
    described_ = TradeItemFocus{.item_id = answer.describe_id, .gesture = answer.item_gesture};
  }
  if (!barter_.open()) {
    return;
  }
  // 🔴 UNE FENÊTRE BAISSÉE EST UNE FERMETURE, quel que soit le geste — la
  // croix, « Fermer », Échap que la pile a pris. Elle se dit au serveur par le
  // congé, plus bas dans la même image.
  //
  // ⚠ MAIS SEULEMENT UNE FENÊTRE QUI MONTRAIT DÉJÀ CETTE OUVERTURE : à l'image
  // où le troc s'ouvre, la fenêtre est encore baissée.
  const bool shown = state.opening_seen == barter_.opening_seq();
  if (answer.close || (!state.open && shown)) {
    barter_.Close();
    return;
  }
  if (!shown) {
    return;
  }
  Edit(answer.edit, context);
  if (!answer.exchange.empty()) {
    Exchange(answer, context);
  }
}

void BarterStage::SayFarewells(const TradeContext& context) {
  // 🔴 À CHAQUE IMAGE, ET APRÈS L'ÉCHANGE : une fermeture arrivée avant l'achat
  // le ferait ignorer, et une fermeture oubliée interdirait au PNJ de rouvrir.
  if (auto said = ro::net::SayBarterFarewells(context.link, barter_); !said) {
    ro::core::LogError("troc : {}", ro::core::Describe(said.error()));
  }
}

void BarterStage::SayResult(const TradeContext& context) {
  if (barter_.result_seq() == 0 || barter_.result_seq() == result_said_) {
    return;
  }
  result_said_ = barter_.result_seq();
  const std::uint8_t code = barter_.result();
  Say(context, BarterResultText(code, context.words),
      SayingOfBarterResult(code).succeeded ? kSucceededColor : kFailedColor);
}

void BarterStage::Advance(ro::ui::BarterWindowState& state,
                          const ro::ui::BarterWindowAction& answer, const TradeContext& context) {
  const bool was_open = barter_.open();
  Follow(state, answer, context);
  SayFarewells(context);

  // 🔴 LE DIALOGUE QUI A OUVERT LE TROC PEUT RESTER À L'ÉCRAN : un script qui
  // écrit puis appelle `callshop` n'envoie jamais de « fermer ». Sa page muette
  // se referme avec le troc ; une page qui attend encore une réponse reste au
  // joueur.
  if (was_open && !barter_.open()) {
    if (const std::optional<ro::sim::NpcDialog>& dialog = context.world.npc_dialog();
        dialog.has_value() && (dialog->prompt == ro::sim::NpcPrompt::kNone ||
                               dialog->prompt == ro::sim::NpcPrompt::kClose)) {
      if (auto left = ro::net::LeaveNpcDialog(context.link, context.world); !left) {
        ro::core::LogError("troc : {}", ro::core::Describe(left.error()));
      }
    }
  }

  if (!barter_.open()) {
    state.open = false;
    state.pending.clear();
  } else if (barter_.opening_seq() != state.opening_seen) {
    ro::ui::OpenBarterOn(state, barter_.opening_seq());
  }

  SayResult(context);

  view_ = nullptr;
  if (!barter_.open()) {
    return;
  }
  const ro::sim::World& world = context.world;
  std::string title;
  if (world.npc_dialog().has_value()) {
    if (const ro::sim::Actor* npc = world.Find(world.npc_dialog()->npc_id); npc != nullptr) {
      title = std::string(ShownNpcName(npc->name));
    }
  }
  const BarterLooks looks{
      .name_of =
          [&context](std::uint32_t item_id, std::uint16_t refine_level) {
            if (refine_level == 0) {
              return context.items.NameOf(item_id);
            }
            ro::sim::InventoryItem shown;
            shown.item_id = item_id;
            shown.refine_level = static_cast<std::uint8_t>(
                std::min<std::uint16_t>(refine_level, std::numeric_limits<std::uint8_t>::max()));
            shown.identified = true;
            return context.items.NameOf(shown, context.creators);
          },
      .icon_of = [&context](std::uint32_t item_id) { return context.items.IconOf(item_id); }};
  view_ = &composer_.Compose(BarterScene{.barter = barter_,
                                         .inventory = world.inventory(),
                                         .status = world.player_status(),
                                         .words = context.words,
                                         .looks = looks,
                                         .title = title});
}

}  // namespace moonclient
