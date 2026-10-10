// La page de réglages du carnet de chasse MVP. Voir l'en-tête.

#include "options/mvp_tracker_options.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier. ⚠ GRAVÉES, et en anglais : les changer rend à tous les
// joueurs les réglages d'origine, sans que rien ne le dise.
constexpr const char* kEnabledKey = "mvp_tracker_enabled";
constexpr const char* kAlertSoundKey = "mvp_tracker_alert_sound";
constexpr const char* kAlertLeadKey = "mvp_tracker_alert_lead_min";
constexpr const char* kAlertToneKey = "mvp_tracker_alert_tone";
constexpr const char* kAlertWavKey = "mvp_tracker_alert_wav";
constexpr const char* kShowTombsKey = "mvp_tracker_show_tombs";
constexpr const char* kShowSpritesKey = "mvp_tracker_show_sprites";
constexpr const char* kAnimateSpritesKey = "mvp_tracker_animate_sprites";
constexpr const char* kShowFilterKey = "mvp_tracker_show_filter";
constexpr const char* kShowDelayKey = "mvp_tracker_show_delay";
constexpr const char* kLineSpriteKey = "mvp_tracker_line_sprite";
constexpr const char* kLineTextKey = "mvp_tracker_line_text";
constexpr const char* kLineBackgroundKey = "mvp_tracker_line_background";

// Les textes composés de la page : la porte de la touche, le compte des lignes
// détachées et la question posée avant de les remettre.
constexpr std::size_t kComposedBytes = 160;

[[nodiscard]] int BoundedLead(int lead_min) {
  return std::clamp(lead_min, kMvpAlertLeadLeastMin, kMvpAlertLeadMostMin);
}

[[nodiscard]] OptionRowView Toggle(MvpTrackerRow row, const char* label, const char* help, bool on,
                                   bool disabled) {
  return OptionRowView{.id = MvpTrackerRowId(row),
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kToggle,
                       .on = on,
                       .disabled = disabled};
}

[[nodiscard]] OptionRowView Color(MvpTrackerRow row, const char* label, const char* help,
                                  ImU32 color, bool disabled) {
  return OptionRowView{.id = MvpTrackerRowId(row),
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kColor,
                       .color = color,
                       .disabled = disabled};
}

// LA RAISON D'UN SON QUI N'A PAS SONNÉ, en une ligne, ou nul. Les échecs du
// fichier personnel nomment `savedata`, ceux d'un son du jeu l'installation.
[[nodiscard]] const char* TroubleText(MvpAlertTrouble trouble, bool custom) {
  switch (trouble) {
    case MvpAlertTrouble::kNone:
      return nullptr;
    case MvpAlertTrouble::kMuted:
      return Tr("Les bruitages du jeu sont coupés : la case « Bruitages » les rallume.");
    case MvpAlertTrouble::kNameEmpty:
      return Tr("Aucun fichier nommé.");
    case MvpAlertTrouble::kNameRefused:
      return Tr("Nom refusé : le fichier doit rester dans savedata.");
    case MvpAlertTrouble::kFileMissing:
      return custom ? Tr("Fichier introuvable dans savedata.")
                    : Tr("Son du jeu introuvable dans l'installation.");
    case MvpAlertTrouble::kFileUnreadable:
      return custom ? Tr("Fichier illisible : ce n'est pas un .wav lisible.")
                    : Tr("Son du jeu illisible.");
  }
  return nullptr;
}

// La raison n'est-elle propre au fichier nommé ? Elle s'écrit alors sous son
// champ, et sinon sous le choix du son.
[[nodiscard]] bool TroubleOfFile(MvpAlertTrouble trouble, bool custom) {
  return custom && trouble != MvpAlertTrouble::kMuted;
}

}  // namespace

MvpTrackerSettings MvpTrackerOptions::settings() const noexcept {
  MvpTrackerSettings bounded = settings_;
  bounded.alert_lead_min = BoundedLead(settings_.alert_lead_min);
  bounded.alert_tone = static_cast<int>(MvpAlertToneOf(settings_.alert_tone));
  return bounded;
}

void MvpTrackerOptions::NoteSoundAudible(bool audible) noexcept {
  if (audible && alert_trouble_ == MvpAlertTrouble::kMuted) {
    alert_trouble_ = MvpAlertTrouble::kNone;
  }
}

bool MvpTrackerOptions::SetShowDelay(bool shown) noexcept {
  if (settings_.look.show_delay == shown) {
    return false;
  }
  settings_.look.show_delay = shown;
  return true;
}

