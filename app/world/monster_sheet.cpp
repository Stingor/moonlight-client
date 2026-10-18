#include "world/monster_sheet.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/vfs/monster_words.h"

#include "game/skin_image_of.h"

namespace moonclient {
namespace {

// Une demande en vol depuis moins que cela ne repart pas. ⚠ VAUT AUSSI POUR
// LE RAFRAÎCHISSEMENT : sans elle, l'EXP périmée redemanderait à chaque image
// jusqu'à la réponse.
constexpr std::uint32_t kAskPatienceMs = 5000;
// Le serveur nomme la carte sans extension ; la table des noms, par son fichier.
constexpr std::string_view kMapFileExtension = ".gat";

// Les cinq Gardiens de forteresse que le jeu refuse de montrer.
constexpr std::array<std::uint32_t, 5> kGuardianClasses = {1285, 1286, 1287, 1829, 1830};

// Le rang d'arrivée d'une fiche tirée de la base, qu'aucune réponse ne porte.
constexpr std::uint32_t kBaseArrival = UINT32_MAX;

[[nodiscard]] bool IsGuardian(std::uint32_t sprite_class) {
  return std::ranges::find(kGuardianClasses, sprite_class) != kGuardianClasses.end();
}

// ── LE QUALIFICATIF ACCOLÉ AU NOM ───────────────────────────────────────────
//
// Nul quand il n'y a rien de SÛR à dire, et c'est le cas le plus fréquent. Du
// plus certain au moins certain, et on s'arrête net :
//   1. `EVENT_` — le nom se déclare lui-même ;
//   2. `summoned` — le serveur a CONSTATÉ qu'un autre monstre l'invoque ;
//   3. `E_` — convention, et l'abréviation évidente du premier cas.
//
// 🔴 AU-DELÀ ON SE TAIT : `META_`, `A_`, `B_`… n'ont pas de sens établi sur ce
// serveur, et certains sont de vrais boss d'instance avec butin. Inventer une
// étiquette serait la désinformation même que cette fiche combat.
[[nodiscard]] const char* VariantQualifier(std::string_view aegis_name, bool summoned) {
  const auto starts = [aegis_name](std::string_view prefix) {
    return aegis_name.size() > prefix.size() && aegis_name.starts_with(prefix);
  };
  if (starts("EVENT_")) {
    return ro::core::Tr("événement");
  }
  if (summoned) {
    return ro::core::Tr("invoqué");
  }
  if (starts("E_")) {
    return ro::core::Tr("événement");
  }
  return nullptr;
}

// Les statistiques FINALES du personnage, ou rien tant que le serveur n'en a
// rien dit : une simulation sur des zéros n'apprendrait rien.
[[nodiscard]] std::optional<ro::ui::MonsterSheetOwnView> OwnOf(
    const ro::sim::PlayerStatus& status) {
  if (status.hit <= 0 && status.flee1 <= 0) {
    return std::nullopt;
  }
  const auto known = [](std::int64_t value) {
    return value == ro::sim::kUnknownStat ? 0 : static_cast<int>(value);
  };
  return ro::ui::MonsterSheetOwnView{.hit = known(status.hit),
                                     .flee = known(status.flee1),
                                     .critical_percent = known(status.critical),
                                     .perfect_dodge_percent = known(status.flee2)};
}

}  // namespace

MonsterSheet::MonsterSheet(ro::render::ActorWardrobe* wardrobe,
                           const ro::vfs::ItemWords* item_words, ro::render::ItemIcons* item_icons,
                           const ro::sim::SkillTree* skill_tree,
                           const ro::vfs::SkillWords* skill_words, ro::vfs::Vfs* models)
    : item_words_(item_words),
      item_icons_(item_icons),
      skill_tree_(skill_tree),
      skill_words_(skill_words) {
  if (wardrobe != nullptr) {
    sprite_.emplace(*wardrobe, models);
    preview_sprite_.emplace(*wardrobe);
  }
}

void MonsterSheet::Open(ro::ui::MonsterSheetWindowState& state, std::uint32_t monster_id,
                        bool by_view) {
  if (monster_id == 0) {
    return;
  }
  if (monster_id != shown_id_) {
    seen_map_.clear();
  }
  shown_id_ = monster_id;
  Asked& asked = asked_[monster_id];
  if (!asked.sent) {
    asked.by_view = by_view;
  }
  state.open = true;
  state.raise = true;
}

void MonsterSheet::Show(std::uint32_t monster_id) {
  if (monster_id == 0) {
    return;
  }
  if (monster_id != shown_id_) {
    seen_map_.clear();
  }
  shown_id_ = monster_id;
  // Le premier `operator[]` note une question par identifiant de base.
  static_cast<void>(asked_[monster_id]);
}

bool MonsterSheet::shown_by_view() const {
  const auto found = asked_.find(shown_id_);
  return found != asked_.end() && found->second.by_view;
}

void MonsterSheet::NoteBase(std::uint32_t monster_id, ro::sim::MonsterSheet sheet) {
  base_[monster_id] = std::move(sheet);
}

std::optional<MonsterSheet::Ask> MonsterSheet::NextAsk(const ro::sim::PlayerStatus& status,
                                                       bool sheet_open, std::uint32_t now_ms) {
  if (sheet_open && shown_id_ != 0) {
    if (std::optional<Ask> ask = AskFor(Aimed{.monster_id = shown_id_}, status, now_ms);
        ask.has_value()) {
      return ask;
    }
  }
  if (previewed_.has_value()) {
    if (std::optional<Ask> ask = AskFor(*previewed_, status, now_ms); ask.has_value()) {
      return ask;
    }
  }
  if (const std::optional<Aimed> wanted = std::exchange(wanted_, std::nullopt);
      wanted.has_value()) {
    return AskFor(*wanted, status, now_ms);
  }
  return std::nullopt;
}

void MonsterSheet::NoteWanted(std::uint32_t sprite_class) noexcept {
  if (sprite_class != 0) {
    wanted_ = Aimed{.monster_id = sprite_class, .by_view = true};
  }
}

std::optional<MonsterSheet::Ask> MonsterSheet::AskFor(const Aimed& aimed,
                                                      const ro::sim::PlayerStatus& status,
                                                      std::uint32_t now_ms) {
  const std::uint32_t monster_id = aimed.monster_id;
  if (monster_id == 0 || book_.IsUnknown(monster_id)) {
    return std::nullopt;
  }
  const bool first = !asked_.contains(monster_id);
  Asked& asked = asked_[monster_id];
  // ⚠ LA NATURE DE L'IDENTIFIANT EST CELLE DE LA PREMIÈRE QUESTION : c'est
  // elle qu'on repose. `Open` l'a notée pour la fenêtre ; ici, pour l'aperçu.
  if (first) {
    asked.by_view = aimed.by_view;
  }
  // LES NIVEAUX DE LA RÉPONSE SE NOTENT À SON ARRIVÉE : c'est l'état du
  // personnage pour lequel le serveur a calculé.
  if (const std::uint32_t arrival = book_.ArrivalOf(monster_id); arrival != asked.arrival) {
    asked.arrival = arrival;
    asked.base_level = status.base_level;
    asked.job_level = status.job_level;
  }
  const ro::sim::MonsterSheet* const sheet = book_.Find(monster_id);
  const bool exp_stale =
      sheet != nullptr && sheet->exp_gain.has_value() &&
      (asked.base_level != status.base_level || asked.job_level != status.job_level);
  if (sheet != nullptr && !exp_stale) {
    return std::nullopt;
  }
  if (asked.sent && now_ms - asked.at_ms < kAskPatienceMs) {
    return std::nullopt;
  }
  asked.sent = true;
  asked.at_ms = now_ms;
  return Ask{.monster_id = monster_id, .by_view = asked.by_view};
}

void MonsterSheet::NoteHovered(std::uint32_t monster_id, bool by_view, std::string_view hint) {
  if (monster_id != 0) {
    hovered_ = Aimed{.monster_id = monster_id, .by_view = by_view};
    hover_hint_.assign(hint);
  }
}

const ro::ui::MonsterSheetPreview* MonsterSheet::Preview(
    const ro::ui::MonsterSheetPreviewState& state, bool animate, std::uint32_t now_ms) {
  previewed_ = std::exchange(hovered_, std::nullopt);
  std::string hint = std::exchange(hover_hint_, std::string());
  if (!previewed_.has_value()) {
    return nullptr;
  }
  const std::uint32_t monster_id = previewed_->monster_id;
  preview_ = ro::ui::MonsterSheetPreview{};
  preview_.shown_id = monster_id;
  preview_.hint = std::move(hint);
  const ro::sim::MonsterSheet* const sheet = book_.Find(monster_id);
  if (sheet == nullptr) {
    preview_.phase = book_.IsUnknown(monster_id) ? ro::ui::MonsterSheetPhase::kUnknown
                                                 : ro::ui::MonsterSheetPhase::kAsking;
    return &preview_;
  }
  preview_.phase = ro::ui::MonsterSheetPhase::kReady;
  preview_.name = sheet->name;
  preview_.boss_rank = sheet->boss_rank;
  preview_.level = sheet->level;
  preview_.element_level = sheet->element_level;
  preview_.hp = sheet->hp;
  preview_.sp = sheet->sp;
  if (item_words_ != nullptr) {
    preview_.race_value = ro::vfs::MonsterRaceWord(*item_words_, sheet->race);
    preview_.element_value = ro::vfs::MonsterElementWord(*item_words_, sheet->element);
    preview_.size_value = ro::vfs::MonsterSizeWord(*item_words_, sheet->size);
  }
  // ⚠ LA POSE DE FACE, qui existe dans tout .act : l'aperçu ne se tourne pas.
  if (preview_sprite_.has_value()) {
    const MonsterSprite::Shown shown = preview_sprite_->Compose(
        MonsterSprite::Framing{
            .sprite_class = sheet->sprite_class != 0 ? sheet->sprite_class : monster_id,
            .direction = 0,
            .animate = animate,
            .box_width = static_cast<int>(state.sprite_box_width),
            .box_height = static_cast<int>(state.sprite_box_height)},
        now_ms);
    preview_.sprite = shown.image;
  }
  return &preview_;
}

std::optional<std::uint32_t> MonsterSheet::TakeSense(bool show_guardians) {
  if (book_.sense_readings() == sense_seen_) {
    return std::nullopt;
  }
  sense_seen_ = book_.sense_readings();
  const std::optional<ro::sim::SenseReading>& reading = book_.sense();
  if (!reading.has_value() || (!show_guardians && IsGuardian(reading->sprite_class))) {
    return std::nullopt;
  }
  sense_ = *reading;
  return reading->sprite_class;
}

void MonsterSheet::ComposeLists(const ro::sim::MonsterSheet& sheet) {
  drops_.clear();
  for (const ro::sim::MonsterDrop& drop : sheet.drops) {
    ro::ui::MonsterSheetDropView shown{.item_id = drop.item_id,
                                       .name = drop.name,
                                       .rate_hundredths = drop.rate_hundredths,
                                       .mvp_reward = drop.mvp_reward};
    drops_.push_back(std::move(shown));
  }
  spawns_.clear();
  for (const ro::sim::MonsterSpawn& spawn : sheet.spawns) {
    // ⚠ LA TABLE SE CONSULTE PAR LE NOM DE FICHIER, extension comprise.
    const std::optional<std::string_view> shown =
        map_names_ != nullptr
            ? map_names_->DisplayNameOf(spawn.map_name + std::string(kMapFileExtension))
            : std::nullopt;
    std::string label = shown.has_value() && !shown->empty()
                            ? std::string(*shown) + " (" + spawn.map_name + ")"
                            : spawn.map_name;
    spawns_.push_back(ro::ui::MonsterSheetSpawnView{
        .map_name = spawn.map_name, .label = std::move(label), .count = spawn.count});
  }
  skills_.clear();
  for (const ro::sim::MonsterSkill& skill : sheet.skills) {
    ro::ui::MonsterSheetSkillView shown{.skill_id = skill.skill_id, .level = skill.level};
    // 🔴 AUCUNE COMPÉTENCE N'EST ÉCARTÉE. Le nom de l'installation d'abord —
    // localisé —, celui du serveur ensuite, le numéro en dernier recours.
    const ro::sim::SkillNode* const node =
        skill_tree_ != nullptr ? skill_tree_->Find(skill.skill_id) : nullptr;
    if (node != nullptr && !node->display_name.empty()) {
      shown.name = node->display_name;
    } else if (!skill.server_name.empty()) {
      shown.name = skill.server_name;
    } else {
      shown.name = ro::core::Printed("#%u", static_cast<unsigned>(skill.skill_id));
    }
    // ⚠ LE NOM ET LA DESCRIPTION VIVENT DANS DEUX FICHIERS DIFFÉRENTS : une
    // compétence peut avoir l'un sans l'autre, et c'est le cas courant des
    // `NPC_*`. Seule la description décide du lien.
    shown.describable = skill_words_ != nullptr && !skill_words_->Of(skill.skill_id).empty();
    skills_.push_back(std::move(shown));
  }
}

const ro::ui::MonsterSheetView& MonsterSheet::Refresh(const ro::ui::MonsterSheetWindowState& state,
                                                      const ro::sim::PlayerStatus& status,
                                                      bool animate, bool can_post_link,
                                                      std::uint32_t now_ms) {
  view_ = ro::ui::MonsterSheetView{};
  view_.shown_id = shown_id_;
  if (sense_.has_value()) {
    view_.sense = ro::ui::MonsterSheetSenseView{.sprite_class = sense_->sprite_class,
                                                .level = sense_->level,
                                                .hp = sense_->hp,
                                                .def = sense_->def,
                                                .mdef = sense_->mdef};
  }
  // LA RÉPONSE DU SERVEUR D'ABORD, la fiche de la base sinon : une fiche
  // complète l'emporte sur une fiche partielle.
  const ro::sim::MonsterSheet* sheet = book_.Find(shown_id_);
  const auto base = base_.find(shown_id_);
  const bool from_base = sheet == nullptr && base != base_.end();
  if (from_base) {
    sheet = &base->second;
  }
  if (sheet == nullptr) {
    view_.phase = book_.IsUnknown(shown_id_) ? ro::ui::MonsterSheetPhase::kUnknown
                                             : ro::ui::MonsterSheetPhase::kAsking;
    return view_;
  }
  view_.phase = ro::ui::MonsterSheetPhase::kReady;
  view_.resist_known = !from_base;

  // La fiche de la base se range sous un rang à elle : la réponse qui la
  // remplace refait les listes.
  const std::uint32_t arrival = from_base ? kBaseArrival : book_.ArrivalOf(shown_id_);
  if (lists_for_id_ != shown_id_ || lists_for_arrival_ != arrival) {
    lists_for_id_ = shown_id_;
    lists_for_arrival_ = arrival;
    ComposeLists(*sheet);
  }
  // Les icônes se relisent à chaque image : le cache les garde, et une icône
  // téléversée plus tard doit apparaître sans attendre une nouvelle réponse.
  for (ro::ui::MonsterSheetDropView& drop : drops_) {
    drop.icon = {};
    if (item_icons_ != nullptr) {
      if (const ro::render::IconTexture& icon = item_icons_->Get(drop.item_id); icon.usable()) {
        drop.icon = SkinImageOf(icon);
      }
    }
  }

  view_.name = sheet->name;
  view_.qualifier = VariantQualifier(sheet->aegis_name, sheet->summoned);
  view_.boss_rank = sheet->boss_rank;
  view_.can_post_link = can_post_link;
  if (item_words_ != nullptr) {
    using ro::vfs::MonsterLabel;
    const ro::vfs::ItemWords& words = *item_words_;
    view_.words.level = ro::vfs::MonsterLabelWord(words, MonsterLabel::kLevel);
    view_.words.race = ro::vfs::MonsterLabelWord(words, MonsterLabel::kRace);
    view_.words.element = ro::vfs::MonsterLabelWord(words, MonsterLabel::kElement);
    view_.words.size = ro::vfs::MonsterLabelWord(words, MonsterLabel::kSize);
    view_.words.def = ro::vfs::MonsterLabelWord(words, MonsterLabel::kDef);
    view_.words.mdef = ro::vfs::MonsterLabelWord(words, MonsterLabel::kMdef);
    view_.words.race_value = ro::vfs::MonsterRaceWord(words, sheet->race);
    view_.words.element_value = ro::vfs::MonsterElementWord(words, sheet->element);
    view_.words.size_value = ro::vfs::MonsterSizeWord(words, sheet->size);
    for (std::size_t element = 0; element < ro::ui::kMonsterSheetResistCount; ++element) {
      view_.words.resist[element] =
          ro::vfs::MonsterElementWord(words, static_cast<std::uint8_t>(element));
    }
  }

  view_.level = sheet->level;
  view_.element_level = sheet->element_level;
  view_.mode = sheet->mode;
  view_.hp = sheet->hp;
  view_.sp = sheet->sp;
  view_.base_exp = sheet->base_exp;
  view_.job_exp = sheet->job_exp;
  view_.mvp_exp = sheet->mvp_exp;
  view_.atk_min = sheet->atk_min;
  view_.atk_max = sheet->atk_max;
  view_.matk_min = sheet->matk_min;
  view_.matk_max = sheet->matk_max;
  view_.def = sheet->def;
  view_.def_bonus = sheet->def_bonus;
  view_.mdef = sheet->mdef;
  view_.mdef_bonus = sheet->mdef_bonus;
  view_.str = sheet->str;
  view_.agi = sheet->agi;
  view_.vit = sheet->vit;
  view_.int_stat = sheet->int_stat;
  view_.dex = sheet->dex;
  view_.luk = sheet->luk;
  view_.attack_range_cells = sheet->attack_range_cells;
  view_.walk_ms_per_cell = sheet->walk_ms_per_cell;
  std::ranges::copy(sheet->resist_percent, view_.resist_percent.begin());
  if (sheet->exp_gain.has_value()) {
    const ro::sim::MonsterExpGain& gain = *sheet->exp_gain;
    view_.exp_gain = ro::ui::MonsterSheetExpView{.base_exp = gain.base_exp,
                                                 .job_exp = gain.job_exp,
                                                 .next_base_exp = gain.next_base_exp,
                                                 .next_job_exp = gain.next_job_exp,
                                                 .base_level_maxed = gain.base_level_maxed,
                                                 .job_level_maxed = gain.job_level_maxed};
  }
  view_.own = OwnOf(status);
  // L'EXP ÉCRITE, que seule la base sait : celle de la fiche montrée, ou de sa
  // fiche de base quand le serveur a répondu.
  const std::optional<ro::sim::MonsterWrittenExp> written =
      sheet->written_exp.has_value() ? sheet->written_exp
      : base != base_.end()          ? base->second.written_exp
                                     : std::nullopt;
  if (written.has_value()) {
    view_.written_exp =
        ro::ui::MonsterSheetWrittenExpView{.base_exp = written->base_exp,
                                           .job_exp = written->job_exp,
                                           .mvp_exp = written->mvp_exp,
                                           .base_rate_percent = written->base_rate_percent,
                                           .job_rate_percent = written->job_rate_percent,
                                           .mvp_rate_percent = written->mvp_rate_percent};
  }
  view_.aegis_name = sheet->aegis_name;
  view_.namesake_count = sheet->namesake_count;
  view_.namesake_origin = sheet->namesake_origin;
  view_.sprite_class = sheet->sprite_class;
  view_.drops = drops_;
  view_.spawns = spawns_;
  view_.skills = skills_;

  // ── LE SPRITE ─────────────────────────────────────────────────────────────
  //
  // Depuis la classe de VUE que le serveur renvoie, pas depuis l'identifiant
  // demandé : un monstre déguisé porte le sprite d'un autre.
  if (sprite_.has_value()) {
    const std::uint32_t sprite_class = sheet->sprite_class != 0 ? sheet->sprite_class : shown_id_;
    const MonsterSprite::Shown shown = sprite_->Compose(
        MonsterSprite::Framing{.sprite_class = sprite_class,
                               .direction = state.sprite_direction,
                               .animate = animate,
                               .box_width = static_cast<int>(state.sprite_box_width),
                               .box_height = static_cast<int>(state.sprite_box_height)},
        now_ms);
    view_.sprite = shown.image;
    view_.sprite_shake_px = shown.shake_px;
    view_.sprite_missing = shown.missing;
    view_.sprite_is_model = shown.model;
  } else {
    view_.sprite_missing = true;
  }
  return view_;
}

bool MonsterSheet::ForgetOnMapEntry(std::uint32_t map_entries) {
  if (map_entries == map_entries_) {
    return false;
  }
  map_entries_ = map_entries;
  Forget();
  return true;
}

void MonsterSheet::Forget() {
  book_.Clear();
  asked_.clear();
  shown_id_ = 0;
  seen_map_.clear();
  sense_.reset();
  lists_for_id_ = 0;
  lists_for_arrival_ = 0;
  drops_.clear();
  spawns_.clear();
  skills_.clear();
  view_ = ro::ui::MonsterSheetView{};
  hovered_.reset();
  hover_hint_.clear();
  previewed_.reset();
  wanted_.reset();
  preview_ = ro::ui::MonsterSheetPreview{};
  if (sprite_.has_value()) {
    sprite_->Forget();
  }
  if (preview_sprite_.has_value()) {
    preview_sprite_->Forget();
  }
}

}  // namespace moonclient
