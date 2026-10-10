// LES RÉGLAGES DES TROIS FENÊTRES D'OBJETS : le sac, la charrette, le coffre.
//
// ⚠ UNE SECTION CHACUNE, et non un bloc commun. C'est là que vient les chercher
// qui sort de la fenêtre — et les trois ne portent pas les mêmes : on JETTE
// depuis un sac, on TRIE dans un coffre.
//
// 🔴 L'ORDRE ET LES LIBELLÉS SONT GRAVÉS, à la ligne près. Ce ne sont pas des
// synonymes : un joueur cherche « Onglets verticaux » et « Prix de revente »,
// pas « Onglets en rangée » et « Colonne « Valeur » ». Les renommer pour mieux
// dire lui coûterait la moitié de ce qu'il sait déjà.
//
// ⚠ AUCUN « TRI SERVEUR », ET C'EST VOULU : le tri se fait dans la table, par
// ImGui, et un combo qui demanderait au serveur de retrier ferait deux tris
// pour un.
//
// 🔴 ELLES VIVENT HORS DE `game_option_rows.cpp`, que le plafond borne, parce
// qu'elles forment un tout : trois fenêtres sœurs, dont les réglages se
// répondent d'une section à l'autre. Un découpage au hasard donnerait deux
// fichiers dont aucun nom ne serait vrai.
//
// 🔴 LES LIGNES SE REFONT À CHAQUE IMAGE, panneau ouvert : c'est ce qui fait
// qu'un changement de langue se voit à l'image suivante.

#include "ro/core/i18n.h"

#include "options/game_options.h"
#include "options/option_row_kinds.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

}  // namespace

