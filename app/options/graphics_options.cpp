#include "options/graphics_options.h"

#include <algorithm>
#include <cmath>

#include "ro/core/i18n.h"

#include "options/display_options.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::core::TrText;
using ro::render::PostFxPreset;
using ro::render::PostFxRange;
using ro::render::PostFxSettings;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

constexpr const char* kLightmapsKey = "shadows_of_the_map";
constexpr const char* kColdLightKey = "light_cold";
constexpr const char* kSunShadowsKey = "render_shadows_of_the_sun";
constexpr const char* kEffectLightsKey = "light_from_effects";
constexpr const char* kMapEffectsUnboundedKey = "effects_of_the_map_at_any_distance";
constexpr const char* kRoundedGroundKey = "ground_rounded";
constexpr const char* kDayNightCycleKey = "cycle_day_night";
constexpr const char* kFixedSkyKey = "cycle_day_night_fixed";
constexpr const char* kFixedSkyHourKey = "cycle_day_night_hour";

constexpr float kHoursPerDay = 24.0F;
constexpr const char* kFogKey = "fog";
constexpr const char* kScreenQuakeKey = "screen_quake";
constexpr const char* kDecorVeilKey = "scenery_veil_front_the_player";
constexpr const char* kSummonTranslucentKey = "summons_translucent";
constexpr const char* kElementGlowKey = "element_glow";
constexpr const char* kWandOrbKey = "enhanced_wand_shot";
constexpr const char* kCorpseFadeKey = "corpses_duration";
constexpr const char* kAppearFadeKey = "spawn_fade_ms";
constexpr const char* kLeaveFadeKey = "exit_of_view_fade_ms";
constexpr const char* kSkillGestureFloorKey = "gesture_of_skill_min_ms";
constexpr const char* kCastSpansBarKey = "cast_on_the_bar";

// Le plus long plancher proposé au geste d'une compétence : au-delà, le geste
// déborderait sur le coup suivant d'un personnage qui enchaîne.
constexpr std::uint32_t kLongestSkillGestureFloorMs = 1000;
constexpr const char* kMsaaKey = "render_msaa";
constexpr const char* kRenderScaleKey = "render_scale";
constexpr const char* kDecorFilterKey = "render_filtering_scenery";
constexpr const char* kFrameInterpolationKey = "render_frame_interpolation";
constexpr const char* kSpinBlurKey = "render_motion_blur";
constexpr const char* kSpinBlurStrengthKey = "render_motion_blur_strength";
constexpr const char* kPostFxKey = "post_fx_active";
constexpr const char* kBrightnessKey = "post_fx_brightness";
constexpr const char* kContrastKey = "post_fx_contrast";
constexpr const char* kGammaKey = "post_fx_gamma";
constexpr const char* kSaturationKey = "post_fx_saturation";
constexpr const char* kTemperatureKey = "post_fx_temperature";
constexpr const char* kFilterKey = "post_fx_filter";
constexpr const char* kVignetteKey = "post_fx_thumbnail";
constexpr const char* kGrainKey = "post_fx_grain";
constexpr const char* kAberrationKey = "post_fx_aberration";
constexpr const char* kSharpenKey = "post_fx_sharpness";
constexpr const char* kFxaaKey = "post_fx_fxaa";
constexpr const char* kFxaaStrengthKey = "post_fx_fxaa_forced";

// Le premier rang de la liste des préréglages dit qu'aucun ne correspond.
constexpr int kCustomPresetRank = 0;

constexpr int kPercent = 100;

// Le curseur du cadavre compte en SECONDES, le réglage en millisecondes : « 6,1 s »
// se lit mieux que « 6120 ms ».
constexpr float kMillisecondsPerSecond = 1000.0F;

int IdOf(GraphicsRow row) { return GraphicsRowId(row); }

OptionRowView Toggle(GraphicsRow row, const char* label, const char* help, const char* command,
                     bool on) {
  return OptionRowView{.id = IdOf(row),
                       .label = label,
                       .help = help,
                       .command = command,
                       .kind = OptionKind::kToggle,
                       .on = on};
}

OptionRowView Slider(GraphicsRow row, const char* label, float value, const PostFxRange& range) {
  return OptionRowView{.id = IdOf(row),
                       .label = label,
                       .kind = OptionKind::kSlider,
                       .value = value,
                       .lowest = range.lowest,
                       .highest = range.highest};
}

OptionRowView Choice(GraphicsRow row, const char* label, const char* help,
                     std::span<const char* const> choices, int chosen) {
  return OptionRowView{.id = IdOf(row),
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kChoice,
                       .chosen = chosen,
                       .choices = choices};
}

