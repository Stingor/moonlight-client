#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/inventory_window.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LE COFFRE : LA TROISIÈME SŒUR, ET CELLE QUI NE LEUR RESSEMBLE PAS.
//
// 🔴 CE N'EST PAS UNE GRILLE. Le sac et la charrette montrent des cases ; le
// coffre montre une TABLE À COLONNES — case, objet, identifiant, emplacements,
// quantité —, qu'on trie et dont on range les colonnes. Ce dessin a sa raison :
// on ne fouille pas un coffre comme on fouille un sac. Un sac se reconnaît
// d'un coup d'œil sur des icônes qu'on y a mises soi-même ; un coffre porte
// six cents lignes qu'on a oubliées, et ce qu'on y cherche se trie.
//
// Ses onglets sont donc COUCHÉS et non debout : sa largeur est déjà disputée par
// les colonnes, et une bande à gauche la lui prendrait deux fois.
//
// ⚠ ET IL EST UNE SESSION DU SERVEUR. La fenêtre ne s'ouvre pas au clavier : un
// PNJ ou une compétence l'ouvre, et la refermer se DIT au serveur — tant qu'il
// croit le coffre ouvert, il refuse en silence les échanges entre le sac et la
// charrette.

// La taille d'ouverture, qui est aussi le PLANCHER : sous cela les colonnes
// s'écrasent, la rangée d'onglets se coupe et le pied passe sous la table.
inline constexpr float kStorageFirstWidthPixels = 320.0F;
inline constexpr float kStorageFirstHeightPixels = 420.0F;

// Les colonnes, par ordre de DÉCLARATION, et cet ordre est figé.
//
// ⚠ L'ORDRE D'AFFICHAGE APPARTIENT AU JOUEUR : les en-têtes se glissent. Ces
// rangs restent donc les seuls repères stables — ils disent quelle colonne
// remplir et sur laquelle trier, où qu'elle soit rendue.
enum class StorageColumn : int {
  kSlot,
  kItem,
  kItemId,
  kCardSlots,
  kAmount,
  // CE QUE LA PILE SE REVEND. ⚠ EN DERNIER, et l'ordre de déclaration est figé :
  // l'insérer au milieu décalerait les rangs que le tri et le remplissage
  // nomment, et chaque colonne écrirait dans celle de sa voisine.
  kValue,
};

// ⚠ IL SE DÉDUIT DU DERNIER ÉNUMÉRATEUR, et il faut le RELIRE en ajoutant une
// colonne : la table en déclare autant qu'il annonce, et une de plus fait tomber
// ImGui sur une assertion — « TableSetupColumn(): called too many times! ».
inline constexpr int kStorageColumnCount = static_cast<int>(StorageColumn::kValue) + 1;

// Ce que le pied annonce.
struct StorageTally {
  int item_count = 0;
  // Ce que le coffre peut porter. Le serveur l'annonce à part de son contenu.
  int capacity = 0;
  // Combien de lignes l'onglet et le filtre laissent voir. ⚠ CE N'EST PAS
  // `item_count` : le pied montre les deux, parce qu'un coffre plein dont
  // l'onglet ne rend rien se lit autrement qu'un coffre vide.
  int shown_count = 0;
  // CE QUE LE COFFRE VAUT, revendu à un PNJ — la somme des lignes AFFICHÉES.
  //
  // ⚠ CELLES QUI SE VOIENT, et non tout le coffre : le total suit donc l'onglet
  // et le filtre. Un total qui ignorerait le filtre répondrait à une question
  // que personne n'a posée.
  std::int64_t shown_value = 0;
  // ⚠ LE SERVEUR A-T-IL PARLÉ ? Sans lui, un total de zéro se lirait « ce coffre
  // ne vaut rien » au lieu de « je ne sais pas ».
  bool value_known = false;
};

// « TOUS LES SOUS-TYPES ». ⚠ CE N'EST PAS ZÉRO : zéro EST une clé de
// sous-type — le premier emplacement du mannequin.
inline constexpr int kAnySubCategory = -1;

