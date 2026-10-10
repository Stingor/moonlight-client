#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/text.h"
#include "ro/ui/chat_log.h"
#include "ro/ui/chat_strip.h"
#include "ro/ui/skin_widgets.h"

#include "chat_log_parts.h"

// LA MISE EN PAGE DU FIL : où chaque mot d'une ligne se pose, et ce qui s'y
// clique. Les gestes, eux, vivent dans `chat_log.cpp`.
namespace ro::ui::chat_log_detail {
namespace {

// LE FAUX ITALIQUE : de combien un sommet glisse à droite par pixel au-dessus
// du pied de la rangée. Ni Malgun Gothic ni Tahoma n'ont de fichier italique :
// sans ce cisaillement, `*x*` ne se verrait pas.
//
// ⚠ LA PENTE N'EST PAS RÉSERVÉE DANS LA MESURE : le haut du dernier glyphe
// penché dépasse d'au plus `0,2 × taille` sur ce qui le suit. La réserver par
// morceau écarterait chaque mot italique de son voisin.
constexpr float kFauxItalicShear = 0.2F;
// L'épaisseur du souligné et de la barre.
constexpr float kChatRulePixels = 1.0F;
// Où passe la barre, en fraction de la taille du texte depuis le haut : au
// milieu des minuscules, pas au milieu de la cellule.
constexpr float kChatStrikeRatio = 0.55F;
// Le fond d'un `code` : un voile clair, qui se lit sur le fond sombre du fil
// sans éteindre le texte.
constexpr ImU32 kChatCodeBackColor = IM_COL32(255, 255, 255, 40);
// LE MASQUE D'UN SPOILER : opaque, sinon le texte se devinerait au travers.
constexpr ImU32 kChatSpoilerMaskColor = IM_COL32(90, 90, 90, 255);
// La case d'une vignette qui attend son image, et son bord.
constexpr ImU32 kChatPictureWaitColor = IM_COL32(255, 255, 255, 30);
constexpr ImU32 kChatPictureEdgeColor = IM_COL32(255, 255, 255, 90);
// L'air laissé après une image posée dans le texte, pour qu'elle ne colle pas
// au mot suivant.
constexpr float kChatPictureGapPixels = 2.0F;
constexpr double kMillisecondsPerSecond = 1000.0;
// Deux rectangles se prolongent quand leurs bords tombent à moins de ça : la
// plume avance par flottants, et le bord de l'un n'est pas exactement le début
// de l'autre.
constexpr float kSameSpot = 0.5F;
// LA TOLÉRANCE DU NOMBRE DE PAS d'une vignette : une hauteur qui vaut trois pas,
// arrondie un rien au-dessus par la division, ne doit pas en réserver quatre.
constexpr float kPictureRowSlack = 1.0e-3F;

// Referme la rangée en cours — sa largeur posée est notée —, descend d'une
// rangée et revient à gauche.
void Wrap(Pen* pen, float step) {
  if (pen->row_widths != nullptr) {
    pen->row_widths->push_back(pen->inked);
  }
  pen->x = 0.0F;
  pen->inked = 0.0F;
  ++pen->row_rank;
  pen->y += pen->row;
  pen->row = step;
}

// Le bord gauche de la rangée en cours, en coordonnées d'écran.
[[nodiscard]] float RowLeft(const PlaceFrame& frame, const Pen& pen) {
  const float shift = pen.row_rank < frame.row_shift.size() ? frame.row_shift[pen.row_rank] : 0.0F;
  return frame.origin.x + shift;
}

// Étend le dernier rectangle quand le morceau se pose dans son prolongement, en
// ouvre un neuf sinon. « Dans son prolongement » veut dire : même rangée, et le
// texte reprend là où le précédent s'arrête.
//
// ⚠ UNE VIGNETTE NE S'ÉTEND PAS ET N'EST PAS ÉTENDUE : son rectangle a la
// hauteur de l'image, pas celle du texte, et l'allonger d'un mot voisin
// rendrait cliquable le vide sous ce mot.
void NoteHit(std::vector<LinkHit>* hits, const ChatLinkSpan& link, std::string_view label,
             ImVec2 top_left, ImVec2 extent, std::uint16_t spoiler, bool picture = false) {
  if (hits == nullptr) {
    return;
  }
  // ⚠ LE PROLONGEMENT SE RECONNAÎT AU JETON, et non à l'identifiant : un style
  // n'en a pas, et deux liens voisins du même objet à des raffinements
  // différents partagent le sien. Le jeton, lui, désigne un lien et un seul.
  if (!picture && !hits->empty()) {
    LinkHit& last = hits->back();
    if (!last.picture && last.kind == link.kind && last.token == link.token &&
        std::fabs(last.top_left.y - top_left.y) < kSameSpot &&
        std::fabs(last.bottom_right.x - top_left.x) < kSameSpot) {
      last.bottom_right.x = top_left.x + extent.x;
      return;
    }
  }
  hits->push_back(LinkHit{.top_left = top_left,
                          .bottom_right = ImVec2(top_left.x + extent.x, top_left.y + extent.y),
                          .kind = link.kind,
                          .item_id = link.item_id,
                          .token = link.token,
                          .label = label,
                          .refusals = link.refusals,
                          .mvp = link.mvp,
                          .navi = link.navi,
                          .mark_at = link.mark_at,
                          .mark_length = link.mark_length,
                          .url = link.url,
                          .picture = picture,
                          .spoiler = spoiler});
}

// Le même prolongement, pour le masque d'un spoiler.
void NoteSpoiler(std::vector<SpoilerHit>* spoilers, const ChatSpoilerKey& key, ImVec2 top_left,
                 ImVec2 extent) {
  if (spoilers == nullptr) {
    return;
  }
  if (!spoilers->empty()) {
    SpoilerHit& last = spoilers->back();
    if (std::fabs(last.top_left.y - top_left.y) < kSameSpot &&
        std::fabs(last.bottom_right.x - top_left.x) < kSameSpot) {
      last.bottom_right.x = top_left.x + extent.x;
      return;
    }
  }
  spoilers->push_back(
      SpoilerHit{.top_left = top_left,
                 .bottom_right = ImVec2(top_left.x + extent.x, top_left.y + extent.y),
                 .line_key = key});
}

// COMMENT UN FRAGMENT SE PEINT : sa police, sa couleur et ses traits.
struct Ink {
  ImFont* font = nullptr;
  ImU32 color = 0;
  ImU32 shadow = 0;
  bool fake_bold = false;
  bool slanted = false;
  bool underline = false;
  bool strike = false;
  bool code = false;
  // Un spoiler encore caché : le masque à la place du texte.
  bool masked = false;
  std::uint16_t spoiler = 0;
};

// CE QUE LES FRAGMENTS D'UNE LIGNE PARTAGENT.
struct Stage {
  const PlaceFrame* frame = nullptr;
  const PlaceTarget* target = nullptr;
  std::string_view line_text;
  float font_size = 0.0F;
  // Nul : ni police de style, ni image — le texte et ses traits simulés.
  const ChatRichArt* rich = nullptr;
  // La ligne, pour le clic qui révèle ; nul quand elle n'a pas de spoiler ou
  // qu'on mesure sans peindre.
  const ChatSpoilerKey* spoiler_key = nullptr;
};

// ⚠ SEULEMENT CE QUI SE VOIT se peint et se clique : une rangée hors de la vue
// n'a pas de rectangle, donc rien à cliquer là où rien ne se voit.
[[nodiscard]] bool Visible(const Stage& stage, const Pen& pen) {
  return stage.target->into != nullptr && pen.y + pen.row >= stage.frame->view_top &&
         pen.y <= stage.frame->view_bottom;
}

[[nodiscard]] ImVec2 At(const Stage& stage, const Pen& pen) {
  return ImVec2(RowLeft(*stage.frame, pen) + pen.x, stage.frame->origin.y + pen.y);
}

// Penche les sommets posés depuis `first_vertex` : le pied de la rangée reste,
// le haut part à droite.
void Slant(ImDrawList* into, int first_vertex, float foot_y) {
  for (int rank = first_vertex; rank < into->VtxBuffer.Size; ++rank) {
    ImDrawVert& vertex = into->VtxBuffer[rank];
    vertex.pos.x += (foot_y - vertex.pos.y) * kFauxItalicShear;
  }
}

// Peint un morceau déjà mesuré : son fond, son texte, ses traits — ou son
// masque. `begin == end` pour une espace, qui n'a que fond et traits.
//
// ⚠ DANS CET ORDRE, et c'est l'ordre des sommets qui le décide : un fond posé
// après le texte le recouvrirait.
void PaintPiece(const Stage& stage, const Ink& ink, ImVec2 where, float width, const char* begin,
                const char* end) {
  ImDrawList* into = stage.target->into;
  const float size = stage.font_size;
  const ImVec2 far(where.x + width, where.y + size);
  if (ink.masked) {
    // 🔴 LE TEXTE CACHÉ N'EST PAS PEINT DU TOUT, pas seulement recouvert : le
    // bord lissé d'un glyphe dépasserait du masque.
    into->AddRectFilled(where, far, kChatSpoilerMaskColor);
    if (stage.spoiler_key != nullptr) {
      NoteSpoiler(stage.target->spoilers, *stage.spoiler_key, where, ImVec2(width, size));
    }
    return;
  }
  if (ink.code) {
    into->AddRectFilled(where, far, kChatCodeBackColor);
  }
  if (begin != end) {
    const int first_vertex = into->VtxBuffer.Size;
    if (ink.shadow != 0) {
      into->AddText(ink.font, size,
                    ImVec2(where.x + kChatDamagedShadowOffset, where.y + kChatDamagedShadowOffset),
                    ink.shadow, begin, end);
    }
    into->AddText(ink.font, size, where, ink.color, begin, end);
    if (ink.fake_bold) {
      into->AddText(ink.font, size, ImVec2(where.x + kFakeBoldPixels, where.y), ink.color, begin,
                    end);
    }
    if (ink.slanted) {
      Slant(into, first_vertex, where.y + size);
    }
  }
  // Le souligné passe un pixel SOUS le glyphe : c'est le débord que
  // `ChatLineOverhang` réserve sous la dernière rangée.
  if (ink.underline) {
    into->AddRectFilled(ImVec2(where.x, far.y), ImVec2(far.x, far.y + kChatRulePixels), ink.color);
  }
  if (ink.strike) {
    const float strike_y = where.y + size * kChatStrikeRatio;
    into->AddRectFilled(ImVec2(where.x, strike_y), ImVec2(far.x, strike_y + kChatRulePixels),
                        ink.color);
  }
}

// Pose un fragment de texte en le repliant, et avance la plume.
//
// `into` nul = ON MESURE SANS PEINDRE. C'est ce qui permet à `ChatLogHeight`
// d'employer ce moteur-ci plutôt qu'une copie.
//
// 🔴 LA MÊME POLICE MESURE ET PEINT, et le faux gras compte dans la mesure :
// mesuré dans une police et peint dans une autre, un mot déborderait de sa
// rangée ou laisserait un trou, et le cache de hauteurs mentirait.
//
// ⚠ UN MOT PEUT ÊTRE PLUS LARGE QUE TOUTE LA ZONE. Le repli mot à mot n'a alors
// rien à couper : il descend le mot d'une rangée, où il déborde exactement
// pareil, et la fin du texte sort de la fenêtre. C'est ce que font un
// « AAAA…AAA » de trois cents caractères, une adresse sans espace ou un nom de
// fichier — tous les jours dans un chat public. Le mot est donc découpé AU
// CARACTÈRE quand il ne tient pas.
//
// `link` non nul = ce fragment désigne quelque chose, et ses rectangles
// s'accumulent dans `hits` pour que l'appelant les mette au clic.
void PlaceText(const Stage& stage, std::string_view text, const Ink& ink, const ChatLinkSpan* link,
               Pen* pen) {
  const PlaceFrame& frame = *stage.frame;
  ImFont* font = ink.font;
  const float size = stage.font_size;
  const float wrap_width = frame.wrap_width;
  const float space_width = font->CalcTextSizeA(size, FLT_MAX, 0.0F, " ").x;
  // Ce que le faux gras ajoute à chaque morceau peint.
  const float bold_extra = ink.fake_bold ? kFakeBoldPixels : 0.0F;
  const std::string_view label =
      link != nullptr ? stage.line_text.substr(link->offset, link->length) : std::string_view();
  const char* const begin = text.data();
  const char* const end = begin + text.size();
  const char* at = begin;

  while (at < end) {
    // ⚠ LE SAUT DE LIGNE EST UN SÉPARATEUR, au même titre que l'espace. Sans ce
    // cas, le mot qui le suit serait passé tel quel au dessin : la MESURE
    // s'arrête au saut et rend une largeur nulle, tandis que le RENDU l'honore et
    // pose le mot une rangée plus bas, au `x` courant — donc en plein milieu du
    // vide. Le mot semble lâché à droite, et tout ce qui suit se réempile là.
    if (*at == '\n') {
      Wrap(pen, frame.step);
      ++at;
      continue;
    }
    if (*at == '\r') {
      ++at;
      continue;
    }
    if (*at == ' ') {
      // 🔴 L'ESPACE D'UN LIEN SE CLIQUE COMME SES LETTRES : la zone d'un lien est
      // le morceau entier sur sa rangée, sans trou entre deux mots. Le mot
      // suivant prolonge ce rectangle-ci ; une espace qui finit une rangée ne
      // rejoint pas la suivante, dont le `y` diffère.
      //
      // ⚠ UNE ESPACE EN BOUT DE RANGÉE S'ARRÊTE AU BORD : son fond, son trait ou
      // son masque ne débordent pas de la zone du fil.
      const float shown_width = std::min(space_width, wrap_width - pen->x);
      if (shown_width > 0.0F && Visible(stage, *pen)) {
        const ImVec2 where = At(stage, *pen);
        PaintPiece(stage, ink, where, shown_width, at, at);
        if (link != nullptr && !ink.masked) {
          NoteHit(stage.target->hits, *link, label, where, ImVec2(shown_width, size), ink.spoiler);
        }
      }
      pen->x += space_width;
      ++at;
      continue;
    }
    const char* word_end = at;
    while (word_end < end && *word_end != ' ' && *word_end != '\n' && *word_end != '\r') {
      ++word_end;
    }
    while (at < word_end) {
      // Une seule mesure, BORNÉE à la place restante : elle répond aux deux
      // questions d'un coup — la largeur à dessiner, et par `stop` le point où le
      // mot cesse de rentrer. `stop` tombe sur une frontière de CODEPOINT, jamais
      // au milieu d'un accent.
      const char* stop = word_end;
      float width =
          font->CalcTextSizeA(size, wrap_width - pen->x - bold_extra, 0.0F, at, word_end, &stop).x +
          bold_extra;
      // Ça ne rentre pas, mais la rangée a déjà du contenu : on descend d'abord —
      // le mot a droit à la largeur entière avant qu'on envisage de le couper.
      if (stop != word_end && pen->x > 0.0F) {
        Wrap(pen, frame.step);
        stop = word_end;
        width = font->CalcTextSizeA(size, wrap_width - bold_extra, 0.0F, at, word_end, &stop).x +
                bold_extra;
      }
      // ⚠ TOUTE LA LARGEUR ET MÊME PAS UN GLYPHE — une fenêtre réduite à quelques
      // pixels : `stop` ne bouge pas, `at` non plus, et la boucle tourne pour
      // toujours. C'est un GEL, pas un défaut d'affichage. On pose un caractère
      // quoi qu'il arrive, ses octets de continuation avec lui.
      if (stop <= at) {
        stop = at + 1;
        while (stop < word_end && (static_cast<unsigned char>(*stop) & 0xC0U) == 0x80U) {
          ++stop;
        }
        width = font->CalcTextSizeA(size, FLT_MAX, 0.0F, at, stop).x + bold_extra;
      }
      if (Visible(stage, *pen)) {
        const ImVec2 where = At(stage, *pen);
        PaintPiece(stage, ink, where, width, at, stop);
        if (link != nullptr && !ink.masked) {
          NoteHit(stage.target->hits, *link, label, where, ImVec2(width, size), ink.spoiler);
        }
      }
      pen->x += width;
      pen->inked = pen->x;
      const bool cut = stop != word_end;
      at = stop;
      // Mot coupé en plein milieu : la suite descend d'une rangée. Le repli est
      // fait ICI, sans attendre le tour suivant — la place restante ne vaut plus
      // qu'un caractère au mieux.
      if (cut) {
        Wrap(pen, frame.step);
      }
    }
  }
}

// Fait place à une image de `width` pixels : elle descend d'une rangée si elle
// ne tient pas sur celle-ci, la rangée prend au moins `reserved` de haut, et la
// plume avance. Rend son coin, ou rien si elle ne se voit pas.
std::optional<ImVec2> MakeRoom(const Stage& stage, float width, float reserved, Pen* pen) {
  if (pen->x > 0.0F && pen->x + width > stage.frame->wrap_width) {
    Wrap(pen, stage.frame->step);
  }
  pen->row = std::max(pen->row, reserved);
  std::optional<ImVec2> corner;
  if (Visible(stage, *pen)) {
    corner = At(stage, *pen);
  }
  pen->inked = pen->x + width;
  pen->x += width + kChatPictureGapPixels;
  return corner;
}

// La vignette attend son image : `app` apprend qu'elle se voit, et la demande.
void NoteSeenPicture(const ChatRichArt& rich, std::uint32_t picture_key) {
  if (rich.seen_pictures == nullptr) {
    return;
  }
  if (std::ranges::find(*rich.seen_pictures, picture_key) == rich.seen_pictures->end()) {
    rich.seen_pictures->push_back(picture_key);
  }
}

// Pose une image du texte. Faux : elle ne se peint pas en image — réglage
// éteint, emote absente, image en échec —, et son repli textuel garde sa place.
//
// ⚠ UNE IMAGE DANS UN SPOILER CACHÉ SE MASQUE comme le texte, à sa taille.
bool PlaceImage(const Stage& stage, const ChatInlineImage& image, const Ink& ink,
                const ChatLinkSpan* link, Pen* pen) {
  if (stage.rich == nullptr) {
    return false;
  }
  const ChatRichArt& rich = *stage.rich;
  const float size = stage.font_size;
  // 🔴 UNE EMOTE OU UNE ICÔNE TIENT DANS SA RANGÉE : au plus un pas de côté, et
  // centrée. À la taille du texte, plus haute que le pas, elle mordrait sur la
  // rangée suivante.
  const float side = std::min(size, stage.frame->step);
  ImDrawList* into = stage.target->into;
  const std::string_view label =
      link != nullptr ? stage.line_text.substr(link->offset, link->length) : std::string_view();
  // Le masque d'un spoiler caché, ou le rectangle cliquable d'un lien.
  const auto finish = [&](ImVec2 corner, ImVec2 extent, bool picture) {
    if (ink.masked) {
      into->AddRectFilled(corner, ImVec2(corner.x + extent.x, corner.y + extent.y),
                          kChatSpoilerMaskColor);
      if (stage.spoiler_key != nullptr) {
        NoteSpoiler(stage.target->spoilers, *stage.spoiler_key, corner, extent);
      }
    } else if (link != nullptr) {
      NoteHit(stage.target->hits, *link, label, corner, extent, ink.spoiler, picture);
    }
  };
  switch (image.kind) {
    case ChatInlineKind::kGameEmote: {
      if (!rich.emote_images || image.emote_action >= rich.emotes_by_action.size()) {
        return false;
      }
      const ChatEmotion& emotion = rich.emotes_by_action[image.emote_action];
      if (emotion.frames.empty()) {
        return false;
      }
      // ⚠ SANS AGRANDIR LA RANGÉE : une emote est un mot.
      if (const std::optional<ImVec2> row = MakeRoom(stage, side, 0.0F, pen)) {
        const ImVec2 corner(row->x, row->y + (pen->row - side) * 0.5F);
        if (!ink.masked) {
          PaintEmotion(into, emotion, corner, side, stage.frame->now_s);
        }
        finish(corner, ImVec2(side, side), false);
      }
      return true;
    }
    case ChatInlineKind::kItemIcon: {
      if (!rich.item_icons || image.texture == 0) {
        return false;
      }
      const float width = image.width_px > 0.0F && image.height_px > 0.0F
                              ? side * image.width_px / image.height_px
                              : side;
      if (const std::optional<ImVec2> row = MakeRoom(stage, width, 0.0F, pen)) {
        const ImVec2 corner(row->x, row->y + (pen->row - side) * 0.5F);
        if (!ink.masked) {
          into->AddImage(image.texture, corner, ImVec2(corner.x + width, corner.y + side));
        }
        finish(corner, ImVec2(width, side), false);
      }
      return true;
    }
    case ChatInlineKind::kPicture: {
      if (rich.picture_pixels <= 0.0F || image.state == ChatPictureState::kFailed) {
        return false;
      }
      const ImTextureID shown = image.state == ChatPictureState::kReady
                                    ? ShownFrame(image.frames, image.texture, stage.frame->now_s)
                                    : 0;
      float height = image.line_high ? side : rich.picture_pixels;
      // Tant qu'elle attend, la vignette est carrée : ses proportions ne sont
      // pas encore connues.
      float width = height;
      if (shown != 0 && image.width_px > 0.0F && image.height_px > 0.0F) {
        width = height * image.width_px / image.height_px;
      }
      // Trop large pour le fil : elle rapetisse en gardant ses proportions.
      const float wrap_width = stage.frame->wrap_width;
      if (width > wrap_width && wrap_width > 0.0F) {
        height *= wrap_width / width;
        width = wrap_width;
      }
      // 🔴 LA RANGÉE GRANDIT D'UN MULTIPLE ENTIER DU PAS, jamais de la hauteur
      // exacte de l'image : le défilement se cale sur le pas. ⚠ CELLE QUI TIENT
      // DANS LA RANGÉE ne l'agrandit pas : elle s'y centre, comme une icône.
      const float reserved = image.line_high ? 0.0F : PictureRowHeight(height, stage.frame->step);
      if (std::optional<ImVec2> corner = MakeRoom(stage, width, reserved, pen)) {
        if (image.line_high) {
          corner->y += (pen->row - height) * 0.5F;
        }
        const ImVec2 far(corner->x + width, corner->y + height);
        if (ink.masked) {
          // Le masque suffit : rien de l'image ne se devine.
        } else if (shown != 0) {
          into->AddImage(shown, *corner, far);
        } else {
          into->AddRectFilled(*corner, far, kChatPictureWaitColor);
          into->AddRect(*corner, far, kChatPictureEdgeColor);
          NoteSeenPicture(rich, image.picture_key);
        }
        // ⚠ LA VIGNETTE QUI RECOUVRE UN LIEN EN DEVIENT LA ZONE : le clic sur
        // l'image vise l'adresse qu'elle remplace.
        finish(*corner, ImVec2(width, height), true);
      }
      return true;
    }
  }
  return false;
}

// UNE DES LISTES TRIÉES D'UNE LIGNE — ses liens, ses styles —, parcourue au
// rythme du texte.
//
// ⚠ UN MORCEAU HORS BORNES NE DOIT PAS FAIRE LIRE À CÔTÉ : la ligne et ses
// morceaux viennent de deux endroits, et rien ne garantit ici qu'ils sont
// d'accord. Un morceau vide, qui déborde ou qui chevauche le précédent est
// sauté.
template <typename Piece>
class Walk {
 public:
  Walk(std::span<const Piece> pieces, std::size_t size) : pieces_(pieces), size_(size) {
    rank_ = Next(0, 0);
  }