// Le rang d'une valeur dans une liste proposée : le sien, ou celui de la plus
// grande valeur qui ne la dépasse pas — un fichier trafiqué tombe sur la plus
// proche en dessous, jamais au-delà.
template <std::size_t kSize>
int RankIn(const std::array<int, kSize>& offered, int value) {
  int rank = 0;
  for (std::size_t index = 0; index < offered.size(); ++index) {
    if (offered[index] <= value) {
      rank = static_cast<int>(index);
    }
  }
  return rank;
}

// Deux réglages égaux champ par champ, l'interrupteur mis à part.
bool SameLook(const PostFxSettings& left, const PostFxSettings& right) {
  return left.brightness == right.brightness && left.contrast == right.contrast &&
         left.gamma == right.gamma && left.saturation == right.saturation &&
         left.temperature == right.temperature && left.filter == right.filter &&
         left.vignette == right.vignette && left.grain == right.grain &&
         left.aberration == right.aberration && left.sharpen == right.sharpen &&
         left.fxaa == right.fxaa && left.fxaa_strength == right.fxaa_strength;
}

// Le préréglage que les réglages suivent, en rang de la liste — après
// « Personnalisé » —, ou `kCustomPresetRank` s'ils n'en suivent aucun.
int PresetRankOf(const PostFxSettings& settings) {
  for (int preset = 0; preset < static_cast<int>(PostFxPreset::kCount); ++preset) {
    if (SameLook(settings, ro::render::PresetSettings(static_cast<PostFxPreset>(preset)))) {
      return preset + 1;
    }
  }
  return kCustomPresetRank;
}

}  // namespace

std::uint32_t GraphicsOptions::corpse_fade_ms() const noexcept {
  return std::clamp(corpse_fade_ms_, ro::render::kShortestCorpseFadeMs,
                    ro::render::kLongestCorpseFadeMs);
}

std::uint32_t GraphicsOptions::appear_fade_ms() const noexcept {
  return std::min(appear_fade_ms_, ro::render::kLongestAppearFadeMs);
}

std::uint32_t GraphicsOptions::leave_fade_ms() const noexcept {
  return std::min(leave_fade_ms_, ro::render::kLongestLeaveFadeMs);
}

std::uint32_t GraphicsOptions::skill_gesture_floor_ms() const noexcept {
  return std::min(skill_gesture_floor_ms_, kLongestSkillGestureFloorMs);
}

ro::render::SceneTargetLook GraphicsOptions::scene_target() const {
  const int percent = kRenderScalePercents[static_cast<std::size_t>(
      RankIn(kRenderScalePercents, render_scale_percent_))];
  return ro::render::SceneTargetLook{
      .samples = kMsaaSamples[static_cast<std::size_t>(RankIn(kMsaaSamples, msaa_samples_))],
      .render_scale = static_cast<float>(percent) / static_cast<float>(kPercent)};
}

ro::render::SpinBlurSettings GraphicsOptions::spin_blur() const {
  return ro::render::SpinBlurSettings{
      .enabled = spin_blur_.enabled,
      .strength = std::clamp(spin_blur_.strength, ro::render::kSpinBlurStrengthRange.lowest,
                             ro::render::kSpinBlurStrengthRange.highest)};
}

ro::render::ModelTextureFilter GraphicsOptions::decor_filter() const {
  const int rank = std::clamp(decor_filter_rank_, 0, static_cast<int>(kDecorAnisotropy.size()) - 1);
  const int anisotropy = kDecorAnisotropy[static_cast<std::size_t>(rank)];
  return ro::render::ModelTextureFilter{.mipmaps = anisotropy > 0,
                                        .anisotropy = std::max(anisotropy, 1)};
}

