// L'onglet « Effets » de l'éditeur de style, côté jeu. Voir l'en-tête.

#include "character/hat_effect_style.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/net/player_style_layers.h"
#include "ro/render/effect_color_tally.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_table.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world.h"

#include "effects/effect_bearer_change.h"

namespace moonclient {
namespace {

constexpr std::string_view kHatEffectPrefix = "HAT_EF_";
// Le micro de la voix, tel que la table le nomme : un signal, pas une parure.
constexpr std::string_view kVoiceMicName = "HAT_EF_MIC";
// Les emplacements d'équipement, un bit chacun.
constexpr std::uint32_t kEquipBits = 32;
// Un envoi sans écho du serveur au bout de ce délai n'est plus « en attente ».
constexpr std::uint32_t kSendPatienceMs = 5000;
// La cadence du serveur entre deux envois de calques.
constexpr std::uint32_t kSendCadenceMs = 1000;
// LA SONDE : ce qu'elle ajoute à la clarté de la teinture, invisible à l'œil et
// distinct de tout réglage, qui va par pour-cent.
constexpr float kProbeLightnessNudge = 0.001F;
// Les images peintes avant de retenir la mesure — une animation entière, ou
// presque — et celles au bout desquelles on renonce à un effet qui ne peint
// rien sous la sonde.
constexpr std::uint32_t kProbeImagesWanted = 90;
constexpr std::uint32_t kProbeNotesGiveUp = 600;

}  // namespace

ro::sim::StyleLayerRecipe HatEffectRecipeOf(std::uint16_t number,
                                            const ro::ui::PaletteEffectSetting& setting) {
  ro::sim::StyleLayerRecipe recipe;
  recipe.layer = ro::sim::StyleLayerKind::kHatEffect;
  recipe.key = number;
  recipe.profile = ro::net::kStyleLayerEffectProfile;
  recipe.adjusts[0].hue = static_cast<std::int16_t>(setting.hue);
  recipe.adjusts[0].saturation = static_cast<std::int8_t>(setting.saturation);
  recipe.adjusts[0].value = static_cast<std::int8_t>(setting.lightness);
  recipe.adjusts[0].absolute = setting.darken;
  return recipe;
}

ro::ui::PaletteEffectSetting HatEffectSettingOf(const ro::sim::StyleLayerRecipe& recipe) {
  const ro::sim::StyleAdjust& adjust = recipe.adjusts[0];
  return ro::ui::PaletteEffectSetting{.hue = adjust.hue,
                                      .saturation = adjust.saturation,
                                      .lightness = adjust.value,
                                      .darken = adjust.absolute};
}

ro::render::EffectDye HatEffectDyeOf(const ro::ui::PaletteEffectSetting& setting) {
  return ro::render::MakeEffectDye(setting.hue, setting.saturation, setting.lightness,
                                   setting.darken);
}

std::string HatEffectDisplayName(std::string_view constant) {
  std::string_view bare = constant;
  if (ro::core::AsciiLower(std::string(bare.substr(0, kHatEffectPrefix.size()))) ==
      ro::core::AsciiLower(std::string(kHatEffectPrefix))) {
    bare.remove_prefix(kHatEffectPrefix.size());
  }
  std::string shown(bare);
  std::ranges::replace(shown, '_', ' ');
  return shown;
}

const HatEffectStyle::Described& HatEffectStyle::Describe(const ro::render::HatEffect& record) {
  if (const auto known = described_.find(record.hat_effect_id); known != described_.end()) {
    return known->second;
  }
  // UN EFFET SANS FICHIER EST ADDITIF, et sa couleur vit dans le code : les
  // nœuds et les classes qu'un chapeau sème le sont presque tous.
  Described described;
  std::string file = record.file;
  if (file.empty() && record.effect_id != 0 && table_.has_value()) {
    const ro::render::HatEffect* shown = table_->FindByConcreteId(record.effect_id);
    file = shown != nullptr ? shown->file : ro::render::EffectFile(record.effect_id);
  }
  if (!catalog_.has_value() && vfs_ != nullptr) {
    catalog_.emplace(*vfs_);
  }
  const ro::render::LoadedEffect* loaded =
      file.empty() || !catalog_.has_value()
          ? nullptr
          : catalog_->Load(std::string(ro::render::kEffectFolder) + file);
  if (loaded != nullptr) {
    std::size_t adds = 0;
    std::size_t keys = 0;
    ro::render::EffectColorTally tally;
    for (const ro::render::PreparedEffectLayer& layer : loaded->prepared.layers) {
      if (layer.keys.empty()) {
        continue;
      }
      for (const ro::render::PreparedEffectKey& key : layer.keys) {
        if (key.key.type != 0) {
          continue;
        }
        ++keys;
        adds +=
            ro::render::EffectBlendAdds(key.key.blend_source, key.key.blend_destination) ? 1U : 0U;
      }
      const std::array<float, 4>& tint = layer.keys.front().key.color;
      for (std::size_t rank = layer.first_texture;
           rank < layer.first_texture + layer.texture_count && rank < loaded->textures.size();
           ++rank) {
        tally.Add(loaded->textures[rank], tint);
      }
    }
    described.additive = keys == 0 || adds * 2 >= keys;
    described.base_color = tally.Color();
  }
  return described_.emplace(record.hat_effect_id, described).first->second;
}

void HatEffectStyle::Refresh(const ro::sim::World& world, ro::ui::PaletteEffectsState& state,
                             bool connected, bool served, bool atelier_open,
                             const ro::vfs::ItemTable* names, std::uint32_t now_ms) {
  if (!table_read_ && vfs_ != nullptr) {
    table_read_ = true;
    if (auto table = ro::render::HatEffectTable::Load(*vfs_)) {
      table_ = std::move(*table);
    }
  }
  rows_.clear();
  posing_items_.clear();
  previews_.clear();
  doll_dyes_.clear();
  for (const auto& [number, setting] : sent_drafts_) {
    state.drafts[number] = setting;
  }
  sent_drafts_.clear();
  const ro::sim::Actor* player = world.player();
  const std::vector<ro::sim::StyleLayerRecipe>* told =
      player != nullptr ? world.styles().FindLayers(player->id) : nullptr;
  if (player != nullptr) {
    for (const ro::sim::HatEffectId number : player->hat_effects) {
      const ro::render::HatEffect* record = table_.has_value() ? table_->Find(number) : nullptr;
      ro::ui::PaletteEffectRow row;
      row.number = number;
      row.name = record != nullptr ? HatEffectDisplayName(record->name)
                                   : "#" + std::to_string(static_cast<unsigned>(number));
      if (record != nullptr &&
          ro::core::AsciiLower(record->name) == ro::core::AsciiLower(std::string(kVoiceMicName))) {
        row.blocked = ro::ui::PaletteEffectBlock::kVoice;
      } else if (record != nullptr && record->file.empty() &&
                 ChangesOnlyItsBearer(record->effect_id)) {
        row.blocked = ro::ui::PaletteEffectBlock::kChangesBody;
      }
      if (record != nullptr) {
        const Described& described = Describe(*record);
        row.additive = described.additive;
        row.base_color = described.base_color;
      }
      if (const auto measured = measured_.find(number);
          !row.base_color.has_value() && measured != measured_.end()) {
        row.base_color = measured->second;
      }
      // L'OBJET PORTÉ QUI LE POSE, pour le nommer ; le serveur le retrouve
      // lui-même à la validation.
      if (items_ != nullptr) {
        for (std::uint32_t bit = 0; bit < kEquipBits; ++bit) {
          const ro::sim::InventoryItem* worn = world.inventory().WornAt(1U << bit);
          if (worn == nullptr ||
              items_->OrdinalOf(worn->item_id) != static_cast<std::int16_t>(number)) {
            continue;
          }
          posing_items_.push_back(worn->item_id);
          const ro::vfs::ItemEntry* entry = names != nullptr ? names->Find(worn->item_id) : nullptr;
          if (entry != nullptr && row.item_name.empty()) {
            row.item_name = entry->name;
          }
        }
      }
      if (told != nullptr) {
        for (const ro::sim::StyleLayerRecipe& recipe : *told) {
          if (recipe.layer == ro::sim::StyleLayerKind::kHatEffect && recipe.key == number &&
              recipe.profile == ro::net::kStyleLayerEffectProfile) {
            row.worn = HatEffectSettingOf(recipe);
          }
        }
      }
      // L'ÉCHO DU SERVEUR : ce qu'on a validé est désormais porté.
      if (const auto draft = state.drafts.find(number);
          draft != state.drafts.end() && draft->second == row.worn) {
        if (sent_number_ == number) {
          send_ = ro::ui::PaletteEffectSend::kShared;
        }
        state.drafts.erase(draft);
      }
      // L'APERÇU, sur le personnage : seulement ce qui diffère de ce qu'il porte.
      const ro::ui::PaletteEffectSetting shown = ro::ui::EffectDraftOf(state, row);
      if (!shown.neutral() && row.blocked == ro::ui::PaletteEffectBlock::kNone) {
        doll_dyes_.emplace(number, HatEffectDyeOf(shown));
      }
      if (shown != row.worn && row.blocked == ro::ui::PaletteEffectBlock::kNone) {
        previews_.push_back(WornHatEffectDye{
            .bearer_id = player->id,
            .hat_effect_id = number,
            .dye = shown.neutral() ? std::nullopt : std::optional(HatEffectDyeOf(shown))});
      }
      rows_.push_back(std::move(row));
    }
  }
  // L'EFFET CHOISI SANS COULEUR CONNUE se mesure : il porte la sonde sur le
  // pantin. Une sonde qui change remet la mesure à zéro.
  const ro::ui::PaletteEffectsView chosen_among{.rows = rows_};
  const ro::ui::PaletteEffectRow* chosen = ro::ui::SelectedEffect(state, chosen_among);
  std::optional<Probe> probe;
  if (chosen != nullptr && !chosen->base_color.has_value() && !measured_.contains(chosen->number)) {
    const auto dyed = doll_dyes_.find(chosen->number);
    ro::render::EffectDye dye = dyed != doll_dyes_.end() ? dyed->second : ro::render::EffectDye{};
    dye.lightness_scale += kProbeLightnessNudge;
    doll_dyes_[chosen->number] = dye;
    probe = Probe{.number = chosen->number, .dye = dye};
    const std::uint16_t measured_number = chosen->number;
    for (ro::ui::PaletteEffectRow& row : rows_) {
      row.measuring_color = row.number == measured_number;
    }
  }
  if (!probe.has_value() || !probe_.has_value() || probe_->number != probe->number) {
    probe_notes_ = 0;
  }
  probe_ = probe;
  if (send_ == ro::ui::PaletteEffectSend::kSent && now_ms - sent_ms_ >= kSendPatienceMs) {
    send_ = ro::ui::PaletteEffectSend::kNone;
  }
  drafts_ = state.drafts;
  view_.rows = rows_;
  view_.connected = connected;
  view_.served = served;
  view_.atelier_open = atelier_open;
  view_.can_send = sent_ms_ == 0 || now_ms - sent_ms_ >= kSendCadenceMs;
  view_.send = send_;
}

void HatEffectStyle::NoteColorTally(const ro::render::EffectColorTally& tally) {
  if (!probe_.has_value() || !(tally.probe == probe_->dye)) {
    return;
  }
  ++probe_notes_;
  const std::optional<std::array<std::uint8_t, 3>> color = tally.Color();
  if ((tally.images_counted() >= kProbeImagesWanted && color.has_value()) ||
      probe_notes_ >= kProbeNotesGiveUp) {
    measured_[probe_->number] = color;
    probe_.reset();
  }
}

bool HatEffectStyle::PosesEffect(std::uint32_t item_id) const {
  return std::ranges::find(posing_items_, item_id) != posing_items_.end();
}

void HatEffectStyle::Apply(const ro::ui::PaletteEffectsAction& asked, ro::net::Connection& link,
                           std::uint32_t now_ms) {
  std::optional<std::uint16_t> number = asked.validate.has_value() ? asked.validate : asked.clear;
  if (!number.has_value()) {
    return;
  }
  const auto row = std::ranges::find(rows_, *number, &ro::ui::PaletteEffectRow::number);
  if (row == rows_.end()) {
    return;
  }
  const ro::ui::PaletteEffectsState shown{.drafts = drafts_};
  ro::ui::PaletteEffectSetting setting =
      asked.clear.has_value() ? ro::ui::PaletteEffectSetting{} : ro::ui::EffectDraftOf(shown, *row);
  setting = ro::ui::ClampedEffectSetting(setting, row->additive);
  // UN RÉGLAGE NEUTRE RETIRE LA TEINTURE : le serveur n'a rien à ranger.
  const ro::net::StyleLayerOrder order{.recipe = HatEffectRecipeOf(*number, setting),
                                       .clear = setting.neutral()};
  if (ro::net::SendStyleLayers(link, std::span(&order, 1), /*clear_character=*/false)) {
    sent_drafts_[*number] = setting;
    sent_number_ = *number;
    sent_ms_ = now_ms;
    send_ = ro::ui::PaletteEffectSend::kSent;
  }
}

}  // namespace moonclient
