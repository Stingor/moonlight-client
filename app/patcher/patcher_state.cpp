#include "patcher/patcher_state.h"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>

#include "ro/core/log.h"

#include "web/json_value.h"

namespace moonclient::patcher {
namespace {

constexpr std::string_view kLastIndexKey = "last_patch_index";

}  // namespace

ro::core::Result<std::uint64_t> ParsePatcherState(std::string_view text) {
  auto document = web::ParseJson(text);
  if (!document) {
    return std::unexpected(document.error());
  }
  const web::JsonValue* field = document->Field(kLastIndexKey);
  const std::optional<std::int64_t> index =
      field != nullptr ? field->Whole() : std::optional<std::int64_t>();
  if (!index.has_value() || *index < 0) {
    return ro::core::Fail(ro::core::ErrorCode::kCorruptData,
                          std::string(kPatcherStateFileName) + " : pas d'index de patch lisible");
  }
  return static_cast<std::uint64_t>(*index);
}

std::string FormatPatcherState(std::uint64_t last_applied_index) {
  return "{\"" + std::string(kLastIndexKey) + "\":" + std::to_string(last_applied_index) + "}";
}

std::optional<std::uint64_t> ReadLastAppliedPatch(const std::filesystem::path& install_root) {
  const std::filesystem::path state_path = install_root / kPatcherStateFileName;
  std::ifstream file(state_path, std::ios::binary);
  if (!file) {
    return std::nullopt;
  }
  const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  auto index = ParsePatcherState(text);
  if (!index) {
    ro::core::LogWarning("patcher : {} illisible ({})", state_path.string(),
                         ro::core::Describe(index.error()));
    return std::nullopt;
  }
  return *index;
}

}  // namespace moonclient::patcher
