#pragma once

#include <cstdint>

#include "ro/render/camera.h"

namespace moonclient {

// CE QUE LE JOUEUR RÈGLE DE SON ÉCRAN DE VEILLE.
//
// ⚠ LES DÉFAUTS S'ÉCRIVENT ICI ET NULLE PART AILLEURS : la déclaration au
// fichier de réglages prend la valeur PRÉSENTE du champ, ce qui interdit une
// seconde source.

// CE QUI A LE DROIT DE RÉVEILLER.
//
// ⚠ CE RÉGLAGE NE DIT PAS ce qui compte comme activité — ça, c'est toujours
// tout. Il dit seulement ce qui ROUVRE les yeux une fois endormi. « Clavier
// seulement » existe pour la raison la plus banale qui soit : une souris posée
// sur un bureau qu'on bouscule, un capteur trop sensible, et la veille ne tient
// jamais.
enum class AfkWake : int {
  kAny = 0,       // clavier et souris
  kKeyboard = 1,  // le clavier seul
  kMouse = 2,     // la souris seule
};

inline constexpr int kAfkWakeModeCount = 3;

// ── LES BORNES DU DÉLAI ─────────────────────────────────────────────────────
inline constexpr int kLeastAfkDelaySeconds = 10;
inline constexpr int kMostAfkDelaySeconds = 900;
// L'annonce d'absence va plus loin : on éteint son écran au bout d'une minute
// et demie, on se déclare absent au bout de cinq — et parfois d'une heure.
inline constexpr int kMostAfkAnnounceSeconds = 3600;

// ── LA PLONGÉE, ET LE PIÈGE DE SON REPÈRE ───────────────────────────────────
//
// 🔴 LA PLONGÉE SE COMPTE DEPUIS LA VUE DE JEU, et c'est le même piège que pour
// le décor de connexion — voir `shell/backdrop_camera.h`.
//
// La veille du joueur se tient CINQ DEGRÉS SOUS la vue de jeu. Or notre vue de
// jeu n'est pas à 45 mais à `kGameElevationDegrees`, cinquante — mesurée par
// superposition d'images contre une capture du client. Une valeur absolue de
// 40 la mettrait cinq degrés plus bas que ce qu'il a réglé.
//
// C'est donc l'ÉCART qui se règle, et les trois valeurs s'en déduisent :
// personne ne peut plus en changer une sans voir les deux autres.
inline constexpr float kAfkTiltBelowGameDegrees = 5.0F;

// La plongée la plus basse offerte : trente degrés sous la vue de jeu.
inline constexpr float kAfkTiltLowestDropDegrees = 30.0F;
inline constexpr float kLeastAfkTiltDegrees =
    ro::render::kGameElevationDegrees - kAfkTiltLowestDropDegrees;
// Et la plus haute : celle de NOTRE moteur, et pas jusqu'à 90 : pile à la
// verticale, la direction du regard devient dégénérée. Nous avons déjà ce
// nombre, et il s'appelle `kMaxPitchDegrees`.
inline constexpr float kMostAfkTiltDegrees = ro::render::kMaxPitchDegrees;

// ── LE RECUL ────────────────────────────────────────────────────────────────
//
// 🔴 CES FACTEURS MULTIPLIENT `kGameViewDistance`, ET NON LA DISTANCE COURANTE.
// Comptés depuis celle où la molette se trouve à l'instant de la bascule, ils
// donneraient un cadrage différent à chaque endormissement, sans que rien à
// l'écran ne dise pourquoi. Depuis le recul de JEU, la veille rend la même
// image, que le joueur se soit rapproché ou éloigné juste avant. Voir
// `AfkScreen::PoseCamera`.
//
// ⚠ UN VAUT LE RECUL DE JEU : sous un, la veille se RAPPROCHE du personnage.
// Les bornes sont celles que le joueur a trouvées utilisables à l'écran.
inline constexpr float kLeastAfkZoom = 0.2F;
inline constexpr float kMostAfkZoom = 2.0F;

// ── L'ORBITE ET LA RAMPE ────────────────────────────────────────────────────
// Une vitesse et une durée : pas de repère non plus.
inline constexpr float kMostAfkSpinDegreesPerSecond = 30.0F;
inline constexpr float kLeastAfkEaseSeconds = 0.2F;
inline constexpr float kMostAfkEaseSeconds = 6.0F;

struct AfkSettings {
  // ⚠ ALLUMÉE D'USINE, ET C'EST UNE DEMANDE DU JOUEUR : c'est le client du
  // serveur, et ces valeurs sont celles qu'il a réglées puis choisies pour tout
  // le monde.
  bool enabled = true;
  // Silence avant bascule.
  int delay_s = 60;