void MvpTrackerOptions::Declare(SettingsFile& into) {
  into.Declare(kEnabledKey, &settings_.enabled);
  into.Declare(kAlertSoundKey, &settings_.alert_sound);
  into.Declare(kAlertLeadKey, &settings_.alert_lead_min);
  into.Declare(kAlertToneKey, &settings_.alert_tone);
  into.Declare(kAlertWavKey, &settings_.alert_wav);
  into.Declare(kShowTombsKey, &settings_.show_tombs);
  into.Declare(kShowSpritesKey, &settings_.look.show_sprites);
  into.Declare(kAnimateSpritesKey, &settings_.look.animate_sprites);
  into.Declare(kShowFilterKey, &settings_.look.show_filter);
  into.Declare(kShowDelayKey, &settings_.look.show_delay);
  into.Declare(kLineSpriteKey, &settings_.look.line_show_sprite);
  into.Declare(kLineTextKey, &settings_.look.line_text_color);
  into.Declare(kLineBackgroundKey, &settings_.look.line_background_color);
}

void MvpTrackerOptions::Compose() {
  const MvpTrackerSettings hunt = settings();
  // 🔴 L'INTERRUPTEUR GRISE TOUT, SAUF LUI-MÊME : il est le seul à rallumer le
  // carnet, et éteint, « Ouvrir le carnet » n'ouvrirait rien.
  const bool off = !hunt.enabled;

  std::array<char, kComposedBytes> text{};
  if (open_key_.empty()) {
    std::snprintf(text.data(), text.size(), Tr("ou une touche à lier : « %s »"),
                  Tr("Carnet de chasse MVP"));
  } else {
    std::snprintf(text.data(), text.size(), Tr("ou la touche %s"), open_key_.c_str());
  }
  open_hint_ = text.data();
  std::snprintf(text.data(), text.size(), Tr("Tout remettre au carnet (%d)"), line_count_);
  put_back_label_ = text.data();
  // ⚠ UNE SEULE LIGNE SE DIT AU SINGULIER : « les 1 lignes » ne se lit pas.
  if (line_count_ == 1) {
    put_back_confirm_ = Tr("Remettre au carnet la ligne détachée ?");
  } else {
    std::snprintf(text.data(), text.size(), Tr("Remettre au carnet les %d lignes détachées ?"),
                  line_count_);
    put_back_confirm_ = text.data();
  }

  // ⚠ LES VUES APRÈS L'ÉCRITURE DES CHAÎNES : elles pointent dedans.
  rows_.clear();
  rows_.push_back(Toggle(
      MvpTrackerRow::kEnabled, Tr("Activer le carnet de chasse MVP"),
      Tr("Éteint, le carnet ne reçoit, n'affiche et n'envoie plus rien : ni fenêtre, ni icône, "
         "ni alerte, ni tombe sur la minimap, ni ligne détachée, ni invitation. Seule cette case "
         "le rallume, et tout revient.\n\nMais tes kills de MVP continuent d'alimenter ton "
         "groupe de chasse : c'est le serveur qui les enregistre, pour chacun de ses membres. "
         "Pour cesser de partager, quitte le groupe."),
      hunt.enabled, false));
  rows_.push_back(OptionRowView{.id = MvpTrackerRowId(MvpTrackerRow::kOpen),
                                .label = Tr("Ouvrir le carnet"),
                                .kind = OptionKind::kAction,
                                .object_note = open_hint_.c_str(),
                                .disabled = off});
  rows_.push_back(Toggle(MvpTrackerRow::kAlertSound, Tr("Alerte sonore sur un favori"), nullptr,
                         hunt.alert_sound, off));
  // LE SON, LA PORTE POUR L'ÉCOUTER ET LE FICHIER PERSONNEL : sous la case qu'ils
  // précisent, et grisés avec elle.
  const bool silent = off || !hunt.alert_sound;
  tone_names_ = {Tr("Son du système"), Tr("Nouveau message"), Tr("Bouton"),
                 Tr("Fichier .wav personnel")};
  rows_.push_back(OptionRowView{
      .id = MvpTrackerRowId(MvpTrackerRow::kAlertTone),
      .label = Tr("Son de l'alerte"),
      .help = Tr("Le son du système suit le réglage de Windows et ignore le volume du client ; "
                 "ailleurs que sous Windows, il est remplacé par « Nouveau message ».\n\nLes "
                 "autres se jouent au volume du client : deux sons du jeu, ou ton propre fichier "
                 ".wav, nommé dans le champ dessous."),
      .kind = OptionKind::kChoice,
      .chosen = hunt.alert_tone,
      .choices = tone_names_,
      .indented = true,
      .disabled = silent});
  rows_.push_back(OptionRowView{.id = MvpTrackerRowId(MvpTrackerRow::kAlertListen),
                                .label = Tr("Écouter"),
                                .kind = OptionKind::kAction,
                                .small_button = true,
                                .same_line = true,
                                .disabled = silent});
  // LA RAISON D'UN SON QUI N'A PAS SONNÉ, sous le réglage en cause. ⚠ ABSENTE
  // QUAND L'ALERTE SE TAIT : il n'y a alors rien à entendre.
  const bool custom = MvpAlertToneOf(hunt.alert_tone) == MvpAlertTone::kCustom;
  const char* trouble_text = silent ? nullptr : TroubleText(alert_trouble_, custom);
  const auto push_trouble = [&] {
    rows_.push_back(OptionRowView{.id = MvpTrackerRowId(MvpTrackerRow::kAlertTrouble),
                                  .label = trouble_text,
                                  .kind = OptionKind::kNote,
                                  .indented = true,
                                  .warning = true});
  };
  if (trouble_text != nullptr && !TroubleOfFile(alert_trouble_, custom)) {
    push_trouble();
  }
  // ⚠ LE TEXTE EST CELUI DES RÉGLAGES MÊMES, et non de la copie normalisée : la
  // rangée le pointe jusqu'au dessin.
  rows_.push_back(OptionRowView{
      .id = MvpTrackerRowId(MvpTrackerRow::kAlertFile),
      .label = Tr("Fichier .wav, dans savedata"),
      .help = Tr("Le nom du fichier, relatif au dossier savedata du jeu : « alerte.wav », ou "
                 "« sons\\alerte.wav » dans un sous-dossier. Un nom qui sort de savedata est "
                 "refusé.\n\nUn fichier absent ou illisible ne sonne pas : l'alerte reste écrite "
                 "à l'écran, et la raison s'écrit sous ce champ."),
      .kind = OptionKind::kText,
      .text = settings_.alert_wav.c_str(),
      .indented = true,
      .disabled = silent || !custom});
  if (trouble_text != nullptr && TroubleOfFile(alert_trouble_, custom)) {
    push_trouble();
  }
  rows_.push_back(OptionRowView{
      .id = MvpTrackerRowId(MvpTrackerRow::kAlertLead),
      .label = Tr("Préavis d'alerte (min)"),
      .help = Tr("Combien de minutes AVANT l'ouverture de la fenêtre de retour l'alerte se "
                 "déclenche.\n\nLes favoris eux-mêmes sont enregistrés sur le serveur : ils "
                 "suivent ton compte, donc tous tes personnages et tous tes postes. Seuls ce "
                 "préavis et le son restent locaux."),
      .kind = OptionKind::kSlider,
      .value = static_cast<float>(hunt.alert_lead_min),
      .lowest = static_cast<float>(kMvpAlertLeadLeastMin),
      .highest = static_cast<float>(kMvpAlertLeadMostMin),
      .format = "%.0f",
      .disabled = off});
  rows_.push_back(Toggle(
      MvpTrackerRow::kShowSprites, Tr("Sprite du MVP dans la liste"),
      Tr("Reconnaître un monstre à sa silhouette va plus vite que lire son nom anglais dans une "
         "table de quatre-vingts lignes.\n\nSurvoler son nom donne la fiche compacte, un clic "
         "droit ouvre les mêmes actions que sur un lien de monstre du chat — fiche, bestiaire, "
         "« où le trouver », @mobinfo, @whereis. Et survoler la carte montre le plan avec la "
         "tombe pointée."),
      hunt.look.show_sprites, off));
  rows_.push_back(Toggle(
      MvpTrackerRow::kAnimateSprites, Tr("Animer les sprites"),
      Tr("Éteint par défaut : quatre-vingts silhouettes qui bougent en même temps font de la "
         "table un aquarium, et le regard n'accroche plus la ligne qu'on cherche."),
      hunt.look.animate_sprites, off || !hunt.look.show_sprites));
  rows_.push_back(Toggle(
      MvpTrackerRow::kShowFilter, Tr("Champ de filtre"),
      Tr("Ajoute une barre de recherche au-dessus de la table, qui filtre sur le nom du MVP "
         "comme sur celui de la carte.\n\nLe filtre lui-même n'est pas enregistré : on filtre "
         "pour trouver quelque chose maintenant, pas pour rouvrir demain sur une table amputée "
         "sans se rappeler pourquoi."),
      hunt.look.show_filter, off));
  rows_.back().help_warning =
      Tr("Les COLONNES, elles, se règlent au clic droit sur la ligne d'en-tête — on y montre "
         "« Délai », le temps de retour habituel, masqué par défaut.");
  rows_.push_back(Toggle(
      MvpTrackerRow::kShowDelay, Tr("Colonne « Délai »"),
      Tr("Montre dans la table le délai habituel de retour, la loi du script de spawn. Le clic "
         "droit sur l'en-tête de la table la montre ou la masque aussi."),
      hunt.look.show_delay, off));
  rows_.push_back(Toggle(
      MvpTrackerRow::kShowTombs, Tr("Tombes sur la minimap"),
      Tr("Marque l'endroit exact où chaque MVP connu de ton groupe est tombé, sur la carte "
         "concernée. Doré quand l'instant du retour a été mérité au Convex Mirror, orangé quand "
         "on n'a qu'une fenêtre.\n\nIndépendant du marqueur « Boss (Convex Mirror) » de la "
         "minimap, qui vient du client."),
      hunt.show_tombs, off));
  rows_.push_back(OptionRowView{
      .id = MvpTrackerRowId(MvpTrackerRow::kLinesNote),
      .label = Tr("Glisser une ligne du carnet par sa poignée, à gauche, et la lâcher HORS de la "
                  "fenêtre : elle reste à l'écran et survit à la fermeture du carnet. Les lignes "
                  "s'aimantent entre elles. Clic droit sur une ligne pour la remettre au carnet."),
      .kind = OptionKind::kNote,
      .heading = Tr("Lignes détachées"),
      .disabled = off});
  rows_.push_back(Toggle(
      MvpTrackerRow::kLineSprite, Tr("Sprite dans les lignes détachées"),
      Tr("Réglage à part de celui de la table, et c'est voulu : une ligne posée sur le décor du "
         "jeu cherche la discrétion, là où la table cherche à faire reconnaître un monstre d'un "
         "coup d'œil."),
      hunt.look.line_show_sprite, off));
  rows_.push_back(Color(MvpTrackerRow::kLineText, Tr("Ligne — texte"), nullptr,
                        hunt.look.line_text_color, off));
  rows_.push_back(Color(
      MvpTrackerRow::kLineBackground, Tr("Ligne — fond"),
      Tr("Le canal alpha de chaque couleur EST son opacité — les deux sont réglables séparément, "
         "précisément pour obtenir ce qu'on veut d'une ligne discrète : un fond presque "
         "transparent sous un texte resté lisible."),
      hunt.look.line_background_color, off));
  // ⚠ ABSENTE SANS LIGNE : il n'y a alors rien à remettre.
  if (line_count_ > 0) {
    rows_.push_back(OptionRowView{.id = MvpTrackerRowId(MvpTrackerRow::kPutBackAll),
                                  .label = put_back_label_.c_str(),
                                  .kind = OptionKind::kAction,
                                  .confirm = put_back_confirm_.c_str(),
                                  .disabled = off});
  }
}

