/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Windows Server
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <winpr/tchar.h>
#include <winpr/windows.h>
#include <freerdp/listener.h>
#include <freerdp/utils/sleep.h>

#include "wf_mirage.h"

#include "wf_peer.h"

void wf_peer_context_new(freerdp_peer* client, wfPeerContext* context)
{
	DWORD dRes;

	if(!wfInfoSingleton)
	{
		wfInfoSingleton = (wfInfo*)malloc(sizeof(wfInfo)); //free this on shutdown
		memset(wfInfoSingleton, 0, sizeof(wfInfo));

		wfInfoSingleton->mutex = CreateMutex( 
        NULL,              // default security attributes
        FALSE,             // initially not owned
        NULL);             // unnamed mutex

		if (wfInfoSingleton->mutex == NULL) 
		{
			_tprintf(_T("CreateMutex error: %d\n"), GetLastError());
		}
	}
	
	dRes = WaitForSingleObject( 
            wfInfoSingleton->mutex,    // handle to mutex
            INFINITE);  // no time-out interval

	switch(dRes)
	{
		case WAIT_OBJECT_0:
			if(wfInfoSingleton->subscribers == 0)
			{
				//only the first peer needs to call this.
				context->wfInfo = wfInfoSingleton;
				wf_check_disp_devices(context->wfInfo);
				wf_disp_device_set_attatch(context->wfInfo, 1);
				wf_update_mirror_drv(context->wfInfo, 0);
				wf_map_mirror_mem(context->wfInfo);
			}
			++wfInfoSingleton->subscribers;

			if (! ReleaseMutex(wfInfoSingleton->mutex)) 
            { 
                _tprintf(_T("Error releasing mutex\n"));
            } 

			break;

		default:
			_tprintf(_T("Error waiting for mutex: %d\n"), dRes);
	}

}

void wf_peer_context_free(freerdp_peer* client, wfPeerContext* context)
{
	if (context && (wfInfoSingleton->subscribers == 1))
	{
		//only the last peer needs to call this
		wf_mirror_cleanup(context->wfInfo);
		wf_disp_device_set_attatch(context->wfInfo, 0);
		wf_update_mirror_drv(context->wfInfo, 1);
	}

	--wfInfoSingleton->subscribers;
}

static DWORD WINAPI wf_peer_mirror_monitor(LPVOID lpParam)
{
	DWORD dRes;
	GETCHANGESBUF* buf;
	//int derp;


	buf = (GETCHANGESBUF*)wfInfoSingleton->changeBuffer;

	//derp = 0;
	while(1)//info->subscribers > 0)
	{

		dRes = WaitForSingleObject(
			wfInfoSingleton->mutex,    // handle to mutex
            INFINITE);  // no time-out interval
		switch(dRes)
		{
			case WAIT_OBJECT_0:
				if(wfInfoSingleton->subscribers > 0)
				{
					_tprintf(_T("Count = %lu\n"), buf->buffer->counter);
				}

				if (! ReleaseMutex(wfInfoSingleton->mutex)) 
				{ 
					_tprintf(_T("Error releasing mutex\n"));
				} 

				break;

			default:
				_tprintf(_T("Error waiting for mutex: %d\n"), dRes);
		}


		freerdp_sleep(1);
		//derp++;
	}

	_tprintf(_T("monitor thread terminating...\n"));
	return 0;
}

void wf_peer_init(freerdp_peer* client)
{
	DWORD dRes;

	client->context_size = sizeof(wfPeerContext);
	client->ContextNew = (psPeerContextNew) wf_peer_context_new;
	client->ContextFree = (psPeerContextFree) wf_peer_context_free;
	freerdp_peer_context_new(client);

	_tprintf(_T("Trying to create a monitor thread...\n"));

	if(wfInfoSingleton)
	{
		dRes = WaitForSingleObject(
			wfInfoSingleton->mutex,    // handle to mutex
            INFINITE);  // no time-out interval
		switch(dRes)
		{
			case WAIT_OBJECT_0:
				if(wfInfoSingleton->threadCnt == 0)
				{
					if (CreateThread(NULL, 0, wf_peer_mirror_monitor, NULL, 0, NULL) != 0)
						_tprintf(_T("Created!\n"));

					++wfInfoSingleton->threadCnt;
				}

				if (! ReleaseMutex(wfInfoSingleton->mutex)) 
				{ 
					_tprintf(_T("Error releasing mutex\n"));
				} 

				break;

			default:
				_tprintf(_T("Error waiting for mutex: %d\n"), dRes);
		}


	}
		
}

boolean wf_peer_post_connect(freerdp_peer* client)
{
	wfPeerContext* context = (wfPeerContext*) client->context;

	/**
	 * This callback is called when the entire connection sequence is done, i.e. we've received the
	 * Font List PDU from the client and sent out the Font Map PDU.
	 * The server may start sending graphics output and receiving keyboard/mouse input after this
	 * callback returns.
	 */

	printf("Client %s is activated (osMajorType %d osMinorType %d)", client->local ? "(local)" : client->hostname,
		client->settings->os_major_type, client->settings->os_minor_type);

	if (client->settings->autologon)
	{
		printf(" and wants to login automatically as %s\\%s",
			client->settings->domain ? client->settings->domain : "",
			client->settings->username);

		/* A real server may perform OS login here if NLA is not executed previously. */
	}
	printf("\n");

	printf("Client requested desktop: %dx%dx%d\n",
		client->settings->width, client->settings->height, client->settings->color_depth);

	return true;
}

boolean wf_peer_activate(freerdp_peer* client)
{
	wfPeerContext* context = (wfPeerContext*) client->context;

	context->activated = true;

	return true;
}

void wf_peer_synchronize_event(rdpInput* input, uint32 flags)
{

}