void GraphicsOptions::Declare(SettingsFile& into) {
  into.Declare(kLightmapsKey, &lightmaps_);
  into.Declare(kColdLightKey, &cold_light_);
  into.Declare(kEffectLightsKey, &effect_lights_);
  into.Declare(kMapEffectsUnboundedKey, &map_effects_unbounded_);
  into.Declare(kRoundedGroundKey, &rounded_ground_);
  into.Declare(kDayNightCycleKey, &day_night_cycle_);
  into.Declare(kFixedSkyKey, &fixed_sky_);
  into.Declare(kFixedSkyHourKey, &fixed_sky_hour_);
  into.Declare(kFogKey, &fog_);
  into.Declare(kScreenQuakeKey, &screen_quake_);
  into.Declare(kDecorVeilKey, &decor_veil_);
  into.Declare(kSummonTranslucentKey, &summon_translucent_);
  into.Declare(kElementGlowKey, &element_glow_);
  into.Declare(kWandOrbKey, &wand_orb_);
  into.Declare(kCorpseFadeKey, &corpse_fade_ms_);
  into.Declare(kAppearFadeKey, &appear_fade_ms_);
  into.Declare(kLeaveFadeKey, &leave_fade_ms_);
  into.Declare(kSkillGestureFloorKey, &skill_gesture_floor_ms_);
  into.Declare(kCastSpansBarKey, &cast_spans_bar_);
  {
    // CE QUE LA CARTE GRAPHIQUE PEUT PORTER TIENT À LA MACHINE.
    const SettingsFile::MachineScope machine(into);
    into.Declare(kMsaaKey, &msaa_samples_);
    into.Declare(kRenderScaleKey, &render_scale_percent_);
    into.Declare(kDecorFilterKey, &decor_filter_rank_);
    into.Declare(kSunShadowsKey, &sun_shadows_);
  }
  into.Declare(kFrameInterpolationKey, &frame_interpolation_);
  into.Declare(kSpinBlurKey, &spin_blur_.enabled);
  into.Declare(kSpinBlurStrengthKey, &spin_blur_.strength);
  into.Declare(kPostFxKey, &post_fx_.enabled);
  into.Declare(kBrightnessKey, &post_fx_.brightness);
  into.Declare(kContrastKey, &post_fx_.contrast);
  into.Declare(kGammaKey, &post_fx_.gamma);
  into.Declare(kSaturationKey, &post_fx_.saturation);
  into.Declare(kTemperatureKey, &post_fx_.temperature);
  into.Declare(kFilterKey, &post_fx_.filter);
  into.Declare(kVignetteKey, &post_fx_.vignette);
  into.Declare(kGrainKey, &post_fx_.grain);
  into.Declare(kAberrationKey, &post_fx_.aberration);
  into.Declare(kSharpenKey, &post_fx_.sharpen);
  into.Declare(kFxaaKey, &post_fx_.fxaa);
  into.Declare(kFxaaStrengthKey, &post_fx_.fxaa_strength);
}

