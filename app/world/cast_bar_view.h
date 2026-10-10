#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "ro/render/actor_pick.h"
#include "ro/render/camera.h"
#include "ro/sim/actor.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/cast_bars.h"
#include "ro/ui/chat_bubble.h"
#include "ro/ui/stat_bars.h"

namespace moonclient {

// ── LES INCANTATIONS EN COURS, PROJETÉES À L'ÉCRAN ──────────────────────────
//
// Récoltées ici, peintes par l'étage d'interface, comme les jauges de vie et
// les bulles : la barre qui remplace celle du client écrit le nom du sort, et
// ce texte veut la police de l'interface.

// L'INCANTATION QU'UN ACTEUR MONTRE À CET INSTANT, ou rien.
//
// 🔴 UNE SEULE BARRE PAR ACTEUR : l'incantation, la barre qu'une compétence
// annoncée pose et celle d'un script sont la même jauge chez le client, que la
// dernière pose reprend. Une incantation ÉCHUE n'en montre plus : le serveur n'annonce pas
// qu'un sort aboutit, il en a donné la durée.
struct ShownCast {
  std::uint32_t started_ms = 0;
  std::uint32_t duration_ms = 0;
  // Zéro quand la barre ne vient pas d'une incantation : celle d'une annonce
  // n'a pas de sort à nommer.
  std::uint16_t skill_id = 0;
};
[[nodiscard]] bool ShownCastOf(const ro::sim::Actor& actor, std::uint32_t now_ms, ShownCast* into);

// LE JOUEUR PORTE-T-IL UNE BARRE à cet instant : incantation, compétence
// annoncée ou barre de progression d'un script. C'est ce qui garde la souris
// vivante sous un dialogue — voir `WorldMouseFrozen`.
[[nodiscard]] bool OwnBarShown(const ro::sim::World& world, std::uint32_t now_ms);

// MON INCANTATION, POUR LA JAUGE « CAST » DU HUD : l'écoulé sur la durée, et le
// texte déjà composé — « Storm Gust  1,4 s », ou le temps seul pour un sort
// qu'on ne sait pas nommer. Inconnue hors incantation : la jauge ne se dessine
// alors pas, plutôt que de rester vide en prenant la place.
[[nodiscard]] ro::ui::StatBarValue OwnCastGauge(const ro::sim::World& world,
                                                const ro::sim::SkillTree* skills,
                                                std::uint32_t now_ms);

// LES BARRES DES ACTEURS DESSINÉS, chacune ancrée au haut du sien, et rangée
// dans l'espèce de ses barres de vie : voir `EntityBarKindOf`.
//
// `bubbles` sont celles qui se peignent à cette image : une barre dont
// l'acteur parle sait ainsi que la place au-dessus est prise. `skills` nomme
// les sorts, ou est nul — seul le temps s'écrit alors.
//
// ⚠ HORS DE L'ÉCRAN, PAS DE BARRE : une barre plaquée au bord pour un lanceur
// qu'on ne voit pas se lirait comme la sienne.
[[nodiscard]] std::vector<ro::ui::CastBarView> CastBarsOnScreen(
    const ro::render::Camera& camera, float aspect_ratio,
    std::span<const ro::render::DrawnActor> drawn, std::span<const std::uint32_t> ids,
    const ro::sim::World& world, const ro::sim::PartyRoster& party,
    const ro::sim::SkillTree* skills, std::span<const ro::ui::ChatBubbleView> bubbles,
    std::uint32_t now_ms, float viewport_width, float viewport_height);

}  // namespace moonclient
