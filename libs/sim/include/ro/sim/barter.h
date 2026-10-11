#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace ro::sim {

class Inventory;
struct PlayerStatus;

// LE TROC : une boutique de PNJ qui se paie EN OBJETS, et parfois en zeny par
// surcroît.
//
// ⚠ DEUX GÉNÉRATIONS, que le serveur choisit boutique par boutique et que seul
// le paquet d'ouverture dit : la SIMPLE porte une seule devise par article ;
// l'ÉTENDUE ajoute un prix en zeny, un raffinement de l'objet livré, et
// plusieurs devises dont chacune peut exiger un raffinement. Le modèle les
// range sous une même forme — une liste de devises, qui n'a qu'une entrée en
// simple — et retient la génération, parce que l'achat et la fermeture ont
// chacun deux paquets.
//
// 🔴 LE CLIENT N'OUVRE JAMAIS LE TROC LUI-MÊME : c'est la liste du serveur qui
// l'ouvre, et rien d'autre.
//
// 🔴 TOUTE FERMETURE DU JOUEUR SE DIT AU SERVEUR. L'ouverture lui arme un état
// de troc ouvert (`barter_open`, `barter_extended_open`) qui interdit de marcher,
// et `clif_barter_open` REFUSE DE ROUVRIR tant qu'il est armé : seul le paquet
// de fermeture de LA MÊME génération le désarme. Sans lui, le PNJ ne répond
// plus jusqu'au prochain changement de carte. D'où `TakeFarewell`.
//
// 🔴 L'ACHAT FERME LE TROC. Le client d'origine enchaîne l'envoi de l'achat sur
// la fermeture de toute la paire de fenêtres, qui émet la fermeture au
// serveur : le résultat arrive donc boutique FERMÉE. Le modèle le reproduit —
// `MarkPurchaseSent` ferme et laisse un résultat ATTENDU, et c'est cette attente,
// pas l'ouverture, qui revendique le résultat. Il n'existe AUCUNE redemande de
// la liste : la revoir, c'est reparler au PNJ, qui la renvoie avec le stock du
// moment.

enum class BarterGeneration : std::uint8_t {
  kSimple,
  kExpanded,
};

// Le stock d'un article que rien ne borne.
inline constexpr std::uint32_t kBarterUnlimitedStock = 0xFFFFFFFF;

// Le raffinement exigé d'une devise qui n'en exige aucun.
//
// ⚠ ZÉRO VEUT AUSSI DIRE « EXACTEMENT +0 » : le serveur écrit zéro pour les deux.
// Le client compte donc tout exemplaire comme bon, et le serveur, s'il voulait
// +0, refuse avec son propre code.
inline constexpr std::uint16_t kBarterAnyRefine = 0;

// La plus grande quantité d'un article au panier : MAX_AMOUNT du serveur, et le
// plafond du client d'origine (qui l'écrit dans son message).
inline constexpr std::uint32_t kBarterMostPerLine = 30000;

// ── LE PLAFOND DE TRAME ─────────────────────────────────────────────────────
//
// Le client d'origine refuse d'émettre un achat de plus de 0x800 octets. Un en-
// tête de quatre, puis une ligne par article du panier : quatorze octets en
// simple, douze en étendu. `net` vérifie que ces tailles sont celles du fil.
inline constexpr std::size_t kBarterMostPurchaseBytes = 0x800;
inline constexpr std::size_t kBarterPurchaseHeaderBytes = 4;
inline constexpr std::size_t kBarterSimpleLineBytes = 14;
inline constexpr std::size_t kBarterExpandedLineBytes = 12;
inline constexpr std::size_t kBarterMostSimpleLines =
    (kBarterMostPurchaseBytes - kBarterPurchaseHeaderBytes) / kBarterSimpleLineBytes;
inline constexpr std::size_t kBarterMostExpandedLines =
    (kBarterMostPurchaseBytes - kBarterPurchaseHeaderBytes) / kBarterExpandedLineBytes;

// ── LES CODES DU RÉSULTAT (`e_purchase_result`, `src/map/clif.hpp`) ──────────
//
// Ce sont ceux du résultat d'achat d'une boutique de PNJ à zeny : même paquet.
//
// 🔴 LE SERVEUR RÉPOND ZÉRO AU SUCCÈS D'UN TROC, pas « troc réussi » :
// `npc_barter_purchase` rend `PURCHASE_SUCCEED`. Le code 12 existe dans
// l'énumération et n'est jamais émis ; on le tient pour un succès quand même.
inline constexpr std::uint8_t kBarterResultSucceeded = 0;
inline constexpr std::uint8_t kBarterResultNotEnoughZeny = 1;
inline constexpr std::uint8_t kBarterResultOverWeight = 2;
// Pile au-delà de MAX_AMOUNT une fois ajoutée au sac, ou cases libres
// insuffisantes.
inline constexpr std::uint8_t kBarterResultTooManyItems = 3;
inline constexpr std::uint8_t kBarterResultFailed = 11;
inline constexpr std::uint8_t kBarterResultExchanged = 12;
inline constexpr std::uint8_t kBarterResultSoldOut = 13;
inline constexpr std::uint8_t kBarterResultNotEnoughGoods = 14;

