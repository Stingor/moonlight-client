#include "options/status_portrait_options.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;
using ro::ui::PortraitPart;

// Les clés du fichier. GRAVÉES : les changer rend à tous les joueurs les
// réglages d'origine, sans que rien ne le dise.
constexpr const char* kShownKey = "portrait_shown";
constexpr const char* kStickyKey = "portrait_snapped";
constexpr const char* kBorderKey = "portrait_border";
constexpr const char* kLabelsShownKey = "labels_shown";
constexpr const char* kLabelsStickyKey = "labels_snapped";
constexpr const char* kLabelsBorderKey = "labels_border";
constexpr const char* kHeadOnlyKey = "portrait_head_only";
constexpr const char* kZoomKey = "portrait_zoom";
constexpr const char* kOffsetXKey = "portrait_offset_x";
constexpr const char* kOffsetYKey = "portrait_offset_y";
constexpr const char* kAnimationKey = "portrait_action";
constexpr const char* kDirectionKey = "portrait_orientation";
constexpr const char* kAnimatedKey = "portrait_animated";
constexpr const char* kGarmentKey = "portrait_garment";
constexpr const char* kLevelFormatKey = "portrait_level_format";

// LES CLÉS RETIRÉES. Le verrou est celui des jauges ; le cadre du portrait
// s'allume par l'interrupteur de son groupe, dont il est le seul membre.
constexpr const char* kRetiredLockedKey = "portrait_locked";
constexpr const char* kRetiredHeadShownKey = "portrait_head_shown";

// Le nom de chaque cadre dans le fichier, dans l'ordre de `PortraitPart`.
constexpr std::array<const char*, ro::ui::kPortraitPartCount> kPartKeys = {"head", "name", "class",
                                                                           "level"};

[[nodiscard]] std::string PartKey(std::size_t part, const char* what) {
  return std::string("portrait_") + kPartKeys[part] + "_" + what;
}

[[nodiscard]] const char* PartLabel(PortraitPart part) {
  switch (part) {
    case PortraitPart::kHead:
      return Tr("Portrait");
    case PortraitPart::kName:
      return Tr("Pseudo");
    case PortraitPart::kClass:
      return Tr("Classe");
    case PortraitPart::kLevel:
      return Tr("Niveau");
  }
  return "";
}

[[nodiscard]] OptionRowView Toggle(PortraitRow row, const char* label, const char* help, bool on) {
  return OptionRowView{.id = PortraitRowId(row),
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kToggle,
                       .on = on};
}

[[nodiscard]] OptionRowView Slider(PortraitRow row, const char* label, const char* help,
                                   float value, float lowest, float highest, const char* format) {
  return OptionRowView{.id = PortraitRowId(row),
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kSlider,
                       .value = value,
                       .lowest = lowest,
                       .highest = highest,
                       .format = format};
}

[[nodiscard]] OptionRowView Choice(PortraitRow row, const char* label, const char* help,
                                   std::span<const char* const> choices, int chosen) {
  return OptionRowView{.id = PortraitRowId(row),
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kChoice,
                       .chosen = chosen,
                       .choices = choices};
}

[[nodiscard]] int Wrapped(int value, int count) { return ((value % count) + count) % count; }

}  // namespace

PortraitFramingSettings BoundedFraming(const PortraitFramingSettings& stored) {
  PortraitFramingSettings bounded = stored;
  bounded.zoom = std::clamp(stored.zoom, kPortraitZoomLeast, kPortraitZoomMost);
  bounded.offset_x = std::clamp(stored.offset_x, -kPortraitOffsetMost, kPortraitOffsetMost);
  bounded.offset_y = std::clamp(stored.offset_y, -kPortraitOffsetMost, kPortraitOffsetMost);
  bounded.animation = std::clamp(stored.animation, 0, kPortraitAnimationCount - 1);
  bounded.direction = Wrapped(stored.direction, kPortraitDirectionCount);
  return bounded;
}

