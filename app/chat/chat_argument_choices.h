#pragma once

#include <cstdint>
#include <deque>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/chat_command_hints.h"
#include "ro/vfs/job_names.h"

namespace moonclient {

// LES CHOIX QUE LA LISTE DES COMMANDES PROPOSE POUR UN ARGUMENT.
//
// Une commande DÉCLARE sa source de choix par son nom ; la liste l'ouvre dès
// qu'on a tapé le nom — ou un alias — suivi d'un blanc. Une commande sans
// source n'en propose aucun.
enum class ChatArgumentSource : std::uint8_t {
  kNone,
  // Les métiers que « @jobchange » accepte, par numéro.
  kServerJobChange,
};

// La source de choix de l'argument d'une commande « @ » du serveur, par son
// nom d'enregistrement (sans l'arobase).
[[nodiscard]] ChatArgumentSource ServerCommandArgumentSource(std::string_view command_name);

// LES CHOIX COMPOSÉS, source par source.
//
// ⚠ LES TEXTES SONT COPIÉS, et dans une FILE : les vues des choix pointent
// dedans, et doivent survivre à l'ajout du suivant.
class ChatArgumentChoices {
 public:
  // LES MÉTIERS : la table du serveur, chacun sous le nom que le client lui
  // donne (`job_names`) et, quand il diffère, sous celui du serveur — celui que
  // la commande reconnaît. Un métier que le client ne nomme pas prend le nom du
  // serveur seul.
  void Rebuild(const ro::vfs::JobNames& job_names);

  [[nodiscard]] std::span<const ro::ui::ChatArgumentChoice> Of(
      ChatArgumentSource source) const noexcept;

 private:
  std::deque<std::string> texts_;
  std::vector<ro::ui::ChatArgumentChoice> server_jobs_;
};

}  // namespace moonclient
