#include "afk/afk_screen.h"

#include <cmath>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/camera.h"
#include "ro/ui/afk_clock.h"

#include "afk/afk_input.h"
#include "afk/afk_settings.h"

using moonclient::AfkIdle;
using moonclient::AfkInput;
using moonclient::AfkInputKind;
using moonclient::AfkPhase;
using moonclient::AfkScreen;
using moonclient::AfkSettings;
using moonclient::AfkWake;
using moonclient::kAfkAnnounceSleep;
using moonclient::kAfkAnnounceTag;
using moonclient::kAfkWakeSpeedup;
using moonclient::kLeastAfkDelaySeconds;
using moonclient::kLeastAfkEaseSeconds;
using moonclient::kLeastAfkTiltDegrees;
using moonclient::kLeastAfkZoom;
using moonclient::kMostAfkAnnounceSeconds;
using moonclient::kMostAfkDelaySeconds;
using moonclient::kMostAfkEaseSeconds;
using moonclient::kMostAfkTiltDegrees;
using moonclient::kMostAfkZoom;

namespace {

constexpr std::uint32_t kMillisecondsPerSecond = 1000;
// Une image de jeu ordinaire. Toutes les avances des essais s'y comptent.
constexpr std::uint32_t kFrameMs = 16;
// Le bouton gauche, dans la numérotation de SDL — celle qu'`AfkInput` reprend.
constexpr std::uint8_t kLeftButton = 1;

// Le recul où l'essai place le joueur : il s'est RAPPROCHÉ à la molette.
//
// 🔴 ET SURTOUT PAS `kGameViewDistance`. Le recul de la veille se comptait
// d'abord en multiples de la distance COURANTE ; un essai parti du recul de jeu
// rend le même nombre dans les deux règles, et n'aurait donc jamais vu la
// différence. Le joueur, lui, l'a vue tout de suite : sa veille ne cadrait
// jamais deux fois pareil.
constexpr float kPlayerZoomedIn = 600.0F;

// Une caméra de JEU, qui n'est à aucun repos par défaut : une pose juste par
// hasard le serait aussi sur les valeurs d'usine.
[[nodiscard]] ro::render::Camera GameCamera() {
  ro::render::Camera camera;
  camera.look_at = glm::vec3(1500.0F, 40.0F, -900.0F);
  camera.distance = kPlayerZoomedIn;
  camera.pitch_degrees = ro::render::kGameElevationDegrees;
  camera.yaw_degrees = 37.0F;
  return camera;
}

// Des réglages où la veille est ACTIVE, avec une rampe courte : les essais
// comptent des images, pas des secondes.
[[nodiscard]] AfkSettings Sleepy() {
  AfkSettings how;
  how.enabled = true;
  how.delay_s = moonclient::kLeastAfkDelaySeconds;
  how.ease_s = 1.0F;
  return how;
}

// Fait tourner la boucle pendant `frames` images, en partant de `now_ms`, qui
// avance.
void Run(AfkScreen& screen, const AfkSettings& how, AfkIdle& idle, ro::render::Camera& camera,
         std::uint32_t& now_ms, int frames) {
  for (int rank = 0; rank < frames; ++rank) {
    now_ms += kFrameMs;
    screen.Advance(how, idle, camera, now_ms);
  }
}

// Un geste de clavier, celui qui repousse l'endormissement comme le réveil.
bool Key(AfkIdle& idle, std::uint32_t now_ms) {
  return idle.Note(AfkInput{.kind = AfkInputKind::kKeyPress}, now_ms);
}

}  // namespace

