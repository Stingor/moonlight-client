#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ro::render {

// L'ÉTIQUETTE D'UN ACTEUR : les lignes qu'elle montre, et leur couleur.
//
// 🔴 CETTE COUCHE NE SAIT PAS CE QU'EST UN JOUEUR. Elle reçoit cinq chaînes et
// deux drapeaux, comme l'enseigne reçoit un titre et des couleurs ; c'est `app`
// qui traduit ce que le monde porte. Même frontière que partout.
//
// ⚠ ET LE CLIENT N'ÉCRIT PAS TOUS LES NOMS EN PERMANENCE. Il n'écrit que celui
// que la souris DÉSIGNE, ou celui de la dernière entité cliquée : un seul
// widget, créé paresseusement, replacé à chaque survol. C'est ce qui garde une carte
// peuplée lisible, et c'est la raison pour laquelle une carte pleine de
// monstres ne se couvre pas de texte.

// LES COULEURS : celle de l'espèce, puis les surcharges que le client applique
// ensuite.
//
// 🔴 CE SONT DES `COLORREF` WINDOWS, DONC `0x00BBGGRR` — le ROUGE EN DERNIER.
// Le client les passe telles quelles à `SetTextColor` de GDI : la valeur
// `0xC3C3FF` se lit donc `#FFC3C3`, un ROSE clair, et non le bleu-violet qu'on
// lirait à l'endroit.
//
// ⚠ UNE CAPTURE LE CONFIRME : la surcharge d'écart de niveau vaut `255`, et le
// client d'origine rend `#FF0000` au pixel. Une lecture à l'endroit en ferait
// du bleu pur.
//
// ⚠ UNE SURCHARGE NE SE PORTE PAS, ET SON ABSENCE EST MESURÉE, non oubliée :
// celle de la liste de MARQUAGE demande une liste que rien ne remplit encore.
inline constexpr std::array<std::uint8_t, 4> kPlayerNameColor = {255, 255, 255, 255};
inline constexpr std::array<std::uint8_t, 4> kMonsterNameColor = {0xFF, 0xC3, 0xC3, 255};
inline constexpr std::array<std::uint8_t, 4> kOwnNameColor = {0xC8, 0xC8, 0xC8, 255};
inline constexpr std::array<std::uint8_t, 4> kHostileNameColor = {0x95, 0xB8, 0xF7, 255};

// L'ÉCART DE NIVEAU, la surcharge la plus voyante du client : ce qui nous
// dépasse de plus de dix niveaux s'écrit en ROUGE, ce qu'on dépasse de plus de
// quinze en GRIS. Entre les deux, la couleur d'espèce reste.
//
// ⚠ ELLE NE VAUT QUE POUR UN MONSTRE dont on connaît le niveau, et c'est ce
// qui la rendait invisible ici : le paquet d'apparition le porte, nous le
// jetions. Voir `sim::Actor::level`.
inline constexpr std::array<std::uint8_t, 4> kOverLevelNameColor = {0xFF, 0x00, 0x00, 255};
inline constexpr std::array<std::uint8_t, 4> kUnderLevelNameColor = {0x9B, 0x9B, 0x9B, 255};

// Les deux bornes de cet écart, telles que le client les compare :
// `mon_niveau - son_niveau`, rouge en DESSOUS de la première, gris AU-DESSUS de
// la seconde.
inline constexpr int kOverLevelGap = -10;
inline constexpr int kUnderLevelGap = 15;

// La couleur d'un nom une fois l'écart de niveau pris en compte.
//
// `mine` ou `theirs` à zéro veut dire « on ne sait pas », et la couleur
// d'espèce passe : c'est le `> 0` du client, et il compte — un monstre dont le
// niveau manque ne doit pas se peindre comme s'il valait zéro, ce qui le
// mettrait en gris pour tout le monde.
[[nodiscard]] std::array<std::uint8_t, 4> NameColorForLevelGap(
    const std::array<std::uint8_t, 4>& by_kind, int mine, int theirs);

