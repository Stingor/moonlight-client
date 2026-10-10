#include "ro/ui/rich_text.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <utility>

#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_tooltip.h"

namespace ro::ui {
namespace {

// Les deux jetons de balise, et leur longueur.
constexpr std::string_view kColorToken = "^";
// ⚠ SEPT OCTETS : l'accent circonflexe et six chiffres hexadécimaux.
constexpr std::size_t kColorTokenLength = 7;

constexpr std::string_view kOpenAddress = "<URL>";
constexpr std::string_view kCloseAddress = "</URL>";
constexpr std::string_view kOpenPlace = "<NAVI>";
constexpr std::string_view kClosePlace = "</NAVI>";
constexpr std::string_view kOpenPayload = "<INFO>";
constexpr std::string_view kClosePayload = "</INFO>";

[[nodiscard]] bool StartsWith(std::string_view text, std::size_t at, std::string_view tag) {
  return text.compare(at, tag.size(), tag) == 0;
}

// Le token de couleur commence-t-il ici ? ⚠ IL FAUT LES SIX CHIFFRES : un
// accent circonflexe suivi d'autre chose est du texte, et s'affiche.
[[nodiscard]] bool ColorTokenAt(std::string_view line, std::size_t at) {
  if (line[at] != kColorToken[0] || at + kColorTokenLength > line.size()) {
    return false;
  }
  return HexByte(line.substr(at + 1, 2)) >= 0 && HexByte(line.substr(at + 3, 2)) >= 0 &&
         HexByte(line.substr(at + 5, 2)) >= 0;
}

// Combien d'octets pour ce caractère UTF-8. ⚠ UN OCTET DE CONTINUATION OU UN
// OCTET INVALIDE COMPTE POUR UN : une source abîmée ne doit pas faire sortir la
// boucle de la chaîne.
[[nodiscard]] int Utf8Length(unsigned char lead) {
  if ((lead & 0x80U) == 0) {
    return 1;
  }
  if ((lead & 0xE0U) == 0xC0U) {
    return 2;
  }
  if ((lead & 0xF0U) == 0xE0U) {
    return 3;
  }
  if ((lead & 0xF8U) == 0xF0U) {
    return 4;
  }
  return 1;
}

// LA TEINTE D'UNE SÉLECTION : un bleu translucide, POSÉ SOUS LE TEXTE. Opaque,
// il effacerait ce qu'on vient de choisir.
constexpr ImU32 kSelectionVeil = IM_COL32(60, 110, 220, 90);

// Les clés du rangement d'ImGui. ⚠ DÉRIVÉES DE L'IDENTIFIANT DU BLOC : deux
// descriptions ouvertes côte à côte ne doivent pas se voler leur sélection.
[[nodiscard]] ImGuiID AnchorKey(ImGuiID block) { return block ^ ImHashStr("ancre_riche"); }
[[nodiscard]] ImGuiID HeadKey(ImGuiID block) { return block ^ ImHashStr("tete_riche"); }
// Quel bloc de CETTE fenêtre porte la sélection vivante. ⚠ UNE CLÉ FIXE, PAS
// DÉRIVÉE : c'est une question sur la fenêtre, pas sur un bloc.
[[nodiscard]] ImGuiID LiveBlockKey() { return ImHashStr("bloc_riche_actif"); }

// La couleur des deux sortes de lien, celles du client : le bleu d'une
// adresse, le vert d'un lieu.
constexpr ImU32 kAddressColor = IM_COL32(64, 132, 224, 255);
constexpr ImU32 kPlaceColor = IM_COL32(60, 175, 90, 255);

}  // namespace

int HexByte(std::string_view pair) {
  if (pair.size() < 2) {
    return -1;
  }
  const auto digit = [](char letter) -> int {
    if (letter >= '0' && letter <= '9') {
      return letter - '0';
    }
    if (letter >= 'a' && letter <= 'f') {
      return letter - 'a' + 10;
    }
    if (letter >= 'A' && letter <= 'F') {
      return letter - 'A' + 10;
    }
    return -1;
  };
  const int high = digit(pair[0]);
  const int low = digit(pair[1]);
  return (high < 0 || low < 0) ? -1 : (high << 4 | low);
}

int VisibleLength(std::string_view line) {
  int shown = 0;
  for (std::size_t at = 0; at < line.size();) {
    if (ColorTokenAt(line, at)) {
      at += kColorTokenLength;
      continue;
    }
    ++shown;
    ++at;
  }
  return shown;
}

RichText LayOutRichText(std::span<const std::string> lines, ImU32 default_color, float wrap_width,
                        float line_height, RichMeasure measure, void* owner) {
  RichText laid;
  if (measure == nullptr) {
    return laid;
  }
  const float wrap = wrap_width > 1.0F ? wrap_width : 1.0F;
  const float space_width = measure(" ", owner);

  float pen_x = 0.0F;
  float pen_y = 0.0F;
  int row = 0;
  // Le lien en cours, ou moins un.
  int link = -1;
  // ⚠ LA COULEUR VIT PLUS LONGTEMPS QUE LA LIGNE : voir l'en-tête. Elle se
  // déclare donc ICI, hors de la boucle des lignes.
  ImU32 color = default_color;
  // On lit la charge d'un lien : elle ne s'affiche pas.
  bool inside_payload = false;
  std::string word;
  // 🔴 UNE ESPACE EN ATTENTE, ET UN SEUL. Un mot se vide aussi bien sur une vraie
  // espace que sur un token rencontré EN PLEIN MOT — « Knight^777777s ». Sans ce
  // drapeau, chaque token intercalé injecterait une espace parasite :
  // « Knight s ».
  // Il ne se lève qu'à la consommation d'une espace de la SOURCE.
  bool pending_space = false;

  const auto put_glyph = [&](std::string_view piece, float width, ImU32 shown) {
    laid.boxes.push_back(RichBox{.left = pen_x,
                                 .right = pen_x + width,
                                 .top = pen_y,
                                 .row = row,
                                 .offset = static_cast<int>(laid.text.size()),
                                 .length = static_cast<int>(piece.size()),
                                 .color = shown,
                                 .link = link});
    laid.text.append(piece);
    pen_x += width;
  };

  const auto flush_word = [&]() {
    if (word.empty()) {
      // ⚠ ON SORT SANS CONSOMMER L'ESPACE EN ATTENTE : deux tokens de couleur à
      // la suite ne doivent pas l'avaler avant le mot qui la mérite.
      return;
    }
    ImU32 shown = color;
    if (link >= 0) {
      shown = laid.links[static_cast<std::size_t>(link)].kind == RichLinkKind::kPlace
                  ? kPlaceColor
                  : kAddressColor;
    }
    const float word_width = measure(word, owner);
    if (pen_x > 0.0F) {
      // ⚠ LE MOT SE REPLIE MÊME SANS SÉPARATEUR EN ATTENTE : c'est le cas de la
      // suite d'un mot coupé par un token de couleur, qui déborde à lui seul.
      const float separator = pending_space ? space_width : 0.0F;
      if (pen_x + separator + word_width > wrap) {
        // REPLI DOUX : l'espace devient un saut de ligne dans le texte plat, et
        // la copie retrouve deux mots séparés plutôt que collés.
        if (pending_space) {
          laid.text.push_back(' ');
        }
        pen_x = 0.0F;
        pen_y += line_height;
        ++row;
      } else if (pending_space) {
        put_glyph(" ", space_width, shown);
      }
    }
    pending_space = false;
    for (std::size_t at = 0; at < word.size();) {
      std::size_t length =
          static_cast<std::size_t>(Utf8Length(static_cast<unsigned char>(word[at])));
      if (at + length > word.size()) {
        length = word.size() - at;
      }
      const std::string_view piece(word.data() + at, length);
      const float glyph_width = measure(piece, owner);
      // COUPE DURE : un mot plus large que la fenêtre se coupe au caractère,
      // comme dans le journal de conversation. Il ne se produit qu'ici, le
      // repli doux ci-dessus ayant déjà renvoyé tout mot qui tient sur une ligne.
      if (pen_x > 0.0F && pen_x + glyph_width > wrap) {
        pen_x = 0.0F;
        pen_y += line_height;
        ++row;
      }
      put_glyph(piece, glyph_width, shown);
      at += length;
    }
    word.clear();
  };

  for (const std::string& line : lines) {
    for (std::size_t at = 0; at < line.size();) {
      if (StartsWith(line, at, kOpenAddress) || StartsWith(line, at, kOpenPlace)) {
        const bool place = StartsWith(line, at, kOpenPlace);
        flush_word();
        link = static_cast<int>(laid.links.size());
        laid.links.push_back(
            RichLink{.kind = place ? RichLinkKind::kPlace : RichLinkKind::kAddress});
        at += place ? kOpenPlace.size() : kOpenAddress.size();
        continue;
      }
      if (StartsWith(line, at, kCloseAddress) || StartsWith(line, at, kClosePlace)) {
        flush_word();
        link = -1;
        at += StartsWith(line, at, kClosePlace) ? kClosePlace.size() : kCloseAddress.size();
        continue;
      }
      if (StartsWith(line, at, kOpenPayload)) {
        flush_word();
        inside_payload = true;
        at += kOpenPayload.size();
        continue;
      }
      if (StartsWith(line, at, kClosePayload)) {
        inside_payload = false;
        at += kClosePayload.size();
        continue;
      }
      if (inside_payload) {
        if (link >= 0) {
          laid.links[static_cast<std::size_t>(link)].payload.push_back(line[at]);
        }
        ++at;
        continue;
      }
      if (ColorTokenAt(line, at)) {
        flush_word();
        const int red = HexByte(line.substr(at + 1, 2));
        const int green = HexByte(line.substr(at + 3, 2));
        const int blue = HexByte(line.substr(at + 5, 2));
        // 🔴 `^000000` EST UN RETOUR AU DÉFAUT, PAS DU NOIR. La fenêtre de
        // description a un fond clair et du texte sombre ; un livre au fond
        // sombre a du texte clair. Prendre le noir à la lettre rendrait le
        // second illisible, et c'est le client qui tranche ainsi.
        color =
            (red == 0 && green == 0 && blue == 0) ? default_color : IM_COL32(red, green, blue, 255);
        at += kColorTokenLength;
        continue;
      }
      if (line[at] == ' ') {
        flush_word();
        pending_space = true;
        ++at;
        continue;
      }
      word.push_back(line[at]);
      ++at;
    }
    flush_word();
    // ⚠ UN LIEN NE TRAVERSE PAS UNE FIN DE LIGNE, la couleur si : le premier est
    // une balise que la source ferme, la seconde un flux qu'elle laisse ouvert.
    link = -1;
    inside_payload = false;
    laid.text.push_back('\n');
    pen_x = 0.0F;
    pen_y += line_height;
    ++row;
    // Le saut de ligne fait office de séparateur.
    pending_space = false;
  }

  laid.height = pen_y > line_height ? pen_y : line_height;
  laid.width = wrap;
  return laid;
}

RichText LayOutRichTextHere(std::span<const std::string> lines, ImU32 default_color,
                            float wrap_width) {
  const auto measure = [](std::string_view piece, void* /*owner*/) {
    return ImGui::CalcTextSize(piece.data(), piece.data() + piece.size()).x;
  };
  return LayOutRichText(lines, default_color, wrap_width, ImGui::GetTextLineHeightWithSpacing(),
                        measure, nullptr);
}

void KeepReadable(RichText& laid, ImU32 paper) {
  // ⚠ UN BLOC SE DISPOSE À CHAQUE IMAGE, et il n'a que quelques couleurs : chacune
  // ne se juge qu'une fois.
  std::vector<std::pair<ImU32, ImU32>> judged;
  for (RichBox& box : laid.boxes) {
    const auto known = std::ranges::find(judged, box.color, &std::pair<ImU32, ImU32>::first);
    if (known != judged.end()) {
      box.color = known->second;
      continue;
    }
    const ImU32 readable = ReadableOn(box.color, paper);
    judged.emplace_back(box.color, readable);
    box.color = readable;
  }
}

RichTextAction DrawRichText(const char* id, const RichText& laid, float line_height,
                            RichTextSelect select) {
  RichTextAction acted;
  ImDrawList* const into = ImGui::GetWindowDrawList();
  ImFont* const font = ImGui::GetFont();
  const float font_size = ImGui::GetFontSize();
  const ImVec2 corner = ImGui::GetCursorScreenPos();

  // OÙ LA SOURIS TOMBE DANS LE TEXTE PLAT.
  //
  // ⚠ LA MOITIÉ D'UN GLYPHE DÉCIDE DU CÔTÉ : viser la première moitié place le
  // curseur AVANT lui, la seconde APRÈS. Sans cela, on ne peut jamais choisir le
  // dernier caractère d'un mot.
  const auto hit = [&](ImVec2 mouse) -> int {
    if (laid.boxes.empty()) {
      return 0;
    }
    const int wanted = std::max(0, static_cast<int>((mouse.y - corner.y) / line_height));
    int found = static_cast<int>(laid.text.size());
    bool row_seen = false;
    for (const RichBox& box : laid.boxes) {
      if (box.row != wanted) {
        continue;
      }
      row_seen = true;
      const float left = corner.x + box.left;
      const float right = corner.x + box.right;
      if (mouse.x < left) {
        found = box.offset;
        break;
      }
      if (mouse.x <= right) {
        found = mouse.x < (left + right) * 0.5F ? box.offset : box.offset + box.length;
        break;
      }
      // Passé ce glyphe : on continue, et l'on s'arrêtera en fin de rangée.
      found = box.offset + box.length;
    }
    if (!row_seen) {
      // ⚠ UNE RANGÉE VIDE N'EST PAS UN TROU : on se cale sur le premier glyphe
      // d'une rangée plus bas, sinon cliquer entre deux paragraphes ne
      // sélectionne rien et le glisser paraît cassé.
      for (const RichBox& box : laid.boxes) {
        if (box.row >= wanted) {
          found = box.offset;
          break;
        }
      }
    }
    return found;
  };

  const auto link_under = [&](ImVec2 mouse) -> int {
    const int wanted = static_cast<int>((mouse.y - corner.y) / line_height);
    if (wanted < 0) {
      return -1;
    }
    for (const RichBox& box : laid.boxes) {
      if (box.row != wanted || box.link < 0) {
        continue;
      }
      if (mouse.x >= corner.x + box.left && mouse.x <= corner.x + box.right) {
        return box.link;
      }
    }
    return -1;
  };

  // LA ZONE QUI CAPTE LA SOURIS, sur tout le bloc.
  //
  // ⚠ UN SEUL ÉLÉMENT POUR TOUT LE TEXTE : un par glyphe donnerait des milliers
  // d'identifiants, et le glisser d'une sélection passerait de l'un à l'autre en
  // se coupant.
  //
  // 🔴 SA LARGEUR VIENT DE LA DISPOSITION, jamais de la place restante. Dans une
  // infobulle — qui se dimensionne sur son contenu — la place restante vaut
  // presque zéro : la zone se réduisait à un trait, la fenêtre naissait
  // minuscule, et plus rien ne se sélectionnait.
  //
  // ⚠ SÉLECTION SOUS MODIFICATEUR : le bloc ne prend la souris que modificateur
  // tenu, sur un lien, ou pendant un glisser déjà commencé. Le reste du temps il
  // n'est qu'une place réservée, et le clic va à la fenêtre, qui se déplace.
  const ImGuiID block = ImGui::GetID(id);
  const ImGuiIO& mouse = ImGui::GetIO();
  const float wide = laid.width > 1.0F ? laid.width : 1.0F;
  const bool captures = select == RichTextSelect::kFree || mouse.KeyShift || mouse.KeyCtrl ||
                        ImGui::GetActiveID() == block || link_under(mouse.MousePos) >= 0;
  bool hovered = false;
  bool held = false;
  if (captures) {
    ImGui::InvisibleButton(id, ImVec2(wide, laid.height), ImGuiButtonFlags_MouseButtonLeft);
    hovered = ImGui::IsItemHovered();
    held = ImGui::IsItemActive();
  } else {
    ImGui::Dummy(ImVec2(wide, laid.height));
    hovered = ImGui::IsItemHovered();
  }
  ImGuiStorage* const store = ImGui::GetStateStorage();

  const auto length = static_cast<int>(laid.text.size());
  int anchor = std::clamp(store->GetInt(AnchorKey(block), -1), -1, length);
  int head = std::clamp(store->GetInt(HeadKey(block), -1), -1, length);
  // Un clic sans modificateur sur le texte efface la sélection qu'il portait.
  if (!captures && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    anchor = -1;
    head = -1;
  }
  if (held) {
    const int at = hit(mouse.MousePos);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      anchor = at;
    }
    head = at;
    store->SetInt(LiveBlockKey(), static_cast<int>(block));
  }

