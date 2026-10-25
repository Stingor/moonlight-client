#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/sim/survivors_ranked.h"

namespace moonclient {

// ── UN RÉSULTAT EN ATTENTE : une partie classée dont le verdict n'est pas venu ─
//
// Le serveur tient le jeton PAR PERSONNAGE : une entrée porte donc le serveur
// et le personnage, et ne se renvoie qu'avec eux. Elle se garde dans le carnet
// (`pending_results`) jusqu'au verdict, ou jusqu'à ce que son jeton expire.
//
// Tout ce qui désigne le catalogue s'y range par CLÉ, jamais par rang.
struct SurvivorsPendingResult {
  // L'adresse du serveur de CONNEXION choisi (`hôte:port`), stable d'une zone
  // à l'autre, ou `local` quand elle manque : voir `SurvivorsServerKeyOf`.
  std::string server_key;
  std::uint32_t character_id = 0;
  std::string character_name;  // en UTF-8, pour le dire au joueur
  std::uint32_t ticket = 0;
  std::int64_t issued_at_utc_s = 0;  // l'heure du jeton, à l'horloge du serveur
  std::uint16_t ruleset = 0;
  std::string arena_key;  // `prt_fild08`
  std::string class_key;  // `mage`
  std::string board_key;  // kRankedBoardKey ou kChallengeBoardKey
  std::uint64_t seed = 0;
  ro::sim::SurvivorsEndComponents components;
  std::uint64_t local_score = 0;     // `SurvivorsScore` du bilan, à comparer au verdict
  std::int64_t ended_at_utc_s = 0;   // la fin de la partie, à l'horloge de la machine
  std::uint32_t send_count = 0;      // les `END` déjà partis
  std::int64_t last_sent_utc_s = 0;  // le dernier envoi ; 0 : jamais
  [[nodiscard]] bool operator==(const SurvivorsPendingResult&) const = default;
};

// UNE ENTRÉE QUI PEUT PARTIR : un serveur, un personnage, une graine non
// nulle, un tableau connu, une arène et une classe nommées, et une issue
// qu'`END` porte. Une autre se garde comme étrangère et ne s'envoie jamais.
[[nodiscard]] bool IsSendablePendingResult(const SurvivorsPendingResult& entry);

// Vrai quand l'entrée est celle de ce jeton, de ce personnage, sur ce serveur.
[[nodiscard]] bool IsPendingResultOf(const SurvivorsPendingResult& entry,
                                     std::string_view server_key, std::uint32_t character_id,
                                     std::uint32_t ticket);

// LE TEXTE YAML D'UNE ENTRÉE, dans les deux sens. La lecture rend rien quand
// le texte ne se lit pas, qu'un champ obligé manque ou est d'un mauvais type,
// ou que l'entrée ne peut pas partir (`IsSendablePendingResult`). Les champs
// facultatifs (nom, score local, fin, envois) gardent leur défaut.
[[nodiscard]] std::optional<SurvivorsPendingResult> ReadSurvivorsPendingResult(
    std::string_view yaml);
[[nodiscard]] std::string WriteSurvivorsPendingResult(const SurvivorsPendingResult& entry);

}  // namespace moonclient
