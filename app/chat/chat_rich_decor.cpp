// UNE LIGNE DU FIL, DÉCORÉE JUSQU'AU TEXTE RICHE. Voir l'en-tête.

#include "chat/chat_rich_decor.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/web_preview_policy.h"
#include "ro/net/discord_relay.h"

namespace moonclient {
namespace {

using ro::ui::ChatInlineImage;
using ro::ui::ChatInlineKind;
using ro::ui::ChatLinkKind;
using ro::ui::ChatLinkSpan;

// Ce qui sépare l'auteur de ce qu'il dit, dans une ligne de parole — la forme
// que les mentions lisent aussi.
constexpr std::string_view kSpeakerSeparator = " : ";
// Le crochet qui ferme le pseudo d'une ligne relayée de Discord.
constexpr char kDiscordNameClose = ']';

// Le premier octet qui n'est pas une espace, à partir de `at`.
[[nodiscard]] std::size_t PastBlanks(std::string_view text, std::size_t at) {
  while (at < text.size() && text[at] == ' ') {
    ++at;
  }
  return at;
}

// Ce que la ligne dit derrière le pseudo qui finit à `name_end` : son crochet,
// puis les espaces.
[[nodiscard]] std::uint32_t BodyAfterDiscordName(std::string_view text, std::size_t name_end) {
  std::size_t at = name_end;
  if (at < text.size() && text[at] == kDiscordNameClose) {
    ++at;
  }
  return static_cast<std::uint32_t>(PastBlanks(text, at));
}

// Ce que le pseudo relayé devient : un lien qui porte le nom tel qu'écrit.
[[nodiscard]] ChatLinkTarget DiscordNameTarget(std::string_view name) {
  ChatLinkTarget target;
  target.kind = ChatLinkKind::kDiscordName;
  target.discord_name = std::string(name);
  return target;
}

// LES IMAGES QUE LE CONTEXTE ÉTEINT : les emotes du jeu sans leur case, et sans
// `pictures` tout ce qui demanderait un téléchargement — les emotes de Discord
// comme les vignettes. Leur repli textuel reste dans le texte.
void DropUnwantedImages(const ChatRichContext& context, DecoratedChatLine& line) {
  std::erase_if(line.images, [&context](const ChatInlineImage& image) {
    return (image.kind == ChatInlineKind::kGameEmote && !context.emote_images) ||
           (image.kind == ChatInlineKind::kPicture && !context.pictures);
  });
  if (!context.pictures) {
    line.picture_urls.clear();
  }
}

// L'ICÔNE D'UN OBJET LIÉ, devant son chevron ouvrant : l'icône et le nom se
// lisent ensemble, et le morceau cliquable reste le nom seul.
void PlaceItemIcons(DecoratedChatLine& line) {
  for (const ChatLinkSpan& link : line.links) {
    if (link.kind != ChatLinkKind::kItem || link.item_id == 0) {
      continue;
    }
    ChatInlineImage icon;
    const bool chevron = link.offset > 0 && link.offset <= line.text.size() &&
                         line.text[link.offset - 1] == kChatLinkOpen;
    icon.offset = chevron ? link.offset - 1 : link.offset;
    icon.kind = ChatInlineKind::kItemIcon;
    icon.item_id = link.item_id;
    line.images.push_back(icon);
  }
}

// UNE ADRESSE D'IMAGE DU MIROIR devient sa vignette, qui recouvre l'adresse :
// le clic sur l'image vise toujours le lien qu'elle remplace.
void PlaceMirrorPictures(DecoratedChatLine& line) {
  for (std::size_t rank = 0; rank < line.links.size() && rank < line.carried.size(); ++rank) {
    const ChatLinkSpan& link = line.links[rank];
    if (link.kind != ChatLinkKind::kUrl || !ro::core::IsMirrorPicture(line.carried[rank].url)) {
      continue;
    }
    ChatInlineImage picture;
    picture.offset = link.offset;
    picture.length = link.length;
    picture.kind = ChatInlineKind::kPicture;
    picture.picture_key = static_cast<std::uint32_t>(line.picture_urls.size());
    line.images.push_back(picture);
    line.picture_urls.push_back(line.carried[rank].url);
  }
}

// 🔴 LE FIL LIT LES IMAGES DANS L'ORDRE DU TEXTE, et à une même place celles
// qui s'insèrent avant celle qui remplace : une image dont l'origine est déjà
// dépassée ne se pose pas.
void SortImages(std::vector<ChatInlineImage>& images) {
  std::ranges::stable_sort(images, [](const ChatInlineImage& left, const ChatInlineImage& right) {
    if (left.offset != right.offset) {
      return left.offset < right.offset;
    }
    return left.length == 0 && right.length != 0;
  });
}

// Où commence ce que dit une ligne déjà décorée : derrière le lien de son
// pseudo quand elle vient de Discord.
[[nodiscard]] std::uint32_t BodyOfDecorated(const DecoratedChatLine& line, bool discord_line) {
  if (discord_line) {
    const auto author =
        std::ranges::find(line.links, ChatLinkKind::kDiscordName, &ChatLinkSpan::kind);
    return author == line.links.end()
               ? 0
               : BodyAfterDiscordName(line.text, std::size_t{author->offset} + author->length);
  }
  return ChatBodyOffset(line.text, false);
}

}  // namespace

bool MayNeedRichDecor(std::string_view raw, bool discord_line) {
  return discord_line || MayCarryChatTags(raw) || ro::ui::MayCarryChatMarkup(raw);
}

std::uint32_t ChatBodyOffset(std::string_view text, bool discord_line) {
  if (discord_line) {
    const std::optional<ro::net::DiscordRelayAuthor> author = ro::net::FindDiscordRelayAuthor(text);
    return author ? BodyAfterDiscordName(text, author->offset + author->length) : 0;
  }
  const std::size_t said = text.find(kSpeakerSeparator);
  return said == std::string_view::npos
             ? 0
             : static_cast<std::uint32_t>(said + kSpeakerSeparator.size());
}

bool DecorateRichChatLine(std::string_view raw, bool discord_line, const ChatDecorTables& tables,
                          const ChatRichContext& context, DecoratedChatLine* out) {
  if (out == nullptr || !MayNeedRichDecor(raw, discord_line)) {
    return false;
  }
  // ── LES BALISES, PUIS L'AUTEUR ──────────────────────────────────────────
  // ⚠ LE TEXTE RESTE UNE VUE tant que rien ne l'a changé : une ligne qui n'a
  // finalement rien à décorer ne doit rien avoir recopié.
  DecoratedChatLine base;
  const bool tagged = tables.items != nullptr && tables.words != nullptr && MayCarryChatTags(raw) &&
                      DecorateChatLine(raw, *tables.items, *tables.words, &base, tables.creators,
                                       tables.maps, tables.mvp_clock, tables.settings);
  const std::string_view text = tagged ? std::string_view(base.text) : raw;
  // L'auteur précède tout lien : il ouvre la ligne, et la liste reste triée.
  const std::optional<ro::net::DiscordRelayAuthor> author =
      discord_line ? ro::net::FindDiscordRelayAuthor(text) : std::nullopt;
  if (author) {
    base.links.insert(base.links.begin(),
                      ChatLinkSpan{.offset = static_cast<std::uint32_t>(author->offset),
                                   .length = static_cast<std::uint32_t>(author->length),
                                   .kind = ChatLinkKind::kDiscordName});
    base.carried.insert(base.carried.begin(),
                        DiscordNameTarget(text.substr(author->offset, author->length)));
  }

  // ── LE BALISAGE ─────────────────────────────────────────────────────────
  ro::ui::ChatMarkupResult marked;
  const bool marked_up = context.markup && ApplyChatMarkup(text, base.links, &marked);
  if (!tagged && !author && !marked_up) {
    return false;
  }

  DecoratedChatLine made;
  if (marked_up) {
    made.text = std::move(marked.text);
    made.links = std::move(marked.links);
    made.styles = std::move(marked.runs);
    made.images = std::move(marked.images);
    made.picture_urls = std::move(marked.picture_urls);
    // 🔴 CE QUE PORTE CHAQUE LIEN SE RECOLLE PAR SON RANG D'ORIGINE : le
    // balisage a pu en ajouter — les adresses — et en écarter un mal borné.
    made.carried.reserve(made.links.size());
    for (std::size_t rank = 0; rank < made.links.size(); ++rank) {
      const std::size_t origin =
          rank < marked.link_origin.size() ? marked.link_origin[rank] : ro::ui::kNewChatLink;
      if (origin != ro::ui::kNewChatLink && origin < base.carried.size()) {
        made.carried.push_back(std::move(base.carried[origin]));
        continue;
      }
      const ChatLinkSpan& link = made.links[rank];
      ChatLinkTarget address;
      address.kind = ChatLinkKind::kUrl;
      address.url = made.text.substr(link.offset, link.length);
      made.carried.push_back(std::move(address));
    }
  } else {
    made.text = tagged ? std::move(base.text) : std::string(raw);
    made.links = std::move(base.links);
    made.carried = std::move(base.carried);
  }

  // ── LES IMAGES ──────────────────────────────────────────────────────────
  DropUnwantedImages(context, made);
  if (context.item_icons) {
    PlaceItemIcons(made);
  }
  if (context.pictures) {
    PlaceMirrorPictures(made);
  }
  SortImages(made.images);
  made.body_offset = BodyOfDecorated(made, discord_line);
  *out = std::move(made);
  return true;
}

bool DecorateBubbleLine(std::string_view said, const ChatDecorTables& tables,
                        const ItemIconLookup& item_icon, DecoratedChatLine* out) {
  const ChatRichContext bubble{.markup = true,
                               .item_icons = static_cast<bool>(item_icon),
                               .emote_images = true,
                               .pictures = false};
  if (!DecorateRichChatLine(said, /*discord_line=*/false, tables, bubble, out)) {
    return false;
  }
  // ⚠ LA TEXTURE SE POSE ICI : le fil la pose en publiant ses lignes, et une
  // bulle ne passe pas par lui.
  for (ChatInlineImage& image : out->images) {
    if (image.kind == ChatInlineKind::kItemIcon) {
      image.texture = item_icon(image.item_id);
    }
  }
  return true;
}

void AppendRichBody(DecoratedChatLine& head, DecoratedChatLine body) {
  const auto body_at = static_cast<std::uint32_t>(head.text.size());
  const auto first_picture = static_cast<std::uint32_t>(head.picture_urls.size());
  head.text.append(body.text);
  for (std::size_t rank = 0; rank < body.links.size() && rank < body.carried.size(); ++rank) {
    ChatLinkSpan moved = body.links[rank];
    moved.offset += body_at;
    head.links.push_back(moved);
    head.carried.push_back(std::move(body.carried[rank]));
  }
  for (ro::ui::ChatStyleRun run : body.styles) {
    run.offset += body_at;
    head.styles.push_back(run);
  }
  for (ChatInlineImage image : body.images) {
    image.offset += body_at;
    if (image.kind == ChatInlineKind::kPicture) {
      image.picture_key += first_picture;
    }
    head.images.push_back(image);
  }
  for (std::string& url : body.picture_urls) {
    head.picture_urls.push_back(std::move(url));
  }
}

bool DecorateAuthoredLine(std::string_view raw, const ro::sim::ChatAuthor& author,
                          std::string_view own_name, const ChatDecorTables& tables,
                          const ChatRichContext& context, DecoratedChatLine* out) {
  const std::size_t name_end = std::size_t{author.name_offset} + author.name_length;
  if (out == nullptr || author.name_length == 0 || name_end > author.said_offset ||
      author.said_offset > raw.size() ||
      raw.substr(author.name_offset, author.name_length) != author.name) {
    return false;
  }
  DecoratedChatLine composed;
  composed.text = std::string(raw.substr(0, author.said_offset));
  // Son propre nom ne se clique pas : on ne se chuchote pas à soi-même.
  if (author.name != own_name) {
    ChatLinkTarget target;
    target.kind = ChatLinkKind::kPlayer;
    target.player = author.name;
    composed.links.push_back(ChatLinkSpan{
        .offset = author.name_offset, .length = author.name_length, .kind = ChatLinkKind::kPlayer});
    composed.carried.push_back(std::move(target));
  }
  const std::string_view said = raw.substr(author.said_offset);
  DecoratedChatLine body;
  if (DecorateRichChatLine(said, /*discord_line=*/false, tables, context, &body)) {
    AppendRichBody(composed, std::move(body));
  } else {
    composed.text.append(said);
  }
  composed.body_offset = author.said_offset;
  *out = std::move(composed);
  return true;
}

void PlaceLineAvatar(DecoratedChatLine& line, std::string url) {
  ChatInlineImage avatar;
  avatar.kind = ChatInlineKind::kPicture;
  avatar.line_high = true;
  avatar.picture_key = static_cast<std::uint32_t>(line.picture_urls.size());
  line.images.insert(line.images.begin(), avatar);
  line.picture_urls.push_back(std::move(url));
}

void NumberPictureKeys(DecoratedChatLine& line, std::uint32_t first_key) {
  for (ChatInlineImage& image : line.images) {
    if (image.kind != ChatInlineKind::kPicture) {
      continue;
    }
    if (image.picture_key >= line.picture_urls.size()) {
      image.state = ro::ui::ChatPictureState::kFailed;
      continue;
    }
    image.picture_key += first_key;
  }
}

}  // namespace moonclient
