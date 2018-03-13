#include "MessageIdentifiers.h"
#include "RakPeerInterface.h"
#include "BitStream.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <map>
#include <mutex>
#include <string>

static unsigned int SERVER_PORT = 65000;
static unsigned int CLIENT_PORT = 65001;
static unsigned int MAX_CONNECTIONS = 3;

enum NetworkState
{
	NS_Init = 0,
	NS_PendingStart,
	NS_Started,
	NS_Lobby,
	NS_Pending,
	NS_PickClass,
	NS_GameplayIntro,
	NS_Gameplay,
	NS_Dead,
	NS_GameOver,
};

bool isServer = false;
bool isRunning = true;

int playersReady = 0;
int whosTurn = 0;
int statPart = 0;
int attPart = 0;
int healPart = 0;
int livingPlayers = 3;

bool goingFirst = true;

RakNet::RakPeerInterface *g_rakPeerInterface = nullptr;
RakNet::SystemAddress g_serverAddress;

std::mutex g_networkState_mutex;
NetworkState g_networkState = NS_Init;

enum {
	ID_THEGAME_LOBBY_READY = ID_USER_PACKET_ENUM,
	ID_PLAYER_READY,
	ID_PICK_CLASS,
	ID_ASSIGN_CLASS,
	ID_CLASS_PICKED,
	ID_GAME_START,
	ID_SERVERGET_STATS,
	ID_CLIENT_READ_STATS,
	ID_ATTACK_TARGET,
	ID_TELL_CLIENT_OF_ATTACK,
	ID_INEED_HEALING,
	ID_TELL_CLIENT_OF_HEAL,
	ID_WHOS_ON_FIRST,
	ID_NOT_YOUR_TURN,
	ID_INFORM_NEXT_OF_KIN,
	ID_SEND_TO_GRAVEYARD,
	ID_GAME_OVER,
	ID_WINNER,
};

enum ePlayerClass
{
	mage = 0,
	ranger,
	warrior,
	hacker,
};

struct SPlayer
{
	std::string name;
	std::string classString;
	RakNet::SystemAddress address;
	int m_health;
	int m_attack;
	int m_heal;
	ePlayerClass m_class;

	unsigned int turnPosition;
};

std::map<unsigned long, SPlayer> m_players;

//server
void OnIncomingConnection(RakNet::Packet* packet)
{
	//must be server in order to recieve connection
	assert(isServer);
	m_players.insert(std::make_pair(RakNet::RakNetGUID::ToUint32(packet->guid), SPlayer()));
	std::cout << "Total Players: " << m_players.size() << std::endl;
	std::cout << "This player's key is: " << RakNet::RakNetGUID::ToUint32(packet->guid) << std::endl;

}

void sendPacketsToClients(RakNet::MessageID id)
{
	//send packet back
	std::string nullIn = "null";
	RakNet::BitStream myBitStream;
	//first thing to write, is packet message identifier
	myBitStream.Write(id);
	RakNet::RakString name(nullIn.c_str());
	myBitStream.Write(name);

	for (auto const& x : m_players)
	{
		g_rakPeerInterface->Send(&myBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, x.second.address, false);
	}
}


//client
void OnConnectionAccepted(RakNet::Packet* packet)
{
	//server should not ne connecting to anybody, 
	//clients connect to server
	assert(!isServer);
	g_networkState_mutex.lock();
	g_networkState = NS_Lobby;
	g_networkState_mutex.unlock();
	g_serverAddress = packet->systemAddress;
}

//client side message
void DisplayPlayerReady(RakNet::Packet*packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);

	std::cout << userName.C_String() << " is in the server." << std::endl;
}

