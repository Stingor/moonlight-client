#include "world/map_edit_store.h"

#include <fstream>
#include <iterator>
#include <optional>
#include <system_error>
#include <utility>

#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"
#include "ro/render/map_scene.h"

namespace moonclient {

namespace {

// LE DOCUMENT QUE LE MIROIR A LIVRÉ pour `map_name`, s'il y en a un, lisible et
// révisé. Un document illisible ou sans révision ne vaut rien ici : il ne
// saurait se comparer à celui du poste.
std::optional<ro::mapedit::RevisedMapEdit> ReadServed(std::string_view map_name,
                                                      ro::vfs::Vfs& vfs) {
  auto bytes = vfs.Read(ro::mapedit::ServedMapEditPath(map_name));
  if (!bytes) {
    return std::nullopt;
  }
  auto revised = ro::mapedit::ParseRevisedMapEdit(
      std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()), map_name);
  if (!revised) {
    ro::core::LogWarning("édition de {} livrée par le miroir, écartée — {}", map_name,
                         ro::core::Describe(revised.error()));
    return std::nullopt;
  }
  if (revised->revision == 0) {
    ro::core::LogWarning("édition de {} livrée par le miroir sans révision, écartée", map_name);
    return std::nullopt;
  }
  return std::move(*revised);
}

}  // namespace

void MapEditStore::Load(std::string_view map_name, ro::vfs::Vfs& vfs) {
  if (by_map_.contains(map_name)) {
    return;
  }
  Entry& entry = by_map_[std::string(map_name)];
  std::optional<ro::mapedit::RevisedMapEdit> served = ReadServed(map_name, vfs);
  std::ifstream stream(FileOf(map_name), std::ios::binary);
  if (!stream) {
    if (served.has_value()) {
      ro::core::LogInfo("édition de {}, révision {}, livrée par le miroir", map_name,
                        served->revision);
      entry.edit = std::move(served->edit);
      entry.revision = served->revision;
      return;
    }
    entry.edit = ro::render::LoadMapEdit(vfs, map_name);
    return;
  }
  const std::string text((std::istreambuf_iterator<char>(stream)),
                         std::istreambuf_iterator<char>());
  auto revised = ro::mapedit::ParseRevisedMapEdit(text, map_name);
  if (!revised) {
    ro::core::LogWarning("{} — {} s'ouvre sans édition", ro::core::Describe(revised.error()),
                         map_name);
    return;
  }
  // LA RÉVISION LA PLUS HAUTE L'EMPORTE. À égalité, celle du poste : elle peut
  // porter des retouches enregistrées sur cette révision, pas encore publiées.
  if (served.has_value() && served->revision > revised->revision) {
    ro::core::LogInfo("édition de {} : révision {} du miroir, plus récente que la {} du poste",
                      map_name, served->revision, revised->revision);
    entry.edit = std::move(served->edit);
    entry.revision = served->revision;
    return;
  }
  ro::core::LogInfo("édition de {}, révision {}, lue dans {}", map_name, revised->revision,
                    ro::core::Utf8FromPath(FileOf(map_name)));
  entry.edit = std::move(revised->edit);
  entry.revision = revised->revision;
}

const ro::mapedit::MapEdit& MapEditStore::Document(std::string_view map_name) const {
  static const ro::mapedit::MapEdit kNoEdit;
  const auto found = by_map_.find(map_name);
  return found == by_map_.end() ? kNoEdit : found->second.edit;
}

void MapEditStore::Set(std::string_view map_name, ro::mapedit::MapEdit edit) {
  Entry& entry = by_map_[std::string(map_name)];
  entry.edit = std::move(edit);
  entry.unsaved = true;
}

bool MapEditStore::Unsaved(std::string_view map_name) const {
  const auto found = by_map_.find(map_name);
  return found != by_map_.end() && found->second.unsaved;
}

std::uint32_t MapEditStore::Revision(std::string_view map_name) const {
  const auto found = by_map_.find(map_name);
  return found == by_map_.end() ? 0 : found->second.revision;
}

void MapEditStore::SetRevision(std::string_view map_name, std::uint32_t revision) {
  by_map_[std::string(map_name)].revision = revision;
}

ro::core::Result<void> MapEditStore::Adopt(std::string_view map_name, ro::mapedit::MapEdit edit,
                                           std::uint32_t revision) {
  if (auto written = WriteServed(map_name, edit, revision); !written) {
    return written;
  }
  Entry& entry = by_map_[std::string(map_name)];
  entry.edit = std::move(edit);
  entry.revision = revision;
  entry.unsaved = false;
  return {};
}

ro::core::Result<void> MapEditStore::WriteServed(std::string_view map_name,
                                                 const ro::mapedit::MapEdit& edit,
                                                 std::uint32_t revision) const {
  const std::filesystem::path path = FileOf(map_name);
  std::error_code ignored;
  std::filesystem::create_directories(path.parent_path(), ignored);
  return ro::core::WriteFileAtomically(path,
                                       ro::mapedit::RevisedMapEditToYaml(edit, map_name, revision));
}

std::filesystem::path MapEditStore::FileOf(std::string_view map_name) const {
  return client_root_ / "data" / "map_edits" /
         ro::core::PathFromUtf8(std::string(map_name) + ".yml");
}

ro::core::Result<void> MapEditStore::Save(std::string_view map_name) {
  const std::filesystem::path path = FileOf(map_name);
  std::error_code ignored;
  std::filesystem::create_directories(path.parent_path(), ignored);
  auto written = ro::core::WriteFileAtomically(
      path, ro::mapedit::RevisedMapEditToYaml(Document(map_name), map_name, Revision(map_name)));
  if (written) {
    by_map_[std::string(map_name)].unsaved = false;
  }
  return written;
}

}  // namespace moonclient
