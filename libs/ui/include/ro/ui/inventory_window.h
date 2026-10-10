#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LE SAC : LA GRILLE D'OBJETS DU CLIENT.
//
// ⚠ ELLE NE RANGE RIEN ET NE COMPTE RIEN. Une case lui arrive avec son icône,
// son nom composé et son badge déjà décidés ; l'onglet où elle tombe est décidé
// ailleurs. `ui` ne connaît ni le sac ni la table des objets — elle dessine ce
// qu'on lui donne.
//
// ⚠ ET ELLE NE FAIT RIEN. Cette livraison est en LECTURE : on voit son sac, on
// n'agit pas dessus. Les verbes — double-clic, menu contextuel, quantité,
// glisser — sont un chantier à part, et leur absence se voit mieux qu'une
// moitié de geste.
//
// Ce qu'elle reprend du client, au pixel : la case de 32 px JOINTIVE, les
// colonnes CALCULÉES et jamais fixes, le redimensionnement par palier de case,
// le fond pavé, le bandeau en trois morceaux, et les sept onglets.

// Le côté d'une case, en pixels d'art. Le client n'en connaît pas d'autre : ses
// icônes sont dessinées pour cette taille.
inline constexpr float kItemCellPixels = 32.0F;

// La fenêtre ne descend pas sous cinq cases de côté : en deçà elle ne montre
// plus assez pour mériter la place qu'elle prend.
inline constexpr int kItemGridMinimumColumns = 5;

// Le badge a SA police, plus petite que le texte courant — comme le client, qui
// écrit ses quantités à part et loge ainsi sept chiffres dans 32 px. Et l'on
// rétrécit encore tant que le nombre déborde, jusqu'à un plancher sous lequel un
// chiffre ne se lit plus.
inline constexpr float kItemBadgeScale = 0.80F;
inline constexpr float kItemBadgeMinimumScale = 0.65F;

// Le voile de survol. ⚠ PAS LE `HeaderHovered` DU THÈME, qui est BLEU : le
// client éclaircit à peine, et un aplat bleu sur une icône la dénature.
inline constexpr ImU32 kItemHoverVeil = IM_COL32(255, 255, 255, 55);

// Un objet ABÎMÉ : son icône est teintée de ce rouge, et son nom en reçoit
// l'ombre. La teinte multiplie la texture, donc l'icône reste reconnaissable.
inline constexpr ImU32 kItemDamagedTint = IM_COL32(0xFA, 0x50, 0x50, 0xFF);

// Le poids, passé le seuil que le serveur annonce.
inline constexpr ImU32 kItemOverweightText = IM_COL32(230, 60, 60, 255);

// TOUS LES ONGLETS QUE LES TROIS FENÊTRES D'OBJETS SAVENT MONTRER.
//
// ⚠ AUCUNE NE LES MONTRE TOUS. Le sac et la charrette prennent les SEPT du
// client ; le coffre en prend DIX, qui séparent ce que les autres réunissent —
// armes, armures et apparat au lieu d'« équipement », cash au lieu de conso. La
// liste se DONNE donc au dessin plutôt que de se déduire d'un compte qu'aucune
// ne partage.
//
// ⚠ UN FAVORI DISPARAÎT DE TOUS LES AUTRES ONGLETS, « Tout » COMPRIS. Ce n'est
// pas un filtre de plus : c'est un déplacement.
enum class ItemTab : std::uint8_t {
  kAll,
  kConsumable,
  kEquipment,
  kAmmo,
  kEtc,
  kCard,
  kFavorite,
  // ── LES QUATRE QUE SEUL LE COFFRE MONTRE ──────────────────────────────────
  //
  // ⚠ AJOUTÉS EN QUEUE, jamais insérés : les tables d'art de `item_grid` et le
  // découpage des familles s'indexent par ce rang, et glisser une valeur au
  // milieu donnerait à chaque onglet la tuile de son voisin.
  kWeapon,
  kArmor,
  // ⚠ L'APPARAT N'EST PAS UN TYPE mais un EMPLACEMENT : une coiffe d'apparat
  // reste de type « armure » sur le fil. Cet onglet se filtre au masque.
  kCostume,
  kCash,
};

