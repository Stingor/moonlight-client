#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/net/connection.h"
#include "ro/render/item_icons.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/inventory_window.h"
#include "ro/vfs/item_link.h"
#include "ro/vfs/item_name.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "items/creator_names.h"
#include "items/item_layout.h"
#include "items/storage_favorites.h"

namespace moonclient {

// LE COURRIER EN COURS D'ÉCRITURE, VU DU SAC : ce qu'il retient de chaque case
// — joint ou en route —, son juge de l'usage d'un item, et où part « Joindre
// au courrier ». Chacun est facultatif.
struct InventoryMailDesk {
  std::function<std::int32_t(std::uint16_t)> withheld;
  // Rend vrai quand il retient l'usage, et c'est lui qui le dit au joueur.
  std::function<bool()> holds_use;
  std::function<void(std::uint16_t, int)> attach;
};

// LE PET, VU DU SAC : quels items lui proposer, et le refus quand aucun n'est
// sorti. Chacun est facultatif ; sans `offers`, aucun item ne lui va.
struct InventoryPetDesk {
  std::function<bool(const ro::sim::InventoryItem&)> offers;
  // Rend vrai quand il refuse, et c'est lui qui le dit au joueur.
  std::function<bool(ro::sim::World&)> refuses;
};

// LE SAC, VU DE LA PARTIE.
//
// ⚠ IL CROISE QUATRE SOURCES QUE RIEN NE RELIE : le sac du serveur — des
// identifiants, des cartes, des raffinements —, la table de l'installation — les
// noms et les emplacements —, les mots du client — de quoi composer le nom —, et
// les icônes. Une case ne se dessine qu'une fois les quatre croisées, et c'est
// le travail de cet étage. `ui` ne connaît aucune des quatre.
//
// ⚠ LE RANGEMENT PAR ONGLET EST ICI, PAS DANS LA FENÊTRE. Il dépend du type que
// le serveur envoie, et `ui` ne sait pas ce qu'est un type d'objet.
class InventoryView {
 public:
  // LES NOMS DES FORGERONS, confiés une fois la description ouverte.
  void SeeCreators(CreatorNames creators) noexcept { creators_ = creators; }

  // L'ÉCHANGE ENTRE JOUEURS, dont l'ouverture retient tout geste d'item sauf le
  // favori — comme le client d'origine. Le juge rend vrai quand il retient le
  // geste, et c'est lui qui le dit au joueur.
  void SeeDealGuard(std::function<bool()> holds_items) {
    deal_holds_items_ = std::move(holds_items);
  }

  // LE COURRIER : le sac MONTRÉ retranche ce que l'écriture retient — celui du
  // monde n'y touche jamais —, l'usage d'un item se retient pendant
  // l'écriture, et « Joindre au courrier » lui confie la case.
  void SeeMailDesk(InventoryMailDesk desk) { mail_ = std::move(desk); }

  // LE PET : un accessoire qu'il peut porter s'équipe du même geste qu'une
  // pièce, et part vers lui.
  void SeePetDesk(InventoryPetDesk desk) { pet_ = std::move(desk); }

  InventoryView(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                ro::render::ItemIcons* icons);

  // Refait les cases de l'onglet demandé. À appeler une fois par image, avant le
  // dessin.
  //
  // ⚠ LES CASES NE SE REFONT QUE FENÊTRE OUVERTE : seule la fenêtre les lit, et
  // composer une centaine de noms par image pour personne serait du temps donné
  // à rien. Le pied et les deux drapeaux, qui ne coûtent rien, se relèvent
  // toujours : le coffre lit `cart_available` sac fermé, et un objet jeté
  // d'ailleurs que de la fenêtre passe encore par le verrou de jet.
  //
  // ⚠ LES NOMS COMPOSÉS SONT GARDÉS D'UNE IMAGE À L'AUTRE, et il le faut : la
  // fenêtre reçoit des vues qui POINTENT dessus. Les recomposer à chaque image
  // coûterait une allocation par objet et par image, pour un texte qui ne change
  // qu'au raffinement ou au sertissage.
  // ⚠ `layout` EST NUL QUAND LE PLACEMENT LIBRE EST ÉTEINT, et c'est l'étage qui
  // en décide : il connaît aussi le verrou de taille, sans lequel une case ne
  // veut rien dire. La vue, elle, se contente de ranger ce qu'on lui donne.
  void Refresh(bool open, const ro::sim::World& world, ro::ui::ItemTab tab, std::string_view filter,
               bool drop_locked, bool sorted, const ItemLayout* layout);

