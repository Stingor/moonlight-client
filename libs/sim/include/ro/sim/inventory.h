#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ro::sim {

// CE QUE LE PERSONNAGE PORTE, RANGE OU TRAÎNE DERRIÈRE LUI.
//
// ⚠ TROIS CONTENANTS, UN SEUL JEU DE PAQUETS. Le sac, la charrette et le
// stockage arrivent par les MÊMES opcodes ; c'est un champ du paquet d'ouverture
// qui dit lequel on remplit. `net` le traduit, `sim` n'en sait rien — il n'y a
// donc pas de valeur de protocole sur cet énuméré, et lui en donner une
// ferait entrer le fil dans une couche qui l'ignore.
enum class ItemHolder : std::uint8_t {
  kInventory,
  kCart,
  kStorage,
};

// Les emplacements de carte d'une pièce d'équipement. MAX_SLOTS, dans
// `src/common/mmo.hpp` du serveur.
inline constexpr std::size_t kItemCardSlots = 4;

// Les options aléatoires que porte une pièce. MAX_ITEM_OPTIONS, même fichier.
inline constexpr std::size_t kItemOptionSlots = 5;

// Une OPTION ALÉATOIRE : un bonus tiré au sort, posé sur une pièce.
//
// ⚠ UNE OPTION D'IDENTIFIANT ZÉRO EST UNE CASE VIDE, et c'est la seule façon de
// les compter. Le serveur écrit toujours les cinq entrées, et met son compteur
// à zéro quel que soit leur nombre — « le client recalcule », dit son code. S'y
// fier donnerait toujours zéro option.
struct ItemOption {
  std::uint16_t id = 0;
  std::int16_t value = 0;
  std::uint8_t param = 0;

  [[nodiscard]] bool filled() const noexcept { return id != 0; }
};

// UN OBJET, tel que le serveur le décrit.
//
// Les noms sont ceux de leur contenu, pas ceux du fil : `wearable_at` et
// `worn_at` s'appellent `location` et `WearState` dans les paquets, et ces deux
// mots-là ne disent pas qu'ils désignent deux choses différentes.
struct InventoryItem {
  // LE NUMÉRO DE CASE VU DU CLIENT, et c'est celui qu'il faut RENVOYER au
  // serveur pour agir sur l'objet.
  //
  // ⚠ CE N'EST PAS L'INDEX DU SERVEUR : il y ajoute deux pour le sac et la
  // charrette, un seul pour le stockage (`client_index` et
  // `client_storage_index`, `src/map/clif.cpp`). Le décalage est asymétrique, on
  // ne le défait pas, et on garde ce que le fil a dit.
  std::uint16_t client_slot = 0;

  // L'identifiant d'objet, celui de la base du serveur et des tables du client.
  std::uint32_t item_id = 0;

  // La FAMILLE, telle que le serveur la replie avant de l'envoyer (`itemtype()`,
  // `src/map/clif.cpp`). C'est ce champ qui range l'objet dans un onglet.
  //
  // ⚠ DEUX VALEURS N'ARRIVENT JAMAIS PAR CE CANAL : le serveur replie l'œuf de
  // familier sur l'armure et l'équipement d'ombre sur l'arme ou l'armure. Les
  // attendre serait attendre pour rien.
  std::uint8_t type = 0;

  // Combien il y en a dans la pile. Une pièce d'équipement ne s'empile pas : sa
  // quantité vaut un, et deux exemplaires sont deux objets.
  std::int16_t amount = 0;

  // OÙ LA PIÈCE PEUT SE PORTER, en drapeaux d'emplacement (les `EQP_*` du
  // serveur). Zéro pour ce qui ne se porte pas.
  std::uint32_t wearable_at = 0;

  // OÙ ELLE EST PORTÉE EN CE MOMENT, mêmes drapeaux. Zéro quand elle ne l'est
  // pas — c'est la seule façon de savoir qu'une pièce est équipée.
  std::uint32_t worn_at = 0;

