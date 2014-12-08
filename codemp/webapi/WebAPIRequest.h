#ifndef _WEBAPI_WEBAPIREQUEST_H
#define _WEBAPI_WEBAPIREQUEST_H

#include <map>
#include <string>
#include <vector>

#include "libfcgi/fcgiapp.h"
#include "json/json.h"

class WebAPIRequest
{
public:
	WebAPIRequest(FCGX_Request& fcgxRequest, std::vector<std::string>& path, std::map<std::string, std::string>& query)
		: fcgxRequest(fcgxRequest), path(path), query(query)
	{
	}

	FCGX_Request& fcgxRequest;
	std::vector<std::string>& path;
	std::map<std::string, std::string>& query;

	void NotFound(const Json::Value& json)
	{
		FCGX_FPrintF(fcgxRequest.out,
			"Status: 404 Not Found\r\n"
			"Content-Type: application/json\r\n"
			"\r\n"
			"%s", json.toStyledString().c_str());
	}

	void OK(const Json::Value& json)
	{
		FCGX_FPrintF(fcgxRequest.out,
			"Content-Type: application/json\r\n"
			"\r\n"
			"%s", json.toStyledString().c_str());
	}
};

#endif //_WEBAPI_WEBAPIREQUEST_H