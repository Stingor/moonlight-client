#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string_view>

#include "ro/sim/style.h"

namespace moonclient {

// LES COULEURS DE CHAQUE PERSONNAGE DE CE POSTE, pour la tablée.
//
// 🔴 LE SERVEUR DE PERSONNAGES NE LES CONNAÎT PAS. Seul le serveur de carte
// diffuse les recettes, et seulement une fois en jeu : la sélection n'en reçoit
// aucune. Sans mémoire, chaque personnage s'y montrerait dans ses couleurs
// natives.
//
// ⚠ PAR IDENTIFIANT DE PERSONNAGE, et non de compte : c'est la clé que la
// liste de la tablée porte, et deux personnages d'un même compte ont chacun
// les leurs.
//
// ⚠ UN CACHE, PAS UNE SOURCE : il suit ce que le serveur a dit la dernière
// fois qu'on a joué ce personnage ICI. Une recette changée depuis un autre
// poste ne s'y verra qu'après y avoir rejoué.

inline constexpr std::string_view kCharacterStylesFileName = "styles_personnages.yaml";

[[nodiscard]] std::filesystem::path CharacterStylesPath(const std::filesystem::path& client_root);

// Ce qui est rangé. Un fichier absent est le cas NORMAL, et un fichier abîmé
// ne doit pas empêcher de jouer : les deux rendent une table vide. Un code
// d'une autre version est écarté, comme partout — voir `style_code.h`.
[[nodiscard]] std::map<std::uint32_t, ro::sim::StyleVariants> LoadCharacterStyles(
    const std::filesystem::path& client_root);

// Range les couleurs d'un personnage — ou les retire, sur `nullptr` ou une
// variante vide. Rend vrai si le fichier a changé ; n'écrit rien quand ce qui
// est rangé dit déjà la même chose.
//
// ⚠ RELIT AVANT D'ÉCRIRE : deux clients ouverts en même temps se marcheraient
// dessus, et le second effacerait ce que le premier vient de noter.
bool NoteCharacterStyles(const std::filesystem::path& client_root, std::uint32_t character_id,
                         const ro::sim::StyleVariants* variants);

}  // namespace moonclient
