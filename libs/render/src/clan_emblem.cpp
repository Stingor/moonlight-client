// Les emblèmes de clan. Voir l'en-tête : un fichier par identifiant.

#include "ro/render/clan_emblem.h"

#include <format>
#include <utility>

#include "ro/core/log.h"

namespace ro::render {

std::string ClanEmblemPath(std::int32_t clan_id) {
  return std::format("data\\texture\\유저인터페이스\\clan_system\\clan_emblem{:02}.bmp", clan_id);
}

const formats::Image* ClanEmblems::Of(std::int32_t clan_id) const {
  const auto found = by_clan.find(clan_id);
  return found == by_clan.end() ? nullptr : &found->second;
}

ClanEmblems LoadClanEmblems(vfs::Vfs& vfs) {
  ClanEmblems emblems;
  // ⚠ DÈS UN : zéro veut dire « sans clan », et son fichier ne se montre pas.
  for (std::int32_t clan_id = 1; clan_id <= kLastClanEmblemId; ++clan_id) {
    const std::string path = ClanEmblemPath(clan_id);
    if (!vfs.Contains(path)) {
      continue;
    }
    auto bytes = vfs.Read(path);
    if (!bytes) {
      core::LogWarning("emblème de clan : {} illisible ({})", path, core::Describe(bytes.error()));
      continue;
    }
    auto image = formats::DecodeImage(path, *bytes);
    if (!image) {
      core::LogWarning("emblème de clan : {} illisible ({})", path, core::Describe(image.error()));
      continue;
    }
    // Le magenta de ces bitmaps d'interface est leur FOND.
    formats::ApplyMagentaColorKey(*image);
    emblems.by_clan.emplace(clan_id, std::move(*image));
  }
  return emblems;
}

}  // namespace ro::render
