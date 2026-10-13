#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/ui/chat_link.h"
#include "ro/ui/chat_markup.h"
#include "ro/ui/chat_rich_art.h"

namespace ro::ui {

// LES BULLES D'ANNONCE, EN HAUT DE L'ÉCRAN.
//
// Le client pose chaque annonce du serveur dans une bulle encadrée, centrée en
// haut de l'écran : un liseré gris arrondi sur un voile noir, le texte dans sa
// couleur avec une ombre d'un pixel. Les bulles s'empilent vers le bas, la plus
// ancienne en tête, rangées par famille — les annonces, puis ce que l'équipe
// chuchote. L'avancée des chasses a son propre bandeau, plus bas.
//
// ⚠ `ui` NE SAIT RIEN DES ANNONCES. Elle reçoit des bulles déjà triées, leur
// texte décoré et leur opacité : la durée, la pile et le fondu se décident dans
// `app`.

// ── LA GÉOMÉTRIE DU CLIENT, en pixels ───────────────────────────────────────

// Le texte s'écarte du bord de six pixels sur les côtés, de quatre en haut et
// en bas, et deux lignes de quatre.
inline constexpr float kBalloonPadX = 6.0F;
inline constexpr float kBalloonPadY = 4.0F;
inline constexpr float kBalloonLineGap = 4.0F;
// Deux bulles d'une même famille sont séparées de dix pixels, deux familles de
// dix-sept.
inline constexpr float kBalloonGap = 10.0F;
inline constexpr float kBalloonFamilyGap = 17.0F;
// Les coins du liseré sont coupés sur trois pixels.
inline constexpr float kBalloonRounding = 3.0F;
// L'ombre du texte : un pixel en bas à droite.
inline constexpr float kBalloonShadowOffset = 1.0F;

// Le haut du bandeau des annonces, sous le bord de l'écran.
inline constexpr int kNativeBalloonTop = 51;
// LE BANDEAU DES CHASSES SE POSE À UNE FRACTION DE LA HAUTEUR : cent trente
// pixels sur quatre cent quatre-vingts.
inline constexpr float kNativeHuntTopOn480 = 130.0F;
inline constexpr float kNativeHuntReferenceHeight = 480.0F;

// LE REPLI DE L'ANNONCE SIMPLE. Le client la coupe tous les 72 octets ; dans sa
// police de douze pixels, un caractère occupe environ 5,3 pixels, et une rangée
// de 72 environ 380. On replie à la LARGEUR, jamais à l'octet : un caractère
// accentué tient sur deux octets, et une coupe à l'octet le briserait.
inline constexpr int kNativeBalloonWrapPixels = 380;

// ── LES BORNES DES RÉGLAGES ─────────────────────────────────────────────────

inline constexpr int kShortestBalloonMs = 1000;
inline constexpr int kLongestBalloonMs = 60000;
inline constexpr int kLeastBalloonsPerFamily = 1;
inline constexpr int kMostBalloonsPerFamily = 10;
inline constexpr int kLongestBalloonFadeMs = 5000;
inline constexpr int kLeastBalloonFontPercent = 60;
inline constexpr int kMostBalloonFontPercent = 250;
inline constexpr int kLeastBalloonWrapPixels = 150;
inline constexpr int kMostBalloonWrapPixels = 1600;

// OÙ SE POSE UN BANDEAU, et s'il se déplace.
//
// La place se prend à la souris, sur une poignée qui n'apparaît que
// déverrouillée. `center_x` NÉGATIF veut dire centré sur l'écran ; `top_y`
// NÉGATIF, la place du client.
struct BalloonAnchor {
  bool locked = true;
  int center_x = -1;
  int top_y = -1;
  friend bool operator==(const BalloonAnchor&, const BalloonAnchor&) = default;
};

// ── LES RÉGLAGES ────────────────────────────────────────────────────────────
//
// L'initialisateur de chaque champ EST son défaut, et celui du client sauf
// mention : le fichier de réglages n'écrit que ce qui s'en écarte.
struct BroadcastBalloonLook {
  // Montrer les bulles. Éteintes, les annonces ne vivent plus que dans la
  // conversation.
  bool shown = true;
  // Combien de temps une bulle tient.
  int duration_ms = 20000;
  // Combien de bulles d'une même famille restent à l'écran ; la plus ancienne
  // part pour la suivante. La chasse n'en garde jamais qu'une : chacune
  // remplace la précédente.
  int most_per_family = 3;
  // S'EFFACER EN FIN DE VIE plutôt que de disparaître d'un coup — le client,
  // lui, l'éteint net.
  bool fade = true;
  int fade_ms = 600;
  // Le corps du texte, en pourcentage de celui du client.
  int font_scale_percent = 100;
  // La largeur au-delà de laquelle une annonce simple se replie.
  int wrap_px = kNativeBalloonWrapPixels;
  // Le voile sous le texte et le liseré : un noir au quart d'opacité et un gris
  // plein chez le client.
  ImU32 background = IM_COL32(0x00, 0x00, 0x00, 0x40);
  ImU32 border = IM_COL32(0x5C, 0x5C, 0x5C, 0xFF);
  BalloonAnchor announcements;
  BalloonAnchor hunts;
  friend bool operator==(const BroadcastBalloonLook&, const BroadcastBalloonLook&) = default;
};

// ── CE QU'IL Y A À PEINDRE ──────────────────────────────────────────────────

// UNE LIGNE D'UNE BULLE : son texte décoré, comme une ligne du fil.
//
// ⚠ DES VUES : ce qui les porte doit survivre à l'image.
struct BalloonLineView {
  std::string_view text;
  ImU32 color = 0;
  std::span<const ChatLinkSpan> links;
  std::span<const ChatStyleRun> styles;
  std::span<const ChatInlineImage> images;
};

// UNE BULLE, dans l'ordre d'affichage.
struct BalloonView {
  std::span<const BalloonLineView> lines;
  // Le corps du texte, déjà mis à l'échelle.
  float font_px = 0.0F;
  // Se replie à la largeur réglée ; sinon une seule rangée par ligne.
  bool wraps = false;
  // De zéro à un : le fondu de fin de vie.
  float alpha = 1.0F;
  // Elle ouvre une nouvelle famille : l'écart qui la précède s'élargit.
  bool new_family = false;
};

// CE QU'UN BANDEAU DEMANDE EN RETOUR : la place que sa poignée vient de prendre.
// `moved` reste faux tant que rien n'a bougé.
struct BalloonAnchorAsk {
  bool moved = false;
  int center_x = 0;
  int top_y = 0;
};

// L'ÉTAT D'UNE POIGNÉE, POSSÉDÉ PAR L'APPELANT : il survit d'une image à
// l'autre.
struct BalloonAnchorGrip {
  // La largeur de la poignée, relue d'une image sur la suivante : on ne peut
  // centrer une fenêtre dont la taille n'est pas encore connue.
  float width_px = 0.0F;
  // Le coin que le dessin a POSÉ. 🔴 C'EST LUI QUI DIT SI LE JOUEUR A TIRÉ LA
  // POIGNÉE : une place recalculée qui s'en écarte vient d'un réglage, d'une
  // largeur ou d'un écran qui change, et se REPOSE ; une fenêtre qui s'en écarte
  // a été tirée.
  float applied_left_x = 0.0F;
  float applied_top_y = 0.0F;
  bool applied = false;
  // Replacer la poignée pour une image, quand un réglage a déplacé le bandeau
  // sous elle.
  bool replace = false;
};

// ── LA GÉOMÉTRIE, EN FONCTIONS PURES ────────────────────────────────────────

// Le haut d'un bandeau. Celui des annonces garde son ordonnée ; celui des
// chasses suit la hauteur de l'écran tant qu'on ne l'a pas placé.
[[nodiscard]] float AnnouncementBalloonTop(const BalloonAnchor& anchor);
[[nodiscard]] float HuntBalloonTop(const BalloonAnchor& anchor, float screen_height_px);
// Le centre d'un bandeau.
[[nodiscard]] float BalloonCenterX(const BalloonAnchor& anchor, float screen_width_px);

// LA TAILLE D'UNE BULLE : sa rangée la plus large plus les deux marges, et la
// hauteur empilée de ses rangées plus la marge du haut.
//
// ⚠ `stacked_px` CUMULE DES PAS INTERLIGNE COMPRIS : la dernière rangée apporte
// donc déjà l'écart qui la sépare du bord du bas.
[[nodiscard]] ImVec2 BalloonSize(float widest_row_px, float stacked_px);

// Le corps du texte : celui de la source, mis à l'échelle et arrondi au pixel
// — un corps fractionnaire rend le texte pâteux.
[[nodiscard]] float BalloonFontPixels(std::uint16_t source_px, int scale_percent);

// ── LE DESSIN ───────────────────────────────────────────────────────────────

// Peint un bandeau, et sa poignée si le verrou est ouvert.
//
// ⚠ SUR LA LISTE DE FOND : au-dessus du monde, SOUS les fenêtres, qu'une bulle
// ne doit jamais traverser.
//
// ⚠ À APPELER DANS L'IMAGE D'IMGUI, hors de toute fenêtre. `label` nomme la
// poignée, `grip_id` la distingue de celle de l'autre bandeau. `top_y` est le
// haut du bandeau, en pixels d'écran.
//
// `rich` est ce que le fil a pour peindre son texte riche. Nul : les styles se
// simulent.
void DrawBroadcastBalloons(std::span<const BalloonView> balloons, const BroadcastBalloonLook& look,
                           const BalloonAnchor& anchor, float top_y, const char* grip_id,
                           const char* label, BalloonAnchorGrip& grip, BalloonAnchorAsk* ask,
                           const ChatRichArt* rich = nullptr);

}  // namespace ro::ui
