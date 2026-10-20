#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ro/render/icon_texture.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// UNE IMAGE DE L'INSTALLATION, DÉSIGNÉE PAR SON CHEMIN.
//
// Les icônes, les illustrations et les pièces d'habillage ont chacune leur
// dossier et leur clé ; celle-ci n'en a pas. Elle sert à ce qu'un SCRIPT
// désigne : une carte du donjon, un portrait, un bandeau d'évènement, nommés
// dans le texte d'un PNJ par `<IMG>`. Le client ne peut pas les connaître
// d'avance — c'est le serveur qui les choisit.
//
// 🔴 LE CHEMIN VIENT DU SERVEUR, ET C'EST TOUTE LA DIFFÉRENCE. Partout ailleurs
// il est composé ici, à partir d'une table de l'installation. Ici, il arrive du
// dehors : voir `SafeGamePath`, et ce qu'elle refuse.
class GameImages {
 public:
  explicit GameImages(vfs::Vfs& vfs) : vfs_(&vfs) {}

  // L'image de ce chemin, TEL QUE LE SERVEUR L'A ÉCRIT — donc en CP949.
  //
  // Rend une texture inutilisable quand l'installation ne l'a pas, ou quand le
  // chemin est refusé — ce qui n'est pas une panne : l'appelant le DIT, plutôt
  // que de laisser un trou.
  //
  // ⚠ L'ABSENCE SE RETIENT. Sans cela, une image manquante se redemanderait au
  // VFS à chaque image, et un script fautif coûterait une recherche d'archive
  // soixante fois par seconde.
  [[nodiscard]] const IconTexture& Get(std::string_view cp949_path);

  // L'image de ce chemin, ÉCRIT PAR NOUS — donc en UTF-8.
  //
  // 🔴 DEUX ENCODAGES, ET LES CONFONDRE FAIT UNE ABSENCE MUETTE. Les archives
  // du client nomment leurs entrées en CP949 ; un chemin qui vient d'un AUTRE
  // FICHIER du jeu est déjà dans cet encodage et se passe tel quel. Un chemin
  // écrit dans NOS sources est de l'UTF-8 — « 유저인터페이스 » y pèse trois
  // octets par idéogramme, et deux en CP949 — et doit être transcodé.
  //
  // 🔴 TANT QUE LE CHEMIN EST EN ASCII LES DEUX SE VALENT, et c'est ce qui rend
  // la faute difficile à voir : `data\texture\lobby_hall.bmp` et
  // `data\texture\discord.bmp` passent par les deux portes, l'illustration de
  // l'écran de connexion par une seule. Payé à l'écran — un fond noir, et pas
  // une ligne au journal.
  [[nodiscard]] const IconTexture& GetOurs(std::string_view utf8_path);

  // LA MÊME, POUR UNE IMAGE FACULTATIVE : celle qui passe avant une autre quand
  // elle est livrée. ⚠ SON ABSENCE NE SE DIT PAS AU JOURNAL : c'est le cas
  // ordinaire, et le dire pour chaque icône noierait les vraies absences.
  [[nodiscard]] const IconTexture& GetOursIfShipped(std::string_view utf8_path);

  // UN FOND PLEIN CADRE, écrit par nous : toujours OPAQUE.
  //
  // 🔴 SANS LA CLÉ MAGENTA : un décor que le joueur dépose n'est pas de l'art
  // d'interface, et un pixel magenta y percerait un trou sur la ville qui passe
  // derrière. L'alpha d'un décor est donc forcé à plein.
  [[nodiscard]] const IconTexture& GetOpaque(std::string_view utf8_path);

  // L'IMAGE ÉCRITE PAR NOUS, RETOURNÉE POUR UN PAPIER SOMBRE : l'art d'une
  // fenêtre peint sur du blanc, dont le blanc tombe et dont les traits
  // s'éclaircissent. Voir `formats::RemapForDarkPaper`.
  [[nodiscard]] const IconTexture& GetOursForDarkPaper(std::string_view utf8_path);

  // L'IMAGE ÉCRITE PAR NOUS, DÉTOURÉE DE SON PAPIER : un objet peint sur du
  // blanc — un trophée, un tampon —, posable sur un papier sombre sans perdre
  // son modelé. Voir `formats::MattePaperBackground`.
  [[nodiscard]] const IconTexture& GetOursMatted(std::string_view utf8_path);

  // L'IMAGE ÉCRITE PAR NOUS, RÉDUITE À SA FORME : une marque d'une seule encre,
  // en blanc, que l'appelant teinte de la couleur qu'il veut. Voir
  // `formats::ReduceToInkShape`.
  [[nodiscard]] const IconTexture& GetOursInkShape(std::string_view utf8_path);

  // OUBLIE CE CHEMIN, texture et absence comprises, sous ses deux formes.
  //
  // ⚠ POUR CE QU'ON PARCOURT : essayer vingt décors plein écran en garderait
  // vingt en mémoire. Et pour ce qu'on RETENTE — une absence retenue ne se
  // redemande jamais, même quand le joueur vient de déposer le fichier.
  void Forget(std::string_view utf8_path);

  // OUBLIE TOUT : le skin a changé. ⚠ L'OBJET RESTE EN PLACE — d'autres le
  // désignent. Ce qu'ils ont copié d'une image se périme : ils comparent
  // `generation` à celle de leur copie.
  void Clear();
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }

 private:
  // CE QU'UNE IMAGE SUBIT ENTRE SON DÉCODAGE ET SA TEXTURE.
  enum class Treatment : std::uint8_t {
    // La clé magenta, pour un BMP.
    kKeyed,
    // Rien : un décor plein cadre reste opaque.
    kOpaque,
    // La clé, puis le retournement pour un papier sombre.
    kForDarkPaper,
    // La clé, puis le détourage.
    kMatted,
    // La clé, puis la réduction à la forme.
    kInkShape,
  };
  static constexpr std::size_t kTreatmentCount = 5;

  // Le corps commun : `ours` dit par quelle porte du VFS passer, `optional` si
  // une absence se tait.
  [[nodiscard]] const IconTexture& Fetch(std::string_view path, bool ours, Treatment treatment,
                                         bool optional = false);

  vfs::Vfs* vfs_ = nullptr;
  // ⚠ UN CACHE PAR TRAITEMENT et non une clé bricolée : le même fichier peut
  // servir de plusieurs façons, et leurs textures ne se valent pas.
  std::array<std::unordered_map<std::string, IconTexture>, kTreatmentCount> caches_;
  std::uint64_t generation_ = 0;
};

// LE CHEMIN QU'ON ACCEPTE DE LIRE, ou une chaîne vide.
//
// 🔴 CE QUI EST REFUSÉ, ET POURQUOI. Un chemin qui remonte (`..`), qui part d'une
// racine (`/`, `\`) ou qui nomme un lecteur (`C:`) ne désigne plus une ressource
// du jeu mais un fichier quelconque de la machine : le VFS lit aussi le dossier
// `data` du disque, et pas seulement les archives. Rien de tout cela n'apparaît
// dans un chemin légitime, écrit par le staff pour nommer une image du client.
//
// ⚠ CE N'EST PAS UNE DÉFENSE CONTRE LE SERVEUR, à qui l'on fait par ailleurs
// confiance pour tout le reste du jeu. C'est une borne sur ce qu'une CHAÎNE
// VENUE DU DEHORS peut atteindre, du même genre que le « http(s) en toutes
// lettres » exigé d'une adresse avant de l'ouvrir.
[[nodiscard]] std::string SafeGamePath(std::string_view path);

}  // namespace ro::render
