#include "chat/chat_item_link.h"

#include <algorithm>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/vfs/item_link.h"
#include "ro/vfs/item_name.h"

#include "chat/chat_setting_link.h"

namespace moonclient {
namespace {

// Le premier octet des huit balises, `<ITEML>`, `<STYL>`, `<MOBL>`, `<CRAF>`,
// `<NAVIL>`, `<MVPL>`, `<NAVS>` et `<SETL>`. ⚠ UNE ESPÈCE QUI S'OUVRIRAIT AUTREMENT ne
// passerait pas `MayCarryChatTags`, et resterait à l'écran telle quelle.
constexpr char kChatTagLead = '<';
static_assert(ro::vfs::kItemLinkOpen.front() == kChatTagLead);
static_assert(kMonsterLinkOpen.front() == kChatTagLead);
static_assert(kPlaceLinkOpen.front() == kChatTagLead);
static_assert(kRecipeLinkOpen.front() == kChatTagLead);
static_assert(kMvpLinkOpen.front() == kChatTagLead);
static_assert(kNaviSearchLinkOpen.front() == kChatTagLead);
static_assert(kSettingLinkOpen.front() == kChatTagLead);

// UNE BALISE D'OBJET, posée dans la ligne en cours de composition. Rend ce
// qu'il faut sauter dans la source.
[[nodiscard]] std::size_t PlaceItemTag(std::string_view tail, const ro::vfs::ItemTable& items,
                                       const ro::vfs::ItemWords& words,
                                       const CreatorNames* creators, DecoratedChatLine* out) {
  ro::vfs::ItemLink link;
  std::size_t after = 0;
  const bool read = ro::vfs::ParseItemLink(tail, &link, &after);
  std::string name;
  if (read) {
    // UN ITEM FORGÉ LIÉ se nomme par son forgeron, comme dans le sac.
    ro::vfs::ItemNameFacts facts = ro::vfs::ItemLinkNameFacts(link);
    if (creators != nullptr) {
      creators->Fill(facts);
    }
    name = ro::vfs::ItemNameText(ro::vfs::ComposeItemName(facts, items, words));
  }
  if (name.empty()) {
    // ⚠ TELLE QUELLE : une balise illisible reste à l'écran. Elle est laide,
    // et c'est mieux qu'une ligne amputée sans que rien ne le dise.
    out->text.append(tail.substr(0, after));
    return after;
  }
  out->text.push_back(kChatLinkOpen);
  const auto offset = static_cast<std::uint32_t>(out->text.size());
  out->text.append(name);
  // ⚠ LES CHEVRONS SONT HORS DU MORCEAU CLIQUABLE, et c'est voulu : ils
  // encadrent le lien, ils n'en font pas partie — la main ne s'allume pas sur
  // la ponctuation.
  out->links.push_back(ro::ui::ChatLinkSpan{.offset = offset,
                                            .length = static_cast<std::uint32_t>(name.size()),
                                            .kind = ro::ui::ChatLinkKind::kItem,
                                            .item_id = link.item_id,
                                            .damaged = link.damaged});
  out->carried.push_back(ChatLinkTarget{.kind = ro::ui::ChatLinkKind::kItem, .item = link});
  out->text.push_back(kChatLinkClose);
  return after;
}

// UNE BALISE DE STYLE. Même contrat de retour.
[[nodiscard]] std::size_t PlaceStyleTag(std::string_view tail, DecoratedChatLine* out) {
  ro::sim::StyleTag tag;
  std::size_t after = 0;
  switch (ro::sim::ParseStyleTag(tail, &tag, &after)) {
    case ro::sim::StyleTagRead::kOk: {
      const std::string label = StyleLinkLabel(tag.label);
      const auto offset = static_cast<std::uint32_t>(out->text.size());
      out->text.append(label);
      // ⚠ LES CROCHETS SONT DANS LE MORCEAU CLIQUABLE, contrairement aux
      // chevrons d'un objet : ils ne décorent pas un nom, ils FONT le
      // libellé — « Style: » compris, qui est ce qui se lit.
      out->links.push_back(ro::ui::ChatLinkSpan{.offset = offset,
                                                .length = static_cast<std::uint32_t>(label.size()),
                                                .kind = ro::ui::ChatLinkKind::kStyle});
      out->carried.push_back(ChatLinkTarget{.kind = ro::ui::ChatLinkKind::kStyle,
                                            .code = std::move(tag.code),
                                            .owner = std::move(tag.label)});
      break;
    }
    case ro::sim::StyleTagRead::kUnreadable:
      // 🔴 PAS DE LIEN, ET L'ÉTIQUETTE SEULE. Une balise d'objet illisible
      // reste à l'écran telle quelle parce qu'elle est courte ; celle-ci fait
      // cent dix caractères d'hexadécimal, qui n'apprennent rien à personne.
      // On garde donc le nom, sans prétendre mener quelque part.
      out->text.push_back('[');
      out->text.append(tag.label);
      out->text.push_back(']');
      break;
    case ro::sim::StyleTagRead::kNotATag:
      // Fermante absente : la balise est tronquée, et elle reste telle
      // quelle — une ligne amputée en silence est pire qu'une ligne laide.
      out->text.append(tail);
      break;
  }
  return after;
}

// UNE BALISE DE MONSTRE. Même contrat de retour.
[[nodiscard]] std::size_t PlaceMonsterTag(std::string_view tail, DecoratedChatLine* out) {
  MonsterLink link;
  std::size_t after = 0;
  if (!ParseMonsterLink(tail, &link, &after)) {
    // ⚠ TELLE QUELLE, comme une balise d'objet illisible : elle porte le nom en
    // clair, et reste lisible même brute.
    out->text.append(tail.substr(0, after));
    return after;
  }
  const std::string name = MonsterLinkName(link);
  out->text.push_back(kChatLinkOpen);
  const auto offset = static_cast<std::uint32_t>(out->text.size());
  out->text.append(name);
  out->links.push_back(ro::ui::ChatLinkSpan{.offset = offset,
                                            .length = static_cast<std::uint32_t>(name.size()),
                                            .kind = ro::ui::ChatLinkKind::kMonster,
                                            .item_id = link.monster_id});
  out->carried.push_back(
      ChatLinkTarget{.kind = ro::ui::ChatLinkKind::kMonster, .monster = std::move(link)});
  out->text.push_back(kChatLinkClose);
  return after;
}

// UNE BALISE DE RECETTE. Même contrat de retour.
[[nodiscard]] std::size_t PlaceRecipeTag(std::string_view tail, DecoratedChatLine* out) {
  RecipeLink link;
  std::size_t after = 0;
  if (!ParseRecipeLink(tail, &link, &after)) {
    // ⚠ TELLE QUELLE : elle porte le nom en clair, et reste lisible brute.
    out->text.append(tail.substr(0, after));
    return after;
  }
  // ⚠ LES CROCHETS DANS LE MORCEAU CLIQUABLE, comme ceux d'un style : ils FONT
  // le libellé, « Recette : » compris.
  //
  // 🔴 UN LIEN MÊME QUAND CE LECTEUR N'A PAS LA RECETTE : son clic mène à
  // l'Atlas, qui DIT pourquoi il ne peut rien montrer — fichier absent, ou item
  // sans recette. Un lien muet ne dit rien.
  const std::string label = RecipeLinkLabel(link.name);
  const auto offset = static_cast<std::uint32_t>(out->text.size());
  out->text.append(label);
  out->links.push_back(ro::ui::ChatLinkSpan{.offset = offset,
                                            .length = static_cast<std::uint32_t>(label.size()),
                                            .kind = ro::ui::ChatLinkKind::kRecipe,
                                            .item_id = link.item_id});
  out->carried.push_back(
      ChatLinkTarget{.kind = ro::ui::ChatLinkKind::kRecipe, .recipe = std::move(link)});
  return after;
}

// UNE BALISE DE LIEU. Même contrat de retour.
[[nodiscard]] std::size_t PlaceLocationTag(std::string_view tail,
                                           const ro::vfs::MapDisplayNames* maps,
                                           DecoratedChatLine* out) {
  PlaceLink link;
  std::size_t after = 0;
  if (!ParsePlaceLink(tail, &link, &after)) {
    // ⚠ TELLE QUELLE, comme une balise illisible : une queue qui n'est pas du
    // base 62 n'est pas une balise, et on n'invente pas de coordonnées.
    out->text.append(tail.substr(0, after));
    return after;
  }
  // ⚠ LES CROCHETS DANS LE MORCEAU CLIQUABLE, comme ceux d'un style : ils FONT
  // le libellé, coordonnées comprises.
  const std::string label = PlaceLinkLabel(link, maps);
  const auto offset = static_cast<std::uint32_t>(out->text.size());
  out->text.append(label);
  out->links.push_back(ro::ui::ChatLinkSpan{.offset = offset,
                                            .length = static_cast<std::uint32_t>(label.size()),
                                            .kind = ro::ui::ChatLinkKind::kPlace});
  out->carried.push_back(
      ChatLinkTarget{.kind = ro::ui::ChatLinkKind::kPlace, .place = std::move(link)});
  return after;
}

// UNE BALISE DE RESPAWN DE MVP. Même contrat de retour.
//
// ⚠ L'EXPÉDITEUR N'EST PAS POSÉ ICI : la ligne seule ne le dit pas toujours —
// un chuchotement se résout par son sens. C'est l'appelant qui le connaît.
[[nodiscard]] std::size_t PlaceMvpTag(std::string_view tail, const MvpLinkClock* mvp_clock,
                                      DecoratedChatLine* out) {
  MvpLink link;
  std::size_t after = 0;
  std::string_view body;
  switch (ParseMvpLink(tail, &link, &after, &body)) {
    case MvpLinkRead::kRead:
      break;
    case MvpLinkRead::kUnclosed:
      // ⚠ FERMANTE ABSENTE : l'ouvrante seule reste du texte, et la lecture
      // reprend juste après elle. Les liens qui la suivent se rendent encore,
      // comme chez les autres lecteurs de la balise.
      out->text.append(kMvpLinkOpen);
      return kMvpLinkOpen.size();
    case MvpLinkRead::kUnreadable:
      // ⚠ LA LIGNE SURVIT, EN TEXTE : le corps entre crochets, sans prétendre
      // mener quelque part.
      out->text.push_back('[');
      out->text.append(body);
      out->text.push_back(']');
      return after;
    case MvpLinkRead::kNoTag:
      // `FindMvpLink` l'a trouvée : ce cas ne se présente pas. Rien à sauter,
      // et la boucle s'arrête.
      out->text.append(tail);
      return tail.size();
  }
  std::string label = MvpLinkLabel(link);
  const bool spent =
      mvp_clock != nullptr && MvpLinkPassed(link, mvp_clock->server_now_s, mvp_clock->book);
  std::size_t mark_at = 0;
  std::string mark;
  if (spent) {
    // ⚠ L'ESPACE HORS DE LA CLÉ : une clé de traduction ne commence jamais par
    // une espace. La marque se glisse AVANT le crochet fermant.
    mark = " ";
    mark += ro::core::Tr("· passé");
    mark_at = label.ends_with(']') ? label.size() - 1 : label.size();
    label.insert(mark_at, mark);
  }
  // ⚠ LES CROCHETS DANS LE MORCEAU CLIQUABLE, comme ceux d'un style : ils FONT
  // le libellé.
  const auto offset = static_cast<std::uint32_t>(out->text.size());
  out->text.append(label);
  out->links.push_back(
      ro::ui::ChatLinkSpan{.offset = offset,
                           .length = static_cast<std::uint32_t>(label.size()),
                           .kind = ro::ui::ChatLinkKind::kMvp,
                           .item_id = link.monster_id,
                           .spent = spent,
                           .mark_at = static_cast<std::uint32_t>(mark_at),
                           .mark_length = static_cast<std::uint32_t>(mark.size())});
  out->carried.push_back(
      ChatLinkTarget{.kind = ro::ui::ChatLinkKind::kMvp, .mvp = std::move(link)});
  return after;
}

// UNE BALISE DE RECHERCHE DE NAVIGATION. Même contrat de retour.
[[nodiscard]] std::size_t PlaceNaviSearchTag(std::string_view tail,
                                             const ro::vfs::MapDisplayNames* maps,
                                             DecoratedChatLine* out) {
  NaviSearchLink link;
  std::size_t after = 0;
  std::string_view raw_term;
  switch (ParseNaviSearchLink(tail, &link, &after, &raw_term)) {
    case NaviSearchLinkRead::kRead:
      break;
    case NaviSearchLinkRead::kNotATag:
      // ⚠ L'OUVRANTE SEULE RESTE DU TEXTE, et la lecture reprend juste après
      // elle, comme chez les autres lecteurs de la balise.
      out->text.append(kNaviSearchLinkOpen);
      return kNaviSearchLinkOpen.size();
    case NaviSearchLinkRead::kAsText:
      // UNE FAMILLE INCONNUE, ou un terme refusé : le terme en texte, sans
      // prétendre mener quelque part.
      out->text.push_back('[');
      out->text.append(raw_term);
      out->text.push_back(']');
      return after;
    case NaviSearchLinkRead::kDropped:
      // Un terme vide : rien à montrer.
      return after;
    case NaviSearchLinkRead::kNoTag:
      out->text.append(tail);
      return tail.size();
  }
  // ⚠ LES CROCHETS DANS LE MORCEAU CLIQUABLE, comme ceux d'un style : ils FONT
  // le libellé, « PNJ: » compris.
  const std::string label = NaviSearchLinkLabel(link, maps);
  const auto offset = static_cast<std::uint32_t>(out->text.size());
  out->text.append(label);
  out->links.push_back(ro::ui::ChatLinkSpan{.offset = offset,
                                            .length = static_cast<std::uint32_t>(label.size()),
                                            .kind = ro::ui::ChatLinkKind::kNaviSearch,
                                            .navi = NaviSearchFactsOf(link)});
  out->carried.push_back(
      ChatLinkTarget{.kind = ro::ui::ChatLinkKind::kNaviSearch, .navi_search = std::move(link)});
  return after;
}

// UNE BALISE DE RÉGLAGE. Même contrat de retour.
//
// 🔴 LE REFUS SE DÉCIDE ICI, JAMAIS AU CLIC : une clé inconnue, ou une
// destination que ce lecteur n'a pas, laisse le libellé de repli en texte
// inerte. Un lien qui n'ouvrirait rien serait une promesse trahie ; un texte
// entre crochets se comprend tout seul.
[[nodiscard]] std::size_t PlaceSettingTag(std::string_view tail, const SettingReach* settings,
                                          DecoratedChatLine* out) {
  SettingLink link;
  std::size_t after = 0;
  switch (ParseSettingLink(tail, &link, &after)) {
    case SettingLinkRead::kRead:
      break;
    case SettingLinkRead::kUnclosed:
    case SettingLinkRead::kNoSeparator:
      // ⚠ TEXTE BRUT, comme une balise d'objet illisible : ce qu'on ne sait pas
      // couper ne s'invente pas. Sans fermante, seule l'ouvrante est sautée, et
      // les liens d'après se lisent encore.
      out->text.append(tail.substr(0, after));
      return after;
  }
  const SettingDestination* const destination = FindSettingDestination(link.key);
  // UNE CLÉ CONNUE DONT LA DESTINATION MANQUE ICI se nomme chez le lecteur,
  // dans sa langue : seul ce qu'on ne connaît pas garde le libellé transporté.
  if (destination != nullptr &&
      !SettingDestinationReachable(*destination,
                                   settings != nullptr ? *settings : SettingReach{})) {
    out->text.push_back('[');
    out->text.append(SettingDestinationPath(*destination));
    out->text.push_back(']');
    return after;
  }
  if (destination == nullptr) {
    // ⚠ UN LIBELLÉ VIDE NE MONTRE RIEN : des crochets vides ne diraient de quoi
    // on parle à personne.
    if (!link.fallback.empty()) {
      out->text.push_back('[');
      out->text.append(link.fallback);
      out->text.push_back(']');
    }
    return after;
  }
  // ⚠ LES CROCHETS DANS LE MORCEAU CLIQUABLE, comme ceux d'un style : ils FONT
  // le libellé, « Réglage: » compris.
  const std::string label = SettingLinkLabel(*destination);
  const auto offset = static_cast<std::uint32_t>(out->text.size());
  out->text.append(label);
  out->links.push_back(ro::ui::ChatLinkSpan{.offset = offset,
                                            .length = static_cast<std::uint32_t>(label.size()),
                                            .kind = ro::ui::ChatLinkKind::kSetting});
  out->carried.push_back(
      ChatLinkTarget{.kind = ro::ui::ChatLinkKind::kSetting, .setting_key = std::move(link.key)});
  return after;
}

}  // namespace

std::string StyleLinkLabel(std::string_view owner) {
  std::string label = "[";
  label += ro::core::Tr("Style");
  // ⚠ SANS ÉTIQUETTE, PAS DE DEUX-POINTS : la balise autorise le champ vide, et
  // « [Style: ] » se lit comme un nom qui manque — et « ? » aurait le même
  // défaut.
  if (!owner.empty()) {
    label += ": ";
    label.append(owner);
  }
  label.push_back(']');
  return label;
}

bool MayCarryChatTags(std::string_view raw) {
  return raw.find(kChatTagLead) != std::string_view::npos;
}

bool DecorateChatLine(std::string_view raw, const ro::vfs::ItemTable& items,
                      const ro::vfs::ItemWords& words, DecoratedChatLine* out,
                      const CreatorNames* creators, const ro::vfs::MapDisplayNames* maps,
                      const MvpLinkClock* mvp_clock, const SettingReach* settings) {
  if (out == nullptr || !MayCarryChatTags(raw) ||
      (ro::vfs::FindItemLink(raw, 0) == std::string_view::npos &&
       ro::sim::FindStyleTag(raw, 0) == std::string_view::npos &&
       FindMonsterLink(raw, 0) == std::string_view::npos &&
       FindRecipeLink(raw, 0) == std::string_view::npos &&
       FindPlaceLink(raw, 0) == std::string_view::npos &&
       FindMvpLink(raw, 0) == std::string_view::npos &&
       FindNaviSearchLink(raw, 0) == std::string_view::npos &&
       FindSettingLink(raw, 0) == std::string_view::npos)) {
    return false;
  }
  out->text.clear();
  out->links.clear();
  out->carried.clear();

  std::size_t at = 0;
  while (at < raw.size()) {
    // ⚠ LA PREMIÈRE DES ESPÈCES, et non l'une puis l'autre : une ligne peut
    // les porter toutes, et traiter les objets d'abord découperait la balise
    // qui les précède.
    const std::size_t item_at = ro::vfs::FindItemLink(raw, at);
    const std::size_t style_at = ro::sim::FindStyleTag(raw, at);
    const std::size_t monster_at = FindMonsterLink(raw, at);
    const std::size_t recipe_at = FindRecipeLink(raw, at);
    const std::size_t place_at = FindPlaceLink(raw, at);
    const std::size_t mvp_at = FindMvpLink(raw, at);
    const std::size_t search_at = FindNaviSearchLink(raw, at);
    const std::size_t setting_at = FindSettingLink(raw, at);
    const std::size_t tag = std::min(
        {item_at, style_at, monster_at, recipe_at, place_at, mvp_at, search_at, setting_at});
    if (tag == std::string_view::npos) {
      out->text.append(raw.substr(at));
      break;
    }
    out->text.append(raw.substr(at, tag - at));
    std::size_t after = 0;
    if (tag == style_at) {
      after = PlaceStyleTag(raw.substr(tag), out);
    } else if (tag == monster_at) {
      after = PlaceMonsterTag(raw.substr(tag), out);
    } else if (tag == recipe_at) {
      after = PlaceRecipeTag(raw.substr(tag), out);
    } else if (tag == place_at) {
      after = PlaceLocationTag(raw.substr(tag), maps, out);
    } else if (tag == mvp_at) {
      after = PlaceMvpTag(raw.substr(tag), mvp_clock, out);
    } else if (tag == search_at) {
      after = PlaceNaviSearchTag(raw.substr(tag), maps, out);
    } else if (tag == setting_at) {
      after = PlaceSettingTag(raw.substr(tag), settings, out);
    } else {
      after = PlaceItemTag(raw.substr(tag), items, words, creators, out);
    }
    // ⚠ RIEN À SAUTER = LA FIN DU TEXTE : la boucle s'arrête alors, au lieu de
    // repartir en arrière sur la même balise.
    at = tag + (after > 0 ? after : raw.size() - tag);
  }
  return true;
}

std::string ChatLinkLabel(std::string_view name) {
  std::string label(1, kChatLinkOpen);
  label.append(name);
  label.push_back(kChatLinkClose);
  return label;
}

void PruneChatLinks(std::string_view typed, std::vector<PendingChatLink>* pending) {
  if (pending == nullptr) {
    return;
  }
  std::vector<PendingChatLink> kept;
  kept.reserve(pending->size());
  std::size_t at = 0;
  for (PendingChatLink& link : *pending) {
    if (link.shown.empty()) {
      continue;
    }
    const std::size_t found = typed.find(link.shown, at);
    if (found == std::string_view::npos) {
      continue;
    }
    at = found + link.shown.size();
    kept.push_back(std::move(link));
  }
  pending->swap(kept);
}

std::string ResolveChatLinks(std::string_view typed, const std::vector<PendingChatLink>& pending) {
  std::string said;
  said.reserve(typed.size());
  std::size_t at = 0;
  for (const PendingChatLink& link : pending) {
    if (link.shown.empty()) {
      continue;
    }
    const std::size_t found = typed.find(link.shown, at);
    if (found == std::string_view::npos) {
      continue;
    }
    said.append(typed.substr(at, found - at));
    said.append(link.wire);
    at = found + link.shown.size();
  }
  said.append(typed.substr(at));
  return said;
}

bool PlaceChatLinkLabel(std::string_view label, ro::ui::ChatInputState& input,
                        std::vector<PendingChatLink>* pending) {
  if (pending == nullptr) {
    return false;
  }
  std::string line(input.text.data());
  line += input.pending_insert;
  // ⚠ ON ÉLAGUE AVANT DE COMPTER : le joueur qui efface un lien de sa saisie
  // doit pouvoir en reposer un autre sans buter sur le plafond.
  PruneChatLinks(line, pending);
  if (pending->size() >= kMostChatLinks) {
    return false;
  }
  // ⚠ SÉPARÉ DE CE QUI PRÉCÈDE : deux libellés collés ne se distingueraient
  // plus à la lecture, et la substitution retrouverait le second au milieu du
  // premier si l'un commence par l'autre.
  std::string piece;
  if (!line.empty() && line.back() != ' ') {
    piece.push_back(' ');
  }
  piece += label;
  if (!input.pending_insert.empty()) {
    return ro::ui::AppendPendingInsert(input, piece);
  }
  line += piece;
  if (line.size() + 1 > input.text.size()) {
    return false;
  }
  std::ranges::copy(line, input.text.begin());
  input.text[line.size()] = '\0';
  return true;
}

}  // namespace moonclient
