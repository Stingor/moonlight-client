#pragma once

#include <cstdint>
#include <span>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/inventory_window.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA CHARRETTE : LA SŒUR DU SAC.
//
// ⚠ MÊME GRILLE, MÊMES CASES, MÊME BANDE D'ONGLETS — et c'est le client qui le
// dit, pas une commodité de notre part : les deux fenêtres partagent son cadre,
// ses images et son modèle d'objet. Ce qui les sépare tient en trois points, et
// ils sont tous ici.
//
//   1. SIX ONGLETS et non sept : on ne range pas de favoris dans une charrette.
//   2. UN PIED SANS BOURSE : la charrette porte un poids et un compte, rien de
//      plus. La bourse est au personnage, pas à ce qu'il pousse.
//   3. UN SEUL VERBE : en sortir un objet. On n'utilise pas, on ne porte pas,
//      on ne jette pas depuis une charrette.
//
// 🔴 ET SANS CHARRETTE, ELLE NE S'OUVRE PAS DU TOUT. Ni grille vide, ni phrase
// d'excuse : la demande est avalée, et le refus se dit dans la CONVERSATION, là
// où le joueur lit déjà tous les autres. Une fenêtre qui s'ouvre pour annoncer
// qu'elle n'a rien à montrer fait faire deux gestes — l'ouvrir, la refermer —
// pour une information d'une ligne.
//
// ⚠ C'EST DONC À L'APPELANT DE TRANCHER, avant d'ouvrir : cette fenêtre ne sait
// pas dire non.

// Ce que le pied annonce.
//
// ⚠ LE POIDS EST EN DIXIÈMES, comme celui du personnage : le serveur multiplie
// même son plafond par dix en lisant sa configuration. Voir `kWeightTenths`.
struct CartTally {
  std::int64_t weight = 0;
  std::int64_t max_weight = 0;
  int item_count = 0;
  // ⚠ ZÉRO TANT QUE LE SERVEUR N'A RIEN DIT, et c'est ce qui distingue « pas de
  // charrette » de « charrette vide » : les deux ont zéro objet, une seule a
  // une capacité.
  int capacity = 0;

  [[nodiscard]] bool exists() const noexcept { return capacity > 0; }
};

// CE QUE LA FENÊTRE SAIT DU MONDE ET DE SES SŒURS, posé par l'appelant à chaque
// image. ⚠ ELLE NE L'ÉCRIT JAMAIS, et c'est pour cela qu'il vit hors de son
// état : `ui` ne possède aucun état de jeu.
struct CartWindowView {
  // 🔴 UN STORAGE EST-IL OUVERT ? Le serveur refuse alors en silence de rendre un
  // objet au sac (`pc_cant_act2` compte `storage_flag`) — mais il accepte de le
  // ranger dans le storage, qui ne passe pas par là.
  bool storage_open = false;
  // UNE ÉCHOPPE SE PRÉPARE : la charrette ne rend plus rien, ni au sac ni au
  // storage.
  bool transfers_frozen = false;
  // ⚠ L'ÉTAT DU SAC, pour que son raccourci dise ce que le clic VA faire.
  bool inventory_open = false;
  // « Voir dans l'album » au menu d'une carte. ⚠ DES VUES, pour l'image.
  CardAlbumLinkFacts album_links;
};

// Ce que la fenêtre garde d'une image à l'autre.
struct CartWindowState {
  bool open = false;
  // L'ÉPINGLE DE LA BARRE DE TITRE : Échap ne referme plus cette fenêtre-là.
  // ⚠ RETENUE D'UNE SESSION À L'AUTRE, à côté de sa place, par `RememberWindow`.
  bool pinned = false;
  ItemTab tab = ItemTab::kAll;
  char filter[64] = {};
  bool filter_shown = false;
  // Les onglets debout en colonne plutôt que couchés en rangée. Voir
  // `InventoryWindowState`, qui porte le même réglage et la même raison.
  bool tabs_vertical = true;
  bool size_locked = false;
  // Le chrome de la fenêtre, mesuré à l'image précédente : le calage sur les
  // cases en a besoin.
  ImVec2 chrome{};
  // La question en cours, quand une pile ne sort pas entière.
  ItemQuantityAsk ask;
  // L'APERÇU DE DESCRIPTION REMPLACE-T-IL L'INFOBULLE DE NOM ?
  //
  // ⚠ IL LA REMPLACE, il ne s'y ajoute pas : deux infobulles pour le même objet
  // se recouvriraient. C'est un réglage, parce qu'un pavé de description au
  // moindre passage de souris gêne qui range son sac.
  bool hover_preview = false;

