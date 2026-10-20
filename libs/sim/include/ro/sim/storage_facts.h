#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace ro::sim {

// CE QUE LE SERVEUR DIT DES OBJETS QU'IL VIENT DE LISTER DANS LE COFFRE.
//
// 🔴 ELLES NE SONT PAS DANS L'INSTALLATION DU JOUEUR. Le prix de revente, le
// sous-type d'une arme et le masque d'emplacement vivent dans la BASE DU
// SERVEUR (`item_db`), et `itemInfo.lua` — la seule table que le client lise —
// n'en porte aucun. Un client qui voudrait les deviner recopierait une base
// qu'il ne saurait pas tenir à jour ; le serveur, lui, les envoie (`ZC 0x0F0F`,
// juste après la liste du coffre).
//
// ⚠ ELLE NE COUVRE QUE LE COFFRE OUVERT, et c'est le paquet qui le veut : le
// serveur la bâtit sur le contenu qu'il vient d'envoyer. Un objet du sac n'y est
// que s'il se trouve aussi dans le coffre — ne rien conclure d'une absence.
struct StorageItemFacts {
  std::uint32_t item_id = 0;
  // Le prix auquel un PNJ RACHÈTE la pièce (`value_sell`), à l'unité.
  //
  // ⚠ CE N'EST PAS CE QU'ELLE A COÛTÉ : un objet se rachète au tiers de son
  // prix d'achat, et c'est le prix de rachat qui dit ce qu'un coffre « vaut »
  // si on le vidait chez le marchand.
  std::uint32_t sell_price = 0;
  // Le type d'arme (`W_*`) ou de munition (`A_*`), tel que le serveur le nomme.
  // Zéro pour tout ce qui n'est ni l'un ni l'autre.
  std::uint8_t subtype = 0;
  // OÙ LA PIÈCE PEUT SE PORTER, en drapeaux — les mêmes que `equip_location.h`.
  // Zéro pour ce qui ne se porte pas.
  std::uint32_t equip_mask = 0;
  // Combien de cartes elle peut recevoir.
  //
  // ⚠ LA TABLE DU CLIENT LE DIT AUSSI, et c'est elle qui sert au NOM composé.
  // Celui-ci vient du serveur et ne s'y substitue pas : il est là parce que le
  // paquet le porte, et parce qu'une base de serveur modifiée est la seule
  // vérité quand les deux divergent.
  std::uint16_t card_slots = 0;
};

// CE QU'UNE TRAME FAIT DE LA TABLE.
//
// 🔴 UN COFFRE PEUT DÉPASSER UNE TRAME. La longueur du paquet tient sur seize
// bits signés : au-delà de 2 183 entrées, le serveur coupe la liste en
// plusieurs trames consécutives, et seule la première dit « repartir de zéro ».
// Une trame qui remplacerait la table n'en laisserait que la dernière tranche.
enum class StorageFactsPart : std::uint8_t {
  // La première trame d'une liste neuve — une ouverture de coffre : la table
  // repart de zéro, puis reçoit les entrées.
  kStartsList,
  // La suite d'une liste découpée, ou l'objet qui vient d'entrer dans le coffre
  // ouvert : les entrées S'AJOUTENT à la table.
  kExtendsList,
};

// LA TABLE, TELLE QUE LE SERVEUR L'A DITE DEPUIS L'OUVERTURE DU COFFRE.
//
// ⚠ UNE LISTE TRIÉE ET NON UNE TABLE DE HACHAGE : elle se refait en entier à
// chaque ouverture de coffre, et une recherche binaire y répond sans allouer.
// C'est aussi ce qui la rend comparable d'une ouverture à l'autre dans un essai.
class StorageFacts {
 public:
  // ⚠ LES ENTRÉES ARRIVENT PAR PLACE et non par objet : le serveur en envoie une
  // par ligne du coffre, donc le même identifiant peut revenir, dans une trame
  // comme d'une trame à l'autre. La première reçue gagne — elles portent la
  // même chose.
  void Tell(std::vector<StorageItemFacts> told, StorageFactsPart part);

  // Rend nul quand le serveur n'a rien dit de cet objet — ce qui est le cas
  // normal tant qu'aucun coffre n'a été ouvert.
  [[nodiscard]] const StorageItemFacts* Of(std::uint32_t item_id) const;

  void Clear() noexcept {
    facts_.clear();
    ++generation_;
  }
  [[nodiscard]] bool empty() const noexcept { return facts_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return facts_.size(); }
  [[nodiscard]] std::span<const StorageItemFacts> entries() const noexcept { return facts_; }

  // COMBIEN DE FOIS LA TABLE A ÉTÉ DITE. Un compte, pas un drapeau : celui qui
  // en dérive une vue compare au sien et sait s'il doit la refaire.
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }

 private:
  std::vector<StorageItemFacts> facts_;
  std::uint64_t generation_ = 0;
};

}  // namespace ro::sim