  // Le personnage pousse-t-il une charrette ? Relevé au rafraîchissement, pour
  // que la fenêtre sache si elle peut proposer d'y ranger.
  [[nodiscard]] bool cart_available() const noexcept { return cart_available_; }

  [[nodiscard]] std::span<const ro::ui::InventoryCellView> cells() const noexcept { return cells_; }
  [[nodiscard]] ro::ui::InventoryTally tally() const noexcept { return tally_; }

  // Fait ce que le joueur a demandé : utiliser, porter, retirer, jeter.
  //
  // ⚠ ÉMET ET S'ARRÊTE LÀ. Le sac ne bougera qu'à la réponse du serveur : agir
  // tout de suite mettrait l'affichage en avance sur le monde, et une demande
  // refusée — surcharge, objet lié — laisserait les deux en désaccord.
  //
  // ⚠ ET IL REVÉRIFIE LA PLACE. Entre le clic et l'émission, l'objet a pu
  // partir : une invite de quantité laisse le temps de vendre ce qu'on allait
  // jeter.
  //
  // ⚠ LE MONDE EST OUVERT EN ÉCRITURE POUR LE SEUL SERTISSAGE : la demande de
  // liste doit y noter sa carte, que la réponse ne rappellera pas.
  void Apply(const ro::ui::InventoryWindowAction& asked, ro::net::Connection& link,
             ro::sim::World& world);

 private:
  // Le verrou de jet, recopie des reglages a chaque rafraichissement : la vue
  // emet, c'est donc elle qui doit le connaitre.
  bool drop_locked_ = false;
  bool cart_available_ = false;
  std::function<bool()> deal_holds_items_;
  InventoryMailDesk mail_;
  InventoryPetDesk pet_;

  // L'item se propose-t-il au pet ?
  [[nodiscard]] bool OffersToPet(const ro::sim::InventoryItem& item) const {
    return pet_.offers && pet_.offers(item);
  }

  const ro::vfs::ItemTable* items_;
  const ro::vfs::ItemWords* words_;
  CreatorNames creators_;
  ro::render::ItemIcons* icons_;

  // Les noms, gardés le temps que la fenêtre les lise. Un par case affichée.
  std::vector<std::string> names_;
  std::vector<ro::ui::InventoryCellView> cells_;
  ro::ui::InventoryTally tally_;
};

// CE QU'ON DEMANDE POUR BÂTIR LES CASES D'UN CONTENANT.
//
// ⚠ GROUPÉ PLUTÔT QU'EN CINQ PARAMÈTRES VOISINS, et c'est la règle du dépôt :
// deux `bool` de suite dans une liste d'arguments s'échangent sans que rien ne
// le dise, et le défaut se lit « le tri ne marche que sur les favoris ».
struct ItemCellsAsk {
  ro::ui::ItemTab tab = ro::ui::ItemTab::kAll;
  std::string_view filter;
  // LE CONTENANT A-T-IL UN ONGLET DE FAVORIS ? Le sac oui, la charrette non.
  // ⚠ SANS CE DRAPEAU, un objet de charrette que le serveur a marqué favori —
  // le même octet sert aux trois — disparaîtrait de TOUS ses onglets, vers un
  // onglet qui n'existe pas.
  bool favorites_tab = false;
  // 🔴 LA FENÊTRE MONTRE-T-ELLE LES DIX ONGLETS DU COFFRE, ou les sept du sac ?
  //
  // Ce n'est pas un détail d'affichage : le MÊME onglet n'y range pas la même
  // chose. « Conso » prend le cash dans le sac, qui n'a pas d'onglet pour lui, et
  // le laisse au coffre, qui en a un. Sans ce drapeau, l'un des deux perdrait ses
  // objets de boutique — vers un onglet qui n'existe pas chez lui.
  bool fine_tabs = false;
  // ⚠ NE VAUT QUE POUR LE SAC, et seulement sur ses favoris : c'est le bouton de
  // tri de son pied.
  bool sorted = false;
  // LES FAVORIS DU COFFRE, quand c'est le coffre.
  //
  // 🔴 ILS SONT À NOUS : le serveur ne connaît aucun favori dans un coffre — sa
  // structure n'a pas le champ —, alors qu'il retient ceux du SAC. Nul pour les
  // deux autres contenants, dont le drapeau vient du fil.
  const StorageFavorites* client_favorites = nullptr;

