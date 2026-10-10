#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/companion_memory.h"

#include "game/companion_ai_file.h"

namespace moonclient {

// ── CE QUE L'IA DES COMPAGNONS GARDE D'UNE SESSION À L'AUTRE ────────────────
//
// `savedata/companion_ai_state.yaml` : les délais des règles, les traces des
// skills, les suspensions et l'attente (voir `ro/sim/companion_memory.h`), par
// personnage, puis par compagnon (sa classe), puis par profil pour les règles.
//
// ⚠ UN FICHIER À PART DE `companion_ai.yaml`, et non une section : celui-ci
// s'écrit au plus toutes les `kCompanionStateSaveEveryMs` et à la sortie de la
// partie, quand celui des profils ne s'écrit que sur un geste du joueur, qui le
// corrige à la main. Un fichier de profils illisible n'empêche ainsi pas de
// garder les délais, et l'écriture périodique ne touche jamais au fichier que
// le joueur corrige.
//
// 🔴 CHAQUE COMPAGNON PORTE L'INSTANT DE SON ÉCRITURE, en horloge murale
// (millisecondes depuis l'époque Unix) : à la relecture, le temps passé hors du
// jeu vieillit la mémoire, et un délai écoulé pendant l'absence est échu.
//
// Mêmes garanties qu'au fichier des profils : un fichier illisible ou d'une
// version inconnue ne se réécrit pas, les nœuds inconnus, les autres
// personnages et les autres compagnons restent, l'écriture est atomique.

inline constexpr int kCompanionStateFileVersion = 1;
inline constexpr std::string_view kCompanionStateFileName = "companion_ai_state.yaml";
// L'ÉCRITURE PÉRIODIQUE : au plus une fois par cette période.
inline constexpr std::uint32_t kCompanionStateSaveEveryMs = 30000;

// CE QUE GARDE UN COMPAGNON.
struct StoredCompanionState {
  std::uint16_t job = 0;
  std::int64_t written_wall_ms = 0;
  bool standby = false;
  std::vector<ro::sim::CompanionSkillMemory> skills;
  // Les états des règles, par nom de profil.
  std::map<std::string, std::vector<ro::sim::CompanionRuleMemory>> rules_by_profile;

  [[nodiscard]] friend bool operator==(const StoredCompanionState&,
                                       const StoredCompanionState&) = default;
};

struct StoredCharacterState {
  CharacterKey key;
  std::vector<StoredCompanionState> companions;

  [[nodiscard]] friend bool operator==(const StoredCharacterState&,
                                       const StoredCharacterState&) = default;
};

struct CompanionStateRead {
  std::vector<StoredCharacterState> characters;
  bool unknown_version = false;
  bool unreadable = false;
};

[[nodiscard]] std::filesystem::path CompanionStatePath(const std::filesystem::path& client_root);

// LE TEXTE, sans I/O.
[[nodiscard]] CompanionStateRead ParseCompanionState(std::string_view text);
// Le texte courant où les compagnons de ce personnage remplacent ceux de même
// classe ; rien quand il est illisible ou d'une version inconnue.
[[nodiscard]] std::optional<std::string> MergeCompanionState(
    std::string_view current_text, const CharacterKey& key,
    const std::vector<StoredCompanionState>& companions);

// LE FICHIER. Absent : rien, sans erreur.
[[nodiscard]] CompanionStateRead LoadCompanionState(const std::filesystem::path& client_root);

// UN FICHIER ILLISIBLE SE MET DE CÔTÉ sous ce suffixe — puis « .2 », « .3 »…
// sans jamais écraser un précédent —, et se réécrit à neuf : il n'est fait que
// par le client, rien n'y est à régler. Au-delà de ce nombre de mises de côté,
// il n'est plus touché.
inline constexpr std::string_view kStateSetAsideSuffix = ".unreadable";
inline constexpr int kStateSetAsideMost = 9;

enum class StateWrite : std::uint8_t {
  kWritten,
  // Une version que ce client ne connaît pas : rien n'est écrit.
  kUnknownVersion,
  // Le disque a refusé, ou un fichier illisible n'a pas pu être mis de côté.
  kFailed,
};
// Relit, fusionne, écrit.
StateWrite SaveCompanionState(const std::filesystem::path& client_root, const CharacterKey& key,
                              const std::vector<StoredCompanionState>& companions);

// LA MÊME MÉMOIRE, VUE `elapsed_ms` PLUS TARD et sans date d'écriture : deux
// écritures qui ne diffèrent que du temps passé se comparent égales. Une liste
// de règles vide disparaît.
[[nodiscard]] std::vector<StoredCompanionState> ComparableStates(
    std::vector<StoredCompanionState> states, std::uint64_t elapsed_ms);

// L'HORLOGE MURALE, en millisecondes depuis l'époque Unix : celle qui date les
// écritures.
[[nodiscard]] std::int64_t WallNowMs();

// LA MÉMOIRE D'UN PROFIL de ce compagnon, vieillie du temps passé depuis
// l'écriture jusqu'à `wall_now_ms`. Une horloge revenue en arrière ne rajeunit
// rien.
[[nodiscard]] ro::sim::CompanionMemory MemoryOf(const StoredCompanionState& state,
                                                std::string_view profile, std::int64_t wall_now_ms);

}  // namespace moonclient
