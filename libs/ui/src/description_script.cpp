// LA DESCRIPTION D'UN OBJET : le script rAthena, dans un bloc « éditeur ».
//
// Une instruction par ligne, une coloration lexicale — mots-clés, commandes,
// paramètres de bonus, constantes, nombres, chaînes, commentaires —, un repli
// doux, et « Copier ». Le fond est sombre et fixe : un bloc de code se lit
// pareil quel que soit le thème de la fenêtre.

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"

#include "description_odds.h"
#include "ui_scale.h"

namespace ro::ui {
namespace {

// La palette du bloc.
constexpr ImU32 kPlainColor = IM_COL32(212, 212, 212, 255);
constexpr ImU32 kCommentColor = IM_COL32(106, 153, 85, 255);
constexpr ImU32 kStringColor = IM_COL32(206, 145, 120, 255);
constexpr ImU32 kNumberColor = IM_COL32(181, 206, 168, 255);
constexpr ImU32 kControlColor = IM_COL32(86, 156, 214, 255);
constexpr ImU32 kCommandColor = IM_COL32(220, 220, 170, 255);
constexpr ImU32 kParameterColor = IM_COL32(156, 220, 254, 255);
constexpr ImU32 kConstantColor = IM_COL32(78, 201, 176, 255);
constexpr ImU32 kBlockColor = IM_COL32(30, 30, 32, 255);
constexpr float kPadXPixels = 6.0F;
constexpr float kPadYPixels = 5.0F;
constexpr float kLeastWidthPixels = 60.0F;
// Le retrait d'une ligne repliée.
constexpr float kContinuationPixels = 12.0F;
constexpr float kBlockRounding = 4.0F;

// Ce qui ouvre et continue un identifiant de script : les variables portent
// leurs préfixes (`.@`, `$`, `#`, `'`).
[[nodiscard]] bool IdentifierStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '.' || c == '@' ||
         c == '$' || c == '#' || c == '\'';
}
[[nodiscard]] bool IdentifierPart(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '.' || c == '@' ||
         c == '$' || c == '#' || c == '\'';
}

template <std::size_t N>
[[nodiscard]] bool OneOf(std::string_view word, const std::array<std::string_view, N>& words) {
  for (const std::string_view known : words) {
    if (word == known) {
      return true;
    }
  }
  return false;
}

constexpr std::array<std::string_view, 12> kControlWords = {
    "if",   "else",    "for",   "while",    "do",     "switch",
    "case", "default", "break", "continue", "return", "function"};
constexpr std::array<std::string_view, 12> kCommandWords = {
    "set",  "mes",  "next",     "close",   "close2", "end",
    "menu", "goto", "callfunc", "callsub", "warp",   "getarg"};

// Un statement par ligne (découpe sur « ; »), une espace après les virgules,
// les blancs multiples compactés. ⚠ LES CHAÎNES ET LES COMMENTAIRES SE COPIENT
// TELS QUELS : un autobonus embarque tout un sous-script entre guillemets, et
// le couper le rendrait faux.
[[nodiscard]] std::string PrettyPrintScript(std::string_view in) {
  std::string out;
  out.reserve(in.size() + 64);
  bool in_string = false;
  bool in_line_comment = false;
  bool in_block_comment = false;
  bool at_line_start = true;
  const std::size_t n = in.size();
  for (std::size_t i = 0; i < n; ++i) {
    const char c = in[i];
    const char next = i + 1 < n ? in[i + 1] : '\0';
    if (in_line_comment) {
      out.push_back(c);
      if (c == '\n') {
        in_line_comment = false;
        at_line_start = true;
      }
      continue;
    }
    if (in_block_comment) {
      out.push_back(c);
      if (c == '*' && next == '/') {
        out.push_back('/');
        ++i;
        in_block_comment = false;
      }
      continue;
    }
    if (in_string) {
      out.push_back(c);
      if (c == '\\' && next != '\0') {
        out.push_back(next);
        ++i;
      } else if (c == '"') {
        in_string = false;
      }
      continue;
    }
    if (c == '/' && next == '/') {
      in_line_comment = true;
      out += "//";
      ++i;
      at_line_start = false;
      continue;
    }
    if (c == '/' && next == '*') {
      in_block_comment = true;
      out += "/*";
      ++i;
      at_line_start = false;
      continue;
    }
    if (c == '"') {
      in_string = true;
      out.push_back(c);
      at_line_start = false;
      continue;
    }
    if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
      if (!at_line_start && !out.empty() && out.back() != ' ') {
        out.push_back(' ');
      }
      continue;
    }
    if (c == ';') {
      while (!out.empty() && out.back() == ' ') {
        out.pop_back();
      }
      out += ";\n";
      at_line_start = true;
      continue;
    }
    if (c == ',') {
      while (!out.empty() && out.back() == ' ') {
        out.pop_back();
      }
      out += ", ";
      at_line_start = false;
      continue;
    }
    out.push_back(c);
    at_line_start = false;
  }
  while (!out.empty() && (out.back() == '\n' || out.back() == ' ')) {
    out.pop_back();
  }
  return out;
}

