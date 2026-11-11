#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <span>
#include <string_view>
#include <vector>

#include "ro/sim/style.h"
#include "ro/sim/style_outfit.h"

namespace moonclient {

// LES COULEURS DE CHAQUE PERSONNAGE DE CE POSTE, pour la tablée.
//
// UN CACHE, PAS UNE SOURCE. Le serveur de personnages envoie la recette entière
// du compte (`ro/net/char_select_styles.h`) quand il sait le faire ; ce fichier
// sert tant qu'il ne répond pas, hors ligne, et avant qu'on ait la preuve qu'il
// la sert.
//
// 🔴 AU FORMAT DES TENUES (`ro/sim/style_outfit.h`) : les corps de la v7 et les
// teintures de calques sous leurs clés AFFICHÉES, l'octet-à-octet
// d'`EncodeOutfit` en hexadécimal. Une tenue garde 12 entrées et 1 Kio au plus :
// trop grosse, elle perd d'abord ses retouches, puis son reste.
//
// ⚠ PAR IDENTIFIANT DE PERSONNAGE, et non de compte : c'est la clé que la liste
// de la tablée porte, et deux personnages d'un même compte ont chacun les leurs.
//
// ⚠ LES ANCIENS FICHIERS SE LISENT : une entrée sans tenue lit ses codes v7, et
// la première écriture la convertit.

inline constexpr std::string_view kCharacterStylesFileName = "styles_personnages.yml";

[[nodiscard]] std::filesystem::path CharacterStylesPath(const std::filesystem::path& client_root);

// Ce que le fichier tient.
struct KeptCharacterStyles {
  // 🔴 LA PREUVE que le serveur de personnages sert la tablée (règle 28), vue en
  // jeu : sans elle, la demande ne part pas, puisqu'un paquet inconnu y coupe la
  // liaison.
  bool served = false;
  std::map<std::uint32_t, ro::sim::StyleOutfit> characters;
};

// Un fichier absent est le cas NORMAL, et un fichier abîmé ne doit pas empêcher
// de jouer : les deux rendent un cache vide. Un code ou une tenue d'une autre
// version est écarté.
[[nodiscard]] KeptCharacterStyles LoadCharacterStyles(const std::filesystem::path& client_root);

// Range la tenue d'un personnage, ou la retire sur `nullptr` ou une tenue vide.
// Rend vrai si le fichier a changé ; n'écrit rien quand il dit déjà la même chose.
//
// ⚠ RELIT AVANT D'ÉCRIRE : deux clients ouverts en même temps se marcheraient
// dessus, et le second effacerait ce que le premier vient de noter.
bool NoteCharacterStyles(const std::filesystem::path& client_root, std::uint32_t character_id,
                         const ro::sim::StyleOutfit* outfit);

// Ce que le serveur de personnages a rendu : la tenue de chacun de `told`, et
// l'oubli de ceux de `listed` qui n'en ont pas reçu. Rend vrai si le fichier a
// changé.
bool NoteAccountStyles(const std::filesystem::path& client_root,
                       std::span<const std::uint32_t> listed,
                       const std::map<std::uint32_t, ro::sim::StyleOutfit>& told);

// Retient que le serveur de personnages sert la tablée. Rend vrai si le fichier
// a changé.
bool NoteCharacterStylesServed(const std::filesystem::path& client_root);

// LA TENUE de ces variantes et de ces teintures, l'une ou l'autre pouvant
// manquer ; et les variantes d'une tenue, repli compris.
[[nodiscard]] ro::sim::StyleOutfit OutfitOf(const ro::sim::StyleVariants* variants,
                                            const std::vector<ro::sim::StyleLayerRecipe>* layers);
[[nodiscard]] ro::sim::StyleVariants VariantsOfOutfit(const ro::sim::StyleOutfit& outfit);

}  // namespace moonclient