//server side
void OnLobbyReady(RakNet::Packet* packet)
{
	unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
	std::map<unsigned long, SPlayer>::iterator it = m_players.find(guid);
	//somehow player didn't connect but now is in lobby ready
	assert(it != m_players.end());

	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);

	SPlayer& player = it->second;
	player.name = userName;
	m_players.find(guid)->second.address = packet->systemAddress;
	std::cout << userName.C_String() << " aka " << player.name.c_str() << " IS READY!!!!!" << std::endl;
	std::cout << "Their GUID is" << guid << std::endl;

	//notify all other connected players that this player is joining
	for (std::map<unsigned long, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it)
	{
		if (guid == it->first)
		{
			continue;
		}
		SPlayer& player = it->second;
		//player.name = userName;

		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_PLAYER_READY);
		RakNet::RakString name(player.name.c_str());
		writeBs.Write(name);

		//returns 0 when something is wrong												RakNet::UNASSIGNED_SYSTEM_ADDRESS
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false));
	}
	RakNet::BitStream writeBs;
	writeBs.Write((RakNet::MessageID)ID_PLAYER_READY);
	RakNet::RakString name(player.name.c_str());
	writeBs.Write(name);
	assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true));

	playersReady += 1;
	if (playersReady == 3)//testing
	{
		NetworkState ns = NS_PickClass;
		RakNet::BitStream bs;
		bs.Write((RakNet::MessageID)ID_PICK_CLASS);
		bs.Write(ns);

		g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, true);
		playersReady = 0;
	}
}

//client side Pat change
void OnLobbyReadyAll(RakNet::Packet* packet) 
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	NetworkState ns;
	bs.Read(ns);

	g_networkState = ns;
}

//server side
void OnClassSelect(RakNet::Packet* packet)
{
	unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
	std::map<unsigned long, SPlayer>::iterator it = m_players.find(guid);

	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString classChoice;
	bs.Read(classChoice);

	SPlayer& player = it->second;

	if (strcmp(classChoice, "mage") == 0)
	{
		player.m_class = mage;
		std::cout << player.name.c_str() << " is a mage." << std::endl;
		player.m_health = 6;
		player.m_attack = 2;
		player.m_heal = 4;
		player.classString = "mage";
	}
	else if (strcmp(classChoice, "ranger") == 0)
	{
		player.m_class = ranger;
		player.m_health = 9;
		player.m_attack = 3;
		player.m_heal = 3;
		player.classString = "ranger";
		std::cout << player.name.c_str() << " is a ranger." << std::endl;
	}
	else if (strcmp(classChoice, "warrior") == 0)
	{
		player.m_class = warrior;
		player.m_health = 12;
		player.m_attack = 1;
		player.m_heal = 2;
		std::cout << player.name.c_str() << " is a warrior." << std::endl;
		player.classString = "warrior";
	}
	else if (strcmp(classChoice, "hacker") == 0)
	{
		player.m_class = hacker;
		player.m_health = 999;
		player.m_attack = 999;
		player.m_heal = 999;
		std::cout << player.name.c_str() << " is a cheating cheater who cheats." << std::endl;
		player.classString = "hacker";
	}

	whosTurn++;
	player.turnPosition = whosTurn;

	RakNet::BitStream writeBs;
	writeBs.Write((RakNet::MessageID)ID_CLASS_PICKED);
	RakNet::RakString name(player.name.c_str());
	writeBs.Write(name);
	assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true));

	playersReady += 1;
	if (playersReady == 3)//testing #2
	{
		NetworkState ns = NS_GameplayIntro;
		RakNet::BitStream bs;
		bs.Write((RakNet::MessageID)ID_GAME_START);
		bs.Write(ns);
		std::cout << "All players chose a class.\n" << std::endl;
		g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, true);
		playersReady = 0;
	}

	//std::cout << userName.C_String() << " aka " << player.name.c_str() << " IS READY!!!!!" << std::endl;
}

//displays on CLIENT
void DisplayClassesPicked(RakNet::Packet*packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);

	std::cout << userName.C_String() << " has picked their class." << std::endl;
	
}

//changing CLIENT state
void TimeToPlay(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	NetworkState ns;
	bs.Read(ns);

	g_networkState = ns;
}

