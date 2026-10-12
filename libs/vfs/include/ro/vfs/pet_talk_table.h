#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// CE QUE DISENT LES FAMILIERS, lu dans `data\yaml_files\pet_talk.yml`, que
// `scripts/yaml_tables/pet_talk.py` tire du fichier de répliques du jeu :
//
//     version: 1
//     default_species: poring
//     classes: {1040: GOLEM, …}
//     species:
//       poring:
//         hungry:
//           feeding: [« Not bad, what else ya got? », …]
//
// Une réplique se désigne par trois nombres, ceux que la parole d'un familier
// porte sur le fil : la classe, le chiffre de la faim, le chiffre de
// l'occasion. Cette table ne fait que les traduire en noms.

inline constexpr std::string_view kPetTalkPath = "data\\yaml_files\\pet_talk.yml";

// Les rangs de faim, dans l'ordre de leur chiffre : un chiffre hors de un à
// quatre parle comme le premier.
inline constexpr auto kPetTalkHungerNodes =
    std::to_array<std::string_view>({"hungry", "bit_hungry", "noting", "full", "so_full"});
// Les occasions, dans l'ordre de leur chiffre.
inline constexpr auto kPetTalkOccasionNodes =
    std::to_array<std::string_view>({"feeding", "hunting", "danger", "dead", "stand", "perfor_s",
                                     "levelup", "perfor_1", "perfor_2", "perfor_3", "connect"});

// Les répliques d'une espèce : par rang de faim, puis par occasion.
using PetTalkLines = std::array<std::array<std::vector<std::string>, kPetTalkOccasionNodes.size()>,
                                kPetTalkHungerNodes.size()>;

struct PetTalkTable {
  // L'espèce d'une classe que `species_by_class` ne nomme pas.
  std::string default_species;
  std::unordered_map<std::uint32_t, std::string> species_by_class;
  std::unordered_map<std::string, PetTalkLines> lines_by_species;

  // Les répliques entre lesquelles le client tire au sort. Vide quand
  // l'espèce de la classe n'a rien à dire là, ou que l'occasion n'a pas de nom.
  [[nodiscard]] std::span<const std::string> LinesOf(std::uint32_t pet_class, int hunger_digit,
                                                     int occasion_digit) const;
};

// Le texte de `pet_talk.yml`. Refuse un texte qui n'est pas du YAML, dont
// `version` n'est pas 1, sans espèce par défaut, ou dont une espèce, un rang
// de faim ou une occasion porte un nom inconnu ou une forme inattendue.
[[nodiscard]] core::Result<PetTalkTable> ParsePetTalkTable(std::string_view yaml_text);

// ABSENTE OU ILLISIBLE, la table est vide et le journal le dit : les familiers
// se taisent, leurs émoticônes se montrent encore.
[[nodiscard]] PetTalkTable ReadPetTalkTable(Vfs& vfs);

}  // namespace ro::vfs
