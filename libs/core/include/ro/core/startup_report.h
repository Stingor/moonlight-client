#pragma once

#include <filesystem>
#include <string_view>

#include "ro/core/error.h"

namespace ro::core {

// LA MACHINE, AU JOURNAL, AVANT TOUT LE RESTE.
//
// Un joueur dont le client se ferme sans un mot n'envoie que son journal :
// c'est la seule pièce qui voyage. Elle doit donc dire SEULE sur quoi le client
// tournait — système, processeur, mémoire, dossiers, droits —, sans qu'on ait à
// redemander chacun de ces faits des jours plus tard.
//
// ⚠ UN SECOND CLIENT DÉJÀ OUVERT se signale ici : les deux écrivent alors le
// même journal, et c'est le premier fait à savoir quand celui-ci se lit mal.
void LogMachine();

// LES MODULES QUI NE SONT PAS À WINDOWS, au journal.
//
// Un antivirus, une surcouche — enregistreur, compteur d'images, messagerie —,
// un `opengl32.dll` déposé à côté du jeu : ils s'injectent dans le processus,
// et ce sont eux qui ferment un client sans faute que le filet puisse voir.
// `moment` dit quand la liste a été prise : la fenêtre et le pilote graphique
// en font charger de nouveaux.
void LogForeignModules(std::string_view moment);

// Écrit, relit et efface un fichier dans ce dossier.
//
// ⚠ CRÉER NE SUFFIT PAS À PROUVER L'ÉCRITURE : un dossier protégé — Program
// Files, la protection contre les rançongiciels, un antivirus — laisse souvent
// naître un fichier sans laisser y écrire. Le journal y apparaît, vide, et les
// réglages, le patch et les rapports de plantage s'y perdent en silence.
[[nodiscard]] Result<void> ProbeWritableFolder(const std::filesystem::path& folder);

}  // namespace ro::core