  // Les cartes serties, zéro pour une case libre.
  //
  // ⚠ SUR UN OBJET FORGÉ, CE NE SONT PAS DES CARTES : les quatre mêmes nombres
  // portent alors l'identité du forgeron et la force de la forge. `ItemIsForged`
  // tranche, et rien d'autre ne le dit.
  std::array<std::uint32_t, kItemCardSlots> cards = {};

  std::array<ItemOption, kItemOptionSlots> options = {};

  // La fin de la location, en secondes depuis l'époque UNIX. Zéro pour un objet
  // que l'on garde.
  std::int32_t rental_until_unix_s = 0;

  // L'APPARENCE que la pièce donne, quand elle en donne une. Le serveur ne
  // l'envoie que pour ce qui se voit.
  std::uint16_t view_id = 0;

  std::uint8_t refine_level = 0;
  std::uint8_t grade = 0;

  // Si la pièce se lie à qui l'équipe : 0 libre, 1 se liera, 2 déjà liée.
  std::uint16_t bind_on_equip = 0;

  bool identified = false;
  bool damaged = false;

  // Rangé par le joueur dans l'onglet des favoris. Le serveur le retient, c'est
  // donc bien un fait du monde et non un réglage d'affichage.
  bool favorite = false;

  [[nodiscard]] bool equipped() const noexcept { return worn_at != 0; }
  [[nodiscard]] bool rented() const noexcept { return rental_until_unix_s != 0; }
};

// L'OBJET EST-IL FORGÉ OU SIGNÉ, auquel cas ses quatre « cartes » sont l'identité
// de son créateur ?
//
// La règle vient du client : une première case non nulle mais petite ne peut pas
// être une carte, les vraies cartes ayant des identifiants bien plus hauts. Un
// objet forgé n'a donc ni cartes à montrer, ni emplacements à compter.
[[nodiscard]] bool ItemIsForged(const InventoryItem& item) noexcept;

// QUI A FORGÉ OU SIGNÉ L'OBJET : l'identifiant de personnage que ses deux
// dernières cases portent, mot bas puis mot haut. Zéro pour un objet qui n'est
// pas forgé.
[[nodiscard]] std::uint32_t ItemCreatorId(const InventoryItem& item) noexcept;
// Le même, depuis les quatre cartes seules — celles d'un lien de conversation.
[[nodiscard]] std::uint32_t CreatorIdOfCards(
    const std::array<std::uint32_t, kItemCardSlots>& cards) noexcept;

// Le PLUS GRAND identifiant de carte que la règle ci-dessus tienne pour une
// donnée de forgeron. C'est la borne du client, reprise telle quelle.
inline constexpr std::uint32_t kHighestForgeMark = 500;

// Ce que vaut `bind_on_equip` quand l'objet est DEJA lie a son porteur. Le
// serveur ne l'annonce que par un paquet a lui, et sans ce nom la valeur deux
// resterait un nombre nu.
inline constexpr std::uint16_t kItemBoundToOwner = 2;

// Combien de cartes sont réellement serties. Zéro sur un objet forgé.
[[nodiscard]] std::size_t ItemCardCount(const InventoryItem& item) noexcept;

// COMBIEN DES `slots` PREMIÈRES CASES PORTENT UNE CARTE — celles qu'un
// sertissage peut encore remplir. Zéro sur un objet forgé.
//
// ⚠ PAS `ItemCardCount` : un enchantement s'écrit dans les cases HAUTES, même
// sur une pièce à deux emplacements, et n'en consomme aucun. Le serveur ne
// cherche une case libre que dans `[0, slots)` ; compter l'enchantement ferait
// passer pour pleine une pièce qu'il accepte encore.
[[nodiscard]] std::size_t ItemCardsInSlots(const InventoryItem& item, std::size_t slots) noexcept;

