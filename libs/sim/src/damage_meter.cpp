#include "ro/sim/damage_meter.h"

#include <algorithm>
#include <array>
#include <utility>

namespace ro::sim {
namespace {

// Les sortes de coup qui ne portent PAS de dégâts, telles que le serveur les
// numérote (`e_damage_type`) : ramasser un objet, s'asseoir, se lever, et
// l'esquive chanceuse.
constexpr std::array<std::uint8_t, 4> kActionsWithoutDamage = {1, 2, 3, 11};

constexpr std::uint32_t kMsPerSecond = 1000;

}  // namespace

bool DamageActionCounts(std::uint8_t action) {
  return std::ranges::find(kActionsWithoutDamage, action) == kActionsWithoutDamage.end();
}

void DamageMeter::Track(std::uint32_t self_id, std::string_view self_name,
                        std::span<const MeterActor> party) {
  self_id_ = self_id;
  self_name_.assign(self_name);
  party_.assign(party.begin(), party.end());
}

bool DamageMeter::tracks(std::uint32_t id) const noexcept {
  if (id == 0) {
    return false;
  }
  return id == self_id_ ||
         std::ranges::any_of(party_, [id](const MeterActor& member) { return member.id == id; });
}

std::string_view DamageMeter::NameOf(std::uint32_t id) const {
  if (id == self_id_) {
    return self_name_;
  }
  const auto found =
      std::ranges::find_if(party_, [id](const MeterActor& member) { return member.id == id; });
  return found != party_.end() ? std::string_view(found->name) : std::string_view();
}

void DamageMeter::Forget() {
  // ⚠ LE COMBAT OUBLIÉ GARDE SA PLACE DANS LA SESSION : ses montants y sont
  // déjà, son temps actif doit les suivre — sinon le débit de la session
  // grimpe d'autant.
  if (in_combat_) {
    session_.active_ms += last_damage_ms_ - fight_since_ms_;
  }
  tracks_ = {};
  last_damage_ms_ = 0;
  fight_since_ms_ = 0;
  in_combat_ = false;
  current_ = MeterSegment();
}

void DamageMeter::ForgetRecords() noexcept {
  for (std::vector<MeterRecord>& kept : records_) {
    kept.clear();
  }
}

void DamageMeter::ForgetSession() {
  Forget();
  previous_ = MeterSegment();
  session_ = MeterSegment();
  ForgetRecords();
}

void DamageMeter::CloseFight() {
  in_combat_ = false;
  current_.active_ms = last_damage_ms_ - fight_since_ms_;
  session_.active_ms += current_.active_ms;
}

void DamageMeter::OpenFight(std::uint32_t now_ms) {
  // LE COMBAT QUI FINIT DEVIENT LE PRÉCÉDENT — s'il a compté quelque chose : un
  // segment vide ne chasse pas le dernier vrai combat.
  if (!current_.empty()) {
    previous_ = std::move(current_);
  }
  current_ = MeterSegment();
  fight_since_ms_ = now_ms;
  ++fight_;
  in_combat_ = true;
}

void DamageMeter::Record(std::int64_t damage, std::uint32_t now_ms) {
  Record(MeterBlow{.mode = MeterMode::kDamageDone, .who = self_id_, .amount = damage}, now_ms);
}

void DamageMeter::Record(const MeterBlow& blow, std::uint32_t now_ms) {
  if (blow.amount <= 0 || !tracks(blow.who)) {
    return;
  }
  // UN SILENCE DE DÉGÂTS ASSEZ LONG OUVRE UN AUTRE COMBAT. C'est ce qui fait
  // qu'un compteur dit le rythme de CE combat-ci et non celui de la soirée.
  if (IsDamage(blow.mode)) {
    if (!in_combat_ || now_ms - last_damage_ms_ > settings_.combat_timeout_secs * kMsPerSecond) {
      if (in_combat_) {
        CloseFight();
      }
      OpenFight(now_ms);
    }
    last_damage_ms_ = now_ms;
  }
  const std::string_view name = NameOf(blow.who);
  session_.Add(blow, name);
  if (in_combat_) {
    current_.Add(blow, name);
  }
  if (blow.who == self_id_) {
    RateTrack& track = tracks_[Rank(blow.mode)];
    Note(track, blow.amount, now_ms);
    Enter(blow, now_ms, track.number);
    if (blow.skill_id == kAreaCycleKey && area_cycles_.size() < kKeptAreaCycles) {
      area_cycles_.push_back(blow.amount);
    }
  }
}

void DamageMeter::Note(RateTrack& track, std::int64_t amount, std::uint32_t now_ms) {
  // UN SILENCE ASSEZ LONG OUVRE UNE AUTRE ACTIVITÉ. ⚠ LE GRAPHIQUE REPART VIDE
  // AVEC ELLE : son rythme ne se lit pas sur la queue de la précédente.
  if (!track.active || now_ms - track.last_ms > settings_.combat_timeout_secs * kMsPerSecond) {
    const std::uint32_t number = track.number + 1;
    track = RateTrack();
    track.number = number;
    track.active = true;
    track.since_ms = now_ms;
    track.slot_since_ms = now_ms;
  }
  track.last_ms = now_ms;
  track.total += amount;
  track.blows.push_back({now_ms, amount});
}

void DamageMeter::Enter(const MeterBlow& blow, std::uint32_t now_ms, std::uint32_t activity) {
  std::vector<MeterRecord>& kept = records_[Rank(blow.mode)];
  // ⚠ `upper_bound` ET NON `lower_bound` : il range le nouveau APRÈS ceux qui
  // l'égalent, et c'est ce qui laisse sa place au plus ancien.
  const auto at = std::upper_bound(
      kept.begin(), kept.end(), blow.amount,
      [](std::int64_t amount, const MeterRecord& record) { return amount > record.blow.amount; });
  if (at == kept.end() && kept.size() >= kKeptRecords) {
    return;
  }
  kept.insert(at, MeterRecord{.blow = blow, .at_ms = now_ms, .activity = activity});
  if (kept.size() > kKeptRecords) {
    kept.pop_back();
  }
}

std::vector<std::int64_t> DamageMeter::TakeAreaCycles() { return std::exchange(area_cycles_, {}); }

void DamageMeter::AdvanceSlots(RateTrack& track, std::uint32_t now_ms) const {
  if (!track.active || settings_.slot_ms == 0) {
    return;
  }
  const std::uint32_t slot_ms = settings_.slot_ms;
  while (now_ms - track.slot_since_ms >= slot_ms) {
    std::int64_t dealt = 0;
    for (const Blow& blow : track.blows) {
      if (blow.at_ms - track.slot_since_ms < slot_ms) {
        dealt += blow.amount;
      }
    }
    track.plot[track.offset] = static_cast<float>(dealt) *
                               (static_cast<float>(kMsPerSecond) / static_cast<float>(slot_ms));
    track.offset = (track.offset + 1) % kPlotSlots;
    track.slot_since_ms += slot_ms;
  }
}

void DamageMeter::Tick(std::uint32_t now_ms) {
  if (in_combat_ && now_ms - last_damage_ms_ > settings_.combat_timeout_secs * kMsPerSecond) {
    CloseFight();
  }
  const std::uint32_t timeout_ms = settings_.combat_timeout_secs * kMsPerSecond;
  for (RateTrack& track : tracks_) {
    if (track.active && now_ms - track.last_ms > timeout_ms) {
      track.active = false;
    }
    AdvanceSlots(track, now_ms);
  }
  const std::uint32_t window_secs = std::max<std::uint32_t>(settings_.window_secs, 1);
  const std::uint32_t window_ms = window_secs * kMsPerSecond;
  // Ce qui est sorti DU GRAPHIQUE ET DE LA FENÊTRE GLISSANTE ne sert plus à
  // personne.
  //
  // 🔴 LES DEUX, et non le graphique seul : cent cinquante colonnes de
  // cinquante millisecondes ne couvrent que sept secondes et demie, et une
  // fenêtre de dix secondes diviserait sinon un total amputé.
  //
  // ⚠ ON COMPARE UN ÉCART, JAMAIS DEUX INSTANTS. L'horloge revient à zéro tous
  // les quarante-neuf jours, et « plus ancien que now − durée » se trompe
  // entièrement une fois passé ce tour.
  const std::uint32_t kept_ms =
      std::max(settings_.slot_ms * static_cast<std::uint32_t>(kPlotSlots), window_ms);
  for (RateTrack& track : tracks_) {
    while (!track.blows.empty() && now_ms - track.blows.front().at_ms > kept_ms) {
      track.blows.pop_front();
    }
    std::int64_t in_window = 0;
    for (const Blow& blow : track.blows) {
      if (now_ms - blow.at_ms <= window_ms) {
        in_window += blow.amount;
      }
    }
    track.rate = static_cast<float>(in_window) / static_cast<float>(window_secs);
    if (track.active && track.rate > track.peak) {
      track.peak = track.rate;
    }
  }
}

std::uint32_t DamageMeter::activity_ms(MeterMode mode, std::uint32_t now_ms) const noexcept {
  const RateTrack& track = tracks_[Rank(mode)];
  return track.active ? now_ms - track.since_ms : 0;
}

std::uint32_t DamageMeter::activity_span_ms(MeterMode mode, std::uint32_t now_ms) const noexcept {
  const RateTrack& track = tracks_[Rank(mode)];
  if (track.number == 0) {
    return 0;
  }
  return (track.active ? now_ms : track.last_ms) - track.since_ms;
}

std::uint32_t DamageMeter::combat_ms(std::uint32_t now_ms) const noexcept {
  return in_combat_ ? now_ms - fight_since_ms_ : 0;
}

std::uint32_t DamageMeter::fight_span_ms(std::uint32_t now_ms) const noexcept {
  if (fight_ == 0) {
    return 0;
  }
  return (in_combat_ ? now_ms : last_damage_ms_) - fight_since_ms_;
}

std::uint32_t DamageMeter::active_ms(const MeterSegment& segment,
                                     std::uint32_t now_ms) const noexcept {
  const std::uint32_t running = in_combat_ ? now_ms - fight_since_ms_ : 0;
  if (&segment == &current_) {
    return in_combat_ ? running : current_.active_ms;
  }
  if (&segment == &session_) {
    return session_.active_ms + running;
  }
  return segment.active_ms;
}

}  // namespace ro::sim
