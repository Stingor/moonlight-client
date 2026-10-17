#include "ro/ui/staff_tools.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <format>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_bounds.h"
#include "ro/ui/window_name.h"

#include "imgui.h"

namespace ro::ui {
namespace {

// La taille au PREMIER affichage seulement : ensuite, c'est celle que le staff
// lui a donnée qui compte. Celle de l'implémentation de référence, moins la
// hauteur des huit sections qui ne sont pas encore portées.
constexpr float kFirstWidth = 420.0F;
constexpr float kFirstHeight = 320.0F;

// La largeur des curseurs et des listes : FIXE, et non la place restante. Un
// contrôle qui suit la fenêtre change de taille à chaque redimensionnement, et
// une colonne de contrôles de même largeur aligne leurs libellés d'elle-même.
constexpr float kControlWidthPixels = 190.0F;

// L'aide au survol du sol uni. Une ligne par idée : l'infobulle de
// l'implémentation de référence les sépare de la même façon.
constexpr std::array<const char*, 5> kFlatGroundHelp = {
    core::TrText("Repeint tout le terrain d'une couleur unie, sans toucher au reste de la scène."),
    core::TrText(
        "La géométrie et la profondeur du sol restent intactes : ce qui passe derrière une "
        "colline reste caché."),
    core::TrText("L'eau, le ciel et le brouillard ne sont PAS affectés."),
    core::TrText("Un carreau que sa texture rend transparent le reste : le sol uni ne bouche pas "
                 "les trous d'une carte."),
    core::TrText("La couleur est posée telle quelle, sans éclairage : c'est ce qui en fait un fond "
                 "de capture."),
};

// LE FOND DE CAPTURE : le sol d'une seule couleur.
void DrawCaptureBackdrop(FlatGroundLook& flat, const SkinArt& art) {
  // Le titre de section vient d'ImGui, comme dans le panneau d'options : le
  // skin n'a pas de pièce pour cela, et en inventer une ferait une seconde
  // grammaire de séparateurs dans le même client.
  ImGui::SeparatorText(core::Tr("Fond de capture"));

  // Rien à prévenir quand la case bascule : le dessin du monde relit ce réglage
  // à chaque image.
  static_cast<void>(SkinCheckBox(core::Tr("Sol uni (fond de capture)"), &flat.on, art));
  ImGui::SameLine();
  SkinHelpMarker(core::Tr("Sol uni (fond de capture)"), kFlatGroundHelp);

  // ⚠ GRISÉ, JAMAIS RETIRÉ : un réglage qui disparaît fait sauter la mise en
  // page sous les yeux de qui vient de cocher la case au-dessus.
  ImGui::BeginDisabled(!flat.on);
  // ⚠ SANS BARRE D'OPACITÉ, et c'est un écart ASSUMÉ avec la référence : la
  // passe du sol est opaque, donc l'alpha ne commande rien. La référence en
  // montre un et le dément par une phrase ; ici on ne montre pas une poignée qui
  // ne tire sur rien.
  static_cast<void>(SkinColorSwatch(core::Tr("Couleur du sol"), &flat.color, art,
                                    /*with_alpha=*/false, /*numbers=*/true));
  ImGui::EndDisabled();
}

constexpr std::array<const char*, 3> kFreeCameraHelp = {
    core::TrText("Retire les bornes du jeu : recul, inclinaison et rotation vont aussi loin qu'on "
                 "veut."),
    core::TrText("L'œil peut passer sous le sol, ce que le jeu empêche."),
    core::TrText("Les gestes gardent leurs pas : seules les limites disparaissent."),
};

constexpr std::array<const char*, 2> kCameraHudHelp = {
    core::TrText("Affiche en direct le lacet, l'élévation et le recul, dans nos unités et "
                 "dans celles du jeu."),
    core::TrText("Et les bornes en vigueur sur la carte, avec le plancher de l'œil."),
};

void DrawDebugCamera(StaffToolsLook& look, const SkinArt& art) {
  ImGui::SeparatorText(core::Tr("Caméra"));
  static_cast<void>(SkinCheckBox(core::Tr("Caméra sans bornes"), &look.free_camera, art));
  ImGui::SameLine();
  SkinHelpMarker(core::Tr("Caméra sans bornes"), kFreeCameraHelp);
  static_cast<void>(SkinCheckBox(core::Tr("Relevé de la caméra"), &look.camera_hud, art));
  ImGui::SameLine();
  SkinHelpMarker(core::Tr("Relevé de la caméra"), kCameraHudHelp);
}

constexpr std::array<const char*, 4> kSoundZonesHelp = {
    core::TrText("Dessine autour du joueur les sources de son posées par la carte : leur emprise "
                 "en bleu, et le cercle d'où l'on entend leur bruit en vert."),
    core::TrText("Le point marque d'où le bruit part en ce moment : il suit le joueur, et peut "
                 "quitter l'emprise, comme dans le jeu."),
    core::TrText("Le tracé s'allume en jaune quand la source sonne."),
    core::TrText("Grisée quand les bruitages sont coupés : une source ne se place qu'en écoutant, "
                 "et le client n'écoute plus. Rallumez-les dans Sound."),
};

void DrawSound(StaffToolsState& state, const SkinArt& art) {
  ImGui::SeparatorText(core::Tr("Son"));
  // ⚠ LA CASE GARDE SA VALEUR, grisée : rallumer les bruitages rend les zones
  // telles qu'on les avait demandées.
  ImGui::BeginDisabled(!state.sound_audible);
  static_cast<void>(
      SkinCheckBox(core::Tr("Zones de son d'ambiance"), &state.look.sound_zones, art));
  ImGui::EndDisabled();
  // L'aide HORS du grisé : c'est elle qui dit pourquoi la case ne répond plus.
  ImGui::SameLine();
  SkinHelpMarker(core::Tr("Zones de son d'ambiance"), kSoundZonesHelp);
}

constexpr std::array<const char*, 4> kSkyHelp = {
    core::TrText("Place le soleil, ou la lune, à l'heure choisie : la lumière du sol et du décor "
                 "suit, et les ombres aussi quand « Ombres en temps réel » est cochée dans "
                 "Graphics."),
    core::TrText("Le soleil se lève et se couche aux heures que règle la course des astres ; "
                 "la lune prend la nuit, plus faible et bleutée."),
    core::TrText("Seul ce client voit ce ciel : rien ne part au serveur."),
    core::TrText("Rien n'est retenu : chaque lancement rend le ciel de la carte."),
};

constexpr float kMinutesPerHour = 60.0F;

// Les libellés des vitesses, dans l'ordre de `StaffSkyClock::kMinutesPerMinute`.
constexpr std::array<const char*, StaffSkyClock::kMinutesPerMinute.size()> kSkySpeedNames = {
    core::TrText("Temps réel (1 min / min)"),
    core::TrText("2 min / min"),
    core::TrText("5 min / min"),
    core::TrText("10 min / min"),
    core::TrText("30 min / min"),
    core::TrText("1 h / min"),
    core::TrText("4 h / min"),
    core::TrText("1 jour / min"),
    core::TrText("5 jours / min"),
};

// LE CIEL : l'heure du soleil et de la lune.
void DrawSky(StaffSkyClock& sky, const render::SkyCourse& course, const SkinArt& art) {
  ImGui::SeparatorText(core::Tr("Ciel"));
  static_cast<void>(SkinCheckBox(core::Tr("Forcer l'heure du ciel"), &sky.forced, art));
  ImGui::SameLine();
  SkinHelpMarker(core::Tr("Forcer l'heure du ciel"), kSkyHelp);
  ImGui::SameLine();
  const auto whole_hours = static_cast<int>(sky.hour);
  const auto minutes =
      static_cast<int>((sky.hour - static_cast<float>(whole_hours)) * kMinutesPerHour);
  ImGui::TextDisabled(core::Tr("%02d h %02d — %s"), whole_hours, minutes,
                      course.SunAt(sky.hour) ? core::Tr("soleil") : core::Tr("lune"));
  ImGui::BeginDisabled(!sky.forced);
  static_cast<void>(
      SkinSliderFloat(core::Tr("Heure"), &sky.hour, 0.0F, render::kHoursPerDay, art, "%.2f"));
  static_cast<void>(SkinCheckBox(core::Tr("Faire tourner le temps"), &sky.running, art));
  ImGui::BeginDisabled(!sky.running);
  static_cast<void>(SkinCombo(core::Tr("Vitesse du temps"), &sky.speed_rank, kSkySpeedNames, art));
  ImGui::EndDisabled();
  ImGui::EndDisabled();
}

constexpr std::array<const char*, 3> kSkyCourseHelp = {
    core::TrText("Règle le chemin du soleil et de la lune dans le ciel : d'où ils viennent, à "
                 "quelle hauteur ils culminent à midi, et quelle part de l'horizon ils balaient."),
    core::TrText("Vaut pour l'heure forcée ci-dessus comme pour le « Cycle jour/nuit » de "
                 "Graphics. Seul ce client le voit."),
    core::TrText("Décochée, c'est la course tirée de la lumière de la carte."),
};

// Les bornes des curseurs de la course, en degrés et en heures.
constexpr float kMostAzimuthOffsetDegrees = 180.0F;
constexpr float kLowestNoonAltitudeDegrees = 5.0F;
constexpr float kHighestNoonAltitudeDegrees = 90.0F;
constexpr float kLatestSunriseHour = 12.0F;

// L'ÉDITEUR DE CARTE : sa porte. Le mode lui-même a son panneau.
void DrawMapEditorDoor(StaffToolsState& state, const SkinArt& art) {
  ImGui::SeparatorText(core::Tr("Éditeur de carte"));
  if (SkinButton(state.map_editor.active ? core::Tr("Quitter l'éditeur")
                                         : core::Tr("Ouvrir l'éditeur de carte"),
                 art)) {
    state.map_editor_intents.emplace_back(map_editor::Toggle{});
  }
}

// LA COURSE DES ASTRES : voir `render::SkyCourse`.
void DrawSkyCourse(StaffToolsLook& look, const SkinArt& art) {
  static_cast<void>(
      SkinCheckBox(core::Tr("Régler la course des astres"), &look.sky_course_tuned, art));
  ImGui::SameLine();
  SkinHelpMarker(core::Tr("Régler la course des astres"), kSkyCourseHelp);
  render::SkyCourse& course = look.sky_course;
  ImGui::BeginDisabled(!look.sky_course_tuned);
  static_cast<void>(SkinSliderFloat(core::Tr("Décalage d'azimut (°)"),
                                    &course.azimuth_offset_degrees, -kMostAzimuthOffsetDegrees,
                                    kMostAzimuthOffsetDegrees, art, "%.0f"));
  static_cast<void>(
      SkinCheckBox(core::Tr("Hauteur de midi imposée"), &course.noon_altitude_forced, art));
  ImGui::BeginDisabled(!course.noon_altitude_forced);
  static_cast<void>(SkinSliderFloat(core::Tr("Hauteur de midi (°)"), &course.noon_altitude_degrees,
                                    kLowestNoonAltitudeDegrees, kHighestNoonAltitudeDegrees, art,
                                    "%.0f"));
  ImGui::EndDisabled();
  static_cast<void>(SkinSliderFloat(core::Tr("Balayage (°)"), &course.sweep_degrees,
                                    render::kNarrowestSweepDegrees, render::kWidestSweepDegrees,
                                    art, "%.0f"));
  static_cast<void>(SkinSliderFloat(core::Tr("Lever (h)"), &course.sunrise_hour, 0.0F,
                                    kLatestSunriseHour, art, "%.1f"));
  static_cast<void>(SkinSliderFloat(core::Tr("Coucher (h)"), &course.sunset_hour,
                                    kLatestSunriseHour, render::kHoursPerDay, art, "%.1f"));
  if (SkinButton(core::Tr("Rétablir la course"), art)) {
    course = render::SkyCourse();
  }
  ImGui::EndDisabled();
}

constexpr std::array<const char*, 3> kDebugConsoleHelp = {
    core::TrText("Ouvre une écoute sur 127.0.0.1 : un outil de cette machine — le pont MCP de "
                 "tools/moonclient_mcp — lit alors l'état du client : acteurs, positions, IA "
                 "des compagnons, trames reçues, journal."),
    core::TrText("En lecture seule : rien ne part vers le serveur, rien ne change dans le jeu."),
    core::TrText("Éteinte à chaque lancement. Seule cette machine peut s'y brancher."),
};

void DrawDebugConsole(StaffToolsState& state, const SkinArt& art) {
  ImGui::SeparatorText(core::Tr("Console de mise au point"));
  static_cast<void>(SkinCheckBox(core::Tr("Ouvrir la console (MCP)"), &state.debug_console, art));
  ImGui::SameLine();
  SkinHelpMarker(core::Tr("Ouvrir la console (MCP)"), kDebugConsoleHelp);
  if (!state.debug_console) {
    return;
  }
  if (!state.debug_console_failure.empty()) {
    ImGui::TextWrapped(core::Tr("Écoute impossible : %s"), state.debug_console_failure.c_str());
    return;
  }
  if (state.debug_console_port > 0) {
    ImGui::TextDisabled(
        core::Tr("127.0.0.1:%d — %s"), state.debug_console_port,
        state.debug_console_connected ? core::Tr("pont branché") : core::Tr("en attente du pont"));
  }
}

constexpr std::array<const char*, 3> kPickBoxesHelp = {
    core::TrText("Dessine les rectangles d'écran dont la souris se sert pour savoir sur quoi elle "
                 "pointe : ce ne sont pas les positions au sol."),
    core::TrText("Jaune épais : celui qu'un clic prendrait maintenant, avec son numéro. Rose : le "
                 "vôtre. Vert : un acteur ; bleu : un item au sol ; orange : une unité de skill."),
    core::TrText("À regarder quand un skill part sur la mauvaise cible, ou ne part pas alors que "
                 "le curseur semble bien placé."),
};

constexpr std::array<const char*, 2> kForcedFloorHelp = {
    core::TrText("Décochée, le minimum des acteurs est celui du jeu, divisé par le réglage "
                 "au-dessus."),
    core::TrText("Cochée, le curseur le remplace, en pixels : le défaut du jeu est rappelé à "
                 "côté."),
};

constexpr std::array<const char*, 3> kNegativeIdHelp = {
    core::TrText("Un numéro d'entité négatif n'existe pas dans le protocole : c'est un acteur "
                 "fantôme, que le serveur ne connaît pas et vers lequel toute action se perd."),
    core::TrText("Sa zone cliquable, elle, est bien là, et peut gagner le clic devant la vraie "
                 "cible."),
    core::TrText("Cochée, elle est rétrécie autour de son centre, à la taille restante choisie."),
};

// La plus petite valeur que le curseur du plancher forcé propose, et le haut
// de sa course quand le défaut du jeu est plus petit.
constexpr int kLeastForcedFloorPixels = 4;
constexpr int kForcedFloorCeilingPixels = 64;
constexpr int kWholePercent = 100;

// LES ZONES CLIQUABLES : les voir, et régler ce que le clic attrape.
void DrawPicking(PickTuningLook& look, const SkinArt& art) {
  ImGui::SeparatorText(core::Tr("Zones cliquables"));
  static_cast<void>(SkinCheckBox(core::Tr("Contour des zones cliquables"), &look.boxes_shown, art));
  ImGui::SameLine();
  SkinHelpMarker(core::Tr("Contour des zones cliquables"), kPickBoxesHelp);

  // ⚠ LE DIVISEUR N'EST PAS ICI : c'est un réglage du joueur, dans la section
  // « Précision du ciblage » des options. Seul le plancher forcé reste à
  // l'établi.
  const float width = ImGui::GetIO().DisplaySize.x;

  const int game_floor =
      static_cast<int>(render::PickFloorPixels(render::kActorPickFloorFactor, width, 0));
  static_cast<void>(
      SkinCheckBox(core::Tr("Forcer la zone cliquable minimale"), &look.actor_floor_forced, art));
  ImGui::SameLine();
  SkinHelpMarker(core::Tr("Forcer la zone cliquable minimale"), kForcedFloorHelp);
  ImGui::SameLine();
  ImGui::TextDisabled(core::Tr("jeu : %d px"), game_floor);
  ImGui::BeginDisabled(!look.actor_floor_forced);
  static_cast<void>(SkinSliderInt(core::Tr("Zone cliquable minimale (px)"),
                                  &look.actor_floor_pixels, kLeastForcedFloorPixels,
                                  std::max(game_floor, kForcedFloorCeilingPixels), art));
  ImGui::EndDisabled();

  static_cast<void>(SkinCheckBox(core::Tr("Rétrécir la zone cliquable des numéros négatifs"),
                                 &look.shrink_negative_ids, art));
  ImGui::SameLine();
  SkinHelpMarker(core::Tr("Rétrécir la zone cliquable des numéros négatifs"), kNegativeIdHelp);
  ImGui::BeginDisabled(!look.shrink_negative_ids);
  static_cast<void>(SkinSliderInt(core::Tr("Taille restante (%)"), &look.negative_id_percent, 0,
                                  kWholePercent, art));
  ImGui::EndDisabled();
}

}  // namespace

PickTuning PickTuningOf(const PickTuningLook& look, bool staff_tools, float viewport_width,
                        int floor_shift) {
  PickTuning tuning;
  const int shift = std::clamp(floor_shift, 0, render::kLargestPickFloorShift);
  tuning.actor_floor_pixels =
      render::PickFloorPixels(render::kActorPickFloorFactor, viewport_width, shift);
  tuning.ground_floor_pixels =
      render::PickFloorPixels(render::kGroundPickFloorFactor, viewport_width, shift);
  if (staff_tools && look.actor_floor_forced) {
    tuning.actor_floor_pixels = static_cast<float>(std::max(1, look.actor_floor_pixels));
  }
  if (look.shrink_negative_ids) {
    tuning.negative_id_share =
        static_cast<float>(std::clamp(look.negative_id_percent, 0, kWholePercent)) /
        static_cast<float>(kWholePercent);
  }
  return tuning;
}

std::array<float, 3> FlatGroundColorOf(const FlatGroundLook& flat) {
  const ImVec4 held = ImGui::ColorConvertU32ToFloat4(flat.color);
  // ⚠ TROIS CANAUX, PAS QUATRE : la passe du sol est opaque, et un alpha qui
  // arriverait jusque-là ne ferait que se faire ignorer plus loin, à un endroit
  // où plus personne ne saurait pourquoi.
  return {held.x, held.y, held.z};
}

float StaffSkyClock::HoursPerMinute() const noexcept {
  const auto rank = static_cast<std::size_t>(
      std::clamp(speed_rank, 0, static_cast<int>(kMinutesPerMinute.size()) - 1));
  return static_cast<float>(kMinutesPerMinute[rank]) / kMinutesPerHour;
}

bool SoundZonesShown(const StaffToolsState& state) noexcept {
  return state.look.sound_zones && state.sound_audible;
}

StaffToolsAction DrawStaffTools(StaffToolsState& state, const SkinArt& art, bool allowed) {
  StaffToolsAction action;
  // 🔴 LE DROIT SE REVÉRIFIE ICI, À CHAQUE IMAGE. Il n'est ni mémorisé ni
  // recopié dans l'état : le niveau de groupe arrive au login et peut changer en
  // cours de session.
  //
  // ⚠ ET `open` N'EST PAS EFFACÉ pour autant. Le droit peut revenir, et un
  // établi qu'il faut rouvrir après chaque changement de compte serait une
  // corvée. Il est ignoré, pas oublié.
  if (!allowed) {
    return action;
  }
  // LE PANNEAU DE L'ÉDITEUR, établi ouvert ou non : le mode garde sa fenêtre.
  DrawMapEditor(state.map_editor, state.map_editor_panel, state.map_editor_intents, art);
  if (!state.open) {
    return action;
  }

  // Recomposé au changement de langue seulement : voir `WindowName`.
  static WindowName name(kStaffToolsTitle, kStaffToolsId);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidth, kFirstHeight), ImGuiCond_FirstUseEver);
  ConstrainNextWindowToScreen(name.Get());
  // LE JOURNAL DU PROGRAMME ET L'INSPECTEUR DE TRAMES, à un clic : ce sont des
  // outils du staff, et ces boutons sont leur seule porte.
  SetNextWindowTitleAction(core::Tr("Journal"),
                           core::Tr("Ouvrir ou refermer le journal du programme."), true);
  SetNextWindowTitleAction(core::Tr("Trames"),
                           core::Tr("Ouvrir ou refermer l'inspecteur des trames reçues."), true);
  const bool body_shown = BeginSkinnedWindow(name.Get(), &state.open, art);
  // ⚠ LUS AVANT DE SORTIR : repliée, la fenêtre garde ses deux boutons de barre.
  action.toggle_log = TitleActionClicked(0);
  action.toggle_packets = TitleActionClicked(1);
  if (!body_shown) {
    EndSkinnedWindow();
    return action;
  }

  ImGui::PushItemWidth(kControlWidthPixels * ImGui::GetStyle().FontScaleMain);
  DrawCaptureBackdrop(state.look.flat_ground, art);
  // Juste avant la caméra.
  static_cast<void>(DrawQuickCastSettings(state.quick_cast, art, allowed));
  DrawDebugCamera(state.look, art);
  DrawSky(state.sky, state.look.sky_course_tuned ? state.look.sky_course : render::SkyCourse(),
          art);
  DrawSkyCourse(state.look, art);
  DrawMapEditorDoor(state, art);
  DrawSound(state, art);
  DrawPicking(state.look.picking, art);
  DrawDebugConsole(state, art);
  ImGui::PopItemWidth();

  // Fermer, calé à droite comme dans le panneau de réglages : position
  // recalculée à chaque image, largeur mesurée sur le libellé traduit.
  const float close_width = SkinButtonWidth(core::Tr("Fermer"), art);
  ImGui::SetCursorPosX(
      std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - close_width));
  if (SkinButton(core::Tr("Fermer"), art, ImVec2(close_width, 0.0F))) {
    state.open = false;
  }

  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