TEST_CASE("Veille : les défauts sont ceux que le joueur a réglés", "[app][afk]") {
  // 🔴 UN DÉFAUT QUI DÉRIVE NE SE VOIT PAS. Ces vingt-trois valeurs sont celles
  // que le joueur a réglées puis choisies pour tout le monde : elles ne sont
  // pas « ce qui semblait raisonnable », et c'est précisément ce qu'un essai
  // doit tenir.
  const AfkSettings how;

  CHECK(how.enabled);
  CHECK(how.delay_s == 300);
  CHECK(how.wake_on == static_cast<int>(AfkWake::kAny));

  CHECK(how.spin_deg_s == Catch::Approx(6.0F));
  // ⚠ UN ÉCART À LA VUE DE JEU, ET NON UN ANGLE ABSOLU. La veille que le joueur
  // a réglée plonge CINQ DEGRÉS SOUS le jeu. La vue de jeu est à cinquante —
  // mesurée —, et c'est l'ÉCART qui se transporte, pas le chiffre.
  CHECK(how.tilt_deg == Catch::Approx(ro::render::kGameElevationDegrees - 5.0F));
  CHECK(how.zoom_factor == Catch::Approx(0.65F));
  CHECK(how.ease_s == Catch::Approx(2.5F));

  CHECK(how.hide_ui);
  CHECK(how.hide_cursor);
  CHECK(how.vignette == Catch::Approx(0.45F));
  CHECK(how.desaturate == Catch::Approx(0.0F));
  CHECK(how.grain == Catch::Approx(0.0F));

  CHECK(how.show_clock);
  CHECK(how.show_away);
  CHECK(how.clock_anchor == 7);  // bas-centre
  CHECK(how.clock_margin == 233);
  CHECK(how.clock_scale == Catch::Approx(6.0F));
  CHECK(how.clock_shadow);
  CHECK(how.clock_color == 0xFFFFFFFFU);

  CHECK(how.announce);
  CHECK(how.announce_delay_s == 60);
  CHECK(how.announce_zzz);
  CHECK(how.announce_tag);

  // ⚠ ET TOUS TIENNENT DANS LEURS BORNES : un défaut hors plage se ferait
  // écrêter au premier dessin, et le joueur verrait le curseur sauter.
  CHECK(how.tilt_deg >= kLeastAfkTiltDegrees);
  CHECK(how.tilt_deg <= kMostAfkTiltDegrees);
  CHECK(how.zoom_factor >= kLeastAfkZoom);
  CHECK(how.zoom_factor <= kMostAfkZoom);
  CHECK(how.delay_s >= kLeastAfkDelaySeconds);
  CHECK(how.delay_s <= kMostAfkDelaySeconds);
  CHECK(how.announce_delay_s >= kLeastAfkDelaySeconds);
  CHECK(how.announce_delay_s <= kMostAfkAnnounceSeconds);
  CHECK(how.clock_margin <= ro::ui::kMostAfkClockMargin);
  CHECK(how.clock_scale <= ro::ui::kMostAfkClockScale);
  CHECK(how.ease_s >= kLeastAfkEaseSeconds);
  CHECK(how.ease_s <= kMostAfkEaseSeconds);
}

TEST_CASE("Veille : rien ne s'endort tant que le joueur n'a rien fait", "[app][afk]") {
  // ⚠ ZÉRO VEUT DIRE « À L'INSTANT », ET NON « L'AN ZÉRO ». Notre horloge part
  // de zéro au lancement, contrairement à celle du système : un client qui
  // prendrait l'absence de geste pour une éternité d'inactivité s'endormirait
  // avant le premier geste de la partie.
  AfkIdle idle;
  CHECK(idle.IdleMs(500000) == 0);

  AfkScreen screen;
  AfkSettings how = Sleepy();
  ro::render::Camera camera = GameCamera();
  std::uint32_t clock_ms = 500000;
  Run(screen, how, idle, camera, clock_ms, 3);
  CHECK(screen.phase() == AfkPhase::kAwake);
}

TEST_CASE("Veille : elle s'endort au bout du délai, et pas avant", "[app][afk]") {
  AfkIdle idle;
  AfkScreen screen;
  const AfkSettings how = Sleepy();
  ro::render::Camera camera = GameCamera();

  std::uint32_t clock_ms = 1000;
  CHECK_FALSE(Key(idle, clock_ms));

  const auto delay_ms = static_cast<std::uint32_t>(how.delay_s) * kMillisecondsPerSecond;
  clock_ms += delay_ms - 1;
  screen.Advance(how, idle, camera, clock_ms);
  CHECK(screen.phase() == AfkPhase::kAwake);

  clock_ms += 1;
  screen.Advance(how, idle, camera, clock_ms);
  CHECK(screen.phase() == AfkPhase::kEntering);
}

