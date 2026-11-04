#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace moonclient {

// LE GARDE : CE QUE LE CLIENT REMARQUE DE LA MACHINE QUI LE FAIT TOURNER.
//
// 🔴 C'EST UN RALENTISSEUR, PAS UNE BARRIÈRE. Le dépôt est public : n'importe
// qui peut bâtir ce client sans garde, ou lui faire taire ce qu'il trouve. Ce
// qui protège vraiment le jeu tient au serveur (dérive d'horloge, régularité
// des actions). Le garde attrape le tricheur qui ne s'en donne pas la peine.
//
// ⚠ IL SIGNALE, IL NE SANCTIONNE PAS. Chaque indice a son usage honnête : un
// clavier virtuel ou un bureau à distance injecte ses touches, un outil de
// mémoire sert aussi au développement. Le serveur journalise, un humain tranche.
//
// Ce fichier ne tient que des règles pures, sans système ni horloge :
// `cheat_guard.h` les nourrit de ce que la machine dit.

// Une trouvaille : l'outil, et ce qui l'a trahi. Elles s'identifient par leurs
// deux champs.
struct GuardFinding {
  std::string tool;
  std::string detail;

  friend bool operator==(const GuardFinding&, const GuardFinding&) = default;
};

// ── LES SIGNATURES : DES NOMS CONNUS ───────────────────────────────────────

enum class GuardProbe : std::uint8_t {
  // Le nom d'un exécutable en cours, comparé en entier.
  kProcess,
  // Le nom d'un exécutable en cours, qui CONTIENT le motif : certains outils
  // suffixent leur nom selon le processeur.
  kProcessSubstring,
  // La classe d'une fenêtre ouverte, qui contient le motif.
  kWindowClass,
  // Le nom d'un module chargé dans CE processus, qui contient le motif.
  kModule,
};

struct GuardSignature {
  GuardProbe probe;
  std::string_view tool;
  // En minuscules : la comparaison ignore la casse du nom observé.
  std::string_view pattern;
};

// 🔴 LA TABLE DES NOMS N'EST PAS DANS LE DÉPÔT. Elle est lue, à la bâtisse,
// dans `app/guard/guard_signatures.local.inc`, que git ignore : un nom écrit
// dans un dépôt public est un nom que chacun sait éviter. Le modèle
// `guard_signatures.example.inc` en donne la forme. Sans ce fichier, la table
// est vide et cette sonde se tait ; les deux autres travaillent.
[[nodiscard]] std::span<const GuardSignature> KnownSignatures();

// La signature de `table` que trahit `name`, observé par la sonde `probe`. Un
// processus est comparé aux signatures `kProcess` ET `kProcessSubstring`.
[[nodiscard]] std::optional<GuardFinding> MatchSignature(std::span<const GuardSignature> table,
                                                         GuardProbe probe, std::string_view name);

// ── LA PROVENANCE DES TOUCHES ──────────────────────────────────────────────
//
// Trois façons pour une touche d'arriver à la fenêtre :
//   - du clavier, par le pilote : l'entrée brute nomme son périphérique ;
//   - injectée par `SendInput` : l'entrée brute arrive SANS périphérique ;
//   - postée par `PostMessage` directement à la fenêtre : elle ne passe pas par
//     la file d'entrée du système, donc l'état asynchrone du clavier ignore que
//     la touche est enfoncée au moment où le message arrive.
//
// On juge par lots de frappes : une frappe isolée lue tard (touche déjà
// relâchée) ressemble à une frappe postée, et seul un lot massivement
// synthétique dit quelque chose.

// Frappes par lot.
inline constexpr std::uint32_t kKeysPerVerdict = 64;
// Part synthétique signalée, en pour mille.
inline constexpr std::uint32_t kSyntheticSharePermille = 500;

