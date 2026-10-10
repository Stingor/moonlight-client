#pragma once

// LA BOUTIQUE DE VOTE, DANS LA BOUCLE DE LA PARTIE : la session que le serveur
// ouvre et ferme, ce que la fenêtre en montre, et ce que ses gestes deviennent
// sur le fil.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ro/net/connection.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/item_icons.h"
#include "ro/sim/cash_shop.h"
#include "ro/sim/hat_effect_items.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/cash_shop_window.h"
#include "ro/vfs/item_link.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "items/item_doll.h"

namespace moonclient {

class CashShopStage {
 public:
  // Ce que la boutique lit ailleurs : les noms, les images, la garde-robe du
  // pantin. Tous facultatifs — sans eux, « #id », une case vide, aucun aperçu.
  void See(const ro::vfs::ItemTable* items, ro::render::ItemIcons* icons,
           ro::render::ActorWardrobe* wardrobe);

  // UNE PARTIE COMMENCE : la session d'avant est finie, et le serveur renverra
  // le catalogue à qui le redemande.
  void StartSession();

  [[nodiscard]] ro::sim::CashShop& shop() noexcept { return shop_; }
  [[nodiscard]] ro::sim::HatEffectItems& hat_effects() noexcept { return hat_effects_; }
  // Le cadrage du pantin de l'aperçu, que son effet de chapeau suit.
  [[nodiscard]] ro::render::VignetteFraming doll_framing() const noexcept;

  // LE CATALOGUE, AVANT QU'ON EN AIT BESOIN. Le serveur ne l'envoie que sur
  // demande, et une fois par session : demandé à la première ouverture
  // seulement, rien ne se saurait d'un item avant que le joueur n'ait ouvert la
  // boutique — c'est-à-dire, en pratique, jamais. Il se demande donc dès que le
  // serveur écoute, une fois par partie.
  void RequestCatalogueOnce(ro::net::Connection& link);

  // Le bouton du menu : il DEMANDE, et la réponse du serveur ouvre — ou ferme.
  void Toggle(ro::net::Connection& link);

  // « CET ITEM EST-IL VENDU À LA BOUTIQUE ? » — son onglet et son prix.
  //
  // 🔴 UNE ABSENCE NE PROUVE RIEN tant que le catalogue n'est pas arrivé : qui
  // demande ne doit afficher que le POSITIF. Les onglets masqués sont ignorés —
  // on ne renvoie pas une adresse où l'on ne peut pas emmener le joueur.
  struct Offer {
    std::size_t tab = 0;
    std::int32_t price = 0;
  };
  [[nodiscard]] std::optional<Offer> Find(std::uint32_t item_id) const;

  // LE CATALOGUE VU DU MENU D'UN ITEM : les items des onglets montrés et leur
  // prix, triés par identifiant. Recomposé quand un paquet de catalogue arrive.
  [[nodiscard]] std::span<const ro::ui::VoteShopOffer> offers();

  // Ouvre la boutique sur l'item : son onglet, le filtre sur son nom, et une
  // unité au panier. Faux, et rien ne se passe, s'il n'y est pas.
  //
  // ⚠ FERMÉE, ELLE NE S'OUVRE PAS TOUT DE SUITE : c'est le serveur qui ouvre,
  // et l'item attend sa réponse.
  bool OpenWithItem(std::uint32_t item_id, ro::ui::CashShopWindowState& state,
                    ro::net::Connection& link);

  // L'IMAGE : la réponse de la fenêtre à l'image précédente, la fermeture due
  // au serveur, l'ouverture qui arrive, puis la vue de la prochaine.
  // `dressing` : le registre des couleurs de la carte, pour le pantin de
  // l'aperçu — voir `ItemDoll::Refresh`.
  void Advance(ro::ui::CashShopWindowState& state, const ro::ui::CashShopWindowAction& answer,
               ro::net::Connection& link, const ro::sim::World& world,
               const ro::vfs::ItemWords& words, ActorStyles& dressing, std::uint32_t now_ms);

  // LA CARTE DONT LE CLIC DROIT DEMANDE LA DESCRIPTION, une fois : l'item
  // comme un lien le porterait, avec son apparence et son emplacement.
  //
  // 🔴 CE SONT EUX QUI DÉVERROUILLENT L'APERÇU PORTÉ de la description : un
  // item qu'on ne possède pas n'en a pas d'autre source, et se voir porter un
  // costume avant de l'acheter est tout l'intérêt de la boutique.
  [[nodiscard]] std::optional<ro::vfs::ItemLink> TakeDescribed() noexcept {
    return std::exchange(described_, std::nullopt);
  }

  // LA CARTE SURVOLÉE, pour l'aperçu de sa description : le même lien que sa
  // description. Vide quand la souris n'est sur aucune carte.
  [[nodiscard]] const std::optional<ro::vfs::ItemLink>& hovered() const noexcept {
    return hovered_;
  }

  // LA CARTE DONT MAJ + CLIC DEMANDE LE LIEN DANS LA SAISIE, une fois.
  [[nodiscard]] std::optional<ro::vfs::ItemLink> TakeReposted() noexcept {
    return std::exchange(reposted_, std::nullopt);
  }

  // La vue que la fenêtre dessine, nulle quand la boutique est fermée.
  [[nodiscard]] const ro::ui::CashShopView* view() const noexcept {
    return shop_.open() ? &view_ : nullptr;
  }

 private:
  [[nodiscard]] std::string_view NameOf(std::uint32_t item_id);
  void NoteDrawn(const std::vector<std::uint32_t>& item_ids);
  [[nodiscard]] ro::vfs::ItemLink DescribedLinkOf(std::uint32_t item_id) const;
  void Close(ro::ui::CashShopWindowState& state);
  void ComposeCards();
  void ComposeView(const ro::ui::CashShopWindowState& state, const ro::vfs::ItemWords& words);

  const ro::vfs::ItemTable* items_ = nullptr;
  ro::render::ItemIcons* icons_ = nullptr;
  std::optional<ItemDoll> doll_;

  ro::sim::CashShop shop_;
  ro::sim::HatEffectItems hat_effects_;
  bool catalogue_requested_ = false;
  // L'item à montrer dès que le serveur aura ouvert, zéro pour aucun.
  std::uint32_t pending_item_ = 0;
  std::optional<ro::vfs::ItemLink> described_;
  std::optional<ro::vfs::ItemLink> hovered_;
  std::optional<ro::vfs::ItemLink> reposted_;
  // Le rang du dernier achat dont on a tiré les conséquences.
  std::uint32_t purchase_handled_ = 0;
  std::vector<ro::ui::VoteShopOffer> offers_;
  std::uint32_t offers_catalogue_seq_ = 0;

  // ── CE QUI SE COMPOSE QUAND ÇA CHANGE ─────────────────────────────────────
  std::unordered_map<std::uint32_t, std::string> names_;
  std::unordered_map<std::uint32_t, ro::ui::SkinImage> images_;
  std::uint32_t cards_catalogue_seq_ = 0;
  std::uint32_t cards_hat_revision_ = 0;
  std::size_t cards_images_ = 0;
  bool cards_composed_ = false;
  // LA GÉNÉRATION DES ICÔNES dont `images_` est tiré : un skin changé la fait
  // grandir, et les vignettes se relisent.
  std::uint64_t images_generation_ = 0;
  // ⚠ LES VUES POINTENT DEDANS : elles se prennent une fois les vecteurs
  // remplis, jamais avant.
  std::array<std::vector<ro::ui::CashShopCard>, ro::ui::kCashShopTabCount> cards_;
  ro::ui::CashShopView view_;
};

}  // namespace moonclient
