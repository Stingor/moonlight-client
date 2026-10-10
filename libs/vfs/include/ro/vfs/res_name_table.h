#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace ro::vfs {

class Vfs;

// LE CLONAGE DES CARTES : `data\resnametable.txt`.
//
// Une carte que le serveur annonce peut n'avoir AUCUN fichier à son nom.
// `pvp_n_1-1` en est une : ni `.rsw`, ni `.gnd`, ni `.gat` ne portent ce
// nom-là dans l'installation, et c'est cette table qui dit d'y lire
// `prt_maze02`. Sans elle, la carte ne se charge pas.
//
// Elle ne sert qu'à cela : ses 2 274 entrées sont 602 scènes, 599 reliefs,
// 599 grilles de marche et 571 minimaps — 578 cartes clonées, aucune autre
// espèce de ressource.
//
// 🔴 CHAQUE FICHIER SE TRADUIT SEUL, et les croire solidaires serait faux :
// neuf cartes ne sont pas clonées de bout en bout. `ordeal_1-3` prend son
// relief de `ordeal_a00` et sa scène de `ordeal_a03` ; `thief_1-1` ne clone que
// sa scène et garde ses deux autres fichiers. Traduire le NOM DE LA CARTE une
// fois pour les trois donnerait donc un relief qui ne va pas avec sa scène.
//
// L'ordre est celui du client : la
// casse s'abaisse, la table traduit, le préfixe de la famille (`data\` pour une
// carte) s'ajoute EN DERNIER. La clé est donc le nom nu, sans préfixe.
class ResNameTable {
 public:
  // Lit la table de l'installation.
  //
  // Absente ou illisible, la table est VIDE : un client dont l'installation ne
  // la porte pas lit ses cartes en direct, ce qui est le cas de toutes celles
  // qui ne sont pas des clones.
  [[nodiscard]] static ResNameTable Load(Vfs& vfs);

  // Bâtit la table depuis le contenu du fichier.
  [[nodiscard]] static ResNameTable Parse(std::string_view contents);

  // Le nom de rechange, ou le nom demandé — abaissé de casse — s'il n'est pas
  // cloné. C'est ce que rend le client, qui traduit sur son tampon déjà abaissé.
  //
  // ⚠ SEUL LE NOM DEMANDÉ S'ABAISSE. La clé se range telle que le fichier
  // l'écrit et se compare octet pour octet : une clé portant une majuscule ne
  // se trouve donc jamais. C'est le comportement reproduit, pas un oubli —
  // abaisser la clé clonerait une carte que le jeu d'origine laisse telle quelle.
  [[nodiscard]] std::string CloneOf(std::string_view name) const;

  // Le chemin interne d'un fichier de carte, le clonage appliqué.
  [[nodiscard]] std::string MapFilePath(std::string_view file_name) const;

  [[nodiscard]] std::size_t size() const noexcept { return clones_.size(); }

 private:
  // `std::less<>` pour chercher sur une vue sans bâtir de chaîne.
  std::map<std::string, std::string, std::less<>> clones_;
};

}  // namespace ro::vfs
