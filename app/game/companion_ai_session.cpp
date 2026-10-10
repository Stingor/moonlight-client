// Voir l'en-tête.

#include "game/companion_ai_session.h"

#include <algorithm>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "ro/core/log.h"

namespace moonclient {
namespace {

// Ce qui reste des états de règle mis de côté à `at_ms`, vu de `now_ms`.
[[nodiscard]] std::vector<ro::sim::CompanionRuleMemory> AgedRules(
    std::vector<ro::sim::CompanionRuleMemory> rules, std::uint32_t at_ms, std::uint32_t now_ms) {
  return ro::sim::AgedMemory(ro::sim::CompanionMemory{.rules = std::move(rules)}, now_ms - at_ms)
      .rules;
}

}  // namespace

bool FollowCompanion(FollowedCompanion& followed, ro::sim::CompanionBrain& brain,
                     std::uint32_t actor_id, std::uint16_t job, std::uint32_t arrived_ms) {
  const bool other_actor = actor_id != followed.actor_id;
  const bool other_class = job != 0 && followed.job.has_value() && *followed.job != job;
  followed.actor_id = actor_id;
  if (job != 0) {
    followed.job = job;
  }
  if (!other_actor && !other_class) {
    return false;
  }
  brain = ro::sim::CompanionBrain();
  brain.ForgetMap(false, arrived_ms);
  return true;
}

void CompanionAiSession::Open(const std::filesystem::path& client_root,
                              std::uint32_t character_id) {
  client_root_ = client_root;
  opened_ = true;
  edits_ = CompanionProfileEdits();
  profiles_ = LoadCompanionAi(client_root_).profiles;
  character_ = CharacterKey{.cid = character_id};
  context_watch_ = ro::sim::CompanionContextWatch();
  stashed_.clear();
  state_loaded_ = false;
  loaded_state_.clear();
  state_saved_ms_.reset();
  state_written_.clear();
  state_written_ms_ = 0;
  state_warned_ = false;
}

void CompanionAiSession::NoteCharacterName(std::string_view name) {
  if (!name.empty()) {
    character_.name = std::string(name);
  }
}

void CompanionAiSession::ObserveMap(std::uint32_t map_entries, const ro::sim::MapMode& mode,
                                    std::uint32_t arrived_ms, std::uint32_t now_ms) {
  static_cast<void>(context_watch_.Observe(map_entries, mode, arrived_ms, now_ms));
}

ro::sim::CompanionAiSettings CompanionAiSession::SettingsFor(ro::sim::AiCompanion companion,
                                                             std::uint16_t job,
                                                             std::string_view profile) const {
  return SettingsOfProfile(companion, job, profiles_.Find(job, profile));
}

bool CompanionAiSession::SaveProfiles() {
  if (edits_.empty() || !opened_) {
    return false;
  }
  std::optional<CompanionAiFileRead> written = SaveCompanionAiEdits(client_root_, edits_);
  if (!written.has_value()) {
    // Gardées : le prochain enregistrement les retente.
    return false;
  }
  edits_ = CompanionProfileEdits();
  // LE FICHIER ÉCRIT FAIT FOI : il porte aussi ce qu'un autre client vient d'y
  // enregistrer.
  profiles_ = std::move(written->profiles);
  return true;
}

void CompanionAiSession::Refresh(const SeatedCompanion& seated) const {
  if (seated.seat->applied.has_value()) {
    *seated.settings =
        SettingsFor(seated.companion, seated.seat->applied->first, seated.seat->applied->second);
  }
}

bool CompanionAiSession::Exists(std::uint16_t job, std::string_view name) const {
  return name == kBasicProfileName || profiles_.Find(job, name) != nullptr;
}

void CompanionAiSession::Resolve(const SeatedCompanion& seated, std::uint16_t job,
                                 std::uint32_t now_ms, std::int64_t wall_now_ms) {
  if (!opened_ || !character_known() || job == 0) {
    return;
  }
  CompanionProfileBook book(profiles_, edits_);
  // LA FICHE ne s'écrit qu'en changeant : à l'évolution, à la première
  // apparition de ce personnage, quand son nom change, ou quand une
  // proposition se pose.
  const auto snapshot = [this]() {
    const CompanionCharacter* character = profiles_.FindCharacter(character_);
    return character != nullptr
               ? std::optional(
                     std::tuple(character->previous_form, character->key.name, character->proposed))
               : std::nullopt;
  };
  const auto before = snapshot();
  seated.seat->proposal = book.NoteForm(character_, job);
  if (snapshot() != before && SaveProfiles()) {
    Refresh(seated);
  }
  const std::string wanted = book.Active(character_, job, context());
  const CompanionSeat& seat = *seated.seat;
  if (!seat.applied.has_value() || seat.applied->first != job || seat.applied->second != wanted) {
    Apply(seated, job, wanted, now_ms, wall_now_ms);
  }
}

void CompanionAiSession::LoadState() {
  if (state_loaded_ || !character_known() || client_root_.empty()) {
    return;
  }
  state_loaded_ = true;
  for (const StoredCharacterState& character : LoadCompanionState(client_root_).characters) {
    if (SameCharacter(character.key, character_)) {
      for (const StoredCompanionState& state : character.companions) {
        loaded_state_.emplace(state.job, state);
      }
    }
  }
}

void CompanionAiSession::Apply(const SeatedCompanion& seated, std::uint16_t job,
                               const std::string& name, std::uint32_t now_ms,
                               std::int64_t wall_now_ms) {
  LoadState();
  CompanionSeat& seat = *seated.seat;
  ro::sim::CompanionBrain& brain = *seated.brain;
  // LE PROFIL QUITTÉ GARDE SES ÉTATS DE RÈGLE, s'il existe encore.
  if (seat.applied.has_value() && Exists(seat.applied->first, seat.applied->second)) {
    stashed_[*seat.applied] = StashedRules{.rules = brain.Remember(now_ms).rules, .at_ms = now_ms};
  }
  *seated.settings = SettingsFor(seated.companion, job, name);
  // 🔴 L'ATTENTE NE BOUGE PAS : « garder l'attente » ne parle que des portails
  // et des reconnexions.
  ro::sim::CompanionMemory next{.standby = brain.standby()};
  // À LA PREMIÈRE APPLICATION DE SA CLASSE, LE FICHIER DES DÉLAIS PARLE : le
  // temps passé hors du jeu a vieilli chaque mémoire, et l'attente ne revient
  // que si le profil la garde.
  if (const auto stored = loaded_state_.find(job); stored != loaded_state_.end()) {
    for (const auto& [profile, rules] : stored->second.rules_by_profile) {
      stashed_[{job, profile}] = StashedRules{
          .rules = MemoryOf(stored->second, profile, wall_now_ms).rules, .at_ms = now_ms};
    }
    const ro::sim::CompanionMemory from_file = MemoryOf(stored->second, "", wall_now_ms);
    next.skills = from_file.skills;
    next.standby = next.standby || (from_file.standby && seated.settings->standby_kept_across_maps);
    loaded_state_.erase(stored);
  }
  if (const auto stash = stashed_.find({job, name}); stash != stashed_.end()) {
    next.rules = AgedRules(stash->second.rules, stash->second.at_ms, now_ms);
    stashed_.erase(stash);
  }
  // 🔴 RIEN NE REDÉMARRE : la cible, l'ordre direct et la file restent ; seuls
  // les états de règle suivent le profil.
  brain.Recall(next, now_ms);
  seat.applied = std::pair(job, name);
  ro::core::LogInfo("IA des compagnons : classe {}, profil « {} »", job, name);
}

AiEditTarget CompanionAiSession::EditTarget(const SeatedCompanion& seated) {
  CompanionSeat& seat = *seated.seat;
  if (!seat.applied.has_value() || !character_known()) {
    return AiEditTarget{.companion = seated.companion};
  }
  const std::uint16_t job = seat.applied->first;
  if (seat.applied->second == kBasicProfileName) {
    CompanionProfileBook book(profiles_, edits_);
    if (profiles_.Find(job, kOwnProfileName) == nullptr &&
        book.Create(job, kOwnProfileName, kBasicProfileName) != ProfileRefusal::kNone) {
      return AiEditTarget{.companion = seated.companion};
    }
    // « MON PROFIL » EST LE REPLI D'UN PVE SANS CHOIX, et par lui des autres
    // contextes sans choix ; s'il ne prend pas ce contexte-ci de lui-même, il en
    // devient le choix.
    if (book.Active(character_, job, context()) != kOwnProfileName) {
      static_cast<void>(book.SetActive(character_, job, context(), kOwnProfileName));
    }
    // LA COPIE VAUT L'IA BASIQUE : le profil appliqué change de nom, rien ne
    // redémarre.
    seat.applied->second = std::string(kOwnProfileName);
    Refresh(seated);
  }
  const std::string& name = seat.applied->second;
  return AiEditTarget{.companion = seated.companion,
                      .job = job,
                      .profile = profiles_.Find(job, name),
                      .profile_name = name,
                      .edits = &edits_};
}

AiEditTarget CompanionAiSession::NamedTarget(const SeatedCompanion& seated,
                                             std::string_view profile) {
  const CompanionSeat& seat = *seated.seat;
  if (!seat.applied.has_value() || profile == kBasicProfileName) {
    return AiEditTarget{.companion = seated.companion};
  }
  const std::uint16_t job = seat.applied->first;
  CompanionProfile* found = profiles_.Find(job, profile);
  if (found == nullptr) {
    return AiEditTarget{.companion = seated.companion};
  }
  return AiEditTarget{.companion = seated.companion,
                      .job = job,
                      .profile = found,
                      .profile_name = std::string(profile),
                      .edits = &edits_};
}

void CompanionAiSession::Answer(const SeatedCompanion& seated, bool accept) {
  CompanionSeat& seat = *seated.seat;
  if (!seat.proposal.has_value() || !character_known()) {
    return;
  }
  CompanionProfileBook(profiles_, edits_).Answer(character_, *seat.proposal, accept);
  seat.proposal.reset();
  static_cast<void>(SaveProfiles());
}

ProfileRefusal CompanionAiSession::Rename(std::span<const SeatedCompanion> seated,
                                          std::uint16_t job, std::string_view from,
                                          std::string_view to) {
  CompanionProfileBook book(profiles_, edits_);
  // CE PERSONNAGE A SA FICHE : sans elle, le repli de « Mon profil » renommé ne
  // pourrait pas lui être posé.
  if (from == kOwnProfileName) {
    book.Know(character_);
  }
  const ProfileRefusal refusal = book.Rename(job, from, to);
  if (refusal != ProfileRefusal::kNone) {
    return refusal;
  }
  const std::string kept = TrimmedProfileName(to);
  for (const SeatedCompanion& one : seated) {
    if (one.seat->applied.has_value() && one.seat->applied->first == job &&
        one.seat->applied->second == from) {
      one.seat->applied->second = kept;
    }
  }
  if (const auto stash = stashed_.find({job, std::string(from)}); stash != stashed_.end()) {
    StashedRules moved = std::move(stash->second);
    stashed_.erase(stash);
    stashed_[{job, kept}] = std::move(moved);
  }
  if (const auto stored = loaded_state_.find(job); stored != loaded_state_.end()) {
    auto& rules = stored->second.rules_by_profile;
    if (const auto found = rules.find(std::string(from)); found != rules.end()) {
      auto moved = std::move(found->second);
      rules.erase(found);
      rules[kept] = std::move(moved);
    }
  }
  return ProfileRefusal::kNone;
}

ProfileRefusal CompanionAiSession::Delete(std::uint16_t job, std::string_view name) {
  const ProfileRefusal refusal = CompanionProfileBook(profiles_, edits_).Delete(job, name);
  if (refusal != ProfileRefusal::kNone) {
    return refusal;
  }
  stashed_.erase({job, std::string(name)});
  if (const auto stored = loaded_state_.find(job); stored != loaded_state_.end()) {
    stored->second.rules_by_profile.erase(std::string(name));
  }
  return ProfileRefusal::kNone;
}

void CompanionAiSession::SaveState(std::span<const SeatedCompanion> seated, std::uint32_t now_ms,
                                   std::int64_t wall_now_ms, bool force) {
  if (!opened_ || !character_known()) {
    return;
  }
  if (!force) {
    if (!state_saved_ms_.has_value()) {
      state_saved_ms_ = now_ms;
      return;
    }
    if (now_ms - *state_saved_ms_ < kCompanionStateSaveEveryMs) {
      return;
    }
  }
  state_saved_ms_ = now_ms;
  std::vector<StoredCompanionState> companions;
  const auto state_of = [&companions, wall_now_ms](std::uint16_t job) -> StoredCompanionState& {
    for (StoredCompanionState& state : companions) {
      if (state.job == job) {
        return state;
      }
    }
    return companions.emplace_back(
        StoredCompanionState{.job = job, .written_wall_ms = wall_now_ms});
  };
  for (const SeatedCompanion& one : seated) {
    if (!one.seat->applied.has_value()) {
      continue;
    }
    const ro::sim::CompanionMemory memory = one.brain->Remember(now_ms);
    StoredCompanionState& state = state_of(one.seat->applied->first);
    state.skills = memory.skills;
    state.standby = memory.standby && one.settings->standby_kept_across_maps;
    state.rules_by_profile[one.seat->applied->second] = memory.rules;
  }
  for (const auto& [applied, stash] : stashed_) {
    std::vector<ro::sim::CompanionRuleMemory> rules = AgedRules(stash.rules, stash.at_ms, now_ms);
    if (!rules.empty() && Exists(applied.first, applied.second)) {
      state_of(applied.first).rules_by_profile[applied.second] = std::move(rules);
    }
  }
  if (companions.empty()) {
    return;
  }
  // RIEN DE CHANGÉ depuis la dernière écriture, au temps passé près : rien ne
  // s'écrit.
  if (!state_written_.empty() && ComparableStates(companions, 0) ==
                                     ComparableStates(state_written_, now_ms - state_written_ms_)) {
    return;
  }
  if (SaveCompanionState(client_root_, character_, companions) == StateWrite::kWritten) {
    state_written_ = std::move(companions);
    state_written_ms_ = now_ms;
    return;
  }
  if (!state_warned_) {
    state_warned_ = true;
    ro::core::LogWarning(
        "IA des compagnons : les délais ne sont pas gardés pour la session suivante");
  }
}

}  // namespace moonclient
