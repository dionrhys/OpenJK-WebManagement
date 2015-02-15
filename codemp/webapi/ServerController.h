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
				else if (mRequest.path[1] == "broadcast")
				{
					if (mRequest.method == "POST")
					{
						PostBroadcast();
						return;
					}
					else
					{
						mRequest.MethodNotAllowed();
						return;
					}
				}
				else if (mRequest.path[1] == "level")
				{
					if (mRequest.method == "POST")
					{
						PostLevel();
						return;
					}
					else
					{
						mRequest.MethodNotAllowed();
						return;
					}
				}
				else if (mRequest.path[1] == "gamemode")
				{
					if (mRequest.method == "POST")
					{
						PostGamemode();
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
		server["uptime"] = svs.time / 1000.0f;
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

	// POST /server/broadcast
	void PostBroadcast()
	{
		if (!com_sv_running->integer) {
			mRequest.NotFound("Server is not running.");
			return;
		}

		Json::Value input;
		char data[1024];
		int len = FCGX_GetStr(data, sizeof(data), mRequest.fcgxRequest.in);
		if (len >= sizeof(data))
		{
			mRequest.BadRequest("Request content is too large.");
			return;
		}

		Json::Reader reader = Json::Reader(Json::Features::strictMode());
		bool success = reader.parse(data, data + len, input);
		if (!success)
		{
			mRequest.BadRequest("Unable to parse the request content.");
			return;
		}

		const Json::Value& value = input["message"];
		if (!value.isString())
		{
			mRequest.BadRequest("You must provide a string 'message' field in the request content.");
			return;
		}
		const char* message = value.asCString();

		// Ensure potentially-dangerous characters can't be injected into the command buffer
		if (strchr(message, '\n') || strchr(message, '\r') || strchr(message, ';') || strchr(message, '\"') || strchr(message, '/') || strchr(message, '*'))
		{
			mRequest.BadRequest("Invalid characters in the 'message' field.");
			return;
		}

		Cbuf_AddText(va("svsay \"%s\"\n", message));

		mRequest.NoContent();
	}

	// POST /server/level
	void PostLevel()
	{
		Json::Value input;
		char data[1024];
		int len = FCGX_GetStr(data, sizeof(data), mRequest.fcgxRequest.in);
		if (len >= sizeof(data))
		{
			mRequest.BadRequest("Request content is too large.");
			return;
		}

		Json::Reader reader = Json::Reader(Json::Features::strictMode());
		bool success = reader.parse(data, data + len, input);
		if (!success)
		{
			mRequest.BadRequest("Unable to parse the request content.");
			return;
		}

		const Json::Value& value = input["level"];
		if (!value.isString())
		{
			mRequest.BadRequest("You must provide a string 'level' field in the request content.");
			return;
		}
		const char* level = value.asCString();

		// Ensure potentially-dangerous characters can't be injected into the command buffer
		if (strchr(level, '\n') || strchr(level, '\r') || strchr(level, ';') || strchr(level, '\"') || strchr(level, '*') || strchr(level, ':'))
		{
			mRequest.BadRequest("Invalid characters in the 'level' field.");
			return;
		}

		Cbuf_AddText(va("map \"%s\"\n", level));

		mRequest.NoContent();
	}

	// POST /server/gamemode
	void PostGamemode()
	{
		Json::Value input;
		char data[1024];
		int len = FCGX_GetStr(data, sizeof(data), mRequest.fcgxRequest.in);
		if (len >= sizeof(data))
		{
			mRequest.BadRequest("Request content is too large.");
			return;
		}

		Json::Reader reader = Json::Reader(Json::Features::strictMode());
		bool success = reader.parse(data, data + len, input);
		if (!success)
		{
			mRequest.BadRequest("Unable to parse the request content.");
			return;
		}

		const Json::Value& value = input["gameMode"];
		if (!value.isString())
		{
			mRequest.BadRequest("You must provide a string 'gameMode' field in the request content.");
			return;
		}
		int gametype = 0;

		// Ensure gametype is valid
		if (!StringToInt(value.asString(), gametype) || gametype < 0 || gametype > GT_MAX_GAME_TYPE)
		{
			mRequest.NotFound("Invalid game mode chosen.");
			return;
		}

		Cbuf_AddText(va("set g_gametype %d\nmap %s\n", gametype, Cvar_VariableString("mapname")));

		mRequest.NoContent();
	}
};

#endif //_WEBAPI_SERVERCONTROLLER_H