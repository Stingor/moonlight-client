#pragma once

#include <string>
#include <vector>

namespace moonclient {

// Les onglets de la section, par leur rang. ⚠ CELUI DE LA FENÊTRE : elle rend
// l'onglet ouvert par son indice dans la liste qu'on lui donne — et la partie
// le lit pour savoir si le volet des dégâts est sous les yeux.
enum class DamageTab : int {
  kShown = 0,
  kMotion,
  kColors,
  kCount,
};

// LES LISTES DE LA SECTION DES DÉGÂTS : ses onglets, et ce que ses listes
// déroulantes offrent.
//
// ⚠ LES LIBELLÉS SE REFONT À CHAQUE IMAGE, comme les noms de langue : un texte
// traduit ne se garde pas d'une image à l'autre, son pointeur meurt au
// changement de langue. Seuls les noms des polices du dossier du jeu restent :
// ils ne se traduisent pas, et ne changent qu'au lancement.
struct DamagePageNames {
  std::vector<const char*> tabs;
  std::vector<const char*> splits;
  // Les types de dégâts, au rang de `DamagePath`.
  std::vector<const char*> paths;
  // « Celle de l'interface », les familles présentes sur la machine, puis les polices du
  // dossier du jeu — dans cet ordre, qui est celui des rangs de la liste.
  std::vector<const char*> fonts;
  std::vector<std::string> loose_fonts;
};

}  // namespace moonclient