  // LE RACCOURCI DE BARRE DE TITRE VERS LE STORAGE. ⚠ OPT-IN, et propre
  // à chaque fenêtre : c'est un ajout à la barre du client,
  // pas une pièce de son dessin.
  //
  // ⚠ UN SEUL DRAPEAU POUR LES DEUX SENS du geste — ouvrir, refermer : c'est
  // l'état de la session qui dit lequel, et le dédoubler ferait deux vérités à
  // tenir d'accord.
  bool storage_button = false;
  // 🔴 VERS OÙ LA QUESTION ENVERRA. Deux destinations la partagent, et sans ce
  // souvenir la réponse irait toujours à la première.
  bool ask_to_storage = false;
};

// LES DEUX VERBES DE LA CHARRETTE, ET C'EST LE STORAGE QUI LES ARBITRE.
//
// 🔴 TANT QU'IL EST OUVERT, le serveur refuse en silence de rendre un objet au
// sac — `pc_cant_act2` compte `storage_flag` — et n'accepte que le rangement
// vers le storage. Fermé, c'est l'inverse : il n'y a pas de storage où ranger.
//
// ⚠ LE PREMIER NE DISPARAÎT PAS POUR AUTANT : il se grise, sous son propre nom,
// avec l'explication en infobulle. C'est une règle qu'on ne peut pas voir
// autrement, et la cacher la rendrait deux fois invisible.
enum class CartVerb : std::uint8_t {
  kNothing,
  kToInventory,
  kToStorage,
};

struct CartWindowAction {
  // L'IDENTIFIANT DE L'OBJET SURVOLÉ, zéro si aucun.
  //
  // ⚠ L'IDENTIFIANT ET NON LA PLACE : l'aperçu montre une DESCRIPTION, qui ne
  // dépend pas du contenant. C'est aussi ce qui permet à l'étage de la bâtir
  // sans savoir de quelle fenêtre le survol venait.
  std::uint32_t hovered_id = 0;

  CartVerb verb = CartVerb::kNothing;
  // Le raccourci de barre de titre vers le storage a été cliqué.
  // CE QU'UNE AUTRE FENÊTRE VIENT DE LÂCHER ICI. ⚠ RENDU PAR CELLE QUI REÇOIT :
  // c'est elle qui a vu le lâcher, et `app` sait quel paquet les deux bouts
  // appellent. Place nulle = rien.
  ItemTransferAsk received;

  // L'OBJET DONT IL VEUT LA DESCRIPTION, zéro si aucun.
  //
  // ⚠ UN IDENTIFIANT, PAS UN VERBE. Décrire n'envoie rien au serveur : c'est une
  // fenêtre qu'on ouvre, et elle survit à l'objet qui l'a ouverte. La PLACE
  // l'accompagne pour décorer le nom — raffinement, cartes — tant que l'objet
  // est là.
  std::uint32_t describe_id = 0;
  std::uint16_t describe_slot = 0;
  // « Voir dans l'album » : la carte dont il veut la pochette ; zéro sinon.
  std::uint32_t see_in_album_id = 0;

  // LA PLACE DONT IL VEUT POSER LE LIEN DANS LA CONVERSATION, ou zéro.
  //
  // ⚠ UNE PLACE ET NON UN IDENTIFIANT : un lien porte l'objet DE QUELQU'UN —
  // raffinement, cartes, grade —, et il n'y a que la place pour les retrouver.
  std::uint16_t link_slot = 0;

  bool toggle_storage = false;
  bool toggle_inventory = false;
  // La puce de la barre de titre a été cliquée. Voir `InventoryWindowAction`.
  bool settings_asked = false;
  std::uint16_t slot = 0;
  // Combien on en sort. ⚠ UN DOUBLE-CLIC SORT LA PILE ENTIÈRE — c'est le geste
  // du client — là où le menu ouvre la question.
  int amount = 1;
  std::uint16_t hovered_slot = 0;
};

inline constexpr const char* kCartWindowTitle = core::TrText("Cart");
inline constexpr const char* kCartWindowId = "cart";

// Les six onglets de la charrette, dans l'ordre du client.
[[nodiscard]] std::span<const ItemTab> CartTabs();

[[nodiscard]] CartWindowAction DrawCartWindow(CartWindowState& state, const CartWindowView& view,
                                              std::span<const InventoryCellView> cells,
                                              CartTally tally, const SkinArt& art);

}  // namespace ro::ui