// UN COFFRE QU'ON PEUT OUVRIR, tel que la rangée le propose.
//
// ⚠ `ui` NE SAIT PAS CE QU'EST UN COFFRE ALTERNATIF. Elle reçoit un numéro et
// un nom, comme elle reçoit un libellé et une teinte pour un mode d'envoi ;
// c'est `app` qui traduit ce que le monde porte, et le numéro ne lui sert qu'à
// le lui rendre.
struct StorageShelfView {
  std::uint8_t id = 0;
  // LE NOM DU SERVEUR — « Storage Alt 3 ». ⚠ IL RESTE, même quand le joueur en
  // a donné un autre : c'est lui que l'infobulle montre, et c'est la seule chose
  // qui dise de QUEL coffre du serveur il s'agit.
  std::string_view name;
  // LE NOM QUE LE JOUEUR A DONNÉ, vide s'il n'en a pas donné.
  //
  // 🔴 L'ONGLET ÉCRIT CELUI-CI, OU LE NUMÉRO — jamais le nom du serveur. Un
  // cadre d'onglet fait la taille d'une icône : « Storage Alt 3 » n'y tiendrait
  // pas, et le rogner donnerait « Sto… » sur tous les coffres. C'est le dessin
  // du client.
  std::string_view own_name;
  // L'ICÔNE D'UN OBJET, choisie par le joueur — vide quand il n'en a pas choisi.
  //
  // 🔴 LE SERVEUR NOMME SES COFFRES « Storage Alt 3 », ET CELA NE DIT RIEN. Le
  // joueur, lui, sait que c'est son coffre à consommables : il le nomme, ou il
  // le reconnaît à une potion rouge. C'est entièrement CLIENT — aucun paquet.
  SkinImage icon;
  // L'IDENTIFIANT DE CET OBJET, zéro quand aucune icône n'a été demandée.
  //
  // ⚠ IL NE SE DÉDUIT PAS DE `icon` : une icône DEMANDÉE que l'installation n'a
  // pas rend une image vide, et le menu doit pouvoir dire « introuvable pour cet
  // id » plutôt que « glissez-y un objet ». Deux silences différents.
  std::uint32_t icon_item_id = 0;
};

// UNE ENTRÉE DU COMBO DES SOUS-TYPES.
//
// ⚠ `ui` NE SAIT PAS CE QU'EST UN TYPE D'ARME. Elle reçoit une clé et un libellé
// — comme pour un coffre —, et la clé ne lui sert qu'à rendre le choix. C'est
// `app` qui sait ce qu'elle désigne, et qui filtre.
struct StorageSubCategoryView {
  int key = -1;
  std::string_view label;
};

// « Aucun coffre ouvert », le même que celui du serveur. ⚠ CE N'EST PAS ZÉRO :
// zéro EST un coffre, le principal.
inline constexpr std::uint8_t kNoShelfOpen = 0xFF;

// CE QUE LA FENÊTRE SAIT DU MONDE ET DE SES SŒURS, posé par l'appelant à chaque
// image. ⚠ ELLE NE L'ÉCRIT JAMAIS, et c'est pour cela qu'il vit hors de son
// état : `ui` ne possède aucun état de jeu.
struct StorageWindowView {
  // LE NOM QUE LE SERVEUR A DONNÉ au coffre ouvert, posé par l'appelant.
  //
  // ⚠ C'EST LA SEULE CHOSE QUI SÉPARE le coffre personnel de celui de la
  // guilde : le serveur les annonce tous deux sous le même numéro de contenant.
  // Vide tant qu'il n'a rien dit, et le titre retombe alors sur « Storage ».
  std::string_view name;
  // LES COFFRES QU'ON PEUT OUVRIR, et lequel l'est — posés par l'appelant.
  //
  // ⚠ DES VUES, et elles ne valent que pour l'image : la liste vit dans le
  // monde. Vides tant que le serveur n'a rien dit — la rangée ne paraît alors
  // pas, et la fenêtre est celle d'avant.
  //
  // ⚠ ET LA RANGÉE NE PARAÎT PAS NON PLUS POUR UN SEUL COFFRE : un onglet
  // unique n'offre aucun choix et prend une ligne à la table.
  std::span<const StorageShelfView> shelves;
  std::uint8_t open_shelf = kNoShelfOpen;
  // LES SOUS-TYPES PRÉSENTS, sous l'onglet — voir `sub_category`. ⚠ DES VUES,
  // qui ne valent que pour l'image, comme celles des coffres.
  std::span<const StorageSubCategoryView> sub_categories;
  // Le personnage pousse-t-il une charrette ? Sans elle, rien à proposer.
  bool cart_available = false;
  // ⚠ L'ÉTAT DES DEUX AUTRES, pour que chaque raccourci dise ce que le clic VA
  // faire : son libellé est le NOM de la fenêtre, pas le geste.
  bool inventory_open = false;
  bool cart_open = false;
  // UNE ÉCHOPPE SE PRÉPARE : le coffre ne rend plus rien, ni au sac ni à la
  // charrette. Le fermer reste permis.
  bool transfers_frozen = false;
  // « Voir dans l'album » au menu d'une carte. ⚠ DES VUES, pour l'image.
  CardAlbumLinkFacts album_links;
};

