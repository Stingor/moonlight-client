#include "options/signboard_options.h"

#include <algorithm>
#include <cstddef>
#include <span>

#include "ro/core/i18n.h"
#include "ro/ui/system_fonts.h"

#include "options/option_row_kinds.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier. GRAVÉES : en changer une rend à tous les joueurs le
// réglage d'origine, sans que rien ne le dise.
constexpr const char* kChatRoomsShownKey = "signboards_chat_rooms_shown";
constexpr const char* kOpacityKey = "signboards_opacity_percent";
constexpr const char* kChatRoomTintKey = "signboards_chat_room_tint";
constexpr const char* kShopFillKey = "signboards_shop_fill";
constexpr const char* kSpotlitFillKey = "signboards_spotlit_fill";
constexpr const char* kShopEdgeKey = "signboards_shop_edge";
constexpr const char* kTitleColorKey = "signboards_title_color";
constexpr const char* kFontNameKey = "signboards_font_name";
constexpr const char* kFontBoldKey = "signboards_font_bold";
constexpr const char* kFontPixelsKey = "signboards_font_pixels";

// L'OPACITÉ LA PLUS BASSE. ⚠ PAS ZÉRO : une enseigne invisible se clique
// encore, et « masquer » est l'affaire des deux bascules du dessus.
constexpr int kLeastOpacityPercent = 10;
constexpr int kWholePercent = 100;
constexpr int kWholeAlpha = 255;

[[nodiscard]] constexpr int RowId(SignboardRow row) {
  return kSignboardRowBase + static_cast<int>(row);
}

[[nodiscard]] OptionRowView Swatch(SignboardRow row, const char* label, const char* help,
                                   ImU32 color) {
  return OptionRowView{
      .id = RowId(row), .label = label, .help = help, .kind = OptionKind::kColor, .color = color};
}

[[nodiscard]] std::uint8_t ChannelOf(ImU32 color, int shift) {
  return static_cast<std::uint8_t>((color >> shift) & kWholeAlpha);
}

[[nodiscard]] std::array<std::uint8_t, 3> RgbOf(ImU32 color) {
  return {ChannelOf(color, IM_COL32_R_SHIFT), ChannelOf(color, IM_COL32_G_SHIFT),
          ChannelOf(color, IM_COL32_B_SHIFT)};
}

[[nodiscard]] std::array<std::uint8_t, 4> RgbaOf(ImU32 color) {
  return {ChannelOf(color, IM_COL32_R_SHIFT), ChannelOf(color, IM_COL32_G_SHIFT),
          ChannelOf(color, IM_COL32_B_SHIFT), ChannelOf(color, IM_COL32_A_SHIFT)};
}

// Le nom de la famille à ce rang du menu — vide au rang zéro, la police du jeu.
[[nodiscard]] std::string FontNameAt(int chosen) {
  const std::span<const int> installed = ro::ui::InstalledFontFamilies();
  if (chosen <= 0 || static_cast<std::size_t>(chosen) > installed.size()) {
    return {};
  }
  return std::string(
      ro::ui::SystemFontFamilies()[static_cast<std::size_t>(
                                       installed[static_cast<std::size_t>(chosen) - 1])]
          .label);
}

// Et le rang d'un nom, zéro quand la machine ne le porte plus.
[[nodiscard]] int FontRankOf(const std::string& name) {
  if (name.empty()) {
    return 0;
  }
  const std::span<const int> installed = ro::ui::InstalledFontFamilies();
  for (std::size_t rank = 0; rank < installed.size(); ++rank) {
    if (ro::ui::SystemFontFamilies()[static_cast<std::size_t>(installed[rank])].label == name) {
      return static_cast<int>(rank) + 1;
    }
  }
  return 0;
}

}  // namespace

