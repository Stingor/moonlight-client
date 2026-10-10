#include "ro/vfs/item_link.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>

namespace ro::vfs {
namespace {

// ── base62, poids fort en tête ───────────────────────────────────────────────
//
// ⚠ LOCAL À CE FICHIER, et volontairement : c'est le seul endroit du client qui
// en lit ou en écrit. Le promouvoir avant qu'un second appelant existe donnerait
// une bibliothèque à entretenir pour un usage.
constexpr std::string_view kDigits =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr std::uint32_t kBase = 62;

// Le plus de chiffres qu'on lise ou écrive pour un champ. Un entier de
// trente-deux bits en demande six ; la borne n'est là que pour qu'une valeur
// abîmée ne fasse pas boucler. Ce qui dépasse 32 bits est refusé à la lecture :
// voir `ReadBase62`.
constexpr std::size_t kMostDigits = 12;

// LE CADRAGE MINIMAL : deux chiffres pour tout, cinq pour l'emplacement.
//
// ⚠ C'EST `string_left_pad(base62_encode(v), '0', 2)` DU SERVEUR, et c'est aussi
// ce que la routine du client produit. Les zéros de tête ne sont donc pas un
// ornement : sans eux, l'autre bout lirait un autre nombre.
constexpr std::size_t kLeastDigits = 2;
constexpr std::size_t kLocationDigits = 5;

[[nodiscard]] int DigitOf(char c) {
  const std::size_t rank = kDigits.find(c);
  return rank == std::string_view::npos ? -1 : static_cast<int>(rank);
}

void AppendBase62(std::string& into, std::uint32_t value, std::size_t least_digits) {
  std::array<char, kMostDigits> digits{};
  std::size_t count = 0;
  do {
    digits[count++] = kDigits[value % kBase];
    value /= kBase;
  } while (value != 0 && count < kMostDigits);
  while (count < least_digits && count < kMostDigits) {
    digits[count++] = kDigits[0];
  }
  while (count > 0) {
    into.push_back(digits[--count]);
  }
}

// Lit les chiffres base62 qui commencent à `at`, et avance. Rend zéro s'il n'y en
// a aucun — ce qui est le cas d'un séparateur sans charge.
//
// 🔴 UN NOMBRE AU-DELÀ DE 32 BITS VAUT ZÉRO, comme un champ vide : replié, il
// désignait un AUTRE objet, et une balise de chat forgée en montrait un que
// personne n'avait lié. Les chiffres sont consommés quand même, pour que la
// suite de la balise se lise à sa place.
[[nodiscard]] std::uint32_t ReadBase62(std::string_view text, std::size_t* at) {
  std::uint64_t value = 0;
  bool overflowed = false;
  std::size_t read = 0;
  while (*at < text.size() && read < kMostDigits) {
    const int digit = DigitOf(text[*at]);
    if (digit < 0) {
      break;
    }
    if (!overflowed) {
      value = value * kBase + static_cast<std::uint64_t>(digit);
      overflowed = value > std::numeric_limits<std::uint32_t>::max();
    }
    ++*at;
    ++read;
  }
  return overflowed ? 0 : static_cast<std::uint32_t>(value);
}

void AppendField(std::string& into, char separator, std::uint32_t value) {
  into.push_back(separator);
  AppendBase62(into, value, kLeastDigits);
}

// LES DEUX TYPES QUI REPRODUISENT LA DÉCISION DU CLIENT.
//
// 🔴 LA BALISE NE PORTE PAS LE TYPE, elle porte le PRÉDICAT — un seul caractère,
// qui vaut `ItemTypeIsDecorated`. Pour composer le nom il faut pourtant un type :
// on en pose un DEDANS ou DEHORS, et la composition prend exactement la même
// décision que le client, sans avoir à deviner le vrai.
constexpr std::int32_t kDecoratedType = 4;  // une arme
constexpr std::int32_t kPlainType = 3;      // un consommable

}  // namespace

std::size_t ItemLinkCardSlots(const ItemLink& link, std::uint8_t card_slots) {
  std::size_t slots = card_slots;
  if (slots == 0) {
    slots = kItemNameCardSlots;
  }
  const bool any = std::any_of(link.cards.begin(), link.cards.end(),
                               [](std::uint32_t card) { return card != 0; });
  if (any) {
    slots = kItemNameCardSlots;
  }
  return std::min(slots, kItemNameCardSlots);
}

std::string BuildItemLink(const ItemLink& link, std::uint8_t card_slots) {
  if (link.item_id == 0) {
    return {};
  }
  std::string text(kItemLinkOpen);
  AppendBase62(text, link.wearable_at, kLocationDigits);
  text.push_back(link.equipable ? '1' : '0');
  AppendBase62(text, link.item_id, kLeastDigits);
  // ⚠ LE RAFFINEMENT SEULEMENT QUAND IL Y EN A UN : le client saute le champ à
  // zéro, et le serveur aussi.
  if (link.refine_level != 0) {
    AppendField(text, kItemLinkRefine, link.refine_level);
  }
  // ⚠ L'APPARENCE MÊME SUR CE QUI NE SE PORTE PAS. Le serveur, lui, ne l'écrit
  // que pour un objet équipable ; c'est le CLIENT qui tranche, parce que c'est
  // son texte que les autres joueurs reçoivent.
  AppendField(text, kItemLinkView, link.view_id);
  AppendField(text, kItemLinkGrade, link.grade);

  const std::size_t slots = ItemLinkCardSlots(link, card_slots);
  for (std::size_t slot = 0; slot < slots; ++slot) {
    AppendField(text, kItemLinkCard, link.cards[slot]);
  }

  for (std::size_t rank = 0; rank < link.option_count && rank < kItemLinkOptionSlots; ++rank) {
    const ItemLinkOption& option = link.options[rank];
    // ⚠ UN IDENTIFIANT NUL ARRÊTE TOUT, et les deux bouts font pareil : le client
    // n'affiche rien au-delà, donc écrire la suite serait du bruit.
    if (option.id == 0) {
      break;
    }
    AppendField(text, kItemLinkOptionId, option.id);
    AppendField(text, kItemLinkOptionParam, option.param);
    AppendField(text, kItemLinkOptionValue, option.value);
  }

  if (link.damaged) {
    AppendField(text, kItemLinkDamaged, 1);
  }
  text += kItemLinkClose;
  return text;
}

bool ParseItemLink(std::string_view text, ItemLink* out, std::size_t* after) {
  if (after != nullptr) {
    *after = text.size();
  }
  if (out == nullptr || !text.starts_with(kItemLinkOpen)) {
    return false;
  }
  *out = ItemLink{};

  // ⚠ LA FERMANTE SE CHERCHE DANS LES BORNES. Une balise tronquée — une ligne
  // coupée par le serveur — se lit jusqu'au bout du texte et pas au-delà.
  const std::size_t closing = text.find(kItemLinkClose, kItemLinkOpen.size());
  const std::size_t stop = closing == std::string_view::npos ? text.size() : closing;
  if (after != nullptr) {
    *after = closing == std::string_view::npos ? text.size() : closing + kItemLinkClose.size();
  }

  std::size_t at = kItemLinkOpen.size();
  // L'emplacement sur cinq, puis le drapeau « se décore », puis l'identifiant.
  if (stop - at < kLocationDigits + 1) {
    return false;
  }
  const std::string_view head = text.substr(at, kLocationDigits);
  std::size_t in_head = 0;
  out->wearable_at = ReadBase62(head, &in_head);
  if (in_head != kLocationDigits) {
    return false;
  }
  at += kLocationDigits;
  out->equipable = text[at] == '1';
  ++at;

  const std::size_t id_at = at;
  out->item_id = ReadBase62(text.substr(0, stop), &at);
  if (at == id_at) {
    return false;
  }

  std::size_t card = 0;
  while (at < stop) {
    const char separator = text[at];
    ++at;
    const std::uint32_t value = ReadBase62(text.substr(0, stop), &at);
    // ⚠ UNE OPTION ARRIVE EN TROIS MORCEAUX, et c'est la VALEUR qui clôt le
    // triplet : compter là, c'est ne jamais enregistrer une option à moitié lue.
    const std::size_t slot = out->option_count;
    switch (separator) {
      case kItemLinkRefine:
        out->refine_level = value;
        break;
      case kItemLinkView:
        out->view_id = value;
        break;
      case kItemLinkGrade:
        out->grade = value;
        break;
      case kItemLinkCard:
        if (card < kItemNameCardSlots) {
          out->cards[card++] = value;
        }
        break;
      case kItemLinkOptionId:
        if (slot < kItemLinkOptionSlots) {
          out->options[slot].id = static_cast<std::uint16_t>(value);
        }
        break;
      case kItemLinkOptionParam:
        if (slot < kItemLinkOptionSlots) {
          out->options[slot].param = static_cast<std::uint8_t>(value);
        }
        break;
      case kItemLinkOptionValue:
        if (slot < kItemLinkOptionSlots) {
          out->options[slot].value = static_cast<std::uint16_t>(value);
          ++out->option_count;
        }
        break;
      case kItemLinkDamaged:
        out->damaged = value != 0;
        break;
      default:
        // Un séparateur qu'on ne connaît pas : sa charge est déjà sautée.
        break;
    }
  }
  return out->item_id != 0;
}

std::size_t FindItemLink(std::string_view text, std::size_t from) {
  return text.find(kItemLinkOpen, from);
}

ItemNameFacts ItemLinkNameFacts(const ItemLink& link) {
  ItemNameFacts facts;
  facts.item_id = link.item_id;
  facts.type = link.equipable ? kDecoratedType : kPlainType;
  facts.refine_level = static_cast<std::int32_t>(link.refine_level);
  facts.grade = static_cast<std::int32_t>(link.grade);
  facts.cards = link.cards;
  // 🔴 IDENTIFIÉ, TOUJOURS. La balise ne porte pas ce drapeau, et un objet non
  // identifié ne se lie pas : composer sous le nom de base perdrait tout ce que
  // la balise transporte justement.
  facts.identified = true;
  return facts;
}

}  // namespace ro::vfs