// LA FAMILLE D'UN OBJET : ce qu'il EST, indépendamment de toute fenêtre.
//
// ⚠ ELLE VIT ICI ET NON DANS L'INTERFACE, et ce n'est pas un rangement de
// commodité : c'est un fait que le fil porte. Une fenêtre qui la calculerait
// elle-même la recalculerait dans chacune — le sac, la charrette, le coffre —
// et les trois dériveraient.
// 🔴 SEPT FAMILLES ET NON CINQ, parce que le COFFRE sépare ce que le sac
// réunit : les armes des armures, le cash des consommables. Ranger la famille
// grossière ici aurait obligé le coffre à refaire le tri sur les types — un
// second découpage, qui dériverait du premier à la première divergence.
//
// ⚠ LES DEUX FENÊTRES À GRILLE VEULENT LE GROS GRAIN, et il se REGROUPE :
// `ItemCategoryIsUsable` et `ItemCategoryIsGear` le disent, plutôt que chacune
// à sa façon.
enum class ItemCategory : std::uint8_t {
  kConsumable,
  kWeapon,
  kArmor,
  kAmmo,
  kEtc,
  kCard,
  // L'objet de boutique — le serveur lui donne son propre type, et le coffre son
  // propre onglet. Le sac, lui, le range avec les consommables.
  kCash,
  // ⚠ « AUCUNE DES AUTRES » EXISTE, et il faut la nommer : le serveur envoie des
  // types que ces listes ne couvrent pas, et les forcer dans « divers » les
  // ferait apparaître là où ils n'ont rien à faire. Ils ne se voient que dans
  // l'onglet qui montre tout.
  kOther,
};

[[nodiscard]] ItemCategory ItemCategoryOf(const InventoryItem& item) noexcept;

// LE GROS GRAIN DU SAC ET DE LA CHARRETTE, celui du client d'origine.
//
// ⚠ L'APPARAT N'EST PAS UNE FAMILLE : c'est un EMPLACEMENT, et une coiffe
// d'apparat reste de type « armure » sur le fil. Le coffre l'isole par le masque
// (`IsCostumeLocation`), pas par le type — d'où l'absence de `kCostume` ici.

// « Conso » : les consommables ET les objets de boutique, que le client mêle.
[[nodiscard]] constexpr bool ItemCategoryIsUsable(ItemCategory family) noexcept {
  return family == ItemCategory::kConsumable || family == ItemCategory::kCash;
}

// « Equip » : les armes ET les armures — protections, apparat et ombre compris.
[[nodiscard]] constexpr bool ItemCategoryIsGear(ItemCategory family) noexcept {
  return family == ItemCategory::kWeapon || family == ItemCategory::kArmor;
}

// L'ORDRE D'ARRIVÉE, COMMUN AUX DEUX JOURNAUX.
//
// 🔴 LE RANG PROPRE À CHAQUE JOURNAL NE LES DÉPARTAGE PAS : ils comptent
// séparément, si bien qu'un gain de rang 3 et un refus de rang 3 ne disent rien
// de leur ordre. Et l'HORLOGE non plus : l'appelant date toute une fournée de
// trames du même instant, celui où il l'a drainée. Sans ce compteur-ci, qui
// avance à chaque évènement des deux journaux confondus, celui qui les relit ne
// peut que les mettre bout à bout — et un refus survenu AVANT un gain
// s'écrirait après lui, en semblant porter sur l'objet que le gain vient de
// nommer.
using PickupArrival = std::uint64_t;

// CE QUI VIENT D'ARRIVER DANS LE SAC, ET QUAND.
//
// 🔴 LA QUANTITÉ RAMASSÉE NE SURVIT PAS AU RANGEMENT : le serveur annonce un
// AJOUT, que `Put` additionne à ce que la case portait déjà. Après coup, la pile
// dit son total et plus jamais le gain. Ce journal garde le gain.
struct ObtainedItem {
  std::uint64_t seq = 0;          // le rang, évictions comprises : il ne recule jamais
  PickupArrival arrival = 0;      // l'ordre d'arrivée, comparable à celui d'un refus
  std::uint32_t obtained_ms = 0;  // l'horloge de l'appelant
  InventoryItem picked;           // `amount` = la quantité RAMASSÉE, pas le total
};

