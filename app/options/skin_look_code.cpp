// Le code d'un thème. Voir l'en-tête.

#include "options/skin_look_code.h"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <system_error>
#include <vector>

#include <imgui.h>

#include "options/skin_look_colors.h"
#include "options/skin_look_options.h"

namespace moonclient {
namespace {

using ro::ui::SkinLook;
using skin_look::ColorRow;
using skin_look::kColorRows;
using skin_look::kCurrentVersion;
using skin_look::kOriginalVersion;

constexpr std::string_view kPrefix = "moonlook";
constexpr char kVersionEnd = ':';
constexpr std::string_view kBlanks = " \t\r\n";
constexpr int kHexBase = 16;
// Les chiffres du thème ; ceux d'un réel, ses bits mêmes, pour qu'un thème
// collé tel quel se reconnaisse encore ; et ceux d'une couleur.
constexpr std::size_t kThemeDigits = 2;
constexpr std::size_t kWordDigits = 8;
constexpr std::size_t kHeaderDigits = kThemeDigits + 2 * kWordDigits;
// Les canaux dans RRGGBBAA.
constexpr unsigned kChannelMask = 0xFFU;
constexpr unsigned kRedShift = 24U;
constexpr unsigned kGreenShift = 16U;
constexpr unsigned kBlueShift = 8U;

// LES COULEURS QU'UN CODE DE CETTE VERSION PORTE, dans leur ordre : par
// version, puis par clé.
[[nodiscard]] std::vector<const ColorRow*> CodeOrder(int version) {
  std::vector<const ColorRow*> order;
  for (const ColorRow& color : kColorRows) {
    if (color.since <= version) {
      order.push_back(&color);
    }
  }
  std::ranges::sort(order, [](const ColorRow* left, const ColorRow* right) {
    if (left->since != right->since) {
      return left->since < right->since;
    }
    return std::string_view(left->key) < std::string_view(right->key);
  });
  return order;
}

void AppendHex(std::string& into, std::uint32_t value, std::size_t digits) {
  std::array<char, kWordDigits> text{};
  const auto written = static_cast<std::size_t>(
      std::to_chars(text.data(), text.data() + text.size(), value, kHexBase).ptr - text.data());
  into.append(digits - std::min(digits, written), '0');
  into.append(text.data(), written);
}

[[nodiscard]] std::optional<std::uint32_t> ReadHex(std::string_view digits) {
  std::uint32_t value = 0;
  const auto [end, error] =
      std::from_chars(digits.data(), digits.data() + digits.size(), value, kHexBase);
  if (error != std::errc{} || end != digits.data() + digits.size()) {
    return std::nullopt;
  }
  return value;
}

// Une couleur d'ImGui en RRGGBBAA, et retour.
[[nodiscard]] std::uint32_t Rgba(ImU32 color) {
  const auto channel = [color](unsigned shift) { return (color >> shift) & kChannelMask; };
  return (channel(IM_COL32_R_SHIFT) << kRedShift) | (channel(IM_COL32_G_SHIFT) << kGreenShift) |
         (channel(IM_COL32_B_SHIFT) << kBlueShift) | channel(IM_COL32_A_SHIFT);
}
[[nodiscard]] ImU32 FromRgba(std::uint32_t rgba) {
  return IM_COL32((rgba >> kRedShift) & kChannelMask, (rgba >> kGreenShift) & kChannelMask,
                  (rgba >> kBlueShift) & kChannelMask, rgba & kChannelMask);
}

// Un réel de [0, 1] lu dans ses bits, ou rien.
[[nodiscard]] std::optional<float> ReadShare(std::string_view digits) {
  const std::optional<std::uint32_t> bits = ReadHex(digits);
  if (!bits.has_value()) {
    return std::nullopt;
  }
  const auto share = std::bit_cast<float>(*bits);
  if (!std::isfinite(share) || share < 0.0F || share > 1.0F) {
    return std::nullopt;
  }
  return share;
}

}  // namespace

std::string SkinLookCode(const SkinLook& look, int theme_rank) {
  std::string code(kPrefix);
  code += std::to_string(kCurrentVersion);
  code += kVersionEnd;
  AppendHex(code, static_cast<std::uint32_t>(std::max(theme_rank, 0)), kThemeDigits);
  AppendHex(code, std::bit_cast<std::uint32_t>(look.art_brightness), kWordDigits);
  AppendHex(code, std::bit_cast<std::uint32_t>(look.opacity), kWordDigits);
  for (const ColorRow* color : CodeOrder(kCurrentVersion)) {
    AppendHex(code, Rgba(look.*color->member), kWordDigits);
  }
  return code;
}

std::optional<SharedSkinLook> ReadSkinLookCode(std::string_view code) {
  const std::size_t first = code.find_first_not_of(kBlanks);
  if (first == std::string_view::npos) {
    return std::nullopt;
  }
  code = code.substr(first, code.find_last_not_of(kBlanks) - first + 1);
  if (!code.starts_with(kPrefix)) {
    return std::nullopt;
  }
  code.remove_prefix(kPrefix.size());
  const std::size_t version_end = code.find(kVersionEnd);
  int version = 0;
  if (version_end == std::string_view::npos ||
      std::from_chars(code.data(), code.data() + version_end, version).ptr !=
          code.data() + version_end ||
      version < kOriginalVersion || version > kCurrentVersion) {
    return std::nullopt;
  }
  code.remove_prefix(version_end + 1);
  const std::vector<const ColorRow*> order = CodeOrder(version);
  if (code.size() != kHeaderDigits + order.size() * kWordDigits) {
    return std::nullopt;
  }
  const std::optional<std::uint32_t> theme = ReadHex(code.substr(0, kThemeDigits));
  const std::optional<float> brightness = ReadShare(code.substr(kThemeDigits, kWordDigits));
  const std::optional<float> opacity =
      ReadShare(code.substr(kThemeDigits + kWordDigits, kWordDigits));
  if (!theme.has_value() || *theme >= SkinLookPresets().size() || !brightness.has_value() ||
      !opacity.has_value()) {
    return std::nullopt;
  }
  // ⚠ DEPUIS LE THÈME : ce qu'un code plus ancien ne porte pas en vient.
  SharedSkinLook shared{.look = SkinLookPresets()[*theme], .theme_rank = static_cast<int>(*theme)};
  shared.look.rounding = SkinLook{}.rounding;
  shared.look.art_brightness = *brightness;
  shared.look.opacity = *opacity;
  std::size_t at = kHeaderDigits;
  for (const ColorRow* color : order) {
    const std::optional<std::uint32_t> rgba = ReadHex(code.substr(at, kWordDigits));
    if (!rgba.has_value()) {
      return std::nullopt;
    }
    shared.look.*color->member = FromRgba(*rgba);
    at += kWordDigits;
  }
  return shared;
}

}  // namespace moonclient
