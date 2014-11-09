#include "webapi.h"

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
		Com_Printf("Web API request %s:%s : %s %s %s\n",
			FCGX_GetParam("REMOTE_ADDR", webapiRequest.envp),
			FCGX_GetParam("REMOTE_PORT", webapiRequest.envp),
			FCGX_GetParam("REQUEST_METHOD", webapiRequest.envp),
			FCGX_GetParam("PATH_INFO", webapiRequest.envp),
			FCGX_GetParam("QUERY_STRING", webapiRequest.envp));
		//FCGX_FPrintF(webapiRequest.out, "Content-Type: text/plain\r\n\r\nIt works!\ncom_version: %s\ncom_frameTime: %d", com_version->string, com_frameTime);
		FCGX_FPrintF(webapiRequest.out, "Content-Type: application/json\r\n\r\n{\n  \"conclusion\": \"It works!\",\n  \"version\": \"%s\",\n  \"frameTime\": %d\n}", com_version->string, com_frameTime);

		// TODO: Ensure required parameters are given
		// TODO: Ensure globally valid REQUEST_METHOD (GET, HEAD, POST, PUT, DELETE)
		// TODO: Authentication/Authorization
		// TODO: Parse out PATH_INFO segments (probably into std::vector<string>)
		// TODO: Parse out QUERY_STRING values (probably into std::map<string, string>)
		// TODO: Locate appropriate resource controller to handle the request

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