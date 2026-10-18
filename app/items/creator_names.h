#pragma once

#include "ro/sim/inventory.h"
#include "ro/vfs/item_name.h"

#include "items/item_facts_asker.h"

namespace moonclient {

// Le nom composé d'un objet tenu : le pont entre ce que `sim` porte et ce que
// `vfs` sait composer. ⚠ LE FORGERON N'EST PAS SUR LE FIL : `CreatorNames` le
// pose ensuite.
[[nodiscard]] ro::vfs::ItemNameFacts ItemNameFactsOf(const ro::sim::InventoryItem& item);

// LE NOM DU FORGERON, POUR TOUTE SURFACE QUI NOMME UN ITEM.
//
// Un item forgé ou signé ne porte que l'identifiant de son créateur ; son nom
// se demande au serveur (`CZ 0x0369`), une fois par personnage et par session.
// Tant qu'il n'est pas arrivé, le nom composé dit « Sans nom », en rouge —
// comme le client pendant sa première seconde.
//
// ⚠ UNE VUE, PAS UN LIVRE : les noms vivent dans les faits d'objet de la
// description, et la même poignée sert aux grilles, au mannequin, au journal,
// à la conversation. Une copie vide ne nomme rien et ne demande rien.
class CreatorNames {
 public:
  CreatorNames() = default;
  explicit CreatorNames(ItemFactsAsker* asker) noexcept : asker_(asker) {}

  // Pose le forgeron dans les faits quand leurs cartes en portent un et que le
  // serveur l'a nommé. Rend VRAI quand il en porte un qu'on ne connaît pas
  // encore — la demande est alors partie.
  bool Fill(ro::vfs::ItemNameFacts& facts) const;

 private:
  ItemFactsAsker* asker_ = nullptr;
};

// UN GAIN ATTEND-IL ENCORE SON FORGERON ? Vrai quand un ramassage plus récent
// que `seen_seq` porte un forgeron que le serveur n'a pas nommé, depuis moins de
// trois secondes. Le bandeau et le journal s'y arrêtent, pour ne pas écrire
// « Sans nom » une milliseconde avant la réponse.
[[nodiscard]] bool AwaitsCreator(const ro::sim::Inventory& bag, std::uint64_t seen_seq,
                                 const CreatorNames& creators, std::uint32_t now_ms);

}  // namespace moonclient
