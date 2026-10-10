#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "ro/net/connection.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/book_window.h"
#include "ro/vfs/books.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/vfs.h"

#include "items/book_bookmark_file.h"

namespace moonclient {

// LES LIVRES : « Lire » ouvre la fenêtre, « Lecture auto » fait réciter le livre
// au personnage — comme le client.
//
// 🔴 LES DEUX BOUTONS NE PARAISSENT QUE POUR UN LIVRE QU'ON POSSÈDE : l'item est
// dans la table des livres ET dans l'inventory. C'est la double condition du
// client ; un livre vu dans une boutique ne se lit pas.
//
// ⚠ LA LECTURE À VOIX HAUTE PARLE POUR DE VRAI : toutes les quatre secondes,
// la ligne suivante part dans la conversation publique et s'affiche dans une
// bulle en haut de l'écran. Une ligne vide se saute ; la fin du livre arrête
// tout. C'est le comportement du client.
class BookReader {
 public:
  void See(ro::vfs::Vfs* vfs, const ro::vfs::BookList* list,
           const ro::vfs::ItemTable* items) noexcept;

  // Les signets de ce personnage, relus de `savedata` ; chaque fermeture les
  // y réécrit. Sans cet appel, ils ne vivent que la session.
  void RememberFor(std::filesystem::path client_root, std::uint32_t character_id);

  [[nodiscard]] bool Readable(const ro::sim::World& world, std::uint32_t item_id) const;

  // « Lire » : charge le livre et ouvre sa fenêtre, à la première page ou au
  // signet selon le réglage.
  void Open(std::uint32_t item_id, ro::ui::BookWindowState& window, bool first_page);
  // « Lecture auto » : la récitation commence, et la fenêtre se ferme — le
  // client la cache pendant qu'on lit.
  void StartRecital(std::uint32_t item_id, ro::ui::BookWindowState& window, bool first_page,
                    std::uint32_t now_ms);
  // La fenêtre refermée : la page devient le signet du livre, et s'écrit sur
  // le disque si elle a changé.
  void NoteClosed(std::uint32_t item_id, int page);
  // LA PARTIE S'ARRÊTE — on quitte, on se déconnecte, on change de
  // personnage : un livre encore ouvert retient sa page comme à la fermeture.
  // Fenêtre fermée : rien ne s'écrit.
  void NoteLeaving(const ro::ui::BookWindowState& window);

  // À chaque image : la récitation avance.
  void Tick(ro::net::Connection& link, const ro::sim::World& world, std::uint32_t now_ms);

  [[nodiscard]] const ro::ui::BookView& view() const noexcept { return view_; }
  // La ligne récitée, pour la bulle ; vide hors récitation.
  [[nodiscard]] const std::string& recital_line() const noexcept { return recital_line_; }

 private:
  [[nodiscard]] bool Load(std::uint32_t item_id);
  [[nodiscard]] int StartPage(std::uint32_t item_id, bool first_page) const;

  ro::vfs::Vfs* vfs_ = nullptr;
  const ro::vfs::BookList* list_ = nullptr;
  const ro::vfs::ItemTable* items_ = nullptr;

  std::uint32_t loaded_ = 0;
  ro::vfs::Book book_;
  std::string title_;
  ro::ui::BookView view_;
  // LE SIGNET DE CHAQUE LIVRE, et le personnage sous lequel il se range ;
  // un identifiant nul : rien ne s'écrit.
  BookBookmarks bookmarks_;
  std::filesystem::path client_root_;
  std::uint32_t character_id_ = 0;

  // La récitation : les lignes à dire, la suivante, et l'heure de la dernière.
  bool reciting_ = false;
  std::vector<std::string> recital_;
  std::size_t next_ = 0;
  std::uint32_t said_at_ms_ = 0;
  std::string recital_line_;
};

}  // namespace moonclient
