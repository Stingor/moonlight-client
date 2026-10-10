#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace moonclient {

// LES BONS DE TRAVAIL « ALBUMS D'OBJETS » DU STAFF.
//
// 🔴 CE FICHIER NE DIT PAS CE QU'UN ALBUM D'OBJETS CONTIENT. Il dit ce qu'un
// membre du staff veut y ajouter ou en retirer, et rien d'autre.
//
// La vérité est SERVEUR : sa base de groupes d'objets (les groupes de l'Old et
// du Mystical Card Album). Le `packageitem.lub` que le client lit — et d'où
// l'album tire ses macarons — en est ENGENDRÉ : c'est un fichier d'affichage.
// D'où la règle : on n'édite JAMAIS ce fichier-là. Une carte ajoutée à lui seul
// ferait promettre au client une carte que l'album ne donnera jamais, et
// l'édition serait écrasée sans un mot à la régénération suivante.
//
// Le geste du staff pose donc une INTENTION ici ; le script
// `apply_card_album_delta.py` du dépôt des données du client la porte dans la
// base du serveur, puis régénère le fichier d'affichage. Le sens de la
// synchronisation reste serveur → client.
//
// ⚠ `savedata\card_album_delta.yml` : son nom, sa clé racine et ses champs sont
// un contrat, le script le lit tel quel.
// Absent, c'est le cas nominal — personne n'a rien demandé.

enum class AlbumDeltaAction : std::uint8_t { kAdd, kRemove };

struct AlbumDeltaEntry {
  // L'OBJET album (l'Old ou le Mystical Card Album), pas le groupe du serveur :
  // le script fait la conversion, lui seul connaît les groupes.
  std::uint32_t album_id = 0;
  std::uint32_t card_id = 0;
  AlbumDeltaAction action = AlbumDeltaAction::kAdd;
  // Le poids de tirage demandé ; sans objet pour un retrait.
  int rate = 0;
};

class CardAlbumDelta {
 public:
  explicit CardAlbumDelta(std::filesystem::path client_root);

  [[nodiscard]] const std::vector<AlbumDeltaEntry>& entries();
  // L'intention posée sur ce couple, ou nul.
  [[nodiscard]] const AlbumDeltaEntry* Find(std::uint32_t album_id, std::uint32_t card_id);

  // Pose une intention (elle remplace celle du même couple) et réécrit le
  // fichier. 🔴 REND FAUX SI L'ÉCRITURE A ÉCHOUÉ, et l'état d'avant est alors
  // rétabli : une intention que le disque ne porte pas serait un macaron
  // d'attente pour un travail que le script ne verra jamais.
  [[nodiscard]] bool Set(const AlbumDeltaEntry& entry);
  // Retire l'intention. Retirer une intention absente est un succès.
  [[nodiscard]] bool Clear(std::uint32_t album_id, std::uint32_t card_id);

  // CHANGE À CHAQUE INTENTION POSÉE OU RETIRÉE : l'album refait alors ses
  // macarons.
  [[nodiscard]] std::uint32_t revision() const noexcept { return revision_; }

 private:
  void Load();
  [[nodiscard]] bool Flush() const;

  std::filesystem::path path_;
  std::vector<AlbumDeltaEntry> entries_;
  bool loaded_ = false;
  std::uint32_t revision_ = 0;
};

}  // namespace moonclient
