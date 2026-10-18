#pragma once

// L'ÉCHOPPE DE VENTE, DANS LA BOUCLE DE LA PARTIE : la préparation qu'ouvre la
// compétence, mon échoppe et son journal, et l'échoppe d'un autre que je
// visite — ce que leurs fenêtres montrent, et ce que leurs gestes deviennent
// sur le fil.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/vending.h"

#include "game/npc_shop_stage.h"
#include "game/trade_items.h"
#include "game/vending_memory.h"
#include "game/vending_windows.h"

namespace moonclient {

// ⚠ LE SERVEUR PEUT SE TAIRE À UNE DEMANDE D'OUVERTURE : sur une carte ou une
// case qui l'interdit, il n'envoie qu'un message, et la préparation reste
// ouverte chez lui. Aucun paquet ne dit alors que la demande est morte ; passé
// ce délai sans réponse, on la tient pour telle, et le bouton revient.
inline constexpr std::uint32_t kVendingOpeningSilenceMs = 3000;

class VendingStage {
 public:
  [[nodiscard]] ro::sim::Vending& vending() noexcept { return vending_; }
  [[nodiscard]] const ro::sim::Vending& vending() const noexcept { return vending_; }

  // Où le client retient les échoppes du joueur. Lu une fois.
  void SeeSaveRoot(std::filesystem::path client_root);

  // Une partie commence : rien de la précédente ne survit, que le journal des
  // réglages sur le disque.
  void StartSession();

  // L'IMAGE : les réponses des fenêtres, les adieux dus au serveur, ce qui
  // arrive, puis les vues de la prochaine.
  void Advance(VendingWindows& windows, const TradeContext& context);

  // UNE DE SES FENÊTRES RETIENT-ELLE LE MONDE ? La préparation, mon échoppe,
  // une visite : la souris ne marche ni n'attaque.
  [[nodiscard]] bool HoldsWorld() const noexcept;

  [[nodiscard]] TradeItemFocus hovered() const noexcept { return hovered_; }
  [[nodiscard]] std::optional<TradeItemFocus> TakeDescribed() noexcept;

 private:
  // ── MON ÉCHOPPE (vending_stage.cpp) ──────────────────────────────────────
  void FollowSetup(VendingWindows& windows, const TradeContext& context);
  void FollowShop(VendingWindows& windows, const TradeContext& context);
  void FollowLog(VendingWindows& windows);
  void Resume(ro::ui::VendingSetupWindowState& state, const TradeContext& context);
  void OpenWindows(VendingWindows& windows);
  void SayOwnNews(VendingWindows& windows, const TradeContext& context);
  void RememberOpenedShop(const TradeContext& context);
  void SaveMemory() const;

  // ── LA VISITE (vending_visit_stage.cpp) ──────────────────────────────────
  void FollowVisit(VendingWindows& windows, const TradeContext& context);
  void SayVisitNews(VendingWindows& windows, const TradeContext& context);

  // ── LES VUES (vending_views.cpp) ─────────────────────────────────────────
  void ComposeSetup(const ro::ui::VendingSetupWindowState& state, const TradeContext& context);
  void ComposeShop(const TradeContext& context);
  void ComposeLog(const TradeContext& context);
  // ⚠ LE PANIER SE BORNE ICI à la liste qui vient d'arriver, avant son verdict.
  void ComposeVisit(ro::ui::VendingVisitWindowState& state, const TradeContext& context);

  // Les lignes que la préparation enverrait, et les rangs de celles dont le
  // prix ne se lit pas — vide compris — : elles partent à zéro, et le verdict
  // les retient.
  [[nodiscard]] static std::vector<ro::sim::VendingSetupLine> SetupLinesOf(
      const ro::ui::VendingSetupWindowState& state, std::vector<std::size_t>& unreadable);
  // Ce que le joueur possède, zéro tant que le serveur ne l'a pas dit.
  [[nodiscard]] static std::int64_t ZenyOf(const ro::sim::World& world);

  // `gesture` : ce que demande l'objet décrit, sans objet pour un survol.
  void Note(const ro::sim::InventoryItem* item, bool hovered, bool described,
            ro::ui::ChatLinkGesture gesture = ro::ui::ChatLinkGesture::kDescribe);

  ro::sim::Vending vending_;
  std::filesystem::path client_root_;
  VendingMemory memory_;
  TradeItemFocus hovered_;
  std::optional<TradeItemFocus> described_;

  // ── LES RANGS DÉJÀ DITS ─────────────────────────────────────────────────
  std::uint32_t opening_result_said_ = 0;
  std::uint32_t purchase_result_said_ = 0;
  std::uint32_t journal_said_ = 0;
  std::size_t sales_said_ = 0;
  std::uint32_t visit_list_seen_ = 0;
  std::uint32_t shop_remembered_ = 0;
  // L'échoppe était ouverte à l'image précédente : sa fermeture montre le
  // journal.
  bool shop_was_open_ = false;
  // L'échoppe vide a déjà été fermée : la fermeture automatique ne part
  // qu'une fois par ouverture.
  std::uint32_t sold_out_closed_ = 0;
  // L'instant où la demande d'ouverture est partie.
  std::uint32_t opening_sent_ms_ = 0;
  // L'ACHAT EN ROUTE : venait-il du panier, et quel était le rang des refus à
  // son départ ? Revenue sans refus neuf, la liste dit qu'il a été servi.
  bool purchase_in_flight_ = false;
  bool purchase_from_basket_ = false;
  std::uint32_t purchase_refusals_at_send_ = 0;
  // Le dernier refus d'achat, avec les mots du jeu, pour la fenêtre.
  std::string visit_outcome_;

  // ── CE QUI SE COMPOSE À CHAQUE IMAGE ──────────────────────────────────────
  // ⚠ LES VUES POINTENT DEDANS : elles se prennent une fois les vecteurs remplis.
  std::vector<std::string> stock_names_;
  std::vector<std::string> stock_refusals_;
  std::vector<ro::ui::VendingStockView> stock_;
  std::vector<std::string_view> setup_blockers_;
  std::vector<std::string> line_fault_texts_;
  std::vector<std::string_view> line_faults_;
  std::string fitted_title_;
  std::string resume_title_;
  ro::ui::VendingSetupView setup_view_;

  std::vector<std::string> shop_names_;
  std::vector<ro::ui::VendingShopLineView> shop_lines_;
  ro::ui::VendingShopView shop_view_;

  std::vector<std::string> log_names_;
  std::vector<std::string> log_times_;
  std::vector<ro::ui::VendingLogLineView> log_lines_;
  ro::ui::VendingLogView log_view_;

  std::vector<std::string> offer_names_;
  std::vector<ro::ui::VendingOfferView> offers_;
  std::vector<std::string_view> visit_blockers_;
  std::string vendor_name_;
  std::string visit_title_;
  ro::ui::VendingVisitView visit_view_;
};

}  // namespace moonclient
