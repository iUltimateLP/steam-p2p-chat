// Written by Jonathan 'Johnny' Verbeek - 2016

#include <iostream>
#include <windows.h>
#include <sstream>
#include "steam_api.h"
#include <vector>

// Simple macro to check if a key is down (http://stackoverflow.com/a/8468595)
#define IsKeyDown(k) (GetAsyncKeyState(k) & 0x8000) != 0

// Macro to clear the console input buffer
#define ClearInputBuffer() FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE))

// Macro to check whether the window is in foreground or not
#define IsConsoleFocussed() GetConsoleWindow() == GetForegroundWindow()

namespace
{
	// Custom function to print something with a specified color
	void printf_color(WORD color, const char* Format, ...)
	{
		va_list ap;

		CONSOLE_SCREEN_BUFFER_INFO csbi;
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);

		va_start(ap, Format);
		_vfprintf_l(stdout, Format, NULL, ap);

		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), csbi.wAttributes);

		va_end(ap);
	}
}

// Global class (we need one for Steam callbacks to work)
class CSteamP2PChat
{
public:
	// Main function (contains everything)
	int main();

	// Steam callback for allowing packages from an unkown steam id
	STEAM_CALLBACK(CSteamP2PChat, OnPacketFromUnknownSource, P2PSessionRequest_t);

	// Steam callback for errors in the P2P transfer
	STEAM_CALLBACK(CSteamP2PChat, OnSessionConnectFail, P2PSessionConnectFail_t);

	// A vector to store steam ids
	std::vector<uint64> cachedSteamIds;
};