void GameOptions::ComposeItemWindowRows() {
  // ── LE SAC ────────────────────────────────────────────────────────────────
  //
  // ⚠ SA PROPRE SECTION : chaque fenêtre porte ses réglages. Les noyer dans
  // « Interface » les ferait chercher ailleurs par qui vient de la fenêtre.
  inventory_rows_.clear();
  inventory_rows_.push_back(
      Toggle(GameOption::kInventoryHoverDescription, Tr("Description au survol"),
             Tr("Le survol d'un item montre sa description entière plutôt que son seul "
                "nom. Elle REMPLACE l'infobulle de nom, elle ne s'y ajoute pas."),
             nullptr, inventory_hover_description_));
  inventory_rows_.push_back(
      Toggle(GameOption::kInventoryFilter, Tr("Champ de filtre"),
             Tr("Affiche la barre de recherche par nom au-dessus de la grille. "
                "Décoche pour gagner une ligne (le filtre est alors vidé)."),
             nullptr, inventory_filter_));
  inventory_rows_.push_back(
      Toggle(GameOption::kInventoryTabsVertical, Tr("Onglets verticaux (à gauche)"),
             Tr("Les onglets longent la grille, debout à sa gauche, comme la fenêtre du "
                "client. Décoche pour une rangée couchée au-dessus.\n\nLes deux jeux "
                "d'images viennent de l'installation : une rangée n'est pas la colonne "
                "tournée."),
             nullptr, inventory_tabs_vertical_));
  inventory_rows_.push_back(
      Toggle(GameOption::kInventoryLockSize, Tr("Verrouiller la taille"),
             Tr("La fenêtre ne peut plus être redimensionnée (elle reste déplaçable)."), nullptr,
             inventory_locked_));
  inventory_rows_.push_back(
      Toggle(GameOption::kInventoryFreeLayout, Tr("Placement libre des items"),
             Tr("Glisse un item sur une case vide pour l'y fixer ; sur une case prise, "
                "les deux s'échangent. Un item sans case attribuée remplit les trous, "
                "si bien qu'un item ramassé ne bouscule plus ta disposition.\n\n"
                "L'onglet « Tout » n'est PAS concerné : il mêle les familles, donc une "
                "case n'y désigne pas la même place que dans l'onglet d'origine.\n\n"
                "Exige la taille verrouillée : une case est un index absolu, qu'un "
                "changement de largeur ferait glisser."),
             nullptr, inventory_free_layout_));
  inventory_rows_.back().indented = true;
  inventory_rows_.back().disabled = !inventory_locked_;
  inventory_rows_.push_back(
      OptionRowView{.id = static_cast<int>(GameOption::kInventoryForgetPlaces),
                    .label = Tr("Oublier les places"),
                    .help = Tr("Rend la grille au remplissage automatique : plus aucun item "
                               "n'a de case à lui.\n\nLes places sont retenues par "
                               "IDENTIFIANT d'item, donc elles survivent à tout — vendre, "
                               "consommer, changer de personnage. Sans ce bouton, rien ne les "
                               "reprendrait."),
                    .kind = OptionKind::kAction,
                    .indented = true});
  inventory_rows_.push_back(
      Toggle(GameOption::kInventoryDropLock, Tr("Verrou de jet"),
             Tr("Empêche de jeter un item au sol, dans tous les onglets. Le serveur "
                "n'en sait rien : c'est un garde-fou contre le geste, et jeter ne se "
                "rattrape pas."),
             nullptr, inventory_drop_lock_));
  // ⚠ LE VERROU DE VENTE EST ICI, LE TRI NON, et ce n'est pas un oubli : le
  // premier est une protection qui vaut tant qu'on ne l'ôte pas, comme celui du
  // jet. Le second est une VUE, attachée à un onglet, et son bouton disparaît
  // quand la taille est verrouillée — une rangée qui ment la moitié du temps.
  inventory_rows_.push_back(
      Toggle(GameOption::kInventorySellLock, Tr("Verrou de vente des favoris"),
             Tr("Protège les items rangés dans l'onglet des favoris : ils ne se vendent "
                "pas à un marchand. Le fond de la grille change pour le dire."),
             nullptr, inventory_sell_lock_));
  inventory_rows_.push_back(
      Toggle(GameOption::kInventoryStorageButton, Tr("Raccourcis vers les autres fenêtres"),
             Tr("Ajoute dans la barre de titre un raccourci vers le storage. Il le DEMANDE au "
                "serveur, comme @storage : celui-ci le refuse si le compte n'en a pas le droit, si "
                "un échange est en cours, ou si le storage de guilde est ouvert."),
             nullptr, inventory_storage_button_));

  // ── LA CHARRETTE ET LE COFFRE ─────────────────────────────────────────────
  //
  // ⚠ CHACUNE SA SECTION, pour la même raison que le sac : c'est là que vient
  // les chercher qui sort de la fenêtre. Le verrou de JET n'y est pas — on ne
  // jette rien depuis une charrette ni depuis un coffre.
  cart_rows_.clear();
  cart_rows_.push_back(
      Toggle(GameOption::kCartHoverDescription, Tr("Description au survol"),
             Tr("Le survol d'un item montre sa description entière plutôt que son seul "
                "nom. Elle REMPLACE l'infobulle de nom, elle ne s'y ajoute pas."),
             nullptr, cart_hover_description_));
  cart_rows_.push_back(Toggle(GameOption::kCartFilter, Tr("Champ de filtre"),
                              Tr("Affiche la barre de recherche par nom au-dessus de la grille. "
                                 "Décoche pour gagner une ligne (le filtre est alors vidé)."),
                              nullptr, cart_filter_));
  cart_rows_.push_back(
      Toggle(GameOption::kCartTabsVertical, Tr("Onglets verticaux (à gauche)"),
             Tr("Les onglets longent la grille, debout à sa gauche, comme la fenêtre du "
                "client. Décoche pour une rangée couchée au-dessus.\n\nLes deux jeux "
                "d'images viennent de l'installation : une rangée n'est pas la colonne "
                "tournée."),
             nullptr, cart_tabs_vertical_));
  cart_rows_.push_back(
      Toggle(GameOption::kCartLockSize, Tr("Verrouiller la taille"),
             Tr("La fenêtre ne peut plus être redimensionnée (elle reste déplaçable)."), nullptr,
             cart_locked_));
  cart_rows_.push_back(
      Toggle(GameOption::kCartStorageButton, Tr("Raccourcis vers les autres fenêtres"),
             Tr("Ajoute dans la barre de titre un raccourci vers le storage. Il le DEMANDE au "
                "serveur, comme @storage : celui-ci le refuse si le compte n'en a pas le droit, si "
                "un échange est en cours, ou si le storage de guilde est ouvert."),
             nullptr, cart_storage_button_));

  // ── LE COFFRE ─────────────────────────────────────────────────────────────
  //
  // 🔴 L'ORDRE EST GRAVÉ, LIGNE POUR LIGNE. Il n'est pas alphabétique et il
  // n'est pas thématique : il va du plus souvent touché au plus rarement, et
  // c'est ce que la main retient.
  storage_rows_.clear();
  storage_rows_.push_back(
      Toggle(GameOption::kStorageHoverDescription, Tr("Description au survol"),
             Tr("Le survol d'un item montre sa description entière plutôt que son seul "
                "nom. Elle REMPLACE l'infobulle de nom, elle ne s'y ajoute pas."),
             nullptr, storage_hover_description_));
  // ⚠ LES ONGLETS DU COFFRE SONT COUCHÉS D'AVANCE, seuls des trois : ses
  // colonnes se disputent déjà la largeur qu'une bande debout lui prendrait.
  //
  // 🔴 ET CE RÉGLAGE COMMANDE AUSSI LES COFFRES, qui lui restent
  // PERPENDICULAIRES — y compris quand la bande des catégories est décochée.
  storage_rows_.push_back(
      Toggle(GameOption::kStorageTabsVertical, Tr("Onglets verticaux (à gauche)"),
             Tr("Dispose les catégories en colonne à gauche, comme la fenêtre du client. "
                "Décoche pour une rangée couchée au-dessus — elles prennent alors de la "
                "hauteur plutôt que de la largeur.\n\nDécide AUSSI de l'orientation des "
                "onglets de storage, qui restent toujours PERPENDICULAIRES aux "
                "catégories."),
             nullptr, storage_tabs_vertical_));
  // ⚠ SANS ONGLETS DE CATÉGORIE IL N'Y A PLUS DE TUILE À DESSINER : la rangée
  // est grisée plutôt que laissée cliquable sans effet.
  storage_rows_.push_back(
      Toggle(GameOption::kStorageTabImages, Tr("Images d'onglet"),
             Tr("Les tuiles peintes du jeu. Décoche pour des onglets écrits : leur nom "
                "y est lisible d'un coup, sans passer la souris dessus.\n\nLe libellé "
                "reste DROIT même debout — le client, lui, l'écrit à la verticale."),
             nullptr, storage_tab_images_));
  storage_rows_.back().disabled = !storage_type_tabs_;
  storage_rows_.push_back(
      Toggle(GameOption::kStorageFilter, Tr("Champ de filtre"),
             Tr("Affiche la barre de recherche par nom au-dessus de la liste. C'est "
                "la fenêtre où l'on cherche, donc la seule des trois où il est posé "
                "d'avance."),
             nullptr, storage_filter_));
  storage_rows_.push_back(
      Toggle(GameOption::kStorageTypeTabs, Tr("Filtres par type d'item"),
             Tr("Les DIX onglets du storage — Tout, favoris, consos, armes, armures, "
                "costumes, cartes, munitions, cash, divers — et le choix de sous-type "
                "qui en dépend.\n\nIl sépare ce que l'inventory réunit : c'est la fenêtre où "
                "l'on cherche, et « équipement » y mêlerait les dagues et les capes."
                "\n\nSans elle, la liste repasse à « Tout » : un onglet qui filtrerait "
                "sans se montrer cacherait des lignes sans que rien ne l'explique."),
             nullptr, storage_type_tabs_));
  storage_rows_.push_back(
      Toggle(GameOption::kStorageShelfRow, Tr("Onglets de storage"),
             Tr("Un onglet par storage auquel le personnage a accès — le principal et "
                "ses alternatifs : un clic bascule de l'un à l'autre sans repasser par "
                "@storage.\n\nIls se posent PERPENDICULAIREMENT aux catégories, et ne "
                "paraissent qu'à partir de DEUX storages : un onglet seul n'offre aucun "
                "choix.\n\nClic droit sur un onglet pour lui donner un nom et une icône — "
                "en tapant un identifiant d'item, ou en glissant l'item sur l'onglet."),
             nullptr, storage_shelf_row_));
  storage_rows_.push_back(
      Toggle(GameOption::kStorageTotalValue, Tr("Valeur estimée du storage"),
             Tr("La somme des prix de revente des items AFFICHÉS, au-dessus de la "
                "liste.\n\nElle suit donc l'onglet, le sous-type et le filtre : c'est ce "
                "qui SE VOIT qu'elle additionne, pas tout le storage."),
             nullptr, storage_total_value_));
  storage_rows_.push_back(
      Toggle(GameOption::kStoragePeerButtons, Tr("Raccourcis vers les autres fenêtres"),
             Tr("Ajoute dans la barre de titre de quoi ouvrir ou refermer les fenêtres "
                "Inventory et Cart."),
             nullptr, storage_peer_buttons_));
  // ── LES COLONNES ──────────────────────────────────────────────────────────
  //
  // 🔴 ELLES SE COMMANDENT D'ICI ET DE NULLE PART AILLEURS. Le menu d'en-tête de
  // la table offrirait le même choix, qu'ImGui retient dans `imgui.ini` : deux
  // mémoires pour la même colonne, et celle qu'on vient de décocher reviendrait
  // au démarrage suivant.
  //
  // ⚠ L'OBJET ET LA QUANTITÉ N'ONT PAS DE LIGNE : une table sans nom d'objet ne
  // montre plus rien, et sans quantité elle ment sur ce qu'on retire.
  storage_rows_.push_back(Toggle(GameOption::kStorageSlotColumn, Tr("Index"),
                                 Tr("La place de l'item dans le storage. Un item rangé "
                                    "récemment porte un numéro élevé : trier dessus remonte "
                                    "les derniers dépôts."),
                                 nullptr, storage_slot_column_));
  storage_rows_.back().heading = Tr("Colonnes");
  storage_rows_.push_back(Toggle(GameOption::kStorageItemIdColumn, Tr("ID d'item"),
                                 Tr("L'identifiant numérique de l'item, celui des scripts et "
                                    "des bases du serveur."),
                                 nullptr, storage_item_id_column_));
  storage_rows_.push_back(
      Toggle(GameOption::kStorageCardSlotsColumn, Tr("Slots"),
             Tr("Combien de cartes la pièce peut recevoir. Le nom l'écrit déjà sous la "
                "forme « [2] » ; la colonne, elle, se TRIE."),
             nullptr, storage_card_slots_column_));
  storage_rows_.push_back(
      Toggle(GameOption::kStorageValueColumn, Tr("Prix de revente"),
             Tr("Ce que la pile se REVEND chez un PNJ — le prix unitaire multiplié par "
                "la quantité.\n\nLe prix vient du SERVEUR, qui l'envoie avec le contenu "
                "du storage : la colonne écrit « -- » tant qu'il n'a rien dit."),
             nullptr, storage_value_column_));
  storage_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kStorageCardSlotsColumn),
                                        .label = Tr("Ordre : glissez un en-tête de colonne."),
                                        .kind = OptionKind::kNote});
  storage_rows_.push_back(
      OptionRowView{.id = static_cast<int>(GameOption::kStorageResetColumns),
                    .label = Tr("Remettre l'ordre d'origine"),
                    .help = Tr("Rend aux colonnes leur ordre et leurs largeurs de départ.\n\n"
                               "L'ordre se règle dans la fenêtre, en glissant un en-tête ; "
                               "c'est ImGui qui le retient, et ce bouton est la marche "
                               "arrière."),
                    .kind = OptionKind::kAction});
}

}  // namespace moonclient