//serverside right now
void DisplayStats(RakNet::Packet* packet)
{
	unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
	std::map<unsigned long, SPlayer>::iterator it = m_players.find(guid);

	for (std::map<unsigned long, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it)
	{
		/*if (guid == it->first)
		{
			continue;
		}*/
		SPlayer& player = it->second;
		//player.name = userName;
		/*
		std::cout << "Player Name: "<< player.name.c_str() << std::endl;
		std::cout << "Player Class: " << player.classString.c_str() << std::endl;
		std::cout << "Current Health: " << player.m_health << std::endl;*/
		std::string hp = std::to_string(player.m_health);
		std::string turn = std::to_string(player.turnPosition);

		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_CLIENT_READ_STATS);
		RakNet::RakString playerStats(player.name.c_str());
		writeBs.Write(playerStats);
		g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

		RakNet::BitStream writeBs2;
		writeBs2.Write((RakNet::MessageID)ID_CLIENT_READ_STATS);
		RakNet::RakString playerStats2(player.classString.c_str());
		writeBs2.Write(playerStats2);
		g_rakPeerInterface->Send(&writeBs2, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

		RakNet::BitStream writeBs3;
		writeBs3.Write((RakNet::MessageID)ID_CLIENT_READ_STATS);
		RakNet::RakString playerStats3(hp.c_str());
		writeBs3.Write(playerStats3);
		g_rakPeerInterface->Send(&writeBs3, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

		RakNet::BitStream writeBs4;
		writeBs4.Write((RakNet::MessageID)ID_CLIENT_READ_STATS);
		RakNet::RakString playerStats4(turn.c_str());
		writeBs4.Write(playerStats4);
		g_rakPeerInterface->Send(&writeBs4, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);
	}
}

//clientside
void ClientReadStats(RakNet::Packet* packet)
{
	statPart++;
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString stat;
	bs.Read(stat);
	if (statPart == 1) 
	{
		std::cout << "Player Name: " << stat.C_String() <<  std::endl;
	}
	else if (statPart == 2)
	{
		std::cout << "Player Class: " << stat.C_String() << std::endl;
	}
	else if (statPart == 3)
	{
		std::cout << "Player Health: " << stat.C_String() << std::endl;
	}
	else if (statPart == 4)
	{
		std::cout << "Player Turn Order: " << stat.C_String() <<" [1 or 0 means it's their turn, unless they are dead.]\n"<< std::endl;
		statPart = 0;
	}
	
}

//server side combat numbers
void AttackADude(RakNet::Packet* packet)
{
	unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
	std::map<unsigned long, SPlayer>::iterator it = m_players.find(guid);

	bool doneOnce = false;
	int noName = 0;

	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString target;
	bs.Read(target);
	int attackPow = it->second.m_attack;
	if (it->second.turnPosition <= 1 && it->second.m_health > 0) {

		for (std::map<unsigned long, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it)
		{
			if (guid == it->first)
			{
				continue;
			}
			SPlayer& player = it->second;
			//std::cout << player.name.c_str() << " was cycled through this loop." << std::endl;
			player.turnPosition -= 1;



			if (strcmp(target, player.name.c_str()) == 0 && player.m_health > 0)
			{
				noName += 4;
				player.m_health = player.m_health - attackPow;
				std::cout << player.name.c_str() << " was attacked for " << std::to_string(attackPow) << " dammage. They now have " << std::to_string(player.m_health) << " health." << std::endl;
				
				std::string attPow = std::to_string(attackPow);
				std::string hp = std::to_string(player.m_health);

				RakNet::BitStream writeBs;
				writeBs.Write((RakNet::MessageID)ID_TELL_CLIENT_OF_ATTACK);
				RakNet::RakString playerStats(player.name.c_str());
				writeBs.Write(playerStats);
				g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

				RakNet::BitStream writeBs2;
				writeBs2.Write((RakNet::MessageID)ID_TELL_CLIENT_OF_ATTACK);
				RakNet::RakString playerStats2(attPow.c_str());
				writeBs2.Write(playerStats2);
				g_rakPeerInterface->Send(&writeBs2, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

				RakNet::BitStream writeBs3;
				writeBs3.Write((RakNet::MessageID)ID_TELL_CLIENT_OF_ATTACK);
				RakNet::RakString playerStats3(hp.c_str());
				writeBs3.Write(playerStats3);
				g_rakPeerInterface->Send(&writeBs3, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

				if (player.m_health <= 0)
				{
					livingPlayers -= 1;
					RakNet::BitStream writeBs4;
					writeBs4.Write((RakNet::MessageID)ID_INFORM_NEXT_OF_KIN);
					RakNet::RakString playerStats(player.name.c_str());
					writeBs4.Write(playerStats);
					g_rakPeerInterface->Send(&writeBs4, HIGH_PRIORITY, RELIABLE_ORDERED, 0, player.address, true);

					NetworkState ns = NS_Dead;
					RakNet::BitStream writeBs5;
					writeBs5.Write((RakNet::MessageID)ID_SEND_TO_GRAVEYARD);
					writeBs5.Write(ns);
					g_rakPeerInterface->Send(&writeBs5, HIGH_PRIORITY, RELIABLE_ORDERED, 0, player.address, false);

					for (std::map<unsigned long, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it)
					{
						
						SPlayer& player = it->second;
						//std::cout << player.name.c_str() << " was cycled through the dead loop." << std::endl;
						player.turnPosition -= 1;
					}
				}

				if (doneOnce == false) 
				{
					doneOnce = true;

					RakNet::BitStream writeBs;
					writeBs.Write((RakNet::MessageID)ID_TELL_CLIENT_OF_ATTACK);
					RakNet::RakString playerStats(player.name.c_str());
					writeBs.Write(playerStats);
					g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true);

					RakNet::BitStream writeBs2;
					writeBs2.Write((RakNet::MessageID)ID_TELL_CLIENT_OF_ATTACK);
					RakNet::RakString playerStats2(attPow.c_str());
					writeBs2.Write(playerStats2);
					g_rakPeerInterface->Send(&writeBs2, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true);

					RakNet::BitStream writeBs3;
					writeBs3.Write((RakNet::MessageID)ID_TELL_CLIENT_OF_ATTACK);
					RakNet::RakString playerStats3(hp.c_str());
					writeBs3.Write(playerStats3);
					g_rakPeerInterface->Send(&writeBs3, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true);
				}
			}
			else
			{
				std::cout << "Not the right target or they're already dead.\n" << std::endl;
				
					//SPlayer& player = it->second;
				noName += 1;
			}

		}
		if (noName == 2)
		{
			RakNet::BitStream writeBs;
			writeBs.Write((RakNet::MessageID)ID_WHOS_ON_FIRST);
			RakNet::RakString playerStats(it->second.name.c_str());
			writeBs.Write(playerStats);
			g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true);

			RakNet::BitStream writeBs2;
			writeBs2.Write((RakNet::MessageID)ID_WHOS_ON_FIRST);
			RakNet::RakString playerStats2(it->second.name.c_str());
			writeBs2.Write(playerStats2);
			g_rakPeerInterface->Send(&writeBs2, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);
		}
		it->second.turnPosition = livingPlayers;
		if (livingPlayers == 1)
		{
			SPlayer& player = it->second;
			NetworkState ns = NS_PickClass;
			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)ID_GAME_OVER);
			bs.Write(ns);

			g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, true);

			RakNet::BitStream writeBs;
			writeBs.Write((RakNet::MessageID)ID_WINNER);
			RakNet::RakString name(player.name.c_str());
			writeBs.Write(name);

			//returns 0 when something is wrong												RakNet::UNASSIGNED_SYSTEM_ADDRESS
			g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

			RakNet::BitStream writeBs2;
			writeBs2.Write((RakNet::MessageID)ID_WINNER);
			RakNet::RakString name2(player.name.c_str());
			writeBs2.Write(name2);
			g_rakPeerInterface->Send(&writeBs2, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true);
		}
	}
	else
	{
		std::cout << "It's not your turn yet or you are dead.\n" << std::endl;

		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_NOT_YOUR_TURN);
		RakNet::RakString playerStats("word");
		writeBs.Write(playerStats);
		g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);
	}
}