void GraphicsOptions::Compose() {
  rows_.clear();
  rows_.push_back(Toggle(GraphicsRow::kLightmaps, Tr("Ombres"),
                         Tr("L'ombre et la lumière que la carte a peintes sur le sol, et qui "
                            "assombrissent ou colorent les personnages qui s'y tiennent."),
                         "/lightmap", lightmaps_));
  rows_.back().heading = Tr("La carte");
  // ⚠ GRISÉE SANS OMBRES : la lumière posée s'éteint avec elles, sur le sol comme
  // sur les corps, et il ne reste rien à refroidir.
  rows_.push_back(Toggle(GraphicsRow::kColdLight, Tr("Lumière froide"),
                         Tr("Refroidit la lumière peinte sur le sol : le jaune des lanternes vire "
                            "au bleu. Décoche pour les couleurs d'origine, plus chaudes."),
                         nullptr, cold_light_));
  rows_.back().indented = true;
  rows_.back().disabled = !lightmaps_;
  // ⚠ GRISÉE SANS OMBRES, comme la lumière froide : c'est l'ombre de la carte
  // qu'elle remplace.
  rows_.push_back(Toggle(
      GraphicsRow::kSunShadows, Tr("Ombres en temps réel"),
      Tr("Des ombres calculées à chaque image : celle du soleil de la carte, en plein air, au lieu "
         "de celle que la carte a peinte — les arbres et les bâtiments s'ombrent aussi eux-mêmes "
         "—, et celle des lumières que l'éditeur de carte pose, lanternes et braseros, sur toute "
         "carte.\n\nLe client d'origine n'en a pas, et la carte graphique travaille davantage."),
      nullptr, sun_shadows_));
  rows_.back().indented = true;
  rows_.back().disabled = !lightmaps_;
  rows_.push_back(Toggle(GraphicsRow::kEffectLights, Tr("Les sorts éclairent"),
                         Tr("Un sort de feu, de foudre ou de lumière éclaire le sol, le décor et "
                            "les personnages autour de lui, de la couleur de ses flammes. Les "
                            "monstres qui portent une flamme ou une lampe, et les feux de Sight "
                            "et de Ruwach, éclairent aussi autour d'eux.\n\nLe client d'origine "
                            "ne le fait pas."),
                         nullptr, effect_lights_));
  rows_.push_back(Toggle(GraphicsRow::kMapEffectsUnbounded, Tr("Effets de carte à toute distance"),
                         Tr("Les effets que la carte pose — torches, flammes des braseros, "
                            "lucioles, vapeurs, fumées et vent — vivent sur toute la carte, et "
                            "non plus seulement autour du personnage : on les voit en dézoomant."
                            "\n\nLe client d'origine ne les fait vivre qu'à treize cases du "
                            "personnage, et la carte graphique travaille davantage sur une "
                            "grande carte."),
                         nullptr, map_effects_unbounded_));
  rows_.push_back(Toggle(GraphicsRow::kRoundedGround, Tr("Sol arrondi"),
                         Tr("Les dunes, les collines et les talus s'arrondissent au lieu de se "
                            "plier en arêtes droites : chaque carreau du sol se découpe en seize, "
                            "posés sur une surface lisse. Les falaises restent nettes, et les "
                            "personnages suivent le sol dessiné.\n\nLe client d'origine dessine "
                            "le sol en facettes, et la carte graphique travaille un peu plus."),
                         nullptr, rounded_ground_));
  rows_.push_back(Toggle(GraphicsRow::kDayNightCycle, Tr("Cycle jour/nuit"),
                         Tr("Le soleil, puis la lune, suivent l'heure qu'il fait sur le serveur : "
                            "lumière du matin, du midi et du soir, puis une nuit plus sombre et "
                            "bleutée. En plein air seulement.\n\nAvec « Ombres en temps réel », "
                            "les ombres tournent avec lui. Le client d'origine garde la lumière de "
                            "la carte à toute heure."),
                         nullptr, day_night_cycle_));
  // ⚠ GRISÉE SANS CYCLE : sans lui, le ciel garde la lumière de la carte, et
  // il n'y a pas d'heure à figer.
  rows_.push_back(Toggle(GraphicsRow::kFixedSky, Tr("Heure fixe"),
                         Tr("Le ciel reste à l'heure choisie au lieu de suivre celle du serveur : "
                            "toujours le matin, toujours le soir, ou toujours la nuit. Le soleil, "
                            "la lune et les ombres s'y tiennent."),
                         nullptr, fixed_sky_));
  rows_.back().indented = true;
  rows_.back().disabled = !day_night_cycle_;
  rows_.push_back(OptionRowView{.id = IdOf(GraphicsRow::kFixedSkyHour),
                                .label = Tr("Heure du ciel"),
                                .kind = OptionKind::kSlider,
                                .value = fixed_sky_hour_,
                                .lowest = 0.0F,
                                .highest = kHoursPerDay,
                                .format = "%.1f h"});
  rows_.back().indented = true;
  rows_.back().disabled = !day_night_cycle_ || !fixed_sky_;
  rows_.push_back(Toggle(GraphicsRow::kFog, Tr("Brouillard"),
                         Tr("La brume que certaines cartes posent au loin : le décor et les "
                            "personnages s'effacent dans sa couleur à mesure qu'ils s'éloignent."),
                         "/fog", fog_));
  rows_.push_back(Toggle(GraphicsRow::kDecorVeil, Tr("Décor translucide devant le personnage"),
                         Tr("Un bâtiment, un arbre ou un mur qui se tient entre la caméra et "
                            "ton personnage s'efface jusqu'au tiers de son opacité, le temps de "
                            "te laisser voir, puis revient.\n\nLe client d'origine ne le "
                            "fait qu'avec un correctif de son exécutable."),
                         nullptr, decor_veil_));
  rows_.push_back(Toggle(GraphicsRow::kScreenQuake, Tr("Tremblements d'écran"),
                         Tr("Certains sorts, coups et évènements de carte font trembler la "
                            "caméra un instant. Décoche pour qu'elle reste immobile."),
                         "/quake", screen_quake_));
  rows_.push_back(Toggle(GraphicsRow::kElementGlow, Tr("Lueur élémentaire"),
                         Tr("La flèche en vol et l'arme d'un joueur se bordent de la couleur de "
                            "l'élément de son attaque : orange pour le feu, bleu pour l'eau…\n\n"
                            "Sur une carte de combat entre joueurs et pendant un duel, le serveur "
                            "ne dit l'élément de personne d'autre que toi."),
                         nullptr, element_glow_));
  rows_.push_back(Toggle(GraphicsRow::kWandOrb, Tr("Tir de baguette amélioré"),
                         Tr("Le trait d'une baguette devient une boule d'énergie qui sème une "
                            "courte traînée d'étincelles et éclate à l'arrivée. Elle prend la "
                            "couleur de l'élément de l'attaque quand la lueur élémentaire est "
                            "cochée, un bleu-violet sinon.\n\nDécoche pour retrouver le trait "
                            "d'origine."),
                         nullptr, wand_orb_));
  rows_.push_back(Toggle(GraphicsRow::kSummonTranslucent, Tr("Invocations translucides"),
                         Tr("Le faucon qui accompagne un chasseur se peint presque transparent : "
                            "il ne cache plus ce qui se passe derrière lui."),
                         nullptr, summon_translucent_));
  rows_.back().heading = Tr("Les compagnons");
  rows_.push_back(OptionRowView{
      .id = IdOf(GraphicsRow::kCorpseFade),
      .label = Tr("Durée des cadavres"),
      .help = Tr("Ce qu'un monstre tué reste à terre après sa chute, en s'effaçant peu à peu.\n\n"
                 "Le client d'origine le garde un peu plus de six secondes."),
      .kind = OptionKind::kSlider,
      .value = static_cast<float>(corpse_fade_ms()) / kMillisecondsPerSecond,
      .lowest = static_cast<float>(ro::render::kShortestCorpseFadeMs) / kMillisecondsPerSecond,
      .highest = static_cast<float>(ro::render::kLongestCorpseFadeMs) / kMillisecondsPerSecond,
      .format = "%.1f s"});
  rows_.back().heading = Tr("Les créatures");
  rows_.push_back(OptionRowView{
      .id = IdOf(GraphicsRow::kAppearFade),
      .label = Tr("Fondu d'apparition"),
      .help = Tr("Une créature que le serveur fait apparaître — une réapparition, une "
                 "invocation, une téléportation — naît presque transparente et devient opaque "
                 "en ce temps. Celle qui entre simplement dans le champ de vision ne fond pas.\n\n"
                 "Le client d'origine y met 384 ms. À zéro, elle paraît d'un coup."),
      .kind = OptionKind::kSlider,
      .value = static_cast<float>(appear_fade_ms()),
      .lowest = 0.0F,
      .highest = static_cast<float>(ro::render::kLongestAppearFadeMs),
      .format = "%.0f ms"});
  rows_.push_back(OptionRowView{
      .id = IdOf(GraphicsRow::kLeaveFade),
      .label = Tr("Fondu de sortie de vue"),
      .help = Tr("Un personnage ou un monstre qui sort du champ de vision s'efface en ce temps, "
                 "au lieu de disparaître d'un coup. Pendant le fondu, il ne se clique plus.\n\n"
                 "Le client d'origine y met 510 ms. À zéro, il disparaît d'un coup."),
      .kind = OptionKind::kSlider,
      .value = static_cast<float>(leave_fade_ms()),
      .lowest = 0.0F,
      .highest = static_cast<float>(ro::render::kLongestLeaveFadeMs),
      .format = "%.0f ms"});
  rows_.push_back(OptionRowView{
      .id = IdOf(GraphicsRow::kSkillGestureFloor),
      .label = Tr("Durée minimale du geste de sort"),
      .help = Tr("Quand un sort frappe, le personnage joue son geste à sa vitesse d'attaque : "
                 "avec une forte ASPD et un sort instantané, il passe trop vite pour être vu.\n\n"
                 "Ce réglage l'allonge jusqu'à cette durée. Seul l'affichage change : le coup "
                 "porte et les dégâts tombent au même instant.\n\n"
                 "À zéro, la cadence du client d'origine."),
      .kind = OptionKind::kSlider,
      .value = static_cast<float>(skill_gesture_floor_ms()),
      .lowest = 0.0F,
      .highest = static_cast<float>(kLongestSkillGestureFloorMs),
      .format = "%.0f ms"});
  rows_.push_back(
      Toggle(GraphicsRow::kCastSpansBar, Tr("Incantation déroulée sur la barre"),
             Tr("Pendant une incantation ordinaire — Storm Gust, Cold Bolt… —, le client fige "
                "le personnage sur la première image de son geste jusqu'à la fin de la barre.\n\n"
                "Cochée, le geste se déroule lentement sur toute la durée de la barre. Seul "
                "l'affichage change : la barre et le sort gardent leur durée."),
             nullptr, cast_spans_bar_));

  msaa_names_ = {TrText("Désactivé"), "2×", "4×", "8×"};
  rows_.push_back(
      Choice(GraphicsRow::kMsaa, Tr("Anticrénelage matériel (MSAA)"),
             Tr("Chaque pixel du monde est échantillonné plusieurs fois : les arêtes du décor "
                "s'adoucissent sans flouter le reste. Plus lourd que le FXAA. Il ne lisse pas les "
                "découpes par transparence, comme l'herbe ou les grilles, et se borne à ce que la "
                "carte graphique accepte."),
             msaa_names_, RankIn(kMsaaSamples, msaa_samples_)));
  rows_.back().heading = Tr("Rendu");
  scale_labels_.clear();
  scale_names_.clear();
  for (const int percent : kRenderScalePercents) {
    scale_labels_.push_back(InterfaceScaleLabel(percent));
  }
  // ⚠ LES POINTEURS APRÈS LES TEXTES : remplir le vecteur de textes peut les
  // déplacer, et un pointeur pris avant pendrait.
  for (const std::string& label : scale_labels_) {
    scale_names_.push_back(label.c_str());
  }
  rows_.push_back(Choice(
      GraphicsRow::kRenderScale, Tr("Échelle de rendu"),
      Tr("La taille à laquelle le monde se peint, en part de la fenêtre. Au-dessus de 100 %, "
         "il se réduit ensuite : plus net, plus lourd. En dessous, il s'agrandit : plus "
         "flou, plus léger. L'interface reste à pleine résolution."),
      scale_names_, RankIn(kRenderScalePercents, render_scale_percent_)));
  decor_names_ = {TrText("Nettes (d'origine)"), TrText("Trilinéaire"),   TrText("Anisotrope 2×"),
                  TrText("Anisotrope 4×"),      TrText("Anisotrope 8×"), TrText("Anisotrope 16×")};
  rows_.push_back(Choice(
      GraphicsRow::kDecorFilter, Tr("Textures du décor 3D"),
      Tr("Arbres, maisons, rochers. Nettes, comme le client d'origine : sans niveaux réduits, "
         "un décor lointain scintille. Trilinéaire et anisotrope le lissent de loin, au prix "
         "d'un peu de flou ; l'anisotrope garde plus de netteté sur les faces vues de biais, "
         "dans la limite de la carte graphique. Le filtrage n'agit que sur une texture réduite à "
         "l'écran : son effet grandit quand on dézoome et s'efface quand l'échelle de rendu "
         "monte — à 200 %, le suréchantillonnage en fait déjà l'essentiel. Le sol n'en reçoit "
         "jamais : ses planches de motifs en sortiraient en damier."),
      decor_names_,
      std::clamp(decor_filter_rank_, 0, static_cast<int>(kDecorAnisotropy.size()) - 1)));
  rows_.push_back(
      Toggle(GraphicsRow::kFrameInterpolation, Tr("Interpolation d'images"),
             Tr("Le jeu avance par pas de 16 ms, soit 60 fois par seconde. Sur un écran plus "
                "rapide, ce qui suit ce pas — carts, faucons, rotation des modèles 3D, décor "
                "animé — tiendrait la même place plusieurs images de suite, puis sauterait.\n\n"
                "Cochée, ces éléments se peignent entre deux pas et glissent d'une position à "
                "l'autre, au prix d'au plus un pas de retard. Seul l'affichage change : le jeu "
                "garde sa cadence."),
             nullptr, frame_interpolation_));

  ComposePostFx();
}