// La couleur d'un identifiant : mot-clé, commande (appel, bonus), paramètre de
// bonus (`bStr`), constante (majuscule), ou rien.
[[nodiscard]] ImU32 IdentifierColor(std::string_view code, std::size_t start, std::size_t end) {
  const std::string_view word = code.substr(start, end - start);
  if (OneOf(word, kControlWords)) {
    return kControlColor;
  }
  std::size_t after = end;
  while (after < code.size() && (code[after] == ' ' || code[after] == '\t')) {
    ++after;
  }
  const bool is_call = after < code.size() && code[after] == '(';
  const bool is_bonus = word.starts_with("bonus") || word.starts_with("autobonus");
  if (is_call || is_bonus || OneOf(word, kCommandWords)) {
    return kCommandColor;
  }
  if (word.size() >= 2 && word[0] == 'b' &&
      std::isupper(static_cast<unsigned char>(word[1])) != 0) {
    return kParameterColor;
  }
  if (std::isupper(static_cast<unsigned char>(word[0])) != 0) {
    return kConstantColor;
  }
  return kPlainColor;
}

}  // namespace

void DrawScriptCode(const char* box_id, std::string_view raw, const SkinArt& art) {
  const std::string code = PrettyPrintScript(raw);
  const std::string copy = std::string(core::Tr("Copier")) + "##" + box_id;
  if (SkinSmallButton(copy.c_str(), art)) {
    ImGui::SetClipboardText(code.c_str());
  }
  const float scale = Scale();
  ImDrawList* into = ImGui::GetWindowDrawList();
  ImFont* font = ImGui::GetFont();
  const float size = ImGui::GetFontSize();
  const float line = ImGui::GetTextLineHeightWithSpacing();
  const float pad_x = kPadXPixels * scale;
  const float pad_y = kPadYPixels * scale;
  const float room = std::max(kLeastWidthPixels * scale, ImGui::GetContentRegionAvail().x);
  const float wrap = room - 2.0F * pad_x;
  const ImVec2 top = ImGui::GetCursorScreenPos();
  const float x0 = top.x + pad_x;
  const float y0 = top.y + pad_y;

  // ⚠ LE FOND DERRIÈRE LE TEXTE, mais on ne connaît sa hauteur qu'après : deux
  // canaux, fusionnés à la fin.
  into->ChannelsSplit(2);
  into->ChannelsSetCurrent(1);
  float pen_x = 0.0F;
  float pen_y = 0.0F;
  const auto token = [&](std::size_t start, std::size_t end, ImU32 color) {
    const char* first = code.data() + start;
    const char* last = code.data() + end;
    const ImVec2 measured = font->CalcTextSizeA(size, FLT_MAX, 0.0F, first, last);
    if (pen_x > 0.0F && pen_x + measured.x > wrap) {
      pen_x = kContinuationPixels * scale;
      pen_y += line;
    }
    into->AddText(font, size, ImVec2(x0 + pen_x, y0 + pen_y), color, first, last);
    pen_x += measured.x;
  };
  const std::size_t n = code.size();
  for (std::size_t i = 0; i < n;) {
    const char c = code[i];
    if (c == '\n') {
      pen_x = 0.0F;
      pen_y += line;
      ++i;
      continue;
    }
    std::size_t j = i;
    ImU32 color = kPlainColor;
    if (c == ' ' || c == '\t') {
      while (j < n && (code[j] == ' ' || code[j] == '\t')) {
        ++j;
      }
    } else if (c == '/' && i + 1 < n && code[i + 1] == '/') {
      while (j < n && code[j] != '\n') {
        ++j;
      }
      color = kCommentColor;
    } else if (c == '/' && i + 1 < n && code[i + 1] == '*') {
      j = i + 2;
      while (j + 1 < n && !(code[j] == '*' && code[j + 1] == '/')) {
        ++j;
      }
      j = j + 2 < n ? j + 2 : n;
      color = kCommentColor;
    } else if (c == '"') {
      j = i + 1;
      while (j < n && code[j] != '"') {
        if (code[j] == '\\' && j + 1 < n) {
          ++j;
        }
        ++j;
      }
      if (j < n) {
        ++j;
      }
      color = kStringColor;
    } else if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
      while (j < n && (std::isalnum(static_cast<unsigned char>(code[j])) != 0)) {
        ++j;
      }
      color = kNumberColor;
    } else if (IdentifierStart(c)) {
      while (j < n && IdentifierPart(code[j])) {
        ++j;
      }
      color = IdentifierColor(code, i, j);
    } else {
      j = i + 1;  // ponctuation, opérateur
    }
    token(i, j, color);
    i = j;
  }
  const float height = pen_y + line;
  into->ChannelsSetCurrent(0);
  into->AddRectFilled(top, ImVec2(top.x + room, y0 + height + pad_y), kBlockColor,
                      kBlockRounding * scale);
  into->ChannelsMerge();
  ImGui::Dummy(ImVec2(room, height + 2.0F * pad_y));
}

}  // namespace ro::ui
