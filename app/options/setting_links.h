#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "options/option_sections.h"

namespace moonclient {

// LES DESTINATIONS QU'UN LIEN DE RÉGLAGE PEUT DÉSIGNER.
//
// Un `<SETL>clé:libellé</SETL>` — écrit par un script de PNJ, ou posé dans le
// chat d'un Maj+clic — mène le lecteur à ce dont on lui parle. La balise ne dit
// pas QUELLE fenêtre ouvrir, seulement CE qu'elle désigne : c'est au client de
// savoir où cela vit chez lui.
//
// 🔴 LES CLÉS SONT L'UNIQUE ESPACE DE NOMS PARTAGÉ — avec les autres clients,
// avec les scripts du serveur, avec Discord. Des liens sont déjà écrits dans des scripts
// et dans des lignes relayées depuis des mois : changer une clé les casserait
// tous, en silence. Une clé ne se renomme donc jamais, et une nouvelle clé est
// en snake_case anglais, sans `:` — un autre lecteur coupe la balise au
// premier — ni chevron.
//
// 🔴 UNE SEULE TABLE, ET LE GENRE EST UN CHAMP. Une page de réglages et un
// onglet de fenêtre partagent l'espace de clés : deux tables se numéroteraient
// en parallèle, et la recherche de l'une répondrait un jour pour l'autre.
//
// ⚠ UNE CLÉ INCONNUE, OU UNE DESTINATION QUE CE LECTEUR N'A PAS, NE FORME AUCUN
// LIEN : le libellé de repli que la balise transporte reste en texte simple. Il
// dit encore de quoi on parle, il ne prétend plus mener quelque part.

// CE QU'UNE DESTINATION OUVRE.
enum class SettingDestinationKind : std::uint8_t {
  // Une page du panneau de réglages, et en option l'un de ses onglets.
  kOptions,
  // Un onglet de la fiche du personnage.
  kSheet,
  // Un onglet de la fenêtre des succès.
  kAchievements,
  // Un onglet de la fenêtre Groupe / Amis.
  kPartyFriend,
  // Un onglet de la fenêtre des raccourcis clavier.
  kHotkeys,
};

// « Pas d'onglet » : la page entière.
inline constexpr int kNoSettingTab = -1;

struct SettingDestination {
  // La clé écrite dans la balise.
  std::string_view key;
  // Le libellé, en français et marqué pour le catalogue : il se traduit chez le
  // LECTEUR, jamais chez l'expéditeur du lien. Pour un onglet, le nom de
  // l'onglet seul — la page ou la fenêtre qui le porte se nomme à part.
  const char* label = "";
  SettingDestinationKind kind = SettingDestinationKind::kOptions;
  // La page, pour `kOptions` ; `kCount` pour les autres genres.
  OptionSection section = OptionSection::kCount;
  // L'onglet, par son rang dans la fenêtre ou dans la page ; `kNoSettingTab`
  // pour une page entière.
  int tab = kNoSettingTab;
  // Un numéro posé après le libellé — « Barre 3 » —, ou zéro.
  int ordinal = 0;
};

// CE QUE LE LECTEUR A, À CETTE IMAGE, pour qu'une destination conditionnelle
// soit offerte. ⚠ PAR DÉFAUT RIEN : qui ne sait pas ne promet pas, et un lien
// qui n'ouvrirait rien serait une promesse trahie.
struct SettingReach {
  bool homunculus = false;
  bool mercenary = false;
  // Le catalogue des succès est lu : leur fenêtre a de quoi se montrer.
  bool achievements = false;
};

// La table entière, dans son ordre. ⚠ LA PREMIÈRE CLÉ D'UNE DESTINATION EST
// CELLE QU'UN MAJ+CLIC POSE : les suivantes ne sont là que pour être LUES.
[[nodiscard]] std::span<const SettingDestination> SettingDestinations();

// La destination de cette clé, ou nul.
[[nodiscard]] const SettingDestination* FindSettingDestination(std::string_view key);

// LA CLÉ QU'UN MAJ+CLIC POSE pour cette page et cet onglet, ou pour cet onglet
// de fenêtre : la première de la table. Nul quand aucune ne les désigne.
[[nodiscard]] const SettingDestination* SettingDestinationOf(SettingDestinationKind kind,
                                                             OptionSection section, int tab);

// LA DESTINATION EST-ELLE OFFERTE À CE LECTEUR ? Un onglet d'homoncule sans
// homoncule ne mène nulle part : la fiche ne le montre pas.
[[nodiscard]] bool SettingDestinationReachable(const SettingDestination& destination,
                                               const SettingReach& reach);

// LE CHEMIN, TRADUIT CHEZ LE LECTEUR : « Interface », « Dégâts › Couleurs »,
// « Personnage › Titres ».
[[nodiscard]] std::string SettingDestinationPath(const SettingDestination& destination);

// LE MÊME CHEMIN EN FRANÇAIS NU : c'est le libellé de repli que la balise
// transporte, pour qui ne connaît pas la clé. Il ne dépend ainsi pas de la
// langue dans laquelle joue l'expéditeur.
[[nodiscard]] std::string SettingDestinationFallback(const SettingDestination& destination);

// LE LIBELLÉ D'UN LIEN DANS LE CHAT : « [Réglage: Dégâts › Couleurs] »,
// « [Fenêtre: Personnage › Titres] ». ⚠ DES CROCHETS, comme un style : ils
// FONT le libellé, et un lien de réglage n'a pas de menu d'objet.
[[nodiscard]] std::string SettingLinkLabel(const SettingDestination& destination);

// LE CHEMIN COMPLET DE L'INFOBULLE : « Réglages › UI › Interface » pour une
// page, « Personnage › Titres » pour une fenêtre.
[[nodiscard]] std::string SettingDestinationTip(const SettingDestination& destination);

// LE LIBELLÉ DU LIEN de cette clé quand elle est offerte — le même que dans le
// fil, voir `SettingLinkLabel` —, ou une chaîne vide. C'est ce que la fenêtre de
// dialogue appelle pour savoir s'il y a un lien à former, et sous quel nom.
[[nodiscard]] std::string SettingDestinationLabel(std::string_view key, const SettingReach& reach);

// UNE CLÉ PEUT-ELLE VOYAGER ? Non vide, sans `:` ni chevron.
[[nodiscard]] bool SettingKeyTravels(std::string_view key);

}  // namespace moonclient
