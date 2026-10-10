#include "ro/ui/chat_log.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "chat_log_parts.h"

namespace ro::ui {
namespace {

using chat_log_detail::LayOutLine;
using chat_log_detail::LineHeight;
using chat_log_detail::LinkHit;
using chat_log_detail::Pen;
using chat_log_detail::PlaceFrame;
using chat_log_detail::PlaceTarget;
using chat_log_detail::SpoilerHit;
using chat_log_detail::WithOverhang;

// L'identifiant du menu d'un lien du fil, et celui de la case où sa cible attend.
//
// 🔴 LA CIBLE SURVIT À L'IMAGE, et il le faut : le menu s'ouvre à l'image
// SUIVANTE — l'identifiant d'un popup se hache avec la pile d'ids courante, donc
// l'ouverture se demande et ne se fait pas sur-le-champ —, et le lien n'est alors
// plus forcément survolé.
//
// ⚠ DANS LE MAGASIN DE LA FENÊTRE, jamais dans une variable de fichier : deux
// chatbox détachées ont chacune leur fil, et une cible partagée ferait ouvrir
// l'une sur le lien de l'autre. C'est le même rangement que la sélection du
// texte riche.
constexpr const char* kLinkMenuId = "lien";
constexpr const char* kLinkMenuTargetKey = "##cible_lien";
// LE MENU D'UNE LIGNE. Ce qu'il copiera est retenu par le cache du fil, au clic.
constexpr const char* kLineMenuId = "ligne";
// L'instant du dernier zoom, et la molette cumulée sous Ctrl.
constexpr const char* kZoomShownKey = "##zoom_vu";
constexpr const char* kZoomWheelKey = "##zoom_molette";
// Combien de temps la taille du texte se montre après un cran de zoom.
constexpr double kChatZoomShownSeconds = 1.2;
// LA MOLETTE FINE d'un pavé tactile envoie des dixièmes : dix d'entre eux font
// un cran, à l'arrondi des flottants près.
constexpr float kWheelNotchSlack = 1.0e-3F;
// Le rang de spoiler d'une suite qui n'en porte pas, pour la recopier quand même
// entre ses barres.
constexpr std::uint16_t kUnrankedSpoiler = 0xFFFF;
constexpr std::string_view kSpoilerBars = "||";

// LE BANDEAU D'UNE LIGNE QUI NOUS NOMME : un voile doré sous le texte, qui se
// voit en survolant le fil sans éteindre la couleur de la ligne.
constexpr ImU32 kChatMentionBandColor = IM_COL32(255, 200, 60, 50);
// La pastille des messages arrivés pendant qu'on lisait plus haut.
constexpr ImU32 kChatUnreadPillColor = IM_COL32(30, 30, 30, 220);
constexpr ImU32 kChatUnreadPillHoverColor = IM_COL32(60, 60, 60, 230);
constexpr ImU32 kChatUnreadTextColor = IM_COL32(255, 255, 255, 255);
constexpr float kChatUnreadPaddingPixels = 4.0F;
// Elle flotte au-dessus du bas de la vue, pas collée à lui.
constexpr float kChatUnreadLiftPixels = 4.0F;
constexpr std::size_t kUnreadLabelSize = 64;
// De quoi loger le statut d'un aperçu, nom de site compris.
constexpr std::size_t kUrlStatusSize = 512;
// L'écart entre la souris et l'aperçu d'une adresse, pour qu'il ne la cache pas.
constexpr float kChatPreviewOffsetPixels = 16.0F;

// L'APERÇU PRÊT D'UNE ADRESSE, posé près de la souris : l'image seule, sans
// fond ni cadre.
//
// ⚠ SUR LE PREMIER PLAN, ET NON DANS UNE INFOBULLE : une infobulle a un corps,
// et l'image n'en veut pas. Elle reste dans l'écran, quitte à passer à gauche
// ou au-dessus de la souris.
void PaintUrlPreview(const ChatUrlFacts& facts, ImTextureID shown) {
  const ImGuiIO& io = ImGui::GetIO();
  const ImVec2 size(facts.width_px, facts.height_px);
  ImVec2 corner(io.MousePos.x + kChatPreviewOffsetPixels, io.MousePos.y + kChatPreviewOffsetPixels);
  if (corner.x + size.x > io.DisplaySize.x) {
    corner.x = std::max(0.0F, io.MousePos.x - kChatPreviewOffsetPixels - size.x);
  }
  if (corner.y + size.y > io.DisplaySize.y) {
    corner.y = std::max(0.0F, io.MousePos.y - kChatPreviewOffsetPixels - size.y);
  }
  ImGui::GetForegroundDrawList()->AddImage(shown, corner,
                                           ImVec2(corner.x + size.x, corner.y + size.y));
}

// L'INFOBULLE D'UNE ADRESSE : l'image seule quand elle est prête ; sinon
// l'adresse entière, puis ce qu'il advient de son aperçu.
//
// 🔴 LE STATUT NE PARLE QU'À QUI A DEMANDÉ DES APERÇUS — au survol, ou pour
// cette adresse-là. Les autres ne voient que l'adresse : leur dire qu'un aperçu
// n'a pas été chargé, c'est du bruit pour qui n'en veut pas.
void DrawUrlTooltip(const LinkHit& hit) {
  const ChatUrlFacts* facts = hit.url;
  if (facts != nullptr && facts->preview == ChatPictureState::kReady && facts->width_px > 0.0F &&
      facts->height_px > 0.0F) {
    if (const ImTextureID shown =
            chat_log_detail::ShownFrame(facts->frames, facts->texture, ImGui::GetTime());
        shown != 0) {
      PaintUrlPreview(*facts, shown);
      return;
    }
  }
  std::string said(hit.label);
  if (facts != nullptr && facts->statuses) {
    std::array<char, kUrlStatusSize> status{};
    if (!facts->allowed) {
      const std::string host = facts->host.empty() ? core::Tr("ce site") : std::string(facts->host);
      std::snprintf(status.data(), status.size(),
                    core::Tr("Aperçu non chargé : %s n'est pas dans vos sites autorisés. Clic "
                             "droit pour l'afficher."),
                    host.c_str());
    } else if (facts->preview == ChatPictureState::kPending) {
      std::snprintf(status.data(), status.size(), "%s", core::Tr("Chargement de l'aperçu…"));
    } else if (facts->preview == ChatPictureState::kFailed) {
      std::snprintf(status.data(), status.size(), "%s", core::Tr("Aperçu indisponible."));
    }
    if (status[0] != '\0') {
      said += '\n';
      said += status.data();
    }
  }
  SkinTooltipFormat("%s", said.c_str());
}

// La ligne sous l'ordonnée locale `local_y`, ou `lines.size()`.
[[nodiscard]] std::size_t LineAt(std::span<const float> heights, float local_y) {
  float top = 0.0F;
  for (std::size_t rank = 0; rank < heights.size(); ++rank) {
    const float bottom = top + heights[rank];
    if (local_y >= top && local_y < bottom) {
      return rank;
    }
    top = bottom;
  }
  return heights.size();
}

// Le menu d'une ligne : la copier, entière ou sans ce qui la présente.
//
// ⚠ CE QU'IL COPIE A ÉTÉ RETENU AU CLIC : la ligne a pu partir — le fil ne
// garde qu'un nombre borné de lignes — ou changer de rang depuis. Sans cache, rien
// n'est retenu, et les entrées restent grisées.
void DrawLineMenu(const ChatLogLayout& laid) {
  if (!BeginSkinPopup(kLineMenuId)) {
    return;
  }
  const bool held = laid.holds_copy();
  if (ImGui::MenuItem(core::Tr("Copier la ligne"), nullptr, false, held)) {
    ImGui::SetClipboardText(laid.held_line().c_str());
  }
  if (ImGui::MenuItem(core::Tr("Copier le texte seul"), nullptr, false, held)) {
    ImGui::SetClipboardText(laid.held_body().c_str());
  }
  EndSkinPopup();
}

// Combien de lignes sont arrivées après la dernière lue. ⚠ PAR LA FIN : les
// `seq` croissent le long du fil, et le compte s'arrête à la première ligne déjà
// vue. Une ligne sans `seq` ne se compte pas.
[[nodiscard]] int CountUnread(std::span<const ChatLineView> lines, std::uint64_t seen_through) {
  int unread = 0;
  for (std::size_t rank = lines.size(); rank > 0; --rank) {
    const std::uint64_t seq = lines[rank - 1].seq;
    if (seq == 0) {
      continue;
    }
    if (seq <= seen_through) {
      break;
    }
    ++unread;
  }
  return unread;
}

// Le dernier `seq` du fil, zéro s'il n'en porte aucun.
[[nodiscard]] std::uint64_t LastSeq(std::span<const ChatLineView> lines) {
  for (std::size_t rank = lines.size(); rank > 0; --rank) {
    if (lines[rank - 1].seq != 0) {
      return lines[rank - 1].seq;
    }
  }
  return 0;
}

// Recopie `text` à partir de `from`, chaque spoiler repris entre ses barres.
//
// ⚠ PAR RANG, ET NON PAR SUITE : un spoiler dont une partie est en gras tient en
// deux suites, et ne se recopie qu'entre une seule paire de barres.
void AppendRewrapped(std::string_view text, std::size_t from, std::span<const ChatStyleRun> styles,
                     std::string* out) {
  std::size_t at = from;
  std::uint16_t open = 0;
  const auto close = [&] {
    if (open != 0) {
      *out += kSpoilerBars;
      open = 0;
    }
  };
  for (const ChatStyleRun& run : styles) {
    const std::size_t begin = run.offset;
    const std::size_t end = begin + run.length;
    if (run.length == 0 || end > text.size() || end <= at || begin < at) {
      continue;
    }
    if (begin > at) {
      close();
      *out += text.substr(at, begin - at);
    }
    const std::uint16_t rank = (run.style & kChatSpoiler) == 0 ? 0
                               : run.spoiler != 0              ? run.spoiler
                                                               : kUnrankedSpoiler;
    if (rank != open) {
      close();
      if (rank != 0) {
        *out += kSpoilerBars;
        open = rank;
      }
    }
    *out += text.substr(begin, end - begin);
    at = end;
  }
  close();
  if (at < text.size()) {
    *out += text.substr(at);
  }
}

// LA PASTILLE « N NOUVEAUX MESSAGES », en bas au centre de la vue. Rend vrai
// quand la souris est dessus.
//
// ⚠ SON TEXTE PASSE PAR `RenderText`, comme celui d'un élément : c'est ce qui le
// met dans le journal d'ImGui, où le banc le relit.
[[nodiscard]] bool PaintUnreadPill(ImDrawList* into, int unread) {
  std::array<char, kUnreadLabelSize> label{};
  if (unread == 1) {
    std::snprintf(label.data(), label.size(), "%s", core::Tr("1 nouveau message"));
  } else {
    std::snprintf(label.data(), label.size(), core::Tr("%d nouveaux messages"), unread);
  }
  const ImVec2 text = ImGui::CalcTextSize(label.data());
  const ImVec2 window_at = ImGui::GetWindowPos();
  const ImVec2 window_size = ImGui::GetWindowSize();
  const float pill_width = text.x + 2.0F * kChatUnreadPaddingPixels;
  const float pill_height = text.y + 2.0F * kChatUnreadPaddingPixels;
  const ImVec2 top_left(window_at.x + (window_size.x - pill_width) * 0.5F,
                        window_at.y + window_size.y - pill_height - kChatUnreadLiftPixels);
  const ImVec2 bottom_right(top_left.x + pill_width, top_left.y + pill_height);
  const bool hovered =
      ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(top_left, bottom_right, false);
  into->AddRectFilled(top_left, bottom_right,
                      hovered ? kChatUnreadPillHoverColor : kChatUnreadPillColor,
                      pill_height * 0.5F);
  ImGui::PushStyleColor(ImGuiCol_Text, kChatUnreadTextColor);
  ImGui::RenderText(
      ImVec2(top_left.x + kChatUnreadPaddingPixels, top_left.y + kChatUnreadPaddingPixels),
      label.data());
  ImGui::PopStyleColor();
  return hovered;
}

[[nodiscard]] ImGuiID FontIdOf(const ImFont* font) { return font != nullptr ? font->FontId : 0; }

}  // namespace

float ChatLineStep(const ChatLogStyle& style) {
  const float step = style.font_size + style.line_gap - kChatLineTightening;
  return std::max(step, kChatSmallestLineStep);
}

float ChatLineOverhang(const ChatLogStyle& style) {
  // Le +1 : un soulignement se trace un pixel SOUS le glyphe, et c'est lui le
  // point le plus bas de la rangée.
  return std::max(0.0F, style.font_size + 1.0F - ChatLineStep(style));
}

float ChatLogHeight(std::span<const ChatLineView> lines, const ChatLogStyle& style,
                    float wrap_width) {
  ImFont* font = ImGui::GetFont();
  float stacked = 0.0F;
  for (const ChatLineView& line : lines) {
    stacked += LineHeight(font, line, style, wrap_width);
  }
  return WithOverhang(stacked, style);
}

ChatLogLayout::ImageFacts ChatLogLayout::FactsOf(const ChatInlineImage& image,
                                                 const ChatRichArt& rich) {
  bool paintable = image.texture != 0 || !image.frames.empty();
  if (image.kind == ChatInlineKind::kGameEmote) {
    paintable = image.emote_action < rich.emotes_by_action.size() &&
                !rich.emotes_by_action[image.emote_action].frames.empty();
  }
  return ImageFacts{.kind = image.kind,
                    .emote_action = image.emote_action,
                    .paintable = paintable,
                    .width_px = image.width_px,
                    .height_px = image.height_px,
                    .state = image.state};
}

bool ChatLogLayout::SameImages(const ChatLineView& line, const ChatRichArt& rich,
                               std::uint32_t first, std::uint32_t count) const {
  if (line.images.size() != count) {
    return false;
  }
  for (std::uint32_t rank = 0; rank < count; ++rank) {
    if (!(FactsOf(line.images[rank], rich) == images_[first + rank])) {
      return false;
    }
  }
  return true;
}

float ChatLogLayout::Update(std::span<const ChatLineView> lines, const ChatLogStyle& style,
                            float wrap_width) {
  ImFont* font = ImGui::GetFont();
  // Sans texte riche, les champs qui le décrivent gardent leur défaut.
  const ChatRichArt unadorned{.emote_images = false, .item_icons = false};
  const ChatRichArt& rich = style.rich != nullptr ? *style.rich : unadorned;
  const Key key{.font = font,
                .font_id = font->FontId,
                .font_size = style.font_size,
                .line_gap = style.line_gap,
                .timestamps = style.timestamps,
                .wrap_width = wrap_width,
                .bold = rich.bold,
                .bold_id = FontIdOf(rich.bold),
                .italic = rich.italic,
                .italic_id = FontIdOf(rich.italic),
                .code = rich.code,
                .code_id = FontIdOf(rich.code),
                .picture_pixels = rich.picture_pixels,
                .emote_images = rich.emote_images,
                .item_icons = rich.item_icons};
  if (!(key == key_)) {
    key_ = key;
    ids_.clear();
    heights_.clear();
    image_first_.clear();
    images_.clear();
  }
  next_ids_.clear();
  next_heights_.clear();
  next_image_first_.clear();
  next_images_.clear();
  measured_ = 0;
  // Les deux listes vont dans le même sens : la recherche avance avec le fil, et
  // ne revient jamais en arrière.
  std::size_t kept = 0;
  std::uint64_t previous = 0;
  float stacked = 0.0F;
  for (const ChatLineView& line : lines) {
    float height = -1.0F;
    // ⚠ UNE IDENTITÉ QUI NE CROÎT PAS NE SE CHERCHE PAS. Répétée, elle rendrait
    // la hauteur que le cache tient sous ce numéro — celle d'une autre ligne ;
    // en recul, la recherche, qui n'avance que dans un sens, ne la trouverait de
    // toute façon pas. Celle-ci se remesure, simplement.
    if (line.id != 0 && line.id > previous) {
      previous = line.id;
      while (kept < ids_.size() && ids_[kept] < line.id) {
        ++kept;
      }
      // ⚠ LA MÊME IDENTITÉ NE SUFFIT PAS : une image qui arrive, échoue ou perd
      // son emote change la place de la ligne sans changer son identité.
      if (kept < ids_.size() && ids_[kept] == line.id &&
          SameImages(line, rich, image_first_[kept], image_first_[kept + 1] - image_first_[kept])) {
        height = heights_[kept];
      }
    }
    if (height < 0.0F) {
      height = LineHeight(font, line, style, wrap_width);
      ++measured_;
    }
    next_ids_.push_back(line.id);
    next_heights_.push_back(height);
    next_image_first_.push_back(static_cast<std::uint32_t>(next_images_.size()));
    for (const ChatInlineImage& image : line.images) {
      next_images_.push_back(FactsOf(image, rich));
    }
    stacked += height;
  }
  // La borne de fin de la dernière ligne.
  next_image_first_.push_back(static_cast<std::uint32_t>(next_images_.size()));
  std::swap(ids_, next_ids_);
  std::swap(heights_, next_heights_);
  std::swap(image_first_, next_image_first_);
  std::swap(images_, next_images_);
  return WithOverhang(stacked, style);
}

void ChatLogLayout::See(std::uint64_t seq) noexcept { seen_through_ = seq; }

bool ChatLogLayout::Revealed(const ChatSpoilerKey& key) const {
  return std::ranges::find(revealed_, key) != revealed_.end();
}

void ChatLogLayout::Reveal(ChatSpoilerKey key) {
  if (Revealed(key)) {
    return;
  }
  if (revealed_.size() >= kMostRevealedSpoilers) {
    revealed_.pop_front();
  }
  revealed_.push_back(std::move(key));
}

void ChatLogLayout::HoldCopy(std::string whole_line, std::string body_only) {
  held_line_ = std::move(whole_line);
  held_body_ = std::move(body_only);
  holds_copy_ = true;
}

std::string ChatLineCopyText(const ChatLineView& line, bool timestamps, bool body_only) {
  std::string copied;
  if (body_only) {
    AppendRewrapped(line.text, std::min<std::size_t>(line.body_offset, line.text.size()),
                    line.styles, &copied);
    return copied;
  }
  // ⚠ LES MÊMES FORMES QUE LE FIL, pour que la copie se lise comme l'écran.
  if (timestamps) {
    std::array<char, chat_log_detail::kStampSize> stamp{};
    std::snprintf(stamp.data(), stamp.size(), chat_log_detail::kStampFormat, line.hour, line.minute,
                  line.second);
    copied += stamp.data();
  }
  for (const std::string_view named : {line.tag, line.type_tag}) {
    if (named.empty()) {
      continue;
    }
    copied += '[';
    copied += named;
    copied += "] ";
  }
  AppendRewrapped(line.text, 0, line.styles, &copied);
  return copied;
}

ChatLogAction DrawChatLog(std::span<const ChatLineView> lines, const ChatLogStyle& style,
                          float height, const SkinArt& art, ChatLogLayout* layout) {
  ChatLogAction action;
  ChatLinkAsk& asked = action.link;
  const float step = ChatLineStep(style);
  ImGui::BeginChild("##fil", ImVec2(0.0F, height), ImGuiChildFlags_None);

  ImDrawList* into = ImGui::GetWindowDrawList();
  ImFont* font = ImGui::GetFont();
  const float wrap_width = ImGui::GetContentRegionAvail().x;
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  // Les bornes de ce qui se voit, en coordonnées locales. Une rangée de marge de
  // chaque côté : la ligne à cheval sur le bord doit être peinte.
  const float view_top = ImGui::GetScrollY() - step;
  const float view_bottom = view_top + ImGui::GetWindowSize().y + 2.0F * step;
  // ⚠ LE COLLAGE AU BAS SE DÉCIDE SANS VARIABLE D'ÉTAT. `GetScrollMaxY` porte le
  // contenu de l'image PRÉCÉDENTE : « ScrollY >= ScrollMaxY » veut donc dire
  // « j'étais en bas au début de cette image », ce qui est exactement la
  // question. Un booléen mis à jour après coup répondrait avec une image de
  // retard — le fil se recollerait en bas à chaque tentative de remonter, et il
  // faudrait insister pour s'en détacher.
  const bool at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();

  ChatLogLayout unkept;
  ChatLogLayout& laid = layout != nullptr ? *layout : unkept;
  const float used = laid.Update(lines, style, wrap_width);
  // ⚠ SEULES LES LIGNES QUI SE VOIENT SE REMETTENT EN PAGE, pour être peintes :
  // la hauteur des autres suffit à les sauter, et tout ce qui suit le bas de la
  // vue est plus bas encore.
  std::vector<LinkHit> hits;
  std::vector<SpoilerHit> spoilers;
  const PlaceFrame frame{.font = font,
                         .style = &style,
                         .origin = origin,
                         .wrap_width = wrap_width,
                         .step = step,
                         .view_top = view_top,
                         .view_bottom = view_bottom,
                         .now_s = ImGui::GetTime(),
                         .memory = &laid};
  const PlaceTarget target{.into = into, .hits = &hits, .spoilers = &spoilers};
  const std::span<const float> heights = laid.heights();
  float top = 0.0F;
  for (std::size_t rank = 0; rank < lines.size() && top <= view_bottom; ++rank) {
    const float bottom = top + heights[rank];
    if (bottom >= view_top) {
      // LE BANDEAU D'UNE MENTION, sous le texte : il se peint d'abord.
      if (lines[rank].mention) {
        into->AddRectFilled(ImVec2(origin.x, origin.y + top),
                            ImVec2(origin.x + wrap_width, origin.y + bottom),
                            kChatMentionBandColor);
      }
      Pen pen;
      pen.y = top;
      pen.row = step;
      LayOutLine(frame, target, lines[rank], &pen);
    }
    top = bottom;
  }
  ImGui::Dummy(ImVec2(wrap_width, used));  // réserve la hauteur pour le défilement

  // ── CE QUI EST ARRIVÉ PENDANT QU'ON LISAIT PLUS HAUT ──────────────────────
  //
  // Collé en bas, le fil voit tout ce qu'il montre. Décollé, ce qui arrive se
  // compte, et une pastille le dit ; son clic recolle.
  //
  // ⚠ SANS CACHE, PAS DE MÉMOIRE : un fil remis en page à chaque image ne sait
  // pas ce qu'il a déjà montré, et compterait tout.
  bool pill_hovered = false;
  bool restick = false;
  if (layout != nullptr) {
    if (at_bottom) {
      layout->See(LastSeq(lines));
    } else if (const int unread = CountUnread(lines, layout->seen_through()); unread > 0) {
      pill_hovered = PaintUnreadPill(into, unread);
      if (pill_hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
          restick = true;
          layout->See(LastSeq(lines));
        }
      }
    }
  }

