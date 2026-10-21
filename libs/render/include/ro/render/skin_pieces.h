#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/formats/image.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// LES PIECES DU SKIN RO : les bitmaps dont le client compose ses fenêtres.
//
// Une fenêtre de Ragnarok n'est pas peinte, elle est ASSEMBLÉE : une barre de
// titre en trois morceaux dont celui du milieu s'étire, un cadre en huit
// morceaux autour d'un fond uni, des boutons en trois morceaux à leur tour.
// C'est ce découpage — et non une image par fenêtre — qui laisse une fenêtre
// prendre n'importe quelle taille sans se déformer.
//
// ⚠ ELLES VIENNENT DE L'INSTALLATION DU JOUEUR, et rien n'est versionné ici.
// C'est la règle du dépôt, et c'est aussi ce qui rend les skins de joueur
// possibles : qui remplace un de ces bitmaps dans son GRF voit son habillage,
// comme avec le client d'origine.
//
// ⚠ ET UNE PIÈCE MANQUANTE N'EMPÊCHE PAS DE JOUER. Elle laisse sa case vide et
// l'interface se rabat sur un aplat. Refuser de démarrer pour un bitmap de
// bordure serait hors de proportion — même règle que les pièces du jeu.

// Le dossier des textures d'interface, en coréen comme dans l'archive.
inline constexpr std::string_view kSkinRoot = "data\\texture\\유저인터페이스\\";

