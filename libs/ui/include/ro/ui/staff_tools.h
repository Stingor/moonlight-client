#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/render/actor_pick.h"
#include "ro/render/sky_clock.h"
#include "ro/ui/quick_cast_look.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// L'ÉTABLI DU STAFF : les outils de travail, dans leur propre fenêtre.
//
// ⚠ CE NE SONT PAS DES RÉGLAGES DE CONFORT, et c'est ce qui les sort du panneau
// d'options : repeindre le sol pour une capture, lire un journal, mesurer ce que
// le client applique. On les ouvre pour s'en servir, puis on les referme. Noyés
// dans une liste de préférences, ils étaient longs à atteindre et faciles à
// ouvrir par accident.
//
// ⚠ ET ELLE SE PORTE HABILLÉE. Ce qui décide de l'habillage n'est pas l'usage
// mais l'ORIGINE : cette fenêtre est reprise de l'implémentation de référence,
// elle restera devant le staff, et une copie nue se reconnaîtrait au premier
// coup d'œil. Seules les fenêtres que NOUS avons inventées pour travailler —
// journal, inspecteur de trames — restent nues.

// LE SEUIL, et il n'est pas arbitraire : des groupes qui ne sont pas du staff
// ont un niveau non nul. C'est celui de l'implémentation de référence.
inline constexpr std::uint8_t kStaffMinGroupLevel = 80;

// LE DROIT DU STAFF : l'établi, le journal, l'inspecteur de trames, et les
// commandes qui partent au serveur — « @resetskill », « @allskills ».
//
// C'est le SERVEUR qui l'accorde, par le niveau de groupe du compte, et il ne
// vaut que TANT QUE LA LIAISON TIENT. 🔴 Une liaison perdue ne laisse personne
// pour l'accorder : le niveau retenu est celui d'une session finie, et ouvrir
// les outils à quiconque perd sa connexion les donnerait à tout le monde.
//
// ⚠ POUR LES COMMANDES, C'EST UN GATE D'AFFICHAGE, pas de permission : le
// serveur revalide chacune selon le groupe du compte. Il décide de ce qui se
// MONTRE — un bouton qu'on n'a pas le droit d'employer n'a rien à encombrer
// l'en-tête.
[[nodiscard]] constexpr bool StaffToolsAllowed(std::uint8_t group_level, bool online) noexcept {
  return online && group_level >= kStaffMinGroupLevel;
}

// CE QUE LA SESSION AUTORISE, porté par un type plutôt que par un booléen nu.
//
// ⚠ UN `bool` DE PLUS DANS UNE LONGUE SIGNATURE EST UN PIÈGE : il se confond
// avec le voisin, et l'inversion ne se voit ni à l'appel ni à la compilation.
// Un droit mérite son nom.
struct SessionRights {
  // Vrai quand les outils et les commandes du staff s'offrent. Voir
  // `StaffToolsAllowed`.
  bool staff_tools = false;
};

// LE SOL UNI : un aplat EXACT à la place du terrain, pour détourer un sprite ou
// un effet sur un fond neutre.
//
// 🔴 IL REMPLACE, IL NE TEINTE PAS. Ni texture, ni éclairage, ni ombre cuite :
// une couleur seulement modulée sortirait dégradée, et ne servirait plus de
// fond de capture. Voir `render::TerrainStages::flat_color`.
struct FlatGroundLook {
  bool on = false;
  // Noir opaque au départ, comme dans l'implémentation de référence.
  //
  // ⚠ L'ALPHA VOYAGE MAIS NE PEINT PAS : la passe du terrain est opaque, et le
  // sol n'en prend que les trois autres canaux. Il est gardé parce que le
  // sélecteur en montre un, et la fenêtre le dit sous lui.
  ImU32 color = IM_COL32(0, 0, 0, 255);

  // 🔴 ENGENDRÉE, ET C'EST TOUT L'INTÉRÊT. Ce réglage se persiste par RELEVÉ :
  // à chaque image on compare ce que la fenêtre montre à ce que le fichier
  // porte. Une comparaison écrite à la main oublierait le champ ajouté demain,
  // et l'oubli se lirait « ce réglage-là ne s'enregistre pas » — sans que rien,
  // ni le compilateur ni un essai, ne désigne la ligne fautive.
  friend bool operator==(const FlatGroundLook&, const FlatGroundLook&) = default;
};

// Les trois canaux que le sol prend, en 0..1 — ce que `render` attend.
[[nodiscard]] std::array<float, 3> FlatGroundColorOf(const FlatGroundLook& flat);

