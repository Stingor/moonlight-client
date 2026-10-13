#include "ro/render/map_effects.h"

#include <algorithm>
#include <map>
#include <utility>

#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_step_blend.h"
#include "ro/render/sprite_billboard.h"

namespace ro::render {
namespace {

// Le dossier sous lequel le client cherche ces textures.
constexpr std::string_view kTextureFolder = "data\\texture\\";

// Au-delà de ce retard, le reste est abandonné : un chargement de carte ne doit
// pas faire jouer d'un coup des secondes de vie.
constexpr std::uint32_t kLongestCatchUpMs =
    static_cast<std::uint32_t>(kEffectNodeMaxCatchUpTicks * kEffectTickMs);

// LA CLÉ D'UN LOT : la sorte de son objet dans les deux bits forts, son rang —
// ou le numéro d'une cheminée — dessous. Les lots sortent par sorte puis par
// rang croissants, donc par clé croissante.
enum LotKind : std::uint64_t { kEmitterLots = 0, kWindLots = 1, kChimneyLots = 2 };
constexpr unsigned kLotKindShift = 62;

[[nodiscard]] std::uint64_t LotKey(LotKind kind, std::uint64_t rank) {
  return (static_cast<std::uint64_t>(kind) << kLotKindShift) | rank;
}

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
  current_ = LotSet{};
  previous_lots_ = LotSet{};
  previous_emitters_.clear();
  previous_winds_.clear();
  previous_chimneys_.clear();
  has_previous_ = false;
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
      chimneys_.back().Identify(++last_chimney_serial_);
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
  chimneys_.back().Identify(++last_chimney_serial_);
}

void MapEffects::SetSmoothSteps(bool on) {
  smooth_ = on;
  if (!on && has_previous_) {
    previous_emitters_.clear();
    previous_winds_.clear();
    previous_chimneys_.clear();
    previous_lots_ = LotSet{};
    has_previous_ = false;
  }
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
    // LA COPIE SE PREND AVANT LE DERNIER PAS : voir `SetSmoothSteps`.
    if (smooth_ && tick + 1 == ticks) {
      previous_emitters_ = emitters_;
      previous_winds_ = winds_;
      previous_chimneys_ = chimneys_;
      has_previous_ = true;
    }
    for (MapEmitter& emitter : emitters_) {
      emitter.Tick(player, draws_);
    }
    for (MapWind& wind : winds_) {
      wind.Tick(player);
    }
    for (MapChimney& chimney : chimneys_) {
      chimney.Tick(player, draws_);
    }
    for (MapEffectMaker& maker : makers_) {
      if (maker.Tick(player)) {
        sown_.push_back({maker.effect_id(), frame_.ToWorld(maker.native_position()),
                         maker.param()[0], maker.param()[1]});
      }
    }
  }
}

std::span<const MapEffectLot> MapEffects::Lots(const Camera& camera,
                                               const ChimneySmokeLook* smoke) {
  const BillboardBasis basis = MakeBillboardBasis(camera);
  ComposeLots(emitters_, winds_, chimneys_, basis, smoke, current_);
  if (smooth_ && has_previous_) {
    SmoothLots(basis, smoke);
  }
  return std::span<const MapEffectLot>(current_.lots).first(current_.count);
}

