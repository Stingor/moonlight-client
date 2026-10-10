#include "ro/vfs/item_name.h"

#include <array>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>

namespace ro::vfs {
namespace {

// ── Ce que le fil met dans les cartes quand il n'y met pas de cartes ─────────
//
// Le serveur détourne `card[0]` pour marquer deux états, et les trois autres
// cartes cessent alors d'être des cartes : elles portent l'élément, les étoiles
// et l'identifiant du forgeron.
constexpr std::uint32_t kCardMarkerForged = 0x00FF;
constexpr std::uint32_t kCardMarkerCreated = 0x00FE;

// L'élément tient sur l'octet bas de `card[1]`, les étoiles sur l'octet haut.
constexpr std::uint32_t kByteMask = 0xFF;
constexpr int kByteShift = 8;

// Un œuf de familier qu'on a renommé le dit par sa quatrième carte.
constexpr std::uint32_t kRenamedPetMarker = 1;

// Le type que le client exige pour préfixer « Bien-aimé ».
constexpr std::int32_t kWeaponType = 4;

// Les deux plages d'identifiants réservées aux familiers.
constexpr std::uint32_t kPetEggIdFirst = 9001;
constexpr std::uint32_t kPetEggIdLast = 9499;
constexpr std::uint32_t kPetGearIdFirst = 10000;
constexpr std::uint32_t kPetGearIdLast = 10999;

// La lettre d'un grade, du plus faible au plus fort. Le rang 0 n'existe pas :
// le client ne compose rien tant que le grade est nul.
//
// ⚠ AU-DELÀ DE QUATRE, LE CLIENT ÉCRIT DES CROCHETS VIDES, et nous faisons
// pareil (ADR 0030) : son tampon de deux octets reste à zéro et le « %s » rend
// une chaîne vide. Corriger ici donnerait un affichage que personne d'autre ne
// produit.
constexpr std::array<std::string_view, 5> kGradeLetters = {"", "D", "C", "B", "A"};

// L'espace qui accompagne un mot de carte. Le client le porte dans ses formats
// — « %s » devant le nom, « %s » derrière —, et il n'est pas décoratif : sans
// lui les mots se soudent au nom.
constexpr std::string_view kWordBeforeSuffix = " ";
constexpr std::string_view kWordAfterPrefix = " ";

// Ce qu'on montre d'un objet que la table ignore : son numéro. Le client, lui,
// écrit une espace — un nom vide, que rien ne permet de rattraper.
constexpr std::string_view kUnknownItemPrefix = "#";

// Le nom se bâtit morceau par morceau, et deux morceaux ordinaires de suite se
// fondent : ils se dessineront de la même couleur, les séparer ne dirait rien.
class NameBuilder {
 public:
  void Add(std::string_view text) {
    if (text.empty()) {
      return;
    }
    if (!segments_.empty() && segments_.back().role == ItemNameRole::kOrdinary) {
      segments_.back().text.append(text);
      return;
    }
    segments_.push_back(
        ItemNameSegment{.text = std::string(text), .role = ItemNameRole::kOrdinary});
  }

  // Le nom du créateur est le seul morceau que le client colore à part.
  void AddCreator(std::string_view text, ItemNameRole role) {
    if (text.empty()) {
      return;
    }
    segments_.push_back(ItemNameSegment{.text = std::string(text), .role = role});
  }

  [[nodiscard]] std::vector<ItemNameSegment> Take() { return std::move(segments_); }

