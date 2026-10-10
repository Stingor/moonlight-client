// LES PROFILS DE L'IA DES COMPAGNONS DANS LA PARTIE : le câblage de
// `CompanionAiSession` sur les emplacements des deux compagnons. Voir
// `game/companion_ai_stage.h`.

#include <array>
#include <optional>
#include <string>

#include "game/companion_ai_stage.h"

namespace moonclient {
namespace {

using ro::sim::AiCompanion;

}  // namespace

void CompanionAiStage::See(const std::filesystem::path& client_root, std::uint32_t character_id) {
  session_.Open(client_root, character_id);
  edited_since_ms_.reset();
  // UNE AUTRE PARTIE : chaque emplacement entier repart à neuf — classe,
  // acteur suivi, désignation, IA, profil appliqué —, et rien de ce qui visait
  // un compagnon de la partie d'avant ne survit. La mémoire du même compagnon
  // revient du fichier des délais.
  for (Slot& slot : slots_) {
    slot = Slot();
  }
  aim_.reset();
  proposal_texts_ = {};
  proposal_shown_ = {};
  profiles_view_ = ProfilesViewStore();
  edited_.reset();
  seen_map_entries_.reset();
  arrived_ms_.reset();
  noted_arrival_ms_.reset();
}

void CompanionAiStage::Leave(std::uint32_t now_ms) {
  SaveProfiles();
  const std::array<SeatedCompanion, 2> seated = SeatedAll();
  session_.SaveState(seated, now_ms, WallNowMs(), /*force=*/true);
}

const std::optional<EvolutionProposal>& CompanionAiStage::proposal(
    AiCompanion companion) const noexcept {
  return SlotOf(companion).seat.proposal;
}

void CompanionAiStage::AnswerProposal(AiCompanion companion, bool accept) {
  session_.Answer(Seated(companion), accept);
  RefreshSettings();
}

SeatedCompanion CompanionAiStage::Seated(AiCompanion companion) noexcept {
  Slot& slot = SlotOf(companion);
  return SeatedCompanion{
      .companion = companion, .seat = &slot.seat, .brain = &slot.brain, .settings = &slot.settings};
}

std::array<SeatedCompanion, 2> CompanionAiStage::SeatedAll() noexcept {
  return {Seated(AiCompanion::kHomunculus), Seated(AiCompanion::kMercenary)};
}

std::string CompanionAiStage::AppliedProfile(const Slot& slot) const {
  return slot.seat.applied.has_value() ? slot.seat.applied->second : std::string(kBasicProfileName);
}

void CompanionAiStage::RefreshSettings() {
  for (const SeatedCompanion& seated : SeatedAll()) {
    session_.Refresh(seated);
  }
}

}  // namespace moonclient
