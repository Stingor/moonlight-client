#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"

namespace ro::formats {

// Un calque d'une image d'animation : quelle image du .spr, posée où.
//
// Champs BRUTS, sans interprétation géométrique — c'est l'appelant qui compose.
struct ActLayer {
  float offset_x = 0.0F;
  float offset_y = 0.0F;

  // Numéro de l'image DANS SA SECTION, et non dans la concaténation des deux.
  // -1 désigne un calque vide, qui existe réellement dans les fichiers.
  std::int32_t frame_index = -1;
  // 0 = section palettisée, 1 = section BGRA du .spr.
  std::int32_t section = 0;

  bool mirrored = false;
  // Teinte multiplicative, en rouge, vert, bleu, alpha.
  std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
  float scale_x = 1.0F;
  float scale_y = 1.0F;
  std::int32_t rotation_degrees = 0;

  // Les dimensions que le fichier DÉCLARE pour le calque, à partir de la 2.5 ;
  // vides avant. ⚠ ELLES PEUVENT MENTIR sur l'image du .spr : la composition
  // d'un acteur ne les lit pas. Seul le bouton d'image d'une fenêtre s'en sert,
  // parce que le jeu y pose chaque calque par le coin qu'elles donnent.
  std::optional<std::int32_t> declared_width;
  std::optional<std::int32_t> declared_height;
};

// Point d'ancrage d'une image d'animation.
//
// ⚠ C'est ce qui permet d'ACCROCHER deux .act l'un à l'autre, et il n'y a pas
// d'autre moyen : la tête se pose sur le corps par la DIFFÉRENCE entre l'ancre
// du corps et celle de la tête, et les coiffes se posent sur la tête pareil.
//
// Absentes avant la version 2.3, et absentes aussi des fichiers 2.3 et 2.4 qui
// déclenchent le repli de lecture. Une liste vide vaut « pas de décalage »,
// jamais une erreur : mieux vaut une pièce mal attachée que pas de personnage.
struct ActAnchor {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t attribute = 0;
};

struct ActFrame {
  std::vector<ActLayer> layers;
  std::vector<ActAnchor> anchors;
  // Index dans ActAnimation::sound_files, ou -1.
  std::int32_t sound_index = -1;
};

// Durée d'une image, en millisecondes, pour une unité de cadence. C'est le
// pas du client : il divise le temps écoulé par 24 pour trouver l'image.
inline constexpr float kMillisecondsPerSpeedUnit = 24.0F;

// Cadence retenue quand le fichier n'en déclare pas, soit 144 ms par image.
inline constexpr float kDefaultSpeedUnits = 6.0F;

struct ActAction {
  std::vector<ActFrame> frames;
  // Cadence déclarée, en unités RO. La durée d'une image s'en déduit par
  // kMillisecondsPerSpeedUnit — le nombre nu ne veut rien dire seul.
  float speed_units = kDefaultSpeedUnits;

  [[nodiscard]] float frame_interval_ms() const noexcept {
    return speed_units * kMillisecondsPerSpeedUnit;
  }
};

// Un fichier .act : les animations d'un sprite.
//
// Le recensement des 130 716 .act de l'installation donne les versions :
//
//   2.5   126 461      2.4   3553      2.3   395      2.1   304      2.0   3
//
// Ni 2.2 ni 2.6 n'y figurent. La 2.6 est tout de même lue — elle ne diffère que
// par des décalages en flottants, ce qui se vérifie sur des données
// synthétiques ; à l'inverse d'une section compressée qu'on ne pourrait pas
// éprouver.
class ActAnimation {
 public:
  [[nodiscard]] static core::Result<ActAnimation> Parse(std::span<const std::uint8_t> data);

  // Version condensée : majeure × 10 + mineure, donc 25 pour la 2.5.
  [[nodiscard]] std::int32_t version() const noexcept { return version_; }

  [[nodiscard]] const std::vector<ActAction>& actions() const noexcept { return actions_; }

  // Noms de fichiers son déclarés, indexés par ActFrame::sound_index.
  //
  // ⚠ La table porte aussi des marqueurs d'animation : une entrée qui ne finit
  // pas par « .wav » n'est PAS un son.
  [[nodiscard]] const std::vector<std::string>& sound_files() const noexcept {
    return sound_files_;
  }

  // Vrai si le fichier a dû être relu en IGNORANT ses ancres.
  //
  // Quelques .act 2.3 et 2.4 déclarent zéro ancre alors que des octets d'ancre
  // suivent quand même ; la lecture déraille alors, et le seul remède connu est
  // de tout relire sans les ancres. Ces fichiers ressortent sans ancrage, et le
  // composeur doit s'en accommoder.
  //
  // ⚠ AUCUN fichier de l'installation de référence ne déclenche ce repli :
  // `roc-cli sprite scan` lit ses 130 716 .act sans jamais l'emprunter. Il est
  // conservé parce qu'il vient d'un correctif daté de l'implémentation de
  // référence, donc d'autres installations en portent ; il n'est éprouvé que
  // sur données synthétiques, et c'est à savoir avant de s'y fier.
  [[nodiscard]] bool anchors_dropped() const noexcept { return anchors_dropped_; }

 private:
  std::int32_t version_ = 0;
  bool anchors_dropped_ = false;
  std::vector<ActAction> actions_;
  std::vector<std::string> sound_files_;
};

}  // namespace ro::formats