// 🔴 IL SE DÉRIVE DU DERNIER ÉNUMÉRATEUR, ET C'EST UN PIÈGE CONNU : les tables
// d'art de `item_grid` sont dimensionnées par lui, et un compte resté sur
// l'avant-dernier les laisserait trop courtes — le dessin lirait hors de la
// table. Ajouter un onglet, c'est déplacer ce nom.
inline constexpr std::size_t kItemTabCount = static_cast<std::size_t>(ItemTab::kCash) + 1;

// Une case de la grille.
struct InventoryCellView {
  // La place du serveur, déjà décalée. Sert d'identité ImGui : elle survit à un
  // tri, contrairement au rang d'affichage.
  std::uint16_t client_slot = 0;

  // Le nom COMPOSÉ, prêt à montrer. Ni `ui` ni personne ne le refabrique ici.
  std::string_view name;

  // L'icône, vide quand l'installation ne l'a pas — la case montre alors un
  // point d'interrogation grisé plutôt que rien.
  SkinImage icon;

  // Le badge : le raffinement s'il y en a, SINON la quantité, et jamais les
  // deux. Une quantité de un ne s'écrit pas.
  int refine_level = 0;
  int amount = 0;

  bool damaged = false;

  // CE QUE L'OBJET PERMET, décidé ailleurs : `ui` ne sait pas ce qu'est un type
  // d'objet. Sans ces deux drapeaux, le menu proposerait « Équiper » sur une
  // potion et « Utiliser » sur une épée.
  bool usable = false;
  bool wearable = false;
  // Portée EN CE MOMENT. Le même geste la retire alors au lieu de la poser.
  bool worn = false;
  // UN ACCESSOIRE DE PET : il ne se porte pas sur le personnage, mais le même
  // geste — double-clic ou « Équiper » — le propose au pet sorti.
  bool pet_accessory = false;
  // UNE CARTE : ni utilisée ni portée, elle se SERTIT. Le double-clic demande
  // au serveur les pièces qui peuvent la recevoir.
  bool card = false;
  // UNE ARME OU UNE ARMURE QUI A ENCORE UNE CASE DE CARTE LIBRE, et qu'on ne
  // porte pas : elle offre le sertissage rapide. ⚠ LE SERVEUR TRANCHE SEUL
  // quelles cartes y vont — ce drapeau ne dit que qu'il y a de la place.
  bool socket_free = false;
  // L'identifiant de l'objet. ⚠ IL VOYAGE DANS UN GLISSER là où la place,
  // elle, se renumérote dès qu'on consomme.
  std::uint32_t item_id = 0;
  // Rangée dans l'onglet des favoris. ⚠ C'EST UN FAIT DU SERVEUR, qui le
  // retient d'une session à l'autre — pas un réglage d'affichage.
  bool favorite = false;
  // Combien de cartes la pièce peut recevoir.
  //
  // ⚠ LE NOM COMPOSÉ LE PORTE DÉJÀ, sous la forme « [2] ». Ce champ existe pour
  // la seule fenêtre qui en fait une COLONNE — le coffre —, et la raison qu'une
  // colonne a d'exister est qu'on trie dessus. Une grille n'en a pas l'usage.
  std::uint8_t card_slots = 0;
  // CE QUE LA PILE ENTIÈRE SE REVEND, chez un PNJ — le prix unitaire multiplié
  // par la quantité.
  //
  // ⚠ IL VIENT DU SERVEUR ET DE LUI SEUL (`ZC 0x0F0F`), et il n'arrive qu'AVEC
  // UN COFFRE : zéro veut donc dire « le serveur n'a rien dit », jamais
  // « gratuit ». La colonne écrit alors un tiret plutôt qu'un zéro, qui se
  // lirait comme un prix.
  std::int64_t value = 0;
};

