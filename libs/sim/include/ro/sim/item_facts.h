#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ro::sim {

// CE QUE LE SERVEUR SEUL SAIT D'UN OBJET : d'où il tombe, et son script.
//
// Aucune table du client ne le dit. Les sources se CALCULENT sur la base des
// monstres du serveur — et leurs taux bougent avec les événements, le VIP et
// l'équipement du joueur ; le script est celui de la base d'objets du serveur,
// que le client n'a jamais eue. Tout arrive sur demande.

// LES DEUX PORTÉES d'une demande de sources : les monstres ordinaires, puis les
// boss et MVP — leur butin ET leurs récompenses de MVP.
enum class ItemSourceScope : std::uint8_t { kOrdinary = 0, kBoss = 1 };

// Un monstre qui laisse tomber l'objet.
struct ItemDropSource {
  std::uint32_t monster_id = 0;
  // Le taux, en centièmes de pour-cent : 10 000 = 100 %.
  std::uint32_t rate_hundredths = 0;
  // Zéro ordinaire, un mini-boss, deux MVP.
  std::uint8_t boss_rank = 0;
  // Une récompense de MVP, et non un butin ordinaire.
  bool mvp_reward = false;
  // En UTF-8.
  std::string name;
};

struct ItemDropSources {
  std::vector<ItemDropSource> sources;
  // Le serveur a coupé la liste : d'autres sources existent.
  bool truncated = false;
  // Les coffres au trésor exclus du compte.
  std::uint8_t treasure_excluded = 0;
};

// Un objet d'un ensemble — un « combo ».
struct ItemComboMember {
  std::uint32_t item_id = 0;
  // En UTF-8.
  std::string name;
};

struct ItemCombo {
  std::vector<ItemComboMember> members;
  std::string script;
};

struct ItemScripts {
  // Zéro : l'objet est connu du serveur. Sinon, rien d'autre n'a de sens.
  std::uint8_t status = 0;
  std::string main;
  std::string equip;
  std::string unequip;
  std::vector<ItemCombo> combos;
};

class ItemFacts {
 public:
  void TellSources(std::uint32_t item_id, ItemSourceScope scope, ItemDropSources told);
  [[nodiscard]] const ItemDropSources* SourcesOf(std::uint32_t item_id,
                                                 ItemSourceScope scope) const;
  // Oublie les sources de cet objet : la prochaine demande repartira du serveur.
  void ForgetSources(std::uint32_t item_id, ItemSourceScope scope);

  void TellScripts(std::uint32_t item_id, ItemScripts told);
  [[nodiscard]] const ItemScripts* ScriptsOf(std::uint32_t item_id) const;

  // LE NOM D'UN PERSONNAGE, sous son identifiant : celui qui a forgé ou signé
  // un objet. Vide quand le serveur ne le connaît pas — l'objet reste alors
  // « sans nom », comme au client.
  void TellCreator(std::uint32_t character_id, std::string name);
  // Nul tant que le serveur n'a pas répondu.
  [[nodiscard]] const std::string* CreatorOf(std::uint32_t character_id) const;

  // LA LISTE DE RAMASSAGE AUTOMATIQUE du personnage, telle que le serveur l'a
  // dite en dernier — puis corrigée d'avance par nos propres gestes.
  void TellAutoloot(std::vector<std::uint32_t> item_ids);
  void NoteAutoloot(std::uint32_t item_id, bool listed);
  [[nodiscard]] bool Autoloots(std::uint32_t item_id) const;
  // La liste dans l'ordre du serveur, nos ajouts d'avance à la suite.
  [[nodiscard]] const std::vector<std::uint32_t>& autoloot() const noexcept { return autoloot_; }
  // ⚠ FAUX TANT QUE LE SERVEUR N'A RIEN DIT : sans lui, la liste n'existe pas —
  // et un bouton qui l'éditerait parlerait dans le vide.
  [[nodiscard]] bool autoloot_told() const noexcept { return autoloot_told_; }

  void Clear() noexcept {
    sources_.clear();
    scripts_.clear();
    creators_.clear();
    autoloot_.clear();
    autoloot_told_ = false;
  }

 private:
  std::map<std::pair<std::uint32_t, ItemSourceScope>, ItemDropSources> sources_;
  std::map<std::uint32_t, ItemScripts> scripts_;
  std::map<std::uint32_t, std::string> creators_;
  std::vector<std::uint32_t> autoloot_;
  bool autoloot_told_ = false;
};

}  // namespace ro::sim
