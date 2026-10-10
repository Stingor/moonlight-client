#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "ro/formats/image.h"
#include "ro/render/screen_grab.h"
#include "ro/ui/zone_capture.h"

namespace moonclient {

// LES DEUX TAILLES D'UNE MÊME IMAGE.
//
// 🔴 ELLES DIFFÈRENT SUR UN ÉCRAN À FACTEUR D'ÉCHELLE, et c'est le piège de tout
// ce module : la souris et les fenêtres parlent en POINTS, la relecture d'image
// parle en PIXELS du tampon de dessin. Une zone reportée telle quelle viserait
// le quart haut-gauche de ce qu'on a tracé.
struct FrameSurface {
  int drawable_width = 0;
  int drawable_height = 0;
  int point_width = 0;
  int point_height = 0;
};

// ── L'ENREGISTREUR DE ZONE, CÔTÉ MACHINE ────────────────────────────────────
//
// `ui` trace le rectangle et montre le témoin ; celui-ci tient l'horloge, relit
// l'écran, encode et dépose dans le presse-papier. Voir `zone_capture.h` pour la
// règle qui commande tout le dessin, et `capture_clipboard.h` pour la livraison.
//
// 🔴 LES IMAGES SONT RÉSERVÉES AVANT LE DÉPART. Une allocation qui échouerait au
// milieu d'un enregistrement laisserait un clip tronqué, et le joueur l'aurait
// appris en le collant. Le refus, lui, arrive avant qu'on ait compté « 3 ».
//
// 🔴 L'ENCODAGE PART SUR UN FIL À LUI. Quantifier et compresser des centaines de
// milliers de pixels prend des secondes ; sur le fil de la boucle, ce sont des
// secondes d'image figée. Le fil ne touche qu'aux images qu'on lui a CÉDÉES, et
// l'étape reste « encodage » tant qu'il n'a pas rendu la main — ce qui interdit
// un second enregistrement entre-temps.
class ZoneRecorder {
 public:
  ZoneRecorder() = default;
  ZoneRecorder(const ZoneRecorder&) = delete;
  ZoneRecorder& operator=(const ZoneRecorder&) = delete;
  ZoneRecorder(ZoneRecorder&&) = delete;
  ZoneRecorder& operator=(ZoneRecorder&&) = delete;
  ~ZoneRecorder();

  // Les réglages du fichier, relus à chaque image : ils changent sous nos pieds
  // quand le joueur promène un curseur.
  void Follow(const ro::ui::ZoneCaptureSettings& how) noexcept { how_ = how; }

  // ── LES GESTES ─────────────────────────────────────────────────────────────

  // Ouvre le tracé à la souris. ⚠ PENDANT UN DÉCOMPTE, IL RENONCE AU DÉPART
  // plutôt que de refuser : c'est le moment où l'on s'aperçoit qu'on a mal
  // cadré.
  void AskZone();
  // Lance, ou arrête avant la fin. Rien si la zone manque.
  void ToggleRecording(std::uint32_t now_ms);
  // Une image fixe de la même zone, prise à la fin de l'image en cours.
  void AskPicture();
  void Cancel();

  // Ce que le tracé a rendu. La zone neuve, quand il y en a une — c'est à
  // l'appelant de l'enregistrer.
  [[nodiscard]] std::optional<ro::ui::ZoneRect> Note(const ro::ui::ZoneCaptureAsk& asked);

  // Fait avancer l'horloge et récolte l'encodage fini. À chaque image.
  void Tick(std::uint32_t now_ms);

  // LA PRISE, À LA FIN DE L'IMAGE ET AVANT DE PRÉSENTER.
  //
  // 🔴 C'EST LA PLACE QUI DÉCIDE DE CE QU'ON VOIT DANS LE CLIP : appelée plus
  // tôt, elle rendrait un écran sans ses fenêtres. Le facteur d'échelle de
  // l'écran est le rapport entre le tampon de dessin et les points d'interface.
  void GrabIfDue(const FrameSurface& surface, std::uint32_t now_ms);

  [[nodiscard]] ro::ui::ZoneCaptureView view(bool show_zone) const;
  [[nodiscard]] bool busy() const noexcept { return phase_ != ro::ui::ZoneCapturePhase::kIdle; }
  [[nodiscard]] bool selecting() const noexcept {
    return phase_ == ro::ui::ZoneCapturePhase::kSelecting;
  }

 private:
  // Valide, réserve, et passe au décompte. Rend faux en disant pourquoi.
  [[nodiscard]] bool Arm(std::uint32_t now_ms);
  void Start(std::uint32_t now_ms);
  void Finish();
  void Say(const char* what);
  // La zone, des points d'interface vers les pixels du tampon de dessin.
  [[nodiscard]] ro::render::ScreenRegion InPixels(const FrameSurface& surface) const;

  ro::ui::ZoneCaptureSettings how_;
  ro::ui::ZoneCapturePhase phase_ = ro::ui::ZoneCapturePhase::kIdle;

  // ⚠ FACULTATIVE : une cible de rendu n'a pas d'état neutre, et celle-ci est
  // taillée à la sortie voulue — donc refaite à chaque enregistrement.
  std::optional<ro::render::ScreenGrab> grab_;
  std::vector<ro::formats::Image> frames_;
  int seconds_left_ = 0;
  int wanted_frames_ = 0;
  std::uint32_t interval_ms_ = 100;
  std::uint32_t next_grab_ms_ = 0;
  std::uint32_t countdown_end_ms_ = 0;
  // Une image fixe attendue à la fin de l'image en cours.
  bool picture_due_ = false;

  std::thread encoder_;
  std::atomic<bool> encoded_{false};
  std::vector<std::uint8_t> bytes_;

  // Le dernier mot dit au joueur. ⚠ UNE COPIE, ET NON LE POINTEUR DE `Tr` :
  // celui-ci appartient au catalogue et meurt au changement de langue.
  std::string message_;
  std::uint32_t message_ms_ = 0;
};

}  // namespace moonclient
