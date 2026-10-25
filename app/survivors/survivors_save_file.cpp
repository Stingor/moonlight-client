// LE CARNET SUR LE DISQUE : relire, appliquer, écrire (survivors_save.h).

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"
#include "ro/sim/survivors_week.h"

#include "game/save_folder.h"
#include "game/yaml_load.h"
#include "survivors/survivors_save.h"

namespace moonclient {
namespace {

// Deux fichiers abîmés dans la même seconde prennent un suffixe `-2`, `-3`… :
// au-delà, la copie renonce plutôt que d'écraser une copie.
constexpr int kMostDamagedCopiesPerSecond = 100;

}  // namespace

std::filesystem::path SurvivorsSavePath(const std::filesystem::path& client_root) {
  return client_root / ro::core::PathFromUtf8(kSaveFolder) /
         ro::core::PathFromUtf8(kSurvivorsSaveFileName);
}

ro::core::Result<SurvivorsSaveApplied> ApplySurvivorsSaveDelta(std::string_view text,
                                                               const SurvivorsSaveDelta& delta) {
  ro::core::Result<SurvivorsSave> save = ReadSurvivorsSave(text);
  if (!save) return std::unexpected(save.error());
  SurvivorsSaveApplied applied{.save = std::move(*save)};
  if (delta(applied.save)) applied.text = WriteSurvivorsSave(applied.save);
  return applied;
}

std::string DamagedCopyName(std::chrono::system_clock::time_point now) {
  const ro::sim::UtcInstant instant = ro::sim::UtcInstantOf(
      std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
  return std::format("{}.damaged-{:04}{:02}{:02}-{:02}{:02}{:02}", kSurvivorsSaveFileName,
                     instant.date.year, unsigned{instant.date.month}, unsigned{instant.date.day},
                     unsigned{instant.hour}, unsigned{instant.minute}, unsigned{instant.second});
}

SurvivorsSaveFile::SurvivorsSaveFile(Clock now) : now_(std::move(now)) {
  if (!now_) now_ = [] { return std::chrono::system_clock::now(); };
}

std::optional<std::filesystem::path> SurvivorsSaveFile::CopyDamaged(
    const std::filesystem::path& path) const {
  const std::string name = DamagedCopyName(now_());
  for (int attempt = 1; attempt <= kMostDamagedCopiesPerSecond; ++attempt) {
    const std::string suffixed = attempt == 1 ? name : std::format("{}-{}", name, attempt);
    const std::filesystem::path copy = path.parent_path() / ro::core::PathFromUtf8(suffixed);
    std::error_code taken;
    if (std::filesystem::exists(copy, taken)) continue;
    std::error_code failed;
    if (std::filesystem::copy_file(path, copy, std::filesystem::copy_options::none, failed)) {
      return copy;
    }
    ro::core::LogWarning("survivors : {} abîmé ne se copie pas sous {} ({})",
                         ro::core::Utf8FromPath(path), ro::core::Utf8FromPath(copy),
                         failed.message());
    return std::nullopt;
  }
  return std::nullopt;
}

SurvivorsSaveRead SurvivorsSaveFile::Load(const std::filesystem::path& client_root) const {
  const std::filesystem::path path = SurvivorsSavePath(client_root);
  const std::optional<std::string> text = ReadFileText(path);
  if (!text.has_value()) {
    ro::core::LogWarning("survivors : {} ne s'ouvre pas — il ne sera réécrit qu'une fois relu",
                         ro::core::Utf8FromPath(path));
    return {.file_unread = true};
  }
  ro::core::Result<SurvivorsSave> read = ReadSurvivorsSave(*text);
  if (read) return {.save = std::move(*read)};
  // Abîmé : copié de côté avant d'être remplacé, jamais remplacé sans copie.
  std::optional<std::filesystem::path> copy = CopyDamaged(path);
  if (!copy.has_value()) return {.file_unread = true};
  if (const ro::core::Result<void> written =
          ro::core::WriteFileAtomically(path, WriteSurvivorsSave(SurvivorsSave{}));
      !written) {
    ro::core::LogWarning("survivors : carnet vierge non écrit ({})",
                         ro::core::Describe(written.error()));
  }
  ro::core::LogWarning("survivors : {} abîmé ({}) — copié sous {}, carnet vierge",
                       ro::core::Utf8FromPath(path), ro::core::Describe(read.error()),
                       ro::core::Utf8FromPath(*copy));
  return {.damaged_copy = std::move(copy)};
}

ro::core::Result<SurvivorsSaveRead> SurvivorsSaveFile::Mutate(
    const std::filesystem::path& client_root, const SurvivorsSaveDelta& delta) const {
  const std::filesystem::path path = SurvivorsSavePath(client_root);
  const std::optional<std::string> text = ReadFileText(path);
  if (!text.has_value()) {
    return ro::core::Fail(ro::core::ErrorCode::kIoFailure, ro::core::Utf8FromPath(path));
  }
  ro::core::Result<SurvivorsSaveApplied> applied = ApplySurvivorsSaveDelta(*text, delta);
  if (applied) {
    if (applied->text.has_value()) {
      if (ro::core::Result<void> written = ro::core::WriteFileAtomically(path, *applied->text);
          !written) {
        return std::unexpected(written.error());
      }
    }
    return SurvivorsSaveRead{.save = std::move(applied->save)};
  }
  SurvivorsSaveRead result{.damaged_copy = CopyDamaged(path)};
  if (!result.damaged_copy.has_value()) {
    return ro::core::Fail(ro::core::ErrorCode::kIoFailure,
                          std::format("{} abîmé ne se copie pas", ro::core::Utf8FromPath(path)));
  }
  ro::core::LogWarning("survivors : {} abîmé ({}) — copié sous {}, carnet vierge",
                       ro::core::Utf8FromPath(path), ro::core::Describe(applied.error()),
                       ro::core::Utf8FromPath(*result.damaged_copy));
  // Le carnet vierge remplace l'abîmé même sans delta : la copie est faite.
  static_cast<void>(delta(result.save));
  if (ro::core::Result<void> written =
          ro::core::WriteFileAtomically(path, WriteSurvivorsSave(result.save));
      !written) {
    return std::unexpected(written.error());
  }
  return result;
}

SurvivorsOptions LoadSurvivorsOptions(const std::filesystem::path& client_root) {
  return SurvivorsSaveFile().Load(client_root).save.options;
}

ro::core::Result<SurvivorsSaveRead> SaveSurvivorsOptions(const std::filesystem::path& client_root,
                                                         const SurvivorsOptions& before,
                                                         const SurvivorsOptions& after) {
  return SurvivorsSaveFile().Mutate(client_root, [&before, &after](SurvivorsSave& save) {
    const SurvivorsOptions read = save.options;
    SurvivorsOptions& kept = save.options;
    if (after.reduced_effects != before.reduced_effects) {
      kept.reduced_effects = after.reduced_effects;
    }
    if (after.show_measures != before.show_measures) kept.show_measures = after.show_measures;
    if (after.horde_sun_shadows != before.horde_sun_shadows) {
      kept.horde_sun_shadows = after.horde_sun_shadows;
    }
    if (after.first_skill_id != before.first_skill_id) {
      kept.first_skill_id = after.first_skill_id;
    }
    return kept != read;
  });
}

}  // namespace moonclient