  // Le morceau qui couvre l'octet `at`, ou nul.
  [[nodiscard]] const Piece* Covering(std::size_t at) const {
    return rank_ < pieces_.size() && pieces_[rank_].offset <= at ? &pieces_[rank_] : nullptr;
  }
  // Le prochain octet où la couverture change.
  [[nodiscard]] std::size_t Boundary(std::size_t at) const {
    if (rank_ >= pieces_.size()) {
      return size_;
    }
    const Piece& piece = pieces_[rank_];
    return piece.offset <= at ? End(piece) : piece.offset;
  }
  // Oublie les morceaux finis avant `at`.
  void Pass(std::size_t at) {
    while (rank_ < pieces_.size() && End(pieces_[rank_]) <= at) {
      rank_ = Next(rank_ + 1, End(pieces_[rank_]));
    }
  }

 private:
  [[nodiscard]] static std::size_t End(const Piece& piece) {
    return static_cast<std::size_t>(piece.offset) + piece.length;
  }
  [[nodiscard]] std::size_t Next(std::size_t rank, std::size_t floor) const {
    while (rank < pieces_.size()) {
      const Piece& piece = pieces_[rank];
      if (piece.length != 0 && piece.offset >= floor && piece.offset <= size_ &&
          piece.length <= size_ - piece.offset) {
        return rank;
      }
      ++rank;
    }
    return rank;
  }

