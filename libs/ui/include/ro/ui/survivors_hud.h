#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include <imgui.h>

#include "ro/ui/survivors_effects.h"

namespace ro::ui {

// LE HUD D'UNE PARTIE DE « SURVIVANTS DE MIDGARD » (U12, U13), peint sur la
// liste de fond d'ImGui : sous toutes les fenêtres, par-dessus la scène.
//
//   - en haut, la barre d'expérience sur toute la largeur, le niveau dessus ;
//   - le minuteur centré dessous, puis la barre du boss quand il vit ;
//   - en haut à gauche les skills, puis les cartes (icône et niveau) ;
//   - en haut à droite les tués, les Zeny et le compteur discret des messages
//     privés reçus pendant la partie (U5), puis les charges et les vies, puis
//     la colonne des effets en cours ;
//   - sous le minuteur, la marque « Classée », « Défi » ou « Cursed Water n » ;
//   - en bas à droite, l'illustration de la carte ramassée, qui glisse ;
//   - des flèches au bord de l'écran vers le boss et les Old Blue Box hors du
//     champ ;
//   - la jauge verte de RO sous les pieds du héros, et au-dessus de lui l'avis
//     d'un objet ramassé.
//
// 🔴 IL NE LIT AUCUNE ENTRÉE et ne rend rien : il n'ouvre aucune fenêtre, donc
// ne prend ni le clavier ni la souris. La vue porte des identifiants de
// texture, jamais un chemin ; les noms (boss) arrivent déjà résolus par `app`.

// Une case de skill ou de carte.
struct SurvivorsHudSlot {
  ImTextureID icon = 0;  // 0 : pas d'icône, la case garde sa place
  int level = 0;
};

// Une charge de la Kafra (U12) : l'icône de l'objet RO et les charges qui
// restent dans la partie.
struct SurvivorsHudCharge {
  ImTextureID icon = 0;
  int count = 0;
};

// La carte qui vient d'être ramassée (Q6) : son illustration, à sa taille de
// texture, et où elle en est de sa vignette, de 0 à 1. Sans illustration,
// rien ne paraît.
struct SurvivorsHudCard {
  ImTextureID illustration = 0;
  ImVec2 size;
  float fraction = 0.0F;
};

enum class SurvivorsEdgeKind : std::uint8_t { kBoss, kChest };

// Une cible désignée par une flèche de bord quand elle sort du champ, à sa
// place projetée sur l'écran (hors du cadre, ou dedans : rien n'est alors
// peint).
struct SurvivorsEdgeTarget {
  ImVec2 screen_px;
  SurvivorsEdgeKind kind = SurvivorsEdgeKind::kChest;
};

struct SurvivorsHudView {
  int level = 1;
  float exp_fill = 0.0F;  // de 0 à 1, vers le niveau suivant
  std::uint64_t clock_ms = 0;
  std::uint32_t kills = 0;
  std::uint32_t zeny = 0;
  std::span<const SurvivorsHudSlot> skills;
  std::span<const SurvivorsHudSlot> cards;
  const char* boss_name = nullptr;  // nul : aucun boss en vie
  float boss_fill = 0.0F;
  std::span<const SurvivorsEdgeTarget> edge_targets;
  int unread_whispers = 0;  // 0 : rien n'est dit
  std::optional<ImVec2> hero_feet_px;
  float hero_fill = 1.0F;
  // Sous les compteurs, les charges qui restent et les vies ; une charge à
  // zéro garde sa case, des vies à zéro ne montrent rien.
  std::span<const SurvivorsHudCharge> charges;
  int lives = 0;
  ImTextureID life_icon = 0;  // l'icône de Resurrection
  // La marque sous le minuteur : « Classée » pour une partie classée,
  // sinon « Défi », sinon « Cursed Water n » au-delà du rang 0.
  bool ranked = false;
  bool challenge = false;
  int cursed_water = 0;
  // La vignette de la carte ramassée, dans le coin bas droit.
  SurvivorsHudCard found_card;
  // Sous les charges et les vies, la colonne des effets en cours ; au-dessus
  // du héros, les avis des objets ramassés (`survivors_effects.h`).
  std::span<const SurvivorsHudEffect> effects;
  std::span<const SurvivorsHudNotice> notices;
};

void DrawSurvivorsHud(const SurvivorsHudView& view);

// LA VIGNETTE QUI GLISSE : la part de sa largeur sortie du bord droit, de 0
// (cachée) à 1 (entière), le long de sa vie `fraction`. Elle entre pendant le
// premier dixième, reste, et repart pendant le dernier.
[[nodiscard]] float SurvivorsCardSlide(float fraction) noexcept;

// LE MINUTEUR, « mm:ss » ; au-delà d'une heure, les minutes continuent.
inline constexpr std::size_t kSurvivorsClockTextSize = 16;
[[nodiscard]] std::array<char, kSurvivorsClockTextSize> SurvivorsClockText(std::uint64_t clock_ms);

// LA PLACE D'UNE FLÈCHE DE BORD : sur le bord du cadre rentré de `inset_px`,
// au point où la droite du centre vers la cible le coupe, et l'angle de cette
// droite (radians, 0 vers la droite, sens horaire de l'écran). Rien quand la
// cible est dans le cadre rentré.
struct EdgeArrowPlace {
  ImVec2 tip;
  float angle_radians = 0.0F;
};
[[nodiscard]] std::optional<EdgeArrowPlace> PlaceEdgeArrow(ImVec2 frame_min, ImVec2 frame_max,
                                                           float inset_px, ImVec2 target);

}  // namespace ro::ui
