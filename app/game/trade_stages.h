#pragma once

// LES COMMERCES, DANS LA BOUCLE DE LA PARTIE : un seul point d'entrée pour les
// trames qu'ils lisent, pour l'image où ils répondent, et pour ce qu'ils
// montrent au survol.
//
// ⚠ GROUPÉS ICI plutôt qu'appelés un à un depuis la boucle : chaque commerce
// s'y ajoute sans que la boucle change, et l'ORDRE dans lequel ils lisent une
// trame partagée ne s'écrit qu'à un endroit.

#include <cstdint>
#include <optional>
#include <utility>

#include "ro/net/world_update.h"

#include "game/barter_stage.h"
#include "game/buying_store_stage.h"
#include "game/deal_stage.h"
#include "game/market_stage.h"
#include "game/npc_shop_stage.h"
#include "game/store_search_stage.h"
#include "game/trade_items.h"
#include "game/trade_windows.h"
#include "game/vending_stage.h"
#include "world/signboard_pick.h"

namespace moonclient {

class TradeStages {
 public:
  // Ce que les commerces lisent ailleurs : les noms, les icônes, les mots.
  // Tous facultatifs.
  void See(const ro::vfs::ItemTable* items, ro::render::ItemIcons* icons,
           const ro::vfs::ItemWords* words) noexcept;

  // L'atelier des noms et des icônes, prêté à qui montre un objet hors
  // commerce : les récompenses d'un champ de bataille.
  [[nodiscard]] const TradeItems& items() const noexcept { return items_; }

  // Les réglages de commande du serveur, lus à chaque image. ⚠ LEUR ADRESSE NE
  // BOUGE PAS d'une partie à l'autre : on la donne une fois.
  void SeeCommands(const ro::sim::CommandSettings* commands) noexcept { commands_ = commands; }

  // Les propriétés d'item du serveur — poids, drapeau de buying store —, qui
  // laissent un commerce refuser avant lui. Même règle que les réglages : une
  // adresse donnée une fois.
  // Les mots interdits de l'installation, qu'un titre d'échoppe éprouve.
  void SeeBannedWords(const ro::sim::BannedWords* banned) noexcept { banned_ = banned; }
  // Les images d'interface de l'installation. Même règle : une adresse donnée
  // une fois.
  void SeeGameImages(ro::render::GameImages* images) noexcept { images_ = images; }
  void SeeItemProperties(const ro::vfs::ItemProperties* properties) noexcept {
    item_properties_ = properties;
  }

  // Une partie commence : rien de la précédente ne survit.
  void StartSession();

  // Une trame reçue, lue par chaque commerce. Comme les autres traducteurs :
  // `kUnhandled` si aucun ne la prend.
  //
  // ⚠ LE TROC AVANT LA BOUTIQUE DE PNJ : le résultat d'achat est le même paquet
  // pour les deux, et chacun ne le prend que s'il l'attend.
  //
  // ⚠ LE SAC EST CELUI DU MONDE : l'accusé d'un dépôt dans un échange l'ampute.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame,
                                                 ro::sim::Inventory& bag);

  // L'IMAGE : les réponses des fenêtres, puis les vues de la prochaine.
  // `in_chat_room` : le joueur est dans un salon de discussion.
  void Advance(TradeWindows& windows, ro::net::Connection& link, ro::sim::World& world,
               const ro::vfs::ItemWords& words, const CreatorNames& creators, bool sell_locked,
               bool in_chat_room, std::uint32_t now_ms);

  // UNE ÉCHOPPE SE PRÉPARE-T-ELLE, de vente ou d'achat ? Les transferts entre
  // sac, charrette et coffre sont alors gelés : la préparation lit ce qu'ils
  // contiennent, et le serveur refuserait de toute façon.
  [[nodiscard]] bool PreparingStore() const noexcept;

  // L'objet qu'une fenêtre de commerce survolait à l'image précédente, et
  // celui qu'on veut décrire — une fois.
  [[nodiscard]] TradeItemFocus hovered() const noexcept;
  [[nodiscard]] std::optional<TradeItemFocus> TakeDescribed() noexcept;

