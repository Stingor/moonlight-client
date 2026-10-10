#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/ui/chat_link.h"
#include "ro/ui/chat_log.h"
#include "ro/ui/chat_rich_art.h"

// LE MOTEUR DU FIL, partagé entre sa mise en page (`chat_log_place.cpp`) et ses
// gestes (`chat_log.cpp`).
//
// ⚠ UN EN-TÊTE INTERNE, et il n'a pas vocation à sortir de `src/` : personne
// hors du fil ne pose une ligne, et ses rectangles n'ont de sens que posés sur
// la liste de dessin qui vient d'être remplie.
namespace ro::ui::chat_log_detail {

// L'horodatage tel qu'il se lit, plus l'espace qui le sépare de la phrase.
inline constexpr const char* kStampFormat = "[%02d:%02d:%02d] ";
// Assez pour « [23:59:59] » et son zéro terminal.
inline constexpr std::size_t kStampSize = 16;

// L'étiquette du diagnostic, et de quoi loger le plus long nom de genre.
inline constexpr const char* kTagFormat = "[%.*s] ";
inline constexpr std::size_t kTagSize = 32;

// L'image d'une animation à montrer à l'instant `now_s`, ou `still` pour une
// image fixe ; zéro si aucune.
[[nodiscard]] ImTextureID ShownFrame(std::span<const ChatPictureFrame> frames, ImTextureID still,
                                     double now_s);

// L'état du parcours : où en est la plume, et quelle hauteur fait la rangée
// qu'elle est en train de remplir.
struct Pen {
  float x = 0.0F;
  float y = 0.0F;
  // ⚠ LA HAUTEUR APPARTIENT À LA RANGÉE, PAS À LA LIGNE : elle repart au pas
  // ordinaire à chaque repli. Une vignette l'agrandit, toujours d'un multiple
  // entier du pas — voir `PictureRowHeight`.
  float row = 0.0F;
  // Le rang de la rangée en cours, à partir de zéro.
  std::size_t row_rank = 0;
  // Où finit ce qui est posé sur la rangée en cours : l'espace qui la termine
  // n'en fait pas partie.
  float inked = 0.0F;
  // SORTIE, ou nul : la largeur posée de chaque rangée refermée, dans l'ordre.
  // C'est ce qu'une surface qui centre ses rangées mesure d'abord.
  std::vector<float>* row_widths = nullptr;
};

// UN RECTANGLE CLIQUABLE, en coordonnées d'écran, et ce qu'il ouvre.
//
// 🔴 UN LIEN QUI SE REPLIE EN A DEUX, et c'est la raison d'être de cette liste
// plutôt que d'un rectangle par morceau : un nom coupé en fin de rangée continue
// à gauche de la suivante, et un seul rectangle engloberait tout ce qu'il y a
// entre les deux — la moitié droite de la rangée du dessus, la gauche de celle
// du dessous, et le texte d'autrui au passage.
struct LinkHit {
  ImVec2 top_left;
  ImVec2 bottom_right;
  ChatLinkKind kind = ChatLinkKind::kItem;
  std::uint32_t item_id = 0;
  std::uint32_t token = 0;
  // Le nom du lien, pour la tête du menu : il dit de QUEL mot il parle.
  std::string_view label;
  // Ce que le menu d'un joueur refuse d'avance.
  const PlayerLinkRefusals* refusals = nullptr;
  // Ce que le menu d'un respawn de MVP sait d'avance.
  const MvpLinkMenuFacts* mvp = nullptr;
  // Ce que le menu d'une recherche de navigation sait d'avance.
  NaviSearchMenuFacts navi;
  // La marque « · passé » d'un respawn échu, DANS le nom : la tête du menu la
  // retire.
  std::uint32_t mark_at = 0;
  std::uint32_t mark_length = 0;
  // Ce qu'on sait d'une adresse web, pour son infobulle et son menu.
  const ChatUrlFacts* url = nullptr;
  // Le rectangle est celui d'une vignette qui recouvre le lien.
  bool picture = false;
  // Le rang du spoiler qui le cache, zéro hors spoiler.
  std::uint16_t spoiler = 0;
};

// UN SPOILER ENCORE MASQUÉ : son rectangle, et la ligne qu'un clic révèle.
struct SpoilerHit {
  ImVec2 top_left;
  ImVec2 bottom_right;
  ChatSpoilerKey line_key;
};

// Ce que la pose d'une ligne lit, le même pour toutes les lignes d'une image.
struct PlaceFrame {
  ImFont* font = nullptr;
  const ChatLogStyle* style = nullptr;
  ImVec2 origin{0.0F, 0.0F};
  float wrap_width = 0.0F;
  float step = 0.0F;
  // Les bornes de ce qui se voit, en coordonnées locales.
  float view_top = 0.0F;
  float view_bottom = 0.0F;
  // L'horloge des animations, en secondes.
  double now_s = 0.0;
  // Ce que le fil retient — les spoilers révélés. Nul : rien n'est révélé.
  const ChatLogLayout* memory = nullptr;
  // De combien chaque rangée se pose à droite de `origin`, par rang. Vide, ou
  // plus court que les rangées : aucun décalage — le fil aligne à gauche.
  std::span<const float> row_shift;
};

// Où la pose écrit. Tout nul : ON MESURE SANS PEINDRE.
struct PlaceTarget {
  ImDrawList* into = nullptr;
  std::vector<LinkHit>* hits = nullptr;
  std::vector<SpoilerHit>* spoilers = nullptr;
};

// Pose UNE ligne — son horodatage, son étiquette, puis son texte en morceaux —
// et referme sa dernière rangée : la plume repart à gauche, sous elle.
void LayOutLine(const PlaceFrame& frame, const PlaceTarget& target, const ChatLineView& line,
                Pen* pen);
// La hauteur d'une ligne seule : le même moteur, sans peindre.
[[nodiscard]] float LineHeight(ImFont* font, const ChatLineView& line, const ChatLogStyle& style,
                               float wrap_width);
// Le cumul des hauteurs, plus le débord du dernier glyphe.
[[nodiscard]] float WithOverhang(float stacked, const ChatLogStyle& style);
// La ligne qui porte un spoiler : son identité, ou son texte quand elle n'en a
// pas.
[[nodiscard]] ChatSpoilerKey SpoilerKey(const ChatLineView& line);
// LA HAUTEUR RÉSERVÉE À UNE RANGÉE QUI PORTE UNE VIGNETTE : un multiple entier du
// pas, pour que le défilement reste calé sur les rangées.
[[nodiscard]] float PictureRowHeight(float picture_px, float step);

}  // namespace ro::ui::chat_log_detail
