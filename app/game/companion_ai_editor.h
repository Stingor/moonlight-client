#pragma once

// CE QU'UN GESTE DE LA FENÊTRE « IA DU COMPAGNON » FAIT AU PROFIL, sans I/O :
// réglages, tactiques, règles et amis posés dans le profil visé et notés à
// enregistrer, gestes refusés avec leur raison, et la fenêtre suivie d'une
// image à l'autre (départ d'un compagnon, fermeture). C'est ce que les essais
// éprouvent ; `CompanionAiStage` n'en est que le câblage.

#include <array>
#include <cstdint>
#include <span>
#include <string>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_tactics.h"
#include "ro/ui/companion_ai_window.h"

#include "game/companion_ai_file.h"

namespace moonclient {

// LE PROFIL QU'UN GESTE MODIFIE, son nom, et les modifications à enregistrer.
struct AiEditTarget {
  ro::sim::AiCompanion companion = ro::sim::AiCompanion::kHomunculus;
  std::uint16_t job = 0;
  CompanionProfile* profile = nullptr;
  std::string profile_name;
  CompanionProfileEdits* edits = nullptr;
};

// QUAND LE GESTE S'ENREGISTRE.
enum class AiEditSave : std::uint8_t {
  kNone,
  // Après le calme de `kTacticSaveQuietMs`, la fermeture ou le départ.
  kAfterCalm,
  // Aussitôt.
  kNow,
};

struct AiEditResult {
  bool accepted = false;
  AiEditSave save = AiEditSave::kNone;
  // La raison d'un refus, traduite ; vide sans refus à dire.
  std::string refusal;
  // La règle en cause, moins de zéro pour aucune.
  int refused_rule = -1;
};

// UN GESTE DE LA FENÊTRE. `tactic_keys` et `conduct_keys` : la clé de chaque
// ligne de tactique et de conduite de la vue, dans son ordre.
[[nodiscard]] AiEditResult ApplyAiEdit(
    const ro::ui::CompanionAiWindowAction& action, const AiEditTarget& target,
    std::span<const ro::sim::TacticKey> tactic_keys,
    std::span<const ro::sim::PlayerConductKey> conduct_keys = {});

// UN AMI posé ou retiré, depuis la fenêtre ou le menu d'entité. Le nom perd
// ses espaces autour ; un nom vide, déjà là, ou une liste pleine, refusés.
[[nodiscard]] AiEditResult SetProfileFriend(const AiEditTarget& target, std::string_view typed,
                                            bool befriend);

// POURQUOI L'IA REFUSE CETTE RÈGLE, traduit ; vide quand elle l'accepte.
[[nodiscard]] std::string RuleRefusal(const ro::sim::CompanionRule& rule);

// CE PROFIL VAUT L'IA BASIQUE : rien de réglé, ni tactique, ni règle, ni ami.
[[nodiscard]] bool ProfileIsBasic(ro::sim::AiCompanion companion, std::uint16_t job,
                                  const CompanionProfile& profile);

// LA FENÊTRE D'UNE IMAGE À L'AUTRE : les compagnons présents, et si elle était
// ouverte.
struct AiWindowWatch {
  std::array<bool, 2> present{};
  bool open = false;
};

// À CHAQUE IMAGE. Un compagnon parti, ou la fenêtre fermée (croix, Échap,
// plus aucun compagnon) : l'enregistrement part aussitôt, ce qui rend vrai, et
// l'état propre au compagnon repart de zéro. L'onglet d'un compagnon parti
// cède la place à l'autre.
[[nodiscard]] bool FollowAiWindow(ro::ui::CompanionAiWindowState& window, AiWindowWatch& watch,
                                  std::array<bool, 2> present_now);

}  // namespace moonclient
