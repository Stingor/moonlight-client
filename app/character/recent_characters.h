#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace moonclient {

// LES PERSONNAGES JOUÉS RÉCEMMENT, DU PLUS RÉCENT AU PLUS ANCIEN.
//
// 🔴 CE QUE CETTE LISTE ACHÈTE EST UNE TOUCHE. Un joueur qui se reconnecte
// revient presque toujours sur le personnage qu'il vient de quitter ; sans
// mémoire, la tablée s'ouvre sur le premier siège occupé et il faut viser le
// bon avant d'entrer. Avec elle, la scène s'ouvre déjà sur lui et Entrée joue.
//
// ⚠ UNE LISTE ET NON UN SEUL NOM, parce qu'un personnage peut avoir disparu —
// supprimé, ou sur un autre compte. On descend alors au suivant plutôt que de
// retomber sur le siège zéro.
//
// ⚠ PAR IDENTIFIANT DE PERSONNAGE, qui est unique sur tout le serveur : la
// liste traverse donc les comptes sans les mélanger, puisqu'un identifiant
// d'un autre compte ne se trouvera simplement pas dans la tablée ouverte.
//
// ⚠ CHEZ LE JOUEUR, comme la barre de raccourcis : c'est une commodité de SA
// machine, pas un état de jeu. Le serveur n'en sait rien et n'a pas à en
// savoir.

// Le fichier, à la racine du client.
inline constexpr std::string_view kRecentCharactersFileName = "recent_characters.yaml";

// COMBIEN ON S'EN RAPPELLE.
//
// ⚠ SEIZE, et c'est exprès plus que le nombre d'emplacements d'un compte :
// passer d'un compte à l'autre ne doit pas chasser les personnages du premier.
inline constexpr std::size_t kRecentCharactersMax = 16;

[[nodiscard]] std::filesystem::path RecentCharactersPath(const std::filesystem::path& client_root);

// Lit la liste. Un fichier absent ou illisible rend une liste VIDE : l'absence
// de mémoire n'est pas une panne, c'est un premier lancement.
[[nodiscard]] std::vector<std::uint32_t> LoadRecentCharacters(
    const std::filesystem::path& client_root);

// Pousse `character_id` en tête, dédoublonne, borne à `kRecentCharactersMax`,
// et réécrit le fichier.
//
// ⚠ ELLE RELIT D'ABORD : deux clients ouverts en même temps écriraient sinon
// l'un par-dessus l'autre. Rend faux si le disque refuse — la reprise se
// dégrade alors, et rien de plus.
bool NoteRecentCharacter(const std::filesystem::path& client_root, std::uint32_t character_id);

// CELUI QU'IL FAUT DÉSIGNER dans une tablée, par son RANG dans `present`.
//
// 🔴 LE PLUS RÉCENT QUI SOIT ENCORE LÀ, et à défaut le premier occupé — jamais
// « rien ». Un écran qui s'ouvre sans surbrillance oblige à viser avant de
// pouvoir jouer, ce qui est précisément ce qu'on veut éviter.
//
// Rend -1 seulement quand `present` est vide.
[[nodiscard]] int PreferredCharacter(std::span<const std::uint32_t> recent,
                                     std::span<const std::uint32_t> present);

}  // namespace moonclient