bool MvpTrackerOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kMvpTrackerRowBase ||
      asked.changed >= kMvpTrackerRowBase + static_cast<int>(MvpTrackerRow::kCount)) {
    return false;
  }
  switch (static_cast<MvpTrackerRow>(asked.changed - kMvpTrackerRowBase)) {
    case MvpTrackerRow::kEnabled:
      settings_.enabled = asked.on;
      break;
    case MvpTrackerRow::kAlertSound:
      settings_.alert_sound = asked.on;
      break;
    case MvpTrackerRow::kAlertLead:
      settings_.alert_lead_min = BoundedLead(static_cast<int>(std::lround(asked.value)));
      break;
    case MvpTrackerRow::kShowSprites:
      settings_.look.show_sprites = asked.on;
      break;
    case MvpTrackerRow::kAnimateSprites:
      settings_.look.animate_sprites = asked.on;
      break;
    case MvpTrackerRow::kShowFilter:
      settings_.look.show_filter = asked.on;
      break;
    case MvpTrackerRow::kShowTombs:
      settings_.show_tombs = asked.on;
      break;
    case MvpTrackerRow::kLineSprite:
      settings_.look.line_show_sprite = asked.on;
      break;
    case MvpTrackerRow::kLineText:
      settings_.look.line_text_color = asked.color;
      break;
    case MvpTrackerRow::kLineBackground:
      settings_.look.line_background_color = asked.color;
      break;
    case MvpTrackerRow::kShowDelay:
      settings_.look.show_delay = asked.on;
      break;
    case MvpTrackerRow::kAlertTone:
      settings_.alert_tone = static_cast<int>(MvpAlertToneOf(asked.chosen));
      alert_failure_reported_.clear();
      alert_trouble_ = MvpAlertTrouble::kNone;
      break;
    case MvpTrackerRow::kAlertFile:
      settings_.alert_wav = asked.text;
      alert_failure_reported_.clear();
      alert_trouble_ = MvpAlertTrouble::kNone;
      break;
    case MvpTrackerRow::kAlertListen:
    case MvpTrackerRow::kAlertTrouble:
    case MvpTrackerRow::kOpen:
    case MvpTrackerRow::kLinesNote:
    case MvpTrackerRow::kPutBackAll:
    case MvpTrackerRow::kCount:
      break;
  }
  return true;
}

}  // namespace moonclient