TEST_CASE("Veille : la caméra MONTE, elle ne s'enfonce pas", "[app][afk]") {
  // 🔴 LE SIGNE DE LA PLONGÉE, ET C'EST LE DÉFAUT QUI SE CACHE LE MIEUX. Le
  // moteur du client d'origine veut l'opposé du nôtre — une plongée négative —,
  // et reprendre son signe enverrait la caméra SOUS le terrain : on verrait le
  // dessous du monde.
  AfkIdle idle;
  AfkScreen screen;
  AfkSettings how = Sleepy();
  how.ease_s = 0.1F;  // qu'on atteigne la pose tenue en quelques images
  ro::render::Camera camera = GameCamera();
  const glm::vec3 eye_before = camera.Position();

  std::uint32_t clock_ms = 1000;
  screen.StartNow(idle, camera, clock_ms);
  Run(screen, how, idle, camera, clock_ms, 20);
  REQUIRE(screen.phase() == AfkPhase::kHeld);

  CHECK(camera.pitch_degrees > 0.0F);
  CHECK(camera.pitch_degrees == Catch::Approx(how.tilt_deg));
  CHECK(camera.Position().y > eye_before.y);
}

TEST_CASE("Veille : elle recule et tourne, dans les proportions demandées", "[app][afk]") {
  AfkIdle idle;
  AfkScreen screen;
  AfkSettings how = Sleepy();
  how.ease_s = 0.1F;
  how.zoom_factor = 2.0F;
  how.spin_deg_s = 6.0F;
  ro::render::Camera camera = GameCamera();
  const float rest_distance = camera.distance;
  const float rest_yaw = camera.yaw_degrees;

  std::uint32_t clock_ms = 1000;
  screen.StartNow(idle, camera, clock_ms);
  Run(screen, how, idle, camera, clock_ms, 10);
  REQUIRE(screen.phase() == AfkPhase::kHeld);
  // ⚠ LE RECUL DE JEU, ET NON CELUI DU JOUEUR : la pose est fixe.
  CHECK(camera.distance == Catch::Approx(ro::render::kGameViewDistance * how.zoom_factor));
  CHECK(camera.distance != Catch::Approx(rest_distance * how.zoom_factor));

  // Une seconde de plus, et l'orbite a tourné de ce qu'elle promet.
  const float yaw_before = camera.yaw_degrees;
  Run(screen, how, idle, camera, clock_ms, kMillisecondsPerSecond / kFrameMs);
  // ⚠ EN DÉCROISSANT : notre cap est l'opposé du client, et un réglage positif
  // tourne dans le sens que le joueur a choisi.
  CHECK(yaw_before - camera.yaw_degrees == Catch::Approx(how.spin_deg_s).margin(0.5));
  CHECK(camera.yaw_degrees < rest_yaw);
}

TEST_CASE("Veille : le cadrage ne dépend pas du zoom qu'on avait", "[app][afk]") {
  // 🔴 UN RECUL COMPTÉ EN MULTIPLES DE LA DISTANCE COURANTE cadrerait autrement
  // selon l'endroit où la molette se trouvait au déclenchement — et rien à
  // l'écran ne dirait pourquoi. Le nôtre part du recul de JEU.
  //
  // ⚠ DEUX DÉPARTS QUI NE SE RESSEMBLENT PAS, dont un PLUS LOIN que la pose
  // visée : la veille doit alors RAPPROCHER, ce qu'un essai parti de deux vues
  // rapprochées n'aurait jamais montré.
  const AfkSettings how = [] {
    AfkSettings tuned = Sleepy();
    tuned.ease_s = 0.1F;
    return tuned;
  }();

  const auto pose_after = [&how](float start_ms) {
    AfkIdle idle;
    AfkScreen screen;
    ro::render::Camera camera = GameCamera();
    camera.distance = start_ms;
    std::uint32_t clock_ms = 1000;
    screen.StartNow(idle, camera, clock_ms);
    Run(screen, how, idle, camera, clock_ms, 20);
    REQUIRE(screen.phase() == AfkPhase::kHeld);
    return camera.distance;
  };

  const float from_near = pose_after(300.0F);
  const float from_far = pose_after(5000.0F);
  CHECK(from_near == Catch::Approx(from_far));
  CHECK(from_near == Catch::Approx(ro::render::kGameViewDistance * how.zoom_factor));
  // Et la seconde s'est bien RAPPROCHÉE pour y arriver.
  CHECK(from_far < 5000.0F);
}

