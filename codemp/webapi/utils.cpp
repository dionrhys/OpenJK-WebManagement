#include "utils.h"

#include <map>
#include <string>
#include <vector>

static const char validSegmentChars[] = {
	':', '@', '-', '.', '_', '~', '%', '!', '$',
	'&', '\'', '(', ')', '*', '+', ',', ';', '='
};

static bool IsValidSegmentChar(char c)
{
	if ((c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9'))
		return true;

	for (size_t i = 0; i < sizeof(validSegmentChars) / sizeof(validSegmentChars[0]); i++)
	{
		if (c == validSegmentChars[i])
			return true;
	}

	return false;
}

/// <summary>
/// </summary>
void ParsePathInfo(const char* pathInfo, std::vector<std::string>& path)
{
	// absolute-path = 1*( "/" segment )
	// segment       = *pchar
	// pchar         = unreserved / pct-encoded / sub-delims / ":" / "@"
	//
	// unreserved    = ALPHA / DIGIT / "-" / "." / "_" / "~"
	// pct-encoded   = "%" HEXDIG HEXDIG
	// sub-delims    = "!" / "$" / "&" / "'" / "(" / ")"
	//               / "*" / "+" / "," / ";" / "="

	if (pathInfo == NULL) throw std::invalid_argument("pathInfo argument is NULL");

	path.clear();

	if (*pathInfo != '/') throw std::runtime_error("Invalid path; Path doesn't start with '/'");

	const char* pStart = pathInfo + 1;
	const char* pSeek = pStart;

	while (true)
	{
		if (*pSeek == '\0' || *pSeek == '/') {
			// reached end of segment
			ptrdiff_t segmentLen = pSeek - pStart;

			// discard empty segments
			if (segmentLen > 0)
			{
				// add segment to path vector
				std::string segment = std::string(pStart, segmentLen);
				path.push_back(segment);
			}

			if (*pSeek == '\0') {
				// reached end of entire path
				break;
			}

			// still more to go, start work on next segment
			pSeek++;
			pStart = pSeek;
			continue;
		}

		// ensure character is valid for a segment
		if (!IsValidSegmentChar(*pSeek)) {
			throw std::runtime_error("Invalid path; Invalid character found in segment");
		}

		pSeek++;
	}
}

static const char validQueryChars[] = {
	'/', '?', ':', '@', '-', '.', '_', '~', '%', '!',
	'$', '&', '\'', '(', ')', '*', '+', ',', ';', '='
};

static bool IsValidQueryChar(char c)
{
	if ((c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9'))
		return true;

	for (size_t i = 0; i < sizeof(validQueryChars) / sizeof(validQueryChars[0]); i++)
	{
		if (c == validQueryChars[i])
			return true;
	}

	return false;
}

void ParseQueryString(const char* queryString, std::map<std::string, std::string>& query)
{
	// query       = *( pchar / "/" / "?" )
	// pchar         = unreserved / pct-encoded / sub-delims / ":" / "@"
	//
	// unreserved    = ALPHA / DIGIT / "-" / "." / "_" / "~"
	// pct-encoded   = "%" HEXDIG HEXDIG
	// sub-delims    = "!" / "$" / "&" / "'" / "(" / ")"
	//               / "*" / "+" / "," / ";" / "="

	if (queryString == NULL) throw std::invalid_argument("queryString argument is NULL");

	query.clear();

	const char *pStart = queryString;
	const char *pSeek = pStart;

	std::string fieldName;
	std::string fieldValue;
	bool gotFieldName = false;
	while (true)
	{
		ptrdiff_t tokenLen = pSeek - pStart;

		if (!gotFieldName)
		{
			// parsing field name
			if (*pSeek == '\0')
			{
				if (tokenLen == 0)
				{
					break;
				}
				else
				{
					throw std::runtime_error("Invalid query; Reached end of string while parsing field name");
				}
			}
			else if (*pSeek == '&')
			{
				if (tokenLen == 0)
				{
					// allow repetitions of ampersands and stray ampersands at the end of the query
					pSeek++;
					pStart = pSeek;
					continue;
				}
				else
				{
					throw std::runtime_error("Invalid query; Unexpected '&' (ampersand) while parsing field name");
				}
			}
			else if (*pSeek == '=')
			{
				if (tokenLen == 0)
				{
					// don't allow empty field names
					throw std::runtime_error("Invalid query; Encountered '=' (equals sign) after empty field name");
				}
				else
				{
					// got field name now, start parsing the value next
					fieldName = std::string(pStart, tokenLen);
					gotFieldName = true;
					pSeek++;
					pStart = pSeek;
					continue;
				}
			}
		}
		else
		{
			// parsing field value
			if (*pSeek == '\0' || *pSeek == '&')
			{
				fieldValue = std::string(pStart, tokenLen);

				if (query.count(fieldName) > 0)
				{
					// don't allow duplicate parameters
					throw std::runtime_error("Invalid query; Duplicate parameter given");
				}

				query[fieldName] = fieldValue;

				if (*pSeek == '\0')
				{
					break;
				}

				// start work on next parameter
				gotFieldName = false;
				pSeek++;
				pStart = pSeek;
				continue;
			}
			else if (*pSeek == '=')
			{
				throw std::runtime_error("Invalid query; Unexpected '=' (equals sign) while parsing field value");
			}
		}

		// ensure character is valid for a query
		if (!IsValidQueryChar(*pSeek)) {
			throw std::runtime_error("Invalid query; Invalid character found in query");
		}

		pSeek++;
	}
}

//
// C++ standard library doesn't have a robust method to parse numbers...
// http://stackoverflow.com/a/6154614
//
bool StringToInt(const std::string& input, int& output)
{
	const char *str = input.c_str();
	char *end = NULL;

	long value = strtol(str, &end, 10);

	if (*str == '\0' || *end != '\0')
	{
		return false;
	}
	if ((value == LONG_MAX && errno == ERANGE) || value > INT_MAX)
	{
		return false;
	}
	if ((value == LONG_MIN && errno == ERANGE) || value < INT_MIN)
	{
		return false;
	}

	output = static_cast<int>(value);

	return true;
}