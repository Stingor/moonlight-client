#include "game/character_bug_report.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <format>
#include <optional>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/core/text.h"

namespace moonclient {
namespace {

using ro::ui::CharacterTab;
// LE PLAFOND DES CHAMPS, en octets : les listes s'arrêtent avant lui. Avec la
// révision du client, le contexte reste loin des 1024 octets que le serveur
// garde.
constexpr std::size_t kFieldsBytes = 760;
// Un texte libre — un compte rendu du serveur, un refus — n'en garde pas plus.
constexpr std::size_t kNoteBytes = 96;

// Les emplacements, au rang de `EquipSlot`.
constexpr std::array<const char*, ro::ui::kEquipSlotCount> kSlotWords = {
    "head_top", "head_low",        "garment", "accessory_left", "head_mid", "armor",
    "shoes",    "accessory_right", "weapon",  "shield",         "ammo"};

[[nodiscard]] const char* TabWord(CharacterTab tab) {
  switch (tab) {
    case CharacterTab::kEquipment:
      return "equipment";
    case CharacterTab::kCostume:
      return "costume";
    case CharacterTab::kPresets:
      return "presets";
    case CharacterTab::kTitles:
      return "titles";
    case CharacterTab::kGuild:
      return "guild";
    case CharacterTab::kGrimoire:
      return "grimoire";
    case CharacterTab::kHomunculus:
      return "homunculus";
    case CharacterTab::kMercenary:
      return "mercenary";
  }
  return "?";
}

// Le nom de l'onglet tel que la languette l'écrit.
[[nodiscard]] const char* TabLabel(CharacterTab tab) {
  switch (tab) {
    case CharacterTab::kEquipment:
      return ro::core::Tr("Équipement");
    case CharacterTab::kCostume:
      return ro::core::Tr("Costume");
    case CharacterTab::kPresets:
      return ro::core::Tr("Presets");
    case CharacterTab::kTitles:
      return ro::core::Tr("Titres");
    case CharacterTab::kGuild:
      return ro::core::Tr("Guilde");
    case CharacterTab::kGrimoire:
      return ro::core::Tr("Grimoire");
    case CharacterTab::kHomunculus:
      return ro::core::Tr("Homoncule");
    case CharacterTab::kMercenary:
      return ro::core::Tr("Mercenaire");
  }
  return "?";
}

// Un drapeau qu'on ne connaît pas encore s'écrit `null`.
[[nodiscard]] std::string JsonFlagOrNull(const std::optional<bool>& value) {
  return value.has_value() ? std::string(JsonFlag(*value)) : std::string("null");
}

// Un texte libre, coupé à `kNoteBytes` sur une frontière de caractère, puis
// échappé.
[[nodiscard]] std::string Note(std::string_view text) {
  return JsonEscaped(ro::core::Utf8Prefix(text, kNoteBytes));
}

// Un tampon terminé par un zéro, jusqu'à lui et jamais au-delà.
[[nodiscard]] std::string_view Text(const char* buffer, std::size_t bytes) {
  return {buffer, static_cast<std::size_t>(std::find(buffer, buffer + bytes, '\0') - buffer)};
}
template <std::size_t kBytes>
[[nodiscard]] std::string_view Text(const std::array<char, kBytes>& buffer) {
  return Text(buffer.data(), kBytes);
}
template <std::size_t kBytes>
[[nodiscard]] std::string_view Text(const char (&buffer)[kBytes]) {
  return Text(buffer, kBytes);
}

// UNE LISTE BORNÉE : chaque entrée s'ajoute tant que le tout tient sous le
// plafond des champs.
class BoundedList {
 public:
  explicit BoundedList(const std::string& fields) : fields_(fields) {}

  void Add(const std::string& entry) {
    if (cut_) {
      return;
    }
    const std::size_t separator = items_.empty() ? 0 : 1;
    if (fields_.size() + items_.size() + separator + entry.size() > kFieldsBytes) {
      cut_ = true;
      return;
    }
    if (separator != 0) {
      items_ += ',';
    }
    items_ += entry;
  }

  // `"clé":[…],"clé_cut":…`
  [[nodiscard]] std::string Close(std::string_view key) const {
    return std::format(R"("{}":[{}],"{}_cut":{})", key, items_, key, JsonFlag(cut_));
  }