// Ce que la fenêtre garde d'une image à l'autre.
struct StorageWindowState {
  // ⚠ CE N'EST PAS LE JOUEUR QUI LE POSE : il recopie `world.storage_open()`.
  // Une fenêtre qu'on ouvrirait sans le serveur montrerait un coffre vide dont
  // aucun geste n'aboutirait.
  bool open = false;
  ItemTab tab = ItemTab::kAll;
  char filter[64] = {};
  bool filter_shown = false;

  // ── CE QUE LA BANDE D'ONGLETS MONTRE, ET COMMENT ───────────────────────────
  //
  // ⚠ COUCHÉE PAR DÉFAUT ICI, DEBOUT CHEZ SES DEUX SŒURS, et c'est le seul de
  // leurs réglages dont le défaut diverge : la largeur du coffre est déjà
  // disputée par ses colonnes, et une bande à gauche la lui prendrait deux fois.
  //
  // 🔴 ELLE COMMANDE AUSSI L'ORIENTATION DES COFFRES, qui lui restent
  // PERPENDICULAIRES — et elle la commande encore quand la bande des catégories
  // est décochée, sans quoi les coffres pivoteraient en la masquant.
  bool tabs_vertical = false;
  // LES TUILES DU JEU, ou le libellé écrit. Voir `TabStripLook`.
  bool tab_images = true;
  // LA BANDE DES CATÉGORIES PARAÎT-ELLE ?
  //
  // ⚠ SANS ELLE, LA VUE REPASSE À « TOUT » : un onglet qui filtre sans se
  // montrer cacherait des lignes que rien n'expliquerait. C'est la fenêtre qui
  // s'en charge, pas l'appelant — elle est seule à savoir qu'elle ne l'a pas
  // dessinée.
  bool type_tabs = true;
  // LA RANGÉE DES COFFRES PARAÎT-ELLE ? ⚠ ELLE NE PARAÎT DE TOUTE FAÇON PAS SOUS
  // DEUX COFFRES : ce réglage la retire, il ne la fait pas apparaître.
  bool shelf_row = true;

  // ── LES COLONNES QUI NE VONT PAS DE SOI ────────────────────────────────────
  //
  // 🔴 LE PANNEAU EST LA SEULE VÉRITÉ, et c'est pour cela que la table n'offre
  // plus son menu d'en-tête : deux endroits pour montrer la même colonne, c'est
  // deux états à tenir d'accord — l'un dans notre fichier de réglages, l'autre
  // dans `imgui.ini` — et le joueur aurait vu sa colonne revenir au démarrage
  // suivant.
  //
  // ⚠ L'OBJET ET LA QUANTITÉ N'EN SONT PAS : une table sans nom d'objet ne
  // montre plus rien, et sans quantité elle ment sur ce qu'on retire.
  // REMETTRE L'ORDRE DES COLONNES D'APLOMB.
  //
  // ⚠ UNE DEMANDE, CONSOMMÉE PAR LA TABLE À L'IMAGE SUIVANTE : l'ordre vit dans
  // `imgui.ini`, et seule la table ouverte peut le défaire. Le joueur peut la
  // poser coffre FERMÉ — c'est pour cela qu'elle attend plutôt qu'elle n'agit.
  bool reset_columns = false;
  bool slot_column = false;
  // ⚠ LE PRIX EST MONTRÉ D'AVANCE, à rebours des trois autres, et ce défaut a
  // sa raison — la colonne ne coûte rien à qui range son
  // coffre, et elle répond à la seule question qu'on se pose devant six cents
  // lignes oubliées.
  bool value_column = true;
  bool total_value = true;
  bool item_id_column = false;
  bool card_slots_column = false;

  // Le titre complet, recomposé au changement de nom ET de langue seulement.
  // L'identité ImGui, elle, ne bouge pas : la place et la taille que le joueur a
  // données survivent au passage d'un coffre à l'autre.
  std::string title;
  std::string title_for;
  std::size_t title_language = static_cast<std::size_t>(-1);

  // L'ordre d'affichage des lignes, et l'empreinte de ce qui l'a produit.
  //
  // ⚠ IL VIT ICI ET NON DANS UNE STATIQUE : `ui` ne possède rien de global, et
  // deux coffres ouverts ne doivent pas se voler leur tri.
  std::vector<int> order;
  std::uint64_t sorted_for = 0;