// LES ZONES CLIQUABLES : les montrer, et régler ce que le clic attrape.
//
// ⚠ CE SONT DES RÉGLAGES DE LA VRAIE DÉSIGNATION, pas du seul dessin : le
// plancher et la part réglés ici sont ceux que la souris emploie.
struct PickTuningLook {
  // Le contour des zones, leur famille, celle qu'un clic prendrait.
  bool boxes_shown = false;
  // LE PLANCHER DES ACTEURS, FORCÉ en pixels, par-dessus le diviseur.
  bool actor_floor_forced = false;
  int actor_floor_pixels = static_cast<int>(render::kActorPickFloorFactor);
  // L'EMPRISE D'UN ACTEUR AU NUMÉRO NÉGATIF, qu'aucun paquet ne peut viser : le
  // serveur ne connaît pas ce numéro et jette toute action envoyée vers lui. Sa
  // zone, rétrécie autour de son centre, cesse de voler le clic à la vraie
  // cible.
  bool shrink_negative_ids = true;
  int negative_id_percent = kDefaultNegativeIdPercent;

  static constexpr int kDefaultNegativeIdPercent = 25;

  friend bool operator==(const PickTuningLook&, const PickTuningLook&) = default;
};

// Ce que la désignation emploie à cette image, tiré du réglage et de la largeur
// de la fenêtre. Sans le droit de l'établi, le client tel quel — sauf le
// rétrécissement des numéros négatifs, qui est un correctif et non un outil.
struct PickTuning {
  float actor_floor_pixels = render::kActorPickFloorFactor;
  float ground_floor_pixels = render::kGroundPickFloorFactor;
  // La part qu'un acteur au numéro négatif garde de son emprise.
  float negative_id_share = 1.0F;
};
//
// 🔴 LE DIVISEUR DES PLANCHERS EST UN RÉGLAGE DU JOUEUR, et non de l'établi :
// c'est la « Précision du ciblage ». `floor_shift` divise par deux les
// planchers des trois familles autant de fois qu'il vaut — zéro pour le client
// tel quel. L'établi ne garde que le plancher FORCÉ.
[[nodiscard]] PickTuning PickTuningOf(const PickTuningLook& look, bool staff_tools,
                                      float viewport_width, int floor_shift);

// Ce que l'établi règle et qui survit au lancement. Une copie : elle descend
// avant le dessin et remonte après, voir `GameSettings::NoteStaffTools`.
struct StaffToolsLook {
  FlatGroundLook flat_ground;
  bool free_camera = false;
  bool camera_hud = false;
  // LES ZONES DE SON D'AMBIANCE, dessinées sur le monde. Voir `sound_zones.h`.
  bool sound_zones = false;
  // LES ZONES CLIQUABLES, et ce qui les règle. Voir `pick_boxes.h` et
  // `PickTuningOf`.
  PickTuningLook picking;
  // LA COURSE DES ASTRES, réglée sur ce client quand `sky_course_tuned` : elle
  // vaut pour l'heure forcée comme pour le cycle jour/nuit. Retenue, à la
  // différence de l'heure : elle ne change que sous la main du staff.
  bool sky_course_tuned = false;
  render::SkyCourse sky_course;

  // Engendrée : un champ ajouté entre de lui-même dans le relevé.
  friend bool operator==(const StaffToolsLook&, const StaffToolsLook&) = default;
};

// LE CIEL DE L'ÉTABLI : l'heure du soleil et de la lune, forcée sur CE client
// seulement. Voir `render::SkyLightAt`.
//
// ⚠ PAS DANS `look` : rien n'est retenu, et chaque lancement rend le ciel de la
// carte. Une heure qui tourne changerait aussi le fichier à chaque image.
struct StaffSkyClock {
  // LES VITESSES PROPOSÉES, en minutes du ciel par minute réelle : du temps réel
  // à cinq jours par minute. Une liste, et non un curseur : d'un bout à l'autre
  // il y a un facteur sept mille, et un curseur linéaire ne saurait plus
  // s'arrêter sur le temps réel.
  static constexpr std::array<int, 9> kMinutesPerMinute = {1, 2, 5, 10, 30, 60, 240, 1440, 7200};
  // Quatre heures par minute : une journée en six minutes.
  static constexpr int kDefaultSpeedRank = 6;

  bool forced = false;
  float hour = render::kNoonHour;
  // Le temps tourne tout seul, à la vitesse de rang `speed_rank`.
  bool running = false;
  int speed_rank = kDefaultSpeedRank;

  // Les heures du ciel qui passent par minute réelle, au rang choisi.
  [[nodiscard]] float HoursPerMinute() const noexcept;
};

// LA RETOUCHE D'UN MODÈLE DU DÉCOR, en direct : voir docs/adr/0117.
//
// L'établi règle ; c'est l'appelant qui pose la retouche dans sa table, repose
// le décor et enregistre. ⚠ PAS DANS `look` : rien n'est retenu d'un lancement
// à l'autre, sinon la table enregistrée.
struct StaffMapFixEditor {
  // La place réservée à la raison d'une retouche, zéro final compris.
  static constexpr std::size_t kWhyCapacity = 256;

