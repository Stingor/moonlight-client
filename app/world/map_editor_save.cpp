// L'ENREGISTREMENT DU DOCUMENT ET L'HISTORIQUE DES GESTES de l'éditeur de
// carte : voir `MapEditorSession`.

#include <array>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"

#include "world/map_editor_session.h"

namespace moonclient {
namespace {

// La place d'une phrase d'état, zéro final compris.
constexpr std::size_t kLineCapacity = 192;
using Line = std::array<char, kLineCapacity>;

}  // namespace

bool MapEditorSession::Save(MapEditStore& store) {
  // UNE RETOUCHE SANS RAISON NE S'ÉCRIT PAS : le document la refuserait à la
  // relecture, et toutes les autres avec elle.
  for (const ro::mapedit::ModelEdit& edit : store.Document(map_name_).models()) {
    if (edit.why.empty()) {
      Line refused{};
      std::snprintf(refused.data(), refused.size(),
                    ro::core::Tr("La retouche du modèle %zu n'a pas de raison."), edit.model_index);
      status_ = refused.data();
      return false;
    }
  }
  for (const ro::mapedit::EffectEdit& retouch : store.Document(map_name_).effect_edits()) {
    if (retouch.why.empty()) {
      Line refused{};
      std::snprintf(refused.data(), refused.size(),
                    ro::core::Tr("La retouche de l'effet %zu n'a pas de raison."),
                    retouch.effect_index);
      status_ = refused.data();
      return false;
    }
  }
  const std::vector<ro::mapedit::AddedEffect>& effects = store.Document(map_name_).effects();
  for (std::size_t slot = 0; slot < effects.size(); ++slot) {
    if (effects[slot].why.empty()) {
      Line refused{};
      std::snprintf(refused.data(), refused.size(), ro::core::Tr("L'effet %zu n'a pas de raison."),
                    slot);
      status_ = refused.data();
      return false;
    }
  }
  const std::vector<ro::mapedit::AddedLight>& lights = store.Document(map_name_).lights();
  for (std::size_t slot = 0; slot < lights.size(); ++slot) {
    if (lights[slot].why.empty() || !(lights[slot].range > 0.0F)) {
      Line refused{};
      std::snprintf(refused.data(), refused.size(),
                    ro::core::Tr("La lumière %zu n'a pas de raison, ou pas de portée."), slot);
      status_ = refused.data();
      return false;
    }
  }
  for (const ro::mapedit::MeshEdit& mesh : store.Document(map_name_).meshes()) {
    if (mesh.why.empty()) {
      Line refused{};
      std::snprintf(refused.data(), refused.size(),
                    ro::core::Tr("La retouche du maillage %zu n'a pas de raison."),
                    mesh.model_index);
      status_ = refused.data();
      return false;
    }
  }
  const std::vector<ro::mapedit::AddedModel>& added = store.Document(map_name_).added();
  for (std::size_t slot = 0; slot < added.size(); ++slot) {
    if (added[slot].why.empty()) {
      Line refused{};
      std::snprintf(refused.data(), refused.size(),
                    ro::core::Tr("La retouche du modèle %zu n'a pas de raison."),
                    original_count_ + slot);
      status_ = refused.data();
      return false;
    }
  }
  const std::vector<ro::mapedit::MapSprite>& sprites = store.Document(map_name_).sprites();
  for (std::size_t slot = 0; slot < sprites.size(); ++slot) {
    if (sprites[slot].why.empty()) {
      Line refused{};
      std::snprintf(refused.data(), refused.size(),
                    ro::core::Tr("Le sprite %zu n'a pas de raison."), slot);
      status_ = refused.data();
      return false;
    }
  }
  if (auto saved = store.Save(map_name_); !saved) {
    status_ = ro::core::Describe(saved.error());
    ro::core::LogError("édition de {} : {}", map_name_, status_);
    return false;
  }
  status_ = ro::core::Tr("Enregistré.");
  ro::core::LogInfo("édition de {} enregistrée dans {}", map_name_,
                    ro::core::Utf8FromPath(store.FileOf(map_name_)));
  return true;
}

void MapEditorSession::Record(std::string label, const ro::mapedit::MapEdit& before,
                              const ro::mapedit::MapEdit& after) {
  if (!(before == after)) {
    history_.Record(std::move(label), before, after);
  }
}

}  // namespace moonclient