 private:
  const std::string& fields_;
  std::string items_;
  bool cut_ = false;
};

// Le mannequin, commun à l'équipement et au costume.
void DollFields(const CharacterBugScene& scene, std::string& fields) {
  const ro::ui::CharacterWindowState& window = *scene.window;
  fields += std::format(R"(,"stats_shown":{})", JsonFlag(window.stats_shown));
  if (scene.view != nullptr) {
    fields += std::format(R"(,"equipment_shown":{},"costumes_shown":{})",
                          JsonFlagOrNull(scene.view->equipment_shown),
                          JsonFlagOrNull(scene.view->costumes_shown));
  }
  fields += R"(,"slots":{)";
  bool first = true;
  const std::size_t count = std::min(scene.slots.size(), kSlotWords.size());
  for (std::size_t rank = 0; rank < count; ++rank) {
    const ro::ui::EquipSlotView& slot = scene.slots[rank];
    if (!slot.filled()) {
      continue;
    }
    fields += std::format(R"({}"{}":[{},{}])", first ? "" : ",", kSlotWords[rank], slot.item_id,
                          slot.refine_level);
    first = false;
  }
  fields += '}';
  if (scene.stats == nullptr) {
    return;
  }
  const ro::ui::CharacterStatsView& stats = *scene.stats;
  fields += R"(,"attributes":[)";
  for (std::size_t rank = 0; rank < stats.attributes.size(); ++rank) {
    fields += std::format("{}[{},{}]", rank == 0 ? "" : ",", stats.attributes[rank].base,
                          stats.attributes[rank].bonus);
  }
  fields += std::format(
      R"(],"status_point":{},"atk":[{},{}],"matk":[{},{}],"def":[{},{}],"mdef":[{},{}],)"
      R"("hit":{},"flee":[{},{}],"critical":{},"aspd":{})",
      stats.status_point, stats.atk1, stats.atk2, stats.matk1, stats.matk2, stats.def1, stats.def2,
      stats.mdef1, stats.mdef2, stats.hit, stats.flee1, stats.flee2, stats.critical, stats.aspd);
}

void PresetFields(const CharacterBugScene& scene, std::string& fields) {
  const ro::ui::PresetTabState& state = scene.window->presets;
  fields += std::format(R"(,"capturing":{},"refusal":"{}","status":"{}")", state.capturing,
                        Note(Text(state.refusal)), Note(Text(state.status)));
  if (scene.presets == nullptr) {
    return;
  }
  fields += std::format(R"(,"capacity":{},"count":{},)", scene.presets->capacity,
                        scene.presets->presets.size());
  BoundedList rows(fields);
  for (const ro::ui::PresetRowView& row : scene.presets->presets) {
    rows.Add(std::format(R"(["{}",{},{},"{}"])", Note(row.name), row.pieces.size(),
                         JsonFlag(row.with_costumes), Note(row.hotkey)));
  }
  fields += rows.Close("presets");
}

void TitleFields(const CharacterBugScene& scene, std::string& fields) {
  fields += std::format(R"(,"filter":"{}")", Note(Text(scene.window->title_filter)));
  if (scene.titles != nullptr) {
    fields += std::format(R"(,"equipped":{},"owned":{})", scene.titles->equipped,
                          scene.titles->owned.size());
  }
}

void GuildFields(const CharacterBugScene& scene, std::string& fields) {
  fields += std::format(R"(,"status":"{}")", Note(scene.window->guild.status));
  if (scene.guild == nullptr) {
    return;
  }
  const ro::ui::GuildView& guild = *scene.guild;
  fields += std::format(R"(,"in_guild":{})", JsonFlag(guild.in_guild));
  if (!guild.in_guild) {
    return;
  }
  fields += std::format(
      R"(,"guild_id":{},"name":"{}","level":{},"members":[{},{}],"online":{},"master":{},)"
      R"("position":"{}","skill_points":{})",
      guild.guild_id, Note(guild.name), guild.level, guild.member_count, guild.member_most,
      guild.online, JsonFlag(guild.master), Note(guild.my_position), guild.skill_points);
}

void GrimoireFields(const CharacterBugScene& scene, std::string& fields) {
  const ro::ui::SkillWindowState& grimoire = scene.window->grimoire;
  std::string_view group_label;
  if (grimoire.group >= 0 && static_cast<std::size_t>(grimoire.group) < scene.groups.size()) {
    group_label = scene.groups[static_cast<std::size_t>(grimoire.group)].label;
  }
  fields +=
      std::format(R"(,"group":{},"group_label":"{}","groups":{},"filter":"{}","points_left":{},)"
                  R"("points_reserved":{},"grid":{},"keyboard":{},)",
                  grimoire.group, Note(group_label), scene.groups.size(),
                  Note(Text(grimoire.filter)), scene.points.left, scene.points.reserved,
                  JsonFlag(grimoire.grid), JsonFlag(grimoire.keyboard_layout));
  // LES RÉSERVATIONS : `[skill, appris, visé]`, tous groupes confondus — ce sont
  // elles que « Appliquer » enverra.
  BoundedList reserved(fields);
  for (const ro::ui::SkillGroupView& group : scene.groups) {
    for (const ro::ui::SkillCellView& cell : group.cells) {
      if (cell.reserved > 0) {
        reserved.Add(std::format("[{},{},{}]", cell.skill_id, cell.learned, cell.reserved));
      }
    }
  }
  fields += reserved.Close("reserved");
}

void HomunculusFields(const CharacterBugScene& scene, std::string& fields) {
  fields += std::format(R"(,"status":"{}")", Note(scene.window->homunculus.status));
  if (scene.companions == nullptr) {
    return;
  }
  const ro::ui::HomunculusTabView& homunculus = scene.companions->homunculus;
  fields +=
      std::format(R"(,"name":"{}","species":"{}","level":{},"alive":{},"resting":{},"hp":[{},{}],)"
                  R"("sp":[{},{}],"hunger":{},"intimacy":{},"skill_points":{},"ai_profile":"{}")",
                  Note(homunculus.name), Note(homunculus.species), homunculus.level,
                  JsonFlag(homunculus.alive), JsonFlag(homunculus.resting), homunculus.hp,
                  homunculus.max_hp, homunculus.sp, homunculus.max_sp, homunculus.hunger,
                  homunculus.intimacy, homunculus.skill_points, Note(homunculus.ai_profile));
}

void MercenaryFields(const CharacterBugScene& scene, std::string& fields) {
  fields += std::format(R"(,"status":"{}")", Note(scene.window->mercenary.status));
  if (scene.companions == nullptr) {
    return;
  }
  const ro::ui::MercenaryTabView& mercenary = scene.companions->mercenary;
  fields += std::format(
      R"(,"kind":"{}","level":{},"hp":[{},{}],"sp":[{},{}],"contract_left_s":{},"faith":{},)"
      R"("calls":{},"kills":{},"ai_profile":"{}")",
      Note(mercenary.kind), mercenary.level, mercenary.hp, mercenary.max_hp, mercenary.sp,
      mercenary.max_sp, mercenary.contract_left_s, mercenary.faith, mercenary.calls,
      mercenary.kills, Note(mercenary.ai_profile));
}

}  // namespace

BugReportContext CharacterBugReport(const CharacterBugScene& scene) {
  const ro::ui::CharacterWindowState& window = *scene.window;
  std::string fields = std::format(R"("character":{{"tab":"{}","staff_pane":{})",
                                   TabWord(window.tab), JsonFlag(window.staff_shown));
  if (scene.header != nullptr) {
    const ro::ui::CharacterHeaderView& header = *scene.header;
    fields += std::format(R"(,"job":"{}","base_level":{},"job_level":{},"hp":[{},{}],"sp":[{},{}])",
                          Note(header.job), header.base_level, header.job_level, header.hp,
                          header.max_hp, header.sp, header.max_sp);
  }
  fields += std::format(R"(,"{}":{{)", TabWord(window.tab));
  // ⚠ L'ACCOLADE OUVERTE N'A ENCORE RIEN : chaque onglet pose ses membres
  // précédés d'une virgule, et la première se retire ensuite.
  const std::size_t opened = fields.size();
  switch (window.tab) {
    case CharacterTab::kEquipment:
    case CharacterTab::kCostume:
      DollFields(scene, fields);
      break;
    case CharacterTab::kPresets:
      PresetFields(scene, fields);
      break;
    case CharacterTab::kTitles:
      TitleFields(scene, fields);
      break;
    case CharacterTab::kGuild:
      GuildFields(scene, fields);
      break;
    case CharacterTab::kGrimoire:
      GrimoireFields(scene, fields);
      break;
    case CharacterTab::kHomunculus:
      HomunculusFields(scene, fields);
      break;
    case CharacterTab::kMercenary:
      MercenaryFields(scene, fields);
      break;
  }
  if (fields.size() > opened && fields[opened] == ',') {
    fields.erase(opened, 1);
  }
  fields += "}}";

  std::array<char, kBugReportLabelBytes> label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("Personnage : onglet %s"),
                TabLabel(window.tab));
  return FieldsBugReport(label.data(), fields);
}

}  // namespace moonclient