// COMBIEN DE GAINS ON GARDE.
//
// ⚠ CE N'EST PAS LA PILE DU BANDEAU QUI DÉCIDE, même si elle fut la première à
// lire ce journal : elle n'en montre qu'une poignée et jette le reste sans
// dommage. C'est la CONVERSATION qui fixe le plancher — elle écrit une ligne
// par entrée, et une entrée évincée entre deux relevés est une ligne PERDUE,
// sans que rien ne le dise.
//
// Le serveur annonce une entrée par pile, et pas seulement au ramassage : vider
// un coffre, acheter, toucher la récompense d'une quête en produisent une
// rafale entière dans le même envoi. Soixante-quatre couvre ces rafales-là ;
// vingt, la taille d'un sac de coffre, ne les couvrait pas.
inline constexpr std::size_t kObtainedItemsKept = 64;

// CE QUE LE JOUEUR VIENT DE JETER AU SOL, ET QUAND.
//
// 🔴 COPIÉ AVANT LE RETRAIT : une fois la pile vidée, la case ne dit plus rien
// de ce qu'elle portait, et la ligne « Vous avez jeté » n'aurait plus de nom.
struct DroppedItem {
  std::uint64_t seq = 0;         // le rang, évictions comprises : il ne recule jamais
  PickupArrival arrival = 0;     // l'ordre d'arrivée, comparable à celui d'un gain
  std::uint32_t dropped_ms = 0;  // l'horloge de l'appelant
  InventoryItem dropped;         // `amount` = la quantité JETÉE, pas ce qui reste
};

// COMBIEN DE JETS ON GARDE. Un jet se fait un objet à la fois, à la main :
// rien n'en produit de rafale, et seize couvrent tout ce qu'un relevé manqué
// pourrait laisser passer.
inline constexpr std::size_t kDroppedItemsKept = 16;

// UN RAMASSAGE QUE LE SERVEUR A REFUSÉ, et QUAND.
//
// 🔴 LE PAQUET D'UN REFUS EST VIDE : le serveur remet tout à zéro et ne
// réécrit que l'index, la quantité et le code. Il n'y a donc RIEN à ranger, et
// le code est toute l'information — sans lui, le joueur voit son ramassage
// échouer sans savoir si son sac est plein ou s'il est en surcharge.
//
// ⚠ LE CODE EST CELUI DU SERVEUR, NON INTERPRÉTÉ (`e_additem_result`,
// src/map/pc.hpp). Dire lequel se montre, et avec quels mots, demande la table
// de messages de l'installation : cela appartient à qui écrira la ligne.
struct RefusedPickup {
  std::uint64_t seq = 0;         // le rang, évictions comprises : il ne recule jamais
  PickupArrival arrival = 0;     // l'ordre d'arrivée, comparable à celui d'un gain
  std::uint32_t refused_ms = 0;  // l'horloge de l'appelant
  std::uint8_t result = 0;       // le motif, tel que le serveur l'a écrit
};

// COMBIEN DE REFUS ON GARDE, et pourquoi MOINS que de gains.
//
// ⚠ UN REFUS SE RÉPÈTE À L'IDENTIQUE : un sac plein refuse chaque objet d'un
// tas, à chaque tentative, avec le même code. L'appelant écrit UNE ligne de
// conversation par entrée — en garder autant que de gains ferait déverser
// vingt fois la même phrase rouge dans le fil et pousserait hors de vue ce que
// le joueur y lisait. Les entrées évincées ne disent d'ailleurs rien que la
// dernière ne dise déjà.
//
// Huit couvre ce qu'un anneau doit vraiment couvrir : la rafale reçue entre
// deux relevés de l'appelant, qui lit à chaque image.
inline constexpr std::size_t kRefusedPickupsKept = 8;

