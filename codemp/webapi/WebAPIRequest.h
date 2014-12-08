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
	WebAPIRequest(FCGX_Request& fcgxRequest, const std::string& method, const std::vector<std::string>& path, const std::map<std::string, std::string>& query)
		: fcgxRequest(fcgxRequest), method(method), path(path), query(query)
	{
	}

	FCGX_Request& fcgxRequest;
	const std::vector<std::string>& path;
	const std::map<std::string, std::string>& query;
	const std::string& method;

	void BadRequest(const std::string& message)
	{
		Json::Value json = Json::Value(Json::objectValue);
		json["message"] = message;
		FCGX_FPrintF(fcgxRequest.out,
			"Status: 400 Bad Request\r\n"
			"Content-Type: application/json\r\n"
			"\r\n"
			"%s", json.toStyledString().c_str());
	}

	void MethodNotAllowed()
	{
		Json::Value json = Json::Value(Json::objectValue);
		json["message"] = "The request method is not allowed for the target URI.";
		FCGX_FPrintF(fcgxRequest.out,
			"Status: 405 Method Not Allowed\r\n"
			"Content-Type: application/json\r\n"
			"\r\n"
			"%s", json.toStyledString().c_str());
	}

	void NoContent()
	{
		FCGX_FPrintF(fcgxRequest.out,
			"Status: 204 No Content\r\n"
			"\r\n");
	}

	void NotFound()
	{
		NotFound("Target resource not found.");
	}

	void NotFound(const std::string& message)
	{
		Json::Value json = Json::Value(Json::objectValue);
		json["message"] = message;
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