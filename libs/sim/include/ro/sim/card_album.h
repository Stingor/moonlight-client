#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace ro::sim {

// L'ALBUM DE CARTES : un conteneur de cartes qui vit HORS du système de coffre.
//
// Le serveur le tient au COMPTE Moonlight (et non au personnage), dans une table
// à lui : une carte n'ayant aucun état d'instance, le couple (identifiant,
// réserve) la décrit entièrement.
//
// 🔴 LA RÈGLE : la PREMIÈRE copie d'une carte est SACRIFIÉE — consommée, elle
// ouvre pour toujours la pochette de cette carte. Les copies suivantes s'y
// rangent et restent retirables. Une carte jamais sacrifiée ne se range pas.
//
// ⚠ UNE POCHETTE OUVERTE DONT LA RÉSERVE EST VIDE EST UN ÉTAT NORMAL, et c'est
// pourquoi `unlocked` vit À CÔTÉ de `reserve` : une réserve nulle ne dit pas si
// la pochette a été payée. Confondre les deux ferait réclamer un second
// sacrifice pour une carte déjà acquise.
//
// ⚠ LE SERVEUR NE DIFFUSE RIEN DE LUI-MÊME : on lui demande, il répond par
// l'état COMPLET — tout le catalogue, chaque carte avec sa réserve. Cet état
// se REMPLACE donc à chaque réponse, il ne se fusionne jamais : une fusion
// laisserait vivre une carte que le serveur vient de retirer de son catalogue.

// CE QUE LE SERVEUR A RÉPONDU À LA DERNIÈRE COMMANDE. Les valeurs sont celles du
// fil (`e_card_album_result`), et elles ne se renumérotent pas.
//
// ⚠ UNE VALEUR HORS DE CETTE LISTE PEUT ARRIVER — un serveur plus neuf que nous
// —, et elle se garde telle quelle : c'est l'affichage qui la traduit par un
// refus générique.
enum class CardAlbumResult : std::uint8_t {
  kOk = 0,
  kFail = 1,
  kNotACard = 2,
  kLocked = 3,
  kAlreadyUnlocked = 4,
  kNotEnough = 5,
  kInventoryFull = 6,
  kBusy = 7,
  kBound = 8,
  kStackFull = 9,
  kNoAccount = 10,
  // 🔴 UN AUTRE COMPTE DE JEU DU MÊME COMPTE MOONLIGHT TIENT L'ALBUM. L'état qui
  // accompagne ce code est le cache d'une session qui n'a pas la main : il ne
  // vaut rien, et l'album se vide plutôt que de montrer une réserve qui n'est
  // pas la nôtre à manipuler.
  kInUse = 11,
  kNotOpen = 12,
};

// LA NATURE DU MONSTRE LE PLUS CORIACE qui lâche une carte — `e_mob_bosstype`.
// Le client n'a pas de base de monstres : c'est le catalogue qui l'apporte.
inline constexpr std::uint8_t kCardDropperOrdinary = 0;
inline constexpr std::uint8_t kCardDropperMiniBoss = 1;
inline constexpr std::uint8_t kCardDropperMvp = 2;

// UNE CARTE DU CATALOGUE, telle que le serveur la décrit.
struct CardAlbumCard {
  // 🔴 L'IDENTIFIANT DU SERVEUR, que le retrait lui renvoie tel quel.
  std::uint32_t card_id = 0;
  // Les copies rangées, retirables.
  std::uint16_t reserve = 0;
  // La pochette a été payée d'un sacrifice.
  bool unlocked = false;
  // Le masque de l'emplacement CIBLE (armure, arme…) : le client n'a pas de
  // base d'objets, c'est la seule façon de classer une carte par emplacement.
  std::uint32_t equip_mask = 0;
  // Voir `kCardDropperOrdinary`. Zéro quand le serveur ne l'envoie pas.
  std::uint8_t dropper_rank = kCardDropperOrdinary;
};

class CardAlbum {
 public:
  // Une réponse du serveur : son verdict, et l'état complet qui l'accompagne.
  //
  // `dropper_known` dit si les entrées portaient l'octet de nature ; sans lui,
  // toutes les cartes se lisent « ordinaires » et les filtres de nature n'ont
  // pas lieu d'être.
  void Receive(CardAlbumResult result, std::vector<CardAlbumCard> cards, bool dropper_known);

  // Tout oublier : la session de zone est finie.
  void Clear() noexcept;

  [[nodiscard]] std::span<const CardAlbumCard> cards() const noexcept { return cards_; }
  [[nodiscard]] const CardAlbumCard* Find(std::uint32_t card_id) const;

  [[nodiscard]] CardAlbumResult last_result() const noexcept { return last_result_; }
  // Le RANG de la dernière réponse, zéro tant qu'aucune n'est arrivée. C'est
  // ce qui distingue deux réponses identiques l'une de l'autre.
  [[nodiscard]] std::uint32_t arrival() const noexcept { return arrival_; }
  [[nodiscard]] bool dropper_known() const noexcept { return dropper_known_; }
  // Voir `CardAlbumResult::kInUse`.
  [[nodiscard]] bool blocked() const noexcept { return last_result_ == CardAlbumResult::kInUse; }

  [[nodiscard]] int unlocked_count() const noexcept { return unlocked_count_; }
  [[nodiscard]] std::int64_t total_reserve() const noexcept { return total_reserve_; }

 private:
  std::vector<CardAlbumCard> cards_;
  CardAlbumResult last_result_ = CardAlbumResult::kOk;
  std::uint32_t arrival_ = 0;
  bool dropper_known_ = false;
  int unlocked_count_ = 0;
  std::int64_t total_reserve_ = 0;
};

}  // namespace ro::sim