  // Le prochain clic dans le monde désigne un modèle, au lieu d'agir.
  bool picking = false;
  // Le modèle désigné, par son rang dans le RSW de la carte.
  std::optional<std::size_t> model_index;
  // Ce que le staff règle, dans le repère du RSW.
  std::array<float, 3> position{};
  std::array<float, 3> rotation{};
  std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
  bool hidden = false;
  std::array<char, kWhyCapacity> why{};
  // Les gestes de l'image, que l'appelant consomme : rendre au modèle son
  // placement d'origine, et écrire la table.
  bool revert_asked = false;
  bool save_asked = false;

  // ── Posé par l'appelant avant le dessin ──
  // Le fichier du modèle désigné, en UTF-8, et sa position d'origine.
  std::string model_file;
  std::array<float, 3> origin{};
  // Les rangs que la table retouche sur cette carte.
  std::vector<std::size_t> fixed_models;
  // La table diffère du fichier enregistré.
  bool unsaved = false;
  // La dernière nouvelle : enregistré, refusé, et pourquoi.
  std::string status;
};

// Ce que le joueur a réglé, et qui doit survivre d'une image à la suivante.
struct StaffToolsState {
  bool open = false;
  StaffToolsLook look;
  // ⚠ UNE COPIE DES RÉGLAGES DE QUICK CAST, qui vivent dans le panneau
  // d'options : l'établi les montre aussi. Elle descend avant le dessin et
  // remonte après, comme `look`.
  QuickCastLook quick_cast;
  StaffSkyClock sky;
  StaffMapFixEditor map_fix;
  // LES BRUITAGES SONNENT-ILS ? Posé par l'appelant avant le dessin : l'établi
  // ne connaît pas l'audio.
  //
  // 🔴 LES ZONES DE SON EN DÉPENDENT : une source d'ambiance ne se place qu'en
  // écoutant, autour de l'auditeur, et le client n'écoute pas quand les
  // bruitages sont coupés. La case se grise alors, cochée ou non, plutôt que de
  // promettre un dessin qui ne viendra pas.
  bool sound_audible = true;

  // LA CONSOLE DE MISE AU POINT, allumée par sa case. ⚠ PAS DANS `look` : elle
  // n'est pas retenue, et chaque lancement la trouve éteinte.
  bool debug_console = false;
  // Ce qu'elle dit d'elle-même, posé par l'appelant avant le dessin : le port
  // écouté (zéro, aucun), un pont branché, la panne de l'écoute.
  int debug_console_port = 0;
  bool debug_console_connected = false;
  std::string debug_console_failure;
};

// LES ZONES DE SON SE DESSINENT-ELLES ? La case, ET les bruitages audibles.
// ⚠ UNE SEULE RÈGLE pour la composition et le dessin : deux conditions écrites
// à part finiraient par ne plus dire la même chose.
[[nodiscard]] bool SoundZonesShown(const StaffToolsState& state) noexcept;

// ⚠ LE TITRE PORTE TROIS DIÈSES : ce qui suit est l'IDENTITÉ, que la langue ne
// change pas ; ce qui précède est le titre montré. Sans cela, traduire
// l'interface rouvrirait la fenêtre ailleurs, sans sa taille ni sa place.
//
// « Staff Tools » ne se traduit pas — c'est le nom que le staff se donne — mais
// il passe quand même par le catalogue, comme tout littéral affiché.
inline constexpr const char* kStaffToolsTitle = core::TrText("Staff Tools");

// L'identité seule, pour qui retient l'ouverture de cette fenêtre.
inline constexpr const char* kStaffToolsId = "staff_tools";

// Ce que le staff a demandé à l'établi pendant l'image.
struct StaffToolsAction {
  // Le bouton « Journal » de la barre de titre : ouvrir ou refermer le
  // journal du programme. ⚠ L'ÉTABLI NE L'OUVRE PAS LUI-MÊME : la fenêtre
  // appartient à l'appelant.
  bool toggle_log = false;
  // Le bouton « Trames » : ouvrir ou refermer l'inspecteur des trames reçues.
  // Même règle : la fenêtre appartient à l'appelant.
  bool toggle_packets = false;
};

// Dessine l'établi. `allowed` est revérifié À CHAQUE IMAGE, jamais mémorisé : le
// niveau de groupe arrive au login et peut changer en cours de session. Un
// `open` hérité du fichier de réglages d'un compte devenu ordinaire n'ouvre donc
// rien — et n'est pas effacé pour autant, parce que le droit peut revenir.
[[nodiscard]] StaffToolsAction DrawStaffTools(StaffToolsState& state, const SkinArt& art,
                                              bool allowed);

}  // namespace ro::ui
