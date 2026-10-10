#pragma once

#include <cstdint>

#include "ro/render/camera.h"
#include "ro/render/screen_tint.h"

#include "afk/afk_input.h"
#include "afk/afk_settings.h"

namespace moonclient {

// L'ÉCRAN DE VEILLE : le jeu se regarde tourner.
//
// Passé un délai sans la moindre entrée, l'interface s'efface et la caméra prend
// la main : elle recule, se relève et tourne lentement autour du personnage. Le
// premier geste rend tout comme c'était.
//
// Trois leviers :
//
//   1. LA CAMÉRA — celle du jeu, dont ce module écrit la pose. ⚠ ET LA NÔTRE NE
//      LISSE PAS. Le moteur du client d'origine garde une pose COURANTE qui
//      rejoint une pose CIBLE d'un dixième par image, si bien qu'y écrire une
//      cible donne l'accélération gratuitement. Chez nous la pose écrite
//      EST celle qui se dessine : tout le mouvement vient de la rampe en S de
//      `Blend`, et il n'y a rien derrière elle pour l'adoucir davantage.
//
//   2. L'INTERFACE — un veto à la couche d'écran et un autre aux fenêtres, aux
//      deux uniques points de passage du dessin. ⚠ ILS NE FERMENT RIEN : ce qui
//      était ouvert l'est encore au réveil. Partir se faire un café ne doit pas
//      ranger ce qu'on avait sorti.
//
//   3. LA TEINTE — `ro::render::ScreenTint`, écrite pour ce module et pour lui
//      seul : c'est le seul post-traitement de ce client.
//
// 🔴 LE DÉCOR DE CONNEXION N'EN DÉPEND PAS : il a son propre chemin
// (`shell/backdrop_camera.h`), qui porte une pose EN DUR — la même image pour
// tout le monde. Faire dépendre le décor de ces réglages-ci changerait l'écran
// de connexion d'un joueur à l'autre, selon un curseur qu'aucun d'eux n'a bougé
// pour ça.

// Les bits du masque envoyé au serveur.
//
// ⚠ CONTRAT FIGÉ avec l'énumération des signes d'absence du serveur
// (`moonlight/src/map/clif.hpp`) : leurs valeurs ne changent plus, on n'en
// ajoute qu'à la suite. Le serveur laisse tomber ce qu'il ne connaît pas, si
// bien qu'un client en avance obtient les signes que le serveur sait donner —
// et jamais un état à moitié posé.
inline constexpr std::uint8_t kAfkAnnounceSleep = 0x01;
inline constexpr std::uint8_t kAfkAnnounceTag = 0x02;

// DE COMBIEN LE RETOUR VA PLUS VITE QUE L'ALLER. Le réglage du joueur décrit
// l'ENTRÉE ; la sortie répond à un geste qu'il vient de faire, pas à une mise
// en scène.
inline constexpr float kAfkWakeSpeedup = 3.0F;

enum class AfkPhase : std::uint8_t { kAwake, kEntering, kHeld, kLeaving };

class AfkScreen {
 public:
  [[nodiscard]] AfkPhase phase() const noexcept { return phase_; }

  // Vrai dès que la veille a commencé — rampe d'entrée comprise.
  [[nodiscard]] bool active() const noexcept { return phase_ != AfkPhase::kAwake; }

  // L'AVANCEMENT DE LA RAMPE EN COURS, de zéro à un.
  [[nodiscard]] float blend() const noexcept { return blend_; }

  // L'INTERFACE DOIT-ELLE DISPARAÎTRE DE CETTE IMAGE ?
  //
  // ⚠ DISTINCT D'`active()` : pendant la rampe de SORTIE la caméra revient
  // encore, mais l'interface, elle, est déjà rendue au joueur — sinon le réveil
  // se paierait de deux secondes d'écran nu, exactement quand il veut agir.
  [[nodiscard]] bool hiding_ui(const AfkSettings& settings) const noexcept;

  // Même règle pour le curseur : on vise avec, il ne peut pas manquer à l'appel
  // au moment où la main revient sur la souris.
  [[nodiscard]] bool hiding_cursor(const AfkSettings& settings) const noexcept;

  // La teinte de CETTE image, rampe comprise.
  [[nodiscard]] ro::render::ScreenTintLook tint(const AfkSettings& settings) const noexcept;

  // Quand la veille a commencé — ce que l'horloge compte.
  [[nodiscard]] std::uint32_t sleep_started_ms() const noexcept { return sleep_started_ms_; }