void StatusPortraitOptions::Declare(SettingsFile& into) {
  ro::ui::PortraitGroupLook& head = look_.group(ro::ui::PortraitGroup::kHead);
  into.Declare(kShownKey, &head.shown);
  into.Declare(kStickyKey, &head.sticky);
  into.Declare(kBorderKey, &head.border);
  ro::ui::PortraitGroupLook& labels = look_.group(ro::ui::PortraitGroup::kLabels);
  into.Declare(kLabelsShownKey, &labels.shown);
  into.Declare(kLabelsStickyKey, &labels.sticky);
  into.Declare(kLabelsBorderKey, &labels.border);
  into.Retire(kRetiredLockedKey);
  into.Retire(kRetiredHeadShownKey);
  into.Declare(kHeadOnlyKey, &framing_.head_only);
  into.Declare(kZoomKey, &framing_.zoom);
  into.Declare(kOffsetXKey, &framing_.offset_x);
  into.Declare(kOffsetYKey, &framing_.offset_y);
  into.Declare(kAnimationKey, &framing_.animation);
  into.Declare(kDirectionKey, &framing_.direction);
  into.Declare(kAnimatedKey, &framing_.animated);
  into.Declare(kGarmentKey, &framing_.garment);
  into.Declare(kLevelFormatKey, &look_.level_format);
  // La géométrie ne paraît dans aucune ligne — on la pose à la souris — mais
  // elle se persiste comme le reste.
  for (std::size_t part = 0; part < ro::ui::kPortraitPartCount; ++part) {
    ro::ui::PortraitPartLook& look = look_.parts[part];
    if (static_cast<PortraitPart>(part) != PortraitPart::kHead) {
      into.Declare(PartKey(part, "shown"), &look.shown);
    }
    into.Declare(PartKey(part, "x"), &look.rect.x);
    into.Declare(PartKey(part, "y"), &look.rect.y);
    into.Declare(PartKey(part, "width"), &look.rect.width);
    into.Declare(PartKey(part, "height"), &look.rect.height);
    into.Declare(PartKey(part, "anchor_x"), &look.rect.across);
    into.Declare(PartKey(part, "anchor_y"), &look.rect.down);
    into.Declare(PartKey(part, "background"), &look.background);
    into.Declare(PartKey(part, "text"), &look.text);
    into.Declare(PartKey(part, "rounding"), &look.rounding);
    into.Declare(PartKey(part, "text_size"), &look.text_scale);
  }
}

void StatusPortraitOptions::Compose(PortraitTab tab, bool locked) {
  rows_.clear();
  ComposeGroup(tab, locked);
  if (tab == PortraitTab::kLabels) {
    ComposeLabels();
  } else {
    ComposeFraming();
  }

  // L'interrupteur du groupe grise tout ce qui le suit dans l'onglet :
  // cliquable et sans effet serait le pire des deux mondes.
  const bool off = !look_.group(GroupOfTab(tab)).shown;
  for (std::size_t rank = 1; rank < rows_.size(); ++rank) {
    rows_[rank].disabled = rows_[rank].disabled || off;
  }
}

