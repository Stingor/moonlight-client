#include "items/item_sheet_editor_stage.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/printed.h"
#include "ro/vfs/encoding.h"

#include "game/skin_image_of.h"

namespace moonclient {
namespace {

constexpr const char* kNoTintLabel = ro::core::TrText("Aucune");
// Les noms des deux bons, tels que la notice les dit.
constexpr const char* kItemInfoDeltaName = "item_info_delta.yml";
constexpr const char* kItemDbDeltaName = "item_db_delta.yml";
// `Refineable` dans le bon, comme le serveur l'écrit.
constexpr const char* kTrueWord = "true";
constexpr const char* kFalseWord = "false";

// Un entier écrit dans un bon, ou `fallback` s'il n'en est pas un.
[[nodiscard]] int WholeOr(std::string_view text, int fallback) {
  int value = 0;
  const auto [end, failure] = std::from_chars(text.data(), text.data() + text.size(), value);
  return failure == std::errc() && end == text.data() + text.size() ? value : fallback;
}

// La ressource s'écrit-elle en CP949, à l'identique ? C'est l'encodage des
// archives : un caractère remplacé désignerait un autre fichier.
[[nodiscard]] bool ArchivableResource(std::string_view utf8) {
  const auto archived = ro::vfs::Utf8ToCp949(utf8);
  if (!archived) {
    return false;
  }
  const auto back = ro::vfs::Cp949ToUtf8(*archived);
  return back && *back == utf8;
}

}  // namespace

int ItemSheetTintRank(std::string_view word) {
  const auto& shades = ro::formats::kIconTintShades;
  for (std::size_t rank = 0; rank < shades.size(); ++rank) {
    if (shades[rank].word == word) {
      return static_cast<int>(rank) + 1;
    }
  }
  return 0;
}

std::string_view ItemSheetTintWord(int rank) {
  const auto& shades = ro::formats::kIconTintShades;
  if (rank <= 0 || static_cast<std::size_t>(rank) > shades.size()) {
    return {};
  }
  return shades[static_cast<std::size_t>(rank) - 1].word;
}

ItemInfoDeltaEntry ItemSheetEdit(std::uint32_t item_id, const ro::vfs::ItemInfoRecord& table,
                                 const ro::vfs::ItemInfoRecord& typed) {
  ItemInfoDeltaEntry entry;
  entry.item_id = item_id;
  if (typed.name != table.name) {
    entry.name = typed.name;
  }
  if (typed.resource != table.resource) {
    entry.resource = typed.resource;
  }
  if (typed.tint != table.tint) {
    entry.tint = typed.tint;
  }
  if (typed.description != table.description) {
    entry.description = typed.description;
  }
  return entry;
}

const ro::vfs::ItemInfoRecord* ItemSheetEditorStage::TableRecord(std::uint32_t item_id,
                                                                 ro::vfs::Vfs* vfs) {
  if (record_for_ != item_id) {
    record_for_ = item_id;
    record_.reset();
    // ⚠ TOUTE LA TABLE SE RELIT, pour un objet : le lecteur la parcourt d'un
    // bout à l'autre, et c'est le prix d'un geste du staff, pas d'une image.
    if (vfs == nullptr) {
      return nullptr;
    }
    const auto bytes = vfs->Read(std::string(ro::vfs::kItemInfoPath));
    if (bytes) {
      const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
      const auto read = ro::vfs::ReadItemInfoText(text, [&](ro::vfs::ItemInfoRecord&& record) {
        if (record.item_id == item_id) {
          record_ = std::move(record);
        }
        return ro::core::Result<void>();
      });
      if (!read) {
        ro::core::LogWarning("fiche d'objet : table illisible ({})", read.error().detail);
      }
    }
  }
  return record_.has_value() ? &*record_ : nullptr;
}

void ItemSheetEditorStage::Notice(std::string text, bool ok) {
  notice_ = std::move(text);
  notice_ok_ = ok;
}

void ItemSheetEditorStage::Save(ro::ui::ItemSheetEditorState& state,
                                const ro::vfs::ItemInfoRecord& table,
                                const ro::vfs::ServerItem* server) {
  ro::vfs::ItemInfoRecord typed;
  typed.name = state.name.data();
  typed.resource = state.resource.data();
  typed.tint = std::string(ItemSheetTintWord(state.tint_rank));
  typed.description = ro::ui::ItemSheetEditorLines(state.description.data());
  if (typed.name.empty() && !table.name.empty()) {
    Notice(ro::core::Tr("Le nom ne peut pas être vide."), false);
    return;
  }
  if (!ArchivableResource(typed.resource)) {
    Notice(ro::core::Tr("Cette ressource ne s'écrit pas en CP949, l'encodage des archives."),
           false);
    return;
  }
  // CHAQUE BON SE POSE OU SE RETIRE : une saisie qui redit la table retire le
  // bon d'avant. Celui du serveur ne se touche que si l'onglet a été rempli.
  const ItemInfoDeltaEntry entry = ItemSheetEdit(state.item_id, table, typed);
  bool written = entry.empty() ? delta_->Clear(state.item_id) : delta_->Set(entry);
  std::string files = entry.empty() ? std::string() : std::string(kItemInfoDeltaName);
  if (server != nullptr && state.server_filled) {
    const ItemDbDeltaEntry server_entry =
        ItemSheetServerEdit(state.item_id, ItemSheetServerBase(*server), state);
    written = (server_entry.fields.empty() ? server_delta_->Clear(state.item_id)
                                           : server_delta_->Set(server_entry)) &&
              written;
    if (!server_entry.fields.empty()) {
      files += files.empty() ? "" : ", ";
      files += kItemDbDeltaName;
    }
  }
  if (!written) {
    Notice(ro::core::Tr("Le bon n'a pas pu s'écrire : voir le journal."), false);
  } else if (files.empty()) {
    Notice(ro::core::Tr("La saisie redit la table : aucun bon pour cet item."), true);
  } else {
    Notice(ro::core::Printed(ro::core::Tr("Bon enregistré dans savedata : %s."), files.c_str()),
           true);
  }
  state.filled = false;
  state.server_filled = false;
}

void ItemSheetEditorStage::ComposeServer(std::uint32_t item_id,
                                         const ItemSheetEditorContext& context) {
  view_.server_loading = context.server_db == nullptr;
  const ro::vfs::ServerItem* const item =
      context.server_db != nullptr ? context.server_db->FindItem(item_id) : nullptr;
  view_.server_known = item != nullptr;
  const ItemDbDeltaEntry* const pending = server_delta_->Find(item_id);
  view_.server_pending = pending != nullptr;
  view_.server_pending_count = server_delta_->entries().size();
  if (item == nullptr) {
    return;
  }
  // CE QUE L'ONGLET MONTRE EN S'OUVRANT : le bon en attente par-dessus la base.
  ro::ui::ItemSheetServerFields start = ItemSheetServerBase(*item);
  const auto& numbers = ro::ui::kItemSheetServerNumbers;
  const auto& scripts = ro::ui::kItemSheetServerScripts;
  for (std::size_t rank = 0; rank < scripts.size(); ++rank) {
    server_scripts_[rank] = std::string(start.scripts[rank]);
  }
  const std::vector<std::pair<std::string, std::string>> no_fields;
  for (const auto& [key, value] : pending != nullptr ? pending->fields : no_fields) {
    if (key == ro::ui::kItemSheetRefineableKey) {
      start.refineable = value == kTrueWord;
    }
    for (std::size_t rank = 0; rank < numbers.size(); ++rank) {
      if (numbers[rank].key == key) {
        start.numbers[rank] = WholeOr(value, start.numbers[rank]);
      }
    }
    for (std::size_t rank = 0; rank < scripts.size(); ++rank) {
      if (scripts[rank].key == key) {
        server_scripts_[rank] = value;
      }
    }
  }
  for (std::size_t rank = 0; rank < scripts.size(); ++rank) {
    start.scripts[rank] = server_scripts_[rank];
  }
  view_.server_start = start;
}

int ItemSheetServerNumberOf(const ro::vfs::ServerItem& item, std::string_view key) {
  const auto bounded = [](std::uint64_t value) {
    return static_cast<int>(std::min<std::uint64_t>(value, ro::ui::kItemSheetMostNumber));
  };
  if (key == "Buy") {
    return bounded(item.buy_zeny);
  }
  if (key == "Sell") {
    return bounded(item.sell_zeny);
  }
  if (key == "Weight") {
    return bounded(item.weight_tenths);
  }
  if (key == "Attack") {
    return bounded(item.attack);
  }
  if (key == "MagicAttack") {
    return bounded(item.magic_attack);
  }
  if (key == "Defense") {
    return bounded(item.defense);
  }
  if (key == "Range") {
    return bounded(item.range_cells);
  }
  if (key == "Slots") {
    return bounded(item.slots);
  }
  if (key == "WeaponLevel") {
    return bounded(item.weapon_level);
  }
  if (key == "ArmorLevel") {
    return bounded(item.armor_level);
  }
  if (key == "EquipLevelMin") {
    return bounded(item.equip_level_min);
  }
  if (key == "EquipLevelMax") {
    return bounded(item.equip_level_max);
  }
  return 0;
}

ro::ui::ItemSheetServerFields ItemSheetServerBase(const ro::vfs::ServerItem& item) {
  ro::ui::ItemSheetServerFields base;
  for (std::size_t rank = 0; rank < ro::ui::kItemSheetServerNumbers.size(); ++rank) {
    base.numbers[rank] = ItemSheetServerNumberOf(item, ro::ui::kItemSheetServerNumbers[rank].key);
  }
  base.refineable = item.refineable;
  base.scripts = {item.script, item.equip_script, item.unequip_script};
  return base;
}

ItemDbDeltaEntry ItemSheetServerEdit(std::uint32_t item_id,
                                     const ro::ui::ItemSheetServerFields& base,
                                     const ro::ui::ItemSheetEditorState& typed) {
  ItemDbDeltaEntry entry;
  entry.item_id = item_id;
  for (std::size_t rank = 0; rank < ro::ui::kItemSheetServerNumbers.size(); ++rank) {
    if (typed.numbers[rank] != base.numbers[rank]) {
      entry.fields.emplace_back(std::string(ro::ui::kItemSheetServerNumbers[rank].key),
                                std::to_string(typed.numbers[rank]));
    }
  }
  if (typed.refineable != base.refineable) {
    entry.fields.emplace_back(std::string(ro::ui::kItemSheetRefineableKey),
                              typed.refineable ? kTrueWord : kFalseWord);
  }
  for (std::size_t rank = 0; rank < ro::ui::kItemSheetServerScripts.size(); ++rank) {
    const std::string_view script(typed.scripts[rank].data());
    if (script != base.scripts[rank]) {
      entry.fields.emplace_back(std::string(ro::ui::kItemSheetServerScripts[rank].key),
                                std::string(script));
    }
  }
  return entry;
}

void ItemSheetEditorStage::Compose(const ro::ui::ItemSheetEditorState& state,
                                   const ItemSheetEditorContext& context) {
  const std::uint32_t item_id = state.item_id;
  const ItemInfoDeltaEntry* const pending = delta_->Find(item_id);
  // CE QUE LA SAISIE MONTRE EN S'OUVRANT : le bon en attente par-dessus la table.
  start_ = record_.has_value() ? *record_ : ro::vfs::ItemInfoRecord{.item_id = item_id};
  if (pending != nullptr) {
    start_.name = pending->name.value_or(start_.name);
    start_.resource = pending->resource.value_or(start_.resource);
    start_.tint = pending->tint.value_or(start_.tint);
    start_.description = pending->description.value_or(start_.description);
  }
  // EN FRANÇAIS : la liste les traduit à la lecture.
  tint_labels_[0] = kNoTintLabel;
  for (std::size_t rank = 0; rank < ro::formats::kIconTintShades.size(); ++rank) {
    tint_labels_[rank + 1] = ro::formats::kIconTintShades[rank].label;
  }

  view_ = ro::ui::ItemSheetEditorView{};
  view_.item_id = item_id;
  view_.known = record_.has_value();
  view_.start = ro::ui::ItemSheetEditorFields{.name = start_.name,
                                              .resource = start_.resource,
                                              .tint_rank = ItemSheetTintRank(start_.tint),
                                              .description = start_.description};
  view_.tint_labels = tint_labels_;
  view_.pending = pending != nullptr;
  view_.pending_count = delta_->entries().size();
  if (notice_for_ != item_id) {
    notice_for_ = item_id;
    notice_.clear();
  }
  view_.notice = notice_;
  view_.notice_ok = notice_ok_;
  if (context.icons != nullptr) {
    view_.table_icon = SizedSkinImageOf(context.icons->Get(item_id));
    // L'APERÇU suit la saisie : la ressource tapée, sous la teinte choisie.
    const auto archived = ro::vfs::Utf8ToCp949(state.resource.data());
    if (archived.has_value()) {
      const ro::formats::IconTint tint =
          ro::formats::IconTintFromWord(ItemSheetTintWord(state.tint_rank))
              .value_or(ro::formats::IconTint::kNone);
      view_.preview_icon = SizedSkinImageOf(context.icons->Preview(*archived, tint));
    }
    // L'ILLUSTRATION, dans l'ordre de la description : le dessin de la carte,
    // sinon la collection — celle de la ressource saisie, puisque c'est elle
    // qui la nomme.
    if (const ro::render::IconTexture& drawn = context.icons->GetCardIllustration(item_id);
        drawn.usable()) {
      view_.illustration = SizedSkinImageOf(drawn);
      view_.illustration_is_card = true;
    } else if (archived.has_value()) {
      view_.illustration = SizedSkinImageOf(context.icons->PreviewCollection(*archived));
    }
  }
}

void ItemSheetEditorStage::Advance(ro::ui::StaffToolsState& tools,
                                   const ItemSheetEditorContext& context) {
  const ro::ui::ItemSheetEditorAnswer answer = std::exchange(tools.item_editor_answer, {});
  ro::ui::ItemSheetEditorState& state = tools.item_editor;
  if (!context.allowed || !state.open || state.item_id == 0 || context.client_root == nullptr) {
    tools.item_editor_view = nullptr;
    return;
  }
  if (!delta_.has_value()) {
    delta_.emplace(*context.client_root);
    server_delta_.emplace(*context.client_root);
  }
  const ro::vfs::ItemInfoRecord* const known = TableRecord(state.item_id, context.vfs);
  const ro::vfs::ItemInfoRecord table =
      known != nullptr ? *known : ro::vfs::ItemInfoRecord{.item_id = state.item_id};
  const ro::vfs::ServerItem* const server =
      context.server_db != nullptr ? context.server_db->FindItem(state.item_id) : nullptr;
  if (answer.save) {
    Save(state, table, server);
  }
  if (answer.clear_pending) {
    const bool cleared = delta_->Clear(state.item_id) && server_delta_->Clear(state.item_id);
    Notice(cleared ? ro::core::Tr("Bon annulé : rien ne change.")
                   : ro::core::Tr("Le bon n'a pas pu s'écrire : voir le journal."),
           cleared);
    state.filled = false;
    state.server_filled = false;
  }
  Compose(state, context);
  ComposeServer(state.item_id, context);
  tools.item_editor_view = &view_;
}

}  // namespace moonclient
