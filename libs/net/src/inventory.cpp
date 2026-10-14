#include "ro/net/inventory.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/text.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

// Les contenants, tels que le serveur les numérote (`e_inventory_type`, dans
// src/map/clif.cpp).
//
// ⚠ IL EN DÉCLARE UN QUATRIÈME — le coffre de guilde — ET NE L'ENVOIE JAMAIS :
// il fait passer celui-ci pour un coffre ordinaire et ne le distingue que par le
// NOM qu'il donne à l'ouverture. Guetter un quatrième numéro serait guetter en
// vain, et se fier au numéro pour distinguer les coffres, se tromper.
constexpr std::uint8_t kInvTypeInventory = 0;
constexpr std::uint8_t kInvTypeCart = 1;
constexpr std::uint8_t kInvTypeStorage = 2;

// Les drapeaux d'un objet ORDINAIRE, dans l'octet de fin.
constexpr std::uint8_t kNormalIdentified = 0x01;
constexpr std::uint8_t kNormalFavorite = 0x02;

// Ceux d'un ÉQUIPEMENT. ⚠ LE MÊME BIT NE DIT PAS LA MÊME CHOSE : le second
// annonce ici une pièce cassée, et le favori recule d'un cran.
constexpr std::uint8_t kEquipIdentified = 0x01;
constexpr std::uint8_t kEquipDamaged = 0x02;
constexpr std::uint8_t kEquipFavorite = 0x04;

// « Ce joueur ne montre pas son équipement » : `MSI_OPEN_EQUIPEDITEM_REFUSED`
// dans l'énumération des messages du serveur (src/map/clif.hpp).
constexpr std::uint16_t kEquipViewRefusedMessage = 1357;

[[nodiscard]] std::optional<sim::ItemHolder> HolderOf(std::uint8_t inv_type) {
  switch (inv_type) {
    case kInvTypeInventory:
      return sim::ItemHolder::kInventory;
    case kInvTypeCart:
      return sim::ItemHolder::kCart;
    case kInvTypeStorage:
      return sim::ItemHolder::kStorage;
    default:
      return std::nullopt;
  }
}

// Les cartes, telles quelles. Ce qu'elles veulent dire — cartes, forgeron ou
// familier — se décide chez qui les lit, pas ici.
void CopyCards(const proto::Equipslotinfo& from, sim::InventoryItem& item) {
  for (std::size_t rank = 0; rank < sim::kItemCardSlots && rank < from.card.size(); ++rank) {
    item.cards[rank] = from.card[rank];
  }
}

// Les options aléatoires.
//
// ⚠ ON NE LIT PAS LE COMPTEUR QUI LES PRÉCÈDE : le serveur le met à zéro quel
// que soit leur nombre — « le client recalcule », dit son code. On parcourt donc
// les cinq entrées, et c'est l'identifiant nul qui dit « case vide ».
void CopyOptions(const std::array<proto::Itemoptions, 5>& from, sim::InventoryItem& item) {
  for (std::size_t rank = 0; rank < sim::kItemOptionSlots && rank < from.size(); ++rank) {
    item.options[rank].id = static_cast<std::uint16_t>(from[rank].index);
    item.options[rank].value = from[rank].value;
    item.options[rank].param = from[rank].param;
  }
}

[[nodiscard]] sim::InventoryItem ItemFromNormal(const proto::NormalitemInfo& entry) {
  sim::InventoryItem item;
  item.client_slot = static_cast<std::uint16_t>(entry.index);
  item.item_id = entry.ITID;
  item.type = entry.type;
  item.amount = entry.count;
  // ⚠ SUR UN OBJET ORDINAIRE, CE CHAMP DIT OÙ IL POURRAIT SE PORTER, pas où il
  // l'est : c'est l'emplacement que sa fiche déclare. Rien ici n'est équipé — le
  // serveur range les pièces portées dans l'autre liste.
  item.wearable_at = entry.WearState;
  CopyCards(entry.slot, item);
  item.rental_until_unix_s = entry.HireExpireDate;
  const std::uint8_t flags = entry.Flag.front();
  item.identified = (flags & kNormalIdentified) != 0;
  item.favorite = (flags & kNormalFavorite) != 0;
  return item;
}

