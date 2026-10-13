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

// LES BULLES DE PAROLE, AU-DESSUS DES TÊTES.
//
// 🔴 ELLES PASSENT PAR LE MÊME TEXTE QUE LA CONVERSATION, et c'est tout
// l'intérêt. Un atlas de police à part — gravé en NIVEAUX DE GRIS et borné au
// latin-1 — n'aurait pour un emoji ni point de code ni couleur, alors que la
// même phrase s'affiche correctement dans le fil. Deux piles de texte pour la
// même phrase finissent toujours par diverger.
//
// ⚠ SUR LA LISTE DE FOND, et c'est ce qui donne l'ordre : au-dessus du monde et
// des jauges de vie, que le renderer a déjà peints ; SOUS toutes les fenêtres,
// qu'une bulle ne doit jamais traverser.
//
// ⚠ `ui` NE SAIT TOUJOURS RIEN DU MONDE. Elle reçoit un point d'écran et un
// texte — des données pures, comme la `FrameScene` du renderer. C'est `app` qui
// projette une tête en pixels.

// Ce qu'une bulle montre, et combien de temps.
//
// 🔴 LE CLIENT DONNE CINQ SECONDES À TOUT LE MONDE, et c'est son défaut qu'on
// garde : un pavé de trois lignes y disparaît aussi vite qu'un « ok ». La
// rallonge par caractère répare cela pour qui la veut, sans changer ce que le
// joueur connaît.
struct ChatBubbleLook {
  // Voir aussi CE QU'ON DIT SOI-MÊME. Le client ne le montre pas — on vient de
  // le taper — mais l'écho du serveur dit ce qui est VRAIMENT parti, sigil
  // compris, et c'est parfois la surprise.
  bool show_own = true;
  // La bulle s'efface-t-elle en fin de vie, plutôt que de s'éteindre d'un coup ?
  bool fade = true;
  // Vrai : cinq secondes pour tout le monde, comme le client. Faux : la durée
  // de base plus la rallonge par caractère.
  bool client_timing = false;
  float base_ms = 5000.0F;
  float per_character_ms = 120.0F;
  // De combien la bulle monte au-dessus de ce que son porteur occupe : de quoi
  // laisser passer la jauge de vie.
  float rise_pixels = 14.0F;
  // La largeur qu'une bulle ne dépasse pas, en FRACTION de l'écran. Zéro la
  // laisse d'un seul tenant, quitte à sortir de l'écran.
  float width_fraction = 0.60F;
  // De zéro à un, appliquée à TOUT : fond, liseré et texte. ⚠ EN FACTEUR SUR LE
  // FONDU et non à sa place, sinon baisser l'opacité ferait reparaître une bulle
  // en pleine extinction.
  float opacity = 1.0F;

  // ── LE FOND ───────────────────────────────────────────
  //
  // 🔴 LE CLIENT NE PEINT QU'UN LISERÉ SUR DU TRANSPARENT, et sa bulle devient
  // illisible dès que le décor est clair — un mur blanc, une barre de vie rose,
  // et la phrase disparaît dedans. On garde SON liseré, on ajoute un fond.
  bool background = true;
  ImU32 background_color = IM_COL32(16, 16, 16, 165);
  ImU32 border_color = IM_COL32(0x5C, 0x5C, 0x5C, 255);
  float corner_radius = 4.0F;
  float padding_x = 6.0F;
  float padding_y = 3.0F;
};

// Les bornes des réglages, là où un nombre libre n'aurait pas de sens.
inline constexpr float kShortestBubbleMs = 500.0F;
inline constexpr float kLongestBubbleMs = 30000.0F;
inline constexpr float kMostBubblePerCharacterMs = 500.0F;
inline constexpr float kMostBubbleRisePixels = 120.0F;
inline constexpr float kMostBubbleCornerRadius = 12.0F;
inline constexpr float kMostBubblePadding = 20.0F;

// Ce que le client d'origine donne à toute bulle, quelle qu'en soit la longueur.
inline constexpr float kClientBubbleMs = 5000.0F;

// COMBIEN DE CARACTÈRES LA BULLE MONTRE, en points de code de `text` — le texte
// DÉCORÉ, balisage déjà retiré. Une image qui remplace son repli — une emote
// du jeu à la place de son `:nom:` — compte pour un seul ; une image insérée —
// l'icône d'un objet — pour aucun.
[[nodiscard]] std::size_t ChatBubbleShownCharacters(std::string_view text,
                                                    std::span<const ChatInlineImage> images);

