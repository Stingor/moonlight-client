#include <algorithm>
#include <array>
#include <cstdint>

#include "ro/sim/damage_meter.h"

namespace ro::sim {
namespace {

// Les sortes de coup CRITIQUES, du même `e_damage_type` : le coup simple, et
// le coup multiple.
constexpr std::array<std::uint8_t, 2> kCriticalActions = {10, 13};

[[nodiscard]] std::string NameOrEmpty(const Actor* actor) {
  return actor != nullptr ? actor->name : std::string();
}

}  // namespace

void DamageMeter::NoteStrike(const Strike& strike, std::uint32_t author, bool by_unit,
                             const Actor* attacker, const Actor* target) {
  if (!DamageActionCounts(strike.type)) {
    return;
  }
  MeterBlow blow;
  blow.amount = std::int64_t{strike.damage} + strike.second_damage;
  blow.skill_id = strike.skill_id;
  blow.skill_level = strike.skill_level;
  blow.hits = std::max<std::uint16_t>(strike.hits, 1);
  blow.critical = std::ranges::find(kCriticalActions, strike.type) != kCriticalActions.end();

  // CE QUI EST INFLIGÉ. ⚠ PAS NOS COUPS DE ZONE : le cumul privé du serveur
  // les compte déjà, toutes cibles comprises — voir l'en-tête.
  if (tracks(author) && !(by_unit && author == self_id_)) {
    MeterBlow done = blow;
    done.mode = MeterMode::kDamageDone;
    done.who = author;
    done.other_id = strike.target;
    done.other_name = NameOrEmpty(target);
    Record(done, strike.started_ms);
  }
  // CE QUI EST ENCAISSÉ, d'où qu'il vienne.
  if (tracks(strike.target)) {
    MeterBlow taken = std::move(blow);
    taken.mode = MeterMode::kDamageTaken;
    taken.who = strike.target;
    taken.other_id = author;
    taken.other_name = NameOrEmpty(attacker);
    Record(taken, strike.started_ms);
  }
}

void DamageMeter::NoteHeal(const SupportHit& hit, std::uint32_t author, std::uint32_t target_id,
                           const Actor* healer, const Actor* target) {
  if (std::ranges::find(kHealAnnounceSkills, hit.skill_id) == kHealAnnounceSkills.end() ||
      !hit.succeeded || hit.amount <= 0) {
    return;
  }
  MeterBlow blow;
  blow.amount = hit.amount;
  blow.skill_id = hit.skill_id;
  // ⚠ UN MONSTRE SOIGNÉ NE COMPTE PAS : Sanctuary rend des PV à tout ce qui
  // n'est ni mort-vivant ni démon, ennemis compris.
  const bool enemy = target != nullptr && target->kind == ActorKind::kMonster;
  if (tracks(author) && !enemy) {
    MeterBlow done = blow;
    done.mode = MeterMode::kHealingDone;
    done.who = author;
    done.other_id = target_id;
    done.other_name = NameOrEmpty(target);
    Record(done, hit.started_ms);
  }
  if (tracks(target_id)) {
    MeterBlow received = std::move(blow);
    received.mode = MeterMode::kHealingTaken;
    received.who = target_id;
    received.other_id = author;
    received.other_name = NameOrEmpty(healer);
    Record(received, hit.started_ms);
    if (target_id == self_id_) {
      last_heal_on_self_ms_ = hit.started_ms;
      last_heal_on_self_ = hit.amount;
    }
  }
}

void DamageMeter::NoteRecovery(std::int64_t amount, std::uint32_t now_ms) {
  // ⚠ UN MÊME SOIN PEUT ARRIVER DEUX FOIS : certains soins — Epiclesis — sont
  // annoncés à tous ET rendus à nous seuls, dans la même trame. L'annonce porte
  // le soin DEMANDÉ, le rendu le soin EFFECTIF, plafonné au manque : les
  // montants diffèrent, et seule la trame les relie. Le second ne compte pas.
  if (last_heal_on_self_ != 0 && now_ms == last_heal_on_self_ms_) {
    return;
  }
  Record(MeterBlow{.mode = MeterMode::kHealingTaken,
                   .who = self_id_,
                   .amount = amount,
                   .skill_id = kRecoveryKey},
         now_ms);
}

}  // namespace ro::sim
