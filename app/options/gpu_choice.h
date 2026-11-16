#pragma once

#include <string>
#include <string_view>
#include <vector>

// LA CARTE GRAPHIQUE QUE LE JEU RÉCLAME, sur une machine qui en a deux.
//
// ⚠ C'EST LA PRÉFÉRENCE DE WINDOWS, ET NON UN RÉGLAGE À NOUS. Windows la tient
// par exécutable, dans le registre de l'utilisateur, et c'est la même que règle
// sa page « Paramètres → Affichage → Graphiques » : la rangée du panneau la lit
// et l'écrit, et ne garde rien dans le fichier de réglages. Deux sources qui
// pourraient se contredire n'en font ainsi qu'une.
//
// ⚠ QUE DES DONNÉES ICI : le texte que Windows range se lit et se réécrit sans
// toucher au registre, ce qui permet de l'essayer sans machine à deux cartes.
namespace moonclient {

// LES TROIS VALEURS DE WINDOWS, aux NUMÉROS de Windows : « GpuPreference=2; »
// veut dire hautes performances. ⚠ FIGÉS POUR CETTE RAISON, et ils sont aussi
// les rangs de la liste du panneau.
enum class GpuPreference : int {
  kAutomatic = 0,
  kPowerSaving = 1,
  kHighPerformance = 2,
};

// CE QUE LA RANGÉE « Carte graphique » MONTRE, refait à chaque composition.
//
// ⚠ DEUX PRÉFÉRENCES ET NON UNE : `at_launch` est celle que le pilote a vue en
// ouvrant le contexte, `wanted` celle que le joueur vient de poser. Elles ne se
// rejoignent qu'au lancement suivant, et la rangée le dit tant qu'elles
// diffèrent.
struct GpuOffer {
  GpuPreference wanted = GpuPreference::kAutomatic;
  GpuPreference at_launch = GpuPreference::kAutomatic;
  // 🔴 FAUX TANT QUE LA PRÉFÉRENCE N'A PAS ÉTÉ LUE : avant, `wanted` ne vaut que
  // son défaut, et l'écrire effacerait chez Windows ce que le joueur y a réglé.
  bool read = false;
  // Le fabricant et le nom de la carte qui a répondu, tels que le pilote les dit.
  std::string in_use;
  // ⚠ LES POINTEURS DE `names` DÉSIGNENT DES TRADUCTIONS, et `help` est un
  // tampon : la rangée n'en garde que des vues.
  std::vector<const char*> names;
  std::string help;
};

// La préférence que porte une valeur du registre, « AppStatus=4;GpuPreference=2; ».
// ⚠ Une valeur absente, illisible ou hors des trois rend l'automatique : c'est ce
// que Windows en fait.
[[nodiscard]] GpuPreference GpuPreferenceIn(std::string_view windows_value);

// La même valeur, la préférence remplacée.
//
// 🔴 LES AUTRES CHAMPS RESTENT : Windows range dans cette chaîne d'autres choses
// que la carte (« AppStatus », « SwapEffectUpgradeEnable »), et les effacer
// défairait ce que le joueur a réglé ailleurs. L'automatique RETIRE le champ au
// lieu d'écrire zéro : c'est ce que fait la page de Windows.
[[nodiscard]] std::string WithGpuPreference(std::string_view windows_value, GpuPreference wanted);

// Ce que le rang choisi dans la liste désigne. ⚠ Un rang hors liste rend `kept`.
[[nodiscard]] GpuPreference GpuPreferenceAt(int rank, GpuPreference kept);

}  // namespace moonclient
