#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ro::sim {

// LA BOUTIQUE DE VOTE : ce que le serveur en dit, et rien de ce qu'on en montre.
//
// Le protocole la nomme « cash shop » ; ce serveur y vend contre les points que
// rapportent les votes des joueurs, et sa table de messages l'appelle « Vote
// Shop ». Les deux monnaies sont les points de vote et les points d'événement —
// la seconde est celle que le protocole appelle « kafra ».
//
// 🔴 LE CLIENT N'OUVRE JAMAIS LA BOUTIQUE LUI-MÊME. Il la DEMANDE, et c'est la
// réponse du serveur qui l'ouvre — ou qui la referme si elle était ouverte : le
// paquet d'ouverture BASCULE, comme celui de la banque.
//
// 🔴 ET TOUTE FERMETURE SE DIT AU SERVEUR. L'ouverture pose au personnage l'état
// d'une boutique de PNJ, qui lui interdit de marcher et d'attaquer ; seul le
// paquet de fermeture le lève. D'où `TakeFarewell`, que le monde ne peut pas
// envoyer lui-même.

// Les onglets du serveur, dans son ordre : nouveautés, populaires, limités,
// location, permanents, parchemins, consommables, divers, soldes.
inline constexpr std::size_t kCashShopTabCount = 9;

// Ce que le serveur accepte au plus d'un même item dans un achat : au-delà, il
// refuse l'achat ENTIER.
inline constexpr std::uint32_t kCashShopMostPerItem = 99;

// Le résultat d'achat qui dit « acheté ». Les autres codes sont des refus, que
// l'affichage traduit.
inline constexpr std::uint16_t kCashShopBuySuccess = 0;

// Un item du catalogue.
struct CashShopItem {
  std::uint32_t item_id = 0;
  // En points. ⚠ SIGNÉ, comme le fil le lit chez le serveur.
  std::int32_t price = 0;
  // L'apparence portée et l'emplacement d'équipement : ce qui permet de ranger
  // un costume par emplacement, et de le montrer porté.
  std::uint16_t view_sprite = 0;
  std::uint32_t equip_location = 0;
};

class CashShop {
 public:
  // ── L'OUVERTURE ───────────────────────────────────────────────────────────
  [[nodiscard]] bool open() const noexcept { return open_; }
  // Le rang de l'ouverture : il change à chaque ouverture neuve, et c'est lui
  // qui remet la fenêtre à zéro.
  [[nodiscard]] std::uint32_t opening_seq() const noexcept { return opening_seq_; }
  // L'onglet que le serveur a nommé à l'ouverture.
  [[nodiscard]] std::uint32_t opening_tab() const noexcept { return opening_tab_; }

  // La réponse du serveur à la demande d'ouverture. ⚠ ELLE BASCULE : arrivée
  // boutique ouverte, elle la ferme — et les points qu'elle porte ne sont pas
  // repris, pas plus que chez le client d'origine.
  void TellOpening(std::uint32_t vote_points, std::uint32_t event_points, std::uint32_t tab);

  // Le joueur ferme. La fermeture se dit au serveur : voir `TakeFarewell`.
  void Close() noexcept;
  // Le personnage change de carte sur le même serveur : la boutique se ferme,
  // et ⚠ LA FERMETURE SE DIT QUAND MÊME. Le serveur lève le blocage de lui-même
  // en quittant la carte, mais garde la boutique pour ouverte, et refuse alors
  // `@cash` et `@points` jusqu'à la prochaine fermeture.
  void CloseForMapChange() noexcept;
  // Le personnage change de serveur : la session repart de zéro de l'autre
  // côté, et il n'y a plus personne à qui dire au revoir.
  void CloseSilently() noexcept;

  // Vrai UNE fois après chaque fermeture qui doit se dire au serveur.
  [[nodiscard]] bool TakeFarewell() noexcept;

  // ── LES POINTS ────────────────────────────────────────────────────────────
  [[nodiscard]] std::uint32_t vote_points() const noexcept { return vote_points_; }
  [[nodiscard]] std::uint32_t event_points() const noexcept { return event_points_; }

  // ── LE CATALOGUE ──────────────────────────────────────────────────────────
  //
  // 🔴 UN ONGLET PEUT ARRIVER EN PLUSIEURS PAQUETS : le serveur coupe une liste
  // trop longue pour une trame, et les morceaux se suivent sous le même onglet.
  // Le premier paquet d'un onglet le vide ; les suivants s'y AJOUTENT, tant que
  // l'onglet ne change pas. Un onglet hors de la liste est ignoré.
  void TellCatalogue(std::uint16_t tab, std::span<const CashShopItem> items);

  // Une salve de catalogue va suivre : le premier paquet de chaque onglet le
  // videra, même s'il nomme l'onglet que la salve d'avant a fini.
  void ExpectCatalogue() noexcept { last_catalogue_tab_ = kCashShopTabCount; }

  // Les items d'un onglet, vides pour un onglet inconnu.
  [[nodiscard]] std::span<const CashShopItem> tab(std::size_t tab_index) const noexcept;
  // Le rang du catalogue : il change à chaque paquet reçu.
  [[nodiscard]] std::uint32_t catalogue_seq() const noexcept { return catalogue_seq_; }

  // ── L'ACHAT ───────────────────────────────────────────────────────────────
  //
  // Le résultat porte les soldes à jour, succès ou refus.
  void TellPurchase(std::uint16_t result, std::uint32_t vote_points, std::uint32_t event_points);
  // Le dernier résultat, et son rang. ⚠ ZÉRO VEUT DIRE « AUCUN ACHAT ENCORE » :
  // le résultat vaut alors `kCashShopBuySuccess`, et c'est le rang qui sépare
  // les deux.
  [[nodiscard]] std::uint16_t purchase_result() const noexcept { return purchase_result_; }
  [[nodiscard]] std::uint32_t purchase_seq() const noexcept { return purchase_seq_; }

 private:
  bool open_ = false;
  std::uint32_t opening_seq_ = 0;
  std::uint32_t opening_tab_ = 0;
  bool farewell_due_ = false;
  std::uint32_t vote_points_ = 0;
  std::uint32_t event_points_ = 0;
  std::array<std::vector<CashShopItem>, kCashShopTabCount> tabs_;
  // L'onglet du dernier paquet de catalogue, pour savoir si le suivant le
  // continue. ⚠ REMIS À « AUCUN » À CHAQUE OUVERTURE : une nouvelle salve
  // recommence chaque onglet.
  std::size_t last_catalogue_tab_ = kCashShopTabCount;
  std::uint32_t catalogue_seq_ = 0;
  std::uint16_t purchase_result_ = kCashShopBuySuccess;
  std::uint32_t purchase_seq_ = 0;
};

}  // namespace ro::sim
