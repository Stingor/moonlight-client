#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// LES TITRES, et ils vivent dans les DONNÉES du client.
//
// 🔴 UN TITRE N'A AUCUN TEXTE SUR LE FIL. Le serveur n'envoie qu'un ENTIER —
// `title_id` de `ZC_ACK_REQNAMEALL`, persisté dans `char.title_id` — et le
// libellé est à cent pour cent du côté du client. Chercher le texte dans un
// paquet ou dans l'exécutable ne rend rien.
//
// La table est `data\yaml_files\titles.yml`, que `scripts/yaml_tables/titles.py`
// tire de la table de l'installation et des bornes du serveur :
//
//     version: 1
//     titles:
//       1000: Junction of life
//       1001: Specialist
inline constexpr std::string_view kTitleTablePath = "data\\yaml_files\\titles.yml";

// Le premier numéro que la table emploie — `TITLE_BASE` du serveur. Zéro veut
// donc dire « aucun titre », et non « le titre numéro zéro ».
inline constexpr std::int32_t kFirstTitleId = 1000;

// La table entière, triée par numéro.
class TitleTable {
 public:
  [[nodiscard]] static core::Result<TitleTable> Load(vfs::Vfs& vfs);

  // La MÊME lecture, à partir du texte de la table. Refuse un texte qui n'est
  // pas du YAML, dont `version` n'est pas 1, sans table `titles`, ou dont une
  // entrée n'est pas un numéro et un texte.
  [[nodiscard]] static core::Result<TitleTable> Parse(std::string_view yaml_text);

  // Le libellé, ou vide. ⚠ UN NUMÉRO SANS LIBELLÉ N'EST PAS UNE ERREUR : le
  // serveur en connaît jusqu'à `TITLE_MAX`, et la table n'en traduit que ce
  // que son lot de données porte. Rien ne s'écrit alors.
  [[nodiscard]] std::string_view Find(std::int32_t title_id) const;

  struct Entry {
    std::int32_t title_id = 0;
    std::string label;
  };

  [[nodiscard]] std::span<const Entry> entries() const noexcept { return entries_; }

 private:
  std::vector<Entry> entries_;
};

}  // namespace ro::render