  // UN COMMERCE RETIENT-IL LE MONDE ? Tant qu'une de ses fenêtres est ouverte,
  // la souris ne marche ni n'attaque, comme pendant un dialogue : c'est le
  // client d'origine, et c'est ce qui évite qu'un clic à côté d'une ligne
  // envoie le personnage au loin, boutique ouverte.
  [[nodiscard]] bool HoldsWorld() const noexcept;

  [[nodiscard]] NpcShopStage& npc_shop() noexcept { return npc_shop_; }
  [[nodiscard]] BarterStage& barter() noexcept { return barter_; }
  [[nodiscard]] BuyingStoreStage& buying_store() noexcept { return buying_store_; }
  [[nodiscard]] VendingStage& vending() noexcept { return vending_; }
  [[nodiscard]] DealStage& deal() noexcept { return deal_; }
  [[nodiscard]] const DealStage& deal() const noexcept { return deal_; }
  [[nodiscard]] StoreSearchStage& store_search() noexcept { return store_search_; }
  [[nodiscard]] const StoreSearchStage& store_search() const noexcept { return store_search_; }
  // LE MARCHÉ : son intention unique d'ouverture, et « Y aller ».
  [[nodiscard]] MarketStage& market() noexcept { return market_; }

  // LES ENSEIGNES D'ÉCHOPPE PEINTES À L'IMAGE PRÉCÉDENTE, que la peinture
  // remplit et que le clic lit. Voir `SignboardHit`.
  [[nodiscard]] std::vector<SignboardHit>& signboards() noexcept { return signboards_; }
  [[nodiscard]] std::optional<SignboardHit> SignboardAt(glm::vec2 point_px) const;
  // Une enseigne, salon compris, couvre-t-elle ce point ?
  [[nodiscard]] bool SignboardCovers(glm::vec2 point_px) const {
    return TopSignboardAt(signboards_, point_px).has_value();
  }

  // « MONTRE-MOI CETTE ÉCHOPPE » — le clic sur une enseigne ou sur son
  // marchand. La demande part à l'image suivante, là où le fil est en main ;
  // c'est la RÉPONSE qui ouvre la fenêtre, celle de la vente ou de l'achat.
  void VisitStore(std::uint32_t owner_id, ro::sim::SignboardKind kind) noexcept;

  // « REFERME CE QUE JE VISITE » — le clic droit sur une enseigne, sous
  // `/shopping`. À l'image suivante, l'échoppe d'un vendeur et celle d'un
  // acheteur se ferment, et le panier se vide.
  void CloseVisits() noexcept { close_visits_ = true; }

  // Vrai une fois après une telle fermeture : la description d'item ouverte
  // part avec elle, et c'est la partie qui la tient.
  [[nodiscard]] bool TakeClosedVisits() noexcept { return std::exchange(visits_closed_, false); }

 private:
  // PAS D'ÉCHOPPE EN SALON : une préparation qui arrive pendant qu'on est dans
  // un salon de discussion est refusée — la boîte du jeu, puis l'abandon dit au
  // serveur. Garde de politesse : le serveur refuse déjà toute compétence en
  // salon.
  void RefuseStoresInChatRoom(TradeWindows& windows, const TradeContext& context);

  TradeItems items_;
  const ro::sim::CommandSettings* commands_ = nullptr;
  const ro::vfs::ItemProperties* item_properties_ = nullptr;
  const ro::sim::BannedWords* banned_ = nullptr;
  ro::render::GameImages* images_ = nullptr;
  NpcShopStage npc_shop_;
  BarterStage barter_;
  BuyingStoreStage buying_store_;
  VendingStage vending_;
  DealStage deal_;
  StoreSearchStage store_search_;
  MarketStage market_;
  std::vector<SignboardHit> signboards_;
  // L'échoppe dont on a demandé à voir la liste, pas encore envoyée.
  std::optional<SignboardHit> pending_visit_;
  // L'instant de la dernière demande partie, s'il y en a eu une.
  std::optional<std::uint32_t> last_visit_ms_;
  bool close_visits_ = false;
  bool visits_closed_ = false;
};

}  // namespace moonclient
