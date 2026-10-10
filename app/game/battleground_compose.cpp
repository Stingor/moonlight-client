// La vue de l'inscription au champ de bataille, composée à chaque image. Voir
// `battleground_stage.h`.

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"

#include "game/battleground_rules.h"
#include "game/battleground_stage.h"

namespace moonclient {
namespace {

// Les minimaps des terrains, sous la racine des skins.
constexpr std::string_view kMinimapFolder = "map\\";

constexpr std::uint32_t kMsPerSecond = 1000;

// Les secondes qui restent, arrondies VERS LE HAUT : le compte à rebours ne
// montre zéro qu'une fois l'invitation vraiment expirée.
[[nodiscard]] int SecondsLeft(std::uint32_t ms_left) {
  return static_cast<int>((ms_left + kMsPerSecond - 1) / kMsPerSecond);
}

constexpr std::size_t kRewardCapacity = 160;

}  // namespace

ro::ui::EntryQueueRewardView BattlegroundStage::RewardView(const BattlegroundReward& reward,
                                                           std::string_view language,
                                                           const BattlegroundContext& context) {
  if (reward.item_id == 0) {
    return ro::ui::EntryQueueRewardView{.text = reward.text.In(language)};
  }
  BattlegroundItemFace face =
      context.item_at ? context.item_at(reward.item_id) : BattlegroundItemFace{};
  if (face.name.empty()) {
    if (context.item_at && unknown_items_told_.insert(reward.item_id).second) {
      ro::core::LogWarning(
          "champ de bataille : {} : l'objet de récompense {} n'est pas dans la base",
          kBattlegroundFilePath, reward.item_id);
    }
    face.name = "#" + std::to_string(reward.item_id);
  }
  // Un texte écrit à côté de l'objet l'emporte sur son nom de la base.
  std::string shown = reward.text.empty() ? face.name : std::string(reward.text.In(language));
  if (reward.count > 1) {
    std::array<char, kRewardCapacity> counted{};
    std::snprintf(counted.data(), counted.size(), ro::core::Tr("%d × %s"), reward.count,
                  shown.c_str());
    shown = counted.data();
  }
  reward_texts_.push_back(std::move(shown));
  return ro::ui::EntryQueueRewardView{
      .icon = face.icon, .text = reward_texts_.back(), .item_id = reward.item_id};
}

void BattlegroundStage::ComposeView(BattlegroundWindows& windows,
                                    const BattlegroundContext& context) {
  const std::span<const BattlegroundField> fields =
      table_.has_value() ? table_->fields() : std::span<const BattlegroundField>{};
  const std::string language = ro::core::CurrentLanguage();
  // ⚠ LE DÉTAIL SEULEMENT LISTE OUVERTE : chaque minimap et chaque icône de
  // récompense est une texture, que rien ne justifie de charger pour une
  // fenêtre qu'on ne voit pas.
  const bool detail_wanted = windows.window.open;

  // 🔴 LES TEXTES D'ABORD, LES VUES ENSUITE, et chaque réserve faite avant :
  // une vue pointe dans `field_texts_`, `reward_texts_` et `reward_views_`, qui
  // ne doivent plus bouger une fois pointés.
  field_texts_.clear();
  field_texts_.reserve(fields.size());
  std::size_t reward_count = 0;
  for (const BattlegroundField& field : fields) {
    field_texts_.push_back(FieldTexts{.condition = ConditionText(field), .team = TeamText(field)});
    reward_count += field.reward_win.size() + field.reward_draw.size() + field.reward_lose.size();
  }
  reward_texts_.clear();
  reward_views_.clear();
  if (detail_wanted) {
    reward_texts_.reserve(reward_count);
    reward_views_.reserve(reward_count);
  }
  const auto rewards_of = [&](const std::vector<BattlegroundReward>& rewards) {
    const std::size_t first = reward_views_.size();
    if (detail_wanted) {
      for (const BattlegroundReward& reward : rewards) {
        reward_views_.push_back(RewardView(reward, language, context));
      }
    }
    return std::span<const ro::ui::EntryQueueRewardView>(reward_views_.data() + first,
                                                         reward_views_.size() - first);
  };

  field_views_.clear();
  field_views_.reserve(fields.size());
  for (std::size_t rank = 0; rank < fields.size(); ++rank) {
    const BattlegroundField& field = fields[rank];
    ro::ui::SkinImage minimap;
    if (detail_wanted && context.image_at && !field.minimap_file.empty()) {
      minimap = context.image_at(std::string(kMinimapFolder) + field.minimap_file);
    }
    const std::string_view name = field.name.In(language);
    field_views_.push_back(ro::ui::EntryQueueFieldView{
        .display_name = name.empty() ? std::string_view(field.server_name) : name,
        .victory = field.objective.In(language),
        .help = field.help.In(language),
        .reward_win = rewards_of(field.reward_win),
        .reward_draw = rewards_of(field.reward_draw),
        .reward_lose = rewards_of(field.reward_lose),
        .condition_text = field_texts_[rank].condition,
        .team_text = field_texts_[rank].team,
        .private_apply = field.solo_apply,
        .party_apply = field.party_apply,
        .guild_apply = field.guild_apply,
        .minimap = minimap});
  }

  const ro::sim::EntryQueuePhase phase = queue_.phase();
  const bool queued = queue_.queued();
  const bool applying = phase == ro::sim::EntryQueuePhase::kApplying;
  const bool invited = phase == ro::sim::EntryQueuePhase::kInvited;
  queued_name_ =
      queued || applying ? std::string(DisplayNameOf(queue_.field_name())) : std::string();
  invited_name_ = invited ? queued_name_ : std::string();

  view_ = ro::ui::EntryQueueView{
      .fields = field_views_,
      .queued = queued,
      .applying = applying,
      .invited = invited,
      .accepted = phase == ro::sim::EntryQueuePhase::kAccepted,
      .queued_name = queued_name_,
      .count = queue_.count(),
      .waited_s = queue_.waited_ms(context.now_ms) / kMsPerSecond,
      .invited_name = invited_name_,
      .invitation_s_left = SecondsLeft(queue_.invitation_ms_left(context.now_ms)),
      .notice = notice_shown_,
      .link_ready = context.link_ready};
  windows.view = &view_;
  windows.queued = queued;
  windows.queued_name = queued ? queued_name_ : std::string();
}

}  // namespace moonclient
