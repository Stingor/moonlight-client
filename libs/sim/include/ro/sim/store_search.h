#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/inventory.h"
#include "ro/sim/vending.h"

namespace ro::sim {

// LA RECHERCHE D'ÉCHOPPES : un objet de catalogue l'ouvre côté serveur, qui
// accorde un nombre d'essais ; chaque recherche en coûte un et rapporte, page
// par page, les échoppes qui vendent — ou achètent — les objets nommés.
//
// 🔴 LE CLIENT N'OUVRE RIEN LUI-MÊME : c'est l'annonce du serveur qui ouvre la
// fenêtre. Et c'est la fermeture DITE au serveur qui lui fait oublier les
// résultats, ainsi que les essais qui restaient.

// Le serveur envoie les résultats par pages de dix.
inline constexpr std::size_t kStoreSearchResultsPerPage = 10;

// Le client refuse de chercher plus de vingt identifiants à la fois, objets et
// cartes ensemble, et le dit par le message du serveur « trop de résultats ».
inline constexpr std::size_t kStoreSearchMostIds = 20;

// Ce qu'un clic sur un résultat fait, selon l'objet qui a ouvert la recherche.
enum class StoreSearchEffect : std::uint8_t {
  // Le serveur répond par la position de l'échoppe sur la carte.
  kShowPosition = 0,
  // Le serveur ouvre l'échoppe à distance : sa liste arrive comme celle d'une
  // visite. Le client demande confirmation avant le clic.
  kOpenRemotely = 1,
};

// Les échoppes que l'on cherche.
enum class StoreSearchKind : std::uint8_t {
  kVending = 0,
  kBuying = 1,
};

// LES REFUS DU SERVEUR, par leur code sur le fil.
inline constexpr std::uint8_t kStoreSearchNothingFound = 0;
inline constexpr std::uint8_t kStoreSearchTooManyResults = 1;
inline constexpr std::uint8_t kStoreSearchNoUsesLeft = 2;
inline constexpr std::uint8_t kStoreSearchTooSoon = 3;
// Le clic sur un résultat : l'échoppe a fermé, a changé, ou ne propose plus
// l'objet.
inline constexpr std::uint8_t kStoreSearchStoreUnavailable = 4;

// UN RÉSULTAT : une ligne d'une échoppe.
//
// `item` porte l'objet tel qu'il se vend : identifiant, famille, quantité,
// cartes, options, raffinement et grade. Il est toujours identifié.
struct StoreSearchResult {
  // L'identifiant de l'échoppe, que le clic renvoie avec le compte du
  // marchand. ⚠ CE N'EST PAS UN ACTEUR.
  std::uint32_t store_id = 0;
  // Le compte du marchand : celui de son acteur et de son enseigne.
  std::uint32_t owner_aid = 0;
  // Le titre de l'échoppe, en UTF-8.
  std::string shop_name;
  InventoryItem item;
  std::uint32_t price_zeny = 0;
};

// LA CASE D'UNE ÉCHOPPE TROUVÉE, que la minimap marque.
struct StoreSearchCell {
  std::int16_t x = 0;
  std::int16_t y = 0;
};

class StoreSearch {
 public:
  // ── CE QUE LE SERVEUR A DIT ───────────────────────────────────────────────
  [[nodiscard]] bool open() const noexcept { return open_; }
  // Change à chaque ouverture : c'est lui qui remet la fenêtre à zéro.
  [[nodiscard]] std::uint32_t opening_seq() const noexcept { return opening_seq_; }
  [[nodiscard]] StoreSearchEffect effect() const noexcept { return effect_; }
  [[nodiscard]] int remaining_uses() const noexcept { return remaining_uses_; }

  // Tous les résultats reçus, pages successives mises bout à bout.
  [[nodiscard]] std::span<const StoreSearchResult> results() const noexcept { return results_; }
  // Le serveur a d'autres pages à donner.
  [[nodiscard]] bool more_on_server() const noexcept { return more_on_server_; }
  // Change quand une recherche neuve répond, ou n'a rien trouvé : la fenêtre
  // revient à sa première page.
  [[nodiscard]] std::uint32_t first_page_seq() const noexcept { return first_page_seq_; }
  // Change à chaque réponse ou refus du serveur : la recherche n'est plus en
  // route.
  [[nodiscard]] std::uint32_t answer_seq() const noexcept { return answer_seq_; }

