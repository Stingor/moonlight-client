#pragma once

// LE TROC VU PAR LA FENÊTRE : ce que le modèle devient à l'écran, ce que
// chaque refus et chaque résultat disent, et jusqu'où un ajout reste possible.
//
// ⚠ À PART DE L'ÉTAGE, ET SANS FIL NI TEXTURE : les noms et les icônes arrivent
// par `BarterLooks`. C'est ce qui laisse tout ceci s'essayer sur un banc — le
// troc est éteint sur le serveur, et le banc est la seule preuve.

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/barter.h"
#include "ro/sim/inventory.h"
#include "ro/sim/player_status.h"
#include "ro/ui/barter_window.h"
#include "ro/vfs/item_words.h"
#include "ro/vfs/trade_words.h"

namespace moonclient {

// CE QUE LA CONVERSATION DIT D'UN RÉSULTAT (`0x00CA`), avec les mots du client
// d'origine — la table est celle de la boutique à zeny, qui partage le paquet.
struct BarterResultSaying {
  std::optional<ro::vfs::TradeWord> word;
  bool succeeded = false;
};

[[nodiscard]] BarterResultSaying SayingOfBarterResult(std::uint8_t code);

// La phrase à écrire : celle du jeu, ou la nôtre quand l'installation ne la
// porte pas — un échange qui a consommé des items ne passe pas sous silence.
[[nodiscard]] std::string BarterResultText(std::uint8_t code, const ro::vfs::ItemWords& words);

// CE QUE LE JOUEUR LIT QUAND UN PANIER EST REFUSÉ, avant le clic.
//
// Les six gardes que le client d'origine pose avant la mise au panier parlent
// avec SES mots ; le reste — que le serveur refuse en silence, ou avec un code
// après coup — avec les nôtres, qui nomment l'item en cause.
[[nodiscard]] std::string BarterRefusalText(ro::sim::BarterCartRefusal refusal,
                                            std::string_view item_name,
                                            const ro::vfs::ItemWords& words);

// COMBIEN DE CET ARTICLE PEUVENT ENCORE ALLER AU PANIER sans qu'il soit refusé :
// la plus grande quantité que `CheckBarterAddition` accepte. Zéro si la première
// est déjà refusée.
//
// ⚠ CHERCHÉE PAR DICHOTOMIE : chaque refus croît avec la quantité (stock,
// plafond, poids, devises, zeny), et la borne de départ — stock restant,
// plafond de ligne, ce que les devises tenues permettent — est au-dessus de la
// réponse. Deux essais le plus souvent, quinze au pire.
[[nodiscard]] std::uint32_t BarterMostAddable(const ro::sim::Barter& barter,
                                              const ro::sim::BarterItem& item,
                                              const ro::sim::Inventory& inventory,
                                              const ro::sim::PlayerStatus& status);

// LES NOMS ET LES ICÔNES, qu'`app` tire de l'installation. Un nom porte le
// raffinement qu'on lui passe (« +7 Knife ») ; zéro n'en écrit pas.
struct BarterLooks {
  std::function<std::string(std::uint32_t item_id, std::uint16_t refine_level)> name_of;
  std::function<ro::ui::SkinImage(std::uint32_t item_id)> icon_of;
};

// CE QUE LA VUE DIT DE L'ÉCRAN AUTOUR : le nom du PNJ, faute de quoi « Troc ».
struct BarterScene {
  const ro::sim::Barter& barter;
  const ro::sim::Inventory& inventory;
  const ro::sim::PlayerStatus& status;
  const ro::vfs::ItemWords& words;
  const BarterLooks& looks;
  std::string_view title;
};

// La vue d'une image, et ce vers quoi elle pointe.
//
// ⚠ LA VUE POINTE DANS LE COMPOSITEUR : elle vaut jusqu'au `Compose` suivant.
class BarterViewComposer {
 public:
  [[nodiscard]] const ro::ui::BarterView& Compose(const BarterScene& scene);

 private:
  void ComposeOffers(const BarterScene& scene);
  void ComposeCart(const BarterScene& scene);

  std::vector<std::string> names_;
  std::vector<ro::ui::BarterPriceView> prices_;
  std::vector<std::string> refusals_;
  std::vector<ro::ui::BarterOfferView> offers_;
  std::vector<ro::ui::BarterCartLineView> cart_;
  std::string title_;
  std::string list_title_;
  std::string purse_line_;
  std::string cart_weight_;
  std::string blocker_text_;
  std::vector<std::string_view> blockers_;
  ro::ui::BarterView view_;
};

}  // namespace moonclient
