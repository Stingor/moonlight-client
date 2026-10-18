#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "ro/ui/map_editor_hours.h"

namespace ro::ui {

// LES SONS D'AMBIANCE DANS LE PANNEAU DE L'ÉDITEUR DE CARTE : les sources de
// son du RSW, que le document retouche, et les sons qu'il ajoute. Voir
// docs/adr/0118 et map_editor_sounds_panel.cpp.

// CE QUI DÉSIGNE UN SON : une source du RSW par son rang, ou un son ajouté par
// son rang dans le document.
struct MapEditorSoundKey {
  bool added = false;
  std::size_t index = 0;

  friend bool operator==(const MapEditorSoundKey&, const MapEditorSoundKey&) = default;
};

// UN SON, tel que le panneau le montre et le rend. Les grandeurs sont celles du
// RSW : place dans son repère, emprise et portée en unités, volume de 0 à 1,
// cycle en secondes.
struct MapEditorSound {
  MapEditorSoundKey key;
  // Son fichier, relatif à data\wav\, en UTF-8.
  std::string file;
  // La place d'origine — celle du RSW, ou pour un son ajouté sa place — et
  // celle que l'édition lui donne.
  std::array<float, 3> origin{};
  std::array<float, 3> position{};
  float volume = 1.0F;
  // L'emprise : seul un son ajouté la règle.
  std::int32_t width = 0;
  std::int32_t height = 0;
  float range = 0.0F;
  float cycle_seconds = 0.0F;
  // Le document retouche cette source du RSW.
  bool edited = false;
  bool hidden = false;
  bool night_only = false;
  MapEditorHours hours;
  std::string why;
};

namespace map_editor {
// Régler le son désigné par `key`, ou plus aucun.
struct SelectSound {
  std::optional<MapEditorSoundKey> key;
};
// Le son que désigne `sound.key` devient `sound`. `released` : le geste est
// fini, et il devient une entrée de l'historique.
struct SetSound {
  MapEditorSound sound;
  bool released = true;
};
// Déplacer le son `key` en `native`, dans le repère du RSW — à la flèche.
struct MoveSound {
  MapEditorSoundKey key;
  std::array<float, 3> native{};
  bool released = true;
};
// Rendre à une source du RSW ce que le fichier en dit ; retirer un son ajouté.
struct RevertSound {
  MapEditorSoundKey key;
};
// Ajouter le son `file`, relatif à data\wav\, là où l'appelant pose ce qu'on
// ajoute.
struct AddSound {
  std::string file;
};
// Écouter une fois le son `file`, relatif à data\wav\, au volume `volume`, de 0
// à 1 : rien ne change au document.
struct ListenSound {
  std::string file;
  float volume = 1.0F;
};
}  // namespace map_editor

// CE QUE LA SECTION « Sons » GARDE d'une image à la suivante.
struct MapEditorSoundPanelState {
  static constexpr std::size_t kFilterCapacity = 64;
  static constexpr std::size_t kWhyCapacity = 256;

  // Le filtre de la liste des fichiers, et le fichier qu'on y a choisi.
  std::array<char, kFilterCapacity> filter{};
  std::string choice;
  // La raison du son réglé, et le son dont c'est la saisie.
  std::array<char, kWhyCapacity> why{};
  std::optional<MapEditorSoundKey> why_of;
  // On tape dans ce champ : le document ne le recharge pas sous les doigts.
  bool why_typing = false;
};

}  // namespace ro::ui