  // Cet objet-là est-il favori, tout compte fait ?
  [[nodiscard]] bool FavoriteOf(std::uint32_t item_id, bool told_by_server) const {
    return client_favorites == nullptr ? told_by_server : client_favorites->Has(item_id);
  }
  // Les noms des forgerons : un item forgé se cherche aussi par eux.
  CreatorNames creators;
  // LES FORGERONS QUE `creators` N'A PAS SU NOMMER, quand l'appelant garde ses
  // cases d'une image à l'autre : il doit redemander leurs noms sans
  // recomposer. Nul pour qui recompose à chaque image.
  std::vector<std::uint32_t>* creators_pending = nullptr;
};

// L'onglet où tombe un objet, d'après le type que le serveur envoie.
//
// 🔴 UN FAVORI TOMBE DANS « FAVORIS » ET NULLE PART AILLEURS, « Tout » compris.
// Ce n'est pas un filtre de plus, c'est un DÉPLACEMENT — et c'est la seule règle
// de ce rangement qu'on ne devine pas en lisant la fenêtre.
// ⚠ `favorites_tab` DIT SI LE CONTENANT EN A UN. Le sac oui, la charrette non —
// et sans ce drapeau un objet de charrette marqué favori par le serveur
// disparaîtrait de TOUS ses onglets, vers un onglet qui n'existe pas.
[[nodiscard]] bool ItemBelongsToTab(const ro::sim::InventoryItem& item, const ItemCellsAsk& ask);

// LES CASES D'UN CONTENANT, croisées avec l'installation.
//
// ⚠ ELLE SERT LE SAC ET LA CHARRETTE. Les deux montrent les mêmes objets de la
// même façon ; ce qui les sépare est ce qu'on peut en faire, pas la façon de les
// composer. `names` garde les noms le temps que la fenêtre les lise — les vues
// rendues POINTENT dessus.
// ⚠ `sorted` NE VAUT QUE POUR LE SAC, et seulement sur ses favoris : c'est le
// bouton de tri du pied. Les deux autres contenants passent faux — leur ordre
// est celui du serveur, et le coffre trie déjà par colonne.
void BuildItemCells(const ro::sim::Inventory& holder, const ItemCellsAsk& ask,
                    const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                    ro::render::ItemIcons* icons, std::vector<std::string>& names,
                    std::vector<ro::ui::InventoryCellView>& cells);

// LE NOM QU'UN JOURNAL ANNONCE : la conversation et le bandeau d'obtention.
//
// Complet, emplacements compris (« +7 Sword [3] »), et suivi de la marque
// d'un objet cassé. Le client d'origine y tait les emplacements ; on les montre,
// parce que c'est ce qui distingue deux gains du même objet (ADR 0096).
//
// ⚠ UN SEUL CHEMIN POUR LES DEUX : ils annoncent le même gain à la même
// seconde, et deux orthographes se liraient comme deux objets.
[[nodiscard]] std::string ReportedItemName(const ro::sim::InventoryItem& item,
                                           const CreatorNames& creators,
                                           const ro::vfs::ItemTable& items,
                                           const ro::vfs::ItemWords& words);

// LE NOM DE BASE D'UN ITEM PAR SON SEUL IDENTIFIANT, sans affinage ni
// emplacements : celui qu'un message du serveur écrit quand il ne désigne
// qu'un numéro. Vide sans table, ou pour un item qu'elle ignore.
[[nodiscard]] std::function<std::string(std::uint32_t item_id)> ItemBaseNames(
    const ro::vfs::ItemTable* items);

// LA BALISE DE CHAT D'UN OBJET DU SAC : l'autre pont, dans l'autre sens.
//
// ⚠ ELLE PORTE L'OBJET DE QUELQU'UN, pas une entrée de catalogue : raffinement,
// cartes, grade et options voyagent avec. C'est toute la différence entre
// « Knife » et « +7 Poring Knife [3] » chez celui qui lit.
[[nodiscard]] ro::vfs::ItemLink ItemLinkOf(const ro::sim::InventoryItem& item);

}  // namespace moonclient
