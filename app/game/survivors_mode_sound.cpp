#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/log.h"
#include "ro/render/attack_sound.h"
#include "ro/render/skill_sound_table.h"
#include "ro/sim/level_up_effect.h"

#include "game/survivors_mode.h"
#include "survivors/arena_card_cues.h"
#include "survivors/survivors_looks.h"
#include "survivors/survivors_screen_look.h"

// LE SON DU MODE : les bruits de la partie et sa musique.

namespace moonclient {
namespace {

// Le bruit d'une créature qui frappe le héros, et le gain de tous les bruits
// de l'arène, au centre.
constexpr std::string_view kHeroHurtSound = ro::render::kEnemyHitSounds[0];
constexpr float kArenaSoundGain = 1.0F;
constexpr float kArenaSoundPan = 0.0F;

}  // namespace

void SurvivorsMode::PlaySounds(const ro::sim::SurvivorsNews& news,
                               const SurvivorsContext& context) {
  // Les bruits des effets se prennent même sous un son coupé : ils ne valent
  // que pour l'image qui les a fait naître.
  const std::vector<std::string> effect_sounds =
      arena_.has_value() ? arena_->TakeEffectSounds() : std::vector<std::string>{};
  if (!context.audio->sound_audible()) return;
  if (news.hero_hurt > 0) PlaySound(kHeroHurtSound, ArenaSoundRank::kHeroHurt, context);
  if (news.level_ups > 0 || !news.evolutions.empty()) {
    PlaySound(ro::sim::kBaseLevelUpSound, ArenaSoundRank::kRise, context);
  }
  const ArenaCardSounds cards = CardSoundsOf(news);
  PlaySound(cards.drop, ArenaSoundRank::kTreasure, context);
  PlaySound(cards.pickup, ArenaSoundRank::kTreasure, context);
  // Le tirage des variantes change à chaque lancer : deux Fire Bolt de suite
  // ne sonnent pas forcément pareil.
  std::uint32_t draw = context.now_ms;
  for (const ro::sim::SurvivorsCast& cast : news.casts) {
    const ro::sim::SurvivorsSkillRule* rule = ro::sim::FindSkill(Rules(), cast.skill_id);
    if (rule == nullptr) continue;
    for (const std::string& file : SkillSoundFilesOf(SkillLookOf(*rule), draw++)) {
      PlaySound(file, ArenaSoundRank::kSkill, context);
    }
  }
  // LES SORTS SANS BRUIT SONNENT À L'IMPACT, du coup sourd d'une compétence :
  // un par sort et par image, quel que soit le nombre de monstres touchés.
  std::set<std::uint16_t> thudded;
  for (const ro::sim::SurvivorsHit& hit : news.hits) {
    if (hit.skill_id == 0 || !thudded.insert(hit.skill_id).second) continue;
    const ro::sim::SurvivorsSkillRule* rule = ro::sim::FindSkill(Rules(), hit.skill_id);
    if (rule == nullptr || !SkillThudsOnHit(SkillLookOf(*rule))) continue;
    PlaySound(ro::render::SkillBlowSound(draw++), ArenaSoundRank::kSkill, context);
  }
  for (const std::string& file : effect_sounds) PlaySound(file, ArenaSoundRank::kSkill, context);
}

void SurvivorsMode::PlaySound(std::string_view name, ArenaSoundRank rank,
                              const SurvivorsContext& context) {
  if (name.empty() || missing_sounds_.contains(name)) return;
  ro::audio::AudioEngine& audio = *context.audio;
  const std::string held(name);
  if (!audio.KnowsSound(held)) {
    auto bytes = context.vfs->Read(std::string(ro::render::kSoundFolder) + held);
    if (!bytes) {
      ro::core::LogWarning("survivors : son {} absent", held);
      missing_sounds_.insert(held);
      return;
    }
    if (auto loaded = audio.LoadSound(held, *bytes); !loaded) {
      ro::core::LogWarning("survivors : son {} illisible — {}", held,
                           ro::core::Describe(loaded.error()));
      missing_sounds_.insert(held);
      return;
    }
  }
  const ArenaSoundVerdict verdict =
      sounds_.Ask(held, rank, audio.SoundLengthMs(held), context.now_ms);
  if (!verdict.silenced.empty()) audio.StopSound(verdict.silenced);
  if (verdict.play) {
    audio.PlaySound(held, kArenaSoundGain, kArenaSoundPan, ro::audio::SoundChannel::kEffects);
  }
}

void SurvivorsMode::FollowMusic(const SurvivorsStage& stage, const SurvivorsContext& context) {
  ro::audio::AudioEngine& audio = *context.audio;
  // UNE PISTE ANNONCÉE SOUS UNE MUSIQUE COUPÉE NE JOUE PAS : rallumée, la piste
  // du moment repart.
  if (!audio.music_audible()) {
    music_muted_ = true;
    return;
  }
  if (music_muted_) {
    music_muted_ = false;
    music_.Reset();
  }
  const ArenaMusicCue cue = music_.Advance(
      ArenaMusicFacts{.presentation_ms = stage.presentation_ms(),
                      .paused = SurvivorsMusicPaused(stage.screen()),
                      .field_track = arena_.has_value() ? arena_->field_track() : kArenaFieldTrack,
                      .player_track = context.music->chosen(),
                      .music_volume = music_volume_.player_volume()});
  if (!cue.start_track.empty()) context.music->PlayForMode(audio, cue.start_track);
  if (const std::optional<float> posed = music_volume_.Pose(cue.volume)) {
    audio.SetMusicVolume(*posed);
  }
}

}  // namespace moonclient
