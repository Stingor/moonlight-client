#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_rules.h"

namespace moonclient {

// ── LES CLÉS DU CODEX : une par entrée, anglaise, stable ────────────────────
//
// Elles désignent ce que le carnet retient avoir été vu (section `codex`) et
// la phrase de la fiche. ⚠ EN CHANGER UNE OUBLIE CE QUE LES CARNETS DÉJÀ
// ÉCRITS ONT VU. Une clé se tire du catalogue des règles sans y rien ajouter :
// - un sort, par la clé de son skill (`fire_bolt`) ;
// - un monstre, mini-boss et boss compris, ou une plante, par sa classe RO en
//   décimal (`"1002"`), stable, et non par un rang de tableau ;
// - un objet au sol, par sa sorte (`meat`), une gemme par sa couleur
//   (`blue_gem`), la carte au sol par `card` ;
// - un événement par sa sorte et l'espèce qu'il amène (`swarm_1011`), les
//   élites par `elites`, la nuit par `night`.

// Les catégories, dans l'ordre où le carnet les écrit. Un ajout se fait en FIN.
enum class CodexCategory : std::uint8_t { kSkills, kFoes, kPlants, kItems, kEvents };
inline constexpr std::size_t kCodexCategoryCount = 5;
static_assert(static_cast<std::size_t>(CodexCategory::kEvents) + 1 == kCodexCategoryCount);

// La clé de fichier d'une catégorie (`skills`), et la catégorie d'une clé.
[[nodiscard]] const char* CodexCategoryKey(CodexCategory category) noexcept;
[[nodiscard]] std::optional<CodexCategory> CodexCategoryOf(std::string_view key) noexcept;

inline constexpr std::string_view kCodexElitesKey = "elites";
inline constexpr std::string_view kCodexNightKey = "night";

[[nodiscard]] std::string CodexSkillKey(const ro::sim::SurvivorsSkillRule& rule);
// La classe RO en décimal, sans dépendre de la locale.
[[nodiscard]] std::string CodexFoeKey(std::uint16_t foe_id);
[[nodiscard]] std::string CodexPlantKey(std::uint16_t foe_id);
// Toute sorte d'objet a sa clé, la carte au sol comprise.
[[nodiscard]] const char* CodexItemKey(ro::sim::PickupKind kind) noexcept;
// La clé d'une couleur de gemme, par son objet ; "" pour un objet qui n'en
// montre aucune.
[[nodiscard]] const char* CodexGemKey(std::uint16_t item_id) noexcept;
// `<sorte>_<classe RO>` : `swarm_1011`, `ring_1008`, `raid_1122`, `group_1051`.
[[nodiscard]] std::string CodexEventKey(const ro::sim::EventRule& event);

}  // namespace moonclient