  // LA PLACE DE LA LIGNE QUI SE DESSINE MÊME HORS DE VUE — celle dont le menu
  // est ouvert, ou que la souris tient : zéro si aucune.
  //
  // 🔴 SEULES LES LIGNES VISIBLES SE DESSINENT, et ImGui ferme un menu dont
  // l'objet ne se pose plus, comme il lâche un glisser dont la source a
  // disparu. Faire défiler la table sous l'un ou l'autre les perdrait.
  std::uint16_t held_slot = 0;

  // La question en cours, quand une pile ne sort pas entière.
  ItemQuantityAsk ask;
  // 🔴 VERS OÙ ELLE PARTIRA. Deux destinations demandent « combien » — le sac et
  // la charrette —, et sans ce souvenir la réponse irait toujours à la première.
  bool ask_to_cart = false;

  // ── LE COFFRE QU'ON EST EN TRAIN DE RENOMMER ───────────────────────────────
  //
  // ⚠ LE TAMPON VIT ICI, pas dans une statique de fichier : deux fenêtres de
  // coffre ne doivent pas se voler leur saisie, et `ui` ne possède rien de
  // global. Moins de zéro : personne ne renomme.
  int naming_shelf = -1;
  char shelf_name[24] = {};
  // L'IDENTIFIANT D'OBJET QU'ON EST EN TRAIN DE TAPER, pour l'icône d'un coffre.
  //
  // ⚠ UN TAMPON DE SAISIE, comme le nom au-dessus : il part de ce que le coffre
  // porte quand le menu s'ouvre, et ce qu'il devient est dit par l'action.
  int shelf_icon = 0;

  // ── LE SOUS-TYPE, SOUS L'ONGLET ────────────────────────────────────────────
  //
  // 🔴 UN ONGLET NE SUFFIT PLUS PASSÉ QUELQUES CENTAINES DE LIGNES : on y cherche
  // « mes dagues », pas « mon équipement ». La liste vient de la vue et ne
  // porte QUE ce qui est présent — un sous-type sans objet ne se propose pas.
  //
  // Ce que le joueur a choisi, ou `kAnySubCategory`. ⚠ LA FENÊTRE L'ÉCRIT et
  // l'appelant le lit à l'image suivante, comme l'onglet.
  int sub_category = kAnySubCategory;

  // L'APERÇU DE DESCRIPTION REMPLACE-T-IL L'INFOBULLE DE NOM ?
  //
  // ⚠ IL LA REMPLACE, il ne s'y ajoute pas : deux infobulles pour le même objet
  // se recouvriraient. C'est un réglage, parce qu'un pavé de description au
  // moindre passage de souris gêne qui range son sac.
  bool hover_preview = false;

  // LES RACCOURCIS DE BARRE DE TITRE vers les deux autres fenêtres d'objets.
  // ⚠ OPT-IN, comme chez ses deux sœurs : ce sont des AJOUTS à la barre du
  // client, pas des pièces de son dessin.
  bool peer_buttons = false;
};

// CE QU'ON PEUT FAIRE D'UN OBJET DU COFFRE : l'en sortir, et refermer.
//
// ⚠ DEUX SENS SORTANTS, ET PAS UN SEUL ENTRANT. Ce qui ENTRE dans le coffre part
// de la fenêtre d'où l'objet vient — le sac, la charrette —, qui émet son propre
// paquet. Une fenêtre ne range que ce qu'elle montre.
enum class StorageVerb : std::uint8_t {
  kNothing,
  kToInventory,
  kToCart,
  // 🔴 FERMER EST UN VERBE, pas un effet d'affichage : le serveur doit
  // l'apprendre. La fenêtre ne disparaît qu'à sa réponse.
  kClose,
};

struct StorageWindowAction {
  // L'IDENTIFIANT DE L'OBJET SURVOLÉ, zéro si aucun.
  //
  // ⚠ L'IDENTIFIANT ET NON LA PLACE : l'aperçu montre une DESCRIPTION, qui ne
  // dépend pas du contenant. C'est aussi ce qui permet à l'étage de la bâtir
  // sans savoir de quelle fenêtre le survol venait.
  std::uint32_t hovered_id = 0;

  StorageVerb verb = StorageVerb::kNothing;
  // CE QU'UNE AUTRE FENÊTRE VIENT DE LÂCHER ICI. ⚠ RENDU PAR CELLE QUI REÇOIT :
  // c'est elle qui a vu le lâcher, et `app` sait quel paquet les deux bouts
  // appellent. Place nulle = rien.
  ItemTransferAsk received;

