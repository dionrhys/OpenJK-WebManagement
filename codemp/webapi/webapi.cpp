#include "webapi.h"

#include "qcommon/qcommon.h"
#include "libfcgi/fcgiapp.h"

// The accepting thread will signal acceptedRequestEvent when it has a request that it needs the main thread to handle
// The main thread will signal finishedRequestEvent after it has finished handling the request
// (both events are auto-reset events)
HANDLE acceptedRequestEvent;
HANDLE finishedRequestEvent;

// Handle to the thread used for accepting FastCGI requests
HANDLE acceptingThreadHandle;

// Specifies whether the web API is initialized or not
bool webapiInitialized = false;

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

	int result = WaitForSingleObject(acceptedRequestEvent, 0);
	if (result == WAIT_OBJECT_0)
	{
		Com_Printf("Oooh, piece of candy!\n");
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
	FCGX_Stream *in, *out, *err;
	FCGX_ParamArray envp;
	int result;

	while ((result = FCGX_Accept(&in, &out, &err, &envp)) >= 0)
	{
		// TODO: Pre-process the request and put it in shared memory

		SetEvent(acceptedRequestEvent);
		WaitForSingleObject(finishedRequestEvent, INFINITE);

		// TODO: Un-process the request?

		FCGX_FPrintF(out, "Content-Type: text/plain\r\n\r\nIt works!");
	}
}