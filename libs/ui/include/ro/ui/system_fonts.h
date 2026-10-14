#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/font_setup.h"

namespace ro::ui {

// LES POLICES QUE L'INSTALLATION PORTE DÉJÀ.
//
// ⚠ ON N'EN EMBARQUE AUCUNE, et ce n'est pas une économie : une police livrée
// avec le dépôt a une licence à respecter et un poids à porter, alors que
// Windows en installe une douzaine dont celle qui couvre TOUT. On emprunte.
//
// ⚠ LA PREMIÈRE EST MALGUN GOTHIC, ET C'EST UN CHOIX COMPLET. Elle couvre le
// latin accentué, la ponctuation typographique — tirets, points de suspension,
// guillemets courbes — ET le coréen, qui est la langue des chemins de
// l'installation. Aucune autre police de Windows ne donne les trois.
//
// ⚠ ELLE A UN DÉFAUT, UN SEUL : elle dessine l'antislash comme le WON « ₩ ».
// C'est l'héritage de la code-page 949, où l'octet 0x5C porte le symbole
// monétaire. Voir `FontSetup::backslash_source`, qui le lui reprend.
//
// ⚠ UNE POLICE ABSENTE N'EST PAS UNE PANNE. Les chemins sont vérifiés au
// démarrage ; une famille introuvable disparaît du menu au lieu d'offrir un
// choix sans effet.

// Les familles, dans l'ordre de la table. La PREMIÈRE est le défaut. ⚠ LE
// RÉGLAGE « police » EST UN RANG DANS CETTE TABLE-CI, la même sur toute
// machine ; les menus, eux, ne montrent que les familles présentes.
[[nodiscard]] std::span<const FontFamilySpec> SystemFontFamilies();

// LES FAMILLES QUE CETTE MACHINE PORTE, par leur rang dans la table, dans son
// ordre : ce qu'un menu de polices offre, et ce que l'atlas bâtit. Une famille
// y a donc la même place dans le menu et dans l'atlas.
[[nodiscard]] std::span<const int> InstalledFontFamilies();

// Le rang dans la table de la famille à cette place d'un tel menu — la
// première présente hors du menu —, et la place d'un rang de la table — zéro
// pour une famille que le menu ne montre pas.
[[nodiscard]] int FontFamilyAtMenuRank(int menu_rank);
[[nodiscard]] int MenuRankOfFontFamily(int family);

// La même traduction, sur un relevé donné : c'est elle qui s'éprouve.
[[nodiscard]] constexpr int FontFamilyAtMenuRank(int menu_rank,
                                                 std::span<const int> installed) noexcept {
  if (installed.empty()) {
    return 0;
  }
  if (menu_rank < 0 || menu_rank >= static_cast<int>(installed.size())) {
    return installed.front();
  }
  return installed[static_cast<std::size_t>(menu_rank)];
}

[[nodiscard]] constexpr int MenuRankOfFontFamily(int family,
                                                 std::span<const int> installed) noexcept {
  for (std::size_t place = 0; place < installed.size(); ++place) {
    if (installed[place] == family) {
      return static_cast<int>(place);
    }
  }
  return 0;
}

// La latine qui prête son antislash à Malgun. Vide si aucune n'est installée :
// on n'exclut alors rien, parce qu'un losange « glyphe absent » serait pire
// qu'un won.
[[nodiscard]] std::string_view BackslashFontSource();

// LA POLICE D'EMOJI DU SYSTÈME, ou une vue vide quand il n'y en a pas.
// Voir `FontSetup::emoji_source`.
[[nodiscard]] std::string_view EmojiFontSource();

// LA POLICE DU BANDEAU DU NOM DE LA CARTE : ARIAL GRAS, et non celle de
// l'interface. C'est celle que le jeu y emploie pour une installation en
// langue latine — un fichier de Windows, pas de l'installation. Sous Linux,
// sa jumelle métrique Liberation Sans, puis DejaVu Sans.
//
// Les candidates dans l'ordre de préférence, et la première présente — vide
// si aucune : le bandeau prend alors la police courante.
[[nodiscard]] std::span<const std::string_view> MapSignFontCandidates();
[[nodiscard]] std::string_view MapSignFontSource();

// Ce qu'il faut donner au contexte d'interface, familles introuvables retirées.
//
// ⚠ `usable` PORTE LES FAMILLES, et la réponse n'en est qu'une vue : il doit
// vivre jusqu'à ce que `UseFonts` l'ait lue. C'est l'appelant qui le tient, et
// non un tableau statique qu'un second appel viderait sous la vue du premier.
[[nodiscard]] FontSetup ReadableFontSetup(int chosen, std::vector<FontFamilySpec>& usable);

// ── LES POLICES DU DOSSIER DU JEU ───────────────────────────────────────────
//
// Celles que le joueur dépose lui-même dans `fonts\`, à la racine de son
// installation : une police fantaisie pour ses dégâts, que Windows ne livre
// pas et que le dépôt n'embarque pas. Chacune se nomme par son fichier, sans
// extension.
inline constexpr std::string_view kLooseFontFolder = "fonts";

struct LooseFontFile {
  std::string label;
  std::string path;
};

// Les fichiers `.ttf`, `.otf` et `.ttc` du dossier, rangés par nom. Vide quand
// le dossier n'existe pas : il est facultatif.
[[nodiscard]] std::vector<LooseFontFile> FindLooseFonts(const std::filesystem::path& folder);

}  // namespace ro::ui
