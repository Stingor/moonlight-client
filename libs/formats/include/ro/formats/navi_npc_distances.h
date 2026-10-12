#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/navi_tables.h"

namespace ro::formats {

// LA TABLE DES DISTANCES AUX PNJ, LUE À LA DEMANDE.
//
// Pour chaque PNJ cherchable, la marche depuis chaque liaison qui entre sur sa
// carte, précalculée par le serveur. C'est la moitié du volume des tables de
// navigation (5 Mo de texte), et elle ne sert qu'à guider vers un PNJ précis —
// et encore : le guidage mesure d'abord cette marche sur la grille de la carte
// visée, la table n'est que son repli quand la grille ne se lit pas.
//
// Elle ne s'analyse donc pas d'un bloc. `Index` ne fait que repérer, par carte,
// où commence et finit son bloc ; `ReadMap` n'analyse que celui de la carte
// demandée.
//
// Ce qu'une entrée dit :
//
//   · la liaison est désignée par sa carte de DÉPART et son identifiant ; une
//     liaison que la table des liaisons ne connaît pas est sautée ;
//   · une liaison entrante ABSENTE de la liste d'un PNJ ne mène pas à lui : le
//     serveur ne l'a pas jointe par la marche. Ce n'est pas un dernier passage.

// Les distances aux PNJ d'UNE carte.
struct NaviNpcDistances {
  std::vector<NaviNpcPassRun> runs;
  std::unordered_map<std::uint32_t, NaviPassSpan> spans;
  // Comment la table écrit ses longueurs.
  NaviSteps steps = NaviSteps::kExact;

  // Pour un PNJ, la marche depuis chaque liaison qui entre sur sa carte. Vide
  // si la table n'en dit rien.
  [[nodiscard]] std::span<const NaviNpcPassRun> PassesTo(std::uint32_t npc_id) const;
};

class NaviNpcDistanceFile {
 public:
  // Repère les blocs de chaque carte, sans analyser leurs entrées. Échoue si
  // l'en-tête manque, si une chaîne ou une accolade ne se ferme pas, ou si une
  // carte n'est pas suivie de son bloc.
  [[nodiscard]] static core::Result<NaviNpcDistanceFile> Index(std::vector<std::uint8_t> bytes);

  // Analyse le bloc d'une carte. Une carte que la table ne nomme pas rend des
  // distances vides ; un bloc mal formé, une erreur.
  [[nodiscard]] core::Result<NaviNpcDistances> ReadMap(std::string_view map_name,
                                                       const NaviData& data) const;

  [[nodiscard]] NaviSteps steps() const noexcept { return steps_; }
  [[nodiscard]] std::size_t map_count() const noexcept { return blocks_.size(); }
  [[nodiscard]] std::size_t byte_count() const noexcept { return bytes_.size(); }

 private:
  struct Block {
    std::size_t begin = 0;
    std::size_t end = 0;
  };

  std::vector<std::uint8_t> bytes_;
  // Par nom de carte en minuscules, le bloc `"<carte>", <compte>, { … }`.
  std::unordered_map<std::string, Block> blocks_;
  NaviSteps steps_ = NaviSteps::kExact;
};

}  // namespace ro::formats
