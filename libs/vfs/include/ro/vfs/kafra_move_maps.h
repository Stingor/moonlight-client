#pragma once

#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

namespace ro::vfs {

class Vfs;

// ── LE SERVICE DE TÉLÉPORTATION DES KAFRA ──────────────────────────────────
//
// Pour chaque carte où un PNJ de la famille Kafra téléporte, les cartes où il
// envoie, dans l'ordre de son menu. La carte du monde les montre au survol de
// la ville. La table vient des scripts de PNJ du serveur, par
// `scripts/yaml_tables/kafra_destinations.py` :
//
//     version: 1
//     towns:
//       rachel: [veins]
//
// Une ville que la table ne nomme pas n'a pas de service : rien ne s'y dessine.
inline constexpr std::string_view kKafraMoveMapsPath = "data\\yaml_files\\kafra_destinations.yml";

class KafraMoveMaps {
 public:
  [[nodiscard]] static core::Result<KafraMoveMaps> Load(Vfs& vfs);
  // Le texte de `kafra_destinations.yml`. Refuse un texte qui n'est pas du
  // YAML, dont `version` n'est pas 1, sans table `towns`, ou dont une entrée
  // n'associe pas une carte à une liste de cartes.
  [[nodiscard]] static core::Result<KafraMoveMaps> Parse(std::string_view yaml_text);

  // Les destinations d'une ville, par leur nom interne sans extension ; vide
  // pour une ville sans service.
  [[nodiscard]] std::span<const std::string> DestinationsOf(std::string_view town) const;

 private:
  std::map<std::string, std::vector<std::string>, std::less<>> destinations_;
};

}  // namespace ro::vfs
