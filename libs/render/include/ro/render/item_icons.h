#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "ro/render/icon_texture.h"
#include "ro/vfs/card_illustrations.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// LES ICÔNES D'OBJET, LUES À LA DEMANDE.
//
// ⚠ PAS TOUTES AU DÉMARRAGE. L'installation en porte douze mille sept cent
// dix-sept, et un sac plein n'en montre qu'une centaine. Les lire toutes
// coûterait plusieurs secondes de démarrage pour des bitmaps que personne ne
// regardera.
//
// ⚠ ET UNE ABSENCE SE RETIENT. Un objet dont l'installation n'a pas l'icône
// laisse un point d'interrogation ; sans la retenir, on redemanderait le même
// fichier manquant soixante fois par seconde.
//
// ⚠ LE FICHIER SE NOMME PAR LA RESSOURCE, JAMAIS PAR L'IDENTIFIANT. Le nom vient
// de la table des objets, il est en coréen, et il n'a le plus souvent aucun
// rapport avec le numéro : « 501.bmp » n'existe que parce que la potion rouge a
// « 501 » pour nom de ressource, ce qui est une coïncidence et non une règle.
class ItemIcons {
 public:
  // Tous sont passés par référence et doivent vivre plus longtemps : les icônes
  // se lisent au fil du jeu, pas une fois pour toutes.
  //
  // ⚠ LA TABLE DES ILLUSTRATIONS EST FACULTATIVE : sans elle, une carte montre
  // simplement son image de collection — le dos de carte générique —, ce que
  // faisait ce client avant qu'elle existe.
  ItemIcons(vfs::Vfs& vfs, const vfs::ItemTable& items,
            const vfs::CardIllustrations* cards = nullptr);

  // L'icône d'un objet. Rend une icône inutilisable quand l'installation ne l'a
  // pas, ou quand la table ignore l'objet — ce qui n'est pas une panne : la case
  // se dessine alors sans image, et la quantité reste lisible.
  //
  // ⚠ RIEN NE DISTINGUE L'OBJET NON IDENTIFIÉ, et deux mesures le disent. Le
  // client sait donner une seconde ressource à cet état ; la table de cette
  // installation n'en déclare AUCUNE — pas un `unRes` sur 27 208 entrées — et le
  // serveur, lui, ne fabrique jamais d'objet non identifié hors de commandes
  // d'administration. Une clé de cache à deux champs ne séparerait donc rien.
  [[nodiscard]] const IconTexture& Get(std::uint32_t item_id);

  // L'ILLUSTRATION DE COLLECTION, la grande image de la description.
  //
  // ⚠ UN SECOND CACHE, et non une clé à deux champs : les deux images ne se
  // demandent pas au même endroit ni au même rythme — une case de sac tire son
  // icône à chaque image, une description son illustration une fois.
  //
  // Rend une texture inutilisable quand l'installation ne l'a pas : tout objet
  // n'en a pas, et l'appelant se rabat alors sur l'icône.
  [[nodiscard]] const IconTexture& GetCollection(std::uint32_t item_id);

  // L'ILLUSTRATION D'UNE CARTE, le dessin en pied.
  //
  // ⚠ UN TROISIÈME CACHE, pour la même raison que le second, et parce que la
  // clé N'EST PAS la même : celle-ci passe par la table des illustrations, que
  // la ressource de l'objet ne donne pas.
  //
  // Rend une texture inutilisable pour tout ce qui n'est pas une carte — c'est
  // le test d'appartenance lui-même, et l'appelant se rabat alors sur
  // l'illustration de collection.
  [[nodiscard]] const IconTexture& GetCardIllustration(std::uint32_t item_id);

  // L'IMAGE D'UNE DESCRIPTION, et si c'est le dessin d'une carte.
  //
  // 🔴 TROIS IMAGES POUR UN OBJET, ET L'ORDRE EST CELUI DU CLIENT : le dessin
  // de la carte s'il en est une, sinon la grande image de collection — onze
  // mille objets l'ont —, sinon l'icône de 24 pixels. Sauter la première
  // montrerait le dos de carte générique pour les mille cinq cents cartes,
  // puisqu'elles partagent toutes cette ressource ; sauter la seconde
  // donnerait à une description la vignette d'une case de sac.
  //
  // `texture` est nulle quand aucune des trois n'existe.
  struct DescriptionPicture {
    const IconTexture* texture = nullptr;
    bool card = false;
  };
  [[nodiscard]] DescriptionPicture GetDescriptionPicture(std::uint32_t item_id);

  [[nodiscard]] std::size_t loaded_count() const noexcept { return loaded_.size(); }

  // OUBLIE TOUT : le skin a changé. Voir `GameImages::Clear`.
  void Clear();
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }

 private:
  vfs::Vfs* vfs_ = nullptr;
  const vfs::ItemTable* items_ = nullptr;
  const vfs::CardIllustrations* cards_ = nullptr;
  std::unordered_map<std::uint32_t, IconTexture> loaded_;
  std::unordered_map<std::uint32_t, IconTexture> collections_;
  std::unordered_map<std::uint32_t, IconTexture> illustrations_;
  std::uint64_t generation_ = 0;
};

// LE CHEMIN D'ARCHIVE de l'illustration de collection d'un objet, en CP949.
// Vide aux mêmes conditions que celui de l'icône.
[[nodiscard]] std::string ItemCollectionPath(const vfs::ItemTable& items, std::uint32_t item_id);

// LE CHEMIN D'ARCHIVE de l'illustration d'une carte, en CP949. Vide pour tout ce
// que la table des illustrations ignore — c'est-à-dire pour tout ce qui n'est
// pas une carte.
[[nodiscard]] std::string CardIllustrationPath(const vfs::CardIllustrations& cards,
                                               std::uint32_t item_id);

// LE CHEMIN D'ARCHIVE d'une icône d'objet, en CP949. Vide quand la table ignore
// l'objet ou ne lui donne pas de ressource.
//
// ⚠ EN OCTETS BRUTS, et c'est ce qui évite un aller-retour. La ressource sort de
// la table en CP949 — c'est un nom de fichier d'archive. La repasser en UTF-8
// pour que le VFS la retranscrive serait deux conversions pour rien, et une
// occasion de perdre un octet.
//
// ⚠ SÉPARÉ DU TÉLÉVERSEMENT, parce que celui-ci demande un contexte OpenGL et
// que le chemin, lui, se vérifie hors jeu. C'est ce qui permet à `roc-cli` de
// dire ce que l'installation possède sans ouvrir de fenêtre.
[[nodiscard]] std::string ItemIconPath(const vfs::ItemTable& items, std::uint32_t item_id);

}  // namespace ro::render