SignboardStyle SignboardOptions::Style(bool shops_shown) const {
  SignboardStyle style;
  style.look.chat_rooms_shown = chat_rooms_shown_;
  style.look.shops_shown = shops_shown;
  style.look.alpha = static_cast<std::uint8_t>(std::clamp(opacity_percent_, 0, kWholePercent) *
                                               kWholeAlpha / kWholePercent);
  style.look.chat_room_tint = RgbOf(chat_room_tint_);
  style.look.shop = ro::render::ShopSignColors{
      .fill = RgbOf(shop_fill_), .spotlit_fill = RgbOf(spotlit_fill_), .edge = RgbOf(shop_edge_)};
  style.look.title_color = RgbaOf(title_color_);
  style.font_name = font_name_;
  style.font_bold = font_bold_;
  style.font_pixels = font_pixels_;
  return style;
}

void SignboardOptions::Declare(SettingsFile& into) {
  into.Declare(kChatRoomsShownKey, &chat_rooms_shown_);
  into.Declare(kOpacityKey, &opacity_percent_);
  into.Declare(kChatRoomTintKey, &chat_room_tint_);
  into.Declare(kShopFillKey, &shop_fill_);
  into.Declare(kSpotlitFillKey, &spotlit_fill_);
  into.Declare(kShopEdgeKey, &shop_edge_);
  into.Declare(kTitleColorKey, &title_color_);
  into.Declare(kFontNameKey, &font_name_);
  into.Declare(kFontBoldKey, &font_bold_);
  into.Declare(kFontPixelsKey, &font_pixels_);
}

void SignboardOptions::ComposeInto(std::vector<OptionRowView>& rows, const ShopOptions& shops) {
  // ⚠ LES FAMILLES EN LITTÉRAL MARQUÉ, comme dans la liste de l'interface : la
  // liste déroulante traduit elle-même ce qu'on lui donne.
  font_names_.clear();
  font_names_.push_back(Tr("Celle du jeu"));
  for (const int family : ro::ui::InstalledFontFamilies()) {
    font_names_.push_back(
        ro::ui::SystemFontFamilies()[static_cast<std::size_t>(family)].label.data());
  }

  // ── LESQUELLES SE MONTRENT ────────────────────────────────────────────────
  const std::size_t first = rows.size();
  shops.ComposeSignboardsRow(rows);
  rows[first].fold = Tr("Enseignes");
  rows[first].heading = Tr("Lesquelles se montrent");
  rows.push_back(OptionRowView{
      .id = RowId(SignboardRow::kChatRoomsShown),
      .label = Tr("Afficher les enseignes des salons de discussion"),
      .help = Tr("Éteint, les bulles des salons ouverts par les joueurs disparaissent — et ne se "
                 "cliquent plus : on n'y entre alors que par l'annuaire des salons."),
      .kind = OptionKind::kToggle,
      .on = chat_rooms_shown_});

  // ── LEUR DESSIN ───────────────────────────────────────────────────────────
  rows.push_back(
      WholeSlider(RowId(SignboardRow::kOpacity), Tr("Opacité"),
                  Tr("Celle de l'enseigne entière, cadre et titre. Le client les peint "
                     "pleines ; plus transparentes, une rue de marchands couvre moins la "
                     "carte."),
                  opacity_percent_, kLeastOpacityPercent, kWholePercent, "%.0f %%"));
  rows.back().heading = Tr("Apparence");
  rows.push_back(Swatch(SignboardRow::kChatRoomTint, Tr("Teinte du cadre des salons"),
                        Tr("Le cadre d'un salon est une image du jeu : la teinte la multiplie. "
                           "Blanche, il reste tel quel."),
                        chat_room_tint_));
  rows.push_back(Swatch(SignboardRow::kShopFill, Tr("Fond des échoppes"), nullptr, shop_fill_));
  rows.push_back(Swatch(SignboardRow::kSpotlitFill, Tr("Fond de l'échoppe recherchée"),
                        Tr("Celle du dernier résultat cliqué dans la recherche d'échoppes."),
                        spotlit_fill_));
  rows.push_back(Swatch(SignboardRow::kShopEdge, Tr("Filet des échoppes"), nullptr, shop_edge_));
  rows.push_back(Swatch(SignboardRow::kTitleColor, Tr("Couleur du titre"), nullptr, title_color_));

  // ── LA POLICE DU TITRE ────────────────────────────────────────────────────
  const bool game_font = font_name_.empty();
  rows.push_back(OptionRowView{
      .id = RowId(SignboardRow::kFontFamily),
      .label = Tr("Police"),
      .help = Tr("Celle du jeu est la police du client d'origine. Le titre garde sa place : "
                 "une police plus large se coupe plus tôt en « ... », et la souris sur le titre "
                 "le montre en entier."),
      .kind = OptionKind::kChoice,
      .chosen = FontRankOf(font_name_),
      .choices = font_names_});
  rows.back().heading = Tr("Police du titre");
  rows.push_back(OptionRowView{.id = RowId(SignboardRow::kFontBold),
                               .label = Tr("Gras"),
                               .kind = OptionKind::kToggle,
                               .on = font_bold_,
                               .indented = true,
                               .disabled = game_font});
  rows.push_back(WholeSlider(
      RowId(SignboardRow::kFontPixels), Tr("Taille"),
      Tr("Le client écrit en douze pixels. Au-delà de seize, le bas des lettres "
         "sortirait de l'enseigne."),
      font_pixels_, kSmallestSignboardFontPixels, kLargestSignboardFontPixels, "%.0f px"));
  rows.push_back(OptionRowView{.id = RowId(SignboardRow::kRestore),
                               .label = Tr("Rendre le dessin du client"),
                               .help = Tr("Opacité, couleurs et police reviennent à ceux du "
                                          "jeu. Ce qui se montre ne change pas."),
                               .kind = OptionKind::kAction});
}

