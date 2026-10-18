// LES PROFILS ET LES CONDUITES DE L'IA DES COMPAGNONS, VUS DE LA PARTIE : l'en-tête
// des profils et l'onglet « Joueurs » de la fenêtre « IA du compagnon », la
// proposition de recopie (bannière, onglet, chat), et le sous-menu « Conduite
// du compagnon » du menu d'entité d'un joueur. Le câblage de
// `game/companion_ai_window_compose.h`. Voir `game/companion_ai_stage.h`.

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/printed.h"

#include "game/companion_ai_names.h"
#include "game/companion_ai_stage.h"
#include "game/companion_ai_window_compose.h"
#include "game/companion_tactic_fields.h"
#include "world/entity_menu.h"

namespace moonclient {
namespace {

using ro::sim::AiCompanion;
using ro::sim::CompanionPlayerConduct;
using ro::sim::PlayerConductKey;
using ro::sim::PlayerConductKeyKind;

[[nodiscard]] std::size_t SlotIndexOf(AiCompanion companion) noexcept {
  return companion == AiCompanion::kHomunculus ? 0 : 1;
}

}  // namespace

std::string CompanionAiStage::EditedProfile(const Slot& slot,
                                            const ro::ui::CompanionAiWindowState& window) const {
  const std::uint16_t job = slot.seat.applied.has_value() ? slot.seat.applied->first : 0;
  if (!window.edited_profile.empty() &&
      (window.edited_profile == kBasicProfileName ||
       session_.profiles().Find(job, window.edited_profile) != nullptr)) {
    return window.edited_profile;
  }
  return AppliedProfile(slot);
}

void CompanionAiStage::ComposeProfiles(AiCompanion companion, std::uint16_t job,
                                       const std::string& edited,
                                       ro::ui::CompanionProfilesView& into) {
  ComposeProfilesView(session_, job, edited, proposal_texts_[SlotIndexOf(companion)],
                      profiles_view_, into);
}

void CompanionAiStage::ComposePlayers(const ro::sim::CompanionAiSettings& settings,
                                      const CompanionAiWindowBooks& books) {
  // LES CLASSES DE JOUEUR, refaites quand la table change.
  if (books.job_names != nullptr && books.job_names->size() != job_names_size_) {
    job_names_size_ = books.job_names->size();
    player_jobs_ = PlayerJobChoices(books.job_names->by_id);
  }
  conduct_rows_.clear();
  conduct_row_keys_.clear();
  conduct_generic_present_ = {};
  for (const CompanionPlayerConduct& line : settings.player_conducts.lines()) {
    std::string label;
    switch (line.key.kind) {
      case PlayerConductKeyKind::kPlayer:
        label = line.key.name;
        break;
      case PlayerConductKeyKind::kJob: {
        const std::string_view named =
            books.job_names != nullptr ? books.job_names->Find(line.key.job) : std::string_view();
        if (named.empty()) {
          label = ro::core::Printed(ro::core::Tr("Classe %u"), static_cast<unsigned>(line.key.job));
        } else {
          label = std::string(named);
        }
        break;
      }
      case PlayerConductKeyKind::kAnyPlayer:
        conduct_generic_present_[ro::ui::kConductGenericAnyPlayer] = true;
        label = ro::core::Tr("Tout autre joueur");
        break;
      case PlayerConductKeyKind::kOwnerGuild:
        conduct_generic_present_[ro::ui::kConductGenericOwnerGuild] = true;
        label = ro::core::Tr("Membres de la guilde du maître");
        break;
    }
    conduct_rows_.push_back(ro::ui::ConductRowView{.label = std::move(label),
                                                   .rank = static_cast<int>(line.rank),
                                                   .trigger = static_cast<int>(line.trigger),
                                                   .advanced = ConductFieldsOf(line)});
    conduct_row_keys_.push_back(line.key);
  }
}

void CompanionAiStage::NoteProposals(ro::sim::World& world, std::uint32_t now_ms) {
  for (const AiCompanion companion : {AiCompanion::kHomunculus, AiCompanion::kMercenary}) {
    const Slot& slot = SlotOf(companion);
    const std::size_t index = SlotIndexOf(companion);
    const std::optional<EvolutionProposal> proposal =
        slot.actor_id != 0 ? slot.seat.proposal : std::nullopt;
    // LA PHRASE NE SE REFAIT QUE QUAND LA PROPOSITION CHANGE.
    if (proposal == proposal_shown_[index]) {
      continue;
    }
    proposal_shown_[index] = proposal;
    proposal_texts_[index] = proposal.has_value() ? ProposalLine(*proposal) : std::string();
    // LE CHAT LE DIT UNE FOIS par proposition, d'une connexion à l'autre : la
    // fiche du personnage le retient.
    if (proposal.has_value() && session_.character_known() &&
        CompanionProfileBook(session_.profiles(), session_.edits())
            .MarkTold(session_.character(), *proposal)) {
      world.RecordChat(ro::sim::ChatKind::kServer, 0, ProposalChatLine(*proposal), now_ms);
      SaveProfiles();
    }
  }
}

std::string_view CompanionAiStage::ProposalText(AiCompanion companion) const {
  return proposal_texts_[SlotIndexOf(companion)];
}

std::optional<CompanionPlayerConduct> CompanionAiStage::ConductFor(AiCompanion companion,
                                                                   const std::string& name) const {
  const Slot& slot = SlotOf(companion);
  if (slot.actor_id == 0 || name.empty()) {
    return std::nullopt;
  }
  const CompanionPlayerConduct* line = slot.settings.player_conducts.Find(
      PlayerConductKey{.kind = PlayerConductKeyKind::kPlayer, .name = name});
  return line != nullptr ? std::optional(*line) : std::nullopt;
}

void CompanionAiStage::ChooseConduct(AiCompanion companion, const std::string& name, int choice) {
  if (name.empty() || !present(companion)) {
    return;
  }
  AiEditResult result;
  if (choice == static_cast<int>(TacticMenuChoice::kRemove)) {
    result = RemovePlayerConduct(session_, Seated(companion), name);
  } else {
    const int rank = choice - static_cast<int>(TacticMenuChoice::kRankFirst);
    const int trigger = choice - static_cast<int>(TacticMenuChoice::kTriggerFirst);
    if (rank >= 0 && rank < static_cast<int>(ro::sim::kTacticRankCount)) {
      result = SetPlayerConduct(session_, Seated(companion), name,
                                static_cast<ro::sim::TacticRank>(rank), std::nullopt);
    } else if (trigger >= 0 && trigger < static_cast<int>(ro::sim::kTacticTriggerCount)) {
      result = SetPlayerConduct(session_, Seated(companion), name, std::nullopt,
                                static_cast<ro::sim::TacticTrigger>(trigger));
    }
  }
  if (!result.refusal.empty()) {
    ro::core::LogWarning("IA des compagnons : conduite refusée — {}", result.refusal);
  }
  if (result.accepted) {
    SaveProfiles();
  }
}

void CompanionAiStage::OpenConductWindow(AiCompanion companion,
                                         ro::ui::CompanionAiWindowState& window) {
  OpenAiWindow(companion, window);
  // LA CONDUITE VIENT D'ÊTRE ÉCRITE DANS LE PROFIL APPLIQUÉ : c'est lui que la
  // fenêtre montre.
  window.edited_profile.clear();
  window.expanded_conduct = -1;
  window.section_asked = static_cast<int>(ro::ui::CompanionAiSection::kPlayers);
}

}  // namespace moonclient