[[nodiscard]] constexpr bool BarterResultSucceeded(std::uint8_t code) noexcept {
  return code == kBarterResultSucceeded || code == kBarterResultExchanged;
}

// Une devise : ce qu'il faut rendre pour UN exemplaire de l'article.
struct BarterCurrency {
  std::uint32_t item_id = 0;
  // Voir `kBarterAnyRefine`. Toujours quelconque en simple.
  std::uint16_t refine_level = kBarterAnyRefine;
  std::uint32_t amount = 0;
  // La famille repliée (`itemtype()`). ⚠ LE TROC SIMPLE NE LA DIT PAS : elle vaut
  // alors zéro, et c'est l'inventaire qui tranche ce qui s'empile.
  std::uint16_t type = 0;
};

// Un article de la boutique.
struct BarterItem {
  // L'identifiant que le serveur montre — l'apparence quand l'objet en a une.
  std::uint32_t item_id = 0;
  // La famille repliée : c'est elle qui dit si l'article s'empile.
  std::uint16_t type = 0;
  // Ce qui reste chez le PNJ À L'OUVERTURE. Voir `kBarterUnlimitedStock`.
  std::uint32_t stock = 0;
  // Le poids d'UN exemplaire, en dixièmes d'unité.
  std::uint32_t weight = 0;
  // L'index de l'article DANS LA BOUTIQUE : c'est lui que l'achat renvoie, et
  // le seul champ de la ligne que le serveur lit avec la quantité.
  std::uint32_t shop_index = 0;
  std::uint16_t view_sprite = 0;
  std::uint32_t equip_location = 0;
  // Étendu seulement : le prix en zeny d'un exemplaire, en plus des devises, et
  // le raffinement de l'objet livré.
  std::uint32_t zeny = 0;
  std::uint32_t refine_level = 0;
  // Une seule en simple ; zéro, une ou plusieurs en étendu.
  std::vector<BarterCurrency> currencies;

  [[nodiscard]] bool unlimited() const noexcept { return stock == kBarterUnlimitedStock; }
};

// Une ligne du panier. ⚠ UNE LIGNE PAR ARTICLE : ajouter un article déjà au
// panier augmente sa ligne. Le serveur refuse en silence un index répété.
struct BarterCartLine {
  std::uint32_t shop_index = 0;
  std::uint32_t amount = 0;
};

class Barter {
 public:
  // ── L'OUVERTURE ───────────────────────────────────────────────────────────
  [[nodiscard]] bool open() const noexcept { return open_; }
  // La génération de la dernière ouverture.
  [[nodiscard]] BarterGeneration generation() const noexcept { return generation_; }
  // Change à chaque ouverture : c'est lui qui remet la fenêtre à zéro.
  [[nodiscard]] std::uint32_t opening_seq() const noexcept { return opening_seq_; }

  // La liste du serveur. Elle ouvre le troc et VIDE le panier.
  //
  // ⚠ UNE LISTE DE L'AUTRE GÉNÉRATION, arrivée troc ouvert, laisse armé chez le
  // serveur l'état de la première : sa fermeture est donc due.
  void TellOpening(BarterGeneration generation, std::vector<BarterItem> items);

  // Les articles de la dernière ouverture ; ils restent lisibles après la
  // fermeture, le temps que le résultat d'un achat arrive.
  [[nodiscard]] std::span<const BarterItem> items() const noexcept { return items_; }
  [[nodiscard]] const BarterItem* FindItem(std::uint32_t shop_index) const noexcept;

  // ── LE PANIER, TENU PAR LE CLIENT ─────────────────────────────────────────
  //
  // Le serveur n'en sait rien avant l'achat. Ces verbes ne VALIDENT RIEN au-delà
  // de l'existence de l'article : c'est le rôle de `CheckBarterAddition`, à
  // interroger avant.
  [[nodiscard]] std::span<const BarterCartLine> cart() const noexcept { return cart_; }
  // Change à chaque modification du panier.
  [[nodiscard]] std::uint32_t cart_seq() const noexcept { return cart_seq_; }
  // Combien de cet article sont au panier.
  [[nodiscard]] std::uint32_t InCart(std::uint32_t shop_index) const noexcept;
  // Ce que le joueur peut encore mettre au panier : le stock moins le panier,
  // comme le client d'origine l'affiche. `kBarterUnlimitedStock` si illimité.
  [[nodiscard]] std::uint32_t RemainingStock(const BarterItem& item) const noexcept;

