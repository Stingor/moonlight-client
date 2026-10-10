#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

#include "ro/sim/shortcut_bar.h"
#include "ro/sim/skill_tree.h"

namespace moonclient {

// LA BARRE DE RACCOURCIS, SUR LE DISQUE.
//
// ⚠ PAR PERSONNAGE, et le client fait de même : ses `shortcutlist\` portent un
// fichier par compte et par personnage. Deux personnages d'un même joueur ne
// lancent pas les mêmes choses, et leur donner la même barre la rendrait
// inutilisable pour les deux.
//
// ⚠ CHEZ LE JOUEUR, ET NON SUR LE SERVEUR. Le jeu d'origine garde ces cases sur
// son serveur web ; on l'ignore. Le jour où l'on voudra les y mettre, c'est ce
// fichier qui deviendra un cache, et rien d'autre ne bougera.
//
// ⚠ ET LES COMPÉTENCES S'ÉCRIVENT PAR LEUR NOM INTERNE — « MG_COLDBOLT », jamais
// un numéro. Même raison que pour les touches : le fichier se relit et se
// corrige à la main, et il ne dépend pas d'une numérotation qui pourrait bouger.

inline constexpr std::string_view kShortcutFileName = "shortcut_bar.yaml";

[[nodiscard]] std::filesystem::path ShortcutFilePath(const std::filesystem::path& client_root);

// Lit la barre d'un personnage. Un fichier absent, une entrée illisible ou une
// compétence que les tables ne connaissent plus laissent la case VIDE : une
// barre à moitié lue vaut mieux qu'un refus de jouer.
void LoadShortcutBar(const std::filesystem::path& client_root, std::string_view character,
                     const ro::sim::SkillTree* tree, ro::sim::ShortcutBars& into);

// Lit le jeu de barres qu'un personnage a retenu pour un métier. Rend `false`,
// sans toucher `into`, quand ce métier n'en a pas encore.
//
// ⚠ RÉSERVÉ AU STAFF, qui change de métier en jeu (@jobchange). Un joueur garde
// un seul jeu par personnage : son métier ne change qu'en progressant, et ses
// barres le suivent.
[[nodiscard]] bool LoadJobShortcutBar(const std::filesystem::path& client_root,
                                      std::string_view character, std::uint16_t job,
                                      const ro::sim::SkillTree* tree, ro::sim::ShortcutBars& into);

// Écrit la barre d'un personnage, en gardant celles des autres. Avec `job`, le
// même jeu s'écrit AUSSI sous ce métier ; les jeux des autres métiers restent.
//
// ⚠ IL FAUT DONC RELIRE LE FICHIER D'ABORD : écrire la seule barre courante
// effacerait celles de tous les autres personnages du joueur.
[[nodiscard]] bool SaveShortcutBar(const std::filesystem::path& client_root,
                                   std::string_view character, const ro::sim::SkillTree* tree,
                                   const ro::sim::ShortcutBars& from,
                                   std::optional<std::uint16_t> job = std::nullopt);

}  // namespace moonclient
