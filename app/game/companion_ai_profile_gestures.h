#pragma once

// CE QUE L'EN-TÊTE DES PROFILS DE LA FENÊTRE « IA DU COMPAGNON » DEMANDE, sans
// I/O hors de la session : créer, renommer, dupliquer, supprimer, importer,
// choisir le profil d'un contexte, répondre à la proposition. C'est ce que les
// essais éprouvent ; `CompanionAiStage` n'en est que le câblage.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/ui/companion_ai_window.h"

#include "game/companion_ai_session.h"

namespace moonclient {

// UNE SOURCE D'IMPORT : un profil d'un autre type, ou le profil actif d'un autre
// personnage, tel que le fichier le connaît.
struct ProfileSource {
  std::uint16_t job = 0;
  std::string name;
  // Le libellé que la fenêtre montre, origine comprise, déjà traduit.
  std::string label;
};

// LES SOURCES D'IMPORT pour un compagnon de cette classe : les profils des
// autres types, puis les profils actifs des autres personnages (chaque
// contexte, sans doublon), avec leur classe d'origine. Rien d'extérieur au
// fichier.
[[nodiscard]] std::vector<ProfileSource> ImportSources(const CompanionProfiles& profiles,
                                                       const CharacterKey& self, std::uint16_t job);

struct ProfileGestureResult {
  // Le geste était de l'en-tête des profils.
  bool handled = false;
  bool accepted = false;
  // La raison d'un refus, traduite ; vide sans refus.
  std::string refusal;
  // LE PROFIL QUE LA FENÊTRE ÉDITE DÉSORMAIS, par sa clé : le profil créé,
  // renommé ou importé ; vide pour celui qui est appliqué. Rien quand il ne
  // change pas.
  std::optional<std::string> edited;
};

// UN GESTE DE L'EN-TÊTE. `profile_keys` : la clé de chaque profil de la vue,
// dans son ordre ; `imports` : les sources que « Importer… » proposait.
[[nodiscard]] ProfileGestureResult ApplyProfileGesture(
    const ro::ui::CompanionAiWindowAction& action, CompanionAiSession& session,
    const SeatedCompanion& seated, std::span<const std::string> profile_keys,
    std::span<const ProfileSource> imports);

}  // namespace moonclient
