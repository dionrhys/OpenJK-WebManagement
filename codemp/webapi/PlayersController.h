#ifndef _WEBAPI_PLAYERSCONTROLLER_H
#define _WEBAPI_PLAYERSCONTROLLER_H

#include "WebAPIRequest.h"
#include "libfcgi/fcgiapp.h"
#include "server/server.h"
#include "json/json.h"

class PlayersController
{
public:
	PlayersController(WebAPIRequest& request)
		: mRequest(request)
	{
	}

	void Execute()
	{
		// The server must be running to interact with any player resources
		if (!com_sv_running->integer) {
			mRequest.NotFound("Server is not running.");
			return;
		}

		// Handle /players paths
		if (mRequest.path.size() == 1)
		{
			if (mRequest.method == "GET")
			{
				GetAll();
			}
			else
			{
				mRequest.MethodNotAllowed();
			}
			return;
		}
		else if (mRequest.path.size() >= 2)
		{
			int playerID;
			bool parsed = StringToInt(mRequest.path[1], playerID);
			if (!parsed)
			{
				mRequest.BadRequest("The player ID is not valid.");
				return;
			}

			// Handle /players/:playerID paths
			if (mRequest.path.size() == 2)
			{
				if (mRequest.method == "GET")
				{
					Get(playerID);
					return;
				}
				else
				{
					mRequest.MethodNotAllowed();
					return;
				}
			}

			// Handle /players/:playerID/<action> paths
			if (mRequest.path.size() == 3)
			{
				if (mRequest.path[2] == "message")
				{
					if (mRequest.method == "POST")
					{
						PostMessage(playerID);
						return;
					}
					else
					{
						mRequest.MethodNotAllowed();
						return;
					}
				}
				else if (mRequest.path[2] == "kick")
				{
					if (mRequest.method == "POST")
					{
						PostKick(playerID);
						return;
					}
					else
					{
						mRequest.MethodNotAllowed();
						return;
					}
				}
				else if (mRequest.path[2] == "ban")
				{
					if (mRequest.method == "POST")
					{
						PostBan(playerID);
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

	// GET /players
	void GetAll()
	{
		Json::Value players = Json::Value(Json::arrayValue);

		for (int i = 0; i < sv_maxclients->integer; i++)
		{
			Json::Value player;
			if (!CreatePlayerValue(i, player))
			{
				continue;
			}

			players.append(player);
		}

		mRequest.OK(players);
	}

	// GET /players/:playerID
	void Get(int playerID)
	{
		Json::Value player;
		if (playerID < 0 || playerID >= sv_maxclients->integer || !CreatePlayerValue(playerID, player))
		{
			mRequest.NotFound("Player not found.");
			return;
		}

		mRequest.OK(player);
	}

	// POST /players/:playerID/message
	void PostMessage(int playerID)
	{
		if (playerID < 0 || playerID >= sv_maxclients->integer || !svs.clients[playerID].state)
		{
			mRequest.NotFound("Player not found.");
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

		Cbuf_AddText(va("svtell %d \"%s\"\n", playerID, message));

		mRequest.NoContent();
	}

	// POST /players/:playerID/kick
	void PostKick(int playerID)
	{
		if (playerID < 0 || playerID >= sv_maxclients->integer || !svs.clients[playerID].state)
		{
			mRequest.NotFound("Player not found.");
			return;
		}

		Cbuf_AddText(va("clientkick %d\n", playerID));

		mRequest.NoContent();
	}

	// POST /players/:playerID/ban
	void PostBan(int playerID)
	{
		if (playerID < 0 || playerID >= sv_maxclients->integer || !svs.clients[playerID].state)
		{
			mRequest.NotFound("Player not found.");
			return;
		}

		Cbuf_AddText(va("sv_banaddr %d\n", playerID));

		mRequest.NoContent();
	}

	static bool CreatePlayerValue(int clientNum, Json::Value& out)
	{
		if (!svs.clients[clientNum].state)
		{
			return false;
		}

		client_t *cl = &svs.clients[clientNum];
		playerState_t *ps = SV_GameClientNum(clientNum);

		out = Json::Value(Json::objectValue);
		out["id"] = std::to_string(clientNum);
		out["name"] = cl->name;
		out["playingTime"] = (svs.time - cl->lastConnectTime) / 1000.0f;
		out["isBot"] = (cl->netchan.remoteAddress.type == NA_BOT);
		out["isLocal"] = (cl->netchan.remoteAddress.type == NA_LOOPBACK);
		out["score"] = ps->persistant[PERS_SCORE];

		return true;
	}
};

#endif //_WEBAPI_PLAYERSCONTROLLER_H