#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LA TAILLE ET LES EFFETS QUE CERTAINS MONSTRES PORTENT D'OFFICE.
//
// Le serveur n'en dit rien : c'est une table qui grandit tel monstre d'un
// cinquième et l'entoure de telle aura, par son numéro de classe. Elle se lit
// dans `data\yaml_files\monster_size_effects.yml`, que
// `scripts/yaml_tables/monster_size_effects.py` tire de la table traduite de
// l'installation :
//
//     version: 1
//     monsters:
//       1002: {size: 1.2, effects: [892]}
//
// Le client en tire deux choses :
//
//   * la TAILLE multiplie le sprite du monstre, ombre comprise. Un pour sa
//     taille d'origine ; zéro ou moins ne change rien.
//   * les EFFETS s'attachent au monstre à son apparition, dans l'ordre de la
//     liste, et le quittent quand il se cache.
inline constexpr std::string_view kMonsterSizeEffectPath =
    "data\\yaml_files\\monster_size_effects.yml";

class MonsterSizeEffects {
 public:
  struct Entry {
    std::uint32_t monster_id = 0;
    float size = 0.0F;
    std::vector<std::uint32_t> effect_ids;
  };

  // Lit la table. Absente, elle n'est pas une faute pour l'appelant : aucun
  // monstre n'est alors grandi ni paré.
  [[nodiscard]] static core::Result<MonsterSizeEffects> Load(Vfs& vfs);

  // Le texte de la table. Refuse un texte qui n'est pas du YAML, dont `version`
  // n'est pas 1, ou dont une entrée n'a pas la forme attendue : une classe
  // entière, une taille numérique, une liste d'identifiants entiers.
  [[nodiscard]] static core::Result<MonsterSizeEffects> Parse(std::string_view yaml_text);

  // Ce que la table dit de cette classe, ou rien.
  [[nodiscard]] const Entry* For(std::uint32_t monster_id) const;

  // L'échelle du sprite de cette classe : un quand la table n'en dit rien, ou
  // qu'elle dit zéro.
  [[nodiscard]] float ScaleOf(std::uint32_t monster_id) const;

  [[nodiscard]] std::span<const Entry> entries() const noexcept { return entries_; }

 private:
  std::vector<Entry> entries_;
};

}  // namespace ro::vfs
