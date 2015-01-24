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

		const Json::Value& value = input["command"];
		if (!value.isString())
		{
			mRequest.BadRequest("You must provide a string 'command' field in the request content.");
			return;
		}
		const char* command = value.asCString();

		// No filtering for malicious command buffer injection is done because the whole point
		// of this method is to inject text into the command buffer
		Cbuf_AddText(va("%s\n", command));

		mRequest.NoContent();
	}
};

#endif //_WEBAPI_CONSOLECONTROLLER_H