// Ce que le client laisse autour du texte d'une étiquette : quatre pixels de
// chaque côté, soit huit de plus que le texte en largeur.
// Voir `ro/render/name_plate_layout.h` pour ce qui se pose à gauche de ce texte.
inline constexpr float kNamePlateMarginPixels = 4.0F;

// CE QU'UNE ÉTIQUETTE A À ÉCRIRE, déjà résolu : le titre est un LIBELLÉ et non
// un numéro, parce que ce qui le traduit est une table de données de
// l'installation.
struct NamePlateText {
  std::string_view name;
  std::string_view title;
  // Ce qui s'écrit entre parenthèses à la suite du nom, et la seconde ligne
  // avec ce qui s'y écrit entre crochets. Voir `sim::NameCard` : ce sont des
  // PLACES, et le serveur y met autre chose pour un monstre.
  std::string_view aside;
  std::string_view banner;
  std::string_view badge;
  // Le nom se coupe au croisillon — voir plus bas. ⚠ C'EST CE QUI N'EST PAS UN
  // JOUEUR, et le champ le dit plutôt que de nommer une espèce : `render` ne
  // sait pas ce qu'est un joueur, et c'est `app` qui tranche.
  bool cut_at_hash = false;
};

// 🔴 LE TYPE DE SERVICE CHOISIT LE GABARIT DES DEUX SUITES. Quand il vaut zéro —
// ce que `korea` pose, et ce que le `clientinfo.xml` de cette installation
// déclare —, les DEUX lignes se composent « base (suite) », suivies d'une
// espace : la ligne du nom comme celle de la bannière. Autrement, c'est « base
// (suite) » pour le nom et « base [suite] » pour la bannière, sans espace.
//
// ⚠ LA SUITE N'EST PAS JETÉE, et le niveau et la vie d'un monstre que ce
// serveur envoie (`show_mob_info: 6`) se lisent donc bien après son nom. Le
// gabarit coréen d'origine l'encadrait de crochets pleine chasse que la police
// du jeu ne sait pas dessiner ; celui du client du joueur les remplace par des
// parenthèses ordinaires.
struct NamePlateLook {
  bool korean_service = false;
};

// LES LIGNES DE L'ÉTIQUETTE, de haut en bas. Vide quand il n'y a rien à écrire.
//
// La composition est celle du client, à la lettre :
//
//   · le TITRE se met DEVANT le nom, entre crochets — « [Master] Stingor » ;
//   · l'aside suit, entre parenthèses — et seulement s'il y en a un ;
//   · la bannière fait une SECONDE LIGNE, son insigne à la suite. ⚠ Une
//     bannière vide emporte l'insigne : un membre de clan, dont le serveur ne
//     remplit QUE l'insigne, ne montre donc rien de son clan. C'est le client
//     qui est ainsi ;
//   · ⚠ ET L'INSIGNE, LUI, S'ÉCRIT MÊME VIDE : le client ne regarde que la
//     bannière, et compose la seconde ligne avec ses deux champs quoi qu'il en
//     soit — « Guilde () » pour une bannière sans insigne ;
//   · 🔴 CE QUI N'EST PAS UN JOUEUR SE COUPE AU CROISILLON. Le client cherche un
//     « # » et tronque là, sur un drapeau VRAI hors des métiers de joueur.
//     ⚠ C'est le nom de sept mille cent quarante et un PNJ de ce serveur qui
//     en dépend : leur nom de script porte un croisillon suivi d'un identifiant que l'auteur s'est
//     écrit à lui-même — « Fisk#a2t », « Kafra Employee#alde ».
//
// ⚠ AUCUN MOT N'EST MASQUÉ, comme chez le joueur : le client passe chaque champ
// par sa liste de mots proscrits, mais celui du joueur ne charge pas cette
// liste, et elle reste vide.
[[nodiscard]] std::vector<std::string> ComposeNamePlateLines(const NamePlateText& what,
                                                             const NamePlateLook& look);

}  // namespace ro::render