TEST_CASE("Veille : une image anormalement longue ne fait pas sauter l'orbite", "[app][afk]") {
  // ⚠ UN CHARGEMENT, UNE FENÊTRE RÉDUITE, UN POINT D'ARRÊT : l'horloge saute de
  // plusieurs secondes, et une orbite qui les rattraperait toutes d'un coup
  // ferait pivoter le décor d'un quart de tour entre deux images.
  AfkIdle idle;
  AfkScreen screen;
  AfkSettings how = Sleepy();
  how.ease_s = 0.1F;
  how.spin_deg_s = 30.0F;
  ro::render::Camera camera = GameCamera();

  std::uint32_t clock_ms = 1000;
  screen.StartNow(idle, camera, clock_ms);
  Run(screen, how, idle, camera, clock_ms, 10);
  const float yaw_before = camera.yaw_degrees;

  clock_ms += 30 * kMillisecondsPerSecond;  // trente secondes en une image
  screen.Advance(how, idle, camera, clock_ms);
  // Au plus le quart de seconde que la borne accorde, et non trente.
  CHECK(camera.yaw_degrees - yaw_before <= how.spin_deg_s * 0.25F + 0.01F);
}

TEST_CASE("Veille : le réveil rend la pose de repos, au degré près", "[app][afk]") {
  AfkIdle idle;
  AfkScreen screen;
  AfkSettings how = Sleepy();
  how.ease_s = 0.1F;
  ro::render::Camera camera = GameCamera();
  const ro::render::Camera rest = camera;

  std::uint32_t clock_ms = 1000;
  screen.StartNow(idle, camera, clock_ms);
  Run(screen, how, idle, camera, clock_ms, 10);
  REQUIRE(camera.pitch_degrees != rest.pitch_degrees);

  clock_ms += kFrameMs;
  // ⚠ LA TOUCHE PASSE, elle n'est pas avalée : elle porte une intention
  // explicite. C'est le CLIC qui est confisqué — voir l'essai qui le dit.
  CHECK_FALSE(Key(idle, clock_ms));
  Run(screen, how, idle, camera, clock_ms, 40);

  REQUIRE(screen.phase() == AfkPhase::kAwake);
  CHECK(camera.pitch_degrees == rest.pitch_degrees);
  CHECK(camera.yaw_degrees == rest.yaw_degrees);
  CHECK(camera.distance == rest.distance);
}

