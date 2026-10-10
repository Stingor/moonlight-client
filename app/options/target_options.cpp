#include "options/target_options.h"

#include <array>
#include <cmath>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/status_bar.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;
using ro::ui::TargetPart;

// Les clés du fichier. GRAVÉES : les changer rend à tous les joueurs les
// réglages d'origine, sans que rien ne le dise.
constexpr const char* kEnabledKey = "target_mode";
constexpr const char* kLockedKey = "target_locked";
constexpr const char* kLayoutModeKey = "target_placement";
constexpr const char* kBorderKey = "target_rule";
constexpr const char* kStickyKey = "target_snapped";
constexpr const char* kRoundingKey = "target_rounding";
constexpr const char* kTextScaleKey = "target_text_size";
constexpr const char* kGaugeTextKey = "target_text_gauges";
constexpr const char* kPortraitDirectionKey = "target_portrait_orientation";
constexpr const char* kPortraitAnimationKey = "target_portrait_action";
constexpr const char* kPortraitAnimatedKey = "target_portrait_animated";
constexpr const char* kNativeMarkerKey = "target_arrow";
constexpr const char* kClickNoAttackKey = "target_click_without_attack";
constexpr const char* kCastOnTargetKey = "target_spells_on_target";
constexpr const char* kProxyClickKey = "target_frames_acting";
constexpr const char* kCycleWrapKey = "target_loop";
constexpr const char* kCycleResetKey = "target_resume_ms";
constexpr const char* kEscapeClearsKey = "target_escape";
constexpr const char* kStatusIconKey = "target_statuses_icon";
constexpr const char* kStatusGapKey = "target_statuses_gap";
constexpr const char* kStatusMostKey = "target_statuses_number";
constexpr const char* kStatusRowsKey = "target_statuses_lines";
constexpr const char* kStatusSortKey = "target_statuses_sorting";
constexpr const char* kStatusCountdownKey = "target_statuses_time";
constexpr const char* kStatusCountdownPixelsKey = "target_statuses_time_size";
constexpr const char* kStatusSweepKey = "target_statuses_flow";
constexpr const char* kStatusSweepColorKey = "target_statuses_veil";
constexpr const char* kStatusIconBackgroundKey = "target_statuses_background_icon";

// Le nom de chaque cadre dans le fichier, dans l'ordre de `TargetPart`.
constexpr std::array<const char*, ro::ui::kTargetPartCount> kPartKeys = {
    "portrait", "name", "hp", "sp", "species", "statuses"};

// Les bornes du panneau.
constexpr float kRoundingMost = 16.0F;
constexpr float kPortraitDirectionMost = 7.0F;
constexpr float kPortraitAnimationMost = 8.0F;
constexpr float kCycleResetMost = 5000.0F;
constexpr float kStatusIconLeast = 12.0F;
constexpr float kStatusIconMost = 48.0F;
constexpr float kStatusGapMost = 12.0F;
constexpr float kStatusMostMost = 40.0F;
constexpr float kStatusRowsMost = 6.0F;
constexpr float kCountdownLeast = 7.0F;
constexpr float kCountdownMost = 20.0F;

[[nodiscard]] std::string PartKey(std::size_t part, const char* what) {
  return std::string("target_") + kPartKeys[part] + "_" + what;
}

[[nodiscard]] const char* PartLabel(TargetPart part) {
  switch (part) {
    case TargetPart::kPortrait:
      return Tr("Portrait");
    case TargetPart::kName:
      return Tr("Nom");
    case TargetPart::kHealth:
      return Tr("PV");
    case TargetPart::kMagic:
      return Tr("SP");
    case TargetPart::kKind:
      return Tr("Race / élément");
    case TargetPart::kStatuses:
      return Tr("États");
  }
  return "";
}

[[nodiscard]] OptionRowView Toggle(TargetRow row, const char* label, const char* help, bool on) {
  return OptionRowView{
      .id = TargetRowId(row), .label = label, .help = help, .kind = OptionKind::kToggle, .on = on};
}

