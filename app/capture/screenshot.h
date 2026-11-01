#pragma once

#include <atomic>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace moonclient {

// LE NOM D'UNE CAPTURE PRISE À `stamp` (« 2026-11-01_14-03-22 »), le premier
// libre dans `folder` : une seconde capture dans la même seconde prend un
// suffixe au lieu d'écraser la première.
[[nodiscard]] std::filesystem::path FreeScreenshotPath(const std::filesystem::path& folder,
                                                       std::string_view stamp);

// ── LA CAPTURE D'ÉCRAN ENTIÈRE, ENREGISTRÉE DANS `ScreenShot/` ──────────────
//
// 🔴 LA PRISE À LA FIN DE L'IMAGE, AVANT DE PRÉSENTER, comme celle de
// l'enregistreur de zone : plus tôt, l'écran n'aurait pas ses fenêtres.
//
// 🔴 L'ENCODAGE ET L'ÉCRITURE PARTENT SUR UN FIL À LUI. Compresser un écran
// entier en PNG prend une fraction de seconde : sur la boucle, c'est une image
// figée à chaque pression. Tant que le fil n'a pas rendu la main, une nouvelle
// demande est refusée et le dit.
class ScreenShooter {
 public:
  // Les captures vont sous `ScreenShot/` de l'installation `client_root`.
  explicit ScreenShooter(const std::filesystem::path& client_root);
  ScreenShooter(const ScreenShooter&) = delete;
  ScreenShooter& operator=(const ScreenShooter&) = delete;
  ScreenShooter(ScreenShooter&&) = delete;
  ScreenShooter& operator=(ScreenShooter&&) = delete;
  ~ScreenShooter();

  // Une capture à prendre à la fin de l'image en cours.
  void Ask() noexcept { due_ = true; }

  // LA PRISE, en pixels du tampon de dessin, si une capture est demandée.
  void GrabIfDue(int drawable_width, int drawable_height);

  // Ce qu'il faut dire au joueur, une fois : la capture écrite, ou son échec.
  // Récolte aussi le fil fini. À chaque image.
  [[nodiscard]] std::optional<std::string> TakeReport();

 private:
  std::filesystem::path folder_;
  bool due_ = false;
  std::thread writer_;
  std::atomic<bool> written_{false};
  // Écrit par le fil, lu après qu'il a posé `written_`.
  std::string outcome_;
  // Un refus dit sur la boucle même, sans passer par le fil.
  std::optional<std::string> refusal_;
};

}  // namespace moonclient
