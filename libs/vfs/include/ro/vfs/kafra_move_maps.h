#pragma once

#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/lua_table.h"

namespace ro::vfs {

class Vfs;

// ── LE SERVICE DE TÉLÉPORTATION DES KAFRA ──────────────────────────────────
//
// Pour chaque ville où une Kafra téléporte, les cartes où elle envoie, dans
// l'ordre de la table. La carte du monde les montre au survol de la ville.
inline constexpr std::string_view kKafraMoveMapsPath =
    "data\\luafiles514\\lua files\\datainfo\\kaframovemapservicelist.lub";

class KafraMoveMaps {
 public:
  [[nodiscard]] static core::Result<KafraMoveMaps> Load(Vfs& vfs);
  // Depuis l'état où la table s'est lue : ce qui s'éprouve sans archive.
  [[nodiscard]] static KafraMoveMaps FromGlobals(const formats::LuaTable& globals);

  // Les destinations d'une ville, par leur nom interne sans extension ; vide
  // pour une ville sans service.
  [[nodiscard]] std::span<const std::string> DestinationsOf(std::string_view town) const;

 private:
  std::map<std::string, std::vector<std::string>, std::less<>> destinations_;
};

}  // namespace ro::vfs
