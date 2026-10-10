#pragma once

// L'ÉCHANGE ENTRE JOUEURS, DANS LA BOUCLE DE LA PARTIE : la proposition, faite
// ou reçue, la fenêtre des deux offres, et les garde-fous qui tiennent le reste
// du client pendant qu'un échange est ouvert.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/deal.h"
#include "ro/sim/deal.h"
#include "ro/sim/pending_action.h"
#include "ro/ui/deal_window.h"

#include "game/npc_shop_stage.h"
#include "game/trade_items.h"

namespace moonclient {

// Le temps qu'on se donne pour rejoindre un joueur trop loin avant de lui
// proposer l'échange. Passé ce délai, on y renonce, et on le dit.
inline constexpr std::uint32_t kDealApproachGiveUpMs = 10000;

// Pourquoi un dépôt ou la validation ne partiraient pas, déjà traduit ; vide
// quand rien ne les retient — ou quand la fenêtre le dit autrement.
[[nodiscard]] const char* DealAddFaultText(ro::sim::DealAddFault fault);
[[nodiscard]] const char* DealExecuteFaultText(ro::sim::DealExecuteFault fault);

// LES FENÊTRES DE L'ÉCHANGE, telles que `TradeWindows` les porte.
struct DealWindows {
  ro::ui::DealWindowState state;
  const ro::ui::DealView* view = nullptr;
  ro::ui::DealWindowAction answer;

  ro::ui::DealProposalState proposal;
  const ro::ui::DealProposalView* proposal_view = nullptr;
  ro::ui::DealProposalAction proposal_answer;
};

class DealStage {
 public:
  [[nodiscard]] ro::sim::Deal& deal() noexcept { return deal_; }
  [[nodiscard]] const ro::sim::Deal& deal() const noexcept { return deal_; }

  // L'horloge de la marche du personnage, celle de toutes ses approches. ⚠ SON
  // ADRESSE NE BOUGE PAS d'une partie à l'autre : on la donne une fois.
  void SeeApproachClock(ro::sim::ApproachWalkClock* clock) noexcept { approach_clock_ = clock; }

  // Une partie commence : rien de la précédente ne survit.
  void StartSession();

  // ARME LE DERNIER REMPART sur cette liaison. ⚠ À REFAIRE SUR CHAQUE LIAISON
  // NEUVE : un changement de serveur de zone en ouvre une autre.
  void Watch(ro::net::Connection& link);

  // Une trame reçue. ⚠ LE SAC EST CELUI DU MONDE : l'accusé d'un dépôt l'ampute.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame,
                                                 ro::sim::Inventory& bag);

  // « PROPOSER UN ÉCHANGE », depuis le menu d'un joueur. La proposition part à
  // l'image suivante ; trop loin, le personnage s'approche d'abord.
  void Propose(std::uint32_t account_id, std::string name, std::uint16_t base_level);

  // L'IMAGE : les réponses des fenêtres, les dépôts muets, les nouvelles à
  // dire, l'approche, puis les vues de la prochaine.
  void Advance(DealWindows& windows, const TradeContext& context);

  // L'ÉCHANGE RETIENT-IL LE MONDE ? Ouvert, la souris ne marche ni n'attaque.
  [[nodiscard]] bool HoldsWorld() const noexcept { return deal_.open(); }

  // LE DERNIER REMPART : le juge des trames émises que la liaison consulte.
  // Une trame refusée est retenue, et la raison se dit à l'image suivante.
  [[nodiscard]] bool PermitsFrame(std::span<const std::uint8_t> frame);

  // LE GESTE D'UN ITEM DU SAC — utiliser, équiper, ôter, jeter, ranger — est-il
  // RETENU ? Le client d'origine les refuse tous pendant un échange. Vrai :
  // retenu, et la raison se dira à l'image suivante.
  [[nodiscard]] bool HoldsBagGesture();

  [[nodiscard]] TradeItemFocus hovered() const noexcept { return hovered_; }
  [[nodiscard]] std::optional<TradeItemFocus> TakeDescribed() noexcept;

 private:
  struct Approach {
    std::uint32_t account_id = 0;
    std::string name;
    std::uint16_t base_level = 0;
    std::uint32_t since_ms = 0;
    // La case vers laquelle la dernière marche est partie.
    std::optional<ro::sim::CellPosition> walked_to;
    bool started = false;
  };

  void FollowProposal(DealWindows& windows, const TradeContext& context);
  void FollowWindow(DealWindows& windows, const TradeContext& context);
  void AdvanceApproach(const TradeContext& context);
  void SayNews(const TradeContext& context);
  void SayRefusedFrames(const TradeContext& context);
  void Compose(const TradeContext& context);
  void ComposeProposal(const TradeContext& context);
  void Note(const ro::ui::DealLineRef& line, bool hovered, bool described);

  ro::sim::Deal deal_;
  ro::sim::ApproachWalkClock* approach_clock_ = nullptr;
  std::optional<Approach> approach_;
  std::uint32_t news_said_ = 0;
  // Le nom du partenaire, gardé au-delà de la fermeture : les nouvelles qui la
  // disent le nomment encore.
  std::string partner_name_;
  // Le rang de ma dernière proposition, approche comprise : une neuve rouvre
  // sa boîte.
  std::uint32_t outgoing_seq_ = 0;
  // Les items des lignes montrées, pour les décrire.
  std::vector<ro::sim::InventoryItem> mine_items_;
  std::vector<ro::sim::InventoryItem> partner_items_;
  // Ce que le rempart a retenu depuis la dernière image.
  std::vector<ro::net::DealFrameRefusal> refused_frames_;
  // Le dernier refus de dépôt, pour la fenêtre.
  std::string last_refusal_;
  TradeItemFocus hovered_;
  std::optional<TradeItemFocus> described_;

  // ── CE QUI SE COMPOSE À CHAQUE IMAGE ─────────────────────────────────────
  // ⚠ LES VUES POINTENT DEDANS : elles se prennent une fois les vecteurs remplis.
  std::vector<std::string> mine_names_;
  std::vector<std::string> partner_names_;
  std::vector<std::string> bag_names_;
  std::vector<std::string> bag_refusals_;
  std::vector<ro::ui::DealLineView> mine_;
  std::vector<ro::ui::DealLineView> partner_;
  std::vector<ro::ui::DealBagView> bag_;
  std::vector<std::string_view> lock_blockers_;
  std::vector<std::string_view> execute_blockers_;
  std::string title_;
  ro::ui::DealView view_;
  std::string proposal_text_;
  ro::ui::DealProposalView proposal_view_;
  bool proposal_shown_ = false;
};

}  // namespace moonclient
