#include "ro/render/map_effects.h"

#include <algorithm>
#include <map>
#include <utility>

#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/sprite_billboard.h"

namespace ro::render {
namespace {

// Le dossier sous lequel le client cherche ces textures.
constexpr std::string_view kTextureFolder = "data\\texture\\";

// Au-delà de ce retard, le reste est abandonné : un chargement de carte ne doit
// pas faire jouer d'un coup des secondes de vie.
constexpr std::uint32_t kLongestCatchUpMs =
    static_cast<std::uint32_t>(kEffectNodeMaxCatchUpTicks * kEffectTickMs);

[[nodiscard]] glm::vec3 NativePlaceOf(const formats::RswEffect& object) {
  return {object.position[0], object.position[1], object.position[2]};
}

// Le réglage que reçoit l'objet de ce rang, ou les défauts de la classe.
template <typename Setting>
[[nodiscard]] Setting SettingOfRank(const std::map<std::int32_t, Setting>& entries,
                                    std::int32_t rank) {
  const auto found = entries.find(rank);
  return found != entries.end() ? found->second : Setting{};
}

}  // namespace

void MapEffects::Raise(const MapEffectTable& table, std::span<const formats::RswEffect> objects,
                       std::int32_t gnd_width, std::int32_t gnd_height) {
  frame_ = MapEffectFrame{gnd_width, gnd_height};
  emitters_.clear();
  winds_.clear();
  chimneys_.clear();
  makers_.clear();
  sown_.clear();
  // Les lots d'avant désignent les réglages des effets qu'on vient d'effacer.
  lots_.clear();
  started_ = false;
  owed_ms_ = 0;
  // 🔴 LE RANG SE COMPTE PAR NUMÉRO : c'est la liste que le gestionnaire tient
  // pour chacun, dans l'ordre où
  // les objets l'ont rejointe.
  std::int32_t emitter_rank = 0;
  std::int32_t wind_rank = 0;
  for (const formats::RswEffect& object : objects) {
    if (object.number == kMapChimneySmokeEffect) {
      chimneys_.emplace_back(NativePlaceOf(object));
    } else if (object.number == kMapEmitterEffect) {
      emitters_.emplace_back(NativePlaceOf(object), SettingOfRank(table.emitters, emitter_rank),
                             draws_);
      ++emitter_rank;
    } else if (object.number == kMapWindEffect) {
      winds_.emplace_back(NativePlaceOf(object), SettingOfRank(table.winds, wind_rank), draws_);
      ++wind_rank;
    } else if (!IsMapEffectClassNumber(object.number)) {
      // 🔴 TOUT CE QUI N'EST PAS UNE CLASSE SUIVIE EST UN SEMEUR, y compris un
      // numéro qu'aucune de nos tables ne connaît : le client n'écarte rien
      // ici, il confie l'objet à son semeur d'effets et c'est le moteur qui
      // décide s'il sait le jouer. Trier ici sur ce que nous savons peindre
      // masquerait les absences au lieu de les montrer.
      makers_.emplace_back(NativePlaceOf(object), object.number, object.emit_speed, object.param,
                           draws_);
    }
  }
}

void MapEffects::AddChimney(glm::vec3 world_position, std::uint32_t owner_id) {
  chimneys_.emplace_back(frame_.ToNative(world_position), owner_id);
}

void MapEffects::Advance(std::uint32_t now_ms, glm::vec3 player_world) {
  sown_.clear();
  if (empty()) {
    return;
  }
  if (!started_) {
    started_ = true;
    last_ms_ = now_ms;
    return;
  }
  const std::uint32_t elapsed = now_ms > last_ms_ ? now_ms - last_ms_ : 0;
  last_ms_ = now_ms;
  owed_ms_ += std::min(elapsed, kLongestCatchUpMs);
  const auto tick_ms = static_cast<std::uint32_t>(kEffectTickMs);
  const std::uint32_t ticks =
      std::min(owed_ms_ / tick_ms, static_cast<std::uint32_t>(kEffectNodeMaxCatchUpTicks));
  owed_ms_ %= tick_ms;

  const glm::vec3 player = frame_.ToNative(player_world);
  for (std::uint32_t tick = 0; tick < ticks; ++tick) {
    for (MapEmitter& emitter : emitters_) {
      emitter.Tick(player, draws_, reach_units_);
    }
    for (MapWind& wind : winds_) {
      wind.Tick(player, reach_units_);
    }
    for (MapChimney& chimney : chimneys_) {
      chimney.Tick(player, draws_, reach_units_);
    }
    for (MapEffectMaker& maker : makers_) {
      if (maker.Tick(player, reach_units_)) {
        sown_.push_back({maker.effect_id(), frame_.ToWorld(maker.native_position()),
                         maker.param()[0], maker.param()[1]});
      }
    }
  }
}

std::span<const MapEffectLot> MapEffects::Lots(const Camera& camera,
                                               const ChimneySmokeLook* smoke) {
  std::size_t count = 0;
  // Chaque lot reprend la place d'un lot d'une image précédente : ses quads
  // gardent leur mémoire, et tout le reste repart de ses défauts — une image
  // de cheminée ne doit pas survivre sur l'émetteur qui prend sa place.
  const auto next_lot = [this, &count]() -> MapEffectLot& {
    if (count == lots_.size()) {
      lots_.emplace_back();
    }
    MapEffectLot& lot = lots_[count++];
    std::vector<EffectWorldQuad> quads = std::move(lot.quads);
    quads.clear();
    lot = MapEffectLot{};
    lot.quads = std::move(quads);
    return lot;
  };
  const BillboardBasis basis = MakeBillboardBasis(camera);
  for (const MapEmitter& emitter : emitters_) {
    if (!emitter.drawn()) {
      continue;
    }
    MapEffectLot& lot = next_lot();
    lot.effect_id = kMapEmitterEffect;
    lot.texture = &emitter.setting().texture;
    lot.depth_tested = emitter.setting().depth_tested;
    lot.ground_position = frame_.ToWorld(emitter.native_position());
    emitter.AppendQuads(frame_, basis, lot.quads);
  }
  for (const MapWind& wind : winds_) {
    if (!wind.drawn()) {
      continue;
    }
    MapEffectLot& lot = next_lot();
    lot.effect_id = kMapWindEffect;
    lot.texture = &wind.setting().texture;
    lot.ground_position = frame_.ToWorld(wind.native_position());
    wind.AppendQuads(frame_, lot.quads);
  }
  // Un lot par cheminée : ses bouffées se trient avec les autres effets par la
  // distance de leur cheminée.
  if (smoke != nullptr) {
    for (const MapChimney& chimney : chimneys_) {
      if (!chimney.drawn()) {
        continue;
      }
      MapEffectLot& lot = next_lot();
      lot.effect_id = kMapChimneySmokeEffect;
      lot.texture = &ChimneySmokeTextureKey();
      lot.image = &smoke->image;
      lot.render_word = kMapSmokeRenderWord;
      lot.carried = chimney.owner_id() != 0;
      lot.ground_position = frame_.ToWorld(chimney.native_position());
      chimney.AppendQuads(frame_, basis, *smoke, lot.quads);
    }
  }
  return std::span<const MapEffectLot>(lots_).first(count);
}

std::string MapEffectTexturePath(std::string_view name) {
  return std::string(kTextureFolder) + std::string(name);
}

std::set<std::string> MapEffectTextures(const MapEffectTable& table,
                                        std::span<const formats::RswEffect> objects) {
  std::set<std::string> textures;
  std::int32_t emitter_rank = 0;
  std::int32_t wind_rank = 0;
  for (const formats::RswEffect& object : objects) {
    if (object.number == kMapEmitterEffect) {
      textures.insert(SettingOfRank(table.emitters, emitter_rank).texture);
      ++emitter_rank;
    } else if (object.number == kMapWindEffect) {
      textures.insert(SettingOfRank(table.winds, wind_rank).texture);
      ++wind_rank;
    }
  }
  return textures;
}

}  // namespace ro::render
