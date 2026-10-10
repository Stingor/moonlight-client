#pragma once

// LES PROFILS DE L'IA DES COMPAGNONS AU FIL D'UNE PARTIE : le profil actif du
// contexte appliqué à chaque IA, la proposition de recopie, ce qui survit d'une
// carte, d'un profil et d'une session à l'autre, et l'écriture des deux
// fichiers. Sans horloge ni réseau : l'appelant donne les instants, ce que les
// essais éprouvent ; `CompanionAiStage` n'en est que le câblage.

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_memory.h"
#include "ro/sim/companion_players.h"
#include "ro/sim/map_mode.h"

#include "game/companion_ai_editor.h"
#include "game/companion_ai_file.h"
#include "game/companion_ai_profiles.h"
#include "game/companion_ai_state_file.h"

namespace moonclient {

// CE QUE LA SESSION RETIENT D'UN COMPAGNON.
struct CompanionSeat {
  // Le profil appliqué à son IA : sa classe et son nom ; vide avant le premier.
  std::optional<std::pair<std::uint16_t, std::string>> applied;
  // La proposition de recopie qui attend sa réponse.
  std::optional<EvolutionProposal> proposal;
};

// UN COMPAGNON tel que la session le mène : sa place, son IA et ses réglages.
struct SeatedCompanion {
  ro::sim::AiCompanion companion = ro::sim::AiCompanion::kHomunculus;
  CompanionSeat* seat = nullptr;
  ro::sim::CompanionBrain* brain = nullptr;
  ro::sim::CompanionAiSettings* settings = nullptr;
};

// LE COMPAGNON SUIVI d'une image à l'autre : son acteur, et sa classe une fois
// en vue.
struct FollowedCompanion {
  std::uint32_t actor_id = 0;
  std::optional<std::uint16_t> job;
};

// À CHAQUE IMAGE OÙ LE COMPAGNON EST LÀ. 🔴 UN AUTRE COMPAGNON — un autre
// acteur, ou une autre classe — n'hérite de rien : son IA repart de zéro, ni
// délais, ni traces, ni suspensions, ni attente ; seul le délai d'arrivée sur la
// carte court toujours, depuis `arrived_ms`. Le portail et la reconnexion du
// MÊME compagnon, eux, gardent sa mémoire (voir `ForgetMap` et le fichier des
// délais). ⚠ UNE CLASSE NULLE N'EST PAS UNE ESPÈCE : l'acteur est seulement hors
// de vue, après un portail, et la classe reste celle d'avant. Rend vrai pour un
// autre compagnon.
bool FollowCompanion(FollowedCompanion& followed, ro::sim::CompanionBrain& brain,
                     std::uint32_t actor_id, std::uint16_t job, std::uint32_t arrived_ms);

class CompanionAiSession {
 public:
  // UNE PARTIE COMMENCE pour ce personnage : profils relus, et tout ce qui
  // tient à la session d'avant oublié. `character_id` est son cid, zéro quand
  // le client ne le connaît pas (son nom sert alors de clé).
  void Open(const std::filesystem::path& client_root, std::uint32_t character_id);
  [[nodiscard]] bool opened() const noexcept { return opened_; }
  void NoteCharacterName(std::string_view name);
  [[nodiscard]] bool character_known() const noexcept {
    return character_.cid != 0 || !character_.name.empty();
  }
  [[nodiscard]] const CharacterKey& character() const noexcept { return character_; }

  // LA CARTE, à chaque image : `arrived_ms` est l'arrivée sur l'entrée courante.
  void ObserveMap(std::uint32_t map_entries, const ro::sim::MapMode& mode, std::uint32_t arrived_ms,
                  std::uint32_t now_ms);
  [[nodiscard]] ro::sim::CompanionAiContext context() const noexcept {
    return context_watch_.context();
  }

  [[nodiscard]] CompanionProfiles& profiles() noexcept { return profiles_; }
  [[nodiscard]] const CompanionProfiles& profiles() const noexcept { return profiles_; }
  [[nodiscard]] CompanionProfileEdits& edits() noexcept { return edits_; }
  // Les réglages d'un compagnon de cette classe sous ce profil ; l'IA basique
  // pour elle-même ou pour un profil disparu.
  [[nodiscard]] ro::sim::CompanionAiSettings SettingsFor(ro::sim::AiCompanion companion,
                                                         std::uint16_t job,
                                                         std::string_view profile) const;
  // Fusionne les modifications en attente dans le fichier relu. Rend vrai
  // quand le fichier écrit a remplacé les profils en mémoire : les réglages
  // des compagnons sont alors à refaire (`Refresh`). En attente sinon.
  bool SaveProfiles();
  void Refresh(const SeatedCompanion& seated) const;