void GraphicsOptions::ComposePostFx() {
  // LE FLOU DE MOUVEMENT, HORS DE L'INTERRUPTEUR : il ne dépend d'aucun réglage
  // du post-traitement.
  const ro::render::SpinBlurSettings spin = spin_blur();
  rows_.push_back(Toggle(GraphicsRow::kSpinBlur, Tr("Flou de mouvement"),
                         Tr("Quand tu fais tourner ou incliner la caméra, le monde s'étire le long "
                            "de sa rotation, autour du point qu'elle regarde. Quand ton personnage "
                            "marche, ou que la caméra saute d'un cran, l'image reste nette. Les "
                            "noms, les jauges et l'interface ne sont jamais floutés."),
                         nullptr, spin.enabled));
  rows_.back().heading = Tr("Effets d'écran");
  rows_.push_back(Slider(GraphicsRow::kSpinBlurStrength, Tr("Intensité du flou"), spin.strength,
                         ro::render::kSpinBlurStrengthRange));
  rows_.back().indented = true;
  rows_.back().disabled = !spin.enabled;

  rows_.push_back(Toggle(GraphicsRow::kPostFx, Tr("Post-processing (effets d'écran)"),
                         Tr("Étalonnage, effets d'écran et anticrénelage, sur le monde : terrain, "
                            "décor, personnages et effets. Les noms, les jauges et l'interface "
                            "restent tels quels."),
                         nullptr, post_fx_.enabled));
  // TOUT CE QUI SUIT DÉPEND DE L'INTERRUPTEUR : grisé quand il est éteint, pour
  // qu'on voie ce qu'on retrouvera en le rallumant.
  const std::size_t governed_from = rows_.size();

  preset_names_ = {TrText("Personnalisé"), TrText("Neutre"), TrText("Nuit"),
                   TrText("Cinéma"),       TrText("Rétro"),  TrText("N&B")};
  const PostFxSettings shown = ro::render::Clamped(post_fx_);
  rows_.push_back(Choice(GraphicsRow::kPreset, Tr("Préréglage"),
                         Tr("Remplace tous les réglages ci-dessous. « Personnalisé » dit que les "
                            "réglages ne suivent aucun préréglage."),
                         preset_names_, PresetRankOf(shown)));

  rows_.push_back(Slider(GraphicsRow::kBrightness, Tr("Luminosité"), shown.brightness,
                         ro::render::kBrightnessRange));
  rows_.back().heading = Tr("Couleur");
  rows_.push_back(
      Slider(GraphicsRow::kContrast, Tr("Contraste"), shown.contrast, ro::render::kContrastRange));
  rows_.push_back(Slider(GraphicsRow::kGamma, Tr("Gamma"), shown.gamma, ro::render::kGammaRange));
  rows_.push_back(Slider(GraphicsRow::kSaturation, Tr("Saturation"), shown.saturation,
                         ro::render::kSaturationRange));
  rows_.push_back(Slider(GraphicsRow::kTemperature, Tr("Température"), shown.temperature,
                         ro::render::kTemperatureRange));
  filter_names_ = {TrText("Aucun"), TrText("Noir & blanc"), TrText("Sépia"), TrText("Négatif"),
                   TrText("Daltonien")};
  rows_.push_back(Choice(GraphicsRow::kFilter, Tr("Filtre"), nullptr, filter_names_, shown.filter));

  rows_.push_back(
      Slider(GraphicsRow::kVignette, Tr("Vignette"), shown.vignette, ro::render::kEffectRange));
  rows_.back().heading = Tr("Effets");
  rows_.push_back(Slider(GraphicsRow::kGrain, Tr("Grain"), shown.grain, ro::render::kEffectRange));
  rows_.push_back(Slider(GraphicsRow::kAberration, Tr("Aberration"), shown.aberration,
                         ro::render::kEffectRange));
  rows_.push_back(
      Slider(GraphicsRow::kSharpen, Tr("Netteté"), shown.sharpen, ro::render::kEffectRange));
  rows_.push_back(
      Toggle(GraphicsRow::kFxaa, Tr("FXAA (anti-crénelage)"), nullptr, nullptr, shown.fxaa));
  rows_.push_back(Slider(GraphicsRow::kFxaaStrength, Tr("Force FXAA"), shown.fxaa_strength,
                         ro::render::kEffectRange));
  // ⚠ JUSQU'À UN : le post-traitement ne voit que le monde, et le texte des noms
  // comme de l'interface reste net quoi qu'on pousse.
  rows_.back().help =
      Tr("Le mélange du résultat de l'anticrénelage avec l'image. Il ne touche "
         "que le monde : les noms et l'interface restent nets à toute force.");
  rows_.back().indented = true;
  rows_.back().disabled = !shown.fxaa;
  rows_.push_back(OptionRowView{.id = IdOf(GraphicsRow::kReset),
                                .label = Tr("Réinitialiser"),
                                .kind = OptionKind::kAction,
                                .small_button = true});

  for (std::size_t rank = governed_from; rank < rows_.size(); ++rank) {
    rows_[rank].disabled = rows_[rank].disabled || !post_fx_.enabled;
  }
}

