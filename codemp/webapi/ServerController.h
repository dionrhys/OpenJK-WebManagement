#ifndef _WEBAPI_SERVERCONTROLLER_H
#define _WEBAPI_SERVERCONTROLLER_H

#include "WebAPIRequest.h"
#include "utils.h"
#include "libfcgi/fcgiapp.h"
#include "qcommon/game_version.h"
#include "server/server.h"
#include "json/json.h"

static const char *GetGametypeString(int gametype)
{
	switch (gametype)
	{
	case GT_FFA:
		return "Free For All";
	case GT_HOLOCRON:
		return "Holocron";
	case GT_JEDIMASTER:
		return "Jedi Master";
	case GT_DUEL:
		return "Duel";
	case GT_POWERDUEL:
		return "Power Duel";
	case GT_SINGLE_PLAYER:
		return "Cooperative";

	case GT_TEAM:
		return "Team Deathmatch";
	case GT_SIEGE:
		return "Siege";
	case GT_CTF:
		return "Capture The Flag";
	case GT_CTY:
		return "Capture The Ysalimiri";

	default:
		return "Unknown Gametype";
	}
}

class ServerController
{
public:
	ServerController(WebAPIRequest& request)
		: mRequest(request)
	{
	}

	void Execute()
	{
		// Handle /server paths
		if (mRequest.path.size() == 1)
		{
			if (mRequest.method == "GET")
			{
				Get();
				return;
			}
			else
			{
				mRequest.MethodNotAllowed();
				return;
			}
		}
		else if (mRequest.path.size() >= 2)
		{
			// Handle /server/<action> paths
			if (mRequest.path.size() == 2)
			{
				if (mRequest.path[1] == "restart")
				{
					if (mRequest.method == "POST")
					{
						PostRestart();
						return;
					}
					else
					{
						mRequest.MethodNotAllowed();
						return;
					}
				}
				else if (mRequest.path[1] == "shutdown")
				{
					if (mRequest.method == "POST")
					{
						PostShutdown();
						return;
					}
					else
					{
						mRequest.MethodNotAllowed();
						return;
					}
				}
			}
		}

		// Fallback if no function could handle the request
		mRequest.NotFound();
	}

private:
	WebAPIRequest& mRequest;

	// GET /server
	void Get()
	{
		// Calculate number of players
		int numPlayers = 0;
		if (com_sv_running->integer) {
			for (int i = 0; i < sv_maxclients->integer; i++) {
				if (svs.clients[i].state >= CS_CONNECTED) {
					numPlayers++;
				}
			}
		}

		Json::Value server = Json::Value(Json::objectValue);
		server["state"] = com_sv_running->integer ? "online" : "offline";
		server["name"] = sv_hostname->string;
		server["maxPlayers"] = sv_maxclients->integer;
		server["numPlayers"] = numPlayers;
		server["gameMode"] = GetGametypeString(sv_gametype->integer);
		server["uptime"] = 9999; // TODO
		server["address"] = va("%s:%i", Cvar_VariableString("net_ip"), Cvar_VariableIntegerValue("net_port"));
		server["game"] = "Star Wars Jedi Knight: Jedi Academy";
		server["version"] = JK_VERSION;
		server["platform"] = PLATFORM_STRING;

		// Fields that only exist when the server is actually running
		if (com_sv_running->integer) {
			server["mapName"] = sv_mapname->string;
		}

		mRequest.OK(server);
	}

	// POST /server/restart
	void PostRestart()
	{
		// Server must be running to restart the map (else we wouldn't know what map to load)
		if (!com_sv_running->integer)
		{
			mRequest.NotFound("Server is not running.");
			return;
		}

		// Load the current map again to restart the game
		Cbuf_AddText(va("map %s\n", Cvar_VariableString("mapname")));

		mRequest.NoContent();
	}

	// POST /server/shutdown
	void PostShutdown()
	{
		if (com_sv_running->integer)
		{
			Cbuf_AddText("killserver\n");
		}

		mRequest.NoContent();
	}
};

#endif //_WEBAPI_SERVERCONTROLLER_H