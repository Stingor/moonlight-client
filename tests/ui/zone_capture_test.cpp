// L'ENREGISTREUR DE ZONE : ce qui se prouve sans écran.
//
// 🔴 CE QUI PEUT SE TROMPER ICI N'EST PAS UN PIXEL, C'EST UN NOMBRE QUI COMMANDE
// UNE RÉSERVATION. Le budget mémoire se calcule AVANT le départ, et un calcul
// faux ne se manifesterait qu'au milieu d'un enregistrement — c'est-à-dire par
// un clip perdu, chez le joueur.

#include "ro/ui/zone_capture.h"

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

namespace {

[[nodiscard]] ro::ui::ZoneCaptureSettings Wide() {
  return ro::ui::ZoneCaptureSettings{
      .zone = ro::ui::ZoneRect{.left = 10, .top = 20, .width = 1280, .height = 720},
      .frames_per_second = 10,
      .seconds = 6,
      .widest_output = 640,
      .start_delay_seconds = 3};
}

}  // namespace

TEST_CASE("Capture : la sortie garde les proportions", "[ui][capture]") {
  const ro::ui::ZoneRect out = ro::ui::OutputSize(Wide().zone, 640);
  CHECK(out.width == 640);
  CHECK(out.height == 360);
}

TEST_CASE("Capture : une zone plus étroite que la borne n'est pas agrandie", "[ui][capture]") {
  // ⚠ AGRANDIR NE RAJOUTE AUCUN DÉTAIL et quadruple le poids : un clip flou plus
  // lourd que l'original n'apporte rien à personne.
  const ro::ui::ZoneRect small{.left = 0, .top = 0, .width = 320, .height = 200};
  const ro::ui::ZoneRect out = ro::ui::OutputSize(small, 1280);
  CHECK(out.width == 320);
  CHECK(out.height == 200);
}

TEST_CASE("Capture : le budget compte les images réduites", "[ui][capture]") {
  // 🔴 LE PIÈGE QUE CE CAS TIENT : compter la ZONE et non la SORTIE donnerait
  // ici quatre fois trop, et ferait refuser des enregistrements qui tiennent
  // largement en mémoire.
  const ro::ui::ZoneCaptureSettings how = Wide();
  CHECK(ro::ui::CaptureBytes(how) == 60U * 640U * 360U * 4U);
}

TEST_CASE("Capture : ce qu'un fichier retouché à la main donne est ramené", "[ui][capture]") {
  // Le fichier de réglages s'ouvre dans un éditeur de texte : une durée de trois
  // cents secondes s'y écrit en une frappe, et réserverait trois mille images
  // avant le premier signe de vie.
  ro::ui::ZoneCaptureSettings absurd = Wide();
  absurd.frames_per_second = 9000;
  absurd.seconds = 300;
  absurd.widest_output = -4;
  absurd.start_delay_seconds = 99;
  const ro::ui::ZoneCaptureSettings sane = ro::ui::SaneCapture(absurd);
  CHECK(sane.frames_per_second == ro::ui::kFastestCapture);
  CHECK(sane.seconds == ro::ui::kLongestCapture);
  CHECK(sane.widest_output == ro::ui::kNarrowestOutput);
  CHECK(sane.start_delay_seconds == ro::ui::kLongestDelay);
}

TEST_CASE("Capture : un clic qui a glissé n'est pas une zone", "[ui][capture]") {
  // ⚠ ET ELLE EST EFFACÉE, pas rognée : une zone de trois pixels sur deux
  // laisserait le bouton « Filmer » actif pour un clip que personne ne peut
  // voir.
  ro::ui::ZoneCaptureSettings slipped = Wide();
  slipped.zone = ro::ui::ZoneRect{.left = 4, .top = 4, .width = 5, .height = 3};
  CHECK(ro::ui::SaneCapture(slipped).zone.empty());
}

TEST_CASE("Capture : sans zone, il n'y a rien à mesurer", "[ui][capture]") {
  ro::ui::ZoneCaptureSettings none = Wide();
  none.zone = ro::ui::ZoneRect{};
  CHECK(ro::ui::CaptureBytes(none) == 0U);
  CHECK(ro::ui::OutputSize(none.zone, 640).width == 0);
}

TEST_CASE("Capture : le budget d'usine tient largement", "[ui][capture]") {
  // 🔴 LE CAS QUI SERAIT PASSÉ INAPERÇU : si les réglages d'usine dépassaient
  // déjà le plafond, le bouton « Filmer » refuserait dès la première ouverture,
  // et rien dans l'interface ne dirait que c'est le RÉGLAGE et non l'outil.
  ro::ui::ZoneCaptureSettings factory;
  factory.zone = ro::ui::ZoneRect{.left = 0, .top = 0, .width = 1920, .height = 1080};
  CHECK(ro::ui::CaptureBytes(ro::ui::SaneCapture(factory)) < ro::ui::kLargestCaptureBytes);
}

TEST_CASE("Capture : un tracé abandonné hors de la phase ne survit pas", "[ui][capture]") {
  // 🔴 L'ÉTAT DU GLISSEMENT ÉTAIT UN STATIQUE : annulé ailleurs en plein tracé,
  // il restait « en cours », et la sélection suivante repartait de l'ancienne
  // ancre.
  ro::ui::UiContext ui;
  ro::ui::ZoneTraceState trace;
  trace.dragging = true;
  trace.anchor = ImVec2(10.0F, 20.0F);
  ro::ui::ZoneCaptureView view;
  view.phase = ro::ui::ZoneCapturePhase::kIdle;
  ro::ui::ZoneCaptureAsk asked;

  ui.BeginFrame(ro::ui::Viewport{.width_px = 800.0F, .height_px = 600.0F}, 1.0F / 60.0F);
  ro::ui::DrawZoneSelection(view, ro::ui::SkinArt{}, trace, &asked);
  ui.EndFrame();

  CHECK_FALSE(trace.dragging);
  CHECK_FALSE(asked.drawn);
}