// Chaque pièce du découpage. L'ordre n'a aucun sens en soi : c'est un
// CATALOGUE, et l'on n'y cherche qu'une entrée à la fois.
enum class SkinPiece : std::uint8_t {
  kTitleBarLeft,
  kTitleBarMiddle,
  kTitleBarRight,
  kBulletOff,
  kBulletOn,
  kCloseOff,
  kCloseOn,
  kCollapseOff,
  kCollapseOn,
  kPinLoose,
  kPinDriven,
  kFrameTopLeft,
  kFrameTopMiddle,
  kFrameTopRight,
  kFrameLeft,
  kFrameRight,
  kFrameBottomLeft,
  kFrameBottomMiddle,
  kFrameBottomRight,
  kButtonLeft,
  kButtonMiddle,
  kButtonRight,
  kButtonHoverLeft,
  kButtonHoverMiddle,
  kButtonHoverRight,
  kButtonPressedLeft,
  kButtonPressedMiddle,
  kButtonPressedRight,
  kScrollUp,
  kScrollMiddle,
  kScrollDown,
  kScrollGrabTop,
  kScrollGrabMiddle,
  kScrollGrabBottom,
  kCheckBoxEmpty,
  kCheckBoxTicked,
  kResizeGrip,
  // Les six morceaux d'un ONGLET : gauche, milieu, droite, ouvert et fermé.
  // L'ouvert descend jusqu'au contenu ; le fermé se referme d'un filet en bas.
  kTabLeftOn,
  kTabMiddleOn,
  kTabRightOn,
  kTabLeftOff,
  kTabMiddleOff,
  kTabRightOff,
  // Le PETIT bouton, mêmes trois états. Une barre qui doit partager sa ligne
  // avec autre chose ne peut pas se payer deux boutons pleine taille.
  kSmallButtonLeft,
  kSmallButtonMiddle,
  kSmallButtonRight,
  kSmallButtonHoverLeft,
  kSmallButtonHoverMiddle,
  kSmallButtonHoverRight,
  kSmallButtonPressedLeft,
  kSmallButtonPressedMiddle,
  kSmallButtonPressedRight,
  // La BARRE DE BOUTONS du bas d'une fenêtre : le bandeau sur lequel ils
  // reposent.
  kButtonBarLeft,
  kButtonBarMiddle,
  kButtonBarRight,
  // La barre de titre d'une BOÎTE MODALE et d'un panneau de description. Ce
  // n'est pas celle d'une fenêtre ordinaire : elle est plus haute, et sans
  // boutons système.
  kModalBarLeft,
  kModalBarMiddle,
  kModalBarRight,
  // Le bouton FLÈCHE d'une liste déroulante : au repos, au survol, enfoncé.
  kComboArrow,
  kComboArrowHover,
  kComboArrowPressed,
  // Le bouton radio.
  kRadioOff,
  kRadioOn,
  // ⚠ L'ASCENSEUR HORIZONTAL, ET C'EST LE SLIDER. Le jeu n'a pas de « curseur
  // de réglage » : son équivalent visuel est cette barre-là, flèches comprises,
  // et on la reprend telle quelle plutôt que d'en inventer un.
  kSliderTrackLeft,
  kSliderTrackMiddle,
  kSliderTrackRight,
  kSliderGrabLeft,
  kSliderGrabMiddle,
  kSliderGrabRight,
  // ── Ce qu'une fenêtre à GRILLE D'OBJETS ajoute ──────────────────────────────
  //
  // Le fond d'une case, PAVÉ : la grille n'a pas une image par case mais une
  // seule tuile répétée, alignée sur la première case pour que le fond et les
  // objets partagent la même marge.
  kItemTile,
  // La barre du bas d'une fenêtre à grille. ⚠ CE N'EST PAS `kButtonBar` : le
  // client livre une SECONDE barre, en « 3 », plus haute, et c'est elle que
  // portent l'inventaire et la charrette.
  kItemBarLeft,
  kItemBarMiddle,
  kItemBarRight,
  // Les deux icônes du pied : ce qu'on porte, et combien d'objets on a.
  kWeightIcon,
  kCountIcon,
  // LE VERROU DE JET, éteint et allumé. ⚠ IL N'A PAS D'ART DE SURVOL — le jeu
  // n'en livre que deux états, là où ses voisins du même pied en ont quatre.
  kDropLockOff,
  kDropLockOn,
  // LES DEUX BOUTONS PROPRES AUX FAVORIS : le verrou de vente et le tri.
  //
  // ⚠ L'ALLUMÉ DU TRI N'A PAS DE SUFFIXE — `bt_sort.bmp` face à
  // `bt_sort_off.bmp` —, et rien dans le nom ne le dit. Relevé dans l'archive.
  //
  // ⚠ LE JEU EN LIVRE AUSSI LES SURVOLS ET LES ENFONCÉS ; on ne les prend pas,
  // parce que le verrou de jet n'en a aucun : les trois boutons du pied se
  // comportent ainsi de la même façon.
  kSellLockOff,
  kSellLockOn,
  kSortOff,
  kSortOn,
  // LE FOND PAVÉ VERROUILLÉ. ⚠ C'EST LE SEUL ÉCART DE LA GRILLE DU SAC AVEC
  // CELLE DE LA CHARRETTE : sur les favoris, verrou de vente mis, le client
  // remplace `itemwin_mid` par celui-ci. Il vit dans `inventory`, pas dans
  // `basic_interface` où loge la tuile ordinaire.
  kItemTileLocked,
  // ── Les onglets de catégorie ────────────────────────────────────────────────
  //
  // ⚠ LE SUFFIXE « 1 » EST L'ONGLET ACTIF, « 2 » L'INACTIF — pas l'inverse, et
  // rien dans le nom ne le dit.
  //
  // ⚠ ET « ÉQUIPEMENT » PORTE L'ICÔNE D'APPARAT (`tab_cos`), alors que
  // l'installation livre `tab_wea` et `tab_arm`. C'est voulu : l'interface ne
  // doit pas dériver, et corriger ici changerait un dessin que les joueurs
  // reconnaissent.
  kTabAllOn,
  kTabAllOff,
  kTabConsumableOn,
  kTabConsumableOff,
  kTabEquipmentOn,
  kTabEquipmentOff,
  kTabAmmoOn,
  kTabAmmoOff,
  kTabEtcOn,
  kTabEtcOff,
  kTabCardOn,
  kTabCardOff,
  kTabFavoriteOn,
  kTabFavoriteOff,
  // ── QUATRE TUILES QUE SEUL LE COFFRE EMPLOIE ──────────────────────────────
  //
  // Sa bande sépare ce que les deux fenêtres à grille réunissent : les ARMES des
  // ARMURES, et le CASH des consommables. L'installation en livre l'art, même
  // si le client d'origine ne s'en sert nulle part.
  //
  // ⚠ L'APPARAT N'EN FAIT PAS PARTIE : `tab_cos` est déjà là, sous le nom
  // d'« équipement » — c'est la tuile que le client donne aux deux, et le coffre
  // la reprend pour son onglet « Costumes ».
  kTabWeaponOn,
  kTabWeaponOff,
  kTabArmorOn,
  kTabArmorOff,
  kTabCashOn,
  kTabCashOff,
  // LE MÊME CŒUR, SUR UN FOND BLANC (`tab_favs`).
  //
  // 🔴 CE N'EST PAS UN DOUBLON DE `tab_fav` : celui-là porte le gris-bleu de la
  // GRILLE des favoris, que le coffre n'a pas — sa table ne change pas de fond
  // sous cet onglet. Les deux existent dans l'installation pour cette raison.
  kTabFavoritePlainOn,
  kTabFavoritePlainOff,
  // ── LES MÊMES ONGLETS, COUCHÉS ──────────────────────────────────────────────
  //
  // ⚠ CE N'EST PAS LA MÊME IMAGE TOURNÉE : l'installation en livre un second
  // jeu, `tabh_*`, dessiné pour une rangée — 27 sur 25 là où le jeu debout fait
  // 23 sur 27. Le bord ouvert n'est pas du même côté, et faire pivoter le
  // premier ouvrirait l'onglet sur le vide.
  //
  // Le coffre est la seule des trois fenêtres à s'en servir : sa liste à colonnes se dispute déjà
  // la largeur, ses onglets vont donc en rangée plutôt qu'en bande à gauche.
  kTabHAllOn,
  kTabHAllOff,
  kTabHConsumableOn,
  kTabHConsumableOff,
  kTabHEquipmentOn,
  kTabHEquipmentOff,
  kTabHAmmoOn,
  kTabHAmmoOff,
  kTabHEtcOn,
  kTabHEtcOff,
  kTabHCardOn,
  kTabHCardOff,
  kTabHFavoriteOn,
  kTabHFavoriteOff,
  // Les quatre du coffre, couchées. ⚠ MÊME ORDRE QUE LE JEU DEBOUT : les deux
  // tables du dessin s'indexent par le même rang.
  kTabHWeaponOn,
  kTabHWeaponOff,
  kTabHArmorOn,
  kTabHArmorOff,
  kTabHCashOn,
  kTabHCashOff,
  kTabHFavoritePlainOn,
  kTabHFavoritePlainOff,

