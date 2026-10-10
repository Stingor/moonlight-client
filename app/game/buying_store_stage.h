#pragma once

// L'ÉCHOPPE D'ACHAT, DANS LA BOUCLE DE LA PARTIE : la mienne — de la
// préparation que le serveur ouvre jusqu'à l'historique qu'on relit à la fin —,
// et celle d'un autre, à qui je vends. Ce que les fenêtres en montrent, et ce
// que leurs gestes deviennent sur le fil.
//
// ⚠ DEUX FICHIERS : `buying_store_stage.cpp` tient mon échoppe,
// `buying_store_visit.cpp` la vente à un acheteur.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/sim/buying_store.h"

#include "game/buying_store_windows.h"
#include "game/npc_shop_stage.h"
#include "game/trade_items.h"

namespace moonclient {

class BuyingStoreStage {
 public:
  [[nodiscard]] ro::sim::BuyingStore& store() noexcept { return store_; }
  [[nodiscard]] const ro::sim::BuyingStore& store() const noexcept { return store_; }

  // Une partie commence : rien de la précédente ne survit.
  void StartSession();

  // L'IMAGE : les réponses des fenêtres, la fermeture due au serveur, les
  // ouvertures et les résultats qui arrivent, puis les vues de la prochaine.
  void Advance(BuyingStoreWindows& windows, const TradeContext& context);

  // UNE DE SES FENÊTRES RETIENT-ELLE LE MONDE ? La préparation, mon échoppe
  // ouverte, la vente à un acheteur : la souris ne marche ni n'attaque.
  [[nodiscard]] bool HoldsWorld() const noexcept;

  [[nodiscard]] TradeItemFocus hovered() const noexcept { return hovered_; }
  [[nodiscard]] std::optional<TradeItemFocus> TakeDescribed() noexcept;

 private:
  // ── MON ÉCHOPPE (buying_store_stage.cpp) ─────────────────────────────────
  void FollowSetup(ro::ui::BuyingStoreSetupState& state,
                   const ro::ui::BuyingStoreSetupAction& answer, const TradeContext& context);
  void FollowOwn(const ro::ui::BuyingStoreOwnState& state,
                 const ro::ui::BuyingStoreOwnAction& answer);
  void FollowLog(const ro::ui::BuyingStoreLogState& state,
                 const ro::ui::BuyingStoreLogAction& answer);
  void OpenWindows(BuyingStoreWindows& windows);
  void SayOwnOutcomes(const TradeContext& context);
  void ComposeSetup(const ro::ui::BuyingStoreSetupState& state, const TradeContext& context);
  void ComposeOwn(const TradeContext& context);
  void ComposeLog(const TradeContext& context);

  // ── LA VENTE À UN ACHETEUR (buying_store_visit.cpp) ──────────────────────
  void FollowSell(ro::ui::BuyingStoreSellState& state, const ro::ui::BuyingStoreSellAction& answer,
                  const TradeContext& context);
  void WatchBuyer(const TradeContext& context);
  void OpenSellWindow(ro::ui::BuyingStoreSellState& state);
  void SaySaleOutcomes(ro::ui::BuyingStoreSellState& state, const TradeContext& context);
  void AskRefresh(const TradeContext& context);
  void ComposeSell(const ro::ui::BuyingStoreSellState& state, const TradeContext& context);

  // `gesture` : ce que demande l'objet visé, sans objet pour un survol.
  void Note(std::uint32_t item_id, std::uint16_t slot, bool hovered,
            ro::ui::ChatLinkGesture gesture = ro::ui::ChatLinkGesture::kDescribe);

  // Ce que la conversation dit, en rouge ou dans sa couleur.
  static void Say(const TradeContext& context, std::string text, std::uint32_t color);

  ro::sim::BuyingStore store_;
  TradeItemFocus hovered_;
  std::optional<TradeItemFocus> described_;

  // ── LES RANGS DÉJÀ DITS ────────────────────────────────────────────────────
  std::uint32_t open_refusal_said_ = 0;
  std::uint32_t opening_said_ = 0;
  std::uint32_t end_said_ = 0;
  std::uint32_t sale_seq_said_ = 0;
  std::size_t sales_said_ = 0;
  std::uint32_t sale_refusal_said_ = 0;
  std::uint32_t visit_seen_ = 0;
  // L'échoppe était ouverte à l'image précédente : sa fermeture montre
  // l'historique.
  bool was_open_ = false;
  // L'historique était montré à l'image précédente : un interrupteur baissé
  // depuis — la croix, Échap — le referme.
  bool log_shown_ = false;
  // LA DERNIÈRE PRÉPARATION A ÉTÉ REFUSÉE : la suivante garde les lignes, pour
  // qu'on retire la fautive au lieu de tout retaper.
  bool draft_refused_ = false;
  // L'acheteur visité a été vu avec son enseigne : qu'elle disparaisse, et
  // l'échoppe est fermée.
  bool buyer_had_board_ = false;
  // Une vente est partie ; sa réponse videra la vente préparée.
  bool sale_sent_ = false;
  // La liste a été redemandée pour se rafraîchir : si la fenêtre s'est
  // refermée entre-temps, la réponse ne la rouvre pas.
  bool refresh_asked_ = false;
  // LES NOMS DES PILES VENDUES, composés au départ de la vente : le serveur
  // retire l'objet du sac avant que la ligne ne se dise.
  std::vector<std::pair<std::uint16_t, std::string>> sold_names_;

  // ── CE QUI SE COMPOSE À CHAQUE IMAGE ──────────────────────────────────────
  // ⚠ LES VUES POINTENT DEDANS : elles se prennent une fois les vecteurs remplis.
  std::string setup_title_;
  std::string title_on_wire_;
  std::vector<std::string> candidate_names_;
  std::vector<ro::ui::BuyingStoreCandidateView> candidates_;
  std::vector<std::string> blocker_texts_;
  std::vector<std::string_view> blockers_;
  std::vector<std::string> warning_texts_;
  std::vector<std::string_view> warnings_;
  std::vector<std::string> note_texts_;
  std::vector<std::string_view> notes_;
  ro::ui::BuyingStoreSetupView setup_view_;

  std::string own_title_;
  std::vector<std::string> own_names_;
  std::vector<ro::ui::BuyingStoreOwnLineView> own_lines_;
  ro::ui::BuyingStoreOwnView own_view_;

  std::vector<std::string> log_names_;
  std::vector<std::string> log_clocks_;
  std::vector<ro::ui::BuyingStoreLogLineView> log_lines_;
  ro::ui::BuyingStoreLogView log_view_;

  std::string buyer_;
  std::string store_title_;
  std::vector<std::string> wanted_names_;
  std::vector<ro::ui::BuyingStoreWantedView> wanted_;
  std::vector<std::string> offer_names_;
  std::vector<std::string> offer_bars_;
  std::vector<ro::ui::BuyingStoreOfferView> offers_;
  std::vector<std::string> sale_blocker_texts_;
  std::vector<std::string_view> sale_blockers_;
  std::string sale_outcome_;
  bool sale_outcome_done_ = false;
  ro::ui::BuyingStoreSellView sell_view_;
};

}  // namespace moonclient
