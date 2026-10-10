#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/chat_macros.h"

namespace moonclient {

// CE QUE LE MENU DU CLIC DROIT PROPOSE DANS UNE MACRO.
//
// 🔴 RELEVÉ SUR LE SERVEUR, PAS DEVINÉ. Sources, dans le dépôt `moonlight` :
//   · `conf/atcommands.yml`        — les commandes de rAthena et leurs alias
//     (`gstorage` = `guildstorage`, `noks` = `ksprotection`) ;
//   · `conf/import/atcommands.yml` — les alias PROPRES à Moonlight, dont
//     `storage1..5` = `storagealt1..5`, le cinquième servant aux cartes ;
//   · `src/custom/atcommand.inc`   — les commandes maison : `@storeall` y prend
//     un numéro de storage, `@autolootpognon` un PRIX PLANCHER.
// ⚠ Le fichier de base MENT quand `conf/import/` le surcharge, et c'est le cas
// pour les storages alternatifs.
//
// ⚠ CE NE SONT PAS DES COMMANDES DU CLIENT. Elles dépendent du serveur et des
// permissions du joueur, et on ne les filtre pas : préremplir n'est pas
// exécuter, et un refus du serveur se lit dans la conversation.
//
// ⚠ ET C'EST `app` QUI LES TIENT, jamais `ui` : cette couche-là ne connaît aucun
// serveur, et une liste de commandes d'administration n'a rien à faire dans du
// dessin.

// Les dix valeurs d'usine des macros, telles que le client les pose.
//
// 🔴 CE SONT DES COMMANDES D'ÉMOTE, ET LEUR CHOIX N'EST PAS LE NÔTRE : le client
// remplit ses dix lignes avec les messages 0x220 à 0x223 puis 0x225 à 0x22A —
// il SAUTE le 0x224. ⚠ ET LE RANG SAUTÉ EST `/lv2`, pas le cinquième sprite :
// lu dans la table du client, ce bloc donne les dix PREMIÈRES émoticônes.
//
// ⚠ ELLES SE DÉRIVENT DE NOTRE PROPRE TABLE DE COMMANDES, et ne sont pas
// recopiées à la main : `EmotionCommandOf` est déjà la seule source du lien
// entre une émoticône et sa commande, et une seconde liste finirait par en
// différer.
[[nodiscard]] std::string ChatMacroDefault(std::size_t slot);

// LE CATALOGUE, RECOMPOSÉ QUAND LA LANGUE CHANGE.
//
// 🔴 PAS DE TABLE STATIQUE DE TEXTES TRADUITS. Le pointeur que rend `Tr`
// appartient au catalogue de langue et MEURT au changement de langue : une liste
// bâtie une fois pour toutes montrerait de la mémoire libérée dès que le joueur
// change de langue, ce qui n'est pas un défaut d'affichage mais une lecture
// après libération.
//
// ⚠ ET LES CINQ STORAGES ALTERNATIFS SE COMPOSENT — « @storage3 », « Ouvre le
// storage alternatif 3. » : c'est le GABARIT qui se traduit, jamais la phrase
// finie.
class MacroCommandCatalogue {
 public:
  // Recompose si la langue a changé depuis la dernière fois. Appelée seulement
  // quand la fenêtre est ouverte : composer une trentaine de phrases pour une
  // fenêtre fermée serait du temps donné à rien.
  void Refresh();

  [[nodiscard]] std::span<const ro::ui::ChatMacroCommandGroup> groups() const noexcept {
    return groups_;
  }

 private:
  // ⚠ LES TEXTES D'ABORD, EN ENTIER, PUIS LES VUES : une vue ne survit pas à la
  // réallocation du vecteur qui la porte. Même règle que les lignes de la
  // fenêtre de configuration.
  std::vector<std::string> texts_;
  // ⚠ LES TITRES DE GROUPE AUSSI, et non le pointeur que rend `Tr` : celui-là
  // appartient au catalogue de langue et meurt avec lui.
  std::vector<std::string> group_labels_;
  std::vector<ro::ui::ChatMacroCommand> items_;
  std::vector<ro::ui::ChatMacroCommandGroup> groups_;
  // Le compte de changements de langue au moment de la composition. ⚠ COMMENCE
  // HORS BORNES pour que la première demande compose vraiment.
  std::size_t language_serial_ = static_cast<std::size_t>(-1);
};

}  // namespace moonclient
