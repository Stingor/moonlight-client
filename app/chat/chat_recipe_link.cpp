#include "chat/chat_recipe_link.h"

#include <array>
#include <charconv>
#include <cstdio>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/text.h"

namespace moonclient {
namespace {

constexpr char kFieldSeparator = ':';
// Le libellé composé : un nom d'item et son habillage.
constexpr std::size_t kLabelBytes = 256;

}  // namespace

std::string BuildRecipeLink(const RecipeLink& link) {
  if (link.item_id == 0 || link.name.empty() ||
      link.name.find(kRecipeLinkClose) != std::string::npos) {
    return {};
  }
  std::string tag(kRecipeLinkOpen);
  tag += std::to_string(link.item_id);
  tag.push_back(kFieldSeparator);
  tag += link.name;
  tag += kRecipeLinkClose;
  return tag;
}

bool ParseRecipeLink(std::string_view text, RecipeLink* out, std::size_t* after) {
  if (after != nullptr) {
    *after = 0;
  }
  if (!text.starts_with(kRecipeLinkOpen)) {
    return false;
  }
  const std::size_t body_at = kRecipeLinkOpen.size();
  const std::size_t close = text.find(kRecipeLinkClose, body_at);
  if (after != nullptr) {
    *after = close == std::string_view::npos ? text.size() : close + kRecipeLinkClose.size();
  }
  if (close == std::string_view::npos) {
    return false;
  }
  const std::string_view body = text.substr(body_at, close - body_at);
  const std::size_t separator = body.find(kFieldSeparator);
  if (separator == std::string_view::npos) {
    return false;
  }
  RecipeLink read;
  const std::string_view id_field = body.substr(0, separator);
  const auto [end, trouble] =
      std::from_chars(id_field.data(), id_field.data() + id_field.size(), read.item_id);
  if (trouble != std::errc{} || end != id_field.data() + id_field.size()) {
    return false;
  }
  read.name = std::string(body.substr(separator + 1));
  if (read.item_id == 0 || read.name.empty()) {
    return false;
  }
  if (out != nullptr) {
    *out = std::move(read);
  }
  return true;
}

std::size_t FindRecipeLink(std::string_view text, std::size_t from) {
  return text.find(kRecipeLinkOpen, from);
}

std::string RecipeLinkLabel(std::string_view name) {
  std::array<char, kLabelBytes> label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("[Recette : %s]"),
                std::string(name).c_str());
  // ⚠ UN NOM TROP LONG SE COUPE AU CARACTÈRE, jamais au milieu d'un octet UTF-8.
  return std::string(ro::core::WithoutDanglingUtf8(label.data()));
}

}  // namespace moonclient
