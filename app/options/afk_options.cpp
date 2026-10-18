// LA PAGE DE L'ÉCRAN DE VEILLE : ses clés, ses lignes, et ce qu'une ligne
// change.
//
// 🔴 L'ORDRE ET LES LIBELLÉS SONT GRAVÉS : un joueur qui les connaît les cherche
// à la même place. Là où une infobulle précise ce que mesure un réglage, la
// ligne dit pourquoi.
//
// ⚠ TOUT Y EST VIVANT : la veille relit ces réglages à chaque image, donc une
// valeur touchée pendant un aperçu se voit immédiatement.

#include "options/afk_options.h"

#include "ro/core/i18n.h"

#include "options/option_row_kinds.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier. ⚠ GRAVÉES : les changer rend à tous les joueurs les
// réglages d'origine, sans que rien ne le dise.
constexpr const char* kEnabledKey = "afk_active";
constexpr const char* kDelayKey = "afk_delay";
constexpr const char* kWakeOnKey = "afk_wake";
constexpr const char* kSpinKey = "afk_rotation";
constexpr const char* kTiltKey = "afk_dive";
constexpr const char* kZoomKey = "afk_distance";
constexpr const char* kEaseKey = "afk_transition";
constexpr const char* kHideUiKey = "afk_hide_interface";
constexpr const char* kHideCursorKey = "afk_hide_cursor";
constexpr const char* kVignetteKey = "afk_thumbnail";
constexpr const char* kDesaturateKey = "afk_black_and_white";
constexpr const char* kGrainKey = "afk_grain";
constexpr const char* kShowClockKey = "afk_clock";
constexpr const char* kShowAwayKey = "afk_duration_absence";
constexpr const char* kClockAnchorKey = "afk_clock_position";
constexpr const char* kClockMarginKey = "afk_clock_margin";
constexpr const char* kClockScaleKey = "afk_clock_size";
constexpr const char* kClockColorKey = "afk_clock_color";
constexpr const char* kClockShadowKey = "afk_clock_shadow";
constexpr const char* kAnnounceKey = "afk_announce";
constexpr const char* kAnnounceDelayKey = "afk_announce_delay";
constexpr const char* kAnnounceSleepKey = "afk_announce_sleep";
constexpr const char* kAnnounceTagKey = "afk_announce_label";

}  // namespace

void AfkOptions::Declare(SettingsFile& into) {
  into.Declare(kEnabledKey, &settings_.enabled);
  into.Declare(kDelayKey, &settings_.delay_s);
  into.Declare(kWakeOnKey, &settings_.wake_on);
  into.Declare(kSpinKey, &settings_.spin_deg_s);
  into.Declare(kTiltKey, &settings_.tilt_deg);
  into.Declare(kZoomKey, &settings_.zoom_factor);
  into.Declare(kEaseKey, &settings_.ease_s);
  into.Declare(kHideUiKey, &settings_.hide_ui);
  into.Declare(kHideCursorKey, &settings_.hide_cursor);
  into.Declare(kVignetteKey, &settings_.vignette);
  into.Declare(kDesaturateKey, &settings_.desaturate);
  into.Declare(kGrainKey, &settings_.grain);
  into.Declare(kShowClockKey, &settings_.show_clock);
  into.Declare(kShowAwayKey, &settings_.show_away);
  into.Declare(kClockAnchorKey, &settings_.clock_anchor);
  into.Declare(kClockMarginKey, &settings_.clock_margin);
  into.Declare(kClockScaleKey, &settings_.clock_scale);
  into.Declare(kClockColorKey, &settings_.clock_color);
  into.Declare(kClockShadowKey, &settings_.clock_shadow);
  into.Declare(kAnnounceKey, &settings_.announce);
  into.Declare(kAnnounceDelayKey, &settings_.announce_delay_s);
  into.Declare(kAnnounceSleepKey, &settings_.announce_zzz);
  into.Declare(kAnnounceTagKey, &settings_.announce_tag);
}