bool SignboardOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kSignboardRowBase ||
      asked.changed >= kSignboardRowBase + static_cast<int>(SignboardRow::kCount)) {
    return false;
  }
  switch (static_cast<SignboardRow>(asked.changed - kSignboardRowBase)) {
    case SignboardRow::kChatRoomsShown:
      chat_rooms_shown_ = asked.on;
      break;
    case SignboardRow::kOpacity:
      opacity_percent_ = Stepped(asked.value, kLeastOpacityPercent, kWholePercent);
      break;
    case SignboardRow::kChatRoomTint:
      chat_room_tint_ = asked.color;
      break;
    case SignboardRow::kShopFill:
      shop_fill_ = asked.color;
      break;
    case SignboardRow::kSpotlitFill:
      spotlit_fill_ = asked.color;
      break;
    case SignboardRow::kShopEdge:
      shop_edge_ = asked.color;
      break;
    case SignboardRow::kTitleColor:
      title_color_ = asked.color;
      break;
    case SignboardRow::kFontFamily:
      font_name_ = FontNameAt(asked.chosen);
      break;
    case SignboardRow::kFontBold:
      font_bold_ = asked.on;
      break;
    case SignboardRow::kFontPixels:
      font_pixels_ =
          Stepped(asked.value, kSmallestSignboardFontPixels, kLargestSignboardFontPixels);
      break;
    case SignboardRow::kRestore: {
      // ⚠ CHAMP PAR CHAMP, ET NON `*this = {}` : la liste des polices est
      // encore tenue par la rangée qui vient d'être dessinée.
      const SignboardOptions origin;
      opacity_percent_ = origin.opacity_percent_;
      chat_room_tint_ = origin.chat_room_tint_;
      shop_fill_ = origin.shop_fill_;
      spotlit_fill_ = origin.spotlit_fill_;
      shop_edge_ = origin.shop_edge_;
      title_color_ = origin.title_color_;
      font_name_ = origin.font_name_;
      font_bold_ = origin.font_bold_;
      font_pixels_ = origin.font_pixels_;
      break;
    }
    case SignboardRow::kCount:
      break;
  }
  return true;
}

}  // namespace moonclient