class KeyProvenance {
 public:
  // Une entrée brute de clavier : avec ou sans périphérique d'origine.
  void NoteRawKey(bool has_device);
  // Un message de touche enfoncée reçu par la fenêtre, et ce que l'état
  // asynchrone du clavier en disait.
  void NoteKeyMessage(bool key_down_in_system);

  // Rend une trouvaille quand un lot complet est surtout synthétique. Le lot
  // repart de zéro dans tous les cas.
  [[nodiscard]] std::optional<GuardFinding> TakeVerdict();

 private:
  std::uint32_t raw_from_device_ = 0;
  std::uint32_t raw_injected_ = 0;
  std::uint32_t messages_ = 0;
  std::uint32_t messages_posted_ = 0;
};

// ── DEUX HORLOGES QUI DOIVENT S'ACCORDER ────────────────────────────────────
//
// Un accélérateur de vitesse détourne les fonctions qui donnent l'heure au
// programme (le compteur de performance, entre autres). Le temps d'interruption
// que le noyau publie dans une page partagée, lui, se lit en mémoire sans appel
// de fonction : rien ne le détourne. S'il ne suit plus le compteur, l'un des
// deux ment.
//
// ⚠ UNE MISE EN VEILLE PEUT LES DÉSACCORDER une fois : on exige deux fenêtres de
// suite, et une fenêtre anormalement longue (programme suspendu) ne compte pas.

// Durée d'une fenêtre de comparaison, en temps du noyau.
inline constexpr auto kClockWindowNs = static_cast<std::uint64_t>(
    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(30)).count());
// Au-delà de ce multiple de la fenêtre, le programme a été suspendu.
inline constexpr std::uint64_t kClockStallFactor = 3;
// Désaccord signalé, en pour mille.
inline constexpr std::uint64_t kClockDisagreementPermille = 20;
// Fenêtres consécutives en désaccord avant de signaler.
inline constexpr std::uint32_t kClockStrikes = 2;

class ClockAgreement {
 public:
  // Deux lectures du même instant : le compteur de performance et le temps
  // d'interruption du noyau, en nanosecondes.
  [[nodiscard]] std::optional<GuardFinding> Note(std::uint64_t counter_ns, std::uint64_t kernel_ns);

 private:
  bool started_ = false;
  std::uint64_t counter_start_ns_ = 0;
  std::uint64_t kernel_start_ns_ = 0;
  std::uint32_t strikes_ = 0;
};

// ── LE PORTE-PAROLE : QUOI DIRE AU SERVEUR, ET QUAND ───────────────────────
//
// Le serveur ne garde qu'un signalement par minute et par session, et ne les
// lit que d'une session reconnue. Le porte-parole tient donc une file : chaque
// trouvaille est dite une fois à chaque session reconnue — une entrée en zone en
// ouvre une neuve, qui ne sait rien des précédentes —, et jamais plus vite que
// la cadence du serveur.

// Ce que le porte-parole retient au plus : au-delà, une trouvaille neuve est
// ignorée. Une machine qui en accumule autant a déjà tout dit.
inline constexpr std::size_t kMaxFindings = 32;
// Marge sur la cadence du serveur : un signalement arrivé une milliseconde trop
// tôt serait jeté.
inline constexpr std::uint32_t kReportMarginMs = 1000;

class GuardSpokesman {
 public:
  // Retient une trouvaille. Vrai si elle est neuve et retenue.
  bool Note(GuardFinding finding);

  // Ce qu'il faut envoyer maintenant, ou rien.
  [[nodiscard]] std::optional<GuardFinding> Due(bool session_verified, std::uint32_t now_ms);

  [[nodiscard]] std::size_t known() const noexcept { return findings_.size(); }

 private:
  std::vector<GuardFinding> findings_;
  // Combien de trouvailles, dans l'ordre, la session en cours a déjà reçues.
  std::size_t told_ = 0;
  bool session_open_ = false;
  bool has_sent_ = false;
  std::uint32_t last_sent_ms_ = 0;
};

}  // namespace moonclient