//to client
void ClientReadAttack(RakNet::Packet* packet)
{
	attPart++;
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString stat;
	bs.Read(stat);
	if (attPart == 1)
	{
		std::cout << stat.C_String() << " was attacked."<< std::endl;
	}
	else if (attPart == 2)
	{
		std::cout << "They took " << stat.C_String() << " damage."<< std::endl;
	}
	else if (attPart == 3)
	{
		std::cout << "They now have " << stat.C_String() << " health.\n"<< std::endl;
		attPart = 0;
	}
}


//server
void HealYoSelf(RakNet::Packet* packet)
{
	unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
	std::map<unsigned long, SPlayer>::iterator it = m_players.find(guid);

	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString target;
	bs.Read(target);
	int healstr = it->second.m_heal;


	if (it->second.turnPosition <= 1 && it->second.m_health > 0)
	{
		SPlayer& player = it->second;
		it->second.m_health = it->second.m_health + it->second.m_heal;

		bool doneOnce = false;
		std::string attPow = std::to_string(healstr);
		std::string hp = std::to_string(player.m_health);

		for (std::map<unsigned long, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it)
		{
			it->second.turnPosition -= 1;

			if (guid == it->first)
			{
				RakNet::BitStream writeBs;
				writeBs.Write((RakNet::MessageID)ID_TELL_CLIENT_OF_HEAL);
				RakNet::RakString playerStats(player.name.c_str());
				writeBs.Write(playerStats);
				g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

				RakNet::BitStream writeBs2;
				writeBs2.Write((RakNet::MessageID)ID_TELL_CLIENT_OF_HEAL);
				RakNet::RakString playerStats2(attPow.c_str());
				writeBs2.Write(playerStats2);
				g_rakPeerInterface->Send(&writeBs2, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

				RakNet::BitStream writeBs3;
				writeBs3.Write((RakNet::MessageID)ID_TELL_CLIENT_OF_HEAL);
				RakNet::RakString playerStats3(hp.c_str());
				writeBs3.Write(playerStats3);
				g_rakPeerInterface->Send(&writeBs3, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

				continue;
			}
			if (doneOnce == false)
			{
				doneOnce = true;

				RakNet::BitStream writeBs;
				writeBs.Write((RakNet::MessageID)ID_TELL_CLIENT_OF_HEAL);
				RakNet::RakString playerStats(player.name.c_str());
				writeBs.Write(playerStats);
				g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true);

				RakNet::BitStream writeBs2;
				writeBs2.Write((RakNet::MessageID)ID_TELL_CLIENT_OF_HEAL);
				RakNet::RakString playerStats2(attPow.c_str());
				writeBs2.Write(playerStats2);
				g_rakPeerInterface->Send(&writeBs2, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true);

				RakNet::BitStream writeBs3;
				writeBs3.Write((RakNet::MessageID)ID_TELL_CLIENT_OF_HEAL);
				RakNet::RakString playerStats3(hp.c_str());
				writeBs3.Write(playerStats3);
				g_rakPeerInterface->Send(&writeBs3, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true);
			}
		}
		it->second.turnPosition = livingPlayers;
	}
	else
	{
		std::cout << "It's not your turn yet or you are dead.\n" << std::endl;

		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_NOT_YOUR_TURN);
		RakNet::RakString playerStats("word");
		writeBs.Write(playerStats);
		g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);
	}
}