  // ── CE QUI SE CLIQUE DANS LE FIL ──────────────────────────────────────────
  //
  // ⚠ À LA MAIN, ET APRÈS LE DESSIN : le texte du fil n'est pas fait d'éléments
  // ImGui — c'est une liste de dessin — et il n'y a donc rien à survoler au sens
  // d'ImGui. Le rectangle EST le bouton.
  //
  // ⚠ SEULEMENT SI LA FENÊTRE EST SURVOLÉE : sans cette garde, un lien répondrait
  // sous une fenêtre posée par-dessus lui.
  ImGuiStorage* kept = ImGui::GetStateStorage();
  const ImGuiID target_key = ImGui::GetID(kLinkMenuTargetKey);
  bool on_spoiler = false;
  bool on_link = false;
  if (ImGui::IsWindowHovered() && !pill_hovered) {
    // 🔴 UN SPOILER CACHÉ SE RÉVÈLE AVANT TOUT : son clic ne déclenche rien de ce
    // qu'il cache — on ne clique pas sur ce qu'on n'a pas lu.
    for (const SpoilerHit& spoiler : spoilers) {
      if (!ImGui::IsMouseHoveringRect(spoiler.top_left, spoiler.bottom_right, false)) {
        continue;
      }
      on_spoiler = true;
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      SkinTooltipFormat("%s", core::Tr("Cliquer pour révéler"));
      if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        laid.Reveal(spoiler.line_key);
      }
      break;
    }
    for (const LinkHit& hit : hits) {
      if (on_spoiler) {
        break;
      }
      if (!ImGui::IsMouseHoveringRect(hit.top_left, hit.bottom_right, false)) {
        continue;
      }
      on_link = true;
      asked.hovered_id = hit.item_id;
      asked.hovered_kind = hit.kind;
      asked.hovered_token = hit.token;
      asked.hovering = true;
      if (hit.kind == ChatLinkKind::kUrl) {
        DrawUrlTooltip(hit);
      }
      ChatLinkGesture gesture = HitChatLink(true);
      // Le clic gauche sur une vignette l'agrandit, au lieu d'ouvrir l'adresse.
      if (hit.picture && gesture == ChatLinkGesture::kDescribe) {
        gesture = ChatLinkGesture::kViewImage;
      }
      if (gesture == ChatLinkGesture::kMenu) {
        kept->SetInt(target_key, static_cast<int>(hit.token));
        ImGui::OpenPopup(kLinkMenuId);
      } else if (gesture != ChatLinkGesture::kNothing) {
        asked.gesture = gesture;
        asked.kind = hit.kind;
        asked.item_id = hit.item_id;
        asked.token = hit.token;
      }
      break;
    }
    // LE CLIC DROIT HORS D'UN LIEN : le menu de la ligne entière, et ce qu'il
    // copiera, composé tout de suite — le menu s'ouvre à l'image suivante.
    if (!on_link && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
      const std::size_t rank = LineAt(heights, ImGui::GetIO().MousePos.y - origin.y);
      if (rank < lines.size()) {
        laid.HoldCopy(ChatLineCopyText(lines[rank], style.timestamps, false),
                      ChatLineCopyText(lines[rank], style.timestamps, true));
        ImGui::OpenPopup(kLineMenuId);
      }
    }
  }
  // ⚠ LE NOM SE RETROUVE DANS LES MORCEAUX DE CETTE IMAGE-CI, il ne se garde pas :
  // il pointe dans une ligne du monde, qui peut être partie. Absent, le menu perd
  // sa tête et garde ses entrées — ce qui est exactement le bon ordre de pertes.
  // ⚠ LA CIBLE SE RETIENT PAR SON JETON, qui la désigne SANS AMBIGUÏTÉ : deux
  // liens du même objet à des raffinements différents partagent l'identifiant.
  const auto wanted = static_cast<std::uint32_t>(kept->GetInt(target_key, 0));
  std::string_view label;
  ChatLinkKind target_kind = ChatLinkKind::kItem;
  std::uint32_t target_item = 0;
  const PlayerLinkRefusals* target_refusals = nullptr;
  const MvpLinkMenuFacts* target_mvp = nullptr;
  NaviSearchMenuFacts target_navi;
  const ChatUrlFacts* target_url = nullptr;
  // ⚠ LA TÊTE NOMME LE LIEN, PAS SON ÉTAT : la marque « · passé » se peint dans
  // le fil, mais le menu la retire.
  std::string unmarked;
  for (const LinkHit& hit : hits) {
    if (hit.token == wanted) {
      label = hit.label;
      target_kind = hit.kind;
      target_item = hit.item_id;
      target_refusals = hit.refusals;
      target_mvp = hit.mvp;
      target_navi = hit.navi;
      target_url = hit.url;
      if (hit.mark_length != 0 && hit.mark_at <= label.size() &&
          hit.mark_length <= label.size() - hit.mark_at) {
        unmarked = std::string(label.substr(0, hit.mark_at));
        unmarked += label.substr(hit.mark_at + hit.mark_length);
        label = unmarked;
      }
      break;
    }
  }
  if (const ChatLinkAsk chosen =
          DrawChatLinkMenu(kLinkMenuId, target_kind, target_item, wanted, label, style.vote_shop,
                           target_refusals, target_mvp, target_navi, target_url, style.staff_warp);
      chosen.gesture != ChatLinkGesture::kNothing) {
    asked.gesture = chosen.gesture;
    asked.kind = chosen.kind;
    asked.item_id = chosen.item_id;
    asked.token = chosen.token;
  }
  DrawLineMenu(laid);

  // ── CTRL + MOLETTE : LA TAILLE DU TEXTE ───────────────────────────────────
  //
  // ImGui ne fait pas défiler sous Ctrl : la molette est à nous sans la lui
  // disputer.
  //
  // ⚠ ELLE SE CUMULE, et un cran part au passage de ±1 : la molette fine d'un
  // pavé tactile envoie des dixièmes, et chacun compté pour un cran ferait
  // bondir la taille. Le reste attend le geste suivant, dans le magasin du fil.
  const ImGuiIO& io = ImGui::GetIO();
  const ImGuiID zoom_shown_key = ImGui::GetID(kZoomShownKey);
  const ImGuiID zoom_wheel_key = ImGui::GetID(kZoomWheelKey);
  if (ImGui::IsWindowHovered() && io.KeyCtrl && io.MouseWheel != 0.0F) {
    float wheel = kept->GetFloat(zoom_wheel_key, 0.0F) + io.MouseWheel;
    const float notches = std::trunc(wheel + (wheel > 0.0F ? kWheelNotchSlack : -kWheelNotchSlack));
    if (notches != 0.0F) {
      action.zoom_steps = static_cast<int>(notches);
      wheel -= notches;
      kept->SetFloat(zoom_shown_key, static_cast<float>(ImGui::GetTime()));
    }
    kept->SetFloat(zoom_wheel_key, wheel);
  }
  // La taille se montre un instant après chaque cran, puis s'efface.
  if (const float shown_at = kept->GetFloat(zoom_shown_key, -FLT_MAX);
      shown_at > -FLT_MAX &&
      ImGui::GetTime() - static_cast<double>(shown_at) < kChatZoomShownSeconds) {
    SkinTooltipFormat("%d %%", style.font_percent);
  }

  if (at_bottom || restick) {
    ImGui::SetScrollHereY(1.0F);
  } else if (step > 1.0F) {
    // Ailleurs qu'en bas : le défilement s'aligne sur un multiple du pas, comme
    // le natif. Une rangée de texte ne s'y coupe jamais. ⚠ UNE VIGNETTE RÉSERVE
    // PLUSIEURS PAS, et le calage peut tomber au milieu d'elle : elle est alors
    // coupée en haut ou en bas, jamais le texte qui l'entoure.
    const float at = ImGui::GetScrollY();
    const float snapped = std::floor(at / step + 0.5F) * step;
    if (std::fabs(snapped - at) > 0.5F) {
      ImGui::SetScrollY(snapped);
    }
  }

  PaintWindowScrollbar(art);
  ImGui::EndChild();
  return action;
}

}  // namespace ro::ui
