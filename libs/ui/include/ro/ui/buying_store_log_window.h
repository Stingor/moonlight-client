#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "ro/ui/skin_art.h"
#include "ro/ui/trade_widgets.h"

namespace ro::ui {

// ── L'HISTORIQUE DES ACHATS D'UNE ÉCHOPPE ───────────────────────────────────
//
// Chaque achat de l'échoppe : l'item, la quantité, le prix, le montant, et le
// total.
//
// Elle se montre QUAND L'ÉCHOPPE SE FERME, s'il y a eu des achats : pendant la
// vente, l'échoppe dit déjà ce qui reste voulu. La refermer oublie le journal.
//
// Chaque ligne porte aussi l'HEURE de l'achat (le client d'origine
// l'enregistre) et le VENDEUR, par son numéro de personnage — le serveur ne dit
// pas son nom.

inline constexpr const char* kBuyingStoreLogWindowName = "###buying_store_log";

struct BuyingStoreLogLineView {
  TradeItemView item;
  // « jj/mm - hh:mm:ss », en heure locale.
  std::string_view clock;
  std::uint32_t seller_char_id = 0;
  int amount = 0;
  // Ce que la ligne a coûté, en tout : le prix unitaire s'en déduit.
  std::int64_t zeny = 0;
};

struct BuyingStoreLogView {
  std::span<const BuyingStoreLogLineView> lines;
  std::int64_t total = 0;
};

struct BuyingStoreLogState {
  bool open = false;
};

struct BuyingStoreLogAction {
  // Fermer, la croix ou Échap. ⚠ AUSSI ÉCRITE DANS `state.open`.
  bool close = false;
  std::uint32_t hovered_id = 0;
  std::uint32_t describe_id = 0;
};

[[nodiscard]] BuyingStoreLogAction DrawBuyingStoreLogWindow(BuyingStoreLogState& state,
                                                            const BuyingStoreLogView& view,
                                                            const SkinArt& art);

}  // namespace ro::ui