// Combien de temps CETTE parole-là flotte, selon le réglage : la rallonge se
// compte sur ce que la bulle MONTRE, jamais sur les marques qu'elle a perdues.
[[nodiscard]] float ChatBubbleLifeMs(const ChatBubbleLook& look, std::string_view text,
                                     std::span<const ChatInlineImage> images = {});

// La plus longue vie qu'une bulle puisse avoir sous ce réglage, quelle que
// soit sa phrase : au-delà, inutile de la décorer pour la mesurer.
[[nodiscard]] inline float ChatBubbleLongestLifeMs(const ChatBubbleLook& look) {
  return look.client_timing ? kClientBubbleMs : kLongestBubbleMs;
}

// UNE BULLE À PEINDRE.
//
// ⚠ `head` EST LE POINT D'OÙ ELLE MONTE : le milieu du haut de ce que son
// porteur occupe à l'écran. La bulle pousse VERS LE HAUT depuis là.
struct ChatBubbleView {
  ImVec2 head{0.0F, 0.0F};
  std::string_view text;
  float elapsed_ms = 0.0F;
  // LES MORCEAUX QUI DÉSIGNENT UN OBJET, comme ceux d'une ligne du fil.
  //
  // 🔴 LA BULLE MONTRE LA MÊME PHRASE QUE LE FIL, et il faut donc qu'elle passe
  // par la même traduction : sans elle, ce que le joueur lit au-dessus de sa tête
  // est la BALISE — « <ITEML>0001215IU%04&00'00)00… » — pendant que son fil
  // affiche « +4 Knife [3] ».
  //
  // ⚠ ELLE NE SE CLIQUE PAS. Une bulle vit cinq secondes, flotte au-dessus du
  // monde et suit son porteur : la couleur dit ce que c'est, et le fil est là
  // pour le geste.
  std::span<const ChatLinkSpan> links;
  // LE TEXTE RICHE, comme celui d'une ligne du fil : gras, italique, couleurs,
  // spoilers en suites d'octets de `text`, et les emotes du jeu et les icônes
  // d'objet posées dedans.
  //
  // ⚠ UN SPOILER Y RESTE MASQUÉ, toujours : la bulle ne se clique pas, et c'est
  // le fil qui le révèle.
  std::span<const ChatStyleRun> styles;
  std::span<const ChatInlineImage> images;
  // LA COULEUR DU TEXTE, quand sa source en impose une : le message d'une
  // Talkie Box. Zéro garde celle du thème.
  ImU32 text_color = 0;
  // QUI PARLE, ou zéro pour ce qui n'est pas un acteur. Une barre
  // d'incantation y lit que la place au-dessus de la tête est prise.
  std::uint32_t speaker_id = 0;
};

// Peint toutes les bulles. ⚠ À APPELER DANS L'IMAGE D'IMGUI, hors de toute
// fenêtre : elle écrit sur la liste de fond.
//
// 🔴 LE MÊME MOTEUR QUE LE FIL pose le texte d'une bulle, dans la police
// courante : mêmes styles, mêmes couleurs, même repli au mot puis au caractère.
// Seul l'alignement diffère — chaque rangée est centrée sur la tête.
//
// `rich` est ce que le fil a pour peindre son texte riche — les polices de
// style et les emotes du jeu. Nul : les styles se simulent, et une emote reste
// son nom. Les vignettes n'y paraissent jamais : une bulle ne télécharge rien.
void DrawChatBubbles(std::span<const ChatBubbleView> bubbles, const ChatBubbleLook& look,
                     const ChatRichArt* rich = nullptr);

// LA MÊME PEINTURE, SUR LA LISTE DONNÉE : l'aperçu des réglages pose ses bulles
// dans son cadre. `most_width_px` est la largeur qu'une bulle ne dépasse pas ;
// zéro la laisse d'un seul tenant.
void PaintChatBubbles(ImDrawList& into, std::span<const ChatBubbleView> bubbles,
                      const ChatBubbleLook& look, float most_width_px,
                      const ChatRichArt* rich = nullptr);

}  // namespace ro::ui
