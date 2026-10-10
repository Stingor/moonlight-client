#pragma once

namespace ro::sim {

// LE MONDE NOMMÉ SANS ÊTRE DÉFINI.
//
// Un en-tête qui ne fait que déclarer une fonction prenant le monde par
// référence s'en contente : la définition complète est dans `world.h`, qui
// entraîne vingt-six en-têtes d'état, et seul le fichier qui lit ou écrit le
// monde l'inclut.

class World;

}  // namespace ro::sim