// Main function implementation
int CSteamP2PChat::main()
{
	// Try to initialize Steam API
	if (!SteamAPI_Init())
	{
		printf("Steam API init failure! Press any key to quit...\n");
		getchar();
		return 1;
	}

	system("cls");

	// An enumeration for our hotkeys
	enum
	{
		HK_LIST = 0x4C, // L
		HK_WRITE = 0x43,// C
		HK_EXIT = 0x1B  // Escape
	};

	// Welcome message
	printf("\nSteam Peer-To-Peer chat example by iUltimateLP.\n");
	printf_color(0x08, " - Press 'L' to list your friends and their Steam IDs.\n - Press 'C' to write a message.\n - Press 'Escape' to close.\n");
	printf("Your SteamID: %I64u\n\n", SteamUser()->GetSteamID().ConvertToUint64());

	// Initialize the cache with the total amount of friends
	cachedSteamIds = std::vector<uint64>(SteamFriends()->GetFriendCount(k_EFriendFlagAll));

	// Main loop
	bool keyPressed = false;
	while (true)
	{
		// Branch if the keys are pressed and we don't hold down a key (!keyPressed)
		// Also check if the console is in foreground, because otherwise we would trigger the menus by pressing the keys in another application
		if (IsKeyDown(HK_LIST) && !keyPressed && IsConsoleFocussed())
		{
			keyPressed = true;

			// Clear the input buffer so this key won't show at a prompt
			ClearInputBuffer();

			// Get the total amount of friends
			int friendCount = SteamFriends()->GetFriendCount(k_EFriendFlagAll);

			// Clear the steam id cache
			cachedSteamIds.empty();

			printf_color(0x08, "===========================================================\n    Listing your online steam friends:\n");
			// Iterate through the online friends
			for (int i = 0; i <= friendCount; i++)
			{
				// Save the current friend in a CSteamID variable, and print out it's id (uint64) and it's Persona name
				CSteamID curFriend = SteamFriends()->GetFriendByIndex(i, k_EFriendFlagAll);

				// Only list friends who are not offline (e.g. online or away)
				if (SteamFriends()->GetFriendPersonaState(curFriend) != k_EPersonaStateOffline)
				{
					printf_color(0x08, "      [%d] %I64u - %s\n", i, curFriend.ConvertToUint64(), SteamFriends()->GetFriendPersonaName(curFriend));
					// Cache the steam ID so we can access it later on
					cachedSteamIds.insert(cachedSteamIds.begin()+i, curFriend.ConvertToUint64());
				}
			}
			printf_color(0x08, "===========================================================\n\n");
		}
		else if (IsKeyDown(HK_WRITE) && !keyPressed && IsConsoleFocussed())
		{
			keyPressed = true;

			// Clear the input buffer so this key won't show at a prompt
			ClearInputBuffer();

			// Create variables for the message information
			std::string strTarget;
			std::string strMessage;
			CSteamID targetId;

			// Prompt for the target ID and the message
			printf_color(0x0e, "    Steam ID, list index of the target or me: ");
			getline(std::cin, strTarget);
			printf_color(0x0e, "    Message to send                         : ");
			getline(std::cin, strMessage);

			// If the input id is below 17 characters long (thats how long a steam id is), we assume its a cache index
			if (strTarget == "me")
			{
				targetId = SteamUser()->GetSteamID();
			}
			else if (strTarget.length() < 17)
			{
				targetId.SetFromUint64(cachedSteamIds[atoi(strTarget.c_str())]);
			}
			else
			{ 
				// Parse the given string into an uint64 so we can create a Steam ID from that
				//    see http://www.cplusplus.com/forum/unices/80703/
				uint64 id;
				std::istringstream(strTarget) >> id;
				targetId.SetFromUint64(id);
			}
			printf("      => Sending '%s' to '%s' (%I64u)\n", strMessage.c_str(), SteamFriends()->GetFriendPersonaName(targetId), targetId.ConvertToUint64());

			// Try to send the packet, where the message size is the length of our string, and we choose to send a reliable packet
			if (SteamNetworking()->SendP2PPacket(targetId, strMessage.c_str(), strMessage.length(), k_EP2PSendReliable))
			{
				printf("      => Sent.\n\n");
			}
			else
			{
				printf("      => For some reason not sent.\n\n");
			}
		}
		else if (IsKeyDown(HK_EXIT) && !keyPressed && IsConsoleFocussed())
		{
			printf("Quitting..\n");
			// Shutdown Steam's API
			SteamAPI_Shutdown();
			return 0;
		}

		// If none of the keys is pressed and our blocking variable is true, set it to false
		while (!IsKeyDown(HK_LIST) && !IsKeyDown(HK_WRITE) && !IsKeyDown(HK_EXIT) && keyPressed)
		{
			keyPressed = false;
		}

		// Check if a peer-to-peer packet from Steam is available
		uint32 msgSize = 0;
		while (SteamNetworking()->IsP2PPacketAvailable(&msgSize))
		{
			// Allocate memory for the size of the received message
			void *p = malloc(msgSize);
			CSteamID senderID;
			uint32 bytesRead = 0;
			// Read the packet and dispatch it once reading it was successful
			if (SteamNetworking()->ReadP2PPacket(p, msgSize, &bytesRead, &senderID))
			{
				// Convert the packet of type void* to a string
				std::string strMsg = (const char*)p;
				// Resize the string to the transmitted msgSize since it has some unnecessary bytes at the end
				strMsg.resize(msgSize);

				// Display the message together with the persona name of the sender
				printf_color(0x0a, "[%s] %s\n\n", SteamFriends()->GetFriendPersonaName(senderID), strMsg.c_str());
			}
			// Free the memory allocated for that packet again
			free(p);
		}

		// Give the program some time so we won't cast high performance on the CPU
		Sleep(10);
	}

	return 0;
}

// Steam's callback implementation for receiving packets from an unkown source
void CSteamP2PChat::OnPacketFromUnknownSource(P2PSessionRequest_t* pParam)
{
	printf("Received a package from an unkown source (steam id %I64u). This is because it's your first time!\n", pParam->m_steamIDRemote.ConvertToUint64());
	// Allow the packet and the connection in general of that steam id
	if (SteamNetworking()->AcceptP2PSessionWithUser(pParam->m_steamIDRemote))
	{
		printf("Allowed that packet to pass through!\n");
	}
	else
	{
		printf("Error allowing that package. wtf dude\n");
	}
}

// Steam's callback implementation for errors while connecting
void CSteamP2PChat::OnSessionConnectFail(P2PSessionConnectFail_t* pParam)
{
	// Switch on the error, and print a error message
	switch (pParam->m_eP2PSessionError)
	{
	case k_EP2PSessionErrorNotRunningApp:
		printf("Error! The target is not running the same app like you!\n");
	case k_EP2PSessionErrorNoRightsToApp:
		printf("Error! You don't own this app!\n");
	case k_EP2PSessionErrorDestinationNotLoggedIn:
		printf("Error! The remote user is not connected to Steam!\n");
	case k_EP2PSessionErrorTimeout:
		printf("Error! The connection timed out.\n");
	default:
		printf("Some error happend!\n");
	}
}

// Application's main function
int main()
{
	// Just create a new object of our class, and run the main function
	CSteamP2PChat obj = CSteamP2PChat();
	return obj.main();
}