#include "world/map_editor_reasons.h"

#include <array>
#include <cstdio>
#include <string>
#include <utility>
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
  for (std::size_t slot = 0; slot < document.model_lights().size(); ++slot) {
    if (document.model_lights()[slot].light.why.empty()) {
      ro::mapedit::ModelLight carried = document.model_lights()[slot];
      std::snprintf(reason.data(), reason.size(), ro::core::Tr("Lumière de %s"),
                    carried.model_file.c_str());
      carried.light.why = reason.data();
      document.SetModelLight(slot, std::move(carried));
    }
  }
  for (std::size_t slot = 0; slot < document.model_effects().size(); ++slot) {
    if (document.model_effects()[slot].effect.why.empty()) {
      ro::mapedit::ModelEffect carried = document.model_effects()[slot];
      std::snprintf(reason.data(), reason.size(), ro::core::Tr("Effet #%d de %s"),
                    static_cast<int>(carried.effect.number), carried.model_file.c_str());
      carried.effect.why = reason.data();
      document.SetModelEffect(slot, std::move(carried));
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
  for (std::size_t slot = 0; slot < document.sprites().size(); ++slot) {
    if (document.sprites()[slot].why.empty()) {
      ro::mapedit::MapSprite sprite = document.sprites()[slot];
      std::snprintf(reason.data(), reason.size(), ro::core::Tr("Sprite de décor %s"),
                    sprite.sprite.c_str());
      sprite.why = reason.data();
      document.SetSprite(slot, std::move(sprite));
    }
  }
  if (!document.ground().empty() && document.ground_why().empty()) {
    document.SetGroundWhy(ro::core::Tr("Retouche du relief"));
  }
  if (!document.blocked_cells().empty() && document.blocked_cells_why().empty()) {
    document.SetBlockedCellsWhy(ro::core::Tr("Cases bloquées sous le décor"));
  }
  const std::vector<ro::mapedit::SoundEdit> sound_retouches = document.sound_edits();
  for (ro::mapedit::SoundEdit retouch : sound_retouches) {
    if (retouch.why.empty()) {
      std::snprintf(reason.data(), reason.size(), ro::core::Tr("Retouche du son %s"),
                    retouch.wave_file.c_str());
      retouch.why = reason.data();
      document.SetSoundEdit(std::move(retouch));
    }
  }
  for (std::size_t slot = 0; slot < document.sounds().size(); ++slot) {
    if (document.sounds()[slot].why.empty()) {
      ro::mapedit::AddedSound sound = document.sounds()[slot];
      std::snprintf(reason.data(), reason.size(), ro::core::Tr("Ajout du son %s"),
                    sound.wave_file.c_str());
      sound.why = reason.data();
      document.SetSound(slot, std::move(sound));
    }
  }
  return document;
}

std::string SaveRefusal(const ro::mapedit::MapEdit& document, std::size_t original_count) {
  Reason refused{};
  for (const ro::mapedit::ModelEdit& edit : document.models()) {
    if (edit.why.empty()) {
      std::snprintf(refused.data(), refused.size(),
                    ro::core::Tr("La retouche du modèle %zu n'a pas de raison."), edit.model_index);
      return refused.data();
    }
  }
  for (const ro::mapedit::EffectEdit& retouch : document.effect_edits()) {
    if (retouch.why.empty()) {
      std::snprintf(refused.data(), refused.size(),
                    ro::core::Tr("La retouche de l'effet %zu n'a pas de raison."),
                    retouch.effect_index);
      return refused.data();
    }
  }
  for (std::size_t slot = 0; slot < document.effects().size(); ++slot) {
    if (document.effects()[slot].why.empty()) {
      std::snprintf(refused.data(), refused.size(), ro::core::Tr("L'effet %zu n'a pas de raison."),
                    slot);
      return refused.data();
    }
  }
  for (std::size_t slot = 0; slot < document.lights().size(); ++slot) {
    if (document.lights()[slot].why.empty() || !(document.lights()[slot].range > 0.0F)) {
      std::snprintf(refused.data(), refused.size(),
                    ro::core::Tr("La lumière %zu n'a pas de raison, ou pas de portée."), slot);
      return refused.data();
    }
  }
  for (const ro::mapedit::MeshEdit& mesh : document.meshes()) {
    if (mesh.why.empty()) {
      std::snprintf(refused.data(), refused.size(),
                    ro::core::Tr("La retouche du maillage %zu n'a pas de raison."),
                    mesh.model_index);
      return refused.data();
    }
  }
  for (std::size_t slot = 0; slot < document.added().size(); ++slot) {
    if (document.added()[slot].why.empty()) {
      std::snprintf(refused.data(), refused.size(),
                    ro::core::Tr("La retouche du modèle %zu n'a pas de raison."),
                    original_count + slot);
      return refused.data();
    }
  }
  for (std::size_t slot = 0; slot < document.sprites().size(); ++slot) {
    if (document.sprites()[slot].why.empty()) {
      std::snprintf(refused.data(), refused.size(),
                    ro::core::Tr("Le sprite %zu n'a pas de raison."), slot);
      return refused.data();
    }
  }
  for (const ro::mapedit::SoundEdit& retouch : document.sound_edits()) {
    if (retouch.why.empty()) {
      std::snprintf(refused.data(), refused.size(),
                    ro::core::Tr("La retouche du son %zu n'a pas de raison."), retouch.sound_index);
      return refused.data();
    }
  }
  for (std::size_t slot = 0; slot < document.sounds().size(); ++slot) {
    const ro::mapedit::AddedSound& sound = document.sounds()[slot];
    if (sound.why.empty() || !(sound.range > 0.0F) || !(sound.cycle_seconds > 0.0F)) {
      std::snprintf(refused.data(), refused.size(),
                    ro::core::Tr("Le son ajouté %zu n'a pas de raison, de portée ou de cycle."),
                    slot);
      return refused.data();
    }
  }
  return {};
}

}  // namespace moonclient
