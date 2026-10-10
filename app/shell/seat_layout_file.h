#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/seat_layout.h"

namespace moonclient {

// LA MISE EN PAGE DE LA TABLÉE, sur le disque : l'état courant et les mises en
// page que le joueur a enregistrées sous un nom.
//
// 🔴 LE FORMAT EST CELUI DE `bourgeon_charselect_layout.yaml`, clé pour clé :
// une salle déjà composée se retrouve ici sans rien refaire — on relit ce
// fichier tant que le nôtre n'existe pas. Et le décor s'y nomme relativement à
// `data\texture\`.
//
// ⚠ CHEZ LE JOUEUR, avec les autres fichiers du client : c'est une commodité
// de SA machine, pas un état de jeu.

inline constexpr std::string_view kSeatLayoutFileName = "charselect_layout.yml";
// L'autre fichier relu, à la racine de l'installation.
inline constexpr std::string_view kLegacySeatLayoutFileName = "bourgeon_charselect_layout.yaml";

// OÙ LE JOUEUR DÉPOSE SES DÉCORS, relativement à la racine du client.
//
// 🔴 `data\texture\lobby\`, ET NON `data\lobby\` : un décor se nomme relativement
// à `data\texture\`, donc un fichier posé ailleurs serait listé mais jamais
// trouvé. L'emplacement qu'on croit « naturel » se regarde quand même — pour
// DIRE au joueur que ses images sont au mauvais endroit, plutôt que de le
// laisser devant une liste vide.
inline constexpr std::string_view kUserBackgroundFolder = "data\\texture\\lobby\\";
inline constexpr std::string_view kMisplacedBackgroundFolder = "data\\lobby\\";
// Le même dossier, vu depuis `kBackgroundRoot` : le préfixe d'un décor du
// joueur dans la mise en page.
inline constexpr std::string_view kUserBackgroundPrefix = "lobby\\";
inline constexpr std::string_view kBackgroundSuffix = ".bmp";

// Un nom de mise en page : non vide après retrait des espaces de bordure, et
// quarante octets au plus.
inline constexpr std::size_t kPresetNameMax = 40;
// La galerie s'arrête là : au-delà, ce n'est plus un choix mais un inventaire.
inline constexpr std::size_t kBackgroundsMax = 200;

struct SeatLayoutPreset {
  std::string name;
  ro::ui::SeatLayout layout;
};

struct SeatLayoutBook {
  ro::ui::SeatLayout current;
  std::vector<SeatLayoutPreset> presets;
};

// ── LE TEXTE ─────────────────────────────────────────────────────────────────

// LIT UNE MISE EN PAGE.
//
// 🔴 UN CHAMP ILLISIBLE OU HORS BORNES GARDE LA VALEUR D'USINE, siège par
// siège : un fichier à moitié écrit — disque plein, arrêt brutal — dégrade une
// place au lieu de rendre l'écran inutilisable. Un texte qui n'est pas du YAML
// rend la mise en page d'usine entière.
[[nodiscard]] SeatLayoutBook ParseSeatLayout(std::string_view yaml_text);

// L'ÉCRIT. ⚠ LES MÊMES CLÉS à la racine et dans chaque mise en page nommée :
// c'est ce qui permet de les lire avec la même fonction.
[[nodiscard]] std::string EmitSeatLayout(const SeatLayoutBook& book);

// ── LE DISQUE ────────────────────────────────────────────────────────────────

[[nodiscard]] std::filesystem::path SeatLayoutPath(const std::filesystem::path& client_root);

// Les deux dossiers de décors sur le disque — le bon, et celui qui ne se lit
// pas —, bâtis par composants : l'antislash n'est un séparateur que sous
// Windows.
[[nodiscard]] std::filesystem::path UserBackgroundDirectory(
    const std::filesystem::path& client_root);
[[nodiscard]] std::filesystem::path MisplacedBackgroundDirectory(
    const std::filesystem::path& client_root);

// Le nôtre, sinon `bourgeon_charselect_layout.yaml`, sinon l'usine. ⚠ UN
// FICHIER ABSENT N'EST PAS UNE PANNE : c'est un premier lancement.
[[nodiscard]] SeatLayoutBook LoadSeatLayout(const std::filesystem::path& client_root);

// Rend faux si le disque refuse : la mise en page reste alors en mémoire, et
// rien de plus.
bool SaveSeatLayout(const std::filesystem::path& client_root, const SeatLayoutBook& book);

// ── LES MISES EN PAGE ENREGISTRÉES ───────────────────────────────────────────
//
// ⚠ LA MISE EN PAGE D'ORIGINE N'EN FAIT PAS PARTIE : elle est dans le code, et
// se restaure par « Tout restaurer ». Elle ne peut donc être ni écrasée ni
// supprimée.

// RANGE `current` sous ce nom — un nom déjà pris est MIS À JOUR, pas doublé.
// Rend l'index de l'entrée, ou -1 si le nom est vide, trop long, ou si la
// limite est atteinte.
[[nodiscard]] int StorePreset(std::vector<SeatLayoutPreset>& presets, std::string_view name,
                              const ro::ui::SeatLayout& current);

// RECOPIE la mise en page enregistrée : sièges, blocs et décor.
//
// ⚠ PAS LES PLACES LIBRES : les masquer est un goût du joueur, pas une
// composition.
bool ApplyPreset(const std::vector<SeatLayoutPreset>& presets, int index, ro::ui::SeatLayout& into);

bool DeletePreset(std::vector<SeatLayoutPreset>& presets, int index);

// ── LA GALERIE ───────────────────────────────────────────────────────────────

struct BackgroundGallery {
  std::vector<ro::ui::LayoutBackground> entries;
  int packed = 0;
};

// LA GALERIE, à partir de ce que le disque et les archives proposent.
//
// `disk_files` : les noms de fichier seuls, trouvés dans `kUserBackgroundFolder`.
// `packed_files` : les chemins entiers sous ce même dossier, tels que le VFS
// les liste — il y mêle le disque, et le doublon s'écarte.
//
// ⚠ LE DÉCOR D'USINE EN TÊTE, puis tous les autres triés par libellé : le
// joueur n'a pas à savoir d'où vient chaque décor pour le choisir. Un même
// fichier sur le disque ET dans une archive ne se propose qu'UNE fois, et
// c'est le disque qui gagne — c'est lui que le VFS lira.
//
// ⚠ LES NOMS NON-ASCII SONT ÉCARTÉS : la liste les refuserait de toute façon au
// prochain lancement (voir `ro::ui::ValidBackgroundPath`), et un décor qu'on ne
// retrouve pas est pire qu'un décor qu'on ne propose pas.
[[nodiscard]] BackgroundGallery ComposeGallery(std::span<const std::string> disk_files,
                                               std::span<const std::string> packed_files);

}  // namespace moonclient