  // Ajoute au panier. Faux si le troc est fermé, l'article inconnu ou la
  // quantité nulle.
  bool PutInCart(std::uint32_t shop_index, std::uint32_t amount);
  // Fixe la quantité d'une ligne ; zéro la retire. Faux si la ligne n'existe pas.
  bool SetCartAmount(std::uint32_t shop_index, std::uint32_t amount);
  void ClearCart() noexcept;

  // ── L'ACHAT ───────────────────────────────────────────────────────────────
  //
  // L'achat vient de partir : le troc se FERME, sa fermeture est due, et le
  // prochain résultat lui revient. Le panier envoyé est gardé pour le message.
  void MarkPurchaseSent();
  // Le prochain résultat d'achat revient-il au troc ?
  //
  // ⚠ C'EST LUI QUI DÉPARTAGE LE RÉSULTAT, que la boutique à zeny reçoit par le
  // même paquet. Levé à la réception, et à tout changement de carte : un achat
  // arrivé chez le serveur après son changement de carte n'a pas de réponse.
  [[nodiscard]] bool awaiting_result() const noexcept { return awaiting_result_; }
  [[nodiscard]] std::span<const BarterCartLine> sent_cart() const noexcept { return sent_cart_; }

  void TellResult(std::uint8_t code);
  // Le dernier code, et son rang. ⚠ ZÉRO VEUT DIRE « AUCUN RÉSULTAT ENCORE » pour
  // le rang, et « réussi » pour le code : c'est le rang qui sépare les deux.
  [[nodiscard]] std::uint8_t result() const noexcept { return result_; }
  [[nodiscard]] std::uint32_t result_seq() const noexcept { return result_seq_; }

  // ── LA FERMETURE ──────────────────────────────────────────────────────────
  //
  // Le joueur ferme : la fermeture se dit au serveur.
  void Close();
  // Le personnage change de carte : ⚠ RIEN À DIRE. `pc_setpos` désarme lui-même
  // les deux états de troc, et une fermeture arrivée ensuite ne ferait rien — ou
  // fermerait un troc rouvert entre-temps. Les fermetures encore dues tombent.
  void CloseForMapChange();
  // Le personnage change de serveur : la session repart de zéro. Même effet.
  void CloseSilently();

  // La prochaine génération dont la fermeture est due, UNE fois chacune. À
  // appeler jusqu'à ce qu'elle rende vide : les deux peuvent l'être.
  [[nodiscard]] std::optional<BarterGeneration> TakeFarewell() noexcept;

 private:
  void DropEverythingOpen();

  bool open_ = false;
  BarterGeneration generation_ = BarterGeneration::kSimple;
  std::uint32_t opening_seq_ = 0;
  std::vector<BarterItem> items_;
  std::vector<BarterCartLine> cart_;
  std::uint32_t cart_seq_ = 0;
  bool awaiting_result_ = false;
  std::vector<BarterCartLine> sent_cart_;
  std::uint8_t result_ = kBarterResultSucceeded;
  std::uint32_t result_seq_ = 0;
  bool simple_farewell_due_ = false;
  bool expanded_farewell_due_ = false;
};

// ── LA VALIDATION DU PANIER ─────────────────────────────────────────────────
//
// Ce que le client refuse AVANT d'émettre : les six gardes du client d'origine,
// et ce que le serveur rejette EN SILENCE — un achat qui n'aurait jamais de
// réponse. Ce que le serveur refuse avec un code (stock, devises, zeny) est
// aussi vérifié ici, parce que le joueur le lit mieux avant qu'après.
enum class BarterCartRefusal : std::uint8_t {
  kNone,
  // Aucun troc ouvert. (Serveur : silence.)
  kNotOpen,
  // Panier vide. (Serveur : silence.)
  kEmptyCart,
  // Plus de lignes que la trame d'achat n'en porte.
  kTooManyLines,
  // Un index que la boutique ne connaît pas. (Serveur : silence.)
  kUnknownItem,
  // Une quantité nulle.
  kZeroAmount,
  // Deux lignes pour le même objet livré, sous deux index. (Serveur : silence.)
  kSameItemTwice,
  // Un équipement — qui ne s'empile pas — déjà au panier, qu'on y remet.
  kGearAlreadyInCart,
  // Un équipement demandé à plus d'un exemplaire.
  kGearOneAtATime,
  // Une ligne au-delà de `kBarterMostPerLine`.
  kOverLineLimit,
  // Plus que le stock du PNJ.
  kOutOfStock,
  // Le poids du panier dépasse ce que le personnage peut encore porter.
  kOverWeight,
  // La devise manque, mais le joueur la PORTE : il suffit de l'ôter.
  kCurrencyWorn,
  // La devise manque, mais le joueur l'a rangée dans ses FAVORIS, que le serveur
  // écarte (`hide_fav_sell`).
  kCurrencyFavorite,
  kNotEnoughCurrency,
  // Étendu : le zeny du panier dépasse celui du personnage.
  kNotEnoughZeny,
};

