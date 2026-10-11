#include "game/companion_ai_stage.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/companion_orders.h"
#include "ro/net/member_statuses.h"
#include "ro/net/skill.h"
#include "ro/net/target_info.h"

#include "game/homunculus_stage.h"
#include "game/mercenary_stage.h"

namespace moonclient {
namespace {

using ro::sim::AiCompanion;

constexpr std::size_t kHomunculusSlot = 0;
constexpr std::size_t kMercenarySlot = 1;

[[nodiscard]] std::size_t IndexOf(AiCompanion companion) noexcept {
  return companion == AiCompanion::kHomunculus ? kHomunculusSlot : kMercenarySlot;
}

// La classe de l'acteur du compagnon, zéro tant qu'il n'est pas en vue.
[[nodiscard]] std::uint16_t JobOf(const ro::sim::World& world, std::uint32_t actor_id) {
  const ro::sim::Actor* actor = actor_id != 0 ? world.Find(actor_id) : nullptr;
  return actor != nullptr ? actor->look.job : std::uint16_t{0};
}

// LA RECHARGE QUI RESTE À UN SKILL DE COMPAGNON : le serveur l'annonce au
// maître comme celle de ses propres skills, et le grimoire la range même pour
// un skill qu'il ne porte pas.
[[nodiscard]] std::uint32_t CooldownOf(const ro::sim::World& world, std::uint16_t skill_id,
                                       std::uint32_t now_ms) {
  return world.skill_book().CooldownRemainingMs(skill_id, now_ms);
}

// LE RAYON DE LA ZONE D'UN SKILL à ce niveau, d'après les tables du client :
// la moitié de la largeur déclarée. Zéro sans zone déclarée, ou sans tables.
[[nodiscard]] int AreaRadiusOf(const ro::sim::SkillTree* skill_tree, std::uint16_t skill_id,
                               std::uint16_t level) {
  const ro::sim::SkillNode* node = skill_tree != nullptr ? skill_tree->Find(skill_id) : nullptr;
  if (node == nullptr) {
    return 0;
  }
  const ro::sim::SkillScaleCells scale = ro::sim::SkillScaleAt(*node, level);
  return scale.Declared() ? static_cast<int>(scale.x) / 2 : 0;
}

[[nodiscard]] ro::sim::CompanionFacts HomunculusFacts(const ro::sim::Homunculus& homunculus,
                                                      const ro::sim::World& world,
                                                      const ro::sim::SkillTree* skill_tree,
                                                      std::uint32_t now_ms) {
  const ro::sim::HomunculusSheet& sheet = homunculus.sheet();
  std::vector<ro::sim::CompanionSkillFact> skills;
  skills.reserve(homunculus.skills().size());
  for (const ro::sim::HomunculusSkill& skill : homunculus.skills()) {
    const auto level = static_cast<std::uint16_t>(std::max(skill.level, 0));
    skills.push_back(ro::sim::CompanionSkillFact{
        .skill_id = skill.skill_id,
        .level = level,
        .targeting = skill.targeting,
        .sp_cost = skill.sp_cost,
        .range_cells = skill.range_cells,
        .cooldown_remaining_ms = CooldownOf(world, skill.skill_id, now_ms),
        .area_radius_cells = AreaRadiusOf(skill_tree, skill.skill_id, level)});
  }
  return ro::sim::CompanionFacts{
      .actor_id = homunculus.actor_id(),
      .kind = AiCompanion::kHomunculus,
      .job = JobOf(world, homunculus.actor_id()),
      .active = homunculus.active(),
      .hp = sheet.hp,
      .max_hp = sheet.max_hp,
      .sp = sheet.sp,
      .max_sp = sheet.max_sp,
      .amotion_ms = static_cast<std::uint32_t>(std::max(sheet.amotion_ms, 0)),
      .attack_range_cells = std::max(sheet.attack_range_cells, 1),
      // Les SP viennent de la fiche : connues une fois qu'elle est arrivée.
      .sp_known = homunculus.sheet_known(),
      .skills = std::move(skills),
  };
}

[[nodiscard]] ro::sim::CompanionFacts MercenaryFacts(const ro::sim::MercenaryBook& book,
                                                     const ro::sim::World& world,
                                                     const ro::sim::SkillTree* skill_tree,
                                                     std::uint32_t now_ms) {
  const ro::sim::MercenaryProfile& profile = book.profile();
  std::vector<ro::sim::CompanionSkillFact> skills;
  skills.reserve(book.skills().size());
  for (const ro::sim::MercenarySkill& skill : book.skills()) {
    skills.push_back(ro::sim::CompanionSkillFact{
        .skill_id = skill.skill_id,
        .level = skill.level,
        .targeting = skill.targeting,
        .sp_cost = skill.sp_cost,
        .range_cells = skill.range_cells,
        .cooldown_remaining_ms = CooldownOf(world, skill.skill_id, now_ms),
        .area_radius_cells = AreaRadiusOf(skill_tree, skill.skill_id, skill.level)});
  }
  return ro::sim::CompanionFacts{
      .actor_id = profile.actor_id,
      .kind = AiCompanion::kMercenary,
      .job = JobOf(world, profile.actor_id),
      .active = book.present() && profile.actor_id != 0,
      .hp = profile.hp,
      .max_hp = profile.max_hp,
      .sp = profile.sp,
      .max_sp = profile.max_sp,
      .amotion_ms = static_cast<std::uint32_t>(std::max(profile.amotion_ms, 0)),
      .attack_range_cells = std::max(profile.range_cells, 1),
      // Les SP viennent de la fiche, qui fait exister le mercenaire.
      .sp_known = book.present(),
      .skills = std::move(skills),
  };
}

void Send(ro::net::Connection& link, std::uint32_t companion_id,
          const ro::sim::CompanionOrder& order) {
  ro::core::Result<void> sent;
  switch (order.kind) {
    case ro::sim::CompanionOrder::Kind::kNone:
      return;
    case ro::sim::CompanionOrder::Kind::kMove:
      sent = ro::net::RequestCompanionMove(link, companion_id, order.cell);
      break;
    case ro::sim::CompanionOrder::Kind::kAttack:
      sent = ro::net::RequestCompanionAttack(link, companion_id, order.target_id);
      break;
    case ro::sim::CompanionOrder::Kind::kReturnToOwner:
      sent = ro::net::RequestCompanionReturn(link, companion_id);
      break;
    case ro::sim::CompanionOrder::Kind::kSkill:
      // Le serveur reconnaît le skill du compagnon à son identifiant, et le
      // fait lancer par lui.
      sent = order.target_id != 0 ? ro::net::RequestSkillOnTarget(
                                        link, order.skill_id, order.skill_level, order.target_id)
                                  : ro::net::RequestSkillOnGround(link, order.skill_id,
                                                                  order.skill_level, order.cell);
      break;
  }
  if (!sent) {
    ro::core::LogWarning("IA du compagnon : ordre non émis — {}", ro::core::Describe(sent.error()));
  }
}

}  // namespace

bool CompanionAiStage::Advance(ro::net::Connection& link, ro::sim::World& world,
                               HomunculusStage& homunculus, MercenaryStage& mercenary,
                               const CompanionAiBooks& books, bool grimoire_armed,
                               std::uint32_t now_ms) {
  // UN SEUL ARMEMENT À LA FOIS : le grimoire qui vient de s'armer désarme le
  // skill de compagnon ; celui-ci, en s'armant, demande à l'appelant de désarmer
  // le grimoire.
  if (grimoire_armed && !grimoire_was_armed_) {
    aim_.reset();
  }
  bool armed_now = false;
  for (std::optional<CompanionAim> asked :
       {homunculus.TakeAimRequest(), mercenary.TakeAimRequest()}) {
    if (asked.has_value() && Arm(*asked, world, now_ms)) {
      armed_now = true;
    }
  }
  grimoire_was_armed_ = grimoire_armed && !armed_now;
  last_now_ms_ = now_ms;
  SaveQuietEdits(now_ms);

  const bool new_map = seen_map_entries_ != world.map_entries();
  seen_map_entries_ = world.map_entries();
  if (new_map) {
    arrived_ms_ = noted_arrival_ms_.value_or(now_ms);
    noted_arrival_ms_.reset();
  }
  session_.ObserveMap(world.map_entries(), world.map_mode(), arrived_ms_.value_or(now_ms), now_ms);
  if (const ro::sim::Actor* self = world.player(); self != nullptr) {
    session_.NoteCharacterName(self->name);
  }
  const std::int64_t wall_now_ms = WallNowMs();

  std::array<ro::sim::CompanionFacts, 2> facts = {
      HomunculusFacts(homunculus.homunculus(), world, books.skill_tree, now_ms),
      MercenaryFacts(mercenary.book(), world, books.skill_tree, now_ms)};
  for (std::size_t index = 0; index < facts.size(); ++index) {
    const ro::sim::CompanionFacts& partner = facts[facts.size() - 1 - index];
    facts[index].partner_id = partner.active ? partner.actor_id : 0;
    for (const ro::sim::PartyMember& member : books.party.members()) {
      facts[index].party_member_ids.push_back(member.account_id);
    }
    facts[index].monster_sheets = books.monster_sheets;
    facts[index].walk_map = books.walk_map;
    facts[index].owner_guild_id = books.owner_guild_id;
  }
  for (std::size_t index = 0; index < slots_.size(); ++index) {
    Slot& slot = slots_[index];
    Follow(slot, facts[index], now_ms);
    if (new_map) {
      slot.brain.ForgetMap(slot.settings.standby_kept_across_maps, arrived_ms_.value_or(now_ms));
      slot.statuses.Forget();
    }
    if (facts[index].active && slot.followed.job.has_value()) {
      session_.Resolve(Seated(facts[index].kind), *slot.followed.job, now_ms, wall_now_ms);
    }
    // LA LISTE DES STATUTS, seulement quand une règle la lit et que le compagnon
    // est en vue ; jamais au repos.
    const ro::sim::Actor* self = facts[index].active ? world.Find(facts[index].actor_id) : nullptr;
    if (link.is_open() &&
        slot.statuses.ShouldAsk(ro::sim::ReadsCompanionStatus(slot.settings.rules), self, now_ms)) {
      if (const ro::core::Result<void> asked =
              ro::net::RequestTargetStatuses(link, facts[index].actor_id);
          !asked) {
        ro::core::LogWarning("IA du compagnon : liste des statuts non demandée — {}",
                             ro::core::Describe(asked.error()));
      }
    }
    facts[index].statuses = slot.statuses.statuses();
    facts[index].statuses_ms = slot.statuses.received_ms();
    if (!facts[index].active) {
      slot.trace.Forget();
      if (aim_.has_value() && IndexOf(aim_->companion) == index) {
        aim_.reset();
      }
      continue;
    }
    const std::optional<ro::sim::CompanionOrder> order =
        slot.brain.Tick(world, facts[index], slot.settings, now_ms);
    if (order.has_value() && link.is_open()) {
      Send(link, facts[index].actor_id, *order);
    }
    slot.trace.Note(index == kHomunculusSlot ? "homoncule" : "mercenaire", world, facts[index],
                    slot.brain, order, now_ms);
    if (slot.brain.wanted_monster_sheet() != 0) {
      wanted_monster_sheet_ = slot.brain.wanted_monster_sheet();
    }
  }
  NoteProposals(world, now_ms);
  const std::array<SeatedCompanion, 2> seated = SeatedAll();
  session_.SaveState(seated, now_ms, wall_now_ms, /*force=*/false);
  return armed_now;
}

bool CompanionAiStage::present(AiCompanion companion) const noexcept {
  return SlotOf(companion).actor_id != 0;
}

std::vector<std::uint32_t> CompanionAiStage::target_marks() const {
  std::vector<std::uint32_t> marks;
  for (const Slot& slot : slots_) {
    const std::uint32_t marked_id = slot.designation.marked_id();
    if (slot.actor_id != 0 && marked_id != 0) {
      marks.push_back(marked_id);
    }
  }
  return marks;
}

CompanionGesture CompanionAiStage::PressOnActor(AiCompanion companion, std::uint32_t actor_id,
                                                bool reserve, bool second_of_double) {
  Slot& slot = SlotOf(companion);
  if (slot.actor_id == 0) {
    return CompanionGesture::kIgnored;
  }
  const ro::sim::DirectOrder order{.kind = ro::sim::DirectOrder::Kind::kAttack,
                                   .target_id = actor_id};
  switch (slot.designation.Press(actor_id, reserve, second_of_double)) {
    case ro::sim::AttackPress::kIgnored:
      return CompanionGesture::kIgnored;
    case ro::sim::AttackPress::kDesignated:
      return CompanionGesture::kDesignated;
    case ro::sim::AttackPress::kAttack:
      slot.brain.Command(order);
      return CompanionGesture::kCommanded;
    case ro::sim::AttackPress::kReserveAttack:
      return slot.brain.Reserve(order) ? CompanionGesture::kReserved : CompanionGesture::kIgnored;
  }
  return CompanionGesture::kIgnored;
}

CompanionGesture CompanionAiStage::PressOnGround(AiCompanion companion, ro::sim::CellPosition cell,
                                                 bool reserve, bool second_of_double) {
  Slot& slot = SlotOf(companion);
  if (slot.actor_id == 0 || second_of_double) {
    return CompanionGesture::kIgnored;
  }
  const ro::sim::DirectOrder order{.kind = ro::sim::DirectOrder::Kind::kMove, .cell = cell};
  if (reserve) {
    return slot.brain.Reserve(order) ? CompanionGesture::kReserved : CompanionGesture::kIgnored;
  }
  slot.designation.Clear();
  slot.brain.Command(order);
  return CompanionGesture::kCommanded;
}

bool CompanionAiStage::ToggleStandby(AiCompanion companion, ro::sim::World& world,
                                     std::uint32_t now_ms) {
  Slot& slot = SlotOf(companion);
  if (slot.actor_id == 0) {
    return false;
  }
  slot.brain.ToggleStandby();
  const bool homunculus = companion == AiCompanion::kHomunculus;
  const char* said = nullptr;
  if (slot.brain.standby()) {
    said = homunculus ? ro::core::Tr("L'homoncule se met en attente.")
                      : ro::core::Tr("Le mercenaire se met en attente.");
  } else {
    said = homunculus ? ro::core::Tr("L'homoncule n'est plus en attente.")
                      : ro::core::Tr("Le mercenaire n'est plus en attente.");
  }
  world.RecordChat(ro::sim::ChatKind::kServer, 0, std::string(said), now_ms);
  return true;
}

bool CompanionAiStage::standby(AiCompanion companion) const noexcept {
  return SlotOf(companion).brain.standby();
}

SkillAim CompanionAiStage::cursor_aim() const noexcept {
  if (!aim_.has_value()) {
    return SkillAim::kNothing;
  }
  return aim_->on_ground ? SkillAim::kGround : SkillAim::kTarget;
}

bool CompanionAiStage::ReleaseOnActor(std::uint32_t actor_id) {
  if (!aim_.has_value() || aim_->on_ground) {
    return false;
  }
  const CompanionAim aim = *std::exchange(aim_, std::nullopt);
  SlotOf(aim.companion)
      .brain.Command(ro::sim::DirectOrder{.kind = ro::sim::DirectOrder::Kind::kSkillOnActor,
                                          .target_id = actor_id,
                                          .skill_id = aim.skill_id,
                                          .skill_level = aim.skill_level,
                                          .skill_range_cells = aim.range_cells});
  return true;
}

bool CompanionAiStage::ReleaseOnGround(ro::sim::CellPosition cell) {
  if (!aim_.has_value() || !aim_->on_ground) {
    return false;
  }
  const CompanionAim aim = *std::exchange(aim_, std::nullopt);
  SlotOf(aim.companion)
      .brain.Command(ro::sim::DirectOrder{.kind = ro::sim::DirectOrder::Kind::kSkillOnCell,
                                          .cell = cell,
                                          .skill_id = aim.skill_id,
                                          .skill_level = aim.skill_level,
                                          .skill_range_cells = aim.range_cells});
  return true;
}

void CompanionAiStage::Follow(Slot& slot, const ro::sim::CompanionFacts& facts,
                              std::uint32_t now_ms) {
  slot.actor_id = facts.active ? facts.actor_id : 0;
  if (slot.actor_id == 0) {
    return;
  }
  // UN AUTRE COMPAGNON n'hérite de rien, voir `FollowCompanion` ; son profil
  // s'applique ensuite, voir `CompanionAiSession::Resolve`.
  static_cast<void>(FollowCompanion(slot.followed, slot.brain, slot.actor_id, facts.job,
                                    arrived_ms_.value_or(now_ms)));
}

bool CompanionAiStage::Arm(const CompanionAim& aim, ro::sim::World& world, std::uint32_t now_ms) {
  if (!present(aim.companion)) {
    return false;
  }
  // ⚠ LE MÊME SKILL RÉARMÉ NE SE REDIT PAS : une touche tenue le redemande à
  // chaque répétition.
  if (aim_ == aim) {
    return true;
  }
  aim_ = aim;
  world.RecordChat(
      ro::sim::ChatKind::kServer, 0,
      std::string(ro::core::Tr("Désigne la cible du skill (clic droit ou Échap pour annuler).")),
      now_ms);
  return true;
}

void CompanionAiStage::ObserveFrame(const ro::net::Frame& frame, const HomunculusStage& homunculus,
                                    const MercenaryStage& mercenary, std::uint32_t now_ms) {
  if (const std::optional<ro::net::StatusListAnswer> answer = ro::net::ReadStatusList(frame);
      answer.has_value()) {
    for (Slot& slot : slots_) {
      if (slot.actor_id != 0 && slot.actor_id == answer->gid) {
        std::vector<ro::sim::StatusEffectId> statuses;
        for (const ro::sim::StatusReport& report : answer->reports) {
          statuses.push_back(report.effect);
        }
        slot.statuses.Receive(answer->status == ro::net::kTargetAnswerOk, statuses, now_ms);
      }
    }
    return;
  }
  const std::optional<std::uint16_t> refused = ro::net::RefusedSkillOf(frame);
  if (!refused.has_value()) {
    return;
  }
  if (homunculus.FindSkill(*refused) != nullptr) {
    SlotOf(AiCompanion::kHomunculus).brain.NoteSkillRefused(*refused, now_ms);
  }
  if (mercenary.FindSkill(*refused) != nullptr) {
    SlotOf(AiCompanion::kMercenary).brain.NoteSkillRefused(*refused, now_ms);
  }
}

void CompanionAiStage::NoteArrival(std::uint32_t loaded_ms) noexcept {
  noted_arrival_ms_ = loaded_ms;
}

CompanionAiStage::Slot& CompanionAiStage::SlotOf(AiCompanion companion) noexcept {
  return slots_[IndexOf(companion)];
}

const CompanionAiStage::Slot& CompanionAiStage::SlotOf(AiCompanion companion) const noexcept {
  return slots_[IndexOf(companion)];
}

}  // namespace moonclient