// Ce que le pied annonce. Groupé plutôt qu'en six paramètres voisins : ils se
// lisent ensemble, et l'ordre de six entiers devient vite un piège.
struct InventoryTally {
  // En dixièmes d'unité, comme le serveur les envoie.
  std::int64_t weight = 0;
  std::int64_t max_weight = 0;
  // Le pourcentage à partir duquel le poids s'écrit en rouge. Il vient du
  // serveur (0x0ADE) et n'est PAS une constante : chaque installation le règle.
  std::int64_t overweight_percent = 0;
  std::int64_t zeny = 0;
  int item_count = 0;
  // Ce que le sac peut contenir. ⚠ L'EXTENSION N'EST PAS DÉCODÉE : tant qu'on
  // ne lit pas le paquet qui l'annonce, c'est la taille de base.
  int max_items = 0;
};

// LE GESTE DEMANDÉ SUR UN OBJET.
//
// ⚠ UNE DEMANDE, PAS UN EFFET. La fenêtre n'émet rien et ne touche à rien : elle
// dit ce que le joueur a voulu, et c'est `app` qui parle au serveur. Le sac ne
// bougera qu'à la réponse — une demande refusée ne doit pas laisser l'affichage
// en avance sur le monde.
enum class ItemVerb : std::uint8_t {
  kNothing,
  // Le double-clic : consommer, ou porter. C'est le MÊME geste, et c'est le type
  // de l'objet qui décide lequel — comme dans le client.
  kUseOrWear,
  kTakeOff,
  kDrop,
  // Ranger l'objet dans l'onglet des favoris, ou l'en sortir. Le sens se lit
  // sur la case : un favori qu'on bascule cesse de l'être.
  kToggleFavorite,
  // Ranger l'objet dans la charrette.
  //
  // ⚠ IL N'APPARAÎT QUE SI LE PERSONNAGE EN POUSSE UNE. Le serveur accepterait
  // le paquet et le jetterait en silence ; une entrée qui ne marche jamais use
  // la confiance qu'on met dans les autres.
  kToCart,
  // Ranger l'objet dans le coffre.
  //
  // ⚠ IL N'APPARAÎT QUE PENDANT QU'UN COFFRE EST OUVERT, et c'est le même
  // instant où le rangement en charrette devient impossible. Les deux entrées se
  // relaient donc, et il y a toujours quelque part où envoyer un objet.
  kToStorage,
  // Sertir la carte : demander au serveur les pièces qui peuvent la recevoir.
  // La réponse ouvre la fenêtre de sertissage.
  kInsertCard,
  // LE SERTISSAGE RAPIDE : la carte `insert_card_slot` sur la pièce `slot`,
  // sans fenêtre.
  kQuickInsert,
  // Joindre l'objet au courrier en cours d'écriture.
  //
  // ⚠ IL N'APPARAÎT QUE PENDANT L'ÉCRITURE : les pièces jointes n'existent
  // qu'avec elle.
  kToMail,
};

// UNE CARTE QUE LE SERVEUR ACCEPTERAIT SUR LA PIÈCE DU SOUS-MENU.
struct QuickInsertCardView {
  std::uint16_t card_slot = 0;
  std::string_view name;
};

// CE QU'ON DEMANDE AU JOUEUR QUAND UNE PILE NE PART PAS ENTIÈRE.
//
// ⚠ ZÉRO VEUT DIRE « AUCUNE QUESTION EN COURS ». La place d'un objet n'est
// jamais nulle — le serveur décale ses index — donc elle peut servir de
// sentinelle sans ambiguïté.
//
// ⚠ ELLE EST ICI, dans l'en-tête du sac, parce que c'est là que vivent déjà les
// types que les fenêtres d'objets se partagent — la case, l'onglet. Le jour où
// elles seront trois, ces types mériteront leur propre en-tête ; deux ne le
// justifient pas encore.
struct ItemQuantityAsk {
  std::uint16_t slot = 0;
  int amount = 1;
};