bool GraphicsOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kGraphicsRowBase ||
      asked.changed >= kGraphicsRowBase + static_cast<int>(GraphicsRow::kCount)) {
    return false;
  }
  switch (static_cast<GraphicsRow>(asked.changed - kGraphicsRowBase)) {
    case GraphicsRow::kLightmaps:
      lightmaps_ = asked.on;
      break;
    case GraphicsRow::kColdLight:
      cold_light_ = asked.on;
      break;
    case GraphicsRow::kSunShadows:
      sun_shadows_ = asked.on;
      break;
    case GraphicsRow::kEffectLights:
      effect_lights_ = asked.on;
      break;
    case GraphicsRow::kMapEffectsUnbounded:
      map_effects_unbounded_ = asked.on;
      break;
    case GraphicsRow::kRoundedGround:
      rounded_ground_ = asked.on;
      break;
    case GraphicsRow::kDayNightCycle:
      day_night_cycle_ = asked.on;
      break;
    case GraphicsRow::kFixedSky:
      fixed_sky_ = asked.on;
      break;
    case GraphicsRow::kFixedSkyHour:
      fixed_sky_hour_ = std::clamp(asked.value, 0.0F, kHoursPerDay);
      break;
    case GraphicsRow::kFog:
      fog_ = asked.on;
      break;
    case GraphicsRow::kDecorVeil:
      decor_veil_ = asked.on;
      break;
    case GraphicsRow::kScreenQuake:
      screen_quake_ = asked.on;
      break;
    case GraphicsRow::kElementGlow:
      element_glow_ = asked.on;
      break;
    case GraphicsRow::kWandOrb:
      wand_orb_ = asked.on;
      break;
    case GraphicsRow::kSummonTranslucent:
      summon_translucent_ = asked.on;
      break;
    case GraphicsRow::kAppearFade:
      appear_fade_ms_ = static_cast<std::uint32_t>(std::lround(std::max(asked.value, 0.0F)));
      break;
    case GraphicsRow::kLeaveFade:
      leave_fade_ms_ = static_cast<std::uint32_t>(std::lround(std::max(asked.value, 0.0F)));
      break;
    case GraphicsRow::kCastSpansBar:
      cast_spans_bar_ = asked.on;
      break;
    case GraphicsRow::kSkillGestureFloor:
      skill_gesture_floor_ms_ =
          static_cast<std::uint32_t>(std::lround(std::max(asked.value, 0.0F)));
      break;
    case GraphicsRow::kCorpseFade:
      corpse_fade_ms_ = static_cast<std::uint32_t>(
          std::lround(std::max(asked.value, 0.0F) * kMillisecondsPerSecond));
      break;
    case GraphicsRow::kMsaa:
      msaa_samples_ = kMsaaSamples[static_cast<std::size_t>(
          std::clamp(asked.chosen, 0, static_cast<int>(kMsaaSamples.size()) - 1))];
      break;
    case GraphicsRow::kRenderScale:
      render_scale_percent_ = kRenderScalePercents[static_cast<std::size_t>(
          std::clamp(asked.chosen, 0, static_cast<int>(kRenderScalePercents.size()) - 1))];
      break;
    case GraphicsRow::kDecorFilter:
      decor_filter_rank_ =
          std::clamp(asked.chosen, 0, static_cast<int>(kDecorAnisotropy.size()) - 1);
      break;
    case GraphicsRow::kFrameInterpolation:
      frame_interpolation_ = asked.on;
      break;
    case GraphicsRow::kSpinBlur:
      spin_blur_.enabled = asked.on;
      break;
    case GraphicsRow::kSpinBlurStrength:
      spin_blur_.strength = asked.value;
      break;
    case GraphicsRow::kPostFx:
      post_fx_.enabled = asked.on;
      break;
    case GraphicsRow::kPreset:
      // « Personnalisé » ne pose rien : il ne fait que dire l'état.
      if (asked.chosen > kCustomPresetRank &&
          asked.chosen <= static_cast<int>(PostFxPreset::kCount)) {
        post_fx_ = ro::render::PresetSettings(static_cast<PostFxPreset>(asked.chosen - 1));
      }
      break;
    case GraphicsRow::kBrightness:
      post_fx_.brightness = asked.value;
      break;
    case GraphicsRow::kContrast:
      post_fx_.contrast = asked.value;
      break;
    case GraphicsRow::kGamma:
      post_fx_.gamma = asked.value;
      break;
    case GraphicsRow::kSaturation:
      post_fx_.saturation = asked.value;
      break;
    case GraphicsRow::kTemperature:
      post_fx_.temperature = asked.value;
      break;
    case GraphicsRow::kFilter:
      post_fx_.filter = asked.chosen;
      break;
    case GraphicsRow::kVignette:
      post_fx_.vignette = asked.value;
      break;
    case GraphicsRow::kGrain:
      post_fx_.grain = asked.value;
      break;
    case GraphicsRow::kAberration:
      post_fx_.aberration = asked.value;
      break;
    case GraphicsRow::kSharpen:
      post_fx_.sharpen = asked.value;
      break;
    case GraphicsRow::kFxaa:
      post_fx_.fxaa = asked.on;
      break;
    case GraphicsRow::kFxaaStrength:
      post_fx_.fxaa_strength = asked.value;
      break;
    case GraphicsRow::kReset:
    case GraphicsRow::kCount:
      // Les défauts, interrupteur allumé : c'est le préréglage neutre.
      post_fx_ = ro::render::PresetSettings(PostFxPreset::kNeutral);
      break;
  }
  return true;
}

bool GraphicsOptions::ToggleLightmaps() {
  lightmaps_ = !lightmaps_;
  return lightmaps_;
}

bool GraphicsOptions::ToggleFog() {
  fog_ = !fog_;
  return fog_;
}

}  // namespace moonclient