//client
void ClientReadHeal(RakNet::Packet* packet)
{
	healPart++;
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString stat;
	bs.Read(stat);
	if (healPart == 1)
	{
		std::cout << stat.C_String() << " healed themself." << std::endl;
	}
	else if (healPart == 2)
	{
		std::cout << "They healed " << stat.C_String() << " health." << std::endl;
	}
	else if (healPart == 3)
	{
		std::cout << "They now have " << stat.C_String() << " health.\n" << std::endl;
		healPart = 0;
	}
}

//client msg
void ClientNotYourTurn(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);

	std::cout << "It's not your turn.\n" << std::endl;
}

void KillNotification(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);

	std::cout << userName <<" was killed.\n" << std::endl;
}

void YOUDIED(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	NetworkState ns;
	bs.Read(ns);
	std::cout << "YOU ARE DEAD." << std::endl;
	g_networkState = ns;
}


void GameOverMsg(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	NetworkState ns;
	bs.Read(ns);
	std::cout << "Game Over" << std::endl;

	g_networkState = ns;
}


void WinnerMsg(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);

	std::cout << userName.C_String() << " is the winner." << std::endl;
}


void YourTurn(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);

	std::cout << userName.C_String() << " messed up their attack so it is not their turn any more." << std::endl;
	std::cout << "Check 'stats' to see the updated turn order.\n" << std::endl;
}