  // ENTRER EN VEILLE SUR-LE-CHAMP, sans attendre le délai — le bouton d'essai
  // du panneau, et le raccourci.
  //
  // ⚠ MARCHE MÊME QUAND LA VEILLE AUTOMATIQUE EST DÉCOCHÉE : c'est un geste
  // explicite, et le joueur qui se lève de sa chaise n'a pas à activer d'abord
  // un délai dont il ne veut pas.
  void StartNow(AfkIdle& idle, const ro::render::Camera& camera, std::uint32_t now_ms);

  // Réveil, avec sa rampe. Sans effet si la veille n'est pas en cours.
  void Wake(AfkIdle& idle, std::uint32_t now_ms);

  // FIN SANS RAMPE : la caméra est reposée où elle était, à l'instant même.
  // Pour ce qui n'est pas un réveil du joueur — changement de carte, sortie —,
  // là où dérouler deux secondes d'animation n'aurait aucun sens.
  void EndNow(AfkIdle& idle, ro::render::Camera& camera);

  // AVANCE LA VEILLE D'UNE IMAGE : la décision, la rampe, et la pose.
  //
  // ⚠ ELLE ÉCRIT DANS LA CAMÉRA, et c'est pour cela qu'elle la reçoit sans
  // `const` : la pose de repos se relève à l'endormissement et se repose au
  // réveil. Rien d'autre n'a à s'en souvenir.
  void Advance(const AfkSettings& settings, AfkIdle& idle, ro::render::Camera& camera,
               std::uint32_t now_ms);

  // ── PRÉVENIR L'ENTOURAGE ──────────────────────────────────────────────────
  //
  // Le « zzz » et le « [AFK] » ne sont PAS peints ici, et ne pourraient pas
  // l'être : ils se voient des autres joueurs, sur leurs machines. Tout ce que
  // fait le client est de dire au serveur qu'il s'est absenté. Le serveur, lui,
  // glisse le sommeil dans les paquets d'unité qu'il envoie au voisinage et
  // préfixe l'étiquette de nom ; il ne renvoie RIEN au dormeur, dont le client
  // lirait ce sommeil comme une paralysie.

  // Le masque que le serveur DEVRAIT connaître à cet instant.
  [[nodiscard]] std::uint8_t WantedAnnounce(const AfkSettings& settings,
                                            std::uint32_t idle_ms) const noexcept;

  // Ce qu'il en connaît — le miroir de ce qu'on lui a dit.
  [[nodiscard]] std::uint8_t announced() const noexcept { return announced_; }

  // ⚠ APRÈS L'ENVOI SEULEMENT. Noter un envoi qui n'a pas eu lieu figerait le
  // miroir sur un mensonge, et le joueur resterait annoncé absent en jouant.
  void NoteAnnounced(std::uint8_t mask) noexcept { announced_ = mask; }

  // LE PERSONNAGE A QUITTÉ LE MONDE : sa session meurt de l'autre côté, et
  // l'absence avec elle.
  //
  // 🔴 SURTOUT PAS À CHAQUE CHANGEMENT DE CARTE, où la session survit : sans
  // transition à envoyer, un joueur qui reprend la main juste après un
  // chargement resterait annoncé absent aux yeux de tous jusqu'à sa sieste
  // suivante.
  void ForgetAnnounce() noexcept { announced_ = 0; }

 private:
  void Begin(AfkIdle& idle, const ro::render::Camera& camera, std::uint32_t now_ms);
  void Restore(ro::render::Camera& camera) const;
  [[nodiscard]] float EaseBlend(const AfkSettings& settings, std::uint32_t now_ms) const noexcept;
  void PoseCamera(const AfkSettings& settings, ro::render::Camera& camera, float seconds);

  AfkPhase phase_ = AfkPhase::kAwake;
  float blend_ = 0.0F;
  std::uint32_t phase_started_ms_ = 0;
  std::uint32_t sleep_started_ms_ = 0;
  std::uint32_t last_step_ms_ = 0;

  // ── POSE DE REPOS, reprise telle quelle au réveil ─────────────────────────
  bool base_taken_ = false;
  float base_pitch_ = 0.0F;
  float base_yaw_ = 0.0F;
  float base_distance_ = 0.0F;

  // L'angle d'orbite accumulé depuis l'entrée en veille, en degrés. Il s'ajoute
  // au lacet de repos ; c'est lui qu'on ramène à zéro pendant la rampe de
  // sortie, pour que le joueur retrouve SON orientation et pas une autre.
  float spin_degrees_ = 0.0F;
  // L'orbite figée au réveil, ramenée dans [-180, 180] : la caméra rentre par
  // le chemin le plus court, au lieu de dérouler à l'envers les cinq tours
  // accumulés pendant une longue absence.
  float spin_at_wake_ = 0.0F;

  std::uint8_t announced_ = 0;
};

}  // namespace moonclient