void StatusPortraitOptions::ComposeGroup(PortraitTab tab, bool locked) {
  const ro::ui::PortraitGroupLook& group = look_.group(GroupOfTab(tab));
  if (tab == PortraitTab::kLabels) {
    rows_.push_back(Toggle(PortraitRow::kLabelsShown, Tr("Afficher les étiquettes"),
                           Tr("Le pseudo, la classe et le niveau : trois cadres indépendants, "
                              "que l'on déplace, retaille et colore chacun à son goût."),
                           group.shown));
    rows_.push_back(Toggle(PortraitRow::kLabelsSticky, Tr("Aimanter"),
                           Tr("Les bords d'une étiquette qu'on déplace s'alignent sur ceux des "
                              "autres étiquettes, du portrait et des jauges. Deux étiquettes qui "
                              "se touchent partent ensemble au CTRL ; le portrait et les jauges "
                              "restent en place."),
                           group.sticky));
    rows_.back().disabled = locked;
    rows_.push_back(Toggle(PortraitRow::kLabelsBorder, Tr("Bordure"),
                           Tr("Un trait d'un pixel autour de chaque étiquette."), group.border));
    rows_.push_back(OptionRowView{.id = PortraitRowId(PortraitRow::kLabelsResetLayout),
                                  .label = Tr("Réinitialiser la disposition"),
                                  .help = Tr("Repose les trois étiquettes à leur place d'origine. "
                                             "Leurs couleurs et leurs interrupteurs ne bougent "
                                             "pas."),
                                  .kind = OptionKind::kAction});
    return;
  }
  rows_.push_back(Toggle(PortraitRow::kShown, Tr("Afficher le portrait"),
                         Tr("Le personnage dans son cadre, que l'on déplace, retaille et colore "
                            "à son goût."),
                         group.shown));
  rows_.push_back(Toggle(PortraitRow::kSticky, Tr("Aimanter"),
                         Tr("Les bords du portrait qu'on déplace s'alignent sur ceux des "
                            "étiquettes et des jauges, qui restent en place."),
                         group.sticky));
  rows_.back().disabled = locked;
  rows_.push_back(Toggle(PortraitRow::kBorder, Tr("Bordure"),
                         Tr("Un trait d'un pixel autour du cadre du portrait."), group.border));
  rows_.push_back(OptionRowView{.id = PortraitRowId(PortraitRow::kResetLayout),
                                .label = Tr("Réinitialiser la disposition"),
                                .help = Tr("Repose le cadre du portrait à sa place d'origine. Ses "
                                           "couleurs et son cadrage ne bougent pas."),
                                .kind = OptionKind::kAction});
}

void StatusPortraitOptions::ResetLayout(ro::ui::PortraitGroup group) {
  // Les géométries seules, celles que le propriétaire a livrées : couleurs et
  // interrupteurs ne bougent pas.
  for (std::size_t rank = 0; rank < ro::ui::kPortraitPartCount; ++rank) {
    if (ro::ui::PortraitGroupOf(static_cast<PortraitPart>(rank)) == group) {
      look_.parts[rank].rect = factory_look_.parts[rank].rect;
    }
  }
}

void StatusPortraitOptions::ComposeFraming() {
  animation_names_ = {Tr("Repos"),   Tr("Marche"), Tr("Assis"), Tr("Ramasser"), Tr("Combat"),
                      Tr("Attaque"), Tr("Touché"), Tr("Gelé"),  Tr("Mort")};
  direction_names_ = {Tr("Face"), Tr("Profil-Gauche"),  Tr("Gauche"), Tr("Arrière-Gauche"),
                      Tr("Dos"),  Tr("Arrière-Droite"), Tr("Droite"), Tr("Profil-Droite")};

  rows_.push_back(Toggle(PortraitRow::kHeadOnly, Tr("Tête seule"),
                         Tr("Cadre la tête et ses coiffes ; le corps sort du cadre et la cape est "
                            "masquée.\nDécocher pour le personnage entier."),
                         framing_.head_only));
  rows_.back().heading = Tr("Portrait");
  rows_.push_back(Toggle(PortraitRow::kGarment, Tr("Cape / garment"),
                         Tr("Montre la cape équipée — en personnage entier seulement."),
                         framing_.garment));
  rows_.back().indented = true;
  rows_.back().disabled = framing_.head_only;
  rows_.push_back(Slider(PortraitRow::kZoom, Tr("Zoom"),
                         Tr("Agrandit autour de la tête, ou du personnage entier."), framing_.zoom,
                         kPortraitZoomLeast, kPortraitZoomMost, "%.2f"));
  const char* offset_help =
      Tr("En tailles du cadre : 0 laisse le personnage centré. Le zoom reste centré.");
  rows_.push_back(Slider(PortraitRow::kOffsetX, Tr("Décalage horizontal"), offset_help,
                         framing_.offset_x, -kPortraitOffsetMost, kPortraitOffsetMost, "%+.2f"));
  rows_.push_back(Slider(PortraitRow::kOffsetY, Tr("Décalage vertical"), offset_help,
                         framing_.offset_y, -kPortraitOffsetMost, kPortraitOffsetMost, "%+.2f"));
  rows_.push_back(Choice(PortraitRow::kAnimation, Tr("Animation"), nullptr, animation_names_,
                         std::clamp(framing_.animation, 0, kPortraitAnimationCount - 1)));
  rows_.push_back(Choice(PortraitRow::kDirection, Tr("Direction"), nullptr, direction_names_,
                         Wrapped(framing_.direction, kPortraitDirectionCount)));
  rows_.push_back(Toggle(PortraitRow::kAnimated, Tr("Animer"),
                         Tr("Joue les images de la pose. Décoché, la pose reste figée sur sa "
                            "première image."),
                         framing_.animated));

  ComposePart(PortraitPart::kHead, Tr("Apparence"));
}

