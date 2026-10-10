#include "options/cast_bar_options.h"

#include <cmath>
#include <string>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier : `cast_<espèce>_<réglage>`. ⚠ DES MOTS ET NON DES
// NUMÉROS, comme les barres de vie : l'ordre de l'énumération changerait le
// sens d'un numéro sans que le fichier le dise.
constexpr const char* kReplacedKey = "cast_bar_replaced";
constexpr std::array<const char*, ro::ui::kEntityBarKindCount> kKindNames = {
    "self", "party", "players", "monsters", "boss", "others",
};

[[nodiscard]] std::string KindKey(std::size_t kind, const char* what) {
  return std::string("cast_") + kKindNames[kind] + "_" + what;
}

// Les formats des curseurs : des pixels entiers, puis des parts au centième.
constexpr const char* kPixelsFormat = "%.0f px";
constexpr const char* kShareFormat = "%.2f";

}  // namespace

ro::ui::CastBarsLook CastBarOptions::look() const noexcept {
  ro::ui::CastBarsLook shown = look_;
  for (ro::ui::CastBarLook& kind : shown.kinds) {
    kind.name = ro::ui::CastBarNameOf(static_cast<int>(kind.name));
  }
  return shown;
}

void CastBarOptions::Declare(SettingsFile& into) {
  into.Declare(kReplacedKey, &look_.replaced);
  for (std::size_t kind = 0; kind < look_.kinds.size(); ++kind) {
    ro::ui::CastBarLook& look = look_.kinds[kind];
    into.Declare(KindKey(kind, "shown"), &look.shown);
    // ⚠ UN RANG D'ÉNUMÉRATION SE DÉCLARE EN `int*`, et se relit borné : voir
    // `look()`.
    into.Declare(KindKey(kind, "name"), reinterpret_cast<int*>(&look.name));
    into.Declare(KindKey(kind, "time_remaining"), &look.time_left);
    into.Declare(KindKey(kind, "border"), &look.border);
    into.Declare(KindKey(kind, "width"), &look.width_pixels);
    into.Declare(KindKey(kind, "height"), &look.height_pixels);
    into.Declare(KindKey(kind, "offset"), &look.offset_pixels);
    into.Declare(KindKey(kind, "rounding"), &look.rounding);
    into.Declare(KindKey(kind, "text"), &look.font_scale);
    into.Declare(KindKey(kind, "opacity"), &look.opacity);
    into.Declare(KindKey(kind, "background"), &look.background);
    into.Declare(KindKey(kind, "fill"), &look.fill);
  }
}

