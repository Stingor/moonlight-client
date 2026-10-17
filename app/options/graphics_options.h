#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/render/model_renderer.h"
#include "ro/render/post_fx.h"
#include "ro/render/scene_post_process.h"
#include "ro/render/vanish_fade.h"
#include "ro/ui/options_window.h"

#include "game/settings_file.h"

namespace moonclient {

// L'écart d'usine de la disparition des cadavres sous la durée du client.
inline constexpr std::uint32_t kCorpseFadeShortenedMs = 100;

// LES LIGNES DE LA PAGE portent un identifiant COMPOSÉ, hors de l'énumération
// des réglages, comme celles de la cible : voir la fiche 0038. ⚠ LA PLAGE SUIT
// CELLES DU MENU DU CLIC DROIT (5200 et 5300).
inline constexpr int kGraphicsRowBase = 5400;

// Le rang de chaque ligne dans la plage de la page. ⚠ RIEN N'EN EST ÉCRIT : ce
// sont les CLÉS qui vont au fichier, et ces rangs peuvent se réordonner.
enum class GraphicsRow : int {
  kLightmaps = 0,
  kColdLight,
  kSunShadows,
  kEffectLights,
  kRoundedGround,
  kDayNightCycle,
  kFixedSky,
  kFixedSkyHour,
  kFog,
  kDecorVeil,
  kScreenQuake,
  kSummonTranslucent,
  kCorpseFade,
  kAppearFade,
  kLeaveFade,
  kSkillGestureFloor,
  kCastSpansBar,
  kMsaa,
  kRenderScale,
  kDecorFilter,
  kPostFx,
  kPreset,
  kBrightness,
  kContrast,
  kGamma,
  kSaturation,
  kTemperature,
  kFilter,
  kVignette,
  kGrain,
  kAberration,
  kSharpen,
  kFxaa,
  kFxaaStrength,
  kReset,
  kCount,
};

// L'identifiant que la fenêtre promène pour cette ligne : une commande qui
// bascule un réglage de la page passe par lui, comme la case.
[[nodiscard]] constexpr int GraphicsRowId(GraphicsRow row) {
  return kGraphicsRowBase + static_cast<int>(row);
}

// LES ÉCHANTILLONS PAR PIXEL qu'on propose à l'anticrénelage matériel. Zéro :
// aucun. ⚠ UNE LISTE ET NON UN CURSEUR : les cartes graphiques n'acceptent que
// des puissances de deux.
inline constexpr std::array<int, 4> kMsaaSamples = {0, 2, 4, 8};

// LES ÉCHELLES DE RENDU proposées, en pour cent de la fenêtre.
inline constexpr std::array<int, 6> kRenderScalePercents = {50, 75, 100, 125, 150, 200};

// LES FILTRAGES DU DÉCOR 3D, par rang de la liste : net (aucun mipmap), puis
// trilinéaire, puis anisotrope à tant d'échantillons. ⚠ LE RANG EST ÉCRIT DANS
// LE FICHIER : ne pas réordonner.
inline constexpr std::array<int, 6> kDecorAnisotropy = {0, 1, 2, 4, 8, 16};

// LA PAGE « GRAPHICS » : ce que le moteur fait de l'image — la lumière de la
// carte, la cible où le monde se peint, le post-traitement.
//
// ⚠ UNE CLASSE À PART, comme la veille et la cible : `GameOptions` touche ses
// huit cents lignes. Le moteur relit ces réglages à chaque image ; aucun n'a
// rien à pousser au moment où il change.
class GraphicsOptions {
 public:
  // Les ombres et la lumière que la carte a peintes, sur le sol comme sur les
  // corps. `/lightmap`, allumée par défaut comme chez le client.
  [[nodiscard]] bool lightmaps() const noexcept { return lightmaps_; }
  // La lumière posée, rouge et bleu échangés. Voir `TerrainStages::cold_light`.
  [[nodiscard]] bool cold_light() const noexcept { return cold_light_; }
  // LES OMBRES EN TEMPS RÉEL, calculées à chaque image, seulement sous
  // `lightmaps` : celle du soleil, à la place de celle que la carte a cuite, sur
  // les cartes en plein air ; et celles des lumières ponctuelles de l'édition de
  // carte, sur toute carte. Éteintes par défaut : le client d'origine n'en a pas.
  [[nodiscard]] bool sun_shadows() const noexcept { return sun_shadows_; }
  [[nodiscard]] bool real_time_shadows() const noexcept { return lightmaps_ && sun_shadows_; }
  // LES SORTS ÉCLAIRENT : un effet joué porte une lumière ponctuelle, tirée de
  // ses textures claires. Voir docs/adr/0120.
  [[nodiscard]] bool effect_lights() const noexcept { return effect_lights_; }
  // LE SOL ARRONDI : chaque cube du sol découpé en sous-carreaux posés sur une
  // surface lisse, les falaises gardées. Voir docs/adr/0122.
  [[nodiscard]] bool rounded_ground() const noexcept { return rounded_ground_; }
  // LE CYCLE JOUR ET NUIT : le soleil, puis la lune, suivent l'heure qu'il fait
  // sur le serveur, en plein air. Éteint par défaut : le client d'origine garde
  // la lumière de la carte à toute heure.
  [[nodiscard]] bool day_night_cycle() const noexcept { return day_night_cycle_; }
  // L'HEURE FIXE DU JOUEUR, sous le cycle : le ciel s'y tient au lieu de suivre
  // l'heure du serveur. Rien quand le ciel suit le serveur. Midi tant qu'on
  // n'en a pas choisi d'autre.
  static constexpr float kNoonSkyHour = 12.0F;
  [[nodiscard]] std::optional<float> fixed_sky_hour() const noexcept {
    return fixed_sky_ ? std::optional<float>(fixed_sky_hour_) : std::nullopt;
  }
  // Le brouillard des cartes qui en déclarent un. `/fog`, allumé par défaut
  // comme chez le client.
  [[nodiscard]] bool fog() const noexcept { return fog_; }
  // LES TREMBLEMENTS D'ÉCRAN que des sorts et des effets demandent. `/quake`,
  // allumés par défaut comme chez le client : éteints, la caméra ne bouge plus.
  [[nodiscard]] bool screen_quake() const noexcept { return screen_quake_; }
  // Le décor qui se tient entre la caméra et le personnage s'efface. Éteint par
  // défaut, comme le client d'origine : voir `DecorVeilView`.
  [[nodiscard]] bool decor_veil() const noexcept { return decor_veil_; }
  // Les invocations peintes presque transparentes : aujourd'hui le faucon.
  // Éteint par défaut, comme chez le client.
  [[nodiscard]] bool summon_translucent() const noexcept { return summon_translucent_; }
  // Ce que dure le cadavre d'une créature, BORNÉ : le fichier se modifie à la
  // main. Le défaut est celui du client.
  [[nodiscard]] std::uint32_t corpse_fade_ms() const noexcept;
  // Le fondu d'une créature que le serveur fait apparaître, et celui d'un
  // acteur qui sort de vue, BORNÉS. Ceux du client par défaut ; zéro, aucun.
  [[nodiscard]] std::uint32_t appear_fade_ms() const noexcept;
  [[nodiscard]] std::uint32_t leave_fade_ms() const noexcept;
  // Ce que le geste d'un coup de compétence dure AU MOINS à l'écran, BORNÉ.
  // Zéro, le défaut : la cadence du client, que la vitesse d'attaque accélère.
  [[nodiscard]] std::uint32_t skill_gesture_floor_ms() const noexcept;
  // L'incantation ordinaire déroule son geste sur toute sa barre, au lieu d'en
  // tenir la première image. Éteint par défaut, comme le client.
  [[nodiscard]] bool cast_spans_bar() const noexcept { return cast_spans_bar_; }
  // Le post-traitement, BORNÉ : le fichier se modifie à la main.
  [[nodiscard]] ro::render::PostFxSettings post_fx() const { return ro::render::Clamped(post_fx_); }
  // Où le monde se peint : échantillons et échelle, bornés à ce qu'on propose.
  [[nodiscard]] ro::render::SceneTargetLook scene_target() const;
  // Le filtrage des textures du décor 3D. Le sol n'en reçoit jamais : voir
  // la fiche 0008.
  [[nodiscard]] ro::render::ModelTextureFilter decor_filter() const;