// CE QUE LA FENÊTRE SAIT DU MONDE ET DE SES SŒURS, posé par l'appelant à chaque
// image. ⚠ ELLE NE L'ÉCRIT JAMAIS, et c'est pour cela qu'il vit hors de son
// état : `ui` ne possède aucun état de jeu.
struct InventoryWindowView {
  // LE PERSONNAGE POUSSE-T-IL UNE CHARRETTE ? Sans elle, l'entrée « vers le
  // cart » n'a nulle part où envoyer, et le serveur jetterait le paquet sans un
  // mot.
  bool cart_available = false;
  // 🔴 UN COFFRE EST-IL OUVERT ? Alors le serveur REFUSE l'échange entre le sac
  // et la charrette, et il le refuse SANS RIEN DIRE : `pc_cant_act2` compte
  // `storage_flag` parmi ses empêchements. C'est une règle du monde, pas
  // l'absence d'un contenant — elle se montre donc grisée, comme le verrou de
  // jet, plutôt que de disparaître.
  bool storage_open = false;
  // UNE ÉCHOPPE SE PRÉPARE : les transferts vers la charrette et le coffre sont
  // gelés. Utiliser, porter et jeter restent permis.
  bool transfers_frozen = false;
  // ⚠ L'ÉTAT DE LA CHARRETTE, pour que son raccourci dise ce que le clic VA
  // faire : le libellé d'un raccourci est le NOM de la fenêtre, pas le geste.
  bool cart_open = false;
  // L'ALBUM DE CARTES EST-IL OUVERT ? Alors une carte peut y aller par le menu.
  bool album_open = false;
  // UN COURRIER S'ÉCRIT-IL ? Alors un objet du sac peut y être joint par le
  // menu.
  bool mail_writing = false;
  // ── LE SERTISSAGE RAPIDE : ce que le serveur a répondu ─────────────────────
  //
  // La pièce sur laquelle porte la réponse, zéro si aucune question n'est en
  // cours. ⚠ TANT QU'IL N'A PAS RÉPONDU, le sous-menu dit « Chargement… » : une
  // liste vide est une réponse, « aucune carte ».
  std::uint16_t quick_insert_equip = 0;
  bool quick_insert_answered = false;
  std::span<const QuickInsertCardView> quick_insert_cards;
  // LE STAFF PEUT VIDER SON SAC : un bouton « Vider » paraît dans la barre de
  // titre. Faux, il n'existe pas.
  bool staff_tools = false;
};

// Ce que la fenêtre garde d'une image à l'autre.
struct InventoryWindowState {
  bool open = false;
  ItemTab tab = ItemTab::kAll;
  // Le filtre par nom, tel que le joueur le tape. Vidé quand on le masque :
  // sinon il continuerait de cacher des objets sans que rien ne l'explique.
  char filter[64] = {};
  bool filter_shown = false;
  // LE PLACEMENT LIBRE : chaque objet garde SA case, et un objet ramassé ne
  // bouscule plus la disposition.
  //
  // 🔴 LA FENÊTRE NE LE DÉCIDE PAS ET NE RANGE RIEN. Elle dessine les cases dans
  // l'ordre qu'on lui donne — vides comprises — et rapporte le lâcher ; c'est
  // `app` qui tient la table et qui la persiste. Ce drapeau-ci ne lui sert qu'à
  // savoir si une case peut RECEVOIR un objet du sac.
  //
  // ⚠ IL EXIGE LA TAILLE VERROUILLÉE, et c'est l'étage qui l'impose : une case
  // est un index absolu, donc changer la largeur ferait glisser toute la
  // disposition d'une case à l'autre.
  bool free_layout = false;
  // LES ONGLETS SONT-ILS DEBOUT, en colonne à gauche de la grille, plutôt que
  // couchés en rangée au-dessus ?
  //
  // ⚠ DEBOUT PAR DÉFAUT, comme la fenêtre du client : c'est là que le joueur va
  // les chercher. L'installation livre les deux jeux d'images, `tab_*` et
  // `tabh_*` — la rangée n'est donc pas la colonne tournée.
  //
  // 🔴 LE SENS EST CELUI DU RÉGLAGE, « Onglets verticaux (à gauche) ». Nommé à
  // l'envers, chaque pont qui le descend jusqu'ici aurait eu à l'inverser.
  bool tabs_vertical = true;
  // L'ÉPINGLE DE LA BARRE DE TITRE : Échap ne referme plus cette fenêtre-là.
  // ⚠ RETENUE D'UNE SESSION À L'AUTRE, à côté de sa place, par `RememberWindow`.
  bool pinned = false;
  // LE VERROU DE TAILLE : la fenêtre ne se redimensionne pas, mais reste
  // déplaçable — pour qui a trouvé sa taille et ne veut plus la perdre d'un
  // glissement.
  bool size_locked = false;
  // LE VERROU DE JET. ⚠ ENTIÈREMENT CHEZ NOUS : le serveur n'en sait rien et
  // accepterait le jet. C'est un garde-fou contre le geste, pas une règle du
  // monde — et c'est bien pour cela qu'il existe : jeter ne se rattrape pas.
  bool drop_locked = false;
  // L'APERÇU DE DESCRIPTION REMPLACE-T-IL L'INFOBULLE DE NOM ?
  //
  // ⚠ IL LA REMPLACE, il ne s'y ajoute pas : deux infobulles pour le même objet
  // se recouvriraient. C'est un réglage, parce qu'un pavé de description au
  // moindre passage de souris gêne qui range son sac.
  bool hover_preview = false;