 private:
  std::vector<ItemNameSegment> segments_;
};

[[nodiscard]] std::string_view BaseNameOf(const ItemTable& items, std::uint32_t item_id,
                                          std::string& scratch) {
  const ItemEntry* entry = items.Find(item_id);
  if (entry != nullptr && !entry->name.empty()) {
    return entry->name;
  }
  scratch = std::string(kUnknownItemPrefix) + std::to_string(item_id);
  return scratch;
}

[[nodiscard]] std::uint8_t SlotCountOf(const ItemTable& items, std::uint32_t item_id) {
  const ItemEntry* entry = items.Find(item_id);
  return entry != nullptr ? entry->card_slots : 0;
}

// Les cartes de cet objet portent-elles des mots ?
//
// ⚠ « PRÉSENTE DANS LA TABLE » ET « PORTE UN MOT » NE SONT PAS LA MÊME CHOSE,
// et le client se sert des deux. Ici c'est la présence qui décide, comme chez
// lui : un objet dont une carte est connue mais muette reste sur la voie des
// cartes, et n'écrit simplement rien pour celle-là.
[[nodiscard]] bool HasCardWords(const ItemNameFacts& facts, const ItemWords& words) {
  const bool first_known = words.FindCard(facts.cards[0]) != nullptr;
  if (ItemIdIsPetRange(facts.item_id)) {
    return first_known;
  }
  if (facts.cards[0] == kCardMarkerForged || facts.cards[0] == kCardMarkerCreated) {
    return first_known;
  }
  if (!ItemTypeIsDecorated(facts.type)) {
    return first_known;
  }
  if (first_known) {
    return true;
  }
  for (std::size_t slot = 1; slot < kItemNameCardSlots; ++slot) {
    if (words.FindCard(facts.cards[slot]) != nullptr) {
      return true;
    }
  }
  return false;
}

// Écrit les mots des cartes d'un côté du nom.
//
// Les cartes identiques se comptent et se CONSOMMENT : la première d'une série
// écrit son mot une fois, précédé — ou suivi — du mot de multiplicité, et les
// suivantes disparaissent. Chaque côté travaille sur sa propre copie, donc une
// même carte se compte des deux côtés sans se gêner ; comme son mot ne vit que
// d'un côté, un seul des deux passages écrit.
void AddCardWords(NameBuilder& out, const ItemNameFacts& facts, const ItemWords& words,
                  bool after_name) {
  std::array<std::uint32_t, kItemNameCardSlots> remaining = facts.cards;
  for (std::size_t slot = 0; slot < kItemNameCardSlots; ++slot) {
    const std::uint32_t card_id = remaining[slot];
    if (card_id == 0) {
      continue;
    }
    const CardWord* word = words.FindCard(card_id);
    if (word == nullptr || word->text.empty() || word->after_name != after_name) {
      continue;
    }
    std::size_t identical = 1;
    for (std::size_t other = slot + 1; other < kItemNameCardSlots; ++other) {
      if (remaining[other] == card_id) {
        remaining[other] = 0;
        ++identical;
      }
    }
    const std::string_view repeated = words.RepeatedWord(identical);
    if (after_name) {
      out.Add(kWordAfterPrefix);
      out.Add(word->text);
      if (!repeated.empty()) {
        out.Add(kWordAfterPrefix);
        out.Add(repeated);
      }
    } else {
      if (!repeated.empty()) {
        out.Add(repeated);
        out.Add(kWordBeforeSuffix);
      }
      out.Add(word->text);
      out.Add(kWordBeforeSuffix);
    }
  }
}

void AddCreator(NameBuilder& out, const ItemNameFacts& facts, const ItemWords& words) {
  if (!facts.creator_name.empty()) {
    out.AddCreator(facts.creator_name, ItemNameRole::kCreator);
    return;
  }
  out.AddCreator(words.nameless_creator, ItemNameRole::kUnknownCreator);
}

void AddRefineAndGrade(NameBuilder& out, const ItemNameFacts& facts) {
  if (facts.refine_level > 0) {
    out.Add(std::format("+{} ", facts.refine_level));
  }
  if (facts.grade > 0) {
    const std::size_t rank = static_cast<std::size_t>(facts.grade);
    out.Add(std::format("[{}] ",
                        rank < kGradeLetters.size() ? kGradeLetters[rank] : std::string_view()));
  }
}

void AddSlotCount(NameBuilder& out, const ItemTable& items, std::uint32_t item_id,
                  ItemNameSlots slots) {
  if (slots == ItemNameSlots::kHidden) {
    return;
  }
  const std::uint8_t count = SlotCountOf(items, item_id);
  if (count > 0) {
    out.Add(std::format(" [{}]", static_cast<unsigned>(count)));
  }
}

}  // namespace

bool ItemTypeIsDecorated(std::int32_t type) {
  switch (type) {
    case 4:
    case 5:
    case 8:
    case 9:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 21:
      return true;
    default:
      return false;
  }
}

bool ItemIdIsPetRange(std::uint32_t item_id) {
  return (item_id >= kPetEggIdFirst && item_id <= kPetEggIdLast) ||
         (item_id >= kPetGearIdFirst && item_id <= kPetGearIdLast);
}

std::vector<ItemNameSegment> ComposeItemName(const ItemNameFacts& facts, const ItemTable& items,
                                             const ItemWords& words, ItemNameSlots slots) {
  NameBuilder out;
  std::string scratch;
  const std::string_view base = BaseNameOf(items, facts.item_id, scratch);

  // Un objet non identifié n'a QUE son nom : ni raffinement, ni cartes, ni
  // emplacements. Le client va chercher un second nom que cette installation ne
  // déclare nulle part — on retombe donc sur le même, ce qui ne coûte rien : ce
  // serveur ne crée jamais d'objet non identifié.
  if (!facts.identified) {
    out.Add(base);
    return out.Take();
  }

  if (facts.type == kWeaponType && ItemIdIsPetRange(facts.item_id) &&
      facts.cards[kItemNameCardSlots - 1] == kRenamedPetMarker) {
    out.Add(words.beloved_pet);
  }

  const std::uint32_t element = facts.cards[1] & kByteMask;
  const std::size_t stars = (facts.cards[1] >> kByteShift) & kByteMask;

  // Un objet CRÉÉ sans étoiles prend une voie à lui : son créateur, le
  // possessif neutre, son nom — et rien d'autre, pas même ses emplacements.
  if (facts.cards[0] == kCardMarkerCreated && facts.cards[1] == 0) {
    AddRefineAndGrade(out, facts);
    AddCreator(out, facts, words);
    out.Add(words.PossessiveWord(facts.item_id, element));
    out.Add(base);
    return out.Take();
  }

  // Ce qui ne se porte pas ne se décore pas : consommables, matériaux, cartes.
  if (!ItemTypeIsDecorated(facts.type)) {
    out.Add(base);
    AddSlotCount(out, items, facts.item_id, slots);
    return out.Take();
  }

  AddRefineAndGrade(out, facts);

  if (HasCardWords(facts, words)) {
    AddCardWords(out, facts, words, /*after_name=*/false);
    out.Add(base);
    AddCardWords(out, facts, words, /*after_name=*/true);
  } else if (facts.cards[0] == kCardMarkerForged || facts.cards[0] == kCardMarkerCreated) {
    // ⚠ CETTE VOIE N'AJOUTE PAS LES EMPLACEMENTS, et ce n'est pas un oubli : le
    // client en sort avant d'y arriver. Une arme forgée n'affiche jamais son
    // « [N] ».
    out.Add(words.ForgeStrengthWord(stars));
    AddCreator(out, facts, words);
    out.Add(words.PossessiveWord(facts.item_id, element));
    out.Add(base);
    return out.Take();
  } else {
    out.Add(base);
  }

  AddSlotCount(out, items, facts.item_id, slots);
  return out.Take();
}

std::string ItemNameText(std::span<const ItemNameSegment> segments) {
  std::string text;
  std::size_t total = 0;
  for (const ItemNameSegment& segment : segments) {
    total += segment.text.size();
  }
  text.reserve(total);
  for (const ItemNameSegment& segment : segments) {
    text.append(segment.text);
  }
  return text;
}

}  // namespace ro::vfs
