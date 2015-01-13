#ifndef _WEBAPI_CONSOLECONTROLLER_H
#define _WEBAPI_CONSOLECONTROLLER_H

#include "WebAPIRequest.h"
#include "utils.h"
#include "libfcgi/fcgiapp.h"
#include "server/server.h"
#include "json/json.h"

// Prototype from webapi.cpp
const std::string& WebAPI_GetConsoleBuffer();

class ConsoleController
{
public:
	ConsoleController(WebAPIRequest& request)
		: mRequest(request)
	{
	}

	void Execute()
	{
		// Handle /console paths
		if (mRequest.path.size() == 1)
		{
			if (mRequest.method == "GET")
			{
				Get();
				return;
			}
			else if (mRequest.method == "POST")
			{
				Post();
				return;
			}
			else
			{
				mRequest.MethodNotAllowed();
				return;
			}
		}

		// Fallback if no function could handle the request
		mRequest.NotFound();
	}

private:
	WebAPIRequest& mRequest;

	// GET /console
	void Get()
	{
		// Return console data
		const std::string& buf = WebAPI_GetConsoleBuffer();
		Json::Value console = Json::Value(Json::objectValue);
		console["text"] = buf;
		mRequest.OK(console);
	}

	// POST /console
	void Post()
	{
		// Execute command string
	}
};

#endif //_WEBAPI_CONSOLECONTROLLER_H