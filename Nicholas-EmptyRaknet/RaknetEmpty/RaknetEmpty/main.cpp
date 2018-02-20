
#include "MessageIdentifiers.h"
#include "RakPeerInterface.h"

#include <iostream>
#include <thread>         // std::thread
#include <chrono>
#include "main.h"

static int SERVER_PORT = 65000;
static int CLIENT_PORT = 65001;
static int MAX_CONNECTIONS = 4;

RakNet::RakPeerInterface *g_rakPeerInterface = nullptr;

bool isServer = false;
bool isRunning = true;
unsigned short g_totalPlayers = 0;

enum NetworkStates
{
	NS_Decision = 0,
	NS_CreateSocket,
	NS_PendingConnection,
	NS_Connected,
	NS_Running,
	NS_Lobby
};

NetworkStates g_networkState = NS_Decision;

void OnConnectionAccepted(RakNet::Packet* packet) 
{
	if (isServer)
	{
		//Server should never request connections, only clients do
		assert(0);
	}
	//we have successfully connected, go to lobby
	g_networkState = NS_Lobby;
}

void OnIncomingConnection(RakNet::Packet* packet)
{
	if (!isServer)
	{
		assert(0);
	}
	g_totalPlayers++;
	unsigned short numConnections = g_rakPeerInterface->NumberOfConnections();
	std::cout << "total Players: " << g_totalPlayers << "Num Connection: " << numConnections;
	
}


void InputHandler()
{
	while (isRunning)
	{
		char userInput[255];
		if (g_networkState == NS_Decision) {
			std::cout << "Press (s) for server, (c) for client" << std::endl;
			std::cin >> userInput;
			isServer = userInput[0] == 's';
			g_networkState = NS_CreateSocket;
		}
		else if(g_networkState == NS_CreateSocket)
		{
			if (isServer) 
			{
				std::cout << "Server creating socket..." << std::endl;
			}
			else
			{
				std::cout << "Client creating socket..." << std::endl;
			}
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
	
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

void PacketHandler()
{
	while (isRunning)
	{
		for (RakNet::Packet* packet = g_rakPeerInterface->Receive(); packet != nullptr; g_rakPeerInterface->DeallocatePacket(packet), packet = g_rakPeerInterface->Receive())
		{
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
			case ID_REMOTE_NEW_INCOMING_CONNECTION: // Server telling the clients of another client connecting.  You can manually broadcast this in a peer to peer enviroment if you want.
				printf("ID_REMOTE_NEW_INCOMING_CONNECTION\n");
				OnIncomingConnection(packet);
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
				// It's a client, so just show the message
				printf("%s\n", packet->data);
				break;
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
		if (g_networkState == NS_CreateSocket)
		{
			if (isServer)
			{
				//opening up the server socket
				RakNet::SocketDescriptor socketDescriptors[1];
				socketDescriptors[0].port = SERVER_PORT;
				socketDescriptors[0].socketFamily = AF_INET; // Test out IPV4
				bool isSuccess = g_rakPeerInterface->Startup(MAX_CONNECTIONS, socketDescriptors, 1) == RakNet::RAKNET_STARTED;
				assert(isSuccess);
				g_rakPeerInterface->SetMaximumIncomingConnections(MAX_CONNECTIONS);
				g_networkState = NS_PendingConnection;
				std::cout << "Server is waiting on connection" << std::endl;
			}
			else
			{
				//while(RakNet::IRNS2_Berkley::IsPortInUse())
				//creates server
				RakNet::SocketDescriptor socketDescriptor(CLIENT_PORT, nullptr);
				socketDescriptor.socketFamily = AF_INET;
				g_rakPeerInterface->Startup(8, &socketDescriptor, 1);

				//client connection
				RakNet::ConnectionAttemptResult car = g_rakPeerInterface->Connect("10.4.89.129", SERVER_PORT, nullptr, 0);//127.0.0.1
				RakAssert(car == RakNet::CONNECTION_ATTEMPT_STARTED);
				std::cout << "Client attempted connection" << std::endl;
				g_networkState = NS_PendingConnection;
			}
		}
	}


	packetHandler.join();
	inputHandler.join();

	return 0;
}