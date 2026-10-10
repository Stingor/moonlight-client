#pragma once

#include <cstdint>
#include <span>
#include <string_view>

// ⚠ DÉCLARÉE PLUTÔT QU'INCLUSE : cet en-tête ne manipule qu'un POINTEUR de
// police, et tirer tout `imgui.h` derrière lui le ferait entrer dans chaque
// fichier qui veut seulement nommer une famille.
struct ImFont;

namespace ro::ui {

// LES POLICES DE L'INTERFACE, ET POURQUOI IL EN FAUT UNE VRAIE.
//
// ⚠ CELLE D'IMGUI NE CONNAÎT QUE U+0020..U+00FF. ProggyClean n'a aucun glyphe
// au-delà du latin-1 : le tiret cadratin, les points de suspension, le « œ »
// français sortent tous en « ? » — et c'est INVISIBLE à la relecture, où le
// caractère s'affiche parfaitement.
//
// ⚠ ET RÉÉCRIRE NOS LIBELLÉS EN ASCII NE CORRIGERAIT QUE LES NÔTRES. Les
// catalogues de traduction — dont le texte français EST la clé — réintroduiraient
// la même typographie dès qu'on joue en anglais ou en espagnol, et les textes du
// JEU, eux, ne nous appartiennent pas du tout. C'est la police qu'il faut
// corriger, pas les textes.
//
// ⚠ LES CHEMINS VIENNENT D'`app`, jamais d'ici. `C:\Windows\Fonts\…` est un fait
// de plateforme ; `ui` ne sait qu'assembler un atlas à partir de ce qu'on lui
// nomme. C'est aussi ce qui rend l'assemblage essayable avec des fichiers
// inventés.

// Une famille : ce que le joueur en lit, et les trois fichiers qui la portent.
//
// ⚠ UN CHEMIN VIDE N'EST PAS UNE ERREUR. Tahoma ne livre pas d'italique — le
// fichier n'existe pas — et Franklin Gothic n'a pas plus gras que son « Medium ».
// La variante absente retombe sur le normal.
struct FontFamilySpec {
  std::string_view label;
  std::string_view regular;
  std::string_view bold;
  std::string_view italic;
};

// Les trois dessins d'une famille, dans l'ordre où `ChosenFont` les rend.
// LA TAILLE DU TEXTE DE L'INTERFACE, en pixels d'art.
//
// ⚠ QUINZE, ET C'EST UN RELEVÉ, pas un goût : c'est la taille autour de
// laquelle l'habillage du client a été mesuré. Un
// bouton du skin fait vingt et un pixels de haut ; du texte de vingt n'y tient
// pas, et il le remplit jusqu'aux bords sans laisser d'air.
//
// L'échelle de l'interface la MULTIPLIE ; elle ne la remplace pas.
inline constexpr float kDefaultFontPixels = 15.0F;

enum class FontStyle : std::uint8_t { kRegular = 0, kBold, kItalic, kCount };

// Ce qu'il faut bâtir, et avec quoi.
struct FontSetup {
  // Les familles offertes. La PREMIÈRE est celle qui couvre le plus : c'est elle
  // le défaut, et c'est elle qui décide de ce que l'interface sait dessiner.
  std::span<const FontFamilySpec> families;
  // Celle que le joueur a choisie. MOINS DE ZÉRO = la police intégrée d'ImGui,
  // avec ses trous : c'est un choix qu'on lui laisse, pas un repli.
  int chosen = 0;
  // LA TAILLE, EN PIXELS.
  //
  // 🔴 ZÉRO NE VEUT PAS DIRE « CELLE DE LA POLICE INTÉGRÉE ». Une police chargée
  // sans taille n'a pas de `LegacySize`, et l'interface retombe alors sur son
  // propre défaut — `FONT_DEFAULT_SIZE_BASE`, qui vaut VINGT. Le texte sortait
  // donc un tiers trop grand pour l'habillage du client, dont les boutons, les
  // barres et les cases sont dessinés autour de quinze.
  //
  // ⚠ ET RIEN NE LE SIGNALE : l'interface ne se plaint pas, le texte est net, et
  // l'on cherche la cause dans les marges — qui, elles, sont justes.
  float size_pixels = kDefaultFontPixels;
  // ── L'ANTISLASH, LE SEUL GLYPHE QU'ON NE PREND PAS À LA PRINCIPALE ─────────
  // Les polices coréennes dessinent U+005C comme le WON « ₩ » — héritage de la
  // code-page 949, où l'octet 0x5C porte le symbole monétaire. Un message qui
  // contient un antislash sort alors « ₩ », et ce n'est pas corrigible côté
  // texte : l'octet EST un antislash, c'est le DESSIN qui diffère.
  //
  // Ce fichier-ci, une latine, fournit ce seul point de code. Vide = on n'exclut
  // rien : un losange « glyphe absent » serait pire qu'un won.
  std::string_view backslash_source;

  // LA SOURCE DES EMOJI, fusionnée dans CHAQUE police de l'atlas.
  //
  // 🔴 AUCUNE POLICE DE TEXTE N'EN PORTE. Les familles du client sont
  // latines ; sans cette source-là, une grille d'emoji ne rend que des losanges
  // « glyphe absent », et l'on cherche le défaut du côté de la table alors qu'il
  // est dans l'atlas.
  //
  // Vide = pas d'emoji : une installation qui n'en a pas reste jouable.
  std::string_view emoji_source;

  // ── LES POLICES DU DOSSIER DU JEU ─────────────────────────────────────────
  // Celles que le joueur a déposées lui-même : un fichier chacune, qui ne sert
  // pas l'interface mais ce qui se choisit par son NOM — les nombres de dégâts.
  // Seul `regular` compte ; le nom est `label`.
  //
  // ⚠ UN FICHIER ILLISIBLE EST ÉCARTÉ, PAS FATAL : il vient d'un dossier que
  // n'importe quoi peut remplir.
  std::span<const FontFamilySpec> loose;
};

// Combien de polices du dossier du jeu l'atlas accueille. Au-delà, les
// suivantes sont ignorées : chacune reste en mémoire le temps de la partie.
inline constexpr std::size_t kMostLooseFonts = 32;

// Combien de familles une installation peut offrir. Un plafond nommé plutôt
// qu'un vecteur : la table est écrite à la main et ne grandira pas toute seule.
inline constexpr std::size_t kMostFontFamilies = 16;

}  // namespace ro::ui