[[nodiscard]] sim::InventoryItem ItemFromEquip(const proto::EquipitemInfo& entry) {
  sim::InventoryItem item;
  item.client_slot = static_cast<std::uint16_t>(entry.index);
  item.item_id = entry.ITID;
  item.type = entry.type;
  // Une pièce d'équipement ne s'empile pas : le serveur n'envoie pas sa
  // quantité, et elle vaut un.
  item.amount = 1;
  item.wearable_at = entry.location;
  item.worn_at = entry.WearState;
  CopyCards(entry.slot, item);
  CopyOptions(entry.option_data, item);
  item.rental_until_unix_s = entry.HireExpireDate;
  item.bind_on_equip = entry.bindOnEquipType;
  item.view_id = entry.wItemSpriteNumber;
  item.refine_level = entry.RefiningLevel;
  item.grade = entry.grade;
  const std::uint8_t flags = entry.Flag.front();
  item.identified = (flags & kEquipIdentified) != 0;
  item.damaged = (flags & kEquipDamaged) != 0;
  item.favorite = (flags & kEquipFavorite) != 0;
  return item;
}

// UN OBJET QUI ENTRE DANS LA CHARRETTE OU LE COFFRE.
//
// ⚠ LES DEUX PAQUETS ONT LA MÊME DISPOSITION mais pas le même type : le
// générateur en produit deux structures distinctes, et un gabarit les prend
// toutes deux sans les confondre.
//
// ⚠ ET ILS NE DISENT PAS CE QUI EST PORTÉ. On ne porte rien depuis une
// charrette ni depuis un coffre : `worn_at` reste nul, et `wearable_at` avec lui
// — le paquet ne le donne pas.
template <typename Added>
[[nodiscard]] sim::InventoryItem ItemFromAdded(const Added& packet) {
  sim::InventoryItem item;
  item.client_slot = static_cast<std::uint16_t>(packet.index);
  item.item_id = packet.itemId;
  item.type = packet.itemType;
  item.amount = static_cast<std::int16_t>(packet.amount);
  CopyCards(packet.slot, item);
  CopyOptions(packet.option_data, item);
  item.refine_level = packet.refine;
  item.grade = packet.grade;
  item.identified = packet.identified != 0;
  item.damaged = packet.damaged != 0;
  return item;
}

// Un objet qui ENTRE dans un contenant : il s'ajoute à ce qui s'y trouve déjà,
// sinon il s'y pose.
//
// ⚠ LE SERVEUR ANNONCE UN AJOUT, PAS UN TOTAL. Poser la quantité reçue écraserait
// ce qui était déjà dans la case — et l'écart ne se verrait qu'au second dépôt.
void AddToHolder(sim::Inventory& holder, sim::InventoryItem arriving) {
  const sim::InventoryItem* already = holder.Find(arriving.client_slot);
  if (already != nullptr && already->item_id == arriving.item_id) {
    arriving.amount = static_cast<std::int16_t>(already->amount + arriving.amount);
    // Une pile portée qu'on complète — des flèches rachetées — reste portée :
    // aucun accusé de pose ne suit.
    arriving.worn_at = already->worn_at;
  }
  holder.Put(std::move(arriving));
}

// Le serveur a-t-il accordé le ramassage ?
//
// ⚠ UN REFUS VIDE LE RESTE DU PAQUET. Le serveur remet tout à zéro sauf la case,
// la quantité et le motif : lire l'identifiant d'objet d'un refus rangerait un
// objet numéro zéro dans le sac.
constexpr std::uint8_t kPickupGranted = 0;

// ── TROIS ACCUSÉS, TROIS CONVENTIONS DE RÉSULTAT ────────────────────────────
//
// 🔴 ELLES NE S'ACCORDENT PAS, ET AUCUN NOM DE CHAMP NE LE DIT. Les trois
// s'appellent `result`, tiennent sur un octet, et deux d'entre elles appellent
// « zéro » ce que la troisième appelle « échec ». Relevé dans le serveur, pas
// déduit :
//
//   0x0999 équiper  : `enum clif_equipitemack_flag` — OK = 0, ÉCHEC = 2,
//                     NIVEAU INSUFFISANT = 1 (clif.hpp, branche de cette
//                     version : les valeurs changent selon le PACKETVER).
//   0x099A retirer  : le serveur INVERSE lui-même le booléen avant de l'écrire
//                     (`success = !success`, gardé par PACKETVER >= 20110824),
//                     et sa propre note le dit : « inversed for v2 v5 ».
//                     Donc OK = 0.
//   0x01C8 utiliser : `p.result = ok` — celui-là est direct, OK = 1.
constexpr std::uint8_t kEquipGranted = 0;
constexpr std::uint8_t kTakeOffGranted = 0;
constexpr std::uint8_t kUseRefused = 0;

