#include "webapi.h"
#include "utils.h"
#include "WebAPIRequest.h"
#include "PlayersController.h"

#include "qcommon/qcommon.h"
#include "libfcgi/fcgiapp.h"

// The accepting thread will signal acceptedRequestEvent when it has a request that it needs the main thread to handle
// The main thread will signal finishedRequestEvent after it has finished handling the request
// (both events are auto-reset events)
static HANDLE acceptedRequestEvent;
static HANDLE finishedRequestEvent;

// Handle to the thread used for accepting FastCGI requests
static HANDLE acceptingThreadHandle;

// Specifies whether the web API is initialized or not
static bool webapiInitialized = false;

// The shared FastCGI request object (access must be synchronized)
static FCGX_Request webapiRequest;

static DWORD WINAPI _WebAPI_AcceptingThreadProc(LPVOID);
static void WebAPI_AcceptingThread();
static void WebAPI_HandleRequest(FCGX_Request& request);

///
/// Initialize and start the FastCGI server to accept API requests.
///
void WebAPI_Init()
{
	if (!com_dedicated->integer || webapiInitialized)
	{
		return;
	}

	Com_Printf("Starting Web API: *:9000\n");

	if (FCGX_Init() != 0)
	{
		Com_Printf("- Unable to initialize the FCGX library\n");
		return;
	}

	int socket = FCGX_OpenSocket(":9000", 500);
	if (socket < 0)
	{
		Com_Printf("- Unable to open the socket\n");
		return;
	}

	FCGX_InitRequest(&webapiRequest, 0, 0);

	acceptedRequestEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	finishedRequestEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	acceptingThreadHandle = CreateThread(NULL, 0, _WebAPI_AcceptingThreadProc, NULL, 0, NULL);

	webapiInitialized = true;
}

///
/// Shutdown the FastCGI server.
///
void WebAPI_Shutdown()
{
	if (!webapiInitialized)
	{
		return;
	}

	Com_Printf("Stopping Web API\n");

	FCGX_ShutdownPending(); // Signal FastCGI to shutdown (FCGX_Accept checks for shutdown every 1 second)

	SetEvent(finishedRequestEvent); // Set this event just in case the accepting thread is in the middle of a request (otherwise we'd have deadlock)
	WaitForSingleObject(acceptingThreadHandle, INFINITE); // The thread will end when FCGX_Accept returns an error code due to the shutdown request

	// The accepting thread has finished so it's safe to close all these handles now
	CloseHandle(acceptingThreadHandle);
	CloseHandle(finishedRequestEvent);
	CloseHandle(acceptedRequestEvent);

	webapiInitialized = false;
}

///
/// Poll the accepting thread to see if there's a new request to handle.
///
void WebAPI_Frame()
{
	if (!webapiInitialized)
	{
		return;
	}

	// Ensure the accepting thread is still running
	int result = WaitForSingleObject(acceptingThreadHandle, 0);
	if (result == WAIT_OBJECT_0)
	{
		Com_Printf("Web API error: accepting thread exited unexpectedly\n");
		WebAPI_Shutdown();
		return;
	}

	// Check if the accepting thread has accepted a request (no timeout, returns immediately)
	result = WaitForSingleObject(acceptedRequestEvent, 0);
	if (result == WAIT_OBJECT_0)
	{
		WebAPI_HandleRequest(webapiRequest);

		// Let the accepting thread resume
		SetEvent(finishedRequestEvent);
	}
}

///
/// WIN32 wrapper for the WebAPI_AcceptingThread function.
///
static DWORD WINAPI _WebAPI_AcceptingThreadProc(LPVOID)
{
	WebAPI_AcceptingThread();
	return 1;
}

///
/// Continuously accept FastCGI requests until the library is shutdown.
///
static void WebAPI_AcceptingThread()
{
	int result;

	while ((result = FCGX_Accept_r(&webapiRequest)) == 0)
	{
		// Raise the signal for main thread to handle the request, then wait until it has finished with it
		SetEvent(acceptedRequestEvent);
		WaitForSingleObject(finishedRequestEvent, INFINITE);
	}

	FCGX_Finish_r(&webapiRequest);
}

static void WebAPI_HandleRequest(FCGX_Request& request)
{
	// Ensure required parameters are given
	const char *requestMethod = FCGX_GetParam("REQUEST_METHOD", request.envp);
	if (requestMethod == NULL)
	{
		FCGX_FPrintF(request.err, "Missing REQUEST_METHOD parameter");
		return;
	}

	const char *pathInfo = FCGX_GetParam("PATH_INFO", request.envp);
	if (pathInfo == NULL)
	{
		FCGX_FPrintF(request.err, "Missing PATH_INFO parameter");
		return;
	}

	const char *queryString = FCGX_GetParam("QUERY_STRING", request.envp);
	if (queryString == NULL)
	{
		FCGX_FPrintF(request.err, "Missing QUERY_STRING parameter");
		return;
	}

	const char *remoteAddr = FCGX_GetParam("REMOTE_ADDR", request.envp);
	if (remoteAddr == NULL)
	{
		FCGX_FPrintF(request.err, "Missing REMOTE_ADDR parameter");
		return;
	}

	const char *remotePort = FCGX_GetParam("REMOTE_PORT", request.envp);
	if (remotePort == NULL)
	{
		FCGX_FPrintF(request.err, "Missing REMOTE_PORT parameter");
		return;
	}

	// Ensure globally valid REQUEST_METHOD (GET, HEAD, POST, PUT, DELETE)
	if (strcmp(requestMethod, "GET") &&
		strcmp(requestMethod, "HEAD") &&
		strcmp(requestMethod, "POST") &&
		strcmp(requestMethod, "PUT") &&
		strcmp(requestMethod, "DELETE"))
	{
		FCGX_FPrintF(request.out, "Status: 501 Not Implemented\r\n\r\n");
		return;
	}

	Com_Printf("Web API request %s:%s : %s %s %s\n", remoteAddr, remotePort, requestMethod, pathInfo, queryString);

	// Parse/validate path segments from PATH_INFO
	std::vector<std::string> path;
	try
	{
		ParsePathInfo(pathInfo, path);
	}
	catch (std::exception& ex)
	{
		FCGX_FPrintF(request.err, "ParsePathInfo: %s", ex.what());
		FCGX_FPrintF(request.out, "Status: 400 Bad Request\r\n\r\n");
		return;
	}

	// Parse/validate key/value parameters from QUERY_STRING
	std::map<std::string, std::string> query;
	try
	{
		ParseQueryString(queryString, query);
	}
	catch (std::exception& ex)
	{
		FCGX_FPrintF(request.err, "ParseQueryString: %s", ex.what());
		FCGX_FPrintF(request.out, "Status: 400 Bad Request\r\n\r\n");
		return;
	}

	// TODO: Authentication/Authorization

	// TODO: Locate appropriate resource controller to handle the request
	WebAPIRequest newRequest = WebAPIRequest(request, path, query);
	if (path.size() >= 1 && path[0] == "players")
	{
		PlayersController controller = PlayersController(newRequest);
		controller.Execute();
		return;
	}

	//FCGX_FPrintF(request.out, "Content-Type: text/plain\r\n\r\nIt works!\ncom_version: %s\ncom_frameTime: %d", com_version->string, com_frameTime);
	FCGX_FPrintF(request.out, "Content-Type: application/json\r\n\r\n{\n  \"conclusion\": \"It works!\",\n  \"version\": \"%s\",\n  \"frameTime\": %d\n}", com_version->string, com_frameTime);
}