// UN CONTENANT : le sac, la charrette ou un coffre.
//
// ⚠ IL SE REMPLIT EN PLUSIEURS PAQUETS, et il faut le savoir : le serveur
// annonce l'ouverture, envoie les objets ordinaires puis les équipements — en
// deux paquets au moins, parfois davantage — et referme. Vider à chaque paquet
// de liste ne laisserait que les derniers reçus.
class Inventory {
 public:
  // Ouvre un remplissage : le contenant est VIDÉ et prend son nom.
  //
  // Le nom vient du serveur et n'est renseigné que pour un coffre. C'est la
  // SEULE chose qui distingue le stockage personnel de celui de la guilde : le
  // serveur les annonce tous deux comme « stockage », et ne les sépare que par
  // ce mot.
  void BeginFill(std::string name);

  // Referme le remplissage. Ce qui est dedans est alors complet.
  void EndFill();

  // Un remplissage est-il en cours ? Ce qu'on lit d'ici là est partiel.
  [[nodiscard]] bool filling() const noexcept { return filling_; }

  // Range un objet, ou remplace celui qui occupait déjà la case.
  void Put(InventoryItem item);

  // Retire `amount` exemplaires de la case. La ligne disparaît quand il n'en
  // reste plus. Faux si la case était vide.
  bool Take(std::uint16_t client_slot, std::int16_t amount);

  // Vide le CONTENU. Ce que le contenant peut porter reste : le serveur
  // l'annonce a part, et souvent avant la liste.
  void Clear();

  // Le contenant lui-meme a disparu : contenu ET capacite. Une charrette qu'on
  // rend, la ou un coffre qu'on referme existe encore.
  void Forget();

  [[nodiscard]] const InventoryItem* Find(std::uint16_t client_slot) const;

  // LA PIÈCE PORTÉE À CET EMPLACEMENT, nulle s'il est libre.
  //
  // ⚠ ELLE DOIT COUVRIR TOUS LES BITS DEMANDÉS, et non un seul : demander les
  // deux mains ne doit pas rendre la première épée d'un port à deux armes.
  //
  // 🔴 ET LE BOUCLIER DEMANDE UN TRI DE PLUS, qui n'est pas ici : une arme à
  // deux mains porte AUSSI son bit, donc elle répond. C'est `IsTwoHanded` qui
  // l'écarte, à l'endroit qui dessine — une case d'ombre autour de cette
  // fonction lui ferait mentir sur tous les autres emplacements.
  [[nodiscard]] const InventoryItem* WornAt(std::uint32_t location) const;

  // Range ou sort un objet de l'onglet des favoris. Faux si la case est vide.
  //
  // ⚠ UNE MUTATION NOMMÉE, et non un accès modifiable à l'objet entier. Le
  // serveur est seul maître du reste — quantité, raffinement, cartes — et
  // n'annonce le favori que par ce chemin ; ouvrir l'objet en écriture
  // inviterait à corriger tout le reste sans qu'il le sache.
  bool SetFavorite(std::uint16_t client_slot, bool favorite);

  // ── CE QUE LES ACCUSÉS DU SERVEUR CHANGENT SUR UN OBJET EN PLACE ───────────
  //
  // ⚠ UNE MÉTHODE PAR ÉVÉNEMENT, ET NON PAR CHAMP. « Wear » dit ce qui est
  // arrivé ; « SetWornAt » ne dirait que ce qui a bougé, et le prochain
  // événement qui touche deux champs à la fois n'aurait pas de nom.
  //
  // Chacune rend FAUX sur une case vide — une trame en retard sur un objet
  // vendu arrive ainsi, et le relevé doit pouvoir la ranger dans « ignorée »
  // plutôt que dans « appliquée ».

