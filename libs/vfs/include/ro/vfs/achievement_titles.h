#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/lua_table.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LE TITRE QUE RÉCOMPENSE CHAQUE SUCCÈS.
//
// 🔴 LE SERVEUR NE LE DIT JAMAIS. La liste des succès porte des numéros de
// succès et des drapeaux ; le titre qu'un succès donne n'est écrit que dans
// cette table, et c'est le client qui fait le croisement. Le serveur tient la
// même correspondance dans sa propre base, et les deux doivent s'accorder —
// un titre écrit d'un seul côté ne se propose jamais, ou se refuse.
//
// ⚠ LE FICHIER TRADUIT, PAS CELUI DU JEU D'ORIGINE : c'est lui que ce serveur
// complète de ses succès maison, et l'autre ne connaît aucun de leurs titres.
inline constexpr std::string_view kAchievementTablePath = "SystemEN\\achievements.lub";

// La globale qui range les succès, indexés par leur numéro.
inline constexpr std::string_view kAchievementTableGlobal = "achievement_tbl";

// Le chemin du titre dans une entrée : `reward = { title = 1000 }`.
inline constexpr std::string_view kAchievementRewardField = "reward";
inline constexpr std::string_view kAchievementTitleField = "title";

// ⚠ LE PLUS GRAND NUMÉRO ACCEPTÉ, de succès comme de titre : les deux voyagent
// en trente-deux bits signés, et le client les range ainsi.
inline constexpr std::uint32_t kMostAchievementNumber =
    static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());

class AchievementTitles {
 public:
  [[nodiscard]] static core::Result<AchievementTitles> Load(Vfs& vfs);

  // La MÊME lecture, à partir de globales déjà lues — la seule moitié qui se
  // mette sous test, puisqu'aucun asset du jeu n'est versé au dépôt.
  [[nodiscard]] static core::Result<AchievementTitles> Parse(const formats::LuaTable& globals);

  // Le titre qu'un succès récompense, ou rien. ⚠ RIEN EST LE CAS ORDINAIRE : la
  // plupart des succès donnent un objet ou rien du tout.
  [[nodiscard]] std::optional<std::int32_t> TitleOf(std::uint32_t achievement_id) const;

  struct Entry {
    std::uint32_t achievement_id = 0;
    std::int32_t title_id = 0;
  };

  [[nodiscard]] std::span<const Entry> entries() const noexcept { return entries_; }

 private:
  std::vector<Entry> entries_;
};

}  // namespace ro::vfs