[[nodiscard]] OptionRowView Slider(TargetRow row, const char* label, const char* help, float value,
                                   float lowest, float highest, const char* format) {
  return OptionRowView{.id = TargetRowId(row),
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kSlider,
                       .value = value,
                       .lowest = lowest,
                       .highest = highest,
                       .format = format};
}

[[nodiscard]] OptionRowView Choice(TargetRow row, const char* label, const char* help,
                                   std::span<const char* const> choices, int chosen) {
  return OptionRowView{.id = TargetRowId(row),
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kChoice,
                       .chosen = chosen,
                       .choices = choices};
}

[[nodiscard]] OptionRowView Note(TargetRow row, const char* text) {
  return OptionRowView{.id = TargetRowId(row), .label = text, .kind = OptionKind::kNote};
}

[[nodiscard]] int Rounded(float value) { return static_cast<int>(std::lround(value)); }

}  // namespace

void TargetOptions::Declare(SettingsFile& into) {
  into.Declare(kEnabledKey, &settings_.enabled);
  into.Declare(kLockedKey, &look_.locked);
  into.Declare(kLayoutModeKey, &look_.layout_mode);
  into.Declare(kBorderKey, &look_.border);
  into.Declare(kStickyKey, &look_.sticky);
  into.Declare(kRoundingKey, &look_.rounding);
  into.Declare(kTextScaleKey, &look_.text_scale);
  into.Declare(kGaugeTextKey, &look_.gauge_text);
  into.Declare(kPortraitDirectionKey, &settings_.portrait_direction);
  into.Declare(kPortraitAnimationKey, &settings_.portrait_animation);
  into.Declare(kPortraitAnimatedKey, &settings_.portrait_animated);
  into.Declare(kNativeMarkerKey, &settings_.native_marker);
  into.Declare(kClickNoAttackKey, &settings_.click_no_attack);
  into.Declare(kCastOnTargetKey, &settings_.cast_on_target);
  into.Declare(kProxyClickKey, &settings_.proxy_click);
  into.Declare(kCycleWrapKey, &settings_.cycle_wrap);
  into.Declare(kCycleResetKey, &settings_.cycle_reset_ms);
  into.Declare(kEscapeClearsKey, &settings_.escape_clears);
  // La géométrie ne paraît dans aucune ligne — on la pose à la souris — mais
  // elle se persiste comme le reste.
  for (std::size_t part = 0; part < ro::ui::kTargetPartCount; ++part) {
    ro::ui::TargetPartLook& look = look_.parts[part];
    into.Declare(PartKey(part, "shown"), &look.shown);
    into.Declare(PartKey(part, "x"), &look.rect.x);
    into.Declare(PartKey(part, "y"), &look.rect.y);
    into.Declare(PartKey(part, "width"), &look.rect.width);
    into.Declare(PartKey(part, "height"), &look.rect.height);
    into.Declare(PartKey(part, "anchor_x"), &look.rect.across);
    into.Declare(PartKey(part, "anchor_y"), &look.rect.down);
    into.Declare(PartKey(part, "background"), &look.background);
    into.Declare(PartKey(part, "text"), &look.foreground);
  }
  into.Declare(kStatusIconKey, &look_.status_icon_pixels);
  into.Declare(kStatusGapKey, &look_.status_gap_pixels);
  into.Declare(kStatusMostKey, &look_.status_most);
  into.Declare(kStatusRowsKey, &look_.status_rows);
  into.Declare(kStatusSortKey, &look_.status_sort);
  into.Declare(kStatusCountdownKey, &look_.status_countdown);
  into.Declare(kStatusCountdownPixelsKey, &look_.status_countdown_pixels);
  into.Declare(kStatusSweepKey, &look_.status_sweep);
  into.Declare(kStatusSweepColorKey, &look_.status_sweep_color);
  into.Declare(kStatusIconBackgroundKey, &look_.status_icon_background);
}