void MapEffects::ComposeLots(std::span<const MapEmitter> emitters, std::span<const MapWind> winds,
                             std::span<const MapChimney> chimneys, const BillboardBasis& basis,
                             const ChimneySmokeLook* smoke, LotSet& into) const {
  into.count = 0;
  // Chaque lot reprend la place d'un lot d'une image précédente : ses quads
  // gardent leur mémoire, et tout le reste repart de ses défauts — une image
  // de cheminée ne doit pas survivre sur l'émetteur qui prend sa place.
  // LISSÉ, chaque lot porte aussi la clé de son objet et l'étiquette de ses quads.
  const auto next_lot = [&into](std::uint64_t key) -> MapEffectLot& {
    if (into.count == into.lots.size()) {
      into.lots.emplace_back();
      into.keys.emplace_back();
      into.tags.emplace_back();
    }
    into.keys[into.count] = key;
    into.tags[into.count].clear();
    MapEffectLot& lot = into.lots[into.count++];
    std::vector<EffectWorldQuad> quads = std::move(lot.quads);
    quads.clear();
    lot = MapEffectLot{};
    lot.quads = std::move(quads);
    return lot;
  };
  const auto tags_of_last = [this, &into]() -> std::vector<std::uint64_t>* {
    return smooth_ ? &into.tags[into.count - 1] : nullptr;
  };
  for (std::size_t rank = 0; rank < emitters.size(); ++rank) {
    const MapEmitter& emitter = emitters[rank];
    if (!emitter.drawn()) {
      continue;
    }
    MapEffectLot& lot = next_lot(LotKey(kEmitterLots, rank));
    lot.effect_id = kMapEmitterEffect;
    lot.texture = &emitter.setting().texture;
    lot.depth_tested = emitter.setting().depth_tested;
    lot.ground_position = frame_.ToWorld(emitter.native_position());
    emitter.AppendQuads(frame_, basis, lot.quads, tags_of_last());
  }
  for (std::size_t rank = 0; rank < winds.size(); ++rank) {
    const MapWind& wind = winds[rank];
    if (!wind.drawn()) {
      continue;
    }
    MapEffectLot& lot = next_lot(LotKey(kWindLots, rank));
    lot.effect_id = kMapWindEffect;
    lot.texture = &wind.setting().texture;
    lot.ground_position = frame_.ToWorld(wind.native_position());
    wind.AppendQuads(frame_, lot.quads, tags_of_last());
  }
  // Un lot par cheminée : ses bouffées se trient avec les autres effets par la
  // distance de leur cheminée.
  if (smoke != nullptr) {
    for (const MapChimney& chimney : chimneys) {
      if (!chimney.drawn()) {
        continue;
      }
      MapEffectLot& lot = next_lot(LotKey(kChimneyLots, chimney.serial()));
      lot.effect_id = kMapChimneySmokeEffect;
      lot.texture = &ChimneySmokeTextureKey();
      lot.image = &smoke->image;
      lot.render_word = kMapSmokeRenderWord;
      lot.carried = chimney.owner_id() != 0;
      lot.ground_position = frame_.ToWorld(chimney.native_position());
      chimney.AppendQuads(frame_, basis, *smoke, lot.quads, tags_of_last());
    }
  }
}

void MapEffects::SmoothLots(const BillboardBasis& basis, const ChimneySmokeLook* smoke) {
  ComposeLots(previous_emitters_, previous_winds_, previous_chimneys_, basis, smoke,
              previous_lots_);
  const float alpha = EffectStepAlpha(owed_ms_);
  const auto previous_keys =
      std::span<const std::uint64_t>(previous_lots_.keys).first(previous_lots_.count);
  for (std::size_t index = 0; index < current_.count; ++index) {
    const auto found = std::ranges::lower_bound(previous_keys, current_.keys[index]);
    if (found == previous_keys.end() || *found != current_.keys[index]) {
      continue;
    }
    const auto before = static_cast<std::size_t>(found - previous_keys.begin());
    MapEffectLot& lot = current_.lots[index];
    const MapEffectLot& old_lot = previous_lots_.lots[before];
    // LE NOM, ET NON SON ADRESSE : celui de la copie vit dans la copie.
    if (lot.texture == nullptr || old_lot.texture == nullptr || *lot.texture != *old_lot.texture) {
      continue;
    }
    // UNE CHEMINÉE PORTÉE a suivi son porteur entre les deux : sa copie aussi.
    static_cast<void>(BlendTaggedEffectWorldQuads(old_lot.quads, previous_lots_.tags[before],
                                                  lot.ground_position - old_lot.ground_position,
                                                  alpha, lot.quads, current_.tags[index]));
  }
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