  // Déclare les clés au fichier. ⚠ LA VALEUR DU MEMBRE EST LE DÉFAUT : il n'y
  // en a pas d'autre source.
  void Declare(SettingsFile& into);

  // Refait les lignes, à chaque image où le panneau est ouvert.
  void Compose();

  [[nodiscard]] std::span<const ro::ui::OptionRowView> rows() const noexcept { return rows_; }

  // Écrit ce que la ligne a changé. Rend faux quand la ligne n'est pas de cette
  // page : l'aiguillage continue alors ailleurs.
  bool Change(const ro::ui::OptionsWindowAction& asked);

  // `/lightmap` : bascule les ombres, et rend leur nouvel état.
  bool ToggleLightmaps();
  // `/fog` : bascule le brouillard, et rend son nouvel état.
  bool ToggleFog();

 private:
  void ComposePostFx();

  bool lightmaps_ = true;
  bool cold_light_ = false;
  bool sun_shadows_ = false;
  bool effect_lights_ = true;
  bool rounded_ground_ = true;
  bool day_night_cycle_ = false;
  bool fixed_sky_ = false;
  float fixed_sky_hour_ = kNoonSkyHour;
  bool fog_ = true;
  bool screen_quake_ = true;
  bool decor_veil_ = false;
  bool summon_translucent_ = false;
  // ⚠ CENT MILLISECONDES SOUS LE CLIENT : le réglage d'usine que le joueur a
  // choisi.
  std::uint32_t corpse_fade_ms_ = ro::render::kClientCorpseFadeMs - kCorpseFadeShortenedMs;
  std::uint32_t appear_fade_ms_ = ro::render::kClientAppearFadeMs;
  std::uint32_t leave_fade_ms_ = ro::render::kClientLeaveFadeMs;
  std::uint32_t skill_gesture_floor_ms_ = 0;
  bool cast_spans_bar_ = false;
  int msaa_samples_ = 0;
  int render_scale_percent_ = 100;
  int decor_filter_rank_ = 0;
  ro::render::PostFxSettings post_fx_;
  std::vector<ro::ui::OptionRowView> rows_;
  // Les listes déroulantes. ⚠ REFAITES À CHAQUE COMPOSITION : un libellé doit
  // vivre au moins jusqu'au dessin, et la langue peut changer entre deux.
  std::vector<const char*> msaa_names_;
  std::vector<std::string> scale_labels_;
  std::vector<const char*> scale_names_;
  std::vector<const char*> decor_names_;
  std::vector<const char*> preset_names_;
  std::vector<const char*> filter_names_;
};

}  // namespace moonclient