  // ── PRÉVENIR L'ENTOURAGE ──────────────────────────────────────────────────
  //
  // 🔴 INDÉPENDANT DE `enabled`, et c'est tout l'intérêt : signaler son absence
  // et se regarder tourner sont deux envies distinctes, or la seconde coûte au
  // joueur son interface et sa caméra. Les lier ferait payer ce prix à qui veut
  // seulement prévenir ses amis. D'où un délai à part, plus long par défaut.
  //
  // ⚠ CES TROIS-LÀ NE PEIGNENT RIEN : le « zzz » et le « [AFK] » se voient des
  // AUTRES joueurs, et rien de ce qui se voit d'autrui ne peut venir de notre
  // machine. Ils composent un masque envoyé au serveur, qui seul peut montrer
  // un joueur au voisinage.
  //
  // ⚠ ALLUMÉE D'USINE ELLE AUSSI, et au MÊME délai que la veille : le joueur a
  // posé les deux à soixante secondes.
  bool announce = true;
  int announce_delay_s = 60;
  bool announce_zzz = true;
  bool announce_tag = true;

  // ── CAMÉRA ────────────────────────────────────────────────────────────────
  // Négatif = sens inverse ; zéro fige la vue.
  float spin_deg_s = 6.0F;
  // La hauteur du regard au-dessus de l'horizon. Voir
  // `kAfkTiltBelowGameDegrees` pour ce que ce nombre doit à celui du joueur, et
  // pourquoi il en diffère.
  float tilt_deg = ro::render::kGameElevationDegrees - kAfkTiltBelowGameDegrees;
  // Le recul, en multiples du recul de JEU — une distance fixe, et non celle
  // où la molette se trouvait. Voir `kLeastAfkZoom`.
  float zoom_factor = 0.65F;
  // La durée de la rampe d'ENTRÉE. Le retour se joue trois fois plus vite : il
  // répond à un geste du joueur, pas à une mise en scène.
  float ease_s = 2.5F;

  // ── HABILLAGE ─────────────────────────────────────────────────────────────
  // ⚠ « CLAVIER SEULEMENT » D'USINE, et c'est le réglage du joueur : une souris
  // posée sur un bureau qu'on bouscule, un capteur trop sensible, et la veille
  // ne tient jamais.
  int wake_on = static_cast<int>(AfkWake::kKeyboard);
  bool hide_ui = true;
  bool hide_cursor = true;
  float vignette = 0.45F;
  float desaturate = 0.0F;
  float grain = 0.0F;

  // ── HORLOGE ───────────────────────────────────────────────────────────────
  bool show_clock = true;
  bool show_away = true;
  // Ancrage sur la grille de neuf, en ordre de lecture. Sept = bas-centre.
  int clock_anchor = 7;
  int clock_margin = 233;
  // ⚠ SIX FOIS LA POLICE, et c'est bien ce que le joueur a réglé : la veille
  // n'a plus qu'elle à l'écran. ⚠ ELLE MULTIPLIE NOTRE POLICE DE BASE : c'est
  // un curseur, pas un repère.
  float clock_scale = 6.0F;
  bool clock_shadow = true;
  // ⚠ UN ENTIER ET NON UNE COULEUR TYPÉE : le fichier de réglages n'en connaît
  // pas, et un entier se relit à l'identique d'une version à l'autre. C'est
  // l'empaquetage d'ImGui, transparence comprise, comme la grille d'alignement.
  std::uint32_t clock_color = 0xFFFFFFFFU;
};

}  // namespace moonclient