  // ── UN COMPAGNON ──────────────────────────────────────────────────────────

  // À CHAQUE IMAGE où il est là : le profil actif du contexte, appliqué s'il a
  // changé, et la proposition de recopie.
  void Resolve(const SeatedCompanion& seated, std::uint16_t job, std::uint32_t now_ms,
               std::int64_t wall_now_ms);
  // LE PROFIL QU'UN GESTE MODIFIE : celui qui est appliqué. L'IA basique ne se
  // modifie pas : « Mon profil » est créé s'il n'existe pas, et prend sa place
  // — le choix du contexte courant est posé quand le personnage en a un pour
  // ce type, et le profil appliqué change de nom sans que rien ne redémarre.
  // Sans profil quand rien ne peut être modifié.
  [[nodiscard]] AiEditTarget EditTarget(const SeatedCompanion& seated);
  // LE PROFIL NOMMÉ `profile` de la classe du compagnon, pour un geste de la
  // fenêtre, qui édite un profil choisi ; sans profil pour l'IA basique, qui ne
  // se modifie pas, ou pour un profil disparu.
  [[nodiscard]] AiEditTarget NamedTarget(const SeatedCompanion& seated, std::string_view profile);
  void Answer(const SeatedCompanion& seated, bool accept);

  // ── LES PROFILS, VUS DES COMPAGNONS ───────────────────────────────────────
  //
  // Le profil appliqué renommé garde son IA et sa mémoire ; un profil retiré
  // emporte la sienne.
  ProfileRefusal Rename(std::span<const SeatedCompanion> seated, std::uint16_t job,
                        std::string_view from, std::string_view to);
  ProfileRefusal Delete(std::uint16_t job, std::string_view name);

  // ── LES DÉLAIS ────────────────────────────────────────────────────────────
  //
  // Au plus toutes les `kCompanionStateSaveEveryMs`, ou tout de suite
  // (`force`) ; jamais quand rien n'a changé depuis la dernière écriture. Un
  // échec ne se dit qu'une fois.
  void SaveState(std::span<const SeatedCompanion> seated, std::uint32_t now_ms,
                 std::int64_t wall_now_ms, bool force);

 private:
  // LES ÉTATS DE RÈGLE D'UN PROFIL QUI N'EST PAS APPLIQUÉ, mesurés à `at_ms`.
  struct StashedRules {
    std::vector<ro::sim::CompanionRuleMemory> rules;
    std::uint32_t at_ms = 0;
  };
  // UN AUTRE PROFIL : seuls ses règles, tactiques et réglages changent. Le
  // combat, l'attente et la file ne bougent pas ; les états de règle du profil
  // quitté se gardent de côté, ceux du profil pris reviennent.
  void Apply(const SeatedCompanion& seated, std::uint16_t job, const std::string& name,
             std::uint32_t now_ms, std::int64_t wall_now_ms);
  void LoadState();
  [[nodiscard]] bool Exists(std::uint16_t job, std::string_view name) const;

  std::filesystem::path client_root_;
  bool opened_ = false;
  CompanionProfiles profiles_;
  CompanionProfileEdits edits_;
  CharacterKey character_;
  ro::sim::CompanionContextWatch context_watch_;
  std::map<std::pair<std::uint16_t, std::string>, StashedRules> stashed_;
  // CE QUE LE FICHIER DES DÉLAIS GARDAIT pour ce personnage, par classe, en
  // attendant la première application de chacune ; relu une fois.
  bool state_loaded_ = false;
  std::map<std::uint16_t, StoredCompanionState> loaded_state_;
  std::optional<std::uint32_t> state_saved_ms_;
  // La dernière écriture, et quand : de quoi ne pas réécrire la même chose.
  std::vector<StoredCompanionState> state_written_;
  std::uint32_t state_written_ms_ = 0;
  bool state_warned_ = false;
};

}  // namespace moonclient
