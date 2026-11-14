#include "items/item_sheet_editor_stage.h"

#include <cstddef>
#include <string_view>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/vfs/encoding.h"

#include "game/skin_image_of.h"

namespace moonclient {
namespace {

constexpr const char* kNoTintLabel = ro::core::TrText("Aucune");

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
                                const ro::vfs::ItemInfoRecord& table) {
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
  const ItemInfoDeltaEntry entry = ItemSheetEdit(state.item_id, table, typed);
  if (entry.empty()) {
    const bool cleared = delta_->Clear(state.item_id);
    Notice(cleared ? ro::core::Tr("La saisie redit la table : aucun bon pour cet item.")
                   : ro::core::Tr("Le bon n'a pas pu s'écrire : voir le journal."),
           cleared);
  } else {
    const bool written = delta_->Set(entry);
    Notice(written ? ro::core::Tr("Bon enregistré dans savedata\\item_info_delta.yml.")
                   : ro::core::Tr("Le bon n'a pas pu s'écrire : voir le journal."),
           written);
  }
  state.filled = false;
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
    if (auto archived = ro::vfs::Utf8ToCp949(state.resource.data()); archived) {
      const ro::formats::IconTint tint =
          ro::formats::IconTintFromWord(ItemSheetTintWord(state.tint_rank))
              .value_or(ro::formats::IconTint::kNone);
      view_.preview_icon = SizedSkinImageOf(context.icons->Preview(*archived, tint));
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
  }
  const ro::vfs::ItemInfoRecord* const known = TableRecord(state.item_id, context.vfs);
  const ro::vfs::ItemInfoRecord table =
      known != nullptr ? *known : ro::vfs::ItemInfoRecord{.item_id = state.item_id};
  if (answer.save) {
    Save(state, table);
  }
  if (answer.clear_pending) {
    const bool cleared = delta_->Clear(state.item_id);
    Notice(cleared ? ro::core::Tr("Bon annulé : la table reste telle quelle.")
                   : ro::core::Tr("Le bon n'a pas pu s'écrire : voir le journal."),
           cleared);
    state.filled = false;
  }
  Compose(state, context);
  tools.item_editor_view = &view_;
}

}  // namespace moonclient
