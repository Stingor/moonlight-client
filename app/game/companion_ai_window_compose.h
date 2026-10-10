#pragma once

// LA FENÊTRE « IA DU COMPAGNON » ET LE MENU D'ENTITÉ, VUS DES PROFILS, sans
// I/O hors de la session : l'en-tête des profils de la vue, les classes de
// joueur proposées, les phrases de la proposition, la conduite posée depuis le
// menu d'un joueur, et un geste de la fenêtre entier. C'est ce que les essais
// éprouvent ; `CompanionAiStage` n'en est que le câblage.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/sim/companion_players.h"
#include "ro/sim/companion_tactics.h"
#include "ro/ui/companion_ai_window.h"

#include "game/companion_ai_editor.h"
#include "game/companion_ai_profile_gestures.h"
#include "game/companion_ai_session.h"

namespace moonclient {

static_assert(ro::ui::kCompanionAiProfileNameBytes == kProfileNameMostBytes + 1);

// CE QUE LA VUE DE L'EN-TÊTE DÉSIGNE, gardé d'une image à l'autre. Les sources
// d'import ne se refont que quand le type change ou que le fichier a été
// enregistré (`imports_stale`).
struct ProfilesViewStore {
  std::vector<ro::ui::ProfileChoiceView> choices;
  std::vector<std::string> keys;
  std::vector<ProfileSource> imports;
  std::vector<std::string> import_labels;
  std::uint16_t imports_job = 0;
  bool imports_stale = true;
};

// L'EN-TÊTE DES PROFILS d'un compagnon de cette classe : la liste, le profil
// édité, le choix et le repli de chaque contexte, le contexte de la carte, la
// ligne d'un profil qui n'agit nulle part, l'import et la proposition.
void ComposeProfilesView(CompanionAiSession& session, std::uint16_t job, std::string_view edited,
                         std::string_view proposal, ProfilesViewStore& store,
                         ro::ui::CompanionProfilesView& into);

// LES CLASSES DE JOUEUR qu'une conduite peut désigner, d'après la table des noms
// de classe : sans variante montée ni costume (voir `IsConductJob`), un seul
// rang par libellé, triées par nom.
[[nodiscard]] std::vector<ro::ui::RuleChoice> PlayerJobChoices(
    const std::unordered_map<std::uint32_t, std::string>& job_names);

// LA PHRASE D'UNE PROPOSITION, pour la bannière et l'onglet, et celle du chat.
[[nodiscard]] std::string ProposalLine(const EvolutionProposal& proposal);
[[nodiscard]] std::string ProposalChatLine(const EvolutionProposal& proposal);

// LA CONDUITE ENVERS UN JOUEUR NOMMÉ, depuis le menu d'entité : dans le profil
// appliqué, l'IA basique cédant la place à « Mon profil ». Un rang ou un
// déclencheur ; une ligne neuve naît au rang « moyen » et au déclencheur
// « toujours ».
[[nodiscard]] AiEditResult SetPlayerConduct(CompanionAiSession& session,
                                            const SeatedCompanion& seated, const std::string& name,
                                            std::optional<ro::sim::TacticRank> rank,
                                            std::optional<ro::sim::TacticTrigger> trigger);
// Retirer la ligne, du profil appliqué seulement : rien n'est créé pour cela.
[[nodiscard]] AiEditResult RemovePlayerConduct(CompanionAiSession& session,
                                               const SeatedCompanion& seated,
                                               const std::string& name);

// UN GESTE DE LA FENÊTRE, entier : un geste de l'en-tête des profils, ou un
// geste sur le profil édité.
struct AiWindowOutcome {
  ProfileGestureResult gesture;
  AiEditResult edit;
  // ⚠ LE NOMBRE DE RÈGLES DU PROFIL VISÉ, relevé AVANT tout enregistrement :
  // un enregistrement remplace les profils en mémoire, et un pointeur dans
  // l'ancien ne se relit plus.
  std::size_t rule_count = 0;
};
[[nodiscard]] AiWindowOutcome ApplyAiWindowGesture(
    const ro::ui::CompanionAiWindowAction& action, CompanionAiSession& session,
    const SeatedCompanion& seated, std::span<const std::string> profile_keys,
    std::span<const ProfileSource> imports, std::span<const ro::sim::TacticKey> tactic_keys,
    std::span<const ro::sim::PlayerConductKey> conduct_keys);

}  // namespace moonclient
