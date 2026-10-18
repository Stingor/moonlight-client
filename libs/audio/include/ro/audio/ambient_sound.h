#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ro::audio {

// ── LES SOURCES DE SON D'UNE CARTE ─────────────────────────────────────────
//
// L'eau d'une fontaine, le vent, les animaux, la foule : chaque carte pose des
// sources, et chacune REJOUE son bruit à intervalle fixe tant que le joueur est
// assez près. Il n'y a pas de boucle : un bruit qui dure trois secondes et se
// rejoue toutes les quatre laisse une seconde de silence, comme dans le client.
//
// Une source n'est pas un point mais un RECTANGLE centré sur sa place. Le bruit
// part du point de ce rectangle que le client juge le plus proche du joueur :
// longer le bord d'un lac garde l'eau à la même distance.
//
// ⚠ TOUT SE CALCULE DANS LE REPÈRE DU CLIENT, abscisse et profondeur au sol. Le
// calcul du point n'est pas symétrique — il borne la pente par en bas avant de
// diviser, si bien qu'un joueur à l'ouest d'une source ne l'entend pas partir
// du point miroir de celui à l'est. Le passer dans notre repère, dont la
// profondeur est retournée, changerait le côté d'où le bruit vient.

// Au-delà de cet écart entre le joueur et l'emprise entière de la source, portée
// comprise, on ne calcule rien.
inline constexpr float kAmbientSoundWakeMargin = 130.0F;
// Et le point le plus proche doit être à moins de la portée plus ceci pour que
// le bruit parte.
inline constexpr float kAmbientSoundPlayMargin = 75.0F;
// La proximité — le rayon de plein volume — est cette fraction de la portée.
inline constexpr float kAmbientSoundNearDivisor = 6.0F;

// Une source, telle que la carte la déclare, dans le repère du client.
struct AmbientSoundSource {
  // Abscisse et profondeur de sa place.
  std::array<float, 2> native_ground{};
  // ⚠ ENTIERS, comme le client les range : une largeur nulle ne sonne jamais.
  std::int32_t width = 0;
  std::int32_t height = 0;
  float range = 0.0F;
  float cycle_seconds = 0.0F;
  float volume = 0.0F;
};

// Un bruit à jouer : d'où, jusqu'où il porte, et à quel volume.
struct AmbientSoundCue {
  // Abscisse et profondeur du point d'où il part, dans le repère du client.
  std::array<float, 2> native_ground{};
  float hearing_range = 0.0F;
  float near_distance = 0.0F;
  float volume = 0.0F;
};

class AmbientSoundEmitter {
 public:
  // 🔴 LA SOURCE NAÎT COMME SI ELLE VENAIT DE SONNER IL Y A UN CYCLE : son
  // premier bruit part dès que le joueur est assez près, sans attendre une
  // période.
  AmbientSoundEmitter(const AmbientSoundSource& source, std::uint32_t now_ms);

  // Une image : le bruit à jouer maintenant, ou rien. `native_listener` est la
  // place du joueur, abscisse et profondeur dans le repère du client.
  [[nodiscard]] std::optional<AmbientSoundCue> Tick(std::array<float, 2> native_listener,
                                                    std::uint32_t now_ms);

  [[nodiscard]] std::uint32_t cycle_ms() const noexcept { return cycle_ms_; }
  [[nodiscard]] const AmbientSoundSource& source() const noexcept { return source_; }
  // L'instant du dernier bruit — ou celui, fictif, d'un cycle avant la
  // naissance.
  [[nodiscard]] std::uint32_t last_played_ms() const noexcept { return last_played_ms_; }
  // REPREND LA CADENCE d'une source qu'elle remplace : son prochain bruit part
  // un cycle après le dernier bruit de `earlier`, et non tout de suite. Une
  // source qu'on retouche ne se rejoue pas à chaque retouche.
  void KeepPaceOf(const AmbientSoundEmitter& earlier) noexcept {
    last_played_ms_ = earlier.last_played_ms_;
  }

 private:
  AmbientSoundSource source_;
  // La diagonale ENTIÈRE du rectangle — non sa moitié —, que le client ajoute
  // à la portée pour savoir s'il se réveille.
  float diagonal_ = 0.0F;
  std::uint32_t cycle_ms_ = 0;
  std::uint32_t last_played_ms_ = 0;
};

// Le point du rectangle d'où le bruit part, pour un joueur en `native_listener`,
// dans le repère du client. Exposé pour être essayé : c'est lui qui porte les
// deux bizarreries du client — la division ENTIÈRE de la hauteur par la largeur,
// et la pente bornée par en bas.
[[nodiscard]] std::array<float, 2> AmbientSoundPoint(const AmbientSoundSource& source,
                                                     std::array<float, 2> native_listener);

// ── CE QUE L'ÉTABLI DU STAFF MONTRE ─────────────────────────────────────────
//
// Des contours dans le repère du client, fermés : le dernier point rejoint le
// premier.

// L'EMPRISE : le rectangle largeur × hauteur centré sur la source, ses quatre
// coins. Vide pour une source sans largeur, qui ne sonne jamais.
[[nodiscard]] std::vector<std::array<float, 2>> AmbientSoundFootprint(
    const AmbientSoundSource& source);

// LE CERCLE D'OÙ L'ON ENTEND LE BRUIT : la portée autour du point d'où il part.
//
// ⚠ CE N'EST PAS L'EMPRISE ÉLARGIE DE LA PORTÉE. Le client mesure la distance
// au point qu'il vient de calculer, et ce point peut quitter le rectangle — il
// file jusqu'à la verticale du joueur quand le rapport entier des côtés vaut
// zéro. Un rectangle élargi montrerait une zone que le client n'emploie pas.
[[nodiscard]] std::vector<std::array<float, 2>> AmbientSoundHearingCircle(
    std::array<float, 2> center, float range, std::size_t segments);

}  // namespace ro::audio
