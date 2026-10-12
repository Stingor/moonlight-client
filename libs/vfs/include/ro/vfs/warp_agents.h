#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "ro/core/error.h"

namespace ro::vfs {

class Vfs;

// ── LES WARP AGENT DU SERVEUR ──────────────────────────────────────────────
//
// Le voyage passe par le Warp Agent, un PNJ dont le menu est le même partout.
// Pour chaque carte où il se tient, la classe de sa figure : la carte du monde
// la montre au survol de la ville. La table vient des scripts de PNJ du
// serveur, par `scripts/yaml_tables/warp_agents.py` :
//
//     version: 1
//     agents:
//       prontera: {sprite: "4_F_01", class: 90}
//
// `sprite` est le nom que le script écrit, pour le lecteur ; seule la classe
// se lit. Une carte que la table ne nomme pas n'a pas de Warp Agent.
inline constexpr std::string_view kWarpAgentsPath = "data\\yaml_files\\warp_agents.yml";

class WarpAgents {
 public:
  [[nodiscard]] static core::Result<WarpAgents> Load(Vfs& vfs);
  // Le texte de `warp_agents.yml`. Refuse un texte qui n'est pas du YAML, dont
  // `version` n'est pas 1, sans table `agents`, ou dont une entrée n'associe
  // pas une carte à une table portant une classe entière positive.
  [[nodiscard]] static core::Result<WarpAgents> Parse(std::string_view yaml_text);

  // La classe de la figure du Warp Agent d'une carte, par son nom interne sans
  // extension ; rien pour une carte qui n'en a pas.
  [[nodiscard]] std::optional<std::uint32_t> ClassAt(std::string_view map_name) const;

 private:
  std::map<std::string, std::uint32_t, std::less<>> classes_;
};

}  // namespace ro::vfs
