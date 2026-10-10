#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/minimap_window.h"
#include "ro/ui/navigation_window.h"

namespace moonclient {

// Un marqueur que le joueur a posé sur une carte, en cellules.
//
// 🔴 PAS LES FICHIERS MÉMO DU CLIENT D'ORIGINE (`Memo\$$<carte>.txt`) : ils
// plafonnent à quarante lignes, séparent leurs champs par des tabulations — un
// libellé qui en contient les casse — et sont dans la page de code du jeu là où
// nos réglages sont en UTF-8. Un import pourra venir ; lire leur format ne
// dispense pas de ces trois défauts.
struct NaviMemo {
  int cell_x = 0;
  int cell_y = 0;
  std::string name;
};

// Ce que la navigation et la minimap gardent d'une partie à l'autre.
struct NaviPrefs {
  ro::ui::MinimapSettings minimap;
  // La minimap est-elle affichée ? Un raccourci la masque et la rend.
  bool minimap_shown = true;
  // Le jeu de traces semé au sol, de 1 à 8. Le client d'origine ne le retient
  // pas d'une session à l'autre ; ici c'est un réglage.
  int trail_icon = 1;
  // Semer la trace au sol le long de l'itinéraire.
  bool ground_trail = true;
  // Les marqueurs, par nom de carte interne. Une carte sans marqueur n'a pas
  // d'entrée.
  std::map<std::string, std::vector<NaviMemo>, std::less<>> memos;
};

inline constexpr std::string_view kNaviPrefsFileName = "navigation.yaml";

[[nodiscard]] std::filesystem::path NaviPrefsPath(const std::filesystem::path& client_root);

// Lit le fichier. Absent ou abîmé, les défauts restent : un fichier de
// préférences ne doit jamais empêcher de jouer.
void LoadNaviPrefs(const std::filesystem::path& client_root, NaviPrefs& into);

[[nodiscard]] bool SaveNaviPrefs(const std::filesystem::path& client_root, const NaviPrefs& from);

}  // namespace moonclient