  // ── CE QUI NE VAUT QUE SUR LES FAVORIS ─────────────────────────────────────
  //
  // LE VERROU DE VENTE. Le client d'origine le tient dans un global, et il lui
  // sert à refuser la vente d'un favori à un marchand. ⚠ IL SE VOIT AUSSI : sur
  // cet onglet, verrou mis, le fond pavé de la grille change — c'est le SEUL
  // écart de dessin entre la grille du sac et celle de la charrette.
  bool sell_locked = false;
  // LA VUE TRIÉE — type, puis nom. ⚠ ELLE NE VAUT QUE SUR LES FAVORIS, et le
  // bouton disparaît quand la taille est verrouillée : c'est le mode où la
  // disposition appartient au joueur, et un tri viendrait la défaire. La
  // fenêtre ne trie rien elle-même — elle reçoit les cases dans l'ordre voulu.
  bool sorted = false;
  // LE RACCOURCI DE BARRE DE TITRE VERS LE STORAGE. ⚠ OPT-IN, et propre
  // à chaque fenêtre : c'est un ajout à la barre du client,
  // pas une pièce de son dessin.
  //
  // ⚠ UN SEUL DRAPEAU POUR LES DEUX SENS du geste — ouvrir, refermer : c'est
  // l'état de la session qui dit lequel, et le dédoubler ferait deux vérités à
  // tenir d'accord.
  bool storage_button = false;
  // CE QUE LA FENÊTRE OCCUPE AUTOUR DE LA GRILLE — bande d'onglets, pied,
  // marges —, mesuré à chaque image et relu par le calage de la taille.
  //
  // ⚠ IL VIT ICI ET NON DANS UNE STATIQUE DE FICHIER, et c'est la règle du
  // dépôt : `ui` ne possède rien de global. Deux sacs ouverts sur deux
  // personnages ne doivent pas se voler leur mesure. ImGui passe l'adresse de
  // cet état à son rappel de taille, qui n'a donc rien à aller chercher.
  ImVec2 chrome = ImVec2(0.0F, 0.0F);

  // ── L'INVITE DE QUANTITÉ ───────────────────────────────────────────────────
  //
  // Jeter une pile demande COMBIEN. L'invite vit dans l'état plutôt que dans une
  // statique : deux sacs ouverts ne doivent pas se voler leur question, et une
  // fenêtre refermée doit oublier la sienne.
  //
  // ⚠ LA PLACE VISÉE EST RETENUE, PAS L'OBJET. Le temps de répondre, la liste a
  // pu changer ; `app` revérifie ce qu'il y a dans cette place avant d'émettre.
  std::uint16_t asking_slot = 0;
  int asked_amount = 1;
  // 🔴 QUEL VERBE LA QUESTION CONFIRMERA. Deux gestes demandent « combien » —
  // jeter et ranger dans la charrette — et ils ne se rattrapent pas pareil. Sans
  // ce souvenir, la réponse partirait toujours au premier des deux, et le joueur
  // verrait ses objets tomber au sol au lieu d'entrer dans sa charrette.
  ItemVerb asking_verb = ItemVerb::kDrop;
};

// Ce que le joueur a demandé pendant l'image.
struct InventoryWindowAction {
  // L'objet survolé, zéro si aucun. Sert à qui veut montrer sa description
  // ailleurs.
  std::uint16_t hovered_slot = 0;