TEST_CASE("Veille : cinq tours d'orbite rentrent par le plus court chemin", "[app][afk]") {
  // ⚠ APRÈS VINGT MINUTES D'ABSENCE l'orbite vaut plusieurs tours. Les dérouler
  // à l'envers ferait tourner le décor pendant un temps absurde, juste au
  // moment où le joueur veut jouer.
  AfkIdle idle;
  AfkScreen screen;
  AfkSettings how = Sleepy();
  how.ease_s = 0.1F;
  how.spin_deg_s = 30.0F;
  ro::render::Camera camera = GameCamera();
  const float rest_yaw = camera.yaw_degrees;

  std::uint32_t clock_ms = 1000;
  screen.StartNow(idle, camera, clock_ms);
  // ⚠ LA DURÉE EST CHOISIE POUR QUE L'ORBITE FINISSE AU-DELÀ DU DEMI-TOUR :
  // cinq tours pleins retomberaient à zéro, où le repli n'aurait rien à faire
  // et où l'essai ne verrait donc rien. Huit secondes et demie à trente degrés
  // font deux cent cinquante degrés et quelques — le mauvais côté.
  Run(screen, how, idle, camera, clock_ms,
      static_cast<int>(8.5F * kMillisecondsPerSecond / kFrameMs));
  // Notre cap décroît : voir « elle recule et tourne ».
  REQUIRE(rest_yaw - camera.yaw_degrees > 180.0F);

  clock_ms += kFrameMs;
  CHECK_FALSE(Key(idle, clock_ms));  // la touche passe : elle porte une intention
  clock_ms += kFrameMs;
  screen.Advance(how, idle, camera, clock_ms);
  REQUIRE(screen.phase() == AfkPhase::kLeaving);
  // La caméra rentre par l'autre côté : moins d'un demi-tour à parcourir.
  CHECK(std::abs(camera.yaw_degrees - rest_yaw) <= 180.0F);
}

TEST_CASE("Veille : le retour va trois fois plus vite que l'aller", "[app][afk]") {
  AfkIdle idle;
  AfkScreen screen;
  AfkSettings how = Sleepy();
  how.ease_s = 3.0F;
  ro::render::Camera camera = GameCamera();

  std::uint32_t clock_ms = 1000;
  screen.StartNow(idle, camera, clock_ms);
  // À mi-chemin de l'aller, la rampe n'est pas finie.
  Run(screen, how, idle, camera, clock_ms,
      static_cast<int>(1.5F * kMillisecondsPerSecond / kFrameMs));
  CHECK(screen.phase() == AfkPhase::kEntering);
  Run(screen, how, idle, camera, clock_ms,
      static_cast<int>(2.0F * kMillisecondsPerSecond / kFrameMs));
  REQUIRE(screen.phase() == AfkPhase::kHeld);

  clock_ms += kFrameMs;
  CHECK_FALSE(Key(idle, clock_ms));  // la touche passe : elle porte une intention
  // Le retour tient en `ease_s / 3` : une seconde et demie l'achève largement,
  // là où l'aller en demandait trois.
  Run(screen, how, idle, camera, clock_ms,
      static_cast<int>(1.5F * kMillisecondsPerSecond / kFrameMs));
  CHECK(screen.phase() == AfkPhase::kAwake);
  CHECK(kAfkWakeSpeedup > 1.0F);
}

TEST_CASE("Veille : décocher la veille AUTOMATIQUE ne coupe pas celle qu'on a lancée",
          "[app][afk]") {
  // ⚠ `enabled` GOUVERNE LE DÉCLENCHEMENT, PAS LA VEILLE EN COURS. L'y remettre
  // couperait net toute veille lancée à la main — au bouton d'essai — chez
  // quiconque n'a pas coché la mise en veille automatique, c'est-à-dire
  // précisément ceux à qui ce geste sert.
  AfkIdle idle;
  AfkScreen screen;
  AfkSettings how;
  // ⚠ DÉCOCHÉE EXPRÈS, ET NON LAISSÉE À SON DÉFAUT : celui-ci est allumé depuis
  // que le joueur a choisi ses valeurs, et un essai qui s'appuie sur un défaut
  // cesse d'éprouver ce qu'il croit le jour où ce défaut change.
  how.enabled = false;
  how.ease_s = 0.1F;
  ro::render::Camera camera = GameCamera();

  std::uint32_t clock_ms = 1000;
  screen.StartNow(idle, camera, clock_ms);
  Run(screen, how, idle, camera, clock_ms, 20);
  CHECK(screen.phase() == AfkPhase::kHeld);
}