void StatusPortraitOptions::ComposeLabels() {
  // « Base » et « Job » restent en anglais : c'est le vocabulaire du jeu.
  level_format_names_ = {"99/70", "Base 99 · Job 70", Tr("Base seule (99)")};
  rows_.push_back(Choice(PortraitRow::kLevelFormat, Tr("Format du niveau"), nullptr,
                         level_format_names_,
                         static_cast<int>(ro::ui::LevelFormatOf(look_.level_format))));
  rows_.back().heading = Tr("Niveau");

  const char* heading = Tr("Couleurs, arrondis et taille du texte");
  for (const PortraitPart part :
       {PortraitPart::kName, PortraitPart::kClass, PortraitPart::kLevel}) {
    ComposePart(part, heading);
    // Le titre ne se pose qu'une fois, sur la première étiquette.
    heading = nullptr;
  }
}

void StatusPortraitOptions::ComposePart(PortraitPart part, const char* heading) {
  const ro::ui::PortraitPartLook& look = look_.parts[static_cast<std::size_t>(part)];
  // Le portrait n'écrit rien : ni texte, ni taille. Et il n'a pas d'interrupteur
  // à lui : seul de son groupe, il s'allume par celui du groupe.
  const bool writes = part != PortraitPart::kHead;
  if (writes) {
    rows_.push_back(OptionRowView{.id = PortraitPartRowId(part, PortraitPartField::kShown),
                                  .label = PartLabel(part),
                                  .kind = OptionKind::kToggle,
                                  .on = look.shown,
                                  .heading = heading});
  }
  rows_.push_back(OptionRowView{.id = PortraitPartRowId(part, PortraitPartField::kBackground),
                                .label = Tr("Fond"),
                                .kind = OptionKind::kColor,
                                .color = look.background,
                                .heading = writes ? nullptr : heading,
                                .indented = writes,
                                .disabled = !look.shown});
  if (writes) {
    rows_.push_back(OptionRowView{.id = PortraitPartRowId(part, PortraitPartField::kText),
                                  .label = Tr("Texte"),
                                  .kind = OptionKind::kColor,
                                  .color = look.text,
                                  .indented = true,
                                  .disabled = !look.shown});
  }
  rows_.push_back(OptionRowView{.id = PortraitPartRowId(part, PortraitPartField::kRounding),
                                .label = Tr("Arrondi"),
                                .kind = OptionKind::kSlider,
                                .value = look.rounding,
                                .lowest = 0.0F,
                                .highest = ro::ui::kPortraitRoundingMost,
                                .format = "%.0f",
                                .indented = true,
                                .disabled = !look.shown});
  if (writes) {
    rows_.push_back(OptionRowView{.id = PortraitPartRowId(part, PortraitPartField::kTextScale),
                                  .label = Tr("Taille du texte"),
                                  .kind = OptionKind::kSlider,
                                  .value = look.text_scale,
                                  .lowest = ro::ui::kPortraitTextScaleLeast,
                                  .highest = ro::ui::kPortraitTextScaleMost,
                                  .format = "%.2f",
                                  .indented = true,
                                  .disabled = !look.shown});
  }
}

