#include "items/inventory_view.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/ascii.h"
#include "ro/core/log.h"
#include "ro/net/card_composition.h"
#include "ro/net/inventory.h"
#include "ro/sim/card_composition.h"
#include "ro/sim/equip_location.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"

#include "character/equip_slot_map.h"
#include "game/skin_image_of.h"
#include "items/mail_withheld.h"

namespace moonclient {
namespace {

using ro::sim::InventoryItem;
using ro::sim::ItemCategory;
using ro::ui::ItemTab;

// La taille de base du sac chez ce serveur (`INVENTORY_BASE_SIZE`).
//
// ⚠ ELLE NE SERT QUE TANT QUE LE SERVEUR N'A RIEN DIT. Il annonce la capacité
// réelle — extension comprise — par un paquet à lui, qui n'arrive pas toujours
// avant la liste ; afficher zéro en attendant ferait clignoter « 20/0 ».
constexpr int kInventoryBaseSize = 200;

// L'emplacement d'un accessoire de pet : aucun. Le serveur ne lit pas ce champ
// pour lui.
constexpr std::uint32_t kNoEquipLocation = 0;

// LES DEUX TYPES QUI OFFRENT LE SERTISSAGE RAPIDE, tels que le fil les porte :
// l'armure et l'arme, et elles seules. Le serveur refuse de toute façon une
// carte sur tout autre type.
constexpr std::uint8_t kWireArmorType = 4;
constexpr std::uint8_t kWireWeaponType = 5;

// UNE PIÈCE QUI PEUT ENCORE RECEVOIR UNE CARTE, à ce qu'on en voit : ni forgée,
// ni portée, avec une case libre parmi celles qu'elle possède.
[[nodiscard]] bool OffersQuickInsert(const InventoryItem& item, std::uint8_t card_slots) {
  if (item.type != kWireArmorType && item.type != kWireWeaponType) {
    return false;
  }
  if (item.equipped() || ro::sim::ItemIsForged(item)) {
    return false;
  }
  return ro::sim::ItemCardsInSlots(item, card_slots) < card_slots;
}

}  // namespace

bool ItemBelongsToTab(const InventoryItem& item, const ItemCellsAsk& ask) {
  const ro::ui::ItemTab tab = ask.tab;
  // 🔴 LE FAVORI SE DÉPLACE, IL NE SE DOUBLE PAS. Un objet marqué favori quitte
  // tous les autres onglets, « Tout » compris — c'est le dessin du client, et
  // c'est ce qui rend l'onglet utile : il ne répète pas ce qu'on voit ailleurs.
  //
  // ⚠ ENCORE FAUT-IL QUE L'ONGLET EXISTE. La charrette n'en a pas, et le serveur
  // lui envoie pourtant le même octet de drapeaux : sans ce test, un objet
  // marqué disparaîtrait de TOUS ses onglets, vers un onglet qui n'est nulle
  // part. Un objet qu'on ne retrouve plus est pire qu'un objet mal rangé.
  //
  // ⚠ ET LE COFFRE DIT LUI-MÊME QUI EST FAVORI : le serveur n'en marque aucun
  // chez lui, c'est notre liste qui tranche. Voir `ItemCellsAsk`.
  if (ask.FavoriteOf(item.item_id, item.favorite) && ask.favorites_tab) {
    return tab == ItemTab::kFavorite;
  }
  // ⚠ LA FAMILLE VIENT DE `sim`, elle ne se recalcule pas ici : le sac, la
  // charrette et le coffre la liront tous au même endroit, et ne pourront pas
  // dériver l'un de l'autre.
  const ItemCategory family = ro::sim::ItemCategoryOf(item);
  // 🔴 L'APPARAT N'EST PAS UN TYPE, C'EST UN EMPLACEMENT. Une coiffe d'apparat
  // arrive sur le fil sous le type « armure » ; seul son masque la distingue.
  //
  // ⚠ ET IL N'EST PAS DIT POUR CE QU'ON VIENT DE DÉPOSER : le paquet de dépôt ne
  // porte pas l'emplacement, à la différence de la liste d'ouverture. Un costume
  // déposé pendant la session se montre donc sous « Armures » jusqu'à la
  // réouverture du coffre : le fil ne donne rien de mieux.
  const bool costume = ro::sim::IsCostumeLocation(item.wearable_at);
  // ⚠ L'APPARAT QUITTE TOUS LES AUTRES ONGLETS, comme un favori : son onglet est
  // un DÉPLACEMENT, pas un filtre de plus. « Tout » le montre quand même, et les
  // favoris aussi — ils sont tranchés au-dessus.
  if (ask.fine_tabs && costume && tab != ItemTab::kAll && tab != ItemTab::kCostume) {
    return false;
  }
  switch (tab) {
    case ItemTab::kAll:
      return true;
    case ItemTab::kConsumable:
      // ⚠ LE CASH SUIT LA FENÊTRE : le sac n'a pas d'onglet pour lui et le range
      // avec les consommables, comme le client ; le coffre en a un.
      return ask.fine_tabs ? family == ItemCategory::kConsumable
                           : ro::sim::ItemCategoryIsUsable(family);
    case ItemTab::kEquipment:
      return ro::sim::ItemCategoryIsGear(family);
    case ItemTab::kAmmo:
      return family == ItemCategory::kAmmo;
    case ItemTab::kEtc:
      return family == ItemCategory::kEtc;
    case ItemTab::kCard:
      return family == ItemCategory::kCard;
    case ItemTab::kFavorite:
      return false;
    // ── LES QUATRE DU COFFRE ────────────────────────────────────────────────
    case ItemTab::kWeapon:
      return family == ItemCategory::kWeapon;
    case ItemTab::kArmor:
      return family == ItemCategory::kArmor;
    case ItemTab::kCostume:
      return costume;
    case ItemTab::kCash:
      return family == ItemCategory::kCash;
  }
  return false;
}

std::string ReportedItemName(const InventoryItem& item, const CreatorNames& creators,
                             const ro::vfs::ItemTable& items, const ro::vfs::ItemWords& words) {
  ro::vfs::ItemNameFacts facts = ItemNameFactsOf(item);
  creators.Fill(facts);
  std::string name = ro::vfs::ItemNameText(
      ro::vfs::ComposeItemName(facts, items, words, ro::vfs::ItemNameSlots::kShown));
  if (item.damaged && !words.pickup.damaged_mark.empty()) {
    name += ' ';
    name += words.pickup.damaged_mark;
  }
  return name;
}

std::function<std::string(std::uint32_t item_id)> ItemBaseNames(const ro::vfs::ItemTable* items) {
  return [items](std::uint32_t item_id) {
    const ro::vfs::ItemEntry* entry = items != nullptr ? items->Find(item_id) : nullptr;
    return entry != nullptr ? entry->name : std::string{};
  };
}

ro::vfs::ItemLink ItemLinkOf(const InventoryItem& item) {
  static_assert(ro::sim::kItemOptionSlots == ro::vfs::kItemLinkOptionSlots,
                "le fil porte cinq options des deux côtés de la frontière");

  ro::vfs::ItemLink link;
  link.item_id = item.item_id;
  // ⚠ OÙ ELLE PEUT SE PORTER, jamais où elle est portée : c'est le masque de la
  // base que le serveur écrit dans SA balise, et lire `worn_at` ferait dire à un
  // objet rangé qu'il ne se porte nulle part.
  link.wearable_at = item.wearable_at;
  // 🔴 LE SIXIÈME CARACTÈRE EST CE PRÉDICAT-LÀ, et rien d'autre : la balise ne
  // transporte pas le type.
  link.equipable = ro::vfs::ItemTypeIsDecorated(item.type);
  link.refine_level = item.refine_level;
  link.view_id = item.view_id;
  link.grade = item.grade;
  link.cards = item.cards;
  link.damaged = item.damaged;
  for (std::size_t slot = 0; slot < ro::vfs::kItemLinkOptionSlots; ++slot) {
    if (!item.options[slot].filled()) {
      break;
    }
    link.options[slot] =
        ro::vfs::ItemLinkOption{.id = item.options[slot].id,
                                .param = item.options[slot].param,
                                .value = static_cast<std::uint16_t>(item.options[slot].value)};
    link.option_count = slot + 1;
  }
  return link;
}

void InventoryView::Apply(const ro::ui::InventoryWindowAction& asked, ro::net::Connection& link,
                          ro::sim::World& world) {
  if (asked.verb == ro::ui::ItemVerb::kNothing || asked.slot == 0) {
    return;
  }
  // 🔴 LA PLACE SE REVÉRIFIE, on ne se fie pas au clic. Une invite de quantité
  // laisse tout le temps de vendre, consommer ou échanger l'objet visé ; agir
  // sur une place vide ferait au mieux une demande refusée, au pire une demande
  // sur ce qui a pris la place.
  const ro::sim::InventoryItem* item = world.inventory().Find(asked.slot);
  if (item == nullptr) {
    return;
  }
  // 🔴 PENDANT UN ÉCHANGE, AUCUN GESTE D'ITEM — le favori excepté, qui ne
  // déplace rien. Le client d'origine les refuse tous : un item offert puis
  // équipé ou consommé ferait de l'offre « plus que ce que le joueur possède »,
  // et le serveur bannit pour cela.
  if (deal_holds_items_ && asked.verb != ro::ui::ItemVerb::kToggleFavorite && deal_holds_items_()) {
    return;
  }
  // « JOINDRE AU COURRIER » : la case passe à l'écriture, qui rejuge et émet.
  if (asked.verb == ro::ui::ItemVerb::kToMail) {
    if (mail_.attach) {
      mail_.attach(item->client_slot, std::clamp(asked.amount, 1, static_cast<int>(item->amount)));
    }
    return;
  }
  // 🔴 PENDANT L'ÉCRITURE D'UN COURRIER, LE SERVEUR REFUSE L'USAGE D'UN ITEM,
  // et en silence. Porter reste permis — parer le pet aussi : c'est la même
  // demande que porter.
  const bool to_pet = OffersToPet(*item);
  if (asked.verb == ro::ui::ItemVerb::kUseOrWear && item->wearable_at == 0 && !to_pet &&
      mail_.holds_use && mail_.holds_use()) {
    return;
  }

  ro::core::Result<void> sent;
  switch (asked.verb) {
    case ro::ui::ItemVerb::kNothing:
    case ro::ui::ItemVerb::kToMail:
      return;
    case ro::ui::ItemVerb::kUseOrWear:
      // L'ACCESSOIRE DE PET part en équipement, sans emplacement : le serveur
      // le donne au pet sorti, s'il est le sien. Sans pet, le refus se dit ici.
      if (to_pet) {
        if (pet_.refuses && pet_.refuses(world)) {
          return;
        }
        sent = ro::net::RequestEquip(link, item->client_slot, kNoEquipLocation);
        break;
      }
      // LE MÊME GESTE, DEUX PAQUETS. C'est ce que fait le client : le type de
      // l'objet décide, et le joueur ne connaît qu'un double-clic.
      sent = item->wearable_at != 0
                 ? ro::net::RequestEquip(link, item->client_slot,
                                         WearAt(item->wearable_at, asked.alternate))
                 : ro::net::RequestUseItem(link, item->client_slot, world.player_id());
      break;
    case ro::ui::ItemVerb::kTakeOff:
      sent = ro::net::RequestUnequip(link, item->client_slot);
      break;
    case ro::ui::ItemVerb::kToggleFavorite:
      // Le sens se lit sur l'objet : un favori qu'on bascule cesse de l'etre.
      sent = ro::net::RequestFavorite(link, item->client_slot, !item->favorite);
      break;
    case ro::ui::ItemVerb::kToStorage: {
      // ⚠ LA CASE EST CELLE DU SAC, et le serveur lui retranche DEUX là où il
      // n'en retranche qu'un à celle du storage.
      const int amount = std::clamp(asked.amount, 1, static_cast<int>(item->amount));
      sent = ro::net::RequestInventoryToStorage(link, item->client_slot, amount);
      break;
    }
    case ro::ui::ItemVerb::kToCart: {
      // Jamais plus que ce qu'il y a, jamais moins d'un.
      const int amount = std::clamp(asked.amount, 1, static_cast<int>(item->amount));
      sent = ro::net::RequestInventoryToCart(link, item->client_slot, amount);
      break;
    }
    case ro::ui::ItemVerb::kDrop: {
      // ⚠ LE VERROU SE REVÉRIFIE ICI, et pas seulement dans le menu. Un geste
      // peut venir d'ailleurs — un raccourci, un glisser hors de la fenêtre —
      // et le garde-fou ne vaut que s'il est au dernier endroit qui émet.
      if (drop_locked_) {
        return;
      }
      // Jamais plus que ce qu'il y a, jamais moins d'un.
      const int amount = std::clamp(asked.amount, 1, static_cast<int>(item->amount));
      sent = ro::net::RequestDropItem(link, item->client_slot, static_cast<std::uint16_t>(amount));
      break;
    }
    case ro::ui::ItemVerb::kInsertCard:
      if (ro::sim::ItemCategoryOf(*item) != ItemCategory::kCard) {
        return;
      }
      // 🔴 LA CARTE SE RETIENT AVANT DE PARTIR : la réponse ne la nomme pas.
      world.composition().Ask(item->client_slot);
      sent = ro::net::RequestCardCandidates(link, item->client_slot);
      break;
    case ro::ui::ItemVerb::kQuickInsert: {
      // La carte aussi a pu partir depuis la réponse du serveur.
      if (world.inventory().Find(asked.insert_card_slot) == nullptr) {
        return;
      }
      sent = ro::net::RequestInsertCard(link, asked.insert_card_slot, item->client_slot);
      if (sent) {
        // 🔴 LES DEUX OBJETS SE RETIENNENT AVANT LA RÉPONSE : elle ne rappelle
        // que leurs places, et la carte sera consommée.
        world.composition().AskInsert(asked.insert_card_slot, item->client_slot, world.inventory());
      }
      // ⚠ LA LISTE NE VAUT PLUS : la pièce a une case de moins, et la carte
      // une unité de moins. Le prochain sous-menu la redemandera.
      world.composition().ForgetCompatible();
      break;
    }
  }
  if (!sent) {
    ro::core::LogWarning("sac : demande non émise — {}", ro::core::Describe(sent.error()));
  }
}

InventoryView::InventoryView(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                             ro::render::ItemIcons* icons)
    : items_(items), words_(words), icons_(icons) {}

void BuildItemCells(const ro::sim::Inventory& holder, const ItemCellsAsk& ask,
                    const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                    ro::render::ItemIcons* icons, std::vector<std::string>& names,
                    std::vector<ro::ui::InventoryCellView>& cells) {
  names.clear();
  cells.clear();
  names.reserve(holder.size());
  cells.reserve(holder.size());
  // La famille de chaque case, retenue le temps du tri. ⚠ À CÔTÉ ET NON DANS LA
  // CASE : `ui` ne sait pas ce qu'est un type d'objet, et lui en donner un
  // n'aurait servi qu'ici.
  std::vector<ItemCategory> families;
  families.reserve(holder.size());

  for (const InventoryItem& item : holder.items()) {
    // 🔴 CE QU'ON PORTE N'EST PAS DANS LA GRILLE : la pièce équipée se voit sur
    // le mannequin de la fiche, et là seulement. La montrer aux deux endroits
    // ferait chercher lequel des deux exemplaires on tient.
    //
    // ⚠ ELLE RESTE COMPTÉE. Le pied annonce les places OCCUPÉES du sac, et une
    // pièce portée en occupe une : c'est le serveur qui le dit, pas cette
    // grille. Le compte ne se déduit donc jamais du nombre de cases.
    //
    // 🔴 SAUF CE QUE LE MANNEQUIN NE MONTRE PAS — l'équipement d'ombre. Retiré
    // d'ici, il ne serait plus NULLE PART, et il n'y aurait plus de geste pour
    // l'ôter. C'est là que servent encore « Retirer » et le double-clic de cette
    // fenêtre.
    // Et sauf les munitions : voir `ShownInBag`.
    if (!ShownInBag(item.worn_at)) {
      continue;
    }
    if (!ItemBelongsToTab(item, ask)) {
      continue;
    }
    std::string name;
    if (items != nullptr && words != nullptr) {
      ro::vfs::ItemNameFacts facts = ItemNameFactsOf(item);
      if (ask.creators.Fill(facts) && ask.creators_pending != nullptr) {
        ask.creators_pending->push_back(ro::sim::CreatorIdOfCards(facts.cards));
      }
      name = ro::vfs::ItemNameText(ro::vfs::ComposeItemName(facts, *items, *words));
    }
    if (!ro::core::AsciiContainsFolded(name, ask.filter)) {
      continue;
    }
    names.push_back(std::move(name));

    ro::ui::InventoryCellView cell;
    cell.client_slot = item.client_slot;
    cell.item_id = item.item_id;
    // ⚠ LE NOM SE DÉSIGNE APRÈS LA BOUCLE, jamais ici : `names` peut encore se
    // réallouer, et une vue posée maintenant pointerait dans le vide. C'est le
    // genre de pointeur qui survit aux essais et tombe en jeu.
    cell.refine_level = item.refine_level;
    cell.amount = item.amount;
    cell.damaged = item.damaged;
    // CE QUE L'OBJET PERMET. `ui` ne sait pas ce qu'est un type d'objet, et
    // sans ces trois drapeaux son menu proposerait « Équiper » sur une potion.
    //
    // ⚠ LES MUNITIONS SE PORTENT AUSSI. Le client leur réserve une commande à
    // lui, mais sur le fil c'est le même paquet que pour une armure.
    const ItemCategory family = ro::sim::ItemCategoryOf(item);
    families.push_back(family);
    cell.usable = ro::sim::ItemCategoryIsUsable(family);
    cell.wearable = item.wearable_at != 0 &&
                    (ro::sim::ItemCategoryIsGear(family) || family == ItemCategory::kAmmo);
    cell.worn = item.equipped();
    cell.favorite = ask.FavoriteOf(item.item_id, item.favorite);
    // ⚠ IL VIENT DE LA TABLE DU CLIENT, pas du fil : le serveur n'envoie que
    // les cartes posées, et une pièce à deux emplacements vides est
    // indiscernable d'une pièce qui n'en a aucun.
    if (items != nullptr) {
      if (const ro::vfs::ItemEntry* entry = items->Find(item.item_id); entry != nullptr) {
        cell.card_slots = entry->card_slots;
      }
    }
    cell.card = family == ItemCategory::kCard;
    cell.socket_free = OffersQuickInsert(item, cell.card_slots);
    cell.takes_cards = item.type == kWireArmorType || item.type == kWireWeaponType;
    if (icons != nullptr) {
      const ro::render::IconTexture& icon = icons->Get(item.item_id);
      cell.icon = SizedSkinImageOf(icon);
    }
    cells.push_back(cell);
  }

  // Les noms ne bougeront plus : on peut enfin les désigner.
  for (std::size_t rank = 0; rank < cells.size(); ++rank) {
    cells[rank].name = names[rank];
  }
  if (!ask.sorted) {
    return;
  }
  // ── LA VUE TRIÉE : type d'abord, nom ensuite ────────────────────────────────
  //
  // 🔴 APRÈS QUE LES NOMS SONT DÉSIGNÉS, et il faut que ce soit après : une
  // vue de nom pointe dans `names`, que ce tri-ci NE TOUCHE PAS. Trier avant
  // aurait apparié chaque case au nom de sa voisine.
  //
  // ⚠ ON TRIE UNE PERMUTATION, pas les cases : la famille vit dans un tableau
  // parallèle, et échanger deux cases sans échanger leurs familles ferait
  // diverger les deux au premier échange.
  std::vector<std::size_t> order(cells.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::ranges::sort(order, [&](std::size_t one, std::size_t other) {
    if (families[one] != families[other]) {
      return families[one] < families[other];
    }
    return ro::core::AsciiLess(cells[one].name, cells[other].name);
  });
  std::vector<ro::ui::InventoryCellView> ranked;
  ranked.reserve(cells.size());
  for (const std::size_t rank : order) {
    ranked.push_back(cells[rank]);
  }
  cells.swap(ranked);
}

void InventoryView::Refresh(bool open, const ro::sim::World& world, ItemTab tab,
                            std::string_view filter, bool drop_locked, bool sorted,
                            const ItemLayout* layout) {
  drop_locked_ = drop_locked;
  cart_available_ = world.cart().capacity() > 0;
  const ro::sim::PlayerStatus& status = world.player_status();
  const ro::sim::Inventory& bag = world.inventory();
  tally_ = ro::ui::InventoryTally{
      .weight = status.weight,
      .max_weight = status.max_weight,
      .overweight_percent = status.overweight_percent,
      .zeny = status.zeny,
      .item_count = static_cast<int>(bag.size()),
      .max_items = bag.capacity() > 0 ? bag.capacity() : kInventoryBaseSize,
  };
  if (!open) {
    cells_.clear();
    return;
  }
  // ⚠ LE TRI NE VAUT QUE SUR LES FAVORIS, et seulement taille déverrouillée :
  // c'est la condition sous laquelle le bouton existe, et un tri qui
  // continuerait sans son bouton écraserait l'ordre du serveur sans que rien ne
  // l'explique.
  BuildItemCells(world.inventory(),
                 ItemCellsAsk{.tab = tab,
                              .filter = filter,
                              .favorites_tab = true,
                              .sorted = sorted && tab == ItemTab::kFavorite,
                              .creators = creators_},
                 items_, words_, icons_, names_, cells_);
  // L'ACCESSOIRE DE PET : `BuildItemCells` sert aussi les contenants d'où l'on
  // ne pare rien, c'est donc au sac seul de le marquer.
  if (pet_.offers) {
    for (ro::ui::InventoryCellView& cell : cells_) {
      const InventoryItem* item = bag.Find(cell.client_slot);
      cell.pet_accessory = item != nullptr && pet_.offers(*item);
    }
  }
  // LE COURRIER EN COURS D'ÉCRITURE : ce qu'il retient quitte le sac MONTRÉ.
  WithholdMailedCells(cells_, mail_.withheld);
  // LE PLACEMENT LIBRE, ET SES TROUS.
  //
  // ⚠ APRÈS LE TRI, jamais avant : le tri écraserait des places choisies à la
  // main, et les deux ne se demandent de toute façon pas sur le même onglet.
  //
  // ⚠ LA TABLE EST NULLE QUAND LE PLACEMENT NE VAUT PAS — réglage éteint, taille
  // déverrouillée, onglet « Tout ». C'est l'étage qui en décide, et il le décide
  // à UN endroit : cette vue-ci range ce qu'on lui donne.
  if (layout != nullptr) {
    ArrangeCells(cells_, *layout);
  }
}

}  // namespace moonclient
