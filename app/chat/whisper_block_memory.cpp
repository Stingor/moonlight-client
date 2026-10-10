// La liste de blocage d'une connexion à l'autre : voir l'en-tête.

#include "chat/whisper_block_memory.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "ro/core/log.h"

namespace moonclient {

using ro::sim::WhisperBlockChange;
using ro::sim::WhisperBlockChangeKind;

WhisperBlockMemory::WhisperBlockMemory(web::WebTransport* transport,
                                       web::UserConfigIdentity identity) {
  if (transport == nullptr) {
    GiveUp("aucune adresse « web » dans moonclient.yml");
    return;
  }
  if (!identity.usable()) {
    GiveUp("la connexion n'a pas donné de jeton web");
    return;
  }
  web_.emplace(*transport, std::move(identity));
  web_->Load();
  stage_ = Stage::kLoading;
}

void WhisperBlockMemory::GiveUp(std::string_view why) {
  stage_ = Stage::kSessionOnly;
  early_changes_.clear();
  if (warned_) {
    return;
  }
  warned_ = true;
  ro::core::LogWarning("liste de blocage : {} — elle ne vit que la session", why);
}

bool WhisperBlockMemory::Follow(const WhisperBlockChange& change) {
  switch (change.kind) {
    case WhisperBlockChangeKind::kAdded:
      if (change.name.empty() || std::ranges::find(names_, change.name) != names_.end()) {
        return false;
      }
      names_.push_back(change.name);
      return true;
    case WhisperBlockChangeKind::kRemoved:
      return std::erase(names_, change.name) > 0;
    case WhisperBlockChangeKind::kEmptied: {
      const bool had = !names_.empty();
      names_.clear();
      return had;
    }
  }
  return false;
}

void WhisperBlockMemory::Digest(const web::UserConfigReply& loaded) {
  if (!loaded.served()) {
    GiveUp(loaded.refusal);
    return;
  }
  ro::core::Result<std::vector<std::string>> names = web::WhisperBlocksFromUserConfig(loaded.body);
  if (!names) {
    GiveUp(ro::core::Describe(names.error()));
    return;
  }
  names_ = std::move(*names);
  stage_ = Stage::kKept;
  replay_due_ = !names_.empty();
  ro::core::LogInfo("liste de blocage : {} nom(s) relu(s) au service web", names_.size());
  bool changed = false;
  for (const WhisperBlockChange& change : std::exchange(early_changes_, {})) {
    changed = Follow(change) || changed;
  }
  if (changed) {
    web_->Save(web::UserConfigOfWhisperBlocks(names_));
  }
}

std::vector<std::string> WhisperBlockMemory::Advance(std::span<const WhisperBlockChange> changes) {
  if (stage_ == Stage::kLoading) {
    early_changes_.insert(early_changes_.end(), changes.begin(), changes.end());
  } else if (stage_ == Stage::kKept) {
    bool changed = false;
    for (const WhisperBlockChange& change : changes) {
      changed = Follow(change) || changed;
    }
    if (changed) {
      web_->Save(web::UserConfigOfWhisperBlocks(names_));
    }
  }
  if (web_.has_value()) {
    web_->Advance();
    if (std::optional<web::UserConfigReply> loaded = web_->TakeLoaded(); loaded.has_value()) {
      Digest(*loaded);
    }
    // ⚠ UNE SAUVEGARDE REFUSÉE NE CHANGE RIEN À LA SESSION : la suivante
    // retentera, avec la liste entière.
    if (std::optional<web::UserConfigReply> saved = web_->TakeSaved();
        saved.has_value() && !saved->served() && !warned_) {
      warned_ = true;
      ro::core::LogWarning("liste de blocage : {}", saved->refusal);
    }
  }
  if (!replay_due_ || !map_entered_) {
    return {};
  }
  replay_due_ = false;
  const std::size_t replayed = std::min(names_.size(), ro::sim::kMostPendingWhisperBlocks);
  if (replayed < names_.size()) {
    ro::core::LogWarning("liste de blocage : {} nom(s) rangé(s), {} rejoué(s)", names_.size(),
                         replayed);
  }
  return {names_.begin(), names_.begin() + static_cast<std::ptrdiff_t>(replayed)};
}

}  // namespace moonclient
