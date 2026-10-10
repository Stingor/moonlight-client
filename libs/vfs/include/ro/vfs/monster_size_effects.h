#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/lua_table.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LA TAILLE ET LES EFFETS QUE CERTAINS MONSTRES PORTENT D'OFFICE.
//
// Le serveur n'en dit rien : c'est une table de l'installation qui grandit tel
// monstre d'un cinquième et l'entoure de telle aura, par son numéro de classe.
//
//   tbl = {
//     [1002] = { MonsterSize = 1.2, MonsterEff = { EFFECT.EF_GREEN99_5 } },
//   }
//
// Le client en tire deux choses :
//
//   * la TAILLE multiplie le sprite du monstre, ombre comprise. Un pour sa
//     taille d'origine ; zéro ne change rien.
//   * les EFFETS s'attachent au monstre à son apparition, et le quittent quand
//     il se cache. Un effet négatif (`EF_NONE`) n'en est pas un ; un
//     `MonsterEff` qui n'est pas une liste n'en pose aucun.
//
// ⚠ LE FICHIER TRADUIT, comme les autres tables du client patché : c'est lui
// que le client lit, et l'installation n'a plus de dossier `System\`.
inline constexpr std::string_view kMonsterSizeEffectPath = "SystemEN\\monster_size_effect.lub";

inline constexpr std::string_view kMonsterSizeEffectGlobal = "tbl";
inline constexpr std::string_view kMonsterSizeField = "MonsterSize";
inline constexpr std::string_view kMonsterEffectField = "MonsterEff";

class MonsterSizeEffects {
 public:
  struct Entry {
    std::uint32_t monster_id = 0;
    float size = 0.0F;
    std::vector<std::uint32_t> effect_ids;
  };

  // Lit la table de l'installation. Absente, elle n'est pas une faute pour
  // l'appelant : aucun monstre n'est alors grandi ni paré.
  [[nodiscard]] static core::Result<MonsterSizeEffects> Load(Vfs& vfs);

  // La MÊME lecture, à partir de globales déjà lues — la moitié qui se met sous
  // test, aucun asset du jeu n'étant versé au dépôt.
  [[nodiscard]] static core::Result<MonsterSizeEffects> Parse(const formats::LuaTable& globals);

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