void AfkOptions::Compose() {
  rows_.clear();

  rows_.push_back(Toggle(GameOption::kAfkEnabled, Tr("Activer l'écran de veille"),
                         Tr("Passé un moment sans toucher au clavier ni à la souris, "
                            "l'interface s'efface et la caméra recule pour tourner lentement "
                            "autour de ton personnage. Le premier geste rend tout comme "
                            "c'était, et le clic qui te réveille ne part pas dans le jeu : le "
                            "décor a changé d'angle, il tomberait ailleurs que là où tu crois "
                            "viser."),
                         nullptr, settings_.enabled));

  // ⚠ CE QUI SUIT EST GRISÉ TANT QUE LA VEILLE EST DÉCOCHÉE, et grisé plutôt
  // qu'absent : un réglage qui disparaît fait sauter la mise en page, et le
  // joueur cherche ce qu'il vient de perdre.
  const bool off = !settings_.enabled;

  rows_.push_back(Measure(GameOption::kAfkDelay, Tr("Délai"),
                          Tr("Temps sans toucher au clavier ni à la souris avant que la veille "
                             "ne commence."),
                          static_cast<float>(settings_.delay_s),
                          static_cast<float>(kLeastAfkDelaySeconds),
                          static_cast<float>(kMostAfkDelaySeconds), "%.0f s"));

  wake_names_.clear();
  wake_names_.push_back(Tr("Clavier et souris"));
  wake_names_.push_back(Tr("Clavier seulement"));
  wake_names_.push_back(Tr("Souris seulement"));
  rows_.push_back(Choice(GameOption::kAfkWakeOn, Tr("Réveil"),
                         Tr("Ce qui met fin à la veille. « Clavier seulement » évite qu'une "
                            "souris frôlée ne la réveille sans arrêt. Ce réglage ne change PAS "
                            "ce qui repousse la mise en veille : tant que tu joues, tout "
                            "compte. Et ce qui n'a pas le droit de te réveiller n'agit pas non "
                            "plus dans le jeu — sinon une touche partirait sur un décor que tu "
                            "ne vois pas."),
                         wake_names_, settings_.wake_on));

  rows_.push_back(Measure(GameOption::kAfkSpin, Tr("Rotation"),
                          Tr("Vitesse de l'orbite autour du personnage. Une valeur négative "
                             "tourne dans l'autre sens ; zéro fige la vue, ce qui donne un "
                             "simple plan large."),
                          settings_.spin_deg_s, -kMostAfkSpinDegreesPerSecond,
                          kMostAfkSpinDegreesPerSecond, "%.1f °/s"));
  rows_.back().heading = Tr("Caméra");

  // 🔴 « CINQUANTE » ET NON « QUARANTE-CINQ ». Notre vue de jeu est mesurée à
  // cinquante — par superposition d'images contre une capture, voir
  // `ro/render/camera.h` —, et c'est le nombre que le joueur reconnaîtra sur ce
  // curseur. Le défaut suit le même repère : voir `kAfkTiltBelowGameDegrees`.
  rows_.push_back(Measure(GameOption::kAfkTilt, Tr("Plongée"),
                          Tr("Hauteur du regard au-dessus de l'horizon. Le jeu se joue à 50° ; "
                             "plus haut, la caméra domine la scène, et tout en haut elle est "
                             "presque à la verticale."),
                          settings_.tilt_deg, kLeastAfkTiltDegrees, kMostAfkTiltDegrees, "%.0f °"));

  // 🔴 LE RECUL NE COMPTE PAS DEPUIS LA MOLETTE : compté depuis la distance où
  // elle se trouve au moment de la bascule, la veille ne cadrerait jamais deux
  // fois pareil. Il part du recul de JEU, qui ne bouge pas, et l'infobulle dit
  // ce qu'il mesure vraiment.
  rows_.push_back(Measure(GameOption::kAfkZoom, Tr("Recul"),
                          Tr("De combien la caméra s'éloigne, en multiples du recul de jeu. "
                             "C'est une distance FIXE : la veille cadre pareil que tu te sois "
                             "rapproché ou éloigné juste avant. Un la pose exactement au recul "
                             "de jeu."),
                          settings_.zoom_factor, kLeastAfkZoom, kMostAfkZoom, "%.2f x"));

  rows_.push_back(Measure(GameOption::kAfkEase, Tr("Transition"),
                          Tr("Durée du basculement en veille. Le retour, lui, va trois fois "
                             "plus vite : il répond à un geste que tu viens de faire."),
                          settings_.ease_s, kLeastAfkEaseSeconds, kMostAfkEaseSeconds, "%.1f s"));

  // 🔴 « TOUTE L'INTERFACE » EST NOMMÉE POUR CE CLIENT-CI : il n'a qu'un jeu de
  // fenêtres, et l'infobulle dit ce qui s'efface.
  rows_.push_back(Toggle(GameOption::kAfkHideUi, Tr("Masquer toute l'interface"),
                         Tr("Les fenêtres, les noms au-dessus des personnages, les barres de "
                            "vie et les bulles de parole. Rien n'est fermé pour autant : tout "
                            "ce qui était ouvert l'est encore au réveil."),
                         nullptr, settings_.hide_ui));
  rows_.back().heading = Tr("Image");

  rows_.push_back(Toggle(GameOption::kAfkHideCursor, Tr("Masquer le curseur"),
                         Tr("La flèche de la souris disparaît aussi. Elle revient dès le "
                            "premier geste, en même temps que l'interface."),
                         nullptr, settings_.hide_cursor));

  rows_.push_back(Measure(GameOption::kAfkVignette, Tr("Vignette"), nullptr, settings_.vignette,
                          0.0F, 1.0F, "%.2f"));
  rows_.push_back(Measure(GameOption::kAfkDesaturate, Tr("Noir et blanc"), nullptr,
                          settings_.desaturate, 0.0F, 1.0F, "%.2f"));
  rows_.push_back(
      Measure(GameOption::kAfkGrain, Tr("Grain"), nullptr, settings_.grain, 0.0F, 1.0F, "%.2f"));

  rows_.push_back(Toggle(GameOption::kAfkShowClock, Tr("Afficher l'heure"), nullptr, nullptr,
                         settings_.show_clock));
  rows_.back().heading = Tr("Horloge");

  // ⚠ CE QUI SUIT DÉPEND DE L'HORLOGE, et non de la veille : un second niveau
  // de grisé. Les rangs se relèvent plutôt que de se compter à la main — une
  // ligne insérée au milieu ferait sinon griser sa voisine.
  const bool no_clock = !settings_.show_clock;
  const std::size_t clock_first = rows_.size();

  rows_.push_back(Toggle(GameOption::kAfkShowAway, Tr("Afficher la durée d'absence"), nullptr,
                         nullptr, settings_.show_away));

  anchor_names_.clear();
  anchor_names_.push_back(Tr("Haut-gauche"));
  anchor_names_.push_back(Tr("Haut-centre"));
  anchor_names_.push_back(Tr("Haut-droite"));
  anchor_names_.push_back(Tr("Milieu-gauche"));
  anchor_names_.push_back(Tr("Centre"));
  anchor_names_.push_back(Tr("Milieu-droite"));
  anchor_names_.push_back(Tr("Bas-gauche"));
  anchor_names_.push_back(Tr("Bas-centre"));
  anchor_names_.push_back(Tr("Bas-droite"));
  rows_.push_back(Choice(GameOption::kAfkClockAnchor, Tr("Position"), nullptr, anchor_names_,
                         settings_.clock_anchor));

  rows_.push_back(Measure(GameOption::kAfkClockMargin, Tr("Marge"),
                          Tr("Distance au bord de l'écran. Sans effet sur une position centrée, "
                             "qui n'a pas de bord dont s'écarter."),
                          static_cast<float>(settings_.clock_margin), 0.0F,
                          static_cast<float>(ro::ui::kMostAfkClockMargin), "%.0f px"));

  rows_.push_back(Measure(GameOption::kAfkClockScale, Tr("Taille du texte"),
                          Tr("Taille de l'heure. La ligne d'absence suit, en plus petit."),
                          settings_.clock_scale, ro::ui::kLeastAfkClockScale,
                          ro::ui::kMostAfkClockScale, "%.1fx"));

  // ⚠ AVANT L'OMBRE ET APRÈS LA TAILLE : le nuancier s'ouvre vers le bas et ne
  // recouvre ainsi qu'une case.
  rows_.push_back(
      Swatch(GameOption::kAfkClockColor, Tr("Couleur du texte"), nullptr, settings_.clock_color));

  rows_.push_back(Toggle(GameOption::kAfkClockShadow, Tr("Ombrage du texte"),
                         Tr("Une ombre d'un pixel sous le texte. C'est elle qui le garde "
                            "lisible quand la caméra passe devant un décor clair."),
                         nullptr, settings_.clock_shadow));
  const std::size_t clock_last = rows_.size() - 1;

  rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kAfkTryNow),
                                .label = Tr("Essayer maintenant"),
                                .help = Tr("Entre en veille tout de suite, sans attendre le "
                                           "délai. Bouge la souris pour en sortir."),
                                .kind = OptionKind::kAction});

  // ⚠ LE GRISÉ SE POSE ICI, D'UN SEUL GESTE, sur tout ce qui suit la case du
  // haut : vingt lignes qui en dépendent, c'est vingt occasions d'en oublier
  // une — et une ligne restée vive sous une case décochée règle quelque chose
  // qui ne se produira pas.
  for (std::size_t rank = 1; rank < rows_.size(); ++rank) {
    rows_[rank].disabled = off || (no_clock && clock_first <= rank && rank <= clock_last);
  }

  // ── PRÉVENIR L'ENTOURAGE ──────────────────────────────────────────────────
  //
  // 🔴 HORS DU BLOC GRISÉ, et ce n'est pas un oubli : cette section-ci ne dépend
  // pas de la veille. Se signaler absent et se regarder tourner sont deux envies
  // distinctes, et la première n'a pas à se payer de la seconde — qui coûte au
  // joueur son interface et sa caméra.
  rows_.push_back(
      Toggle(GameOption::kAfkAnnounce, Tr("Signaler mon absence aux autres joueurs"),
             Tr("Passé le délai ci-dessous, les autres joueurs voient que tu n'es plus là. "
                "Rien ne change pour toi : tu bouges, tu attaques et tu te défends exactement "
                "comme d'habitude — c'est un signe donné aux autres, pas une altération. "
                "Indépendant de l'écran de veille : tu peux prévenir sans que ta caméra ne "
                "bouge, comme tu peux te regarder tourner sans le dire à personne."),
             nullptr, settings_.announce));
  rows_.back().heading = Tr("Prévenir mon entourage");

  const bool mute = !settings_.announce;

  rows_.push_back(Measure(GameOption::kAfkAnnounceDelay, Tr("Délai d'absence"),
                          Tr("Temps sans toucher au clavier ni à la souris avant que ton "
                             "absence ne soit annoncée. Le premier geste la retire aussitôt."),
                          static_cast<float>(settings_.announce_delay_s),
                          static_cast<float>(kLeastAfkDelaySeconds),
                          static_cast<float>(kMostAfkAnnounceSeconds), "%.0f s"));
  rows_.back().disabled = mute;

  rows_.push_back(
      Toggle(GameOption::kAfkAnnounceSleepSign, Tr("Effet de sommeil (zzz)"),
             Tr("Le sommeil du jeu flotte au-dessus de ton personnage — le même « zzz » que "
                "sur un monstre endormi. Tu le vois toi aussi : c'est ainsi que tu sais que "
                "ton absence est bien annoncée. Purement visuel, pour toi comme pour les "
                "autres : tu bouges, tu attaques et tu te défends exactement comme "
                "d'habitude."),
             nullptr, settings_.announce_zzz));
  rows_.back().disabled = mute;

  rows_.push_back(
      Toggle(GameOption::kAfkAnnounceTagSign, Tr("Ajouter « [AFK] » devant mon nom"),
             Tr("Ton pseudo s'affiche « [AFK] Nom » dans l'étiquette au-dessus de ton "
                "personnage. Le champ du jeu ne fait que vingt-trois caractères : un pseudo "
                "de plus de dix-sept perd sa fin le temps de l'absence, et la retrouve au "
                "retour."),
             nullptr, settings_.announce_tag));
  rows_.back().disabled = mute;
}

