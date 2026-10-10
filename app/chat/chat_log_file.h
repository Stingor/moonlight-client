#pragma once

#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>

#include "chat/chat_history_file.h"
#include "chat/chat_tabs.h"

namespace moonclient {

// LA CONVERSATION SUR LE DISQUE, EN TEXTE, POUR ÊTRE LUE AILLEURS.
//
// ⚠ CE N'EST PAS L'HISTORIQUE, et les deux ne se remplacent pas. L'historique
// (`chat_history_file.h`) est un fichier de travail : le client l'écrit en
// fermant, le relit en ouvrant, et le joueur n'a aucune raison d'y toucher. Le
// journal, lui, est pour LUI — il l'ouvre dans un éditeur, il le garde, il le
// cite. D'où le texte plat plutôt que le YAML, et l'horodatage en tête de
// chaque ligne.
//
// ⚠ ET IL SURVIT À UN PLANTAGE, ce que l'historique ne peut pas : celui-ci
// s'écrit à la fermeture, donc une sortie brutale l'emporte. Le journal s'écrit
// LIGNE À LIGNE ; ce qui est dit est déjà sur le disque.
//
// 🔴 LES CHUCHOTEMENTS Y SONT EN CLAIR, comme dans l'historique, et pour la
// même raison son option est décochée par défaut.
//
// ⚠ MÊME DOSSIER QUE LE CLIENT D'ORIGINE : `<racine>\Chat\`. Le joueur qui
// connaît déjà `/savechat` va regarder là, et deux emplacements pour la même
// commande seraient une trouvaille de plus à faire.

inline constexpr std::string_view kChatLogFolderName = "Chat";

// Le dump ponctuel porte le nom du client — `Chat_<onglet>.txt` — et le journal
// continu un préfixe à lui, pour qu'un onglet nommé comme une date ne vienne
// jamais écrire dans le journal du jour.
inline constexpr std::string_view kChatDumpPrefix = "Chat_";
inline constexpr std::string_view kChatLogPrefix = "ChatLog_";
inline constexpr std::string_view kChatLogSuffix = ".txt";

// Jusqu'où va la rotation. Le client s'arrête à trois chiffres et nous aussi :
// au-delà, ce n'est plus un dump ponctuel mais un journal, et il y en a un.
inline constexpr int kLargestChatDumpRotation = 1000;

// Le nom qu'on écrit quand un onglet n'en laisse plus rien d'utilisable.
inline constexpr std::string_view kFallbackFileStem = "chat";

[[nodiscard]] std::filesystem::path ChatLogFolder(const std::filesystem::path& client_root);

// Un nom d'onglet est LIBRE : le joueur l'écrit, et rien ne l'empêche d'y mettre
// une barre oblique ou deux points. Tout ce que le système refuse devient un
// souligné — écrire hors du dossier voulu à cause d'un nom d'onglet serait le
// genre de faute qu'on ne remarque qu'une fois le fichier ailleurs.
[[nodiscard]] std::string SafeFileStem(std::string_view name);

// Une ligne telle que le fichier la porte : `[hh:mm:ss] texte`, sans fin de
// ligne. C'est la MÊME pour le dump et pour le journal — deux mises en forme
// pour le même contenu divergeraient au premier ajustement.
[[nodiscard]] std::string ChatLogLine(const PastChatLine& line);

// Ce qu'un dump a fait, pour le dire au joueur. ⚠ LE DOSSIER EN FAIT PARTIE :
// le client d'origine annonce « … is Saved. » sans jamais nommer l'endroit.
struct ChatLogDump {
  int files = 0;
  std::filesystem::path folder;
};

// Un bloc du journal, et le titre qui le coiffe.
//
// ⚠ LE TITRE N'EST PAS UNE LIGNE DE CONVERSATION : il n'a pas de genre, donc
// aucun filtre ne le retient, et il paraît dans TOUS les fichiers. C'est ce qui
// sépare la session d'avant de celle-ci — nécessaire, parce que l'heure d'une
// ligne MENT sur son jour : « 21:12 » se lit comme aujourd'hui.
struct ChatLogPart {
  std::string_view heading;
  std::span<const PastChatLine> lines;
};

// Écrit un fichier par onglet, filtré comme à l'écran, et rend le compte.
//
// ⚠ LE VIDAGE D'UN ONGLET NE S'APPLIQUE PAS ICI, et c'est délibéré. Vider est
// un geste de LECTURE — « je ne veux plus voir ça maintenant » — réversible, qui
// n'efface rien. Enregistrer est un geste d'ARCHIVE. Perdre des lignes archivées
// parce qu'on a nettoyé son écran serait une perte définitive causée par un
// geste qui ne l'était pas.
//
// Un onglet dont rien ne passe le filtre n'a pas de fichier, et un bloc qui ne
// rend rien n'écrit pas son titre : un fichier vide n'apprend rien et se
// confond avec un échec d'écriture.
[[nodiscard]] ChatLogDump DumpChatLogs(const std::filesystem::path& client_root,
                                       std::span<const ChatTab> tabs,
                                       std::span<const ChatLogPart> parts);

// LE JOURNAL CONTINU : un fichier par JOUR, en ajout.
//
// ⚠ PAR JOUR, ET NON PAR SESSION. C'est la date qu'on a en tête quand on
// retourne chercher ce qui s'est dit, pas le numéro d'une session. Deux parties
// dans la même journée se suivent donc dans le même fichier, séparées par le
// repère que l'appelant compose — celui-là même qui sépare deux sessions à
// l'écran.
class ChatLogWriter {
 public:
  // Met le journal du jour EN SERVICE. `day` est la date en `AAAA-MM-JJ` et
  // `mark` le repère à poser en tête de cette session ; l'appelant seul
  // connaît l'heure.
  //
  // ⚠ LE FICHIER NE S'OUVRE QU'À LA PREMIÈRE LIGNE, et le dossier ne se crée
  // qu'avec lui. Une partie où personne ne dit rien laisserait sinon un fichier
  // vide de plus à chaque lancement, et un repère de session qui ne coiffe
  // rien — même règle que pour le dump, et pour la même raison.
  void Open(const std::filesystem::path& client_root, std::string_view day, std::string_view mark);
  // Sans effet si le journal n'est pas en service : l'appelant écrit à chaque
  // ligne moissonnée et n'a pas à savoir si l'option est cochée.
  void Write(const PastChatLine& line);
  void Close();

  // EN SERVICE, et non « fichier ouvert » : c'est la question que pose
  // l'appelant, qui repousse la case à chaque image.
  [[nodiscard]] bool open() const noexcept { return serving_; }
  // Vide tant que rien n'a été écrit : il n'y a pas encore de fichier.
  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

 private:
  // L'ouverture différée. Rend faux une fois pour toutes si le fichier se
  // refuse : réessayer à chaque ligne noierait le journal du programme.
  bool Begin();

  std::ofstream file_;
  std::filesystem::path path_;
  std::filesystem::path folder_;
  std::string day_;
  std::string mark_;
  bool serving_ = false;
};

}  // namespace moonclient