void TargetOptions::Compose() {
  rows_.clear();
  gauge_text_names_ = {Tr("Aucun"), Tr("Pourcentage"), Tr("Valeurs"), Tr("Valeurs et pourcentage")};
  sort_names_ = {Tr("Ordre d'arrivée"), Tr("Bientôt fini d'abord"), Tr("Plus long d'abord")};
  sweep_names_ = {Tr("Aucun"), Tr("Balayage horaire"), Tr("Voile descendant")};
  const bool off = !settings_.enabled;

  rows_.push_back(Toggle(
      TargetRow::kEnabled, Tr("Activer le mode Ciblage + HUD"),
      Tr("La cible est la dernière entité cliquée. Le HUD s'allume dessus et y reste quand on "
         "cesse de l'attaquer ; la flèche de ciblage du jeu la suit aussi longtemps. Tout "
         "s'éteint si la cible change, ou si elle s'éloigne hors de vue.\n\nDécoché, TOUT ce qui "
         "suit s'arrête : le HUD, la flèche, les raccourcis de ciblage, le clic sans attaque, les "
         "cadres qui agissent et les sorts qui partent sur la cible."),
      settings_.enabled));
  const std::size_t governed_from = rows_.size();

  rows_.push_back(Toggle(TargetRow::kLocked, Tr("Verrouiller (fige + clic-traversant)"),
                         Tr("Fige les cadres et laisse passer les clics vers le jeu. À décocher "
                            "pour les déplacer : on saisit un cadre n'importe où, ses BORDS le "
                            "redimensionnent."),
                         look_.locked));
  rows_.push_back(Toggle(TargetRow::kLayoutMode,
                         Tr("Mode placement (garder les cadres sans cible)"),
                         Tr("Sans cible il n'y a rien à attraper : ce mode garde les cadres à "
                            "l'écran le temps de les poser. À décocher une fois le HUD en place."),
                         look_.layout_mode));
  rows_.push_back(
      Toggle(TargetRow::kBorder, Tr("Liseré autour des cadres"), nullptr, look_.border));
  rows_.push_back(Toggle(TargetRow::kSticky, Tr("Aimanter les cadres entre eux"),
                         Tr("Pendant un déplacement, les bords s'alignent sur ceux des autres "
                            "cadres. CTRL + déplacement emmène en bloc tous les cadres qui se "
                            "touchent."),
                         look_.sticky));
  rows_.push_back(Slider(TargetRow::kRounding, Tr("Arrondi"), nullptr, look_.rounding, 0.0F,
                         kRoundingMost, "%.1f"));
  rows_.push_back(Slider(TargetRow::kTextScale, Tr("Taille du texte"), nullptr, look_.text_scale,
                         ro::ui::kTargetTextScaleLeast, ro::ui::kTargetTextScaleMost, "%.2f"));
  rows_.push_back(Choice(TargetRow::kGaugeText, Tr("Texte des barres"), nullptr, gauge_text_names_,
                         static_cast<int>(ro::ui::GaugeTextOf(look_.gauge_text))));

  rows_.push_back(Slider(TargetRow::kPortraitDirection, Tr("Orientation"),
                         Tr("0 = de face, comme le portrait du personnage."),
                         static_cast<float>(settings_.portrait_direction), 0.0F,
                         kPortraitDirectionMost, "%.0f"));
  rows_.back().heading = Tr("Portrait");
  rows_.push_back(Slider(TargetRow::kPortraitAnimation, Tr("Action"), nullptr,
                         static_cast<float>(settings_.portrait_animation), 0.0F,
                         kPortraitAnimationMost, "%.0f"));
  rows_.push_back(Toggle(TargetRow::kPortraitAnimated, Tr("Animer"),
                         Tr("Un monstre est un sprite, un joueur est un assemblage (corps, tête, "
                            "coiffes, cape) monté par le composeur de pantin. Les classes rendues "
                            "par un modèle 3D (Emperium, gardiens, coffres) n'ont pas de sprite : "
                            "le cadre le dit."),
                         settings_.portrait_animated));

  rows_.push_back(Toggle(TargetRow::kNativeMarker, Tr("Flèche de ciblage du jeu sur la cible"),
                         Tr("Le petit triangle blanc au-dessus de l'entité. Coché, il suit la "
                            "cible du HUD aussi longtemps qu'elle."),
                         settings_.native_marker));
  rows_.back().heading = Tr("Ciblage");
  rows_.push_back(Toggle(
      TargetRow::kClickNoAttack, Tr("Le clic cible sans attaquer"),
      Tr("Cliquer une entité la prend pour cible, mais n'engage rien : le personnage ne se "
         "déplace pas vers elle et ne frappe pas.\n\nPour attaquer, DOUBLE-CLIQUE. Le délai du "
         "double-clic est celui de Windows.\n\nLes skills ne sont pas concernés, ni "
         "s'asseoir."),
      settings_.click_no_attack));
  rows_.push_back(Toggle(
      TargetRow::kCastOnTarget, Tr("Les sorts ciblés partent sur la cible"),
      Tr("Un skill ciblé demande normalement deux gestes : la touche arme la visée, le "
         "clic désigne qui. Coché, c'est la cible du HUD qui répond — la souris n'a plus à être "
         "sur l'entité.\n\nAvec le Quick cast des sorts ciblés, la souris garde la priorité "
         "quand elle désigne vraiment quelqu'un : ce réglage comble un vide, il ne détourne pas "
         "une visée. Les sorts de zone ne sont pas concernés, ils visent une case.\n\n"
         "Maintenir la touche relance le skill sur la cible."),
      settings_.cast_on_target));
  rows_.push_back(Toggle(
      TargetRow::kProxyClick, Tr("Les cadres agissent comme la cible"),
      Tr("Cliquer un cadre du HUD revient à cliquer l'entité elle-même : attaque, skill ciblé armé "
         "— c'est le même chemin du client qui est rejoué.\n\nUn SEUL clic "
         "attaque, même si « le clic cible sans attaquer » est coché : ce réglage lève "
         "l'ambiguïté d'un clic dans le monde, et il n'y en a aucune sur un cadre — la cible "
         "est déjà désignée.\n\nSans effet tant que les cadres ne sont pas verrouillés : le "
         "clic sert alors à les poser."),
      settings_.proxy_click));
  rows_.back().help_warning =
      Tr("Un cadre qui agit REPREND LA SOURIS au jeu tant que le curseur est dessus : ni clic "
         "vers le sol derrière, ni rotation de caméra. Il ne le fait que quand il y a une cible "
         "ET que le clic aurait un sens — un sort de zone vise une case, le cadre lui laisse "
         "alors le passage.");
  // Coché et sans effet : on le dit ici plutôt que dans une infobulle qu'on
  // n'ouvre qu'en cas de doute.
  if (settings_.proxy_click && !look_.locked) {
    rows_.push_back(Note(TargetRow::kProxyUnlockedNote,
                         Tr("Sans effet : les cadres ne sont pas verrouillés, le clic sert à "
                            "les déplacer.")));
  }

  rows_.push_back(Note(
      TargetRow::kKeyboardNote,
      Tr("Quatre actions à lier dans l'écran des raccourcis : « Cible suivante », « Cible "
         "précédente », « Cible la plus proche » et « Effacer la cible ». Les deux premières "
         "passent d'un monstre à l'autre parmi ceux qui sont à l'écran, du plus proche au plus "
         "loin ; la troisième prend directement le plus proche, sans rien parcourir ; la "
         "quatrième lâche la cible. Tab et Maj+Tab conviennent bien, et restent au chat tant "
         "que la barre de saisie a le focus.")));
  rows_.back().heading = Tr("Ciblage au clavier");
  rows_.push_back(Toggle(TargetRow::kCycleWrap, Tr("Boucler après la dernière cible"), nullptr,
                         settings_.cycle_wrap));
  rows_.push_back(Toggle(TargetRow::kEscapeClears, Tr("Échap efface la cible"),
                         Tr("Échap lâche d'abord la cible, et n'ouvre le menu du jeu que s'il "
                            "n'y en avait pas.\n\nLa touche n'est prise que si le déciblage a "
                            "vraiment eu lieu : sans cible, Échap fait ce qu'il a toujours "
                            "fait.\n\nElle passe en dernier, après les fenêtres qu'Échap referme "
                            "et la barre de chat — un même appui ne fait jamais deux choses."),
                         settings_.escape_clears));
  rows_.push_back(Slider(
      TargetRow::kCycleResetMs, Tr("Reprendre au plus proche après (ms)"),
      Tr("Passé ce délai sans cycler, le prochain appui repart du monstre le plus proche au lieu "
         "de continuer où le cycle en était.\n\nC'est ce qui sépare les deux usages du cyclage : "
         "enchaîner les appuis pour PARCOURIR ce qu'il y a autour, ou appuyer une fois pour "
         "PRENDRE une cible. Sans ce délai, le second héritait de la position laissée par le "
         "premier — on rappuyait après un combat et on repartait au cinquième monstre.\n\n0 = "
         "jamais : le cycle se souvient indéfiniment."),
      static_cast<float>(settings_.cycle_reset_ms), 0.0F, kCycleResetMost, "%.0f"));

  // Les cadres : un interrupteur, puis le fond, puis le texte ou le remplissage.
  for (std::size_t rank = 0; rank < ro::ui::kTargetPartCount; ++rank) {
    const auto part = static_cast<TargetPart>(rank);
    const ro::ui::TargetPartLook& look = look_.parts[rank];
    rows_.push_back(OptionRowView{.id = TargetPartRowId(part, TargetPartField::kShown),
                                  .label = PartLabel(part),
                                  .kind = OptionKind::kToggle,
                                  .on = look.shown});
    if (rank == 0) {
      rows_.back().heading = Tr("Cadres");
    }
    rows_.push_back(OptionRowView{.id = TargetPartRowId(part, TargetPartField::kBackground),
                                  .label = Tr("Fond"),
                                  .kind = OptionKind::kColor,
                                  .color = look.background,
                                  .indented = true});
    // Pour les deux barres, c'est le REMPLISSAGE : leur texte reste clair.
    const bool gauge = part == TargetPart::kHealth || part == TargetPart::kMagic;
    rows_.push_back(OptionRowView{.id = TargetPartRowId(part, TargetPartField::kForeground),
                                  .label = gauge ? Tr("Remplissage") : Tr("Texte"),
                                  .kind = OptionKind::kColor,
                                  .color = look.foreground,
                                  .indented = true});
  }
  rows_.push_back(OptionRowView{.id = TargetRowId(TargetRow::kResetLayout),
                                .label = Tr("Réinitialiser la disposition"),
                                .help = Tr("Repose les cinq cadres à leur place d'origine, en "
                                           "haut à gauche. Les couleurs et les interrupteurs ne "
                                           "bougent pas."),
                                .kind = OptionKind::kAction});

  // ── LES ÉTATS DE LA CIBLE : ce qui décrit le contenu de leur cadre ─────────
  rows_.push_back(Toggle(TargetRow::kStatusPreview, Tr("Aperçu (faux statuts)"),
                         Tr("Remplit l'affichage de faux états, aux durées étagées, le temps de "
                            "le régler — sans attendre d'en avoir de vrais.\n\nNe se garde pas "
                            "d'une session à l'autre."),
                         status_preview_));
  rows_.back().heading = Tr("États de la cible");
  const bool statuses_on = look_.parts[static_cast<std::size_t>(TargetPart::kStatuses)].shown;
  if (!statuses_on) {
    rows_.push_back(Note(TargetRow::kStatusPartOffNote,
                         Tr("Le cadre « États » est décoché dans Cadres, ci-dessus.")));
  }
  const std::size_t status_details_from = rows_.size();
  rows_.push_back(Slider(TargetRow::kStatusIconPixels, Tr("Taille"), nullptr,
                         static_cast<float>(look_.status_icon_pixels), kStatusIconLeast,
                         kStatusIconMost, "%.0f px"));
  rows_.back().heading = Tr("Icônes");
  rows_.push_back(Slider(TargetRow::kStatusGapPixels, Tr("Écart"), nullptr,
                         static_cast<float>(look_.status_gap_pixels), 0.0F, kStatusGapMost,
                         "%.0f px"));
  rows_.push_back(Slider(TargetRow::kStatusMost, Tr("Nombre au plus"),
                         Tr("Au-delà, les états en trop sont écartés — ce sont les DERNIERS du "
                            "rangement ci-dessous, qui décide donc de ce qu'on perd."),
                         static_cast<float>(look_.status_most), 1.0F, kStatusMostMost, "%.0f"));
  rows_.push_back(Slider(TargetRow::kStatusRows, Tr("Lignes d'icônes"),
                         Tr("Le cadre se replie déjà sur sa largeur : ce réglage force en plus un "
                            "nombre fixe par rangée, pour un cadre haut et étroit plutôt que "
                            "long et plat."),
                         static_cast<float>(look_.status_rows), 1.0F, kStatusRowsMost, "%.0f"));
  rows_.push_back(Choice(TargetRow::kStatusSort, Tr("Rangement"),
                         Tr("« Bientôt fini d'abord » met en tête ce qu'il faudra relancer, ou "
                            "ce qu'il suffit d'attendre.\n\nUn état SANS durée (permanent) n'a "
                            "pas sa place dans un tri par durée : il va en fin dans les deux "
                            "sens, pour ne pas chasser de l'écran ce qui presse."),
                         sort_names_, static_cast<int>(ro::ui::StatusSortOf(look_.status_sort))));
  rows_.push_back(OptionRowView{
      .id = TargetRowId(TargetRow::kStatusIconBackground),
      .label = Tr("Fond des icônes"),
      .help =
          Tr("Posé derrière chaque icône, transparence comprise. Les icônes d'état sont en partie "
             "translucides : noir et opaque, elles restent lisibles sur tout "
             "décor ; transparent, le décor passe à travers."),
      .kind = OptionKind::kColor,
      .color = look_.status_icon_background});
  rows_.push_back(Toggle(TargetRow::kStatusCountdown, Tr("Afficher le temps restant"),
                         Tr("Sous chaque icône. Un état permanent n'en porte pas : un « 0 » sous "
                            "un buff qui ne finit jamais le ferait croire terminé."),
                         look_.status_countdown));
  rows_.back().heading = Tr("Compte à rebours");
  rows_.push_back(Slider(TargetRow::kStatusCountdownPixels, Tr("Taille du texte"), nullptr,
                         static_cast<float>(look_.status_countdown_pixels), kCountdownLeast,
                         kCountdownMost, "%.0f px"));
  rows_.back().indented = true;
  rows_.back().disabled = !look_.status_countdown;
  rows_.push_back(Choice(TargetRow::kStatusSweep, Tr("Grisage de la case"),
                         Tr("La case s'assombrit à mesure que l'état s'écoule : on voit d'un coup "
                            "d'œil lesquels sont près de tomber."),
                         sweep_names_,
                         static_cast<int>(ro::ui::StatusSweepOf(look_.status_sweep))));
  rows_.back().help_warning =
      Tr("Un état sans échéance n'est jamais voilé : il n'a pas de part écoulée, et le griser à "
         "moitié le ferait croire à mi-course.");
  rows_.back().heading = Tr("Écoulement");
  rows_.push_back(OptionRowView{
      .id = TargetRowId(TargetRow::kStatusSweepColor),
      .label = Tr("Voile"),
      .kind = OptionKind::kColor,
      .color = look_.status_sweep_color,
      .indented = true,
      .disabled = ro::ui::StatusSweepOf(look_.status_sweep) == ro::ui::StatusSweep::kNone});

  // Un interrupteur maître grise ce qu'il commande : cliquable et sans effet
  // serait le pire des deux mondes.
  for (std::size_t rank = governed_from; rank < rows_.size(); ++rank) {
    rows_[rank].disabled = rows_[rank].disabled || off;
  }
  for (std::size_t rank = status_details_from; rank < rows_.size(); ++rank) {
    rows_[rank].disabled = rows_[rank].disabled || !statuses_on;
  }
}

