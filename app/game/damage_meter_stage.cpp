#include "game/damage_meter_stage.h"

#include <algorithm>
#include <cmath>
#include <span>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/sim/chat_line.h"

namespace moonclient {
namespace {

// La teinte d'un cumul de zone dans la conversation, en `0xRRGGBB` : un orangé
// qui le sépare des coups ordinaires.
constexpr std::uint32_t kAreaCycleChatColor = 0xFFAA00;
constexpr float kMsPerSecond = 1000.0F;
constexpr const char* kCaptionGlue = "  ·  ";

[[nodiscard]] ro::sim::MeterMode SimMode(ro::ui::DamageMeterMode mode) {
  return static_cast<ro::sim::MeterMode>(mode);
}

[[nodiscard]] float PerSecond(std::int64_t amount, std::uint32_t active_ms) {
  return active_ms == 0 ? 0.0F
                        : static_cast<float>(amount) * kMsPerSecond / static_cast<float>(active_ms);
}

}  // namespace

void DamageMeterStage::Follow(ro::sim::World& world, const ro::sim::PartyRoster& party) {
  // ⚠ LES MEMBRES HORS LIGNE NE SONT PAS SUIVIS : ils ne frappent personne, et
  // un identifiant de compte peut être repris par un autre personnage.
  followed_.clear();
  for (const ro::sim::PartyMember& member : party.members()) {
    if (!member.offline && member.account_id != world.player_id()) {
      followed_.push_back(ro::sim::MeterActor{.id = member.account_id, .name = member.name});
    }
  }
  world.damage_meter().Track(world.player_id(), world.player_name(), followed_);
}

void DamageMeterStage::Compose(ro::sim::World& world, const ro::sim::PartyRoster& party,
                               const ro::sim::SkillTree* skills, DamageMeterOptions& options,
                               ImFont* bold, std::uint32_t now_ms, ro::ui::DamageMeterView* into) {
  Follow(world, party);
  ro::sim::DamageMeter& meter = world.damage_meter();
  const ro::sim::DamageMeter::Settings wanted = options.meter();
  // ⚠ UNE AUTRE LARGEUR DE COLONNE REND LE GRAPHIQUE ILLISIBLE : ses colonnes
  // passées ne mesurent plus la même chose. On l'oublie.
  if (slot_ms_.has_value() && *slot_ms_ != wanted.slot_ms) {
    meter.Forget();
  }
  slot_ms_ = wanted.slot_ms;
  meter.settings() = wanted;
  if (options.TakeResetFight()) {
    meter.Forget();
  }
  if (options.TakeForgetRecords()) {
    meter.ForgetRecords();
  }
  meter.Tick(now_ms);
  WriteAreaCycles(world, options, now_ms);

  const ro::sim::MeterMode mode = SimMode(options.mode());
  // ⚠ LES VUES SE VIDENT AVEC LES NOMS QU'ELLES DÉSIGNENT.
  names_.clear();
  records_.clear();
  bars_.clear();
  detail_.clear();
  ComposeCaption(meter, mode, options, now_ms);

  ro::ui::DamageMeterView& view = *into;
  view.look = options.look();
  if (!view.look.shown) {
    return;
  }
  view.mode = options.mode();
  view.segment = options.segment();
  view.pane = options.pane();
  view.title = title_;
  view.dps = meter.rate(mode);
  view.peak_dps = meter.peak(mode);
  view.total_damage = meter.total(mode);
  // ⚠ L'ACTIVITÉ DU MODE, et non le combat : un soin hors combat a sa courbe.
  view.in_combat = meter.active(mode);
  view.combat_ms = meter.activity_ms(mode, now_ms);
  view.fight_span_ms = meter.activity_span_ms(mode, now_ms);
  view.plot = meter.plot(mode);
  view.plot_offset = meter.plot_offset(mode);
  view.slot_ms = meter.settings().slot_ms;
  NameRecords(meter, mode, skills, now_ms);
  view.records = records_;

  const ro::sim::MeterSegment& segment =
      view.segment == ro::ui::DamageMeterSegment::kPrevious  ? meter.previous()
      : view.segment == ro::ui::DamageMeterSegment::kSession ? meter.session()
                                                             : meter.current();
  ComposeGroup(meter, mode, segment, meter.active_ms(segment, now_ms), skills);
  view.bars = bars_;
  view.detail_id = detail_id_;
  view.detail_name = detail_name_;
  view.detail = detail_;
  view.now_ms = now_ms;
  view.bold = bold;
}

void DamageMeterStage::ComposeCaption(const ro::sim::DamageMeter& meter, ro::sim::MeterMode mode,
                                      const DamageMeterOptions& options, std::uint32_t now_ms) {
  const DamageMeterOptions::Caption& wanted = options.caption();
  const auto ui_mode = static_cast<ro::ui::DamageMeterMode>(mode);
  std::string caption = ro::ui::DamageMeterRateUnit(ui_mode);
  bool first = true;
  const auto add = [&caption, &first](const char* label, std::int64_t amount) {
    caption += first ? " " : kCaptionGlue;
    if (label != nullptr) {
      caption += label;
      caption += ' ';
    }
    caption += ro::ui::FormatDamageAmount(amount);
    first = false;
  };
  // ⚠ LE RYTHME NE S'ÉCRIT QU'EN ACTIVITÉ : au repos, c'est un zéro qui vient
  // de tomber, pas une mesure. Le bilan, lui, reste celui de la dernière.
  if (wanted.rate && meter.active(mode)) {
    add(nullptr, std::llround(meter.rate(mode)));
  }
  const std::int64_t total = meter.total(mode);
  if (wanted.peak && meter.peak(mode) > 0.0F) {
    add(ro::core::Tr("PIC"), std::llround(meter.peak(mode)));
  }
  const std::uint32_t span_ms = meter.activity_span_ms(mode, now_ms);
  if (wanted.average && total > 0 && span_ms > 0) {
    add(ro::core::Tr("MOY"), std::llround(PerSecond(total, span_ms)));
  }
  if (wanted.total && total > 0) {
    add(ro::core::Tr("TOTAL"), total);
  }
  tab_label_ = caption;
  title_ = ro::ui::DamageMeterModeName(ui_mode);
  title_ += " — ";
  title_ += caption;
}

void DamageMeterStage::WriteAreaCycles(ro::sim::World& world, const DamageMeterOptions& options,
                                       std::uint32_t now_ms) {
  // ⚠ LA FILE SE VIDE MÊME QUAND LE JOUEUR N'EN VEUT PAS : sinon le premier
  // allumage déverserait d'un coup tout ce qui attendait.
  const std::vector<std::int64_t> cycles = world.damage_meter().TakeAreaCycles();
  if (!options.area_in_chat()) {
    return;
  }
  for (const std::int64_t damage : cycles) {
    world.RecordChat(ro::sim::ChatKind::kBattle, 0,
                     ro::core::Printed(ro::core::Tr("Sort de zone : %s dégâts"),
                                       ro::ui::GroupDamageDigits(damage).c_str()),
                     now_ms, kAreaCycleChatColor);
  }
}

std::string_view DamageMeterStage::SkillName(std::uint16_t key, const ro::sim::SkillTree* skills) {
  if (key == ro::sim::kAreaCycleKey) {
    return ro::core::Tr("Sort de zone (cumul)");
  }
  if (key == ro::sim::kRecoveryKey) {
    return ro::core::Tr("Récupération (items, régén., vols de vie…)");
  }
  if (key == 0) {
    return ro::core::Tr("Attaque");
  }
  const ro::sim::SkillNode* node = skills != nullptr ? skills->Find(key) : nullptr;
  if (node != nullptr && !node->display_name.empty()) {
    return node->display_name;
  }
  return names_.emplace_back(
      ro::core::Printed(ro::core::Tr("Skill n°%u"), static_cast<unsigned>(key)));
}

void DamageMeterStage::NameRecords(const ro::sim::DamageMeter& meter, ro::sim::MeterMode mode,
                                   const ro::sim::SkillTree* skills, std::uint32_t now_ms) {
  records_.clear();
  for (const ro::sim::MeterRecord& record : meter.records(mode)) {
    const ro::sim::MeterBlow& blow = record.blow;
    const bool area = blow.skill_id == ro::sim::kAreaCycleKey;
    const bool skill = blow.skill_id != 0 && !area && blow.skill_id != ro::sim::kRecoveryKey;
    records_.push_back(ro::ui::DamageMeterRecordView{
        .amount = blow.amount,
        .what = SkillName(blow.skill_id, skills),
        .skill_level = skill ? blow.skill_level : std::uint16_t{0},
        .other = names_.emplace_back(blow.other_name),
        .hits = blow.hits,
        .critical = blow.critical,
        .area_cycle = area,
        .this_fight = meter.active(mode) && record.activity == meter.activity(mode),
        .age_ms = now_ms - record.at_ms});
  }
}

void DamageMeterStage::ComposeGroup(const ro::sim::DamageMeter& meter, ro::sim::MeterMode mode,
                                    const ro::sim::MeterSegment& segment, std::uint32_t active_ms,
                                    const ro::sim::SkillTree* skills) {
  bars_.clear();
  detail_.clear();
  detail_name_.clear();
  const auto& by_actor = segment.of(mode);
  std::int64_t sum = 0;
  for (const auto& [id, tally] : by_actor) {
    sum += tally.total.amount;
  }
  const auto share = [](std::int64_t part, std::int64_t whole) {
    return whole <= 0 ? 0.0F : static_cast<float>(part) / static_cast<float>(whole);
  };
  for (const auto& [id, tally] : by_actor) {
    bars_.push_back(ro::ui::DamageMeterBarView{
        .id = id,
        .name = tally.name.empty() ? names_.emplace_back("#" + std::to_string(id))
                                   : std::string_view(tally.name),
        .amount = tally.total.amount,
        .per_second = PerSecond(tally.total.amount, active_ms),
        .share = share(tally.total.amount, sum),
        .self = id == meter.self_id()});
  }
  std::ranges::sort(bars_, [](const auto& left, const auto& right) {
    return left.amount != right.amount ? left.amount > right.amount : left.id < right.id;
  });

  // LE DÉTAIL DE L'ACTEUR OUVERT. ⚠ ABSENT DU SEGMENT, il reste ouvert mais
  // vide : changer de segment ne doit pas le refermer sous la main du joueur.
  if (detail_id_ == 0) {
    return;
  }
  const auto found = by_actor.find(detail_id_);
  if (found == by_actor.end()) {
    const auto named = std::ranges::find_if(
        followed_, [this](const ro::sim::MeterActor& actor) { return actor.id == detail_id_; });
    detail_name_ = detail_id_ == meter.self_id() ? std::string(ro::core::Tr("Moi"))
                   : named != followed_.end()    ? named->name
                                                 : std::string();
    return;
  }
  detail_name_ = found->second.name;
  for (const auto& [key, tally] : found->second.skills) {
    detail_.push_back(
        ro::ui::DamageMeterSkillView{.name = SkillName(key, skills),
                                     .amount = tally.amount,
                                     .events = tally.events,
                                     .critical = tally.critical,
                                     .biggest = tally.biggest,
                                     .share = share(tally.amount, found->second.total.amount)});
  }
  std::ranges::sort(detail_,
                    [](const auto& left, const auto& right) { return left.amount > right.amount; });
}

bool DamageMeterStage::Apply(const ro::ui::DamageMeterAction& asked, ro::sim::World& world,
                             DamageMeterOptions& options) {
  bool changed = false;
  if (asked.close) {
    options.SetShown(false);
    changed = true;
  }
  if (asked.lock.has_value()) {
    options.SetLocked(*asked.lock);
    changed = true;
  }
  if (asked.records.has_value()) {
    options.SetRecords(*asked.records);
    changed = true;
  }
  if (asked.mode.has_value()) {
    options.SetMode(*asked.mode);
    changed = true;
  }
  if (asked.segment.has_value()) {
    options.SetSegment(*asked.segment);
    changed = true;
  }
  if (asked.pane.has_value()) {
    options.SetPane(*asked.pane);
    changed = true;
  }
  if (asked.detail.has_value()) {
    detail_id_ = *asked.detail;
  }
  if (asked.reset_fight) {
    world.damage_meter().Forget();
  }
  if (asked.forget_records) {
    world.damage_meter().ForgetRecords();
  }
  return changed;
}

}  // namespace moonclient