  // LES ONGLETS DE COFFRE, debout et couchés.
  //
  // 🔴 CE SONT DES ONGLETS, PAS DES BOUTONS, et l'installation en livre l'art :
  // `tab_sto*` pour la colonne, `tabh_sto*` pour la rangée. Les dessiner en
  // petits boutons du thème donnerait une barre d'outils là où le client montre
  // une rangée d'onglets — et rien ne dirait plus lequel était ouvert autrement
  // que par la teinte d'un bouton.
  //
  // ⚠ ILS N'ONT PAS D'IMAGE PAR COFFRE : le même cadre sert tous les numéros,
  // et c'est ce qu'il PORTE — une icône choisie, ou le numéro — qui les
  // distingue.
  kTabStorageOn,
  kTabStorageOff,
  kTabHStorageOn,
  kTabHStorageOff,

  // ── LA BANQUE DE ZENY ──────────────────────────────────────────────────
  //
  // 🔴 SON FOND EST UNE IMAGE ENTIÈRE, ET C'EST LA SEULE DE TOUTE LA TABLE.
  // Partout ailleurs une fenêtre s'ASSEMBLE — trois morceaux de barre, un
  // aplat, des boutons en trois tranches — parce qu'elle doit prendre
  // n'importe quelle taille. Celle-ci n'en prend qu'une : son bitmap porte
  // déjà le sac de zeny et la bande du montant, peints aux bons pixels, et
  // c'est LUI qui commande la largeur de la fenêtre.
  //
  // ⚠ IL N'EST PAS DANS L'INSTALLATION D'ORIGINE : `bg_bank.bmp` est l'art du
  // client, taillé pour SA fenêtre de 280 sur 153. Celui-ci est redessiné à la
  // taille de la nôtre et vit à côté, sans l'écraser.
  kBankBackground,
  // LE BOUTON « SAC DE ZENY » du pied du sac, qui ouvre la banque. Trois
  // états, comme un bouton du jeu — et non deux comme les interrupteurs du
  // pied : ce n'en est pas un, il ne retient aucun état.
  //
  // ⚠ IL VIT DANS `styleshop`, pas dans `bank` : c'est de l'art AJOUTÉ,
  // que l'installation d'origine ne livre pas.
  kBankButtonOut,
  kBankButtonOver,
  kBankButtonDown,

