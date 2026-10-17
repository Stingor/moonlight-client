#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <vector>

#include "ro/sim/inventory.h"

namespace ro::sim {

// LE SERTISSAGE D'UNE CARTE : ce que le serveur PROPOSE, et ce qu'il RÉPOND.
//
// 🔴 LE CLIENT NE DÉCIDE RIEN. Il ne connaît aucune règle de compatibilité —
// type de pièce, emplacement, identification, case libre : il envoie la carte,
// reçoit une liste de places du sac, et la montre telle quelle. Filtrer ici
// proposerait des pièces que le serveur refuse, ou en cacherait qu'il accepte.
//
// ⚠ LA RÉPONSE NE RAPPELLE PAS LA CARTE. Le serveur renvoie les pièces et rien
// d'autre : c'est au demandeur de retenir pour QUELLE carte il a demandé, et
// cette mémoire-là est la seule trace persistante du geste chez le client.
//
// ⚠ ET LA SESSION EST À NOUS. Le serveur ne tient aucun sertissage ouvert : il
// répond à une demande de liste, puis à une demande de sertissage, sans rien
// relier. Fermer n'envoie donc rien.

// Une liste reçue, retenue telle quelle.
struct CardOffer {
  // La carte pour laquelle on avait demandé, figée à la réception.
  std::uint16_t card_slot = 0;
  // Les pièces proposées, dans l'ordre du serveur.
  std::vector<std::uint16_t> equip_slots;
};

// CE QUE LE SERVEUR A RÉPONDU À UNE DEMANDE DE SERTISSAGE.
enum class CardInsertVerdict : std::uint8_t {
  kInserted,
  kRefused,
};

// UNE DEMANDE DE SERTISSAGE PARTIE : les deux objets tels qu'ils étaient au
// départ. Chacun porte sa place du sac, qui est ce que l'accusé rappelle.
struct CardInsertAsk {
  InventoryItem card;
  InventoryItem equip;
};

// CE QU'UN ACCUSÉ RAPPORTE, pour la ligne de conversation.
//
// ⚠ LES DEUX OBJETS SONT CEUX D'AVANT CE SERTISSAGE : la carte n'est pas
// encore retirée du sac, la pièce ne l'a pas encore reçue. C'est ce qui permet
// de dire « Poring Card sertie dans Knife [3] » et non dans « Poring Knife ».
struct CardInsertReport {
  CardInsertVerdict verdict = CardInsertVerdict::kInserted;
  // Une demande retenue répond à cet accusé. Faux : la ligne ne sait nommer
  // ni la carte ni la pièce.
  bool matched = false;
  InventoryItem card;
  InventoryItem equip;
};

// COMBIEN DE DEMANDES, ET D'ACCUSÉS NON LUS, SE RETIENNENT. Le plus long lot
// de la fenêtre en émet quatre ; au-delà, le plus ancien s'oublie.
inline constexpr std::size_t kMostCardInsertAsks = 16;

// LES CARTES QUE LE SERVEUR ACCEPTERAIT SUR UNE PIÈCE — le sertissage RAPIDE.
//
// ⚠ C'EST LA QUESTION INVERSE, et elle n'existe pas dans le protocole du
// client : c'est un paquet du serveur Moonlight, qui applique à chaque carte du
// sac le même prédicat que le sertissage lui-même. La liste est donc exacte.
struct CompatibleCards {
  // La pièce demandée. Zéro : aucune question posée.
  std::uint16_t equip_slot = 0;
  // Le serveur a-t-il répondu ? ⚠ UNE LISTE VIDE EST UNE RÉPONSE : « aucune
  // carte ne convient » ne se confond pas avec « pas encore dit ».
  bool answered = false;
  std::vector<std::uint16_t> card_slots;
};

class CardComposition {
 public:
  // Le client vient de demander la liste des pièces pour cette carte.
  //
  // ⚠ CELA N'OUVRE RIEN : le serveur ne répond PAS quand aucune pièce ne
  // convient. Une session déjà ouverte garde donc sa carte tant qu'une
  // nouvelle liste n'est pas arrivée.
  void Ask(std::uint16_t card_slot) noexcept { asked_card_slot_ = card_slot; }