  // Le dernier refus, et son rang.
  [[nodiscard]] std::uint8_t failure() const noexcept { return failure_; }
  [[nodiscard]] std::uint32_t failure_seq() const noexcept { return failure_seq_; }
  // LE REFUS QUI TIENT ENCORE : le dernier, tant qu'aucune page ne l'a suivi.
  // ⚠ LE SERVEUR FAIT SOUVENT SUIVRE UN REFUS D'UNE PAGE — « rien trouvé » puis
  // le compte d'essais, « trop de résultats » puis les premiers d'entre eux —,
  // et c'est elle qui l'efface du pied de la fenêtre.
  [[nodiscard]] std::optional<std::uint8_t> standing_failure() const noexcept {
    return standing_failure_;
  }

  // LE MARCHAND DONT L'ENSEIGNE SE PEINT EN JAUNE PÂLE : celui du dernier
  // résultat cliqué ; zéro s'il n'y en a pas.
  [[nodiscard]] std::uint32_t spotlit_owner_aid() const noexcept { return spotlit_owner_aid_; }
  // La case que la minimap marque, s'il y en a une.
  [[nodiscard]] std::optional<StoreSearchCell> marker() const noexcept { return marker_; }

  // ── LES PAQUETS REÇUS ───────────────────────────────────────────────────
  //
  // ⚠ HORS D'UNE RECHERCHE OUVERTE, une page, un refus ou une position sont
  // IGNORÉS : arrivés après la fermeture, ils rallumeraient un marqueur que
  // plus rien n'éteint.
  //
  // L'ouverture repart de rien : résultats, enseigne et marqueur d'une
  // recherche précédente s'effacent.
  void TellOpened(StoreSearchEffect effect, int remaining_uses);
  // Une page de résultats. `first_page` efface les précédents ; sinon elle
  // s'ajoute à leur suite.
  void TellResults(bool first_page, bool more_on_server, int remaining_uses,
                   std::vector<StoreSearchResult> page);
  // ⚠ « RIEN TROUVÉ » EFFACE LES RÉSULTATS et décompte un essai : le client
  // d'origine le fait avant même que le serveur ne redise le compte. Un clic
  // refusé éteint l'enseigne et le marqueur.
  void TellFailure(std::uint8_t reason);
  // La case de l'échoppe cliquée. Une case négative — l'échoppe est sur une
  // autre carte — efface le marqueur.
  void TellStorePosition(std::int16_t x, std::int16_t y);

  // ── CE QUE LE CLIENT A ENVOYÉ ───────────────────────────────────────────
  // Le clic sur un résultat : le marqueur s'efface, et l'enseigne de son
  // marchand s'allume, avant même la réponse.
  void NoteClickSent(std::uint32_t owner_aid);
  // La fermeture, dite au serveur : résultats, enseigne et marqueur s'effacent.
  void Close();
  // Le serveur ferme lui-même la recherche quand on quitte la carte : rien ne
  // se dit.
  void CloseForMapChange();

