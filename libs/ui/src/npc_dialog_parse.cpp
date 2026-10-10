#include <array>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/text.h"
#include "ro/ui/npc_dialog_text.h"

namespace ro::ui {
namespace {

// ⚠ SEPT OCTETS : l'accent circonflexe et six chiffres hexadécimaux.
constexpr std::size_t kColorTokenLength = 7;

// L'espace insécable, tel qu'il arrive une fois le texte converti en UTF-8. Il
// se remplace par une espace ordinaire : gardé tel quel, il se dessine en carré
// dans une police qui ne le porte pas, et il empêche le repli à cet endroit.
constexpr std::string_view kNoBreakSpace = "\xC2\xA0";

[[nodiscard]] bool StartsWith(std::string_view text, std::size_t at, std::string_view tag) {
  return text.compare(at, tag.size(), tag) == 0;
}

[[nodiscard]] std::string UpperAscii(std::string_view text) {
  std::string upper(text);
  for (char& letter : upper) {
    letter = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
  }
  return upper;
}

// Le vocabulaire du client. 🔴 C'EST UNE LISTE BLANCHE : voir l'en-tête, et ce
// qu'un parseur glouton fait d'une phrase qui porte des chevrons.
[[nodiscard]] bool IsClientTag(const std::string& upper) {
  static constexpr std::array<std::string_view, 23> kTags = {
      "B",      "/B",    "I",      "/I",     "/FONT",   "INFO", "/INFO", "ITEM",
      "/ITEM",  "ITEML", "/ITEML", "URL",    "/URL",    "NAVI", "/NAVI", "NAVIL",
      "/NAVIL", "QUEST", "/QUEST", "TIPBOX", "/TIPBOX", "MSG",  "/MSG"};
  for (const std::string_view tag : kTags) {
    if (upper == tag) {
      return true;
    }
  }
  // `<FONT …>` porte des attributs : seul son NOM fait foi. La borne sur
  // l'espace évite qu'un « <Fontaine de jouvence> » passe pour une police.
  return upper.starts_with("FONT") && (upper.size() == 4 || upper[4] == ' ');
}

[[nodiscard]] bool AllDigits(std::string_view text) {
  if (text.empty()) {
    return false;
  }
  for (const char letter : text) {
    if (letter < '0' || letter > '9') {
      return false;
    }
  }
  return true;
}

[[nodiscard]] std::uint32_t Number(std::string_view text) {
  std::uint32_t value = 0;
  const char* const first = text.data();
  std::from_chars(first, first + text.size(), value);
  return value;
}

// Le corps d'une balise `<TAG>…</TAG>` ouverte ici. Rend faux si ce n'est pas
// celle-là, ou si sa fermante manque — auquel cas RIEN n'est consommé et le
// texte reste tel quel, plutôt que d'avaler la fin de la ligne.
[[nodiscard]] bool TagBody(std::string_view line, std::size_t at, std::string_view open,
                           std::string_view close, std::string_view* body, std::size_t* after) {
  if (!StartsWith(line, at, open)) {
    return false;
  }
  const std::size_t start = at + open.size();
  const std::size_t stop = line.find(close, start);
  if (stop == std::string_view::npos) {
    return false;
  }
  *body = line.substr(start, stop - start);
  *after = stop + close.size();
  return true;
}

// Découpe `count` champs séparés par ':' — le DERNIER prend tout ce qui reste,
// ':' compris. Rend faux s'il en manque.
[[nodiscard]] bool SplitFields(std::string_view body, std::size_t count,
                               std::span<std::string_view> out) {
  for (std::size_t rank = 0; rank + 1 < count; ++rank) {
    const std::size_t colon = body.find(':');
    if (colon == std::string_view::npos) {
      return false;
    }
    out[rank] = body.substr(0, colon);
    body = body.substr(colon + 1);
  }
  out[count - 1] = body;
  return true;
}

// LE PRÉFIXE D'UN LIEN DE MONSTRE, celui que la chatbox emploie déjà.
//
// ⚠ IL N'EST PAS TRADUIT, et c'est le jargon du jeu qui le veut : « MVP » et
// « Boss » se disent ainsi sur tous les serveurs, et les franciser casserait des
// habitudes plus anciennes que ce client.
[[nodiscard]] std::string_view MonsterRankTag(MonsterRank rank) {
  switch (rank) {
    case MonsterRank::kBoss:
      return "[Boss]";
    case MonsterRank::kChampion:
      return "[MVP]";
    case MonsterRank::kOrdinary:
      break;
  }
  return "[Mob]";
}

// Une balise de la MAISON ouverte ici ? Rend faux si ce n'en est pas une, et le
// parseur reprend alors son traitement ordinaire.
//
// Une balise mal formée — fermante absente, champs manquants, identifiant nul —
// n'est PAS consommée : elle s'affichera telle quelle, chevrons compris. C'est
// le bon échec ; un script fautif se voit, là où l'escamotage se cherche une
// heure.
[[nodiscard]] bool TryHouseTag(std::string_view line, std::size_t at, const DialogVocabulary& words,
                               DialogRun* made, std::size_t* after) {
  std::string_view body;
  std::array<std::string_view, 3> fields;

  if (TagBody(line, at, "<MOBL>", "</MOBL>", &body, after)) {
    if (!SplitFields(body, 3, fields) || !AllDigits(fields[0]) || fields[2].empty()) {
      return false;
    }
    const std::uint32_t id = Number(fields[0]);
    if (id == 0) {
      return false;
    }
    const std::uint32_t rank = Number(fields[1]);
    made->target.kind = DialogTargetKind::kMonster;
    made->target.id = id;
    made->target.rank = rank == 1   ? MonsterRank::kBoss
                        : rank == 2 ? MonsterRank::kChampion
                                    : MonsterRank::kOrdinary;
    made->target.label = std::string(fields[2]);
    made->text =
        "<" + std::string(MonsterRankTag(made->target.rank)) + " " + made->target.label + ">";
    return true;
  }
  if (TagBody(line, at, "<ITMR>", "</ITMR>", &body, after)) {
    if (!SplitFields(body, 2, fields) || !AllDigits(fields[0]) || fields[1].empty()) {
      return false;
    }
    const std::uint32_t id = Number(fields[0]);
    if (id == 0) {
      return false;
    }
    // ⚠ LE NOM TRANSPORTÉ, PAS CELUI DE NOTRE TABLE : celui qui écrit le script
    // sait de quel objet il parle, et un client dans une autre langue ne doit pas
    // le renommer.
    made->target.kind = DialogTargetKind::kItem;
    made->target.id = id;
    made->target.label = std::string(fields[1]);
    made->text = "<" + made->target.label + ">";
    return true;
  }
  if (TagBody(line, at, "<CRAF>", "</CRAF>", &body, after)) {
    if (!SplitFields(body, 2, fields) || !AllDigits(fields[0]) || fields[1].empty()) {
      return false;
    }
    const std::uint32_t id = Number(fields[0]);
    if (id == 0) {
      return false;
    }
    made->target.kind = DialogTargetKind::kRecipe;
    made->target.id = id;
    made->target.label = std::string(fields[1]);
    // ⚠ CE LIEN NE MÈNE PAS À L'OBJET MAIS À SA RECETTE, et son libellé doit le
    // dire : sans cela, deux liens d'aspect identique ouvriraient deux choses
    // différentes.
    std::array<char, 256> label = {};
    std::snprintf(label.data(), label.size(), core::Tr("[Recette : %s]"),
                  made->target.label.c_str());
    made->text = std::string(core::WithoutDanglingUtf8(label.data()));
    return true;
  }
  if (TagBody(line, at, "<SETL>", "</SETL>", &body, after)) {
    if (!SplitFields(body, 2, fields) || fields[0].empty() || fields[1].empty()) {
      return false;
    }
    // 🔴 C'EST LA CLÉ QUI FAIT FOI, ET LE LECTEUR QUI NOMME. Une destination que
    // ce client n'a pas ne forme AUCUN lien : le libellé du script reste, en
    // texte simple, et le fragment retombe sur le traitement ordinaire.
    const std::string named = words.setting_label != nullptr
                                  ? words.setting_label(fields[0], words.owner)
                                  : std::string();
    if (named.empty()) {
      // ⚠ PAS DE LIEN, MAIS PAS DE BALISE NON PLUS : le libellé transporté
      // s'affiche seul. Un lien qui n'ouvre rien vaut moins que pas de lien.
      made->text = std::string(fields[1]);
      return true;
    }
    made->target.kind = DialogTargetKind::kSetting;
    made->target.key = std::string(fields[0]);
    // ⚠ LE LIBELLÉ ARRIVE COMPOSÉ, enveloppe comprise — « [Réglage: …] » ou
    // « [Fenêtre: …] » : c'est celui du fil, et deux typographies pour un même
    // lien se liraient comme deux choses.
    made->target.label = named;
    made->text = named;
    return true;
  }
  if (TagBody(line, at, "<STAL>", "</STAL>", &body, after)) {
    if (!SplitFields(body, 2, fields) || !AllDigits(fields[0]) || fields[1].empty()) {
      return false;
    }
    const std::uint32_t index = Number(fields[0]);
    if (index == 0) {
      return false;
    }
    // 🔴 C'EST L'INDEX QUI FAIT FOI, JAMAIS LE LIBELLÉ DU SCRIPT : chaque client
    // nomme les états dans SA langue, depuis ses propres tables. Un PNJ écrit
    // donc l'index, et chacun lit le nom qu'il connaît.
    const std::string named =
        words.status_label != nullptr ? words.status_label(index, words.owner) : std::string();
    if (named.empty()) {
      // L'état est absent de CETTE installation : le libellé transporté reste,
      // en texte simple. Il dit encore de quoi on parle, il ne prétend plus rien
      // montrer.
      made->text = std::string(fields[1]);
      return true;
    }
    made->target.kind = DialogTargetKind::kStatus;
    made->target.id = index;
    made->target.label = named;
    std::array<char, 192> shown = {};
    std::snprintf(shown.data(), shown.size(), core::Tr("[État : %s]"), named.c_str());
    made->text = std::string(core::WithoutDanglingUtf8(shown.data()));
    return true;
  }
  if (TagBody(line, at, "<MOBS>", "</MOBS>", &body, after)) {
    if (!AllDigits(body)) {
      return false;
    }
    made->media = DialogMedia::kMonsterSprite;
    made->monster_id = Number(body);
    return made->monster_id != 0;
  }
  if (TagBody(line, at, "<MOBP>", "</MOBP>", &body, after)) {
    // Le portrait ne produit AUCUN fragment : il vaut pour la page entière, et
    // `PageMonsterPortrait` l'a déjà relevé. Ici on ne fait que l'effacer du
    // texte, pour qu'il n'y laisse pas ses chevrons.
    return AllDigits(body);
  }
  if (TagBody(line, at, "<IMG>", "</IMG>", &body, after)) {
    // Deux écritures : `source` seule, ou `largeur:hauteur:source`. On ne tente
    // la seconde que si les deux premiers champs sont des NOMBRES — sans quoi le
    // « https: » d'une adresse passerait pour une taille.
    std::string_view source = body;
    if (SplitFields(body, 3, fields) && AllDigits(fields[0]) && AllDigits(fields[1])) {
      made->wanted_width = static_cast<float>(Number(fields[0]));
      made->wanted_height = static_cast<float>(Number(fields[1]));
      source = fields[2];
    }
    if (source.empty()) {
      return false;
    }
    made->media = DialogMedia::kImage;
    made->source = std::string(source);
    return true;
  }
  return false;
}

}  // namespace

std::vector<DialogRun> ParseDialogLine(std::string_view line, const DialogVocabulary& words) {
  std::vector<DialogRun> runs;
  DialogRun current;
  // Dans `<INFO>…</INFO>` : on capture la charge du lien, elle ne s'affiche pas.
  bool inside_payload = false;

  const auto flush = [&runs, &current]() {
    if (current.text.empty()) {
      return;
    }
    runs.push_back(current);
    current.text.clear();
  };

  for (std::size_t at = 0; at < line.size();) {
    // La couleur. ⚠ `^000000` n'est pas du noir : c'est le retour au défaut, et
    // zéro est ce qui le dit ici.
    if (line[at] == '^' && at + kColorTokenLength <= line.size()) {
      const int red = HexByte(line.substr(at + 1, 2));
      const int green = HexByte(line.substr(at + 3, 2));
      const int blue = HexByte(line.substr(at + 5, 2));
      if (red >= 0 && green >= 0 && blue >= 0) {
        flush();
        current.color = (red == 0 && green == 0 && blue == 0) ? 0 : IM_COL32(red, green, blue, 255);
        at += kColorTokenLength;
        continue;
      }
    }
    // L'icône d'un objet, au fil du texte.
    if (line[at] == '^' && at + 3 < line.size() && (line[at + 1] == 'i' || line[at + 1] == 'I') &&
        line[at + 2] == '[') {
      const std::size_t stop = line.find(']', at + 3);
      if (stop != std::string_view::npos) {
        const std::string_view digits = line.substr(at + 3, stop - at - 3);
        if (AllDigits(digits)) {
          flush();
          DialogRun icon;
          icon.icon_item = Number(digits);
          runs.push_back(std::move(icon));
          at = stop + 1;
          continue;
        }
      }
    }
    if (line[at] == '<') {
      // Les balises de la MAISON d'abord : elles ont un corps structuré et une
      // fermante explicite, là où le traitement générique ne lit qu'un nom entre
      // chevrons et jetterait le reste.
      DialogRun made;
      made.color = current.color;
      made.bold = current.bold;
      made.italic = current.italic;
      std::size_t after = at;
      if (TryHouseTag(line, at, words, &made, &after)) {
        flush();
        // Un lien d'OBJET porte son icône, comme dans la chatbox : c'est ce qui
        // rend une liste de composants lisible d'un coup d'œil. Les autres genres
        // n'en ont pas — une recette n'est pas l'objet, un monstre a son sprite.
        if (made.target.kind == DialogTargetKind::kItem && made.target.id != 0) {
          DialogRun icon;
          icon.icon_item = made.target.id;
          runs.push_back(std::move(icon));
        }
        if (made.media != DialogMedia::kNone || !made.text.empty()) {
          runs.push_back(std::move(made));
        }
        at = after;
        continue;
      }
      // Puis celles du client. Ce qui n'est pas dans la liste blanche retombe
      // sur le texte ordinaire, chevrons compris.
      const std::size_t close = line.find('>', at);
      if (close != std::string_view::npos) {
        const std::string upper = UpperAscii(line.substr(at + 1, close - at - 1));
        if (IsClientTag(upper)) {
          at = close + 1;
          if (upper == "B") {
            flush();
            current.bold = true;
          } else if (upper == "/B") {
            flush();
            current.bold = false;
          } else if (upper == "I") {
            flush();
            current.italic = true;
          } else if (upper == "/I") {
            flush();
            current.italic = false;
          } else if (upper.starts_with("FONT") || upper == "/FONT") {
            flush();
          } else if (upper == "INFO") {
            // ⚠ PAS DE `flush` : la charge appartient au lien déjà ouvert, et
            // vider ici couperait son libellé en deux morceaux cliquables.
            inside_payload = true;
          } else if (upper == "/INFO") {
            inside_payload = false;
          } else if (upper == "ITEM" || upper == "ITEML") {
            flush();
            current.link = DialogLink::kItem;
            current.payload.clear();
          } else if (upper == "URL") {
            flush();
            current.link = DialogLink::kAddress;
            current.payload.clear();
          } else if (upper == "NAVI" || upper == "NAVIL") {
            flush();
            current.link = DialogLink::kPlace;
            current.payload.clear();
          } else if (upper == "QUEST") {
            flush();
            current.link = DialogLink::kQuest;
            current.payload.clear();
          } else if (upper == "TIPBOX" || upper == "MSG") {
            flush();
            current.link = DialogLink::kNotice;
            current.payload.clear();
          } else if (upper.starts_with('/')) {
            // La fermante d'un lien : la liste blanche a déjà écarté les autres.
            flush();
            current.link = DialogLink::kNone;
            current.payload.clear();
          }
          continue;
        }
      }
    }
    if (StartsWith(line, at, kNoBreakSpace)) {
      (inside_payload ? current.payload : current.text).push_back(' ');
      at += kNoBreakSpace.size();
      continue;
    }
    // ⚠ LA CHARGE SE CAPTURE INTÉGRALEMENT : un identifiant d'objet comme une
    // adresse, sans rien en interpréter ici.
    (inside_payload ? current.payload : current.text).push_back(line[at]);
    ++at;
  }
  flush();
  return runs;
}

std::uint32_t PageMonsterPortrait(std::span<const std::string> lines) {
  std::uint32_t portrait = 0;
  for (const std::string& line : lines) {
    std::size_t at = 0;
    while (true) {
      const std::size_t open = line.find("<MOBP>", at);
      if (open == std::string::npos) {
        break;
      }
      std::string_view body;
      std::size_t after = open;
      if (!TagBody(line, open, "<MOBP>", "</MOBP>", &body, &after)) {
        break;
      }
      if (AllDigits(body)) {
        portrait = Number(body);
      }
      at = after;
    }
  }
  return portrait;
}

std::optional<ChatLinkKind> DialogTargetMenuKind(DialogTargetKind kind) {
  switch (kind) {
    case DialogTargetKind::kItem:
      return ChatLinkKind::kItem;
    case DialogTargetKind::kRecipe:
      return ChatLinkKind::kRecipe;
    case DialogTargetKind::kSetting:
      return ChatLinkKind::kSetting;
    case DialogTargetKind::kMonster:
    case DialogTargetKind::kStatus:
    case DialogTargetKind::kNone:
      break;
  }
  return std::nullopt;
}

}  // namespace ro::ui
