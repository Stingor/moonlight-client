// LE BALISAGE D'UNE LIGNE DU FIL. Voir l'en-tête.

#include "ro/ui/chat_markup.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <system_error>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/core/web_address.h"
#include "ro/ui/chat_emote_names.h"

namespace ro::ui {
namespace {

// « ^RRGGBB » : la marque et six chiffres hexadécimaux. « ^000000 » rend la
// couleur de la ligne.
constexpr char kColorMark = '^';
constexpr std::size_t kColorDigits = 6;
constexpr std::size_t kColorTokenLength = 1 + kColorDigits;
constexpr int kHexBase = 16;
constexpr unsigned kRedShift = 16;
constexpr unsigned kGreenShift = 8;
constexpr std::uint32_t kChannelMask = 0xFF;
constexpr std::uint8_t kOpaque = 255;

// « ^i[id] » : l'icône d'un objet, que le jeu dessine dans toutes ses fenêtres
// de texte — un joueur peut donc en taper une. Le « i » se lit sans casse.
constexpr std::string_view kItemIconOpen = "^i[";
constexpr char kItemIconClose = ']';

// Les bascules de style : une marque simple ou doublée.
constexpr char kStarMark = '*';
constexpr char kUnderlineMark = '_';
constexpr char kStrikeMark = '~';
constexpr char kSpoilerMark = '|';
constexpr std::size_t kSingleTokenLength = 1;
constexpr std::size_t kPairTokenLength = 2;
constexpr char kCodeMark = '`';

// « <:nom:id> » et « <a:nom:id> », la forme brute d'une emote Discord. Le « a »
// se lit sans casse.
constexpr std::string_view kDiscordStillOpen = "<:";
constexpr std::string_view kDiscordAnimatedOpen = "<a:";
constexpr char kDiscordEmoteClose = '>';
constexpr std::string_view kDiscordEmojiAddress = "https://cdn.discordapp.com/emojis/";
constexpr std::string_view kStillEmojiExtension = ".png";
constexpr std::string_view kAnimatedEmojiExtension = ".gif";

// « :nom: » : un nom de la table, en minuscules, chiffres et tirets bas.
constexpr char kEmoteMark = ':';
// Les deux « : » qui encadrent un nom d'emote.
constexpr std::size_t kEmoteColons = 2;
constexpr std::size_t kLongestEmoteName = 24;

[[nodiscard]] bool IsHexDigit(char letter) {
  return (letter >= '0' && letter <= '9') || (letter >= 'a' && letter <= 'f') ||
         (letter >= 'A' && letter <= 'F');
}

[[nodiscard]] bool IsDigit(char letter) { return letter >= '0' && letter <= '9'; }

[[nodiscard]] bool IsEmoteNameByte(char letter) {
  return (letter >= 'a' && letter <= 'z') || IsDigit(letter) || letter == '_';
}

// Ce que Discord admet dans un nom d'emote. Les comparaisons par plages ne
// dépendent pas du signe de `char` : un octet ≥ 0x80 n'entre dans aucune.
[[nodiscard]] bool IsDiscordEmoteNameByte(char letter) {
  return (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z') || IsDigit(letter) ||
         letter == '_';
}

[[nodiscard]] bool IsBlank(char letter) {
  return letter == ' ' || letter == '\t' || letter == '\n' || letter == '\r';
}

[[nodiscard]] bool IsAsciiPunctuation(char letter) {
  return (letter >= '!' && letter <= '/') || (letter >= ':' && letter <= '@') ||
         (letter >= '[' && letter <= '`') || (letter >= '{' && letter <= '~');
}

// CE QUI BORDE UNE MARQUE DOUBLÉE : une espace ou une ponctuation, sauf la
// marque elle-même — « ___ » ne borde rien.
[[nodiscard]] bool BordersPair(char letter, char mark) {
  return letter != mark && (IsBlank(letter) || IsAsciiPunctuation(letter));
}

// La couleur de « ^RRGGBB » à `at`, ou rien. Zéro pour « ^000000 ».
[[nodiscard]] std::optional<ImU32> ColorTokenAt(std::string_view text, std::size_t at) {
  if (text[at] != kColorMark || text.size() - at < kColorTokenLength) {
    return std::nullopt;
  }
  const std::string_view digits = text.substr(at + 1, kColorDigits);
  if (!std::ranges::all_of(digits, IsHexDigit)) {
    return std::nullopt;
  }
  std::uint32_t rgb = 0;
  std::from_chars(digits.data(), digits.data() + digits.size(), rgb, kHexBase);
  if (rgb == 0) {
    return ImU32{0};
  }
  return IM_COL32((rgb >> kRedShift) & kChannelMask, (rgb >> kGreenShift) & kChannelMask,
                  rgb & kChannelMask, kOpaque);
}

// « ^i[chiffres] » à `at` : la position qui suit, et l'identifiant (zéro s'il
// est nul ou déborde). Rien quand les crochets portent autre chose que des
// chiffres : le texte reste alors tel quel.
[[nodiscard]] std::optional<std::size_t> ItemIconAt(std::string_view text, std::size_t at,
                                                    std::uint32_t* item_id) {
  if (!core::AsciiStartsWithFolded(text.substr(at), kItemIconOpen)) {
    return std::nullopt;
  }
  const std::size_t digits_at = at + kItemIconOpen.size();
  const std::size_t close = text.find(kItemIconClose, digits_at);
  if (close == std::string_view::npos || close == digits_at) {
    return std::nullopt;
  }
  const std::string_view digits = text.substr(digits_at, close - digits_at);
  if (!std::ranges::all_of(digits, IsDigit)) {
    return std::nullopt;
  }
  *item_id = 0;
  if (std::from_chars(digits.data(), digits.data() + digits.size(), *item_id).ec != std::errc{}) {
    *item_id = 0;
  }
  return close + 1;
}

// Le nom de la table à `at` (« :nom: » entier), ou rien.
[[nodiscard]] std::optional<std::uint16_t> GameEmoteAt(std::string_view text, std::size_t at,
                                                       std::size_t* after) {
  if (text[at] != kEmoteMark) {
    return std::nullopt;
  }
  std::size_t stop = at + 1;
  while (stop < text.size() && stop - at <= kLongestEmoteName && IsEmoteNameByte(text[stop])) {
    ++stop;
  }
  if (stop >= text.size() || text[stop] != kEmoteMark || stop == at + 1) {
    return std::nullopt;
  }
  const std::optional<std::uint16_t> action = ChatEmoteAction(text.substr(at + 1, stop - at - 1));
  if (action) {
    *after = stop + 1;
  }
  return action;
}

// L'ouvrante d'une emote Discord à `at` : sa longueur, ou zéro.
[[nodiscard]] std::size_t DiscordEmoteOpenAt(std::string_view text, std::size_t at) {
  const std::string_view rest = text.substr(at);
  if (rest.starts_with(kDiscordStillOpen)) {
    return kDiscordStillOpen.size();
  }
  if (core::AsciiStartsWithFolded(rest, kDiscordAnimatedOpen)) {
    return kDiscordAnimatedOpen.size();
  }
  return 0;
}

// Une adresse peut commencer ici : les seules initiales de ses amorces.
[[nodiscard]] bool MayStartAddress(char letter) {
  return letter == 'h' || letter == 'H' || letter == 'w' || letter == 'W';
}

// Une marque de bascule à `at` : sa longueur, et le style qu'elle bascule.
struct Toggle {
  std::size_t length = 0;
  std::uint8_t style = 0;
  // Une marque qui n'existe que doublée : `__`, `~~`, `||`.
  bool pair_only = false;
};

// Les marques qui n'existent que doublées, et le style de chacune.
struct PairMark {
  char mark = 0;
  std::uint8_t style = 0;
};
constexpr std::array<PairMark, 3> kPairMarks = {{{.mark = kUnderlineMark, .style = kChatUnderline},
                                                 {.mark = kStrikeMark, .style = kChatStrike},
                                                 {.mark = kSpoilerMark, .style = kChatSpoiler}}};

[[nodiscard]] std::optional<Toggle> ToggleAt(std::string_view text, std::size_t at) {
  const char mark = text[at];
  const bool doubled = at + 1 < text.size() && text[at + 1] == mark;
  if (mark == kStarMark) {
    return doubled ? Toggle{.length = kPairTokenLength, .style = kChatBold}
                   : Toggle{.length = kSingleTokenLength, .style = kChatItalic};
  }
  if (!doubled) {
    return std::nullopt;
  }
  for (const PairMark& pair : kPairMarks) {
    if (pair.mark == mark) {
      return Toggle{.length = kPairTokenLength, .style = pair.style, .pair_only = true};
    }
  }
  return std::nullopt;
}

// Ce qui s'écrit tant qu'aucune bascule ne change.
struct Pen {
  std::uint8_t style = 0;
  ImU32 color = 0;
  std::uint16_t spoiler = 0;
};

// UNE LIGNE BALISÉE, d'un bout à l'autre.
//
// 🔴 LES LIENS REÇUS SONT DES BORNES : aucune règle ne mord sur l'un d'eux. Le
// texte jusqu'au prochain lien est la seule fenêtre que les règles voient, et
// le lien lui-même se recopie d'un bloc.
class Marker {
 public:
  Marker(std::string_view text, std::span<const ChatLinkSpan> links, ChatMarkupResult* made)
      : text_(text), links_(links), made_(*made) {
    KeepUsableLinks();
  }

  void Mark() {
    std::size_t at = 0;
    while (at < text_.size()) {
      const std::size_t limit = NextLinkStart();
      if (at >= limit) {
        at = CopyLink();
        continue;
      }
      at = MarkAt(text_.substr(0, limit), at);
    }
  }

  // Le balisage a-t-il changé quelque chose ?
  [[nodiscard]] bool Changed() const {
    if (!made_.runs.empty() || !made_.images.empty() || made_.text != text_) {
      return true;
    }
    return std::ranges::any_of(made_.link_origin,
                               [](std::size_t origin) { return origin == kNewChatLink; });
  }

 private:
  // Les liens d'entrée par ordre de début, sans ceux qui débordent du texte ou
  // chevauchent leur prédécesseur : une borne fausse ne doit pas déplacer le
  // texte.
  void KeepUsableLinks() {
    order_.reserve(links_.size());
    for (std::size_t rank = 0; rank < links_.size(); ++rank) {
      const ChatLinkSpan& link = links_[rank];
      if (link.length != 0 && link.offset <= text_.size() &&
          link.length <= text_.size() - link.offset) {
        order_.push_back(rank);
      }
    }
    std::ranges::stable_sort(order_, [this](std::size_t left, std::size_t right) {
      return links_[left].offset < links_[right].offset;
    });
    std::size_t covered = 0;
    std::erase_if(order_, [this, &covered](std::size_t rank) {
      const ChatLinkSpan& link = links_[rank];
      if (link.offset < covered) {
        return true;
      }
      covered = link.offset + link.length;
      return false;
    });
  }

  [[nodiscard]] std::size_t NextLinkStart() const {
    return next_link_ < order_.size() ? links_[order_[next_link_]].offset : text_.size();
  }

  // Le lien suivant, recopié tel quel à sa nouvelle place.
  std::size_t CopyLink() {
    const std::size_t rank = order_[next_link_++];
    ChatLinkSpan moved = links_[rank];
    moved.offset = static_cast<std::uint32_t>(made_.text.size());
    made_.links.push_back(moved);
    made_.link_origin.push_back(rank);
    const ChatLinkSpan& link = links_[rank];
    Write(text_.substr(link.offset, link.length), pen_.style);
    return link.offset + link.length;
  }

  // Une règle à `at`, ou un octet ordinaire. Rend la position suivante.
  std::size_t MarkAt(std::string_view window, std::size_t at) {
    if (const std::optional<ImU32> color = ColorTokenAt(window, at)) {
      pen_.color = *color;
      return at + kColorTokenLength;
    }
    if (const std::optional<Toggle> toggle = ToggleAt(window, at); toggle && Toggles(at, *toggle)) {
      Switch(toggle->style);
      if (toggle->pair_only && (pen_.style & toggle->style) != 0) {
        pair_opened_before_ = at + toggle->length;
      }
      return at + toggle->length;
    }
    if (window[at] == kCodeMark) {
      const std::size_t close = window.find(kCodeMark, at + 1);
      // ⚠ UNE PAIRE VIDE N'EST PAS DU CODE : « `` » reste deux accents graves.
      if (close != std::string_view::npos && close > at + 1) {
        Write(window.substr(at + 1, close - at - 1),
              static_cast<std::uint8_t>(pen_.style | kChatCode));
        return close + 1;
      }
    }
    std::uint32_t item_id = 0;
    if (const std::optional<std::size_t> after = ItemIconAt(window, at, &item_id)) {
      PlaceItemIcon(item_id);
      return *after;
    }
    if (const std::size_t after = DiscordEmoteAt(window, at); after != 0) {
      return after;
    }
    if (!emote_placed_) {
      std::size_t after = 0;
      if (const std::optional<std::uint16_t> action = GameEmoteAt(window, at, &after)) {
        PlaceGameEmote(window.substr(at, after - at), *action);
        return after;
      }
    }
    if (const std::size_t stop = AddressEndAt(window, at); stop != 0) {
      ChatLinkSpan address;
      address.offset = static_cast<std::uint32_t>(made_.text.size());
      address.length = static_cast<std::uint32_t>(stop - at);
      address.kind = ChatLinkKind::kUrl;
      made_.links.push_back(address);
      made_.link_origin.push_back(kNewChatLink);
      Write(window.substr(at, stop - at), pen_.style);
      return stop;
    }
    Write(window.substr(at, 1), pen_.style);
    return at + 1;
  }

  // La fin d'une adresse qui commence à `at`, ou zéro.
  //
  // ⚠ L'OCTET PRÉCÉDENT SE LIT BRUT, sauf derrière une ouvrante doublée : un
  // `||lien||` est l'usage même du spoiler sur Discord, et l'adresse y commence
  // comme en début de texte. Derrière `*` ou `**`, elle colle à la marque et
  // n'est pas reconnue.
  //
  // 🔴 ET ELLE S'ARRÊTE À LA FERMANTE d'une paire ouverte : « | », « _ » et « ~ »
  // sont des octets d'adresse, et sans cette borne `||lien||` avalerait sa
  // fermante.
  [[nodiscard]] std::size_t AddressEndAt(std::string_view window, std::size_t at) const {
    if (!MayStartAddress(window[at])) {
      return 0;
    }
    const bool after_pair = at == pair_opened_before_;
    const bool starts = after_pair ? core::IsWebAddressStart(window.substr(at), 0)
                                   : core::IsWebAddressStart(window, at);
    if (!starts) {
      return 0;
    }
    const std::string_view bounded = window.substr(0, OpenPairCloserAfter(window, at));
    const std::size_t stop = core::WebAddressEnd(bounded, at);
    return (stop > at && stop <= bounded.size()) ? stop : 0;
  }

  // La première fermante doublée d'un style ouvert après `at`, ou la fin.
  [[nodiscard]] std::size_t OpenPairCloserAfter(std::string_view window, std::size_t at) const {
    std::size_t first = window.size();
    for (const PairMark& pair : kPairMarks) {
      if ((pen_.style & pair.style) == 0) {
        continue;
      }
      const std::array<char, kPairTokenLength> token = {pair.mark, pair.mark};
      const std::size_t hit = window.find(std::string_view(token.data(), token.size()), at);
      first = std::min(first, hit);
    }
    return first;
  }

  // 🔴 UNE BASCULE NE S'OUVRE QUE SI SA FERMETURE EXISTE. Sans ce contrôle, un
  // « 3*4 » ou un « *soupir » passerait toute la fin de la ligne en italique, et
  // le chat est plein d'astérisques isolées. L'ouvrante colle à son texte
  // (« *mot », jamais « * mot ») ; la fermante aussi, par l'autre bout.
  //
  // ⚠ UNE MARQUE DOUBLÉE (`__`, `~~`, `||`) EST EN PLUS BORDÉE de l'extérieur :
  // une espace ou une ponctuation devant l'ouvrante, la fin, une espace ou une
  // ponctuation derrière la fermante. Les tirets bas pullulent dans les
  // identifiants (`my__var__name`) et les pseudos ; un pseudo en tête de ligne
  // (« __Ana__ : salut ») ne se souligne pas, d'où l'ouvrante refusée en tout
  // début de texte.
  //
  // ⚠ LE TEST DE COLLAGE LIT LES OCTETS BRUTS : la fermante de « **gras** » colle
  // à « s ».
  [[nodiscard]] bool Toggles(std::size_t at, const Toggle& toggle) const {
    const char mark = text_[at];
    if ((pen_.style & toggle.style) != 0) {
      if (at == 0 || text_[at - 1] == ' ') {
        return false;
      }
      return !toggle.pair_only || EndsPair(at + toggle.length, mark);
    }
    const std::size_t content = at + toggle.length;
    if (content >= text_.size() || text_[content] == ' ' || text_[content] == mark) {
      return false;
    }
    if (toggle.pair_only && (at == 0 || !BordersPair(text_[at - 1], mark))) {
      return false;
    }
    return ClosingFollows(content, mark, toggle);
  }

  // Derrière une fermante doublée : la fin, une espace ou une ponctuation.
  [[nodiscard]] bool EndsPair(std::size_t past, char mark) const {
    return past >= text_.size() || BordersPair(text_[past], mark);
  }

  // Une fermante de même longueur, collée à son texte, plus loin que `content`.
  // Les liens reçus et les paires d'accents graves ne se fouillent pas : un
  // « * » dans un nom d'objet ou dans du code ne ferme rien.
  [[nodiscard]] bool ClosingFollows(std::size_t content, char mark, const Toggle& toggle) const {
    const std::array stops = {mark, kCodeMark};
    const std::string_view wanted(stops.data(), stops.size());
    std::size_t from = content;
    while (from < text_.size()) {
      const std::size_t hit = text_.find_first_of(wanted, from);
      if (hit == std::string_view::npos) {
        return false;
      }
      if (const std::size_t past = PastLinkCovering(hit); past != 0) {
        from = past;
        continue;
      }
      if (text_[hit] == kCodeMark) {
        from = PastCodePair(hit);
        continue;
      }
      const std::size_t hit_length = (hit + 1 < text_.size() && text_[hit + 1] == mark)
                                         ? kPairTokenLength
                                         : kSingleTokenLength;
      if (hit_length == toggle.length && hit > content && text_[hit - 1] != ' ' &&
          (!toggle.pair_only || EndsPair(hit + hit_length, mark))) {
        return true;
      }
      from = hit + hit_length;
    }
    return false;
  }

  // La fin du lien d'entrée qui couvre `at`, ou zéro.
  [[nodiscard]] std::size_t PastLinkCovering(std::size_t at) const {
    for (std::size_t rank = next_link_; rank < order_.size(); ++rank) {
      const ChatLinkSpan& link = links_[order_[rank]];
      if (link.offset > at) {
        break;
      }
      if (at < link.offset + link.length) {
        return link.offset + link.length;
      }
    }
    return 0;
  }

  // Après la paire d'accents graves ouverte à `open`, comme `MarkAt` la lirait :
  // non vide, et sans lien reçu au milieu. Sinon, l'octet suivant.
  [[nodiscard]] std::size_t PastCodePair(std::size_t open) const {
    const std::size_t close = text_.find(kCodeMark, open + 1);
    if (close == std::string_view::npos || close == open + 1) {
      return open + 1;
    }
    for (std::size_t rank = next_link_; rank < order_.size(); ++rank) {
      const std::size_t link_at = links_[order_[rank]].offset;
      if (link_at > open && link_at < close) {
        return open + 1;
      }
    }
    return close + 1;
  }

  void Switch(std::uint8_t style) {
    pen_.style = static_cast<std::uint8_t>(pen_.style ^ style);
    if (style == kChatSpoiler) {
      pen_.spoiler = (pen_.style & kChatSpoiler) != 0 ? ++spoilers_ : std::uint16_t{0};
    }
  }

  // « ^i[id] » : une icône devant ce qui suit, sans texte. Un identifiant nul
  // fait disparaître la marque : rien ne se dessine.
  void PlaceItemIcon(std::uint32_t item_id) {
    if (item_id == 0) {
      return;
    }
    ChatInlineImage icon;
    icon.offset = static_cast<std::uint32_t>(made_.text.size());
    icon.kind = ChatInlineKind::kItemIcon;
    icon.item_id = item_id;
    made_.images.push_back(icon);
  }

  // « <:nom:id> » ou « <a:nom:id> » : le texte « :nom: », et l'image du CDN de
  // Discord par-dessus. Rend la position qui suit, ou zéro.
  //
  // Le repli compte autant que l'image : lisible tout de suite, sans requête,
  // et suffisant quand les images sont éteintes.
  //
  // ⚠ L'IDENTIFIANT DOIT ÊTRE UN NOMBRE, et le nom ne porte que ce que Discord
  // y admet : « <a:b:c> » tapé par un joueur ne doit pas lancer de requête.
  std::size_t DiscordEmoteAt(std::string_view window, std::size_t at) {
    const std::size_t open_length = DiscordEmoteOpenAt(window, at);
    if (open_length == 0) {
      return 0;
    }
    const bool animated = open_length == kDiscordAnimatedOpen.size();
    const std::size_t name_at = at + open_length;
    const std::size_t name_end = window.find(kEmoteMark, name_at);
    if (name_end == std::string_view::npos || name_end == name_at) {
      return 0;
    }
    const std::size_t close = window.find(kDiscordEmoteClose, name_end);
    if (close == std::string_view::npos || close == name_end + 1) {
      return 0;
    }
    const std::string_view name = window.substr(name_at, name_end - name_at);
    const std::string_view emoji_id = window.substr(name_end + 1, close - name_end - 1);
    if (!std::ranges::all_of(name, IsDiscordEmoteNameByte) ||
        !std::ranges::all_of(emoji_id, IsDigit)) {
      return 0;
    }
    ChatInlineImage picture;
    picture.offset = static_cast<std::uint32_t>(made_.text.size());
    picture.length = static_cast<std::uint32_t>(name.size() + kEmoteColons);
    picture.kind = ChatInlineKind::kPicture;
    picture.picture_key = static_cast<std::uint32_t>(made_.picture_urls.size());
    made_.images.push_back(picture);
    std::string address(kDiscordEmojiAddress);
    address.append(emoji_id);
    address.append(animated ? kAnimatedEmojiExtension : kStillEmojiExtension);
    made_.picture_urls.push_back(std::move(address));
    // Le « : » qui précède le nom est celui de l'ouvrante.
    Write(window.substr(name_at - 1, name.size() + kEmoteColons), pen_.style);
    return close + 1;
  }

  // « :nom: » : le repli reste dans le texte, l'image le recouvre.
  //
  // 🔴 UNE SEULE PAR LIGNE. Le relai Discord n'embarque qu'une image par
  // message : une ligne qui en montrerait trois ici en montrerait trois
  // « :nom: » là-bas. Les suivantes restent du texte, des deux côtés du pont.
  void PlaceGameEmote(std::string_view written, std::uint16_t action) {
    ChatInlineImage emote;
    emote.offset = static_cast<std::uint32_t>(made_.text.size());
    emote.length = static_cast<std::uint32_t>(written.size());
    emote.kind = ChatInlineKind::kGameEmote;
    emote.emote_action = action;
    made_.images.push_back(emote);
    Write(written, pen_.style);
    emote_placed_ = true;
  }

  // Le texte rendu, et sa suite de style quand il en porte un. Deux suites
  // voisines de même style n'en font qu'une.
  void Write(std::string_view piece, std::uint8_t style) {
    if (piece.empty()) {
      return;
    }
    const auto offset = static_cast<std::uint32_t>(made_.text.size());
    made_.text.append(piece);
    if (style == 0 && pen_.color == 0) {
      return;
    }
    const auto length = static_cast<std::uint32_t>(piece.size());
    if (!made_.runs.empty()) {
      ChatStyleRun& last = made_.runs.back();
      if (last.offset + last.length == offset && last.style == style && last.color == pen_.color &&
          last.spoiler == pen_.spoiler) {
        last.length += length;
        return;
      }
    }
    made_.runs.push_back({.offset = offset,
                          .length = length,
                          .style = style,
                          .color = pen_.color,
                          .spoiler = pen_.spoiler});
  }

  std::string_view text_;
  std::span<const ChatLinkSpan> links_;
  ChatMarkupResult& made_;
  std::vector<std::size_t> order_;
  std::size_t next_link_ = 0;
  Pen pen_;
  std::uint16_t spoilers_ = 0;
  // Où finit la dernière ouvrante doublée : une adresse peut y commencer.
  std::size_t pair_opened_before_ = std::string_view::npos;
  bool emote_placed_ = false;
};

}  // namespace

// ⚠ RIEN N'ALLOUE ICI : que des vues et des comparaisons d'octets, `from_chars`
// et la table des emotes, elle aussi parcourue sans copie.
bool MayCarryChatMarkup(std::string_view text) {
  for (std::size_t at = 0; at < text.size(); ++at) {
    const char letter = text[at];
    switch (letter) {
      case kColorMark: {
        std::uint32_t item_id = 0;
        if (ColorTokenAt(text, at) || ItemIconAt(text, at, &item_id)) {
          return true;
        }
        break;
      }
      case kStarMark:
      case kCodeMark:
        return true;
      case kUnderlineMark:
      case kStrikeMark:
      case kSpoilerMark:
        if (ToggleAt(text, at)) {
          return true;
        }
        break;
      case '<':
        if (DiscordEmoteOpenAt(text, at) != 0) {
          return true;
        }
        break;
      case kEmoteMark: {
        std::size_t after = 0;
        if (GameEmoteAt(text, at, &after)) {
          return true;
        }
        break;
      }
      default:
        if (MayStartAddress(letter) && core::IsWebAddressStart(text, at)) {
          return true;
        }
        break;
    }
  }
  return false;
}

bool ApplyChatMarkup(std::string_view text, std::span<const ChatLinkSpan> links,
                     ChatMarkupResult* out) {
  if (out == nullptr || !MayCarryChatMarkup(text)) {
    return false;
  }
  ChatMarkupResult made;
  made.text.reserve(text.size());
  Marker marker(text, links, &made);
  marker.Mark();
  if (!marker.Changed()) {
    return false;
  }
  *out = std::move(made);
  return true;
}

}  // namespace ro::ui