// ── LE REFUS D'ÉQUIPER OU DE RETIRER, DIT DANS LA DISCUSSION ────────────────
//
// Une ligne rouge dans la case « Équipement mis/retiré », et seulement pour
// ces codes-là ; un code inconnu ne dit rien. Un accordé se dit aussi, dans la
// même case, mais pas ici : sa phrase nomme la pièce — voir
// `GrantedEquipChangeOf`.
//
//   0x0999 code 1 (niveau insuffisant) : « conditions non remplies » ;
//   0x0999 code 2 (échec)              : « vous ne pouvez pas équiper » ;
//   0x099A code 1 (échec)              : « conditions non remplies », la même
//                                        phrase qu'à l'équipement, telle quelle.
constexpr std::uint8_t kEquipRefusedRequirements = 1;
constexpr std::uint8_t kEquipRefused = 2;
constexpr std::uint8_t kTakeOffRefused = 1;
constexpr std::uint32_t kEquipRefusalColor = 0xFF0000;

void SayEquipRefusal(sim::World& world, const char* text, std::uint32_t now_ms) {
  world.RecordChat(sim::ChatKind::kServer, sim::ChatType::kEquipmentOnOff, 0, text, now_ms,
                   kEquipRefusalColor);
}

[[nodiscard]] const char* RequirementsRefusalText() {
  return core::Tr("Vous ne pouvez pas équiper cet item : vous n'en remplissez pas les conditions.");
}

// LA TAILLE DE BASE DU SAC chez ce serveur (`INVENTORY_BASE_SIZE`).
//
// ⚠ LE PAQUET N'ANNONCE QUE L'EXTENSION, pas le total : le serveur écrit
// `expansionSize = inventory_slots - INVENTORY_BASE_SIZE`. Prendre le nombre
// reçu pour la capacité afficherait « 20/0 » sur un personnage sans extension.
constexpr int kInventoryBaseSize = 200;

}  // namespace