void CastBarOptions::Compose(std::optional<ro::ui::EntityBarKind> shown_kind) {
  shared_rows_.clear();
  shared_rows_.push_back(OptionRowView{
      .id = kCastBarReplacedRow,
      .label = Tr("Remplacer les barres de cast"),
      .help = Tr("La barre du client fait 60×6 pixels, sans nom de skill ni durée. Celle-ci la "
                 "remplace, sur la même horloge, et se règle espèce par espèce.\n\n"
                 "Éteint, c'est celle du client qui se peint : seule la case « Montrer » de "
                 "chaque espèce compte encore."),
      .kind = OptionKind::kToggle,
      .on = look_.replaced});

  rows_.clear();
  if (!shown_kind.has_value()) {
    return;
  }
  const auto kind = static_cast<std::size_t>(*shown_kind);
  const ro::ui::CastBarLook& look = look_.kinds[kind];
  const bool off = !look.shown;
  // CE QUI DESSINE LA BARRE QUI REMPLACE : grisé quand elle est masquée, ou
  // que c'est celle du client qui se peint.
  const bool drawn_off = off || !look_.replaced;
  const auto id = [kind](CastBarField field) { return CastBarRowId(kind, field); };
  const auto slider = [&](CastBarField field, const char* label, float value, float lowest,
                          float highest, const char* format) {
    rows_.push_back(OptionRowView{.id = id(field),
                                  .label = label,
                                  .kind = OptionKind::kSlider,
                                  .value = value,
                                  .lowest = lowest,
                                  .highest = highest,
                                  .format = format,
                                  .indented = true,
                                  .disabled = drawn_off});
  };
  const auto swatch = [&](CastBarField field, const char* label, ImU32 color) {
    rows_.push_back(OptionRowView{.id = id(field),
                                  .label = label,
                                  .kind = OptionKind::kColor,
                                  .color = color,
                                  .indented = true,
                                  .disabled = drawn_off});
  };

  rows_.push_back(OptionRowView{
      .id = id(CastBarField::kShown),
      .label = Tr("Montrer pour cette espèce"),
      .help = *shown_kind == ro::ui::EntityBarKind::kOwn
                  ? Tr("Masquée, la tienne disparaît d'au-dessus de ta tête, celle du client "
                       "comprise — pour qui préfère la suivre dans son interface. La barre "
                       "« Cast » se règle avec les autres jauges, dans la section « Jauges ».")
                  : nullptr,
      .kind = OptionKind::kToggle,
      .on = look.shown});

  // ⚠ UN LITTÉRAL PAR ENTRÉE : le relevé des traductions ne lit que les
  // littéraux passés à `Tr`.
  name_choices_[static_cast<std::size_t>(ro::ui::CastBarName::kNever)] = Tr("Jamais");
  name_choices_[static_cast<std::size_t>(ro::ui::CastBarName::kIfFree)] = Tr("Si pas déjà annoncé");
  name_choices_[static_cast<std::size_t>(ro::ui::CastBarName::kAlways)] = Tr("Toujours");
  rows_.push_back(OptionRowView{
      .id = id(CastBarField::kName),
      .label = Tr("Nom du skill"),
      .help = Tr("Le client annonce déjà le sort dans une bulle au-dessus de la tête — pour les "
                 "joueurs, pas pour les monstres. « Si pas déjà annoncé » ne l'écrit donc que là "
                 "où l'information manque.\n"
                 "Le nom vient du paquet d'incantation, que le client jette sans le transmettre "
                 "à sa barre."),
      .kind = OptionKind::kChoice,
      .chosen = static_cast<int>(ro::ui::CastBarNameOf(static_cast<int>(look.name))),
      .choices = name_choices_,
      .indented = true,
      .disabled = drawn_off});
  rows_.push_back(OptionRowView{.id = id(CastBarField::kTimeLeft),
                                .label = Tr("Temps restant"),
                                .help = Tr("Sous une bulle, l'étiquette passe à droite de la "
                                           "barre : la bulle occupe exactement la place "
                                           "au-dessus."),
                                .kind = OptionKind::kToggle,
                                .on = look.time_left,
                                .indented = true,
                                .disabled = drawn_off});
  rows_.push_back(OptionRowView{.id = id(CastBarField::kBorder),
                                .label = Tr("Bordure"),
                                .kind = OptionKind::kToggle,
                                .on = look.border,
                                .indented = true,
                                .disabled = drawn_off});

  slider(CastBarField::kWidth, Tr("Largeur"), look.width_pixels, ro::ui::kSmallestCastBarWidth,
         ro::ui::kLargestCastBarWidth, kPixelsFormat);
  rows_.back().heading = Tr("Place et taille");
  slider(CastBarField::kHeight, Tr("Hauteur"), look.height_pixels, ro::ui::kSmallestCastBarHeight,
         ro::ui::kLargestCastBarHeight, kPixelsFormat);
  slider(CastBarField::kOffset, Tr("Décalage vertical"), look.offset_pixels,
         -ro::ui::kLargestCastBarOffset, ro::ui::kLargestCastBarOffset, kPixelsFormat);
  slider(CastBarField::kRounding, Tr("Arrondi"), look.rounding, 0.0F,
         ro::ui::kLargestCastBarRounding, kShareFormat);
  slider(CastBarField::kFontScale, Tr("Taille du texte"), look.font_scale,
         ro::ui::kSmallestCastBarFontScale, ro::ui::kLargestCastBarFontScale, kShareFormat);
  slider(CastBarField::kOpacity, Tr("Opacité"), look.opacity, ro::ui::kSmallestCastBarOpacity, 1.0F,
         kShareFormat);

  swatch(CastBarField::kBackground, Tr("Fond / Opacité"), look.background);
  rows_.back().heading = Tr("Couleurs");
  swatch(CastBarField::kFill, Tr("Remplissage"), look.fill);
}

bool CastBarOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed == kCastBarReplacedRow) {
    look_.replaced = asked.on;
    return true;
  }
  if (asked.changed < kCastBarRowBase || asked.changed >= kCastBarReplacedRow) {
    return false;
  }
  const int offset = asked.changed - kCastBarRowBase;
  const auto kind = static_cast<std::size_t>(offset / kCastBarFieldsPerKind);
  ro::ui::CastBarLook& look = look_.kinds[kind];
  switch (static_cast<CastBarField>(offset % kCastBarFieldsPerKind)) {
    case CastBarField::kShown:
      look.shown = asked.on;
      break;
    case CastBarField::kName:
      look.name = ro::ui::CastBarNameOf(asked.chosen);
      break;
    case CastBarField::kTimeLeft:
      look.time_left = asked.on;
      break;
    case CastBarField::kBorder:
      look.border = asked.on;
      break;
    // ⚠ LES PIXELS SE GARDENT ENTIERS : le curseur en montre des entiers, et
    // une barre posée entre deux pixels se brouille.
    case CastBarField::kWidth:
      look.width_pixels = std::round(asked.value);
      break;
    case CastBarField::kHeight:
      look.height_pixels = std::round(asked.value);
      break;
    case CastBarField::kOffset:
      look.offset_pixels = std::round(asked.value);
      break;
    case CastBarField::kRounding:
      look.rounding = asked.value;
      break;
    case CastBarField::kFontScale:
      look.font_scale = asked.value;
      break;
    case CastBarField::kOpacity:
      look.opacity = asked.value;
      break;
    case CastBarField::kBackground:
      look.background = asked.color;
      break;
    case CastBarField::kFill:
      look.fill = asked.color;
      break;
    case CastBarField::kCount:
      break;
  }
  return true;
}

}  // namespace moonclient
