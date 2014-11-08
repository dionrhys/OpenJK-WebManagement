#include "webapi.h"

#include "qcommon/qcommon.h"
#include "libfcgi/fcgiapp.h"

// The accepting thread will signal acceptedRequestEvent when it has a request that it needs the main thread to handle
// The main thread will signal finishedRequestEvent after it has finished handling the request
// (both events are auto-reset events)
HANDLE acceptedRequestEvent;
HANDLE finishedRequestEvent;

HANDLE acceptingThreadHandle;

static DWORD WINAPI _WEBAPI_AcceptingThreadProc(LPVOID);
static void WEBAPI_AcceptingThread();

void WEBAPI_Init()
{
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
	acceptingThreadHandle = CreateThread(NULL, 0, _WEBAPI_AcceptingThreadProc, NULL, 0, NULL);
}

void WEBAPI_Shutdown()
{
	Com_Printf("Stopping Web API\n");
	FCGX_ShutdownPending();
	WaitForSingleObject(acceptingThreadHandle, INFINITE);
}

void WEBAPI_Frame()
{
	int result = WaitForSingleObject(acceptedRequestEvent, 0);
	if (result == WAIT_OBJECT_0)
	{
		Com_Printf("Oooh, piece of candy!\n");
		SetEvent(finishedRequestEvent);
	}
}

static DWORD WINAPI _WEBAPI_AcceptingThreadProc(LPVOID)
{
	WEBAPI_AcceptingThread();
	return 1;
}

static void WEBAPI_AcceptingThread()
{
	FCGX_Stream *in, *out, *err;
	FCGX_ParamArray envp;
	int result;

	while ((result = FCGX_Accept(&in, &out, &err, &envp)) >= 0)
	{
		SetEvent(acceptedRequestEvent);
		WaitForSingleObject(finishedRequestEvent, INFINITE);
		FCGX_FPrintF(out, "Content-Type: text/plain\r\n\r\nIt works!");
	}
}