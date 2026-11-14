#include "ro/proto/packet_lengths.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using ro::proto::KnownPacketCount;
using ro::proto::kPacketVersion;
using ro::proto::kVariableLength;
using ro::proto::PacketLength;

TEST_CASE("Longueurs : la table porte la version qu'elle décrit", "[proto][lengths]") {
  // Une table sans version se confond avec celle d'un autre PACKETVER, et le
  // désaccord ne se voit qu'à la première désynchronisation.
  CHECK(kPacketVersion == 20250716);
  CHECK(KnownPacketCount() > 900);
}

TEST_CASE("Longueurs : un paquet de taille fixe rend sa taille", "[proto][lengths]") {
  // 0x0064 est la demande de connexion : le tout premier paquet d'une session,
  // et le seul dont on soit certain qu'il circule.
  const auto login = PacketLength(0x0064);
  REQUIRE(login.has_value());
  CHECK(*login == 55);
}

TEST_CASE("Longueurs : un paquet à taille variable se signale", "[proto][lengths]") {
  // Sa taille est portée par le paquet lui-même. La confondre avec une taille
  // fixe désynchronise le flux dès le premier envoi.
  const auto accept = PacketLength(0x0069);
  REQUIRE(accept.has_value());
  CHECK(*accept == kVariableLength);
}

TEST_CASE("Longueurs : les opcodes personnalisés sont là", "[proto][lengths]") {
  // Ils déclarent leur identifiant par une constante et leur taille par un
  // sizeof : aucune relecture des sources du serveur ne les aurait résolus,
  // seul son compilateur le pouvait.
  const auto custom = PacketLength(0x0f04);
  REQUIRE(custom.has_value());
  CHECK(*custom == 10);
}

TEST_CASE("Longueurs : un paquet que la table du serveur n'annonce pas", "[proto][lengths]") {
  // La table du serveur enregistre surtout ce qu'il LIT. Ce qu'il ÉCRIT part par
  // clif_send(&p, sizeof(p)), et son opcode est posé depuis le code, par une
  // constante que rien ne rattache à la structure. Ces paquets-là n'ont de
  // longueur que celle de leur structure, et sans elle le client ferme la
  // session sur un opcode qu'il ne sait pas mesurer.
  //
  // 0x0a41 annonce la portée au sol d'une compétence en préparation. Il arrive
  // dès qu'un voisin en lance une, donc dans la première minute de jeu.
  const auto skill_scale = PacketLength(0x0A41);
  REQUIRE(skill_scale.has_value());
  CHECK(*skill_scale == 18);
}

TEST_CASE("Longueurs : la phase de connexion est couverte", "[proto][lengths]") {
  // Le serveur de carte est le seul à tenir une table ; les serveurs de
  // connexion et de personnages déclarent leurs paquets par des structures.
  // Une table tirée du seul serveur de carte ignore donc TOUT ce qui précède
  // l'entrée en jeu, et le client refuse la session avant de l'avoir ouverte.
  //
  // 0x083e est le refus de connexion : le premier paquet qu'une session reçoit
  // quand le compte n'existe pas, donc le premier qu'un client rencontre.
  const auto refusal = PacketLength(0x083E);
  REQUIRE(refusal.has_value());
  CHECK(*refusal == 26);

  // Les autres étapes du même chemin. Ce que ce cas vérifie est leur PRÉSENCE :
  // c'est elle qui manquait, et une longueur absente ne se distingue pas d'un
  // opcode hostile — le lecteur ferme dans les deux cas.
  CHECK(PacketLength(0x0AC4).has_value());  // acceptation, liste des serveurs
  CHECK(PacketLength(0x006B).has_value());  // liste des personnages
  CHECK(PacketLength(0x0AC5).has_value());  // adresse du serveur de zone
  CHECK(PacketLength(0x0081).has_value());  // déconnexion imposée
}

TEST_CASE("Longueurs : un opcode inconnu ne rend PAS zéro", "[proto][lengths]") {
  // Rendre zéro laisserait croire à un paquet vide, et le lecteur avancerait
  // de zéro octet en boucle. Il faut pouvoir distinguer « je ne sais pas » de
  // « rien à lire » : sur un opcode inconnu, la seule issue est de fermer.
  CHECK_FALSE(PacketLength(0xFFFF).has_value());
  CHECK_FALSE(PacketLength(0x0000).has_value());
}

TEST_CASE("Longueurs : la table est TRIÉE, comme la recherche le suppose", "[proto][lengths]") {
  // La recherche est dichotomique. Une table mal triée rendrait des réponses
  // fausses sur une partie des opcodes seulement — le pire des symptômes.
  std::int32_t previous = -1;
  std::size_t found = 0;
  for (std::int32_t opcode = 0; opcode <= 0xFFFF; ++opcode) {
    if (PacketLength(static_cast<std::uint16_t>(opcode)).has_value()) {
      CHECK(opcode > previous);
      previous = opcode;
      ++found;
    }
  }
  // Le balayage doit retrouver EXACTEMENT ce que la table annonce ; s'il en
  // trouve moins, c'est que la dichotomie en manque.
  CHECK(found == KnownPacketCount());
}