struct BarterCartCheck {
  BarterCartRefusal refusal = BarterCartRefusal::kNone;
  // La ligne du panier en cause (dans le panier ÉVALUÉ : pour une addition, la
  // ligne nouvelle est la dernière).
  std::size_t cart_line = 0;
  // L'objet en cause : l'article, ou la DEVISE qui manque.
  std::uint32_t item_id = 0;
  // Pour un refus de poids seulement, en dixièmes : la charge qu'aurait le
  // personnage après l'échange, devises retranchées, et ce qu'il peut porter.
  std::int64_t load_after_tenths = 0;
  std::int64_t max_load_tenths = 0;

  [[nodiscard]] bool passed() const noexcept { return refusal == BarterCartRefusal::kNone; }
};

// LE POIDS D'UNE UNITÉ D'UN ITEM, en dixièmes, tel que la table des items du
// serveur le donne ; rien quand l'item n'y figure pas. Le fil ne dit pas le
// poids d'une devise : c'est `app` qui le lit dans l'installation.
using BarterUnitWeight = std::function<std::optional<std::uint32_t>(std::uint32_t item_id)>;

// Ce que le serveur rejetterait EN SILENCE, sans rien d'autre que le troc : troc
// fermé, panier vide ou trop long, article inconnu, quantité nulle, même objet
// deux fois. C'est la garde du verbe d'achat.
[[nodiscard]] BarterCartCheck CheckBarterCartShape(const Barter& barter);

// Tout le panier, contre le sac et la fiche du joueur.
//
// 🔴 LE POIDS SE COMPTE COMME LE SERVEUR LE COMPTE : charge courante, plus le
// poids du panier, MOINS celui des devises qui partent, à la quantité payée. Le
// panier entier se juge d'un bloc — un échange lourd contre léger passe à la
// limite de charge, comme au serveur — et la ligne désignée est la première où
// le cumul déborde. Une devise que `unit_weight` ne pèse pas ne retranche rien :
// le contrôle reste du côté du refus, et le serveur garde le dernier mot. Un
// poids, un zeny inconnus ne refusent rien.
//
// ⚠ LES DEVISES SE COMPTENT COMME LE SERVEUR LES CHERCHE, besoins de toutes les
// lignes cumulés, sans les objets portés ni les favoris : une devise qui
// s'empile se prend dans la PREMIÈRE pile qui convient, et seulement là ; une
// devise qui ne s'empile pas se prend exemplaire par exemplaire, au raffinement
// exigé OU PLUS HAUT. Le compte est nécessaire, pas suffisant : le serveur sert
// les lignes dans l'ordre et peut prendre, pour une devise « quelconque », un
// exemplaire raffiné qu'une ligne suivante exigeait. Il le dit alors par son
// code.
[[nodiscard]] BarterCartCheck CheckBarterCart(const Barter& barter, const Inventory& inventory,
                                              const PlayerStatus& status,
                                              const BarterUnitWeight& unit_weight = {});

// Le panier tel qu'il serait après l'ajout de `amount` exemplaires de l'article.
// C'est ce que le client d'origine demande avant de mettre au panier.
[[nodiscard]] BarterCartCheck CheckBarterAddition(const Barter& barter, std::uint32_t shop_index,
                                                  std::uint32_t amount, const Inventory& inventory,
                                                  const PlayerStatus& status,
                                                  const BarterUnitWeight& unit_weight = {});

// ── CE QUE LE JOUEUR TIENT D'UNE DEVISE ─────────────────────────────────────
//
// Ce qu'une fenêtre montre à côté du prix : le compte de `CheckBarterCart`, pour
// UN besoin de cette devise à ce raffinement — sans les objets portés ni les
// favoris, la première pile qui convient pour ce qui s'empile, les exemplaires
// au raffinement exigé ou plus haut pour ce qui ne s'empile pas.
struct BarterCurrencyHolding {
  std::uint64_t usable = 0;
  // 🔴 UN EXEMPLAIRE COMPTÉ EST RAFFINÉ AU-DELÀ DE L'EXIGENCE : le serveur peut
  // le prendre (`npc_barter_purchase` cherche plus haut quand l'exact manque, et
  // une exigence « quelconque » accepte tout). C'est ce qu'une confirmation doit
  // dire avant qu'une pièce raffinée parte.
  bool may_take_higher_refine = false;
};

[[nodiscard]] BarterCurrencyHolding HoldBarterCurrency(const Inventory& inventory,
                                                       std::uint32_t item_id,
                                                       std::uint16_t refine_level);

}  // namespace ro::sim
