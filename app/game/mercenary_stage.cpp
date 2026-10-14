#include "game/mercenary_stage.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/mercenary_frames.h"
#include "ro/net/skill.h"
#include "ro/sim/skill_book.h"

#include "game/game_pieces.h"

namespace moonclient {
namespace {

// La date de fin du contrat, comme le client l'écrit : mois/jour heure:minute.
constexpr const char* kExpiryDateFormat = "%m/%d %H:%M";
constexpr std::size_t kExpiryDateBytes = 32;
// Assez pour la ligne du rappel de fin de contrat, dans toutes les langues.
constexpr std::size_t kReminderLineBytes = 160;
constexpr std::uint32_t kSecondsPerMinute = 60;

std::int64_t NowUnixSeconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::string ExpiryDateText(std::uint32_t unix_s) {
  if (unix_s == 0) {
    return {};
  }
  const auto seconds = static_cast<std::time_t>(unix_s);
  std::tm local{};
#ifdef _WIN32
  if (localtime_s(&local, &seconds) != 0) {
    return {};
  }
#else
  if (localtime_r(&seconds, &local) == nullptr) {
    return {};
  }
#endif
  std::array<char, kExpiryDateBytes> text{};
  if (std::strftime(text.data(), text.size(), kExpiryDateFormat, &local) == 0) {
    return {};
  }
  return text.data();
}

const char* KindText(std::uint16_t job) {
  if (job == 0) {
    return ro::core::Tr("Mercenaire");
  }
  switch (ro::sim::MercenaryGuildOfJob(job)) {
    case ro::sim::MercenaryGuild::kArcher:
      return ro::core::Tr("Mercenaire : Archer");
    case ro::sim::MercenaryGuild::kSpearman:
      return ro::core::Tr("Mercenaire : Spearman");
    case ro::sim::MercenaryGuild::kSwordman:
      return ro::core::Tr("Mercenaire : Swordman");
    case ro::sim::MercenaryGuild::kMonster:
      break;
  }
  return ro::core::Tr("Mercenaire : type monstre");
}

// Le mercenaire se la lance à lui-même : la seule qui parte sans visée.
bool SelfCast(const ro::sim::MercenarySkill& skill) {
  return (skill.targeting & ro::sim::kSkillOnSelf) != 0;
}

void Complain(const ro::core::Result<void>& sent, std::string_view what) {
  if (!sent) {
    ro::core::LogWarning("mercenaire, {} non émis : {}", what, ro::core::Describe(sent.error()));
  }
}

}  // namespace

ro::net::FrameOutcome MercenaryStage::ApplyFrame(const ro::net::Frame& frame, ro::sim::World& world,
                                                 std::uint32_t now_ms) {
  last_now_ms_ = now_ms;
  // UNE ENTRÉE SUR UNE CARTE l'efface : le serveur renvoie sa fiche juste après
  // s'il est toujours sous contrat.
  if (world.map_entries() != seen_map_entries_) {
    seen_map_entries_ = world.map_entries();
    book_.Part(ro::sim::MercenaryFarewell::kNone);
    seen_job_ = 0;
  }
  const ro::net::FrameOutcome outcome = ro::net::ApplyMercenaryFrame(
      frame, book_, ro::net::MercenaryClock{.now_ms = now_ms, .now_unix_s = NowUnixSeconds()});
  // ⚠ L'ADIEU s'écrit dans la conversation par le décodeur commun des messages
  // du serveur ; on le consomme ici pour qu'il ne survive pas au départ.
  static_cast<void>(book_.TakeFarewell());
  if (!book_.present()) {
    seen_job_ = 0;
  }
  return outcome;
}

void MercenaryStage::Stage(const ro::sim::World& world, GamePieces& pieces) {
  if (!book_.present()) {
    view_ = ro::ui::MercenaryTabView{};
    return;
  }
  const ro::sim::MercenaryProfile& profile = book_.profile();
  if (const ro::sim::Actor* actor = world.Find(profile.actor_id); actor != nullptr) {
    seen_job_ = actor->look.job;
  }
  name_ = profile.name;
  kind_ = KindText(seen_job_);
  expires_at_ = ExpiryDateText(profile.expire_unix_s);

  // LES NOMS D'ABORD, tous : les lignes en gardent des vues, qu'une
  // réallocation laisserait pendre.
  const ro::sim::SkillTree* tree = pieces.skill_tree.has_value() ? &*pieces.skill_tree : nullptr;
  skill_names_.clear();
  for (const ro::sim::MercenarySkill& skill : book_.skills()) {
    const ro::sim::SkillNode* node = tree != nullptr ? tree->Find(skill.skill_id) : nullptr;
    skill_names_.push_back(node != nullptr && !node->display_name.empty() ? node->display_name
                                                                          : skill.name);
  }
  skill_rows_.clear();
  std::size_t rank = 0;
  for (const ro::sim::MercenarySkill& skill : book_.skills()) {
    const ro::sim::SkillNode* node = tree != nullptr ? tree->Find(skill.skill_id) : nullptr;
    ro::ui::MercenarySkillRow row{
        .skill_id = skill.skill_id,
        .level = skill.level,
        .name = skill_names_[rank],
        // ⚠ LE NOM DU SERVEUR EST DÉJÀ L'INTERNE : il sert quand l'arbre manque.
        .idname = node != nullptr ? std::string_view(node->idname) : std::string_view(skill.name),
        .active = skill.targeting != ro::sim::kSkillPassive,
        .castable = skill.level > 0 && skill.targeting != ro::sim::kSkillPassive,
        .needs_aim = skill.targeting != ro::sim::kSkillPassive && !SelfCast(skill),
        .sp_cost = skill.sp_cost,
        .range_cells = skill.range_cells,
    };
    ++rank;
    if (pieces.skill_icons.has_value()) {
      const ro::render::IconTexture& icon =
          pieces.skill_icons->Get(skill.skill_id, node != nullptr ? node->idname : skill.name);
      if (icon.usable()) {
        row.icon = ro::ui::SkinImage{.id = static_cast<ImTextureID>(icon.texture.name()),
                                     .width = static_cast<float>(icon.width_px),
                                     .height = static_cast<float>(icon.height_px)};
      }
    }
    skill_rows_.push_back(row);
  }

  view_ = ro::ui::MercenaryTabView{
      .present = true,
      .name = name_,
      .kind = kind_,
      .level = profile.level,
      .hp = profile.hp,
      .max_hp = profile.max_hp,
      .sp = profile.sp,
      .max_sp = profile.max_sp,
      .contract_left_s = book_.contract_left_s(last_now_ms_),
      .expires_at = expires_at_,
      .contract_ending = book_.contract_left_s(last_now_ms_) <=
                         ro::sim::kMercenaryReminderMinutes.front() * kSecondsPerMinute,
      .faith = profile.faith,
      .calls = profile.calls,
      .kills = profile.kills,
      .atk = profile.atk,
      .matk = profile.matk,
      .hit = profile.hit,
      .crit = profile.crit,
      .def = profile.def,
      .mdef = profile.mdef,
      .flee = profile.flee,
      .aspd = ro::sim::MercenaryAspd(profile.amotion_ms),
      .amotion_ms = profile.amotion_ms,
      .range_cells = profile.range_cells,
      .skills = skill_rows_,
  };
}

void MercenaryStage::Apply(const ro::ui::MercenaryTabAction& asked, ro::net::Connection& link,
                           ro::sim::World& world, std::uint32_t now_ms,
                           ro::ui::SkillDescriptionWindowState& description) {
  last_now_ms_ = now_ms;
  if (const std::uint32_t minutes = book_.TakeContractReminder(now_ms); minutes != 0) {
    std::array<char, kReminderLineBytes> line{};
    std::snprintf(line.data(), line.size(),
                  ro::core::Tr("Le contrat du mercenaire prend fin dans %d min."),
                  static_cast<int>(minutes));
    world.RecordChat(ro::sim::ChatKind::kServer, 0, line.data(), now_ms);
  }
  ro::ui::AskSkillDescription(description, asked.describe_skill_id);
  if (asked.cast.has_value()) {
    // L'onglet ne propose que ce qui part : un refus ici vient d'une liste
    // changée entre le dessin et le geste.
    if (const char* refused = Cast(link, asked.cast->skill_id); refused != nullptr) {
      ro::core::LogWarning("mercenaire, skill {} refusé : {}", asked.cast->skill_id, refused);
    }
  }
  if (asked.dismiss && book_.present()) {
    Complain(ro::net::RequestMercenaryDismiss(link), "renvoi");
    // ⚠ LE SERVEUR NE RÉPOND RIEN : l'oublier est l'affaire du client.
    book_.Part(ro::sim::MercenaryFarewell::kNone);
    seen_job_ = 0;
  }
}

std::uint32_t MercenaryStage::own_mercenary_id() const noexcept {
  return book_.present() ? book_.profile().actor_id : 0;
}

void MercenaryStage::StartSession() {
  book_ = ro::sim::MercenaryBook{};
  seen_map_entries_ = 0;
  seen_job_ = 0;
  aim_request_.reset();
}

void MercenaryStage::RequestInfo(ro::net::Connection& link) {
  if (book_.present()) {
    Complain(ro::net::RequestMercenaryInfo(link), "demande de fiche");
  }
}

const char* MercenaryStage::Cast(ro::net::Connection& link, std::uint16_t skill_id) {
  if (!book_.present()) {
    return ro::core::Tr("Aucun mercenaire sous contrat.");
  }
  const ro::sim::MercenarySkill* skill = book_.FindSkill(skill_id);
  if (skill == nullptr) {
    return ro::core::Tr("Ce skill n'est pas celui du mercenaire.");
  }
  if (skill->targeting == ro::sim::kSkillPassive || skill->level == 0) {
    return ro::core::Tr("Skill passif : il ne se lance pas.");
  }
  if (!SelfCast(*skill)) {
    aim_request_ = CompanionAim{
        .companion = ro::sim::AiCompanion::kMercenary,
        .skill_id = skill->skill_id,
        .skill_level = skill->level,
        .range_cells = skill->range_cells,
        .on_ground = ro::sim::SkillNeedsGround(ro::sim::KnownSkill{.targeting = skill->targeting}),
    };
    return nullptr;
  }
  // SUR LUI-MÊME : le serveur remplace de toute façon la cible par le mercenaire.
  Complain(
      ro::net::RequestSkillOnTarget(link, skill->skill_id, skill->level, book_.profile().actor_id),
      "skill");
  return nullptr;
}

}  // namespace moonclient
