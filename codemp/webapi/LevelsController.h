#ifndef _WEBAPI_LEVELSCONTROLLER_H
#define _WEBAPI_LEVELSCONTROLLER_H

#include "WebAPIRequest.h"
#include "utils.h"
#include "libfcgi/fcgiapp.h"
#include "server/server.h"
#include "json/json.h"

class LevelsController
{
public:
	LevelsController(WebAPIRequest& request)
		: mRequest(request)
	{
	}

	void Execute()
	{
		// Handle /levels paths
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

		// Fallback if no function could handle the request
		mRequest.NotFound();
	}

private:
	WebAPIRequest& mRequest;

	static Json::Value levelArray;

	static void AppendLevel(const char *s)
	{
		Json::Value lvl = Json::Value(Json::objectValue);
		lvl["name"] = s;
		levelArray.append(lvl);
	}

	// GET /levels
	void GetAll()
	{
		// Get a list of all the levels (.bsp files)
		levelArray = Json::Value(Json::arrayValue);
		FS_FilenameCompletion("maps", "bsp", qtrue, AppendLevel, qfalse);

		mRequest.OK(levelArray);
	}
};

Json::Value LevelsController::levelArray;

#endif //_WEBAPI_LEVELSCONTROLLER_H