bool StatusPortraitOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed >= kPortraitPartRowBase &&
      asked.changed < kPortraitPartRowBase +
                          static_cast<int>(ro::ui::kPortraitPartCount) * kPortraitFieldsPerPart) {
    const int offset = asked.changed - kPortraitPartRowBase;
    ro::ui::PortraitPartLook& look =
        look_.parts[static_cast<std::size_t>(offset / kPortraitFieldsPerPart)];
    switch (static_cast<PortraitPartField>(offset % kPortraitFieldsPerPart)) {
      case PortraitPartField::kShown:
        look.shown = asked.on;
        break;
      case PortraitPartField::kBackground:
        look.background = asked.color;
        break;
      case PortraitPartField::kText:
        look.text = asked.color;
        break;
      case PortraitPartField::kRounding:
        look.rounding = std::clamp(asked.value, 0.0F, ro::ui::kPortraitRoundingMost);
        break;
      case PortraitPartField::kTextScale:
        look.text_scale = std::clamp(asked.value, ro::ui::kPortraitTextScaleLeast,
                                     ro::ui::kPortraitTextScaleMost);
        break;
      case PortraitPartField::kCount:
        break;
    }
    return true;
  }
  if (asked.changed < kPortraitRowBase ||
      asked.changed >= kPortraitRowBase + static_cast<int>(PortraitRow::kCount)) {
    return false;
  }
  ro::ui::PortraitGroupLook& head = look_.group(ro::ui::PortraitGroup::kHead);
  ro::ui::PortraitGroupLook& labels = look_.group(ro::ui::PortraitGroup::kLabels);
  switch (static_cast<PortraitRow>(asked.changed - kPortraitRowBase)) {
    case PortraitRow::kShown:
      head.shown = asked.on;
      break;
    case PortraitRow::kSticky:
      head.sticky = asked.on;
      break;
    case PortraitRow::kBorder:
      head.border = asked.on;
      break;
    case PortraitRow::kResetLayout:
      ResetLayout(ro::ui::PortraitGroup::kHead);
      break;
    case PortraitRow::kLabelsShown:
      labels.shown = asked.on;
      break;
    case PortraitRow::kLabelsSticky:
      labels.sticky = asked.on;
      break;
    case PortraitRow::kLabelsBorder:
      labels.border = asked.on;
      break;
    case PortraitRow::kLabelsResetLayout:
      ResetLayout(ro::ui::PortraitGroup::kLabels);
      break;
    case PortraitRow::kHeadOnly:
      framing_.head_only = asked.on;
      break;
    case PortraitRow::kGarment:
      framing_.garment = asked.on;
      break;
    case PortraitRow::kZoom:
      framing_.zoom = std::clamp(asked.value, kPortraitZoomLeast, kPortraitZoomMost);
      break;
    case PortraitRow::kOffsetX:
      framing_.offset_x = std::clamp(asked.value, -kPortraitOffsetMost, kPortraitOffsetMost);
      break;
    case PortraitRow::kOffsetY:
      framing_.offset_y = std::clamp(asked.value, -kPortraitOffsetMost, kPortraitOffsetMost);
      break;
    case PortraitRow::kAnimation:
      framing_.animation = std::clamp(asked.chosen, 0, kPortraitAnimationCount - 1);
      break;
    case PortraitRow::kDirection:
      framing_.direction = Wrapped(asked.chosen, kPortraitDirectionCount);
      break;
    case PortraitRow::kAnimated:
      framing_.animated = asked.on;
      break;
    case PortraitRow::kLevelFormat:
      look_.level_format = static_cast<int>(ro::ui::LevelFormatOf(asked.chosen));
      break;
    case PortraitRow::kCount:
      break;
  }
  return true;
}

}  // namespace moonclient