  std::span<const Piece> pieces_;
  std::size_t size_ = 0;
  std::size_t rank_ = 0;
};

// L'ENCRE D'UN FRAGMENT : son style, et le lien qu'il porte.
//
// ⚠ LA POLICE D'UN STYLE MANQUE SOUVENT, et c'est alors la police courante qui
// le simule : repassée un pixel plus loin pour le gras, penchée pour
// l'italique. Le code garde la police courante, sur son fond.
[[nodiscard]] Ink InkFor(const Stage& stage, const ChatStyleRun* run, const ChatLinkSpan* link,
                         ImU32 line_color, bool revealed) {
  const std::uint8_t style = run != nullptr ? run->style : 0;
  const bool bold = (style & kChatBold) != 0;
  const bool italic = (style & kChatItalic) != 0;
  const bool code = (style & kChatCode) != 0;
  ImFont* const base = stage.frame->font;
  // 🔴 UNE POLICE DE STYLE ÉGALE À CELLE DU FIL N'EN EST PAS UNE : l'étage
  // d'interface y repose la police ordinaire faute de fichier, et la prendre
  // pour un vrai gras ou un vrai italique effacerait le style.
  const auto own = [base](ImFont* styled) { return styled != base ? styled : nullptr; };
  ImFont* const bold_font = stage.rich != nullptr ? own(stage.rich->bold) : nullptr;
  ImFont* const italic_font = stage.rich != nullptr ? own(stage.rich->italic) : nullptr;
  ImFont* const code_font = stage.rich != nullptr ? own(stage.rich->code) : nullptr;
  Ink ink;
  ink.font = base;
  if (code) {
    if (code_font != nullptr) {
      ink.font = code_font;
    }
  } else if (italic && italic_font != nullptr) {
    ink.font = italic_font;
  } else if (bold && bold_font != nullptr) {
    ink.font = bold_font;
  }
  ink.fake_bold = bold && ink.font != bold_font;
  ink.slanted = italic && ink.font != italic_font;
  ink.underline = (style & kChatUnderline) != 0;
  ink.strike = (style & kChatStrike) != 0;
  ink.code = code;
  ink.masked = (style & kChatSpoiler) != 0 && !revealed;
  ink.spoiler = run != nullptr ? run->spoiler : 0;
  ImU32 color = run != nullptr && run->color != 0 ? run->color : line_color;
  if (link != nullptr) {
    // ⚠ UN PSEUDO GARDE LA COULEUR DE SON TEXTE : peint en orange, il passerait
    // pour un objet lié et promettrait son menu. La main au survol suffit. Un
    // respawn ÉCHU s'éteint.
    if (link->kind != ChatLinkKind::kPlayer && link->kind != ChatLinkKind::kDiscordName) {
      color = link->spent ? kChatLinkSpentColor : kChatLinkColor;
    }
    if (link->damaged) {
      ink.shadow = kChatDamagedShadow;
    }
  }
  ink.color = color;
  return ink;
}

// Pose le texte d'une ligne en fragments de même encre, et ses images.
//
// ⚠ EN FRAGMENTS, ET NON EN UN SEUL APPEL : chaque fragment a sa couleur et sa
// police, et le repli doit couper la zone cliquable au bon endroit. Poser toute
// la ligne puis repasser dessus pour la colorer demanderait de refaire toute la
// mesure.
void PlaceLine(const Stage& stage, const ChatLineView& line, ImU32 color, bool revealed, Pen* pen) {
  const std::size_t size = line.text.size();
  Walk<ChatLinkSpan> links(line.links, size);
  Walk<ChatStyleRun> runs(line.styles, size);
  std::size_t image_rank = 0;
  std::size_t at = 0;
  while (true) {
    const ChatLinkSpan* link = links.Covering(at);
    const ChatStyleRun* run = runs.Covering(at);
    const Ink ink = InkFor(stage, run, link, color, revealed);
    // Les images posées ICI : celles qui s'insèrent, puis celle qui remplace ce
    // qui suit. ⚠ UNE IMAGE DONT L'ORIGINE EST DÉJÀ DÉPASSÉE NE SE POSE PAS : ses
    // octets sont sous une autre image.
    std::size_t replaced = 0;
    while (image_rank < line.images.size() && line.images[image_rank].offset <= at) {
      const ChatInlineImage& image = line.images[image_rank];
      ++image_rank;
      if (image.offset < at || image.length > size - image.offset) {
        continue;
      }
      if (PlaceImage(stage, image, ink, link, pen) && image.length != 0) {
        replaced = image.length;
        break;
      }
    }
    if (replaced != 0) {
      at += replaced;
      links.Pass(at);
      runs.Pass(at);
      continue;
    }
    if (at >= size) {
      break;
    }
    std::size_t next = std::min(links.Boundary(at), runs.Boundary(at));
    if (image_rank < line.images.size()) {
      next = std::min(next, std::max<std::size_t>(line.images[image_rank].offset, at + 1));
    }
    next = std::min(next, size);
    PlaceText(stage, line.text.substr(at, next - at), ink, link, pen);
    at = next;
    links.Pass(at);
    runs.Pass(at);
  }
}

[[nodiscard]] bool CarriesSpoiler(const ChatLineView& line) {
  return std::ranges::any_of(
      line.styles, [](const ChatStyleRun& run) { return (run.style & kChatSpoiler) != 0; });
}

}  // namespace

void LayOutLine(const PlaceFrame& frame, const PlaceTarget& target, const ChatLineView& line,
                Pen* pen) {
  ImDrawList* into = target.into;
  ImFont* font = frame.font;
  const ChatLogStyle& style = *frame.style;
  const ImVec2 origin = frame.origin;
  const float view_top = frame.view_top;
  const float view_bottom = frame.view_bottom;
  if (style.timestamps) {
    std::array<char, kStampSize> stamp{};
    std::snprintf(stamp.data(), stamp.size(), kStampFormat, line.hour, line.minute, line.second);
    const float width = font->CalcTextSizeA(style.font_size, FLT_MAX, 0.0F, stamp.data()).x;
    if (into != nullptr && pen->y + pen->row >= view_top && pen->y <= view_bottom) {
      into->AddText(font, style.font_size, ImVec2(RowLeft(frame, *pen) + pen->x, origin.y + pen->y),
                    kChatStampColor, stamp.data());
    }
    pen->x += width;
    pen->inked = pen->x;
  }
  // Le genre, puis le type : deux étiquettes sous la même forme.
  for (const std::string_view named : {line.tag, line.type_tag}) {
    if (named.empty()) {
      continue;
    }
    // Les crochets se posent ICI et nulle part ailleurs : `app` rend un NOM, et
    // lui faire porter aussi la ponctuation demanderait une seconde table à
    // tenir d'accord avec la première.
    std::array<char, kTagSize> tag{};
    std::snprintf(tag.data(), tag.size(), kTagFormat, static_cast<int>(named.size()), named.data());
    // ⚠ UN NOM TROP LONG SE COUPE À L'OCTET : on ne garde que des caractères
    // entiers, sans quoi la queue s'afficherait en carré.
    tag[core::WithoutDanglingUtf8(tag.data()).size()] = '\0';
    const float width = font->CalcTextSizeA(style.font_size, FLT_MAX, 0.0F, tag.data()).x;
    if (into != nullptr && pen->y + pen->row >= view_top && pen->y <= view_bottom) {
      into->AddText(font, style.font_size, ImVec2(RowLeft(frame, *pen) + pen->x, origin.y + pen->y),
                    kChatTagColor, tag.data());
    }
    pen->x += width;
    pen->inked = pen->x;
  }
  // ⚠ LA CLÉ D'UN SPOILER NE SE COMPOSE QUE POUR PEINDRE une ligne qui en porte
  // un : sans identité, c'est une copie du texte. La mesure n'en a pas besoin —
  // caché ou révélé, un spoiler a la même largeur.
  std::optional<ChatSpoilerKey> spoiler_key;
  bool revealed = false;
  if (into != nullptr && CarriesSpoiler(line)) {
    spoiler_key = SpoilerKey(line);
    revealed = frame.memory != nullptr && frame.memory->Revealed(*spoiler_key);
  }
  const Stage stage{.frame = &frame,
                    .target = &target,
                    .line_text = line.text,
                    .font_size = style.font_size,
                    .rich = style.rich,
                    .spoiler_key = spoiler_key.has_value() ? &*spoiler_key : nullptr};
  PlaceLine(stage, line, line.color != 0 ? line.color : kChatPlainColor, revealed, pen);
  // La dernière rangée de la ligne se referme ici.
  Wrap(pen, frame.step);
}

float LineHeight(ImFont* font, const ChatLineView& line, const ChatLogStyle& style,
                 float wrap_width) {
  const float step = ChatLineStep(style);
  Pen pen;
  pen.row = step;
  const PlaceFrame frame{.font = font,
                         .style = &style,
                         .origin = ImVec2(0.0F, 0.0F),
                         .wrap_width = wrap_width,
                         .step = step,
                         .view_top = -FLT_MAX,
                         .view_bottom = FLT_MAX};
  LayOutLine(frame, PlaceTarget{}, line, &pen);
  return pen.y;
}

// ⚠ RÉSERVER LE SEUL CUMUL DES PAS CROIT LA DERNIÈRE LIGNE D'UN DEMI-CARACTÈRE :
// le pas est plus court que le texte dès que l'interligne est serré, et c'est ce
// cumul qui borne le défilement. La ligne la plus récente serait rognée en
// permanence, quel que soit le réglage.
float WithOverhang(float stacked, const ChatLogStyle& style) {
  return stacked > 0.0F ? stacked + ChatLineOverhang(style) : 0.0F;
}

// 🔴 COMPARÉE CHAMP À CHAMP, JAMAIS HACHÉE : l'identité quand la ligne en a une,
// sinon son texte entier.
ChatSpoilerKey SpoilerKey(const ChatLineView& line) {
  if (line.id != 0) {
    return ChatSpoilerKey{.line_id = line.id};
  }
  return ChatSpoilerKey{.line_text = std::string(line.text)};
}

float PictureRowHeight(float picture_px, float step) {
  if (step <= 0.0F) {
    return picture_px;
  }
  return std::ceil(picture_px / step - kPictureRowSlack) * step;
}

ImTextureID ShownFrame(std::span<const ChatPictureFrame> frames, ImTextureID still, double now_s) {
  if (frames.empty()) {
    return still;
  }
  const auto elapsed_ms = static_cast<std::uint64_t>(std::max(0.0, now_s) * kMillisecondsPerSecond);
  const std::size_t shown = std::min(ChatPictureFrameAt(frames, elapsed_ms), frames.size() - 1);
  return frames[shown].texture;
}

}  // namespace ro::ui::chat_log_detail