bool TargetOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed >= kTargetPartRowBase &&
      asked.changed <
          kTargetPartRowBase + static_cast<int>(ro::ui::kTargetPartCount) * kTargetFieldsPerPart) {
    const int offset = asked.changed - kTargetPartRowBase;
    ro::ui::TargetPartLook& look =
        look_.parts[static_cast<std::size_t>(offset / kTargetFieldsPerPart)];
    switch (static_cast<TargetPartField>(offset % kTargetFieldsPerPart)) {
      case TargetPartField::kShown:
        look.shown = asked.on;
        break;
      case TargetPartField::kBackground:
        look.background = asked.color;
        break;
      case TargetPartField::kForeground:
      case TargetPartField::kCount:
        look.foreground = asked.color;
        break;
    }
    return true;
  }
  if (asked.changed < kTargetRowBase ||
      asked.changed >= kTargetRowBase + static_cast<int>(TargetRow::kCount)) {
    return false;
  }
  switch (static_cast<TargetRow>(asked.changed - kTargetRowBase)) {
    case TargetRow::kEnabled:
      settings_.enabled = asked.on;
      break;
    case TargetRow::kLocked:
      look_.locked = asked.on;
      break;
    case TargetRow::kLayoutMode:
      look_.layout_mode = asked.on;
      break;
    case TargetRow::kBorder:
      look_.border = asked.on;
      break;
    case TargetRow::kSticky:
      look_.sticky = asked.on;
      break;
    case TargetRow::kRounding:
      look_.rounding = asked.value;
      break;
    case TargetRow::kTextScale:
      look_.text_scale = asked.value;
      break;
    case TargetRow::kGaugeText:
      look_.gauge_text = asked.chosen;
      break;
    case TargetRow::kPortraitDirection:
      settings_.portrait_direction = Rounded(asked.value);
      break;
    case TargetRow::kPortraitAnimation:
      settings_.portrait_animation = Rounded(asked.value);
      break;
    case TargetRow::kPortraitAnimated:
      settings_.portrait_animated = asked.on;
      break;
    case TargetRow::kNativeMarker:
      settings_.native_marker = asked.on;
      break;
    case TargetRow::kClickNoAttack:
      settings_.click_no_attack = asked.on;
      break;
    case TargetRow::kCastOnTarget:
      settings_.cast_on_target = asked.on;
      break;
    case TargetRow::kProxyClick:
      settings_.proxy_click = asked.on;
      break;
    case TargetRow::kCycleWrap:
      settings_.cycle_wrap = asked.on;
      break;
    case TargetRow::kEscapeClears:
      settings_.escape_clears = asked.on;
      break;
    case TargetRow::kCycleResetMs:
      settings_.cycle_reset_ms = Rounded(asked.value);
      break;
    case TargetRow::kResetLayout:
      // Les géométries seules : couleurs et interrupteurs ne bougent pas.
      for (std::size_t rank = 0; rank < ro::ui::kTargetPartCount; ++rank) {
        look_.parts[rank].rect = ro::ui::DefaultTargetPartRect(static_cast<TargetPart>(rank));
      }
      break;
    case TargetRow::kStatusPreview:
      status_preview_ = asked.on;
      break;
    case TargetRow::kStatusIconPixels:
      look_.status_icon_pixels = Rounded(asked.value);
      break;
    case TargetRow::kStatusGapPixels:
      look_.status_gap_pixels = Rounded(asked.value);
      break;
    case TargetRow::kStatusMost:
      look_.status_most = Rounded(asked.value);
      break;
    case TargetRow::kStatusRows:
      look_.status_rows = Rounded(asked.value);
      break;
    case TargetRow::kStatusSort:
      look_.status_sort = asked.chosen;
      break;
    case TargetRow::kStatusCountdown:
      look_.status_countdown = asked.on;
      break;
    case TargetRow::kStatusCountdownPixels:
      look_.status_countdown_pixels = Rounded(asked.value);
      break;
    case TargetRow::kStatusSweep:
      look_.status_sweep = asked.chosen;
      break;
    case TargetRow::kStatusSweepColor:
      look_.status_sweep_color = asked.color;
      break;
    case TargetRow::kStatusIconBackground:
      look_.status_icon_background = asked.color;
      break;
    case TargetRow::kProxyUnlockedNote:
    case TargetRow::kKeyboardNote:
    case TargetRow::kStatusPartOffNote:
    case TargetRow::kCount:
      break;
  }
  return true;
}

}  // namespace moonclient