TEST_CASE("Veille : l'interface revient AVANT la caméra", "[app][afk]") {
  // ⚠ PENDANT LA RAMPE DE SORTIE la caméra revient encore, mais le joueur vient
  // d'agir : lui faire attendre deux secondes d'écran nu, exactement quand il
  // veut jouer, serait le pire moment pour le faire.
  AfkIdle idle;
  AfkScreen screen;
  AfkSettings how = Sleepy();
  how.ease_s = 3.0F;
  ro::render::Camera camera = GameCamera();

  std::uint32_t clock_ms = 1000;
  screen.StartNow(idle, camera, clock_ms);
  Run(screen, how, idle, camera, clock_ms, 10);
  CHECK(screen.hiding_ui(how));
  CHECK(screen.hiding_cursor(how));

  clock_ms += kFrameMs;
  CHECK_FALSE(Key(idle, clock_ms));  // la touche passe : elle porte une intention
  clock_ms += kFrameMs;
  screen.Advance(how, idle, camera, clock_ms);
  REQUIRE(screen.phase() == AfkPhase::kLeaving);
  CHECK_FALSE(screen.hiding_ui(how));
  CHECK_FALSE(screen.hiding_cursor(how));
  CHECK(screen.active());
}

TEST_CASE("Veille : la teinte suit la rampe, et s'éteint avec elle", "[app][afk]") {
  AfkIdle idle;
  AfkScreen screen;
  AfkSettings how = Sleepy();
  how.ease_s = 1.0F;
  ro::render::Camera camera = GameCamera();

  CHECK_FALSE(screen.tint(how).visible());

  std::uint32_t clock_ms = 1000;
  screen.StartNow(idle, camera, clock_ms);
  Run(screen, how, idle, camera, clock_ms, 5);
  const float partway = screen.tint(how).vignette;
  CHECK(partway > 0.0F);
  CHECK(partway < how.vignette);

  Run(screen, how, idle, camera, clock_ms, kMillisecondsPerSecond / kFrameMs);
  REQUIRE(screen.phase() == AfkPhase::kHeld);
  CHECK(screen.tint(how).vignette == Catch::Approx(how.vignette));
}

TEST_CASE("Veille : la fin SANS RAMPE repose la caméra sur-le-champ", "[app][afk]") {
  // ⚠ LE CHANGEMENT DE CARTE : dérouler deux secondes d'animation sur un décor
  // qu'on est en train de remplacer n'aurait aucun sens, et la pose de repos
  // n'aurait plus personne pour la remettre.
  AfkIdle idle;
  AfkScreen screen;
  AfkSettings how = Sleepy();
  ro::render::Camera camera = GameCamera();
  const ro::render::Camera rest = camera;

  std::uint32_t clock_ms = 1000;
  screen.StartNow(idle, camera, clock_ms);
  Run(screen, how, idle, camera, clock_ms, 10);
  REQUIRE(camera.pitch_degrees != rest.pitch_degrees);

  screen.EndNow(idle, camera);
  CHECK(screen.phase() == AfkPhase::kAwake);
  CHECK(camera.pitch_degrees == rest.pitch_degrees);
  CHECK(camera.yaw_degrees == rest.yaw_degrees);
  CHECK(camera.distance == rest.distance);
}

// ── LE COMPTEUR D'INACTIVITÉ ────────────────────────────────────────────────

TEST_CASE("Veille : le relâchement d'un clic AVALÉ ne réveille pas", "[app][afk]") {
  // 🔴 C'EST CE QUI REND « ESSAYER MAINTENANT » UTILISABLE. Le clic sur le
  // bouton endort le client entre l'appui et le relâchement, et ce relâchement
  // — le second temps d'un geste déjà fini — rouvrirait les yeux aussitôt
  // fermés : la veille durerait une image, et le bouton passerait pour cassé.
  AfkIdle idle;
  std::uint32_t clock_ms = 1000;

  // L'appui, hors veille : il passe.
  CHECK_FALSE(
      idle.Note(AfkInput{.kind = AfkInputKind::kMousePress, .button = kLeftButton}, clock_ms));

  // Puis la veille commence, bouton encore enfoncé.
  idle.SetSleeping(true);
  idle.SwallowHeldButtons(1U << (kLeftButton - 1U));
  const std::uint32_t sleep_ms = clock_ms;

  clock_ms += 100;
  CHECK(idle.Note(AfkInput{.kind = AfkInputKind::kMouseRelease, .button = kLeftButton}, clock_ms));
  CHECK_FALSE(idle.WokeSince(sleep_ms));
}