FrameOutcome ApplyInventoryFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  switch (frame.opcode) {
    // ── L'OUVERTURE D'UN CONTENANT ─────────────────────────────────────────
    case proto::ZcInventoryStart::kOpcode:
      return Decoded<proto::ZcInventoryStart>(frame, [&world](const auto& packet) {
        const std::optional<sim::ItemHolder> holder = HolderOf(packet.invType);
        if (!holder.has_value()) {
          return FrameOutcome::kUnhandled;
        }
        world.HolderFor(*holder).BeginFill(packet.name);
        // ⚠ ET POUR LE COFFRE, C'EST AUSSI L'OUVERTURE DE LA SESSION. Le sac et
        // la charrette se relistent sans rien ouvrir ; le coffre, lui, n'arrive
        // que parce que le serveur vient d'en ouvrir un.
        if (*holder == sim::ItemHolder::kStorage) {
          world.OpenStorage();
        }
        return FrameOutcome::kApplied;
      });

    case proto::ZcInventoryEnd::kOpcode:
      return Decoded<proto::ZcInventoryEnd>(frame, [&world](const auto& packet) {
        const std::optional<sim::ItemHolder> holder = HolderOf(packet.invType);
        if (!holder.has_value()) {
          return FrameOutcome::kUnhandled;
        }
        world.HolderFor(*holder).EndFill();
        return FrameOutcome::kApplied;
      });

    // ── LES DEUX LISTES ────────────────────────────────────────────────────
    case kItemListNormalOpcode:
      return Decoded<proto::PacketItemlistNormal>(frame, [&world](const auto& packet) {
        const std::optional<sim::ItemHolder> holder = HolderOf(packet.invType);
        if (!holder.has_value()) {
          return FrameOutcome::kUnhandled;
        }
        sim::Inventory& into = world.HolderFor(*holder);
        for (const proto::NormalitemInfo& entry : packet.list) {
          into.Put(ItemFromNormal(entry));
        }
        return FrameOutcome::kApplied;
      });

    case kItemListEquipOpcode:
      return Decoded<proto::PacketItemlistEquip>(frame, [&world](const auto& packet) {
        const std::optional<sim::ItemHolder> holder = HolderOf(packet.invType);
        if (!holder.has_value()) {
          return FrameOutcome::kUnhandled;
        }
        sim::Inventory& into = world.HolderFor(*holder);
        for (const proto::EquipitemInfo& entry : packet.list) {
          into.Put(ItemFromEquip(entry));
        }
        return FrameOutcome::kApplied;
      });

    // ── CE QUI ENTRE ET CE QUI SORT DU SAC ─────────────────────────────────
    //
    // Ces trois-là ne portent aucun contenant : ils ne parlent que du sac.
    case proto::ZcItemPickupAck::kOpcode:
      return Decoded<proto::ZcItemPickupAck>(frame, [&world, now_ms](const auto& packet) {
        if (packet.result != kPickupGranted) {
          // ⚠ UN REFUS NE RANGE RIEN, ET N'EST PAS MUET POUR AUTANT. Sans
          // trace, le joueur voit son ramassage échouer sans savoir si son sac
          // est plein ou s'il est en surcharge : le code EST l'information, le
          // reste du paquet étant remis à zéro.
          //
          // 🔴 LE CODE PART TEL QUEL, sans filtre ni traduction : les mots de
          // la ligne sont ceux que `vfs` pose et que `net` ne voit pas. Le fait
          // est daté ici, la phrase s'écrit au-dessus.
          world.inventory().RecordRefused(packet.result, now_ms);
          return FrameOutcome::kApplied;
        }
        sim::InventoryItem item;
        item.client_slot = packet.Index;
        item.item_id = packet.nameid;
        item.type = packet.type;
        item.amount = static_cast<std::int16_t>(packet.count);
        item.wearable_at = packet.location;
        CopyCards(packet.slot, item);
        CopyOptions(packet.option_data, item);
        item.rental_until_unix_s = packet.HireExpireDate;
        item.bind_on_equip = packet.bindOnEquipType;
        item.view_id = packet.look;
        item.refine_level = packet.refiningLevel;
        item.grade = packet.grade;
        // ⚠ ICI LES DRAPEAUX SONT DES OCTETS PLEINS, pas des bits : le même fait
        // se lit autrement que dans les listes.
        item.identified = packet.IsIdentified != 0;
        item.damaged = packet.IsDamaged != 0;
        item.favorite = packet.favorite != 0;

        sim::Inventory& bag = world.inventory();
        // 🔴 LE JOURNAL D'ABORD, LE RANGEMENT ENSUITE : `packet.count` est un
        // AJOUT, et une fois la pile mise à jour la quantité ramassée n'existe
        // plus nulle part.
        bag.RecordObtained(item, now_ms);

        // Un objet qui s'empile ARRIVE PAR SA CASE, avec ce qu'elle contient
        // désormais en tout : le serveur envoie la quantité AJOUTÉE, et la case
        // porte peut-être déjà le reste de la pile.
        //
        // ⚠ LA MÊME RÈGLE QUE `AddToHolder`, qui fait la même chose pour le
        // chariot et le coffre : une case qui porte un AUTRE objet — état
        // resté d'avant une resynchronisation — est remplacée, pas additionnée.
        AddToHolder(bag, item);
        return FrameOutcome::kApplied;
      });

    case proto::ZcDeleteItemFromBody::kOpcode:
      return Decoded<proto::ZcDeleteItemFromBody>(frame, [&world](const auto& packet) {
        // Le MOTIF du retrait — vendu, consommé, rangé — n'entre pas dans le
        // monde : le sac ne retient que ce qu'il contient. Il se dira le jour où
        // quelque chose l'affiche.
        world.inventory().Take(packet.index, packet.count);
        return FrameOutcome::kApplied;
      });

    case proto::ZcItemThrowAck::kOpcode:
      return Decoded<proto::ZcItemThrowAck>(frame, [&world, now_ms](const auto& packet) {
        // UN JET AU SOL, que ce paquet seul annonce. ⚠ UN COMPTE NUL N'EN EST
        // PAS UN : le serveur réemploie ce paquet à zéro pour faire relire une
        // case sans rien retirer, et la ligne « Vous avez jeté » mentirait.
        sim::Inventory& bag = world.inventory();
        if (const sim::InventoryItem* held = bag.Find(packet.index);
            held != nullptr && packet.count > 0) {
          sim::InventoryItem dropped = *held;
          dropped.amount = static_cast<std::int16_t>(packet.count);
          bag.RecordDropped(dropped, now_ms);
        }
        bag.Take(packet.index, static_cast<std::int16_t>(packet.count));
        return FrameOutcome::kApplied;
      });

    case proto::ZcInventoryTab::kOpcode:
      return Decoded<proto::ZcInventoryTab>(frame, [&world](const auto& packet) {
        // 🔴 LE CHAMP DIT LE CONTRAIRE DE SON NOM. Le serveur écrit
        // `favorite = (item.favorite == 1) ? false : true` : sur le fil, FAUX
        // signifie que l'objet EST un favori. Ce champ dit « il est dans
        // l'onglet NORMAL », et le lire tel quel range chaque objet du mauvais
        // côté — sans erreur, sans trace, et de façon parfaitement symétrique à
        // l'aller comme au retour.
        return world.inventory().SetFavorite(static_cast<std::uint16_t>(packet.index),
                                             !packet.favorite)
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    case proto::ZcReqWearEquipAck::kOpcode:
      return Decoded<proto::ZcReqWearEquipAck>(frame, [&world, now_ms](const auto& packet) {
        if (packet.result != kEquipGranted) {
          // ⚠ REFUS = RIEN À FAIRE AU SAC, et surtout pas « appliqué » : il n'a
          // pas bougé, et le relevé ne doit pas laisser croire le contraire. Le
          // refus se DIT, lui.
          if (packet.result == kEquipRefusedRequirements) {
            SayEquipRefusal(world, RequirementsRefusalText(), now_ms);
          } else if (packet.result == kEquipRefused) {
            SayEquipRefusal(world, core::Tr("Vous ne pouvez pas équiper cet item."), now_ms);
          }
          return FrameOutcome::kIgnored;
        }
        return world.inventory().Wear(packet.index, packet.wearLocation, packet.wItemSpriteNumber)
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    case proto::PacketUnequipitemAck::kOpcode:
      return Decoded<proto::PacketUnequipitemAck>(frame, [&world, now_ms](const auto& packet) {
        if (packet.result != kTakeOffGranted) {
          if (packet.result == kTakeOffRefused) {
            SayEquipRefusal(world, RequirementsRefusalText(), now_ms);
          }
          return FrameOutcome::kIgnored;
        }
        return world.inventory().TakeOff(packet.index) ? FrameOutcome::kApplied
                                                       : FrameOutcome::kIgnored;
      });

    case proto::ZcUseItemAck::kOpcode:
      return Decoded<proto::ZcUseItemAck>(frame, [&world](const auto& packet) {
        if (packet.result == kUseRefused) {
          return FrameOutcome::kIgnored;
        }
        // 🔴 LE SERVEUR DONNE CE QUI RESTE, pas ce qui a été consommé. Le
        // retrancher viderait la pile deux fois trop vite — et l'écart ne se
        // verrait qu'au bout de plusieurs usages.
        return world.inventory().SetAmount(static_cast<std::uint16_t>(packet.index), packet.amount)
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    case proto::ZcAckItemidentify::kOpcode:
      return Decoded<proto::ZcAckItemidentify>(frame, [&world](const auto& packet) {
        // ⚠ CE SERVEUR NE CRÉE JAMAIS D'OBJET NON IDENTIFIÉ hors commandes
        // d'administration : le paquet est traité pour être complet, pas parce
        // qu'on l'attend.
        return world.inventory().Identify(packet.index) ? FrameOutcome::kApplied
                                                        : FrameOutcome::kIgnored;
      });

    case proto::ZcNotifyBindOnEquip::kOpcode:
      return Decoded<proto::ZcNotifyBindOnEquip>(frame, [&world](const auto& packet) {
        return world.inventory().Bind(static_cast<std::uint16_t>(packet.index))
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    case proto::ZcEquipitemDamaged::kOpcode:
      return Decoded<proto::ZcEquipitemDamaged>(frame, [&world](const auto& packet) {
        // ⚠ LE PAQUET PART À TOUTE LA ZONE, et nomme donc QUI. Ce qui concerne
        // le sac n'arrive que si c'est le joueur : l'équipement des autres ne
        // s'y trouve pas.
        if (packet.GID != world.player_id()) {
          return FrameOutcome::kIgnored;
        }
        return world.inventory().MarkWornDamaged(packet.equipLocation) ? FrameOutcome::kApplied
                                                                       : FrameOutcome::kIgnored;
      });

    case proto::ZcExtendBodyitemSize::kOpcode:
      return Decoded<proto::ZcExtendBodyitemSize>(frame, [&world](const auto& packet) {
        world.inventory().SetCapacity(kInventoryBaseSize + packet.expansionSize);
        return FrameOutcome::kApplied;
      });

    case proto::ZcEquipArrow::kOpcode:
      return Decoded<proto::ZcEquipArrow>(frame, [&world](const auto& packet) {
        // ⚠ LES MUNITIONS ONT LEUR PROPRE ACCUSÉ, et il ne dit PAS où elles se
        // posent — il ne donne que la place. L'emplacement se relit donc sur
        // l'objet lui-même : il n'en a qu'un, et le nommer ici en ferait une
        // constante de plus à tenir juste.
        const sim::InventoryItem* ammo = world.inventory().Find(packet.index);
        if (ammo == nullptr) {
          return FrameOutcome::kIgnored;
        }
        return world.inventory().Wear(packet.index, ammo->wearable_at, ammo->view_id)
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    // ── LA CHARRETTE ──────────────────────────────────────────────────────────
    case proto::ZcAddItemToCart::kOpcode:
      return Decoded<proto::ZcAddItemToCart>(frame, [&world](const auto& packet) {
        AddToHolder(world.cart(), ItemFromAdded(packet));
        return FrameOutcome::kApplied;
      });

    case proto::ZcDeleteItemFromCart::kOpcode:
      return Decoded<proto::ZcDeleteItemFromCart>(frame, [&world](const auto& packet) {
        world.cart().Take(packet.index, static_cast<std::int16_t>(packet.amount));
        return FrameOutcome::kApplied;
      });

    case proto::ZcCartoff::kOpcode:
      return Decoded<proto::ZcCartoff>(frame, [&world](const auto&) {
        // ⚠ LA CHARRETTE PART AVEC SON CONTENU ET SA CAPACITÉ. Elle ne se referme
        // pas, elle DISPARAÎT — garder l'un ou l'autre montrerait un contenant
        // que le personnage n'a plus.
        world.cart().Forget();
        return FrameOutcome::kApplied;
      });

    case proto::ZcNotifyCartitemCountinfo::kOpcode:
      return Decoded<proto::ZcNotifyCartitemCountinfo>(frame, [&world](const auto& packet) {
        world.cart().SetCapacity(packet.maxCount);
        // ⚠ UNE CHARRETTE SE PÈSE, et ce poids n'est pas celui du personnage :
        // elle porte pour lui sans qu'il le sente.
        world.cart().SetLoad(packet.curWeight, packet.maxWeight);
        return FrameOutcome::kApplied;
      });

    // ── LE COFFRE ─────────────────────────────────────────────────────────────
    case proto::ZcAddItemToStore::kOpcode:
      return Decoded<proto::ZcAddItemToStore>(frame, [&world](const auto& packet) {
        AddToHolder(world.storage(), ItemFromAdded(packet));
        return FrameOutcome::kApplied;
      });

    case proto::ZcDeleteItemFromStore::kOpcode:
      return Decoded<proto::ZcDeleteItemFromStore>(frame, [&world](const auto& packet) {
        world.storage().Take(packet.index, static_cast<std::int16_t>(packet.amount));
        return FrameOutcome::kApplied;
      });

    case proto::ZcNotifyStoreitemCountinfo::kOpcode:
      return Decoded<proto::ZcNotifyStoreitemCountinfo>(frame, [&world](const auto& packet) {
        world.storage().SetCapacity(packet.max_amount);
        return FrameOutcome::kApplied;
      });

    case proto::ZcCloseStore::kOpcode:
      return Decoded<proto::ZcCloseStore>(frame, [&world](const auto&) {
        world.CloseStorage();
        return FrameOutcome::kApplied;
      });

    case kOverweightPercentOpcode:
      return Decoded<proto::ZcOverweightPercent>(frame, [&world](const auto& packet) {
        world.SetOverweightPercent(static_cast<std::int64_t>(packet.percent));
        return FrameOutcome::kApplied;
      });

    default:
      return FrameOutcome::kUnhandled;
  }
}

std::optional<EquipChange> GrantedEquipChangeOf(const Frame& frame) {
  if (frame.opcode == proto::ZcReqWearEquipAck::kOpcode) {
    proto::ZcReqWearEquipAck packet;
    if (!proto::Decode(frame.bytes, packet) || packet.result != kEquipGranted) {
      return std::nullopt;
    }
    return EquipChange{.client_slot = packet.index, .equipped = true};
  }
  if (frame.opcode == proto::PacketUnequipitemAck::kOpcode) {
    proto::PacketUnequipitemAck packet;
    if (!proto::Decode(frame.bytes, packet) || packet.result != kTakeOffGranted) {
      return std::nullopt;
    }
    return EquipChange{.client_slot = packet.index, .equipped = false};
  }
  return std::nullopt;
}

FrameOutcome ApplyViewEquipFrame(const Frame& frame, sim::ViewedEquipmentBook& book) {
  // LE REFUS passe par le canal des messages du client, qui en porte bien
  // d'autres : seul celui-ci est à nous.
  if (frame.opcode == proto::ZcMsg::kOpcode) {
    return Decoded<proto::ZcMsg>(frame, [&book](const auto& packet) {
      if (packet.msgId != kEquipViewRefusedMessage) {
        return FrameOutcome::kUnhandled;
      }
      book.NoteRefused();
      return FrameOutcome::kApplied;
    });
  }
  if (frame.opcode != proto::ZcEquipwinMicroscope::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  return Decoded<proto::ZcEquipwinMicroscope>(frame, [&book](const auto& packet) {
    sim::ViewedEquipment sheet;
    // Le nom vient du jeu, donc de sa page de codes.
    sheet.name = core::Utf8FromCp1252(packet.characterName);
    // ⚠ BAS, MILIEU, HAUT, dans cet ordre-ci : ce paquet ne range PAS ses coiffes
    // comme celui d'apparition, qui écrit bas, haut, milieu. Le serveur remplit
    // chacun depuis sa propre ligne, et les deux ne se recopient pas l'un l'autre.
    sheet.look = sim::ViewedLook{.job = static_cast<std::uint16_t>(packet.job),
                                 .hair = static_cast<std::uint16_t>(packet.head),
                                 .head_low = static_cast<std::uint16_t>(packet.accessory),
                                 .head_mid = static_cast<std::uint16_t>(packet.accessory2),
                                 .head_top = static_cast<std::uint16_t>(packet.accessory3),
                                 .robe = static_cast<std::uint16_t>(packet.robe),
                                 .hair_color = static_cast<std::uint16_t>(packet.headpalette),
                                 .clothes_color = static_cast<std::uint16_t>(packet.bodypalette),
                                 .body2 = static_cast<std::uint16_t>(packet.body2),
                                 .sex = packet.sex};
    for (const proto::EquipitemInfo& entry : packet.list) {
      // ⚠ UNE PIÈCE QUI N'EST PORTÉE NULLE PART NE SE MONTRE PAS : le serveur ne
      // l'envoie pas, et le client d'origine la saute s'il le faisait.
      if (entry.WearState == 0) {
        continue;
      }
      // ⚠ GARDÉE TELLE QUE LE FIL LA DONNE, doublons compris : voir
      // `ViewedEquipment::pieces`.
      sheet.pieces.push_back(ItemFromEquip(entry));
    }
    book.Receive(std::move(sheet));
    return FrameOutcome::kApplied;
  });
}

}  // namespace ro::net