void AfkOptions::Change(GameOption which, const ro::ui::OptionsWindowAction& asked,
                        const OptionTargets& to) {
  switch (which) {
    case GameOption::kAfkEnabled:
      settings_.enabled = asked.on;
      break;
    case GameOption::kAfkDelay:
      settings_.delay_s = static_cast<int>(asked.value);
      break;
    case GameOption::kAfkWakeOn:
      settings_.wake_on = asked.chosen;
      break;
    case GameOption::kAfkSpin:
      settings_.spin_deg_s = asked.value;
      break;
    case GameOption::kAfkTilt:
      settings_.tilt_deg = asked.value;
      break;
    case GameOption::kAfkZoom:
      settings_.zoom_factor = asked.value;
      break;
    case GameOption::kAfkEase:
      settings_.ease_s = asked.value;
      break;
    case GameOption::kAfkHideUi:
      settings_.hide_ui = asked.on;
      break;
    case GameOption::kAfkHideCursor:
      settings_.hide_cursor = asked.on;
      break;
    case GameOption::kAfkVignette:
      settings_.vignette = asked.value;
      break;
    case GameOption::kAfkDesaturate:
      settings_.desaturate = asked.value;
      break;
    case GameOption::kAfkGrain:
      settings_.grain = asked.value;
      break;
    case GameOption::kAfkShowClock:
      settings_.show_clock = asked.on;
      break;
    case GameOption::kAfkShowAway:
      settings_.show_away = asked.on;
      break;
    case GameOption::kAfkClockAnchor:
      settings_.clock_anchor = asked.chosen;
      break;
    case GameOption::kAfkClockMargin:
      settings_.clock_margin = static_cast<int>(asked.value);
      break;
    case GameOption::kAfkClockScale:
      settings_.clock_scale = asked.value;
      break;
    case GameOption::kAfkClockColor:
      settings_.clock_color = asked.color;
      break;
    case GameOption::kAfkClockShadow:
      settings_.clock_shadow = asked.on;
      break;
    case GameOption::kAfkTryNow:
      // ⚠ UNE DEMANDE, PAS UN GESTE : voir `OptionTargets::afk_try_now`. Ces
      // réglages n'ont ni caméra ni compteur d'inactivité à offrir.
      if (to.afk_try_now != nullptr) {
        *to.afk_try_now = true;
      }
      break;
    case GameOption::kAfkAnnounce:
      settings_.announce = asked.on;
      break;
    case GameOption::kAfkAnnounceDelay:
      settings_.announce_delay_s = static_cast<int>(asked.value);
      break;
    case GameOption::kAfkAnnounceSleepSign:
      settings_.announce_zzz = asked.on;
      break;
    case GameOption::kAfkAnnounceTagSign:
      settings_.announce_tag = asked.on;
      break;
    default:
      break;
  }
}

}  // namespace moonclient