TEST_CASE("Veille : un relâchement ORDINAIRE compte comme activité, sans réveiller", "[app][afk]") {
  AfkIdle idle;
  std::uint32_t clock_ms = 1000;
  idle.SetSleeping(true);
  const std::uint32_t sleep_ms = clock_ms;

  clock_ms += 100;
  // Son appui n'a pas été avalé : il passe, et repousse l'endormissement.
  CHECK_FALSE(
      idle.Note(AfkInput{.kind = AfkInputKind::kMouseRelease, .button = kLeftButton}, clock_ms));
  CHECK(idle.IdleMs(clock_ms) == 0);
  // Mais il ne réveille pas : c'est la fin d'un geste, pas une intention neuve.
  CHECK_FALSE(idle.WokeSince(sleep_ms));

  // ⚠ ET LA TOUCHE RELÂCHÉE NON PLUS. Sans cette règle, le raccourci qui LANCE
  // la veille la terminerait de son propre relâchement, une image plus tard.
  clock_ms += 100;
  CHECK_FALSE(idle.Note(AfkInput{.kind = AfkInputKind::kKeyRelease}, clock_ms));
  CHECK(idle.IdleMs(clock_ms) == 0);
  CHECK_FALSE(idle.WokeSince(sleep_ms));
}

TEST_CASE("Veille : le clic qui réveille est avalé, la touche non", "[app][afk]") {
  // ⚠ LE DÉCOR A TOURNÉ : un clic tomberait ailleurs que là où l'on croit
  // viser. Une touche, elle, porte une intention explicite — elle passe.
  AfkIdle idle;
  std::uint32_t clock_ms = 1000;
  idle.SetSleeping(true);

  CHECK(idle.Note(AfkInput{.kind = AfkInputKind::kMousePress, .button = kLeftButton}, clock_ms));
  CHECK(idle.WokeSince(clock_ms - 1));

  AfkIdle other;
  other.SetSleeping(true);
  CHECK_FALSE(other.Note(AfkInput{.kind = AfkInputKind::kKeyPress}, clock_ms));
  CHECK(other.WokeSince(clock_ms - 1));
}

TEST_CASE("Veille : ce qui n'a pas le droit de réveiller n'a pas le droit d'agir", "[app][afk]") {
  // ⚠ UNE TOUCHE QUI TRAVERSERAIT SANS RÉVEILLER lancerait un sort sur un monde
  // que le joueur ne voit pas, sous un angle qui n'est pas le sien. Demander
  // « seule la souris me réveille », c'est demander que le clavier soit sans
  // effet — pas qu'il agisse en aveugle.
  AfkIdle idle;
  idle.SetWakeMode(static_cast<int>(AfkWake::kMouse));
  idle.SetSleeping(true);
  const std::uint32_t clock_ms = 1000;

  CHECK(idle.Note(AfkInput{.kind = AfkInputKind::kKeyPress}, clock_ms));
  CHECK_FALSE(idle.WokeSince(clock_ms - 1));
}

TEST_CASE("Veille : ce qui ne réveille pas repousse quand même l'endormissement", "[app][afk]") {
  // 🔴 DEUX HORODATAGES, ET LES CONFONDRE CASSE L'UN OU L'AUTRE. Filtrer le
  // compteur d'inactivité ferait s'endormir le client pendant que le joueur
  // promène sa souris, au seul motif qu'il a demandé un réveil au clavier.
  AfkIdle idle;
  idle.SetWakeMode(static_cast<int>(AfkWake::kKeyboard));
  std::uint32_t clock_ms = 1000;

  CHECK_FALSE(
      idle.Note(AfkInput{.kind = AfkInputKind::kMouseMove, .x = 10.0F, .y = 20.0F}, clock_ms));
  clock_ms += 5000;
  CHECK(idle.IdleMs(clock_ms) == 5000);

  CHECK_FALSE(
      idle.Note(AfkInput{.kind = AfkInputKind::kMouseMove, .x = 11.0F, .y = 20.0F}, clock_ms));
  CHECK(idle.IdleMs(clock_ms) == 0);
  // …mais la souris n'a pas le droit de réveiller, et ne l'a pas fait.
  CHECK_FALSE(idle.WokeSince(clock_ms - 1));
}