unsigned char GetPacketIdentifier(RakNet::Packet *packet)
{
	if (packet == nullptr)
		return 255;

	if ((unsigned char)packet->data[0] == ID_TIMESTAMP)
	{
		RakAssert(packet->length > sizeof(RakNet::MessageID) + sizeof(RakNet::Time));
		return (unsigned char)packet->data[sizeof(RakNet::MessageID) + sizeof(RakNet::Time)];
	}
	else
		return (unsigned char)packet->data[0];
}

//client side
void InputHandler()
{
	while (isRunning)
	{
		char userInput[255];
		if (g_networkState == NS_Init)
		{
			std::cout << "press (s) for server (c) for client" << std::endl;
			std::cin >> userInput;
			isServer = (userInput[0] == 's');
			g_networkState_mutex.lock();
			g_networkState = NS_PendingStart;
			g_networkState_mutex.unlock();
		}
		else if (g_networkState == NS_Lobby)
		{
			std::cout << "Enter your name to play or type quit to leave" << std::endl;
			std::cin >> userInput;
			//quitting is not acceptable in our game, create a crash to teach lesson
			assert(strcmp(userInput, "quit"));

			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)ID_THEGAME_LOBBY_READY);
			RakNet::RakString name(userInput);
			bs.Write(name);

			//returns 0 when something is wrong
			assert(g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));
			g_networkState_mutex.lock();
			g_networkState = NS_Pending;
			g_networkState_mutex.unlock();
			//playersReady += 1;
		}
		else if (g_networkState == NS_Pending)
		{
			static bool doOnce = false;
			if(!doOnce)
				std::cout << "pending..." << std::endl;
			doOnce = true;

		}
		else if (g_networkState == NS_PickClass)
		{
			static bool doOnce = false;
			if (!doOnce)
			{
				std::cout << "Enter 'mage', 'ranger' or 'warrior' to pick your class." << std::endl;
				std::cout << "Mage. Health = 6. Attack = 2. Heal = 4. " << std::endl;
				std::cout << "Ranger. Health = 9. Attack = 3. Heal = 3. " << std::endl;
				std::cout << "Warrior. Health = 12. Attack = 1. Heal = 2. " << std::endl;
				std::cout << "Turn order is determined based on who picks first.\n" << std::endl;
				std::cin >> userInput;

				if (strcmp(userInput, "warrior") == 0 || strcmp(userInput, "ranger") == 0 || strcmp(userInput, "mage") == 0 || strcmp(userInput, "hacker") == 0)
				{
					RakNet::BitStream bs;
					bs.Write((RakNet::MessageID)ID_ASSIGN_CLASS);
					RakNet::RakString playerClass(userInput);
					bs.Write(playerClass);
					std::cout << "Choice made! Now wait for the others.\n" << std::endl;
					(g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));
					//g_networkState = NS_Pending;
					//std::this_thread::sleep_for(std::chrono::minutes(1));
					doOnce = true;
				}
				else {
					std::cout << "Try again." << std::endl;
					doOnce = false;
				}
			}
		}
		else if (g_networkState == NS_GameplayIntro)
		{
			std::cout << "Prepare for combat.\n" << std::endl;
			g_networkState = NS_Gameplay;
		}
		else if (g_networkState == NS_Gameplay)
		{
			std::cout << "What would you like to do?" << std::endl;
			std::cout << "Type 'stats' to see the health of all players." << std::endl;
			std::cout << "If it is your turn, you may type 'attack' or 'heal'.\n" << std::endl;
			std::cin >> userInput;
			if (strcmp(userInput, "stats") == 0)
			{
				RakNet::BitStream bs;
				bs.Write((RakNet::MessageID)ID_SERVERGET_STATS);
				RakNet::RakString playerClass(userInput);
				bs.Write(playerClass);
				//std::cout << "Choice made! Now wait for the others." << std::endl;
				(g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));
			}
			else if (strcmp(userInput, "attack") == 0)
			{
				std::cout << "Type the name of your target. Case sensitive." << std::endl;
				std::cin >> userInput;
				RakNet::BitStream bs;
				bs.Write((RakNet::MessageID)ID_ATTACK_TARGET);
				RakNet::RakString target(userInput);
				bs.Write(target);
				//std::cout << "Choice made! Now wait for the others." << std::endl;
				(g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));
			}
			else if (strcmp(userInput, "heal") == 0)
			{
				RakNet::BitStream bs;
				bs.Write((RakNet::MessageID)ID_INEED_HEALING);
				RakNet::RakString playerClass(userInput);
				bs.Write(playerClass);
				//std::cout << "Choice made! Now wait for the others." << std::endl;
				(g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));
			}
			else {
				std::cout << "Not a valid input.\n" << std::endl;
			}
		}
		else if (g_networkState == NS_Dead)
		{
			std::cout << "You are dead. Wait for the game to end." << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(6));
		}
		else if (g_networkState == NS_GameOver)
		{
			static bool doOnce = false;
			if (!doOnce)
				std::cout << "Game Over" << std::endl;
			doOnce = true;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

bool HandleLowLevelPackets(RakNet::Packet* packet)
{
	bool isHandled = true;
	// We got a packet, get the identifier with our handy function
	unsigned char packetIdentifier = GetPacketIdentifier(packet);

	// Check if this is a network message packet
	switch (packetIdentifier)
	{
	case ID_DISCONNECTION_NOTIFICATION:
		// Connection lost normally
		printf("ID_DISCONNECTION_NOTIFICATION\n");
		break;
	case ID_ALREADY_CONNECTED:
		// Connection lost normally
		printf("ID_ALREADY_CONNECTED with guid %" PRINTF_64_BIT_MODIFIER "u\n", packet->guid);
		break;
	case ID_INCOMPATIBLE_PROTOCOL_VERSION:
		printf("ID_INCOMPATIBLE_PROTOCOL_VERSION\n");
		break;
	case ID_REMOTE_DISCONNECTION_NOTIFICATION: // Server telling the clients of another client disconnecting gracefully.  You can manually broadcast this in a peer to peer enviroment if you want.
		printf("ID_REMOTE_DISCONNECTION_NOTIFICATION\n");
		break;
	case ID_REMOTE_CONNECTION_LOST: // Server telling the clients of another client disconnecting forcefully.  You can manually broadcast this in a peer to peer enviroment if you want.
		printf("ID_REMOTE_CONNECTION_LOST\n");
		break;
	case ID_NEW_INCOMING_CONNECTION:
		//client connecting to server
		OnIncomingConnection(packet);
		printf("ID_NEW_INCOMING_CONNECTION\n");
		break;
	case ID_REMOTE_NEW_INCOMING_CONNECTION: // Server telling the clients of another client connecting.  You can manually broadcast this in a peer to peer enviroment if you want.
		OnIncomingConnection(packet);
		printf("ID_REMOTE_NEW_INCOMING_CONNECTION\n");
		break;
	case ID_CONNECTION_BANNED: // Banned from this server
		printf("We are banned from this server.\n");
		break;
	case ID_CONNECTION_ATTEMPT_FAILED:
		printf("Connection attempt failed\n");
		break;
	case ID_NO_FREE_INCOMING_CONNECTIONS:
		// Sorry, the server is full.  I don't do anything here but
		// A real app should tell the user
		printf("ID_NO_FREE_INCOMING_CONNECTIONS\n");
		break;

	case ID_INVALID_PASSWORD:
		printf("ID_INVALID_PASSWORD\n");
		break;

	case ID_CONNECTION_LOST:
		// Couldn't deliver a reliable packet - i.e. the other system was abnormally
		// terminated
		printf("ID_CONNECTION_LOST\n");
		break;

	case ID_CONNECTION_REQUEST_ACCEPTED:
		// This tells the client they have connected
		printf("ID_CONNECTION_REQUEST_ACCEPTED to %s with GUID %s\n", packet->systemAddress.ToString(true), packet->guid.ToString());
		printf("My external address is %s\n", g_rakPeerInterface->GetExternalID(packet->systemAddress).ToString(true));
		OnConnectionAccepted(packet);
		break;
	case ID_CONNECTED_PING:
	case ID_UNCONNECTED_PING:
		printf("Ping from %s\n", packet->systemAddress.ToString(true));
		break;
	default:
		isHandled = false;
		break;
	}
	return isHandled;
}

void PacketHandler()
{
	while (isRunning)
	{
		for (RakNet::Packet* packet = g_rakPeerInterface->Receive(); packet != nullptr; g_rakPeerInterface->DeallocatePacket(packet), packet = g_rakPeerInterface->Receive())
		{
			if (!HandleLowLevelPackets(packet))
			{
				//our game specific packets
				unsigned char packetIdentifier = GetPacketIdentifier(packet);
				switch (packetIdentifier)
				{
				case ID_THEGAME_LOBBY_READY:
					OnLobbyReady(packet);
					break;

				case ID_PLAYER_READY:
					DisplayPlayerReady(packet);
					break;

				case ID_PICK_CLASS:
					OnLobbyReadyAll(packet);
					break;

				case ID_ASSIGN_CLASS:
					OnClassSelect(packet);
					break;

				case ID_CLASS_PICKED:
					DisplayClassesPicked(packet);
					break;

				case ID_GAME_START:
					TimeToPlay(packet);
					break;

				case ID_SERVERGET_STATS:
					DisplayStats(packet);
					break;

				case ID_CLIENT_READ_STATS:
					ClientReadStats(packet);
					break;

				case ID_ATTACK_TARGET:
					AttackADude(packet);
					break;

				case ID_TELL_CLIENT_OF_ATTACK:
					ClientReadAttack(packet);
					break;

				case ID_INEED_HEALING:
					HealYoSelf(packet);
					break;

				case ID_TELL_CLIENT_OF_HEAL:
					ClientReadHeal(packet);
					break;

				case ID_WHOS_ON_FIRST:
					YourTurn(packet);
					break;

				case ID_NOT_YOUR_TURN:
					ClientNotYourTurn(packet);
					break;

				case ID_INFORM_NEXT_OF_KIN:
					KillNotification(packet);
					break;

				case ID_SEND_TO_GRAVEYARD:
					YOUDIED(packet);
					break;

				case ID_GAME_OVER:
					GameOverMsg(packet);
					break;

				case ID_WINNER:
					WinnerMsg(packet);
					break;

				default:
					break;
				}
			}
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

int main()
{
	g_rakPeerInterface = RakNet::RakPeerInterface::GetInstance();

	std::thread inputHandler(InputHandler);
	std::thread packetHandler(PacketHandler);

	while (isRunning)
	{
		if (g_networkState == NS_PendingStart)
		{
			if (isServer)
			{
				RakNet::SocketDescriptor socketDescriptors[1];
				socketDescriptors[0].port = SERVER_PORT;
				socketDescriptors[0].socketFamily = AF_INET; // Test out IPV4

				bool isSuccess = g_rakPeerInterface->Startup(MAX_CONNECTIONS, socketDescriptors, 1) == RakNet::RAKNET_STARTED;
				assert(isSuccess);
				//ensures we are server
				g_rakPeerInterface->SetMaximumIncomingConnections(MAX_CONNECTIONS);
				std::cout << "server started" << std::endl;
				g_networkState = NS_Started;
			}
			//client
			else
			{
				RakNet::SocketDescriptor socketDescriptor(CLIENT_PORT, 0);
				socketDescriptor.socketFamily = AF_INET;

				while (RakNet::IRNS2_Berkley::IsPortInUse(socketDescriptor.port, socketDescriptor.hostAddress, socketDescriptor.socketFamily, SOCK_DGRAM) == true)
					socketDescriptor.port++;

				RakNet::StartupResult result = g_rakPeerInterface->Startup(8, &socketDescriptor, 1);
				assert(result == RakNet::RAKNET_STARTED);
				g_rakPeerInterface->SetOccasionalPing(true);
				//client connection
				//127.0.0.1 or 10.4.89.157 or 192.168.0.31
				RakNet::ConnectionAttemptResult car = g_rakPeerInterface->Connect("10.4.89.105", SERVER_PORT, nullptr, 0);
				RakAssert(car == RakNet::CONNECTION_ATTEMPT_STARTED);
				std::cout << "Client attempted connection" << std::endl;
				g_networkState = NS_Started;
			}
		}
	}


	packetHandler.join();
	inputHandler.join();
	return 0;
}