  // L'IDENTIFIANT DE L'OBJET SURVOLÉ, zéro si aucun.
  //
  // ⚠ L'IDENTIFIANT ET NON LA PLACE : l'aperçu montre une DESCRIPTION, qui ne
  // dépend pas du contenant. C'est aussi ce qui permet à l'étage de la bâtir
  // sans savoir de quelle fenêtre le survol venait.
  std::uint32_t hovered_id = 0;

  ItemVerb verb = ItemVerb::kNothing;
  // Sur quel objet. ⚠ LA PLACE DU SERVEUR, jamais le rang d'affichage : entre le
  // clic et l'émission, un objet consommé peut avoir renuméroté la liste.
  std::uint16_t slot = 0;
  // Combien on en jette. Vaut un hors du jet, et la pile entière ne s'y met
  // jamais par défaut : jeter tout par mégarde ne se rattrape pas.
  int amount = 1;
  // LA CARTE du sertissage rapide ; `slot` est alors la pièce.
  std::uint16_t insert_card_slot = 0;
  // LA PIÈCE DONT LE SOUS-MENU DE SERTISSAGE RAPIDE EST OUVERT, zéro sinon.
  // ⚠ RAPPORTÉE À CHAQUE IMAGE où il l'est : c'est `app` qui sait si la
  // question est déjà posée pour elle.
  std::uint16_t compatible_asked = 0;

  // LA VARIANTE DU GESTE : Ctrl enfoncé pendant le double-clic. Elle ne veut
  // rien dire par elle-même — c'est l'appelant qui sait qu'une arme part alors
  // dans l'autre main.
  bool alternate = false;

  // CE QU'UNE AUTRE FENÊTRE VIENT DE LÂCHER ICI. ⚠ RENDU PAR CELLE QUI REÇOIT :
  // c'est elle qui a vu le lâcher, et `app` sait quel paquet les deux bouts
  // appellent. Place nulle = rien.
  ItemTransferAsk received;

  // L'OBJET DONT IL VEUT LA DESCRIPTION, zéro si aucun.
  //
  // ⚠ UN IDENTIFIANT, PAS UN VERBE. Décrire n'envoie rien au serveur : c'est une
  // fenêtre qu'on ouvre, et elle survit à l'objet qui l'a ouverte. La PLACE
  // l'accompagne pour décorer le nom — raffinement, cartes — tant que l'objet
  // est là.
  std::uint32_t describe_id = 0;
  std::uint16_t describe_slot = 0;

  // ── LE PLACEMENT LIBRE : L'OBJET QU'ON VIENT DE POSER SUR UNE CASE ────────
  //
  // ⚠ LA CASE EST UN RANG D'AFFICHAGE, et c'est le seul endroit du dessin où il
  // en soit un : le joueur désigne UNE CASE de la grille, pas un objet. Moins de
  // zéro quand rien n'a été posé.
  //
  // ⚠ ET L'OBJET SE DIT PAR SA PLACE SERVEUR, comme partout ailleurs : c'est
  // elle qui permet à `app` de retrouver de quoi il s'agit.
  std::uint16_t placed_slot = 0;
  int placed_cell = -1;

  // LA PLACE DONT IL VEUT POSER LE LIEN DANS LA CONVERSATION, ou zéro.
  //
  // ⚠ UNE PLACE ET NON UN IDENTIFIANT : un lien porte l'objet DE QUELQU'UN —
  // raffinement, cartes, grade —, et il n'y a que la place pour les retrouver.
  std::uint16_t link_slot = 0;

  // L'ALBUM DE CARTES. La carte de cette place lui est offerte (« Vers
  // l'album ») — c'est lui qui décide du dépôt ou du sacrifice, et qui demande
  // combien —, ou la pochette de cette carte vient d'être lâchée sur le sac.
  std::uint16_t to_album_slot = 0;
  std::uint32_t album_withdraw = 0;

