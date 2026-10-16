// LA RETOUCHE DES MODÈLES EN DIRECT, que l'établi du staff règle : voir
// docs/adr/0117 et `ro::ui::StaffMapFixEditor`.
//
// L'établi tient les valeurs ; ici, elles deviennent une retouche dans la table
// de travail, le décor se repose aussitôt, et « Enregistrer » écrit la table.

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"
#include "ro/vfs/encoding.h"

#include "game/game.h"
#include "world/map_fix_file.h"

namespace moonclient {
namespace {

// La place d'une phrase d'état, zéro final compris.
constexpr std::size_t kStatusCapacity = 160;

// La retouche de `rank` dans `fixes`, ou rien.
const ro::render::ModelFix* FixOf(const ro::render::MapFixes& fixes, std::size_t rank) {
  const auto found = std::ranges::find(fixes.models, rank, &ro::render::ModelFix::model_index);
  return found == fixes.models.end() ? nullptr : &*found;
}

std::string WhyOf(const ro::ui::StaffMapFixEditor& editor) {
  return std::string(editor.why.data(), strnlen(editor.why.data(), editor.why.size()));
}

void SetWhy(ro::ui::StaffMapFixEditor& editor, std::string_view why) {
  editor.why.fill('\0');
  const std::size_t kept = std::min(why.size(), editor.why.size() - 1);
  std::copy_n(why.begin(), kept, editor.why.begin());
}

}  // namespace

void Game::ServeMapFixEditor() {
  ro::ui::StaffMapFixEditor& editor = windows_.staff_tools.map_fix;
  if (!Rights().staff_tools || !stage_.has_value()) {
    editor.picking = false;
    return;
  }
  const std::string& map_name = stage_->name;
  ro::render::MapScene& scene = stage_->scene;
  const ro::render::MapFixes fixes = map_fixes_.FixesOf(map_name);
  editor.fixed_models.clear();
  for (const ro::render::ModelFix& fix : fixes.models) {
    editor.fixed_models.push_back(fix.model_index);
  }

  if (std::exchange(editor.save_asked, false)) {
    SaveMapFixes(editor);
  }

  // UNE AUTRE CARTE, OU UN AUTRE MODÈLE : l'établi reprend ses valeurs.
  if (!editor.model_index.has_value() || *editor.model_index >= scene.unfixed_models.size()) {
    if (map_name != map_fix_loaded_map_) {
      editor.model_index.reset();
    }
    map_fix_loaded_rank_.reset();
    map_fix_loaded_map_ = map_name;
    return;
  }
  const std::size_t rank = *editor.model_index;
  const ro::formats::RswModel& original = scene.unfixed_models[rank];
  const ro::render::ModelFix* fix = FixOf(fixes, rank);
  const bool reverting = std::exchange(editor.revert_asked, false);
  if (reverting || map_fix_loaded_rank_ != rank || map_fix_loaded_map_ != map_name) {
    if (reverting && fix != nullptr) {
      map_fixes_.RemoveModelFix(map_name, rank);
      map_fixes_unsaved_ = true;
      ReplaceMapModels();
      fix = nullptr;
    }
    editor.model_file = ro::vfs::Cp949ToUtf8Lossy(original.model_name);
    editor.origin = original.position;
    editor.position =
        fix != nullptr ? fix->position.value_or(original.position) : original.position;
    editor.rotation =
        fix != nullptr ? fix->rotation.value_or(original.rotation) : original.rotation;
    editor.scale = fix != nullptr ? fix->scale.value_or(original.scale) : original.scale;
    editor.hidden = fix != nullptr && fix->hidden;
    SetWhy(editor, fix != nullptr ? std::string_view(fix->why) : std::string_view());
    map_fix_loaded_rank_ = rank;
    map_fix_loaded_map_ = map_name;
    editor.unsaved = map_fixes_unsaved_;
    return;
  }

  // CE QUE L'ÉTABLI RÈGLE, en retouche : seuls les champs qui s'écartent du
  // placement d'origine s'écrivent.
  ro::render::ModelFix wanted;
  wanted.model_index = rank;
  wanted.model_file = editor.model_file;
  wanted.origin = original.position;
  if (editor.position != original.position) {
    wanted.position = editor.position;
  }
  if (editor.rotation != original.rotation) {
    wanted.rotation = editor.rotation;
  }
  if (editor.scale != original.scale) {
    wanted.scale = editor.scale;
  }
  wanted.hidden = editor.hidden;
  wanted.why = WhyOf(editor);
  const bool idle = !wanted.position.has_value() && !wanted.rotation.has_value() &&
                    !wanted.scale.has_value() && !wanted.hidden;
  if (idle ? fix != nullptr : (fix == nullptr || !(*fix == wanted))) {
    // Une raison seule ne repose rien : le décor ne bouge que si la pose change.
    const bool moved = fix == nullptr || fix->position != wanted.position ||
                       fix->rotation != wanted.rotation || fix->scale != wanted.scale ||
                       fix->hidden != wanted.hidden;
    if (idle) {
      map_fixes_.RemoveModelFix(map_name, rank);
    } else {
      map_fixes_.SetModelFix(map_name, std::move(wanted));
    }
    map_fixes_unsaved_ = true;
    if (moved) {
      ReplaceMapModels();
    }
  }
  editor.unsaved = map_fixes_unsaved_;
}

void Game::ReplaceMapModels() {
  ro::render::MapScene& scene = stage_->scene;
  const ro::render::MapFixReport report =
      ro::render::ReplaceSceneModels(scene, map_fixes_.FixesOf(stage_->name));
  for (const std::string& refusal : report.refused) {
    ro::core::LogWarning("retouche de {} refusée — {}", stage_->name, refusal);
  }
  if (auto uploaded = models_.Upload(scene.models); !uploaded) {
    ro::core::LogError("retouche de {} : le décor ne se téléverse pas — {}", stage_->name,
                       ro::core::Describe(uploaded.error()));
  }
  // L'horloge des modèles animés repart sur les nouvelles poses.
  model_clock_ = ro::render::ModelClock();
}

void Game::SaveMapFixes(ro::ui::StaffMapFixEditor& editor) {
  // UNE RETOUCHE SANS RAISON NE S'ÉCRIT PAS : la table la refuserait à la
  // relecture, et toutes les autres avec elle.
  for (const std::size_t rank : editor.fixed_models) {
    const ro::render::ModelFix* fix = FixOf(map_fixes_.FixesOf(stage_->name), rank);
    if (fix != nullptr && fix->why.empty()) {
      std::array<char, kStatusCapacity> status{};
      std::snprintf(status.data(), status.size(),
                    ro::core::Tr("La retouche du rang %zu n'a pas de raison."), rank);
      editor.status = status.data();
      return;
    }
  }
  if (auto saved = SaveMapFixFile(client_root_, map_fixes_); !saved) {
    editor.status = ro::core::Describe(saved.error());
    ro::core::LogError("retouches de carte : {}", editor.status);
    return;
  }
  map_fixes_unsaved_ = false;
  editor.unsaved = false;
  editor.status = ro::core::Tr("Retouches enregistrées.");
  ro::core::LogInfo("retouches de carte enregistrées dans {}",
                    ro::core::Utf8FromPath(MapFixFilePath(client_root_)));
}

}  // namespace moonclient
