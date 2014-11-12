#ifndef _WEBAPI_UTILS_H
#define _WEBAPI_UTILS_H

#include <map>
#include <string>
#include <vector>

void ParsePathInfo(const char* pathInfo, std::vector<std::string>& path);
void ParseQueryString(const char* queryString, std::map<std::string, std::string>& query);

#endif //_WEBAPI_UTILS_H