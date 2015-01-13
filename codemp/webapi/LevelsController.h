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

	// GET /levels
	void GetAll()
	{
		// Get a list of all the levels (.bsp files)
	}
};

#endif //_WEBAPI_LEVELSCONTROLLER_H