  // La liste est arrivée : elle ouvre la session, ou remplace celle en cours.
  void Offer(std::vector<std::uint16_t> equip_slots);

  // La session est finie. Aucun paquet : le serveur n'en tient pas.
  void Close() noexcept { open_ = false; }

  [[nodiscard]] bool open() const noexcept { return open_; }
  [[nodiscard]] const CardOffer& offer() const noexcept { return offer_; }

  // LE RANG DE LA DERNIÈRE LISTE, qui ne recule jamais. ⚠ DEUX LISTES
  // IDENTIQUES DE SUITE SONT DEUX RÉPONSES — celle redemandée après un
  // sertissage ressemble souvent à la précédente —, et c'est lui qui le dit.
  [[nodiscard]] std::uint64_t offer_seq() const noexcept { return offer_seq_; }

  // UNE DEMANDE DE SERTISSAGE VIENT DE PARTIR : ses deux objets se retiennent,
  // puisque l'accusé ne rappelle que leurs places et que la carte sera
  // consommée. Rien ne se retient si l'une des deux places est vide : la
  // réponse n'aurait rien à nommer.
  void AskInsert(std::uint16_t card_slot, std::uint16_t equip_slot, const Inventory& bag);

  // L'accusé d'un sertissage, sur ces deux places. ⚠ LE RANG SUIT LA MÊME
  // RÈGLE : deux refus de suite sont deux lignes à écrire.
  //
  // `bag` est le sac AVANT que l'accusé ne le change. La demande qui répond est
  // la PREMIÈRE retenue sur les mêmes places : le serveur traite dans l'ordre,
  // et celles qui la précèdent sont des refus qu'il a tus — ses gardes de
  // compatibilité ne répondent pas. Elles s'oublient.
  //
  // 🔴 LES OBJETS SE RELISENT DANS LE SAC quand ils y sont encore sous la même
  // identité : dans un lot, le deuxième sertissage part d'une pièce qui porte
  // déjà la première carte, et c'est cet état-là qui est « l'avant ». Un objet
  // d'une AUTRE identité à l'une des places — une place réattribuée — fait un
  // rapport sans noms.
  void TellVerdict(CardInsertVerdict verdict, std::uint16_t equip_slot, std::uint16_t card_slot,
                   const Inventory& bag);
  [[nodiscard]] CardInsertVerdict verdict() const noexcept { return verdict_; }
  [[nodiscard]] std::uint64_t verdict_seq() const noexcept { return verdict_seq_; }

  // LES ACCUSÉS PAS ENCORE ÉCRITS, dans l'ordre d'arrivée. Les rend et les
  // oublie : plusieurs accusés d'une même image font chacun leur ligne.
  [[nodiscard]] std::vector<CardInsertReport> TakeReports();
  [[nodiscard]] std::size_t pending_asks() const noexcept { return asks_.size(); }

  // ── LE SERTISSAGE RAPIDE ──────────────────────────────────────────────────
  //
  // La question part pour cette pièce ; la réponse d'avant ne vaut plus.
  void AskCompatible(std::uint16_t equip_slot);
  // La réponse. Fausse, et ignorée, si elle ne porte pas sur la pièce encore
  // demandée : une réponse en retard décrirait la pièce d'avant.
  bool TellCompatible(std::uint16_t equip_slot, std::vector<std::uint16_t> card_slots);
  // Oublie la question : la liste ne vaut plus après un sertissage, et la
  // prochaine ouverture du sous-menu la reposera.
  void ForgetCompatible() { compatible_ = CompatibleCards{}; }
  [[nodiscard]] const CompatibleCards& compatible() const noexcept { return compatible_; }

 private:
  std::uint16_t asked_card_slot_ = 0;
  bool open_ = false;
  CardOffer offer_;
  std::uint64_t offer_seq_ = 0;
  CardInsertVerdict verdict_ = CardInsertVerdict::kInserted;
  std::uint64_t verdict_seq_ = 0;
  std::deque<CardInsertAsk> asks_;
  std::deque<CardInsertReport> reports_;
  CompatibleCards compatible_;
};

}  // namespace ro::sim