  // Les raccourcis de barre de titre vers les autres fenêtres d'objets.
  bool toggle_storage = false;
  bool toggle_cart = false;
  // LE BOUTON « Vider » DU STAFF. ⚠ UNE INTENTION : c'est `app` qui relit le
  // droit au geste et qui sait ce que le serveur attend.
  bool staff_wipe = false;
  // La PUCE de la barre de titre a été cliquée : le joueur veut les réglages de
  // CETTE fenêtre. ⚠ C'est `app` qui sait laquelle — `ui` ne connaît pas les
  // sections du panneau.
  bool settings_asked = false;
  // LES TROIS INTERRUPTEURS DU PIED VIENNENT D'ÊTRE BASCULÉS.
  //
  // ⚠ LA FENÊTRE NE LES BASCULE PAS ELLE-MÊME, et c'est la règle de la couche :
  // les trois drapeaux lui DESCENDENT des réglages, elle ne les écrit pas. Le
  // faire ici laisserait le panneau montrer l'ancienne valeur jusqu'à sa
  // prochaine composition, et le fichier ne saurait rien.
  bool toggle_drop_lock = false;
  bool toggle_sell_lock = false;
  bool toggle_sort = false;
  // LE SAC DE ZENY DU PIED : ouvrir ou refermer la BANQUE.
  //
  // 🔴 SA PLACE EST ICI ET NON DANS UNE FENÊTRE À ELLE. La banque échange des
  // zeny avec la poche, dont ce pied affiche le montant : le bouton est
  // collé au nombre qu'il fait bouger, juste à sa gauche.
  //
  // ⚠ UNE BASCULE, comme le raccourci : ouverte, la banque se referme ;
  // fermée, on la DEMANDE au serveur. Voir `ro/ui/bank_window.h`.
  bool toggle_bank = false;
};

// ⚠ « INVENTORY » ET NON « INVENTAIRE ». Le vocabulaire de jeu de Ragnarok reste
// en anglais jusque dans l'interface française : les joueurs le lisent ainsi
// depuis des années, et le franciser leur ferait retraduire dans leur tête.
inline constexpr const char* kInventoryWindowTitle = core::TrText("Inventory");
inline constexpr const char* kInventoryWindowId = "inventory";

// Le libellé d'un onglet, et la pièce d'habillage qui le dessine.
[[nodiscard]] const char* ItemTabLabel(ItemTab tab);
[[nodiscard]] render::SkinPiece ItemTabPiece(ItemTab tab, bool active);

// Le même onglet, COUCHÉ. ⚠ CE N'EST PAS LA PIÈCE DEBOUT TOURNÉE : l'installation
// en livre un second jeu, dessiné pour une rangée, dont le bord ouvert n'est pas
// du même côté.
[[nodiscard]] render::SkinPiece ItemTabPieceLying(ItemTab tab, bool active);

// UN MONTANT GROUPÉ PAR MILLIERS — « 10,900,000 ».
//
// ⚠ LA VIRGULE, ET NON L'ESPACE DU FRANÇAIS, et ce n'est pas une étourderie :
// c'est ce qu'écrit le client d'origine.
// Un joueur compare deux fenêtres, pas deux typographies — et un montant qui
// change de forme d'un client à l'autre se relit deux fois.
[[nodiscard]] std::string GroupThousands(std::int64_t amount);

// Ce qui suit un montant, collé à lui. Le client ne met pas d'espace, et
// l'ajouter décalerait la seule ligne du pied qui s'aligne à droite.
inline constexpr std::string_view kZenySuffix = "z";

// Combien de colonnes de cases tiennent dans cette largeur. Jamais moins d'une :
// une grille sans colonne ne dessinerait rien et diviserait par zéro.
[[nodiscard]] int ItemGridColumns(float available_width, float cell, float gap);

// Le corps d'un badge qui tient dans une case. Rend le corps demandé quand le
// texte y tient déjà, sinon le plus grand qui tienne, sans descendre sous le
// plancher — la largeur n'étant pas proportionnelle au corps, l'appelant
// termine au pixel près.
[[nodiscard]] float ItemBadgeFontSize(float base_size, float text_width, float available_width);

[[nodiscard]] InventoryWindowAction DrawInventoryWindow(InventoryWindowState& state,
                                                        const InventoryWindowView& view,
                                                        std::span<const InventoryCellView> cells,
                                                        InventoryTally tally, const SkinArt& art);

}  // namespace ro::ui
