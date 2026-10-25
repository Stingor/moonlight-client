#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/sim/survivors_journal.h"

#include "survivors/survivors_stage.h"

namespace moonclient {

// LES FICHIERS DU MODE SURVIVORS, dans `savedata\survivors\`, écrits d'un
// bloc (écriture atomique) :
//
// - `last_run.msvj` : le journal de la dernière partie, finie ou
//   interrompue, au format de `EncodeSurvivorsJournal`. Il redonne la partie
//   entière par `ReplaySurvivors`.
// - `interrupted_run.msve` : la fin d'une partie que la perte de liaison a
//   interrompue (Q3). Elle se montre à la prochaine ouverture de la salle,
//   une seule fois : la lire la retire. Garder la fin plutôt que la rejouer
//   depuis le journal épargne des secondes de rejeu à l'ouverture.
inline constexpr std::string_view kSurvivorsSaveFolder = "survivors";
inline constexpr std::string_view kLastRunFileName = "last_run.msvj";
inline constexpr std::string_view kInterruptedEndingFileName = "interrupted_run.msve";

[[nodiscard]] std::filesystem::path SurvivorsSavePath(const std::filesystem::path& client_root,
                                                      std::string_view file_name);

// Petit-boutiste :
//   "MSVE" | version u16 | class_id u16 | le bilan, champ par champ dans
//   l'ordre de SurvivorsTally (outcome sur u8) | tués par espèce : nombre u16,
//   puis (id u16, valeur u64) | dégâts par skill : de même | cartes : nombre
//   u8, puis (id u16, niveau u8) | zeny u32 | tableau u8 | année ISO i32 |
//   semaine u8 | cartes tombées : nombre u16, puis id u16 | glaçages u64
// La version 1 s'arrête aux Zeny et se lit encore : partie libre, aucune
// carte tombée, aucun glaçage.
[[nodiscard]] std::vector<std::uint8_t> EncodeSurvivorsEnding(const SurvivorsEnding& ending);
// kUnexpectedEnd trop court ; kBadSignature ; kUnsupportedVersion ;
// kCorruptData pour une issue ou un tableau inconnus, ou des octets en trop.
[[nodiscard]] ro::core::Result<SurvivorsEnding> DecodeSurvivorsEnding(
    std::span<const std::uint8_t> bytes);

// Le journal d'une partie finie.
[[nodiscard]] ro::core::Result<void> SaveLastRun(const std::filesystem::path& client_root,
                                                 const ro::sim::SurvivorsJournal& journal);
// Le journal d'une partie interrompue, et sa fin.
[[nodiscard]] ro::core::Result<void> SaveInterruptedRun(const std::filesystem::path& client_root,
                                                        const ro::sim::SurvivorsJournal& journal,
                                                        const SurvivorsEnding& ending);
// La fin d'une partie interrompue, retirée du disque ; rien sans fichier. Un
// fichier illisible se retire aussi, en le disant au journal du client : il
// ne se montrerait jamais.
[[nodiscard]] std::optional<SurvivorsEnding> TakeInterruptedEnding(
    const std::filesystem::path& client_root);
// Un journal relu, tel que DecodeSurvivorsJournal le rend ; kNotFound sans
// fichier.
[[nodiscard]] ro::core::Result<ro::sim::SurvivorsJournal> ReadJournalFile(
    const std::filesystem::path& path);

}  // namespace moonclient