  // La pièce est portée, à cet endroit, et se montre sous cette apparence.
  bool Wear(std::uint16_t client_slot, std::uint32_t worn_at, std::uint16_t view_id);
  bool TakeOff(std::uint16_t client_slot);

  // Ce qu'il en reste après usage. ⚠ LE SERVEUR DONNE LE RESTE, pas ce qui a
  // été consommé : retrancher le nombre reçu viderait la pile deux fois trop
  // vite. Zéro fait disparaître la ligne.
  bool SetAmount(std::uint16_t client_slot, std::int16_t amount);

  bool Identify(std::uint16_t client_slot);

  // L'objet se lie à qui le porte, et ne se cédera plus.
  bool Bind(std::uint16_t client_slot);

  // Une pièce portée vient de s'abîmer. ⚠ ELLE SE DÉSIGNE PAR SON EMPLACEMENT
  // et non par sa place dans le sac : c'est tout ce que le serveur en dit.
  bool MarkWornDamaged(std::uint32_t worn_at);

  // UNE CARTE VIENT D'ÊTRE SERTIE : la pile de la carte perd un exemplaire, et
  // son identifiant s'écrit dans la première case VIDE des quatre de la pièce.
  //
  // 🔴 LE SERVEUR N'ANNONCE NI L'UN NI L'AUTRE : il retire la carte sans paquet
  // de retrait et ne relistera pas la pièce. Son accusé nomme les deux places,
  // et c'est au client d'en tirer les conséquences.
  //
  // ⚠ LA CARTE PART MÊME SI LES QUATRE CASES SONT PLEINES — c'est ce que fait le
  // client, qui ne le vérifie pas avant de décompter. Faux, et rien ne change,
  // si l'une des deux places est vide.
  bool InsertCard(std::uint16_t equip_slot, std::uint16_t card_slot);

  // ── LES DEUX JOURNAUX : CE QUI EST ENTRÉ, ET CE QUI A ÉTÉ REFUSÉ ──────────
  //
  // ⚠ NI `Clear()` NI `Forget()` NI UN RELISTAGE NE LES VIDENT, et ce n'est pas
  // un oubli : ils portent des ÉVÈNEMENTS, pas un contenu. Le serveur reliste
  // le sac à chaque entrée sur une carte ; les vider alors ferait disparaître
  // le bandeau d'un objet ramassé la seconde d'avant.
  //
  // Même contrat des deux côtés : un anneau borné, un rang qui part à UN et ne
  // recule jamais, et aucune horloge lue ici — l'instant vient de l'appelant.

  // Note un ramassage : `picked.amount` est la quantité REÇUE, et `at_ms`
  // l'horloge de l'appelant — le journal ne lit aucune horloge.
  void RecordObtained(const InventoryItem& picked, std::uint32_t at_ms);

  // Du plus ANCIEN ramassage gardé au plus récent, `kObtainedItemsKept` au plus.
  [[nodiscard]] std::span<const ObtainedItem> obtained() const noexcept;

  // Le rang du DERNIER ramassage, évictions comprises : il ne recule jamais.
  // Zéro tant que rien n'a été ramassé : un témoin à zéro est donc sûr.
  [[nodiscard]] std::uint64_t last_obtained_seq() const noexcept;

  // Note un refus : `result` est le code du serveur TEL QUEL, et `at_ms`
  // l'horloge de l'appelant.
  //
  // ⚠ RIEN D'AUTRE N'EST GARDÉ, parce que le paquet ne porte rien d'autre :
  // sur un refus, le serveur n'écrit que la case, la quantité et le code, et
  // l'objet qu'on a tenté de ramasser n'y est même pas nommé.
  void RecordRefused(std::uint8_t result, std::uint32_t at_ms);

