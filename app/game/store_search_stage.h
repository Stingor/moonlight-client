#pragma once

// LA RECHERCHE D'ÉCHOPPES, DANS LA BOUCLE DE LA PARTIE : la fenêtre qu'ouvre le
// serveur, la saisie jugée avant l'envoi, les refus dits au pied ou dans une
// boîte, et l'enseigne qu'un clic allume. Voir `ro/sim/store_search.h`.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/store_search.h"
#include "ro/ui/store_search_window.h"
#include "ro/ui/trade_widgets.h"
#include "ro/vfs/card_illustrations.h"

#include "game/npc_shop_stage.h"
#include "game/trade_items.h"

namespace moonclient {

// LES FENÊTRES DE LA RECHERCHE, telles que `TradeWindows` les porte.
struct StoreSearchWindows {
  ro::ui::StoreSearchWindowState state;
  // Nulle : rien ne se dessine. C'est le serveur qui ouvre.
  const ro::ui::StoreSearchView* view = nullptr;
  ro::ui::StoreSearchAction answer;
  // LE REFUS QUI SE DIT DANS UNE BOÎTE : la recherche trop tôt, et l'échoppe
  // cliquée qui n'est plus là.
  ro::ui::TradeNoticeState notice;
};

class StoreSearchStage {
 public:
  [[nodiscard]] ro::sim::StoreSearch& search() noexcept { return search_; }
  [[nodiscard]] const ro::sim::StoreSearch& search() const noexcept { return search_; }

  // Les illustrations de cartes, qui disent quels objets sont des cartes.
  // Facultatives. ⚠ LEUR ADRESSE NE BOUGE PAS d'une partie à l'autre.
  void SeeCards(const ro::vfs::CardIllustrations* illustrations) noexcept {
    card_illustrations_ = illustrations;
  }

  // Une partie commence : rien de la précédente ne survit.
  void StartSession();

  // L'IMAGE : la réponse de la fenêtre, ce que le serveur a dit, puis la vue de
  // la prochaine.
  void Advance(StoreSearchWindows& windows, const TradeContext& context);

  // LE MARCHAND DONT L'ENSEIGNE SE PEINT EN JAUNE PÂLE ; zéro sinon.
  [[nodiscard]] std::uint32_t spotlit_owner_aid() const noexcept {
    return search_.spotlit_owner_aid();
  }

  [[nodiscard]] TradeItemFocus hovered() const noexcept { return hovered_; }
  [[nodiscard]] std::optional<TradeItemFocus> TakeDescribed() noexcept;

 private:
  void Follow(StoreSearchWindows& windows, const TradeContext& context);
  void Search(const ro::ui::StoreSearchWindowState& state, const TradeContext& context);
  void SayNews(StoreSearchWindows& windows, const TradeContext& context);
  void Compose(const ro::ui::StoreSearchWindowState& state, const TradeContext& context);
  void Note(const ro::ui::StoreSearchWindowState& state, int row, bool hovered, bool described);
  [[nodiscard]] const ro::sim::StoreSearchResult* ResultAt(
      const ro::ui::StoreSearchWindowState& state, int row) const;

  ro::sim::StoreSearch search_;
  const ro::vfs::CardIllustrations* card_illustrations_ = nullptr;
  TradeItemFocus hovered_;
  std::optional<TradeItemFocus> described_;

  // ── LES RANGS DÉJÀ VUS ──────────────────────────────────────────────────
  std::uint32_t answer_seen_ = 0;
  std::uint32_t failure_said_ = 0;

  // LE TEXTE ROUGE DU PIED : le dernier refus du serveur, ou de la saisie.
  std::string status_;

  // ── CE QUI SE COMPOSE À CHAQUE IMAGE ──────────────────────────────────────
  // ⚠ LA VUE POINTE DEDANS : elle se prend une fois les vecteurs remplis.
  std::vector<std::string> names_;
  std::vector<ro::ui::StoreSearchRowView> rows_;
  std::string uses_left_;
  ro::ui::StoreSearchView view_;
};

}  // namespace moonclient
