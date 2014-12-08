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
			Json::Value response = Json::Value(Json::objectValue);
			response["message"] = "Server is not running.";
			mRequest.NotFound(response);
			return;
		}

		// TODO: Check method
		if (mRequest.path.size() < 2)
		{
			GetAll();
		}
		else
		{
			Get();
		}
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

	// GET /players/{player ID}
	void Get()
	{
		int playerID;
		bool parsed = StringToInt(mRequest.path[1], playerID);
		if (!parsed)
		{
			FCGX_FPrintF(mRequest.fcgxRequest.out, "Status: 400 Bad Request\r\n\r\n");
			return;
		}

		Json::Value player;
		if (playerID < 0 || playerID >= sv_maxclients->integer || !CreatePlayerValue(playerID, player))
		{
			Json::Value response = Json::Value(Json::objectValue);
			response["message"] = "Player not found.";
			mRequest.NotFound(response);
			return;
		}

		mRequest.OK(player);
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