  // Du plus ANCIEN refus gardé au plus récent, `kRefusedPickupsKept` au plus.
  [[nodiscard]] std::span<const RefusedPickup> refused() const noexcept;

  // Le rang du DERNIER refus, évictions comprises. Zéro tant qu'aucun ramassage
  // n'a été refusé.
  [[nodiscard]] std::uint64_t last_refused_seq() const noexcept;

  // Note un jet au sol : `dropped.amount` est la quantité JETÉE. Même contrat
  // que les deux journaux précédents, et même ordre d'arrivée qu'eux.
  void RecordDropped(const InventoryItem& dropped, std::uint32_t at_ms);

  // Du plus ANCIEN jet gardé au plus récent, `kDroppedItemsKept` au plus.
  [[nodiscard]] std::span<const DroppedItem> dropped() const noexcept;

  // Le rang du DERNIER jet, évictions comprises. Zéro tant que rien n'a été jeté.
  [[nodiscard]] std::uint64_t last_dropped_seq() const noexcept;

  // Rangés par NUMÉRO DE CASE. Le serveur, lui, n'ordonne pas le sac — il
  // explique même qu'il s'en garde, un tri invalidant ses propres index. Un
  // ordre stable de notre côté est ce qui permet de cliquer deux fois au même
  // endroit sur le même objet.
  [[nodiscard]] std::span<const InventoryItem> items() const noexcept { return items_; }

  [[nodiscard]] std::size_t size() const noexcept { return items_.size(); }

  [[nodiscard]] std::string_view name() const noexcept { return name_; }

  // COMBIEN DE FOIS LE CONTENANT A CHANGÉ. Un compte, pas un drapeau : celui qui
  // en dérive quelque chose compare au sien et sait s'il a manqué un tour.

  // ── CE QUE LE CONTENANT PEUT PORTER ───────────────────────────────────────
  //
  // ⚠ ANNONCÉ À PART DE SON CONTENU, et parfois jamais. Le serveur le dit par
  // des paquets qui lui sont propres — un par contenant —, et un personnage qui
  // n'a pas de charrette n'en reçoit aucun. Zéro veut donc dire « pas encore
  // dit », pas « ne peut rien porter ».
  void SetCapacity(int slots);
  [[nodiscard]] int capacity() const noexcept { return capacity_; }

  // LA CHARGE, en dixièmes d'unité comme le serveur la compte.
  //
  // ⚠ SEULE LA CHARRETTE SE PÈSE. Ce que porte le PERSONNAGE est un fait de sa
  // fiche, pas de son sac : deux poids pour deux choses, et les confondre ferait
  // afficher la charge d'une charrette comme si c'était la sienne.
  void SetLoad(std::int64_t weight, std::int64_t max_weight);
  [[nodiscard]] std::int64_t weight() const noexcept { return weight_; }
  [[nodiscard]] std::int64_t max_weight() const noexcept { return max_weight_; }

  [[nodiscard]] std::uint32_t revision() const noexcept { return revision_; }

 private:
  std::vector<InventoryItem> items_;
  std::vector<ObtainedItem> obtained_;
  std::vector<RefusedPickup> refused_;
  std::vector<DroppedItem> dropped_;
  // Le compteur des trois journaux, seul à pouvoir les remettre dans l'ordre.
  PickupArrival pickup_arrivals_ = 0;
  std::string name_;
  // COMBIEN DE RAMASSAGES EN TOUT, et non combien sont gardés : c'est ce qui
  // rend le rang monotone quand l'anneau écrête.
  std::uint64_t obtained_seen_ = 0;
  // Le même compte pour les refus et pour les jets, et pour la même raison.
  std::uint64_t refused_seen_ = 0;
  std::uint64_t dropped_seen_ = 0;
  std::uint32_t revision_ = 0;
  int capacity_ = 0;
  std::int64_t weight_ = 0;
  std::int64_t max_weight_ = 0;
  bool filling_ = false;
};

}  // namespace ro::sim
