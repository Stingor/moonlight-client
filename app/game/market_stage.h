#pragma once

// LE MARCHÉ, DANS LA BOUCLE DE LA PARTIE : l'intention d'ouvrir — sur un item
// ou sans —, la saisie jugée et envoyée, la réponse composée en lignes triées et
// paginées, les refus dits, « Y aller » rendu à la navigation et « Voir »
// demandé au serveur. Voir `ro/sim/market.h` et l'ADR 0114.

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/sim/market.h"
#include "ro/ui/market_window.h"
#include "ro/vfs/random_option_names.h"

#include "game/market_route.h"
#include "game/npc_shop_stage.h"
#include "game/trade_items.h"

namespace moonclient {

// LES FENÊTRES DU MARCHÉ, telles que `TradeWindows` les porte.
struct MarketWindows {
  ro::ui::MarketWindowState state;
  // Nulle : rien ne se dessine.
  const ro::ui::MarketView* view = nullptr;
  ro::ui::MarketAction answer;
  // L'ICÔNE DU MENU, qui n'atteint que les fenêtres : sa bascule se dépose ici,
  // et l'étage la joue comme celle de la touche (`MarketStage::RequestToggle`).
  bool toggle_asked = false;
};

// Ce que la ligne dépliée d'une offre montre : ses cartes, son grade et ses
// options. `cards_named` nomme une carte par son identifiant ; `options`, nulle,
// laisse les options sans texte.
[[nodiscard]] std::vector<std::string> MarketDetailsOf(const ro::sim::MarketOffer& offer,
                                                       const TradeItems& items,
                                                       const ro::vfs::RandomOptionNames* options);

class MarketStage {
 public:
  [[nodiscard]] ro::sim::Market& market() noexcept { return market_; }
  [[nodiscard]] const ro::sim::Market& market() const noexcept { return market_; }

  // Les noms des options aléatoires. Facultatifs ; leur adresse ne bouge pas.
  void SeeRandomOptions(const ro::vfs::RandomOptionNames* names) noexcept { options_ = names; }

  // Une partie commence : rien de la précédente ne survit.
  void StartSession();

  // 🔴 L'INTENTION UNIQUE : « ouvre le Marché », sur cet item ou sans. Le menu
  // principal, la touche, `/marche`, le geste d'item, le menu du sac et la
  // description passent tous par elle. La fenêtre s'ouvre et la recherche part
  // à l'image suivante, où que soit le joueur.
  void AskOpen(std::optional<std::uint32_t> item_id = std::nullopt) {
    AskOpenOn(MarketOpening{.query = MarketOpeningQuery(item_id)});
  }
  // « COMPARER SUR LE MARCHÉ » : tous les items de cette échoppe, partout.
  void AskCompare(MarketCompare compare) {
    AskOpenOn(MarketOpening{.compare = std::move(compare)});
  }
  // La même, sur une ouverture toute faite : une saisie — celle qu'une
  // ouverture du serveur pose (catalogue, `@shopsearch`) — ou une comparaison.
  void AskOpenOn(MarketOpening opening) {
    asked_open_ = true;
    asked_opening_ = std::move(opening);
  }
  // La touche et l'icône du menu : ferme une fenêtre ouverte, ouvre sinon. Joué
  // à l'image suivante, là où l'étage voit la fenêtre.
  void RequestToggle() noexcept { toggle_asked_ = true; }

  // L'IMAGE : la réponse de la fenêtre, ce que le serveur a dit, puis la vue.
  void Advance(MarketWindows& windows, const TradeContext& context);

  // « Y aller », une fois.
  [[nodiscard]] std::optional<MarketDestination> TakeGoTo() noexcept;

  [[nodiscard]] TradeItemFocus hovered() const noexcept { return hovered_; }
  [[nodiscard]] std::optional<TradeItemFocus> TakeDescribed() noexcept;

 private:
  void OpenAsked(MarketWindows& windows, const TradeContext& context);
  void Follow(MarketWindows& windows, const TradeContext& context);
  void Search(MarketWindows& windows, const TradeContext& context);
  void SayNews();
  void Compose(const ro::ui::MarketWindowState& state, const TradeContext& context);
  void ComposeRows(std::span<const ro::sim::MarketOffer> offers, std::span<const std::size_t> ranks,
                   std::size_t first_name, const TradeContext& context,
                   std::vector<ro::ui::MarketRowView>& rows);
  void Note(int row, bool in_store, bool hovered, bool described,
            ro::ui::ChatLinkGesture gesture = ro::ui::ChatLinkGesture::kDescribe);
  [[nodiscard]] const ro::sim::MarketOffer* OfferAt(int row, bool in_store) const;

  ro::sim::Market market_;
  const ro::vfs::RandomOptionNames* options_ = nullptr;
  bool asked_open_ = false;
  bool toggle_asked_ = false;
  MarketOpening asked_opening_;
  // LA COMPARAISON EN COURS : « Actualiser » la redemande ; une recherche
  // neuve l'oublie.
  std::optional<MarketCompare> comparing_;
  std::optional<MarketDestination> go_to_;
  TradeItemFocus hovered_;
  std::optional<TradeItemFocus> described_;

  // ── LES RANGS DÉJÀ VUS ──────────────────────────────────────────────────
  std::uint32_t open_seen_ = 0;
  std::uint32_t refusal_seen_ = 0;
  std::uint32_t search_seen_ = 0;

  // Le texte rouge du pied, et celui de la fenêtre d'une échoppe.
  std::string status_;
  std::string store_status_;

  // ── CE QUI SE COMPOSE ─────────────────────────────────────────────────────
  // L'ordre des offres reçues, refait quand elles ou le tri changent.
  std::vector<std::size_t> order_;
  ro::sim::MarketSort sorted_by_;
  // Les rangs de la page montrée, et ceux du contenu d'une échoppe.
  std::vector<std::size_t> page_ranks_;
  std::vector<std::size_t> store_ranks_;
  // ⚠ LES VUES POINTENT DEDANS : elles se prennent une fois tout rempli.
  std::vector<std::string> names_;
  std::vector<std::string> places_;
  std::vector<std::string> prices_;
  std::vector<std::string> amounts_;
  std::vector<std::vector<std::string>> details_;
  std::vector<ro::ui::MarketRowView> rows_;
  std::vector<ro::ui::MarketRowView> store_rows_;
  std::string summary_;
  std::string store_title_;
  ro::ui::MarketView view_;
};

}  // namespace moonclient