TEST_CASE("Veille : une souris IMMOBILE ne repousse rien", "[app][afk]") {
  // ⚠ LE SYSTÈME RENVOIE UN MOUVEMENT quand la fenêtre bouge sous une souris
  // qui n'a pas bougé. Sans ce test de position, une animation quelconque sous
  // le curseur suffirait à empêcher la veille indéfiniment.
  AfkIdle idle;
  std::uint32_t clock_ms = 1000;
  CHECK_FALSE(
      idle.Note(AfkInput{.kind = AfkInputKind::kMouseMove, .x = 42.0F, .y = 7.0F}, clock_ms));

  clock_ms += 5000;
  CHECK_FALSE(
      idle.Note(AfkInput{.kind = AfkInputKind::kMouseMove, .x = 42.0F, .y = 7.0F}, clock_ms));
  CHECK(idle.IdleMs(clock_ms) == 5000);
}

// ── PRÉVENIR L'ENTOURAGE ────────────────────────────────────────────────────

TEST_CASE("Veille : l'annonce a son propre délai et ne dépend pas de la veille", "[app][afk]") {
  // 🔴 INDÉPENDANTE DE `enabled`, et c'est tout l'intérêt : signaler son absence
  // et se regarder tourner sont deux envies distinctes, or la seconde coûte au
  // joueur son interface et sa caméra.
  AfkScreen screen;
  AfkSettings how;
  how.enabled = false;  // décochée EXPRÈS : c'est ce que cet essai sépare
  how.announce = true;
  how.announce_delay_s = 300;

  CHECK(screen.WantedAnnounce(how, 299 * kMillisecondsPerSecond) == 0);
  CHECK(screen.WantedAnnounce(how, 300 * kMillisecondsPerSecond) ==
        (kAfkAnnounceSleep | kAfkAnnounceTag));
}

TEST_CASE("Veille : l'annonce ne porte que les signes demandés", "[app][afk]") {
  AfkScreen screen;
  AfkSettings how;
  how.announce = true;
  how.announce_delay_s = 10;
  how.announce_zzz = false;
  const std::uint32_t long_enough = 60 * kMillisecondsPerSecond;
  CHECK(screen.WantedAnnounce(how, long_enough) == kAfkAnnounceTag);

  how.announce_zzz = true;
  how.announce_tag = false;
  CHECK(screen.WantedAnnounce(how, long_enough) == kAfkAnnounceSleep);

  how.announce = false;
  CHECK(screen.WantedAnnounce(how, long_enough) == 0);
}

TEST_CASE("Veille : le miroir de l'annonce ne retombe QUE hors du monde", "[app][afk]") {
  // 🔴 SURTOUT PAS À CHAQUE CHANGEMENT DE CARTE, où la session survit côté
  // serveur : sans transition à envoyer, un joueur qui reprend la main juste
  // après un chargement resterait annoncé absent aux yeux de tous jusqu'à sa
  // sieste suivante.
  AfkScreen screen;
  screen.NoteAnnounced(kAfkAnnounceSleep | kAfkAnnounceTag);
  CHECK(screen.announced() == (kAfkAnnounceSleep | kAfkAnnounceTag));

  AfkIdle idle;
  ro::render::Camera camera = GameCamera();
  screen.EndNow(idle, camera);  // ce que fait un changement de carte
  CHECK(screen.announced() == (kAfkAnnounceSleep | kAfkAnnounceTag));

  screen.ForgetAnnounce();
  CHECK(screen.announced() == 0);
}