  // TOUT CHOISIR, ET COPIER. ⚠ SUR LE BLOC VIVANT, pas seulement sur le survolé :
  // on choisit à la souris, puis on lâche et l'on tape — la souris a souvent
  // quitté le texte entre les deux.
  const bool live = held || store->GetInt(LiveBlockKey(), 0) == static_cast<int>(block);
  if (live && mouse.KeyCtrl) {
    if (ImGui::IsKeyPressed(ImGuiKey_A, false)) {
      anchor = 0;
      head = length;
    }
    const int from = std::min(anchor, head);
    const int to = std::max(anchor, head);
    if (ImGui::IsKeyPressed(ImGuiKey_C, false) && to > from && from >= 0) {
      ImGui::SetClipboardText(
          laid.text.substr(static_cast<std::size_t>(from), static_cast<std::size_t>(to - from))
              .c_str());
    }
  }
  store->SetInt(AnchorKey(block), anchor);
  store->SetInt(HeadKey(block), head);

  const int from = (anchor < 0 || head < 0) ? 0 : std::min(anchor, head);
  const int to = (anchor < 0 || head < 0) ? 0 : std::max(anchor, head);

  // LE LIEN SOUS LA SOURIS. ⚠ UN CLIC, PAS UN GLISSER : relâcher après avoir
  // traîné une sélection à travers un lien ne doit pas le suivre.
  if (hovered) {
    if (const int under = link_under(mouse.MousePos); under >= 0) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      // UN LIEU DIT OÙ IL MÈNE AU SURVOL — carte et
      // coordonnées, que le texte du lien ne montre pas.
      const RichLink& link = laid.links[static_cast<std::size_t>(under)];
      if (link.kind == RichLinkKind::kPlace) {
        if (const std::optional<RichPlace> place = ReadRichPlace(link.payload); place.has_value()) {
          SkinTooltipFormat(core::Tr("Destination : %s (%d, %d)"), place->map.c_str(), place->x,
                            place->y);
        }
      }
      if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && anchor == head) {
        acted.followed = under;
      }
    }
  }

  // LA SÉLECTION SOUS LE TEXTE, puis le texte, glyphe par glyphe.
  if (to > from) {
    for (const RichBox& box : laid.boxes) {
      if (box.offset >= from && box.offset < to) {
        into->AddRectFilled(ImVec2(corner.x + box.left, corner.y + box.top),
                            ImVec2(corner.x + box.right, corner.y + box.top + line_height),
                            kSelectionVeil);
      }
    }
  }
  for (const RichBox& box : laid.boxes) {
    const char* piece = laid.text.data() + box.offset;
    into->AddText(font, font_size, ImVec2(corner.x + box.left, corner.y + box.top), box.color,
                  piece, piece + box.length);
    if (box.link >= 0) {
      // Souligné : c'est ce qui distingue un lien d'un mot coloré.
      into->AddLine(ImVec2(corner.x + box.left, corner.y + box.top + font_size),
                    ImVec2(corner.x + box.right, corner.y + box.top + font_size), box.color);
    }
  }
  return acted;
}

std::optional<RichPlace> ReadRichPlace(std::string_view payload) {
  constexpr char kSeparator = ',';
  const std::size_t first = payload.find(kSeparator);
  if (first == 0 || first == std::string_view::npos) {
    return std::nullopt;
  }
  RichPlace place{.map = std::string(payload.substr(0, first))};
  const char* at = payload.data() + first + 1;
  const char* const end = payload.data() + payload.size();
  auto [after_x, x_error] = std::from_chars(at, end, place.x);
  if (x_error != std::errc() || after_x == end || *after_x != kSeparator) {
    return std::nullopt;
  }
  if (std::from_chars(after_x + 1, end, place.y).ec != std::errc()) {
    return std::nullopt;
  }
  return place;
}

}  // namespace ro::ui
