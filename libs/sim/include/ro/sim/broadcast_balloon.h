#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ro::sim {

// LES BULLES D'ANNONCE, EN HAUT DE L'ÉCRAN.
//
// Une annonce du serveur ne s'écrit pas seulement dans la conversation : le
// client la pose aussi, encadrée, au milieu du haut de l'écran, où elle reste
// vingt secondes. Le monde ne fait que DÉPOSER ce qui doit s'y montrer ;
// l'application en tient l'affichage — durée, pile, place —, parce que ce sont
// des réglages et que le monde ne connaît pas l'horloge.
//
// ⚠ LE TEXTE EST DÉJÀ COMPOSÉ, en UTF-8, et déjà masqué quand il le doit :
// une bulle montre ce que la conversation montre.

// LA FAMILLE D'UNE BULLE : elle décide où la bulle se range et combien de ses
// sœurs restent à l'écran avec elle.
//
// ⚠ L'ORDRE EST CELUI DE L'ÉCRAN : les annonces au-dessus du chuchotement de
// l'équipe. La chasse a son propre bandeau, plus bas.
enum class BalloonFamily : std::uint8_t {
  // Une annonce, un avis du serveur à tous.
  kAnnouncement = 0,
  // Ce qu'un membre de l'équipe chuchote : il se voit même fenêtres fermées.
  kStaffWhisper = 1,
  // L'avancée des chasses d'une quête, une ligne par chasse.
  kHunt = 2,
};
inline constexpr std::size_t kBalloonFamilyCount = 3;

// Le corps de police que le client donne à une bulle, en pixels. L'avis
// d'étoile en prend un plus grand ; l'annonce habillée porte le sien dans le
// paquet.
inline constexpr std::uint16_t kBalloonFontPixels = 12;
inline constexpr std::uint16_t kStarBalloonFontPixels = 14;

// Une ligne d'une bulle, et sa couleur en `0xRRGGBB`. ⚠ JAMAIS ZÉRO pour « pas
// de couleur » : toute source d'annonce en impose une.
struct BalloonLine {
  std::string text;
  std::uint32_t color = 0;
};

// UNE BULLE À MONTRER, datée de sa réception.
struct BalloonNotice {
  BalloonFamily family = BalloonFamily::kAnnouncement;
  std::vector<BalloonLine> lines;
  std::uint16_t font_px = kBalloonFontPixels;
  // LE TEXTE SE REPLIE-T-IL ? Seule l'annonce simple (`ZC_BROADCAST`) se coupe
  // en plusieurs rangées chez le client : les autres sources tiennent sur une
  // seule, quelle que soit leur longueur.
  bool wraps = false;
  std::uint32_t heard_ms = 0;
};

// Au-delà, la plus ancienne bulle en attente part : l'application les prend à
// chaque image, et un tel retard ne vient que d'une partie figée.
inline constexpr std::size_t kBalloonNewsKept = 64;

// LA FILE DES BULLES DÉPOSÉES ET PAS ENCORE PRISES.
class BalloonNews {
 public:
  // Une bulle sans ligne ne se dépose pas : elle n'aurait rien à montrer.
  void Post(BalloonNotice notice);
  // Les bulles arrivées depuis la dernière prise, dans leur ordre ; la file se
  // vide.
  [[nodiscard]] std::vector<BalloonNotice> Take();

 private:
  std::vector<BalloonNotice> notices_;
};

}  // namespace ro::sim
