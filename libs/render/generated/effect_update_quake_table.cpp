// LES TREMBLEMENTS QU'UN EFFET DEMANDE EN VIVANT, par identifiant.
//
// 12 identifiants dont la case du répartiteur de mise à jour envoie au mode
// de jeu la commande du tremblement, aux âges que sa garde nomme.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_effect_update_quakes.py depuis le
// relevé scripts/client_effect_update_quakes.json, pas écrit à la main.

#include <array>

#include "ro/render/effect_update_quake.h"
#include "ro/render/str_effect_setup.h"

namespace ro::render {
namespace {

constexpr std::array<std::uint16_t, 5> kQuakeTicks0 = {50, 100, 150, 200, 250};
constexpr std::array<std::uint16_t, 1> kQuakeTicks1 = {40};
constexpr std::array<std::uint16_t, 3> kQuakeTicks2 = {3, 7, 9};
constexpr std::array<std::uint16_t, 1> kQuakeTicks3 = {18};
constexpr std::array<std::uint16_t, 1> kQuakeTicks4 = {5};
constexpr std::array<std::uint16_t, 3> kQuakeTicks5 = {0, 35, 70};
constexpr std::array<std::uint16_t, 1> kQuakeTicks6 = {1};
constexpr std::array<std::uint16_t, 3> kQuakeTicks7 = {0, 40, 80};

// Triée par identifiant : la recherche est dichotomique.
constexpr std::array<EffectUpdateQuake, 12> kEffectUpdateQuakes = {{
    {90, kQuakeTicks0, kEffectQueuedQuakeMs},
    {92, kQuakeTicks1, kEffectQueuedQuakeMs},
    {183, kQuakeTicks2, kEffectShortQuakeMs},
    {220, kQuakeTicks3, kEffectShortQuakeMs},
    {537, kQuakeTicks4, kEffectQueuedQuakeMs},
    {666, kQuakeTicks5, kEffectQueuedQuakeMs},
    {668, kQuakeTicks6, kEffectQueuedQuakeMs},
    {804, kQuakeTicks7, kEffectQueuedQuakeMs},
    {970, kQuakeTicks7, kEffectQueuedQuakeMs},
    {971, kQuakeTicks7, kEffectQueuedQuakeMs},
    {972, kQuakeTicks7, kEffectQueuedQuakeMs},
    {973, kQuakeTicks7, kEffectQueuedQuakeMs},
}};

}  // namespace

std::span<const EffectUpdateQuake> EffectUpdateQuakeTable() { return kEffectUpdateQuakes; }

}  // namespace ro::render
