// L'établi du staff : le droit qui l'ouvre, et la couleur qu'il donne au sol.
//
// Ce qui se vérifie ici tient en deux points, et ce sont les deux qu'une
// relecture pourrait « corriger » de travers : un droit qui ne s'accorde que
// par le serveur, et une couleur qui perd son alpha EXPRÈS.

#include "ro/ui/staff_tools.h"

#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using ro::ui::FlatGroundColorOf;
using ro::ui::FlatGroundLook;
using ro::ui::StaffToolsAllowed;

TEST_CASE("Établi : en ligne, le droit vient du serveur", "[ui][staff]") {
  // Sous le seuil, rien — et c'est le cas qui compte : des groupes qui ne sont
  // pas du staff ont un niveau NON NUL, si bien qu'un `> 0` ouvrirait l'établi
  // à des comptes ordinaires.
  CHECK_FALSE(StaffToolsAllowed(0, true));
  CHECK_FALSE(StaffToolsAllowed(1, true));
  CHECK_FALSE(StaffToolsAllowed(ro::ui::kStaffMinGroupLevel - 1, true));

  CHECK(StaffToolsAllowed(ro::ui::kStaffMinGroupLevel, true));
  CHECK(StaffToolsAllowed(99, true));
}

TEST_CASE("Établi : liaison perdue, plus personne n'accorde le droit", "[ui][staff]") {
  // Le niveau retenu est celui d'une session finie : ouvrir les outils à qui
  // perd sa connexion les donnerait à tout le monde.
  CHECK_FALSE(StaffToolsAllowed(0, false));
  CHECK_FALSE(StaffToolsAllowed(99, false));
}

TEST_CASE("Sol uni : le sol ne prend que trois canaux", "[ui][staff]") {
  FlatGroundLook flat;
  flat.color = IM_COL32(255, 128, 0, 64);
  const std::array<float, 3> taken = FlatGroundColorOf(flat);

  CHECK(taken[0] == 1.0F);
  CHECK(taken[1] > 0.5F);
  CHECK(taken[1] < 0.51F);
  CHECK(taken[2] == 0.0F);

  // ⚠ ET L'ALPHA NE CHANGE RIEN À CE QUI SORT. La passe du sol est opaque : deux
  // couleurs qui ne diffèrent que par lui donnent le même sol, et c'est pour
  // cela que la fenêtre le dit sous son sélecteur.
  FlatGroundLook transparent = flat;
  transparent.color = IM_COL32(255, 128, 0, 0);
  CHECK(FlatGroundColorOf(transparent) == taken);
}

TEST_CASE("Sol uni : le relevé voit un changement de COULEUR SEULE", "[ui][staff]") {
  // 🔴 LE CAS QUI SÉPARE LES DEUX ÉCRITURES. Le réglage se persiste par relevé :
  // à chaque image, ce que la fenêtre montre est comparé à ce que le fichier
  // porte. Une comparaison qui ne regarderait que l'interrupteur passerait tous
  // les essais où l'on coche la case — et perdrait chaque couleur choisie.
  FlatGroundLook stored;
  stored.on = true;
  FlatGroundLook seen = stored;
  seen.color = IM_COL32(0, 255, 0, 255);
  CHECK_FALSE(stored == seen);

  // Et l'alpha compte ICI, alors qu'il ne peint rien : ce qui s'enregistre est
  // la couleur que le nuancier montrera au prochain lancement, pas celle que le
  // sol prend.
  FlatGroundLook other_alpha = stored;
  other_alpha.color = stored.color ^ IM_COL32(0, 0, 0, 255);
  CHECK_FALSE(stored == other_alpha);

  CHECK(stored == FlatGroundLook{.on = true});
}

TEST_CASE("Sol uni : éteint au départ, et noir", "[ui][staff]") {
  // Un fond de capture allumé tout seul ouvrirait la partie sur un sol uni sans
  // rien pour l'expliquer.
  const FlatGroundLook flat;
  CHECK_FALSE(flat.on);
  const std::array<float, 3> black = FlatGroundColorOf(flat);
  CHECK(black[0] == 0.0F);
  CHECK(black[1] == 0.0F);
  CHECK(black[2] == 0.0F);
}

TEST_CASE("Zones de son : éteintes au départ, et le relevé les voit", "[ui][staff]") {
  // Des tracés allumés tout seuls couvriraient le monde sans rien pour les
  // expliquer ; et une case qui ne s'enregistre pas se lirait « ce réglage-là
  // ne tient pas ».
  const ro::ui::StaffToolsLook defaut;
  CHECK_FALSE(defaut.sound_zones);
  ro::ui::StaffToolsLook allume = defaut;
  allume.sound_zones = true;
  CHECK_FALSE(allume == defaut);
}

TEST_CASE("Zones de son : bruitages coupés, rien ne se dessine, et la case reste cochée",
          "[ui][staff]") {
  // 🔴 UNE SOURCE NE SE PLACE QU'EN ÉCOUTANT : bruitages coupés, les zones
  // n'auraient plus d'auditeur autour de qui se poser.
  ro::ui::StaffToolsState state;
  state.look.sound_zones = true;
  CHECK(ro::ui::SoundZonesShown(state));
  state.sound_audible = false;
  CHECK_FALSE(ro::ui::SoundZonesShown(state));
  // ⚠ LE CHOIX SURVIT : rallumer les bruitages rend les zones demandées.
  CHECK(state.look.sound_zones);
  state.sound_audible = true;
  CHECK(ro::ui::SoundZonesShown(state));
  // Et la case décochée ne montre rien, son allumé ou non.
  state.look.sound_zones = false;
  CHECK_FALSE(ro::ui::SoundZonesShown(state));
}

TEST_CASE("Établi : le ciel descend jusqu'au temps réel", "[ui][staff]") {
  constexpr float kOneMinuteInHours = 1.0F / 60.0F;
  ro::ui::StaffSkyClock sky;
  sky.speed_rank = 0;
  CHECK(sky.HoursPerMinute() == kOneMinuteInHours);
  // Le défaut : quatre heures par minute.
  CHECK(ro::ui::StaffSkyClock().HoursPerMinute() == 4.0F);
  // Un rang hors de la liste se borne à ses deux bouts.
  sky.speed_rank = -3;
  CHECK(sky.HoursPerMinute() == kOneMinuteInHours);
  sky.speed_rank = 99;
  CHECK(sky.HoursPerMinute() == 120.0F);
}