  // ── LES BOUTONS « OK » ET « CANCEL » D'UN SEUL TENANT ─────────────────────
  //
  // 🔴 LE MOT EST DANS L'IMAGE : ce ne sont pas des boutons à libellé, et ils
  // ne se traduisent pas — le client les montre « OK » et « cancel » dans toutes
  // les langues. Trois états chacun : au repos, survolé, enfoncé.
  //
  // ⚠ ILS SONT À LA RACINE DE L'INTERFACE, pas dans `basic_interface` qui en
  // porte d'autres du même nom et d'un autre dessin.
  kOkButtonOut,
  kOkButtonOver,
  kOkButtonDown,
  kCancelButtonOut,
  kCancelButtonOver,
  kCancelButtonDown,

  // ── LA PETITE FENÊTRE DE STATUT ────────────────────────────────────────
  //
  // 🔴 SON FOND PORTE SES LIBELLÉS, peints : la fenêtre n'écrit que les
  // nombres, et prend la largeur du bitmap comme la banque.
  //
  // ⚠ LA FLÈCHE DE MONTÉE N'A QU'UNE IMAGE pour ses trois états — au repos,
  // survolée, enfoncée —, et c'est le dessin du jeu.
  kStatusBackground,
  kStatusRaiseArrow,

  // ── LA FENÊTRE « BASIC INFO » ─────────────────────────────────────────
  //
  // Le fond de la vue dépliée, barre de titre comprise, puis les jauges de vie
  // et de magie : leur gouttière, et les deux remplissages en trois morceaux —
  // bout gauche, tranche d'un pixel qu'on répète, bout droit. Le bleu sert au
  // plein, le rouge sous le quart.
  kBasicInfoBackground,
  kBasicInfoGaugeTrough,
  kBasicInfoGaugeBlueLeft,
  kBasicInfoGaugeBlueMiddle,
  kBasicInfoGaugeBlueRight,
  kBasicInfoGaugeRedLeft,
  kBasicInfoGaugeRedMiddle,
  kBasicInfoGaugeRedRight,
};

inline constexpr std::size_t kSkinPieceCount =
    static_cast<std::size_t>(SkinPiece::kBasicInfoGaugeRedRight) + 1;

// LE NOMBRE DE CHEMINS DÉCLARÉS, pour que la table et l'énumération ne puissent
// pas diverger. ⚠ UN `std::array` SOUS-REMPLI EST LÉGAL : ajouter une pièce sans
// son chemin ne compile pas moins bien, la pièce cherche seulement un fichier au
// nom vide et manque en silence. Un essai s'en charge.

// Le chemin d'une pièce SOUS le dossier d'interface.
[[nodiscard]] std::string_view SkinPiecePath(SkinPiece piece);

// Ce que l'installation du joueur a donné.
struct SkinPieces {
  // Une image par pièce, vide quand elle manque.
  std::array<std::optional<formats::Image>, kSkinPieceCount> images;
  // Ce qui manquait, par son chemin. À DIRE UNE FOIS au démarrage : une
  // interface sans habillage s'explique alors d'elle-même, au lieu de passer
  // pour une panne de rendu.
  std::vector<std::string> missing;

  [[nodiscard]] const formats::Image* Get(SkinPiece piece) const;
  [[nodiscard]] bool complete() const noexcept { return missing.empty(); }
};

// Lit toutes les pièces. Le magenta pur y devient transparent — c'est la
// convention de transparence des textures du jeu, antérieure au canal alpha.
[[nodiscard]] SkinPieces LoadSkinPieces(vfs::Vfs& vfs);

}  // namespace ro::render