  // LE RETRAIT N'AVAIT NULLE PART OÙ ALLER : ni le sac ni la charrette ne sont
  // ouverts. ⚠ CE N'EST PAS UNE ERREUR mais une absence de destination, et le
  // joueur doit l'apprendre — sinon le clic paraît ne rien faire.
  bool nowhere_to_take = false;
  // L'OBJET DONT IL VEUT LA DESCRIPTION, zéro si aucun.
  //
  // ⚠ UN IDENTIFIANT, PAS UN VERBE. Décrire n'envoie rien au serveur : c'est une
  // fenêtre qu'on ouvre, et elle survit à l'objet qui l'a ouverte. La PLACE
  // l'accompagne pour décorer le nom — raffinement, cartes — tant que l'objet
  // est là.
  std::uint32_t describe_id = 0;
  std::uint16_t describe_slot = 0;
  // « Voir dans l'album » : la carte dont il veut la pochette ; zéro sinon.
  std::uint32_t see_in_album_id = 0;

  // LE COFFRE QU'ON VIENT DE DEMANDER, par son NUMÉRO ; moins de zéro si aucun.
  //
  // ⚠ UN NUMÉRO ET NON UN RANG : la liste est refaite à chaque image, et le
  // rang ne veut rien dire une image plus tard. C'est aussi ce que le serveur
  // attend.
  int chose_shelf = -1;

  // L'OBJET DONT IL VEUT BASCULER LE FAVORI, zéro si aucun.
  //
  // 🔴 UN IDENTIFIANT ET NON UNE PLACE : un favori de coffre vaut pour TOUTES
  // les piles du même objet, et il survit à la sortie puis au retour de l'objet.
  // La place, elle, ne désigne qu'une pile et se renumérote.
  //
  // ⚠ ET C'EST `app` QUI BASCULE : la liste vit dans un fichier du joueur, et
  // la fenêtre ne sait rien écrire.
  std::uint32_t toggle_favorite_id = 0;

  // ── CE QUE LE JOUEUR VIENT DE FAIRE D'UN ONGLET DE COFFRE ─────────────────
  //
  // ⚠ LE NOM EST DANS L'ÉTAT (`shelf_name`), pas ici : c'est un tampon de
  // saisie, et le recopier dans l'action à chaque frappe n'apprendrait rien.
  // Ce champ dit seulement QUEL coffre vient d'être renommé.
  int renamed_shelf = -1;
  // L'icône qu'on vient de lui donner — zéro la retire.
  int decorated_shelf = -1;
  std::uint32_t decorate_item_id = 0;
  // 🔴 « TOUT OUBLIER » EST UNE ACTION À PART, et il en faut une : le nom et
  // l'icône se posent chacun de son côté, et deux intentions ne partent pas dans
  // la même image. Les enchaîner aurait perdu la première.
  int reset_shelf = -1;

  // Les raccourcis de barre de titre vers les deux autres fenêtres d'objets.
  bool toggle_inventory = false;
  bool toggle_cart = false;
  // La puce de la barre de titre a été cliquée. Voir `InventoryWindowAction`.
  bool settings_asked = false;
  std::uint16_t slot = 0;
  int amount = 1;
  std::uint16_t hovered_slot = 0;
};

// ⚠ PAS DE CONSTANTE DE TITRE, contrairement à ses deux sœurs : celui du coffre
// vient du SERVEUR. Le repli « Storage » est écrit là où il sert, parce que le
// relèvé des traductions ne lit que les littéraux.
inline constexpr const char* kStorageWindowId = "storage";

// Les sept onglets du coffre — les mêmes que le sac.
//
// 🔴 SES FAVORIS NE VIENNENT PAS DU SERVEUR : la structure d'un objet de coffre
// n'a pas le champ que celle d'un objet de sac porte. La liste est donc à NOUS,
// dans un fichier du joueur.
[[nodiscard]] std::span<const ItemTab> StorageTabs();

// Le libellé d'une colonne.
[[nodiscard]] const char* StorageColumnLabel(StorageColumn column);

// L'empreinte de ce que le tri lit. ⚠ ELLE PORTE TOUT CE QUI PEUT CHANGER SANS
// QUE LA LISTE CHANGE — la quantité surtout, qu'un retrait partiel fait bouger
// sans retirer la ligne.
[[nodiscard]] std::uint64_t StorageRowsFingerprint(std::span<const InventoryCellView> cells);

[[nodiscard]] StorageWindowAction DrawStorageWindow(StorageWindowState& state,
                                                    const StorageWindowView& view,
                                                    std::span<const InventoryCellView> cells,
                                                    StorageTally tally, const SkinArt& art);

}  // namespace ro::ui