 private:
  bool open_ = false;
  std::uint32_t opening_seq_ = 0;
  StoreSearchEffect effect_ = StoreSearchEffect::kShowPosition;
  int remaining_uses_ = 0;
  std::vector<StoreSearchResult> results_;
  bool more_on_server_ = false;
  std::uint32_t first_page_seq_ = 0;
  std::uint32_t answer_seq_ = 0;
  std::uint8_t failure_ = 0;
  std::uint32_t failure_seq_ = 0;
  std::optional<std::uint8_t> standing_failure_;
  std::uint32_t spotlit_owner_aid_ = 0;
  std::optional<StoreSearchCell> marker_;
};

// ── LA SAISIE, JUGÉE AVANT L'ENVOI ──────────────────────────────────────────
//
// Le client d'origine résout lui-même les NOMS en identifiants, dans sa table
// d'objets : le serveur ne reçoit que des numéros. Ces contrôles sont les
// siens, dans son ordre.

// UNE ENTRÉE DE LA TABLE DES OBJETS, telle que la recherche la parcourt.
struct StoreSearchCatalogueEntry {
  std::uint32_t item_id = 0;
  // Le nom identifié.
  std::string_view name;
  // Le mot que la carte pose sur l'objet qui la porte ; vide si elle n'en a
  // pas, ou si l'objet n'est pas une carte.
  std::string_view card_word;
  // L'objet figure dans la table des cartes.
  bool card = false;
};

// LE NOM CORRESPOND-IL ? « Objets similaires » coché, le nom qui CONTIENT la
// saisie suffit ; sinon il doit lui être égal. Casse comprise, dans les deux
// cas.
[[nodiscard]] bool StoreSearchNameMatches(std::string_view name, std::string_view query,
                                          bool similar);

// LES IDENTIFIANTS QUE CETTE SAISIE DÉSIGNE, dans l'ordre croissant. Un objet
// se retient par son nom, puis, à défaut, par le mot de sa carte.
// `cards_only` ne parcourt que les cartes. ⚠ LE CATALOGUE DOIT ÊTRE TRIÉ par
// identifiant : l'ordre des résultats est le sien.
[[nodiscard]] std::vector<std::uint32_t> FindStoreSearchIds(
    std::span<const StoreSearchCatalogueEntry> catalogue, std::string_view query, bool cards_only,
    bool similar);

// UN PRIX SAISI : vide vaut zéro ; sinon le texte doit être l'écriture
// décimale exacte d'un entier signé de 32 bits — ni zéro de tête, ni signe
// plus, ni séparateur. Rien s'il ne l'est pas.
[[nodiscard]] std::optional<std::int32_t> ReadStoreSearchPrice(std::string_view text);

// POURQUOI UNE RECHERCHE NE PART PAS.
enum class StoreSearchFault : std::uint8_t {
  kNone,
  // Aucun nom d'objet.
  kNoItemName,
  // Le nom ne désigne aucun objet.
  kUnknownItem,
  // Une carte est saisie, et ne désigne rien.
  kUnknownCard,
  // Plus de `kStoreSearchMostIds` identifiants.
  kTooManyIds,
  // Un prix illisible, ou un minimum au-dessus d'un maximum saisi.
  kBadPrice,
};

struct StoreSearchForm {
  StoreSearchKind kind = StoreSearchKind::kVending;
  std::string_view item_name;
  std::string_view card_name;
  std::string_view min_price;
  std::string_view max_price;
  bool similar = true;
};

// CE QUI PARTIRA, une fois la saisie jugée.
struct StoreSearchPlan {
  StoreSearchFault fault = StoreSearchFault::kNone;
  StoreSearchKind kind = StoreSearchKind::kVending;
  std::vector<std::uint32_t> item_ids;
  std::vector<std::uint32_t> card_ids;
  // ⚠ UN PRIX NÉGATIF PART TEL QUEL, réinterprété sans signe : c'est ce que fait
  // le client d'origine. Le serveur jette alors la demande en silence, comme
  // tout prix au-delà de `kVendingMostPriceZeny`.
  //
  // ⚠ UN MINIMUM SEUL part avec `kVendingMostPriceZeny` pour maximum, et non
  // zéro : le serveur échange les deux bornes AVANT de lire zéro comme « sans
  // borne », et chercherait de zéro au minimum. Le plafond est le plus haut
  // prix qu'il accepte, donc il ne borne rien.
  std::uint32_t min_price_zeny = 0;
  std::uint32_t max_price_zeny = 0;
};

[[nodiscard]] StoreSearchPlan PlanStoreSearch(const StoreSearchForm& form,
                                              std::span<const StoreSearchCatalogueEntry> catalogue);

// LES RÉSULTATS D'UNE PAGE, comptée depuis un ; vide au-delà.
[[nodiscard]] std::span<const StoreSearchResult> StoreSearchPage(
    std::span<const StoreSearchResult> results, int page);

// La page suivante se montre-t-elle sans rien demander ?
[[nodiscard]] bool StoreSearchPageHeld(std::size_t result_count, int page);

// LA PAGE SUIVANTE SE DEMANDE-T-ELLE AU SERVEUR ? Seulement s'il en annonce une,
// et depuis la DERNIÈRE page reçue, pleine : une demande déjà partie avance la
// page montrée au-delà de ce qui est reçu, et rien ne repart avant la réponse.
[[nodiscard]] bool StoreSearchMayAskNextPage(std::size_t result_count, int page,
                                             bool more_on_server);

// ── LA FENÊTRE, À CHAQUE IMAGE ──────────────────────────────────────────────

// Ce que l'étage sait de la fenêtre et du modèle.
struct StoreSearchWindowFacts {
  // La recherche est ouverte côté modèle.
  bool search_open = false;
  // La fenêtre montre l'ouverture en cours du modèle.
  bool shows_current_opening = false;
  // La fenêtre est ouverte : sa croix ou Échap la baisse.
  bool window_open = false;
  // La fenêtre a demandé sa fermeture.
  bool close_asked = false;
};

enum class StoreSearchWindowStep : std::uint8_t {
  // Rien à montrer : la recherche est fermée — par nous, ou par le serveur au
  // changement de carte. ⚠ AUCUNE FERMETURE NE SE DIT ICI.
  kHide,
  // Une ouverture neuve : la fenêtre se remet à neuf.
  kReopen,
  // Le joueur ferme : la fermeture se dit, UNE fois, puisqu'elle ferme le
  // modèle.
  kSendClose,
  // La fenêtre suit son cours.
  kFollow,
};

[[nodiscard]] StoreSearchWindowStep NextStoreSearchWindowStep(const StoreSearchWindowFacts& facts);

}  // namespace ro::sim
