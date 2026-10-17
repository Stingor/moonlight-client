#include "world/map_editor_reasons.h"

#include <array>
#include <cstdio>
#include <string>
#include <vector>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

// La place d'une raison générique, zéro final compris.
constexpr std::size_t kReasonCapacity = 192;
using Reason = std::array<char, kReasonCapacity>;

}  // namespace

ro::mapedit::MapEdit WithReasons(ro::mapedit::MapEdit document) {
  Reason reason{};
  const std::vector<ro::mapedit::ModelEdit> models = document.models();
  for (ro::mapedit::ModelEdit edit : models) {
    if (edit.why.empty()) {
      std::snprintf(reason.data(), reason.size(), ro::core::Tr("Retouche de %s"),
                    edit.model_file.c_str());
      edit.why = reason.data();
      document.SetModel(std::move(edit));
    }
  }
  const std::vector<ro::mapedit::MeshEdit> meshes = document.meshes();
  for (ro::mapedit::MeshEdit mesh : meshes) {
    if (mesh.why.empty()) {
      std::snprintf(reason.data(), reason.size(), ro::core::Tr("Retouche du maillage de %s"),
                    mesh.model_file.c_str());
      mesh.why = reason.data();
      document.SetMesh(std::move(mesh));
    }
  }
  const std::vector<ro::mapedit::EffectEdit> retouches = document.effect_edits();
  for (ro::mapedit::EffectEdit retouch : retouches) {
    if (retouch.why.empty()) {
      std::snprintf(reason.data(), reason.size(), ro::core::Tr("Retouche de l'effet #%d"),
                    static_cast<int>(retouch.number));
      retouch.why = reason.data();
      document.SetEffectEdit(std::move(retouch));
    }
  }
  for (std::size_t slot = 0; slot < document.effects().size(); ++slot) {
    if (document.effects()[slot].why.empty()) {
      ro::mapedit::AddedEffect effect = document.effects()[slot];
      std::snprintf(reason.data(), reason.size(), ro::core::Tr("Ajout de l'effet #%d"),
                    static_cast<int>(effect.number));
      effect.why = reason.data();
      document.SetEffect(slot, std::move(effect));
    }
  }
  for (std::size_t slot = 0; slot < document.lights().size(); ++slot) {
    if (document.lights()[slot].why.empty()) {
      ro::mapedit::AddedLight light = document.lights()[slot];
      light.why = ro::core::Tr("Ajout d'une lumière");
      document.SetLight(slot, std::move(light));
    }
  }
  for (std::size_t slot = 0; slot < document.added().size(); ++slot) {
    if (document.added()[slot].why.empty()) {
      ro::mapedit::AddedModel added = document.added()[slot];
      std::snprintf(reason.data(), reason.size(), ro::core::Tr("Ajout de %s"),
                    added.model_file.c_str());
      added.why = reason.data();
      document.SetAdded(slot, std::move(added));
    }
  }
  if (!document.ground().empty() && document.ground_why().empty()) {
    document.SetGroundWhy(ro::core::Tr("Retouche du relief"));
  }
  return document;
}

}  // namespace moonclient
