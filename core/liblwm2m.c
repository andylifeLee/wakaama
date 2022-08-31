/*******************************************************************************
 *
 * Copyright (c) 2013, 2014 Intel Corporation and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v20.html
 * The Eclipse Distribution License is available at
 *    http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    David Navarro, Intel Corporation - initial API and implementation
 *    Fabien Fleutot - Please refer to git log
 *    Simon Bernard - Please refer to git log
 *    Toby Jaffey - Please refer to git log
 *    Pascal Rieux - Please refer to git log
 *    Tuve Nordius, Husqvarna Group - Please refer to git log
 *
 *******************************************************************************/

/*
 Copyright (c) 2013, 2014 Intel Corporation

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

     * Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
     * Neither the name of Intel Corporation nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 THE POSSIBILITY OF SUCH DAMAGE.

 David Navarro <david.navarro@intel.com>

*/

#include "internals.h"

#include <stdlib.h>
#include <string.h>

#include <stdio.h>


lwm2m_context_t * lwm2m_init(void * userData)
{
    lwm2m_context_t * contextP;

    LOG("Entering");
    contextP = (lwm2m_context_t *)lwm2m_malloc(sizeof(lwm2m_context_t));
    if (NULL != contextP)
    {
        memset(contextP, 0, sizeof(lwm2m_context_t));
        contextP->userData = userData;
        srand((int)lwm2m_gettime());  //srand(time(NULL)) 해당 함수를 사용해야 랜덤 난수 생성이 가능해진다.
        contextP->nextMID = rand();   //즉 위 코드가 없으면 rand() 함수를 써도 계속 같은 수가 생성된다.
    }

    return contextP;
}

#ifdef LWM2M_CLIENT_MODE
void lwm2m_deregister(lwm2m_context_t * context)
{
    lwm2m_server_t * server = context->serverList;

    LOG("Entering");
    while (NULL != server)
    {
        registration_deregister(context, server);
        server = server->next;
    }
}

static void prv_deleteServer(lwm2m_server_t * serverP, void *userData)
{
    // TODO parse transaction and observation to remove the ones related to this server
    if (serverP->sessionH != NULL)
    {
         lwm2m_close_connection(serverP->sessionH, userData);
    }
    if (NULL != serverP->location)
    {
        lwm2m_free(serverP->location);
    }

    while(serverP->blockData != NULL)
    {
        lwm2m_block_data_t * targetP;
        targetP = serverP->blockData;
        serverP->blockData = serverP->blockData->next;
        free_block_data(targetP);
    }
    
    lwm2m_free(serverP);
}

static void prv_deleteServerList(lwm2m_context_t * context)
{
    while (NULL != context->serverList)
    {
        lwm2m_server_t * server;
        server = context->serverList;
        context->serverList = server->next;
        prv_deleteServer(server, context->userData);
    }
}

static void prv_deleteBootstrapServer(lwm2m_server_t * serverP, void *userData)
{
    // TODO should we free location as in prv_deleteServer ?
    // TODO should we parse transaction and observation to remove the ones related to this server ?
    if (serverP->sessionH != NULL)
    {
         lwm2m_close_connection(serverP->sessionH, userData);
    }
    lwm2m_free(serverP);
}

static void prv_deleteBootstrapServerList(lwm2m_context_t * context)
{
    while (NULL != context->bootstrapServerList)
    {
        lwm2m_server_t * server;
        server = context->bootstrapServerList;
        context->bootstrapServerList = server->next;
        prv_deleteBootstrapServer(server, context->userData);
    }
}

static void prv_deleteObservedList(lwm2m_context_t * contextP)
{
    while (NULL != contextP->observedList)
    {
        lwm2m_observed_t * targetP;
        lwm2m_watcher_t * watcherP;

        targetP = contextP->observedList;
        contextP->observedList = contextP->observedList->next;

        for (watcherP = targetP->watcherList ; watcherP != NULL ; watcherP = watcherP->next)
        {
            if (watcherP->parameters != NULL) lwm2m_free(watcherP->parameters);
        }
        LWM2M_LIST_FREE(targetP->watcherList);

        lwm2m_free(targetP);
    }
}
#endif

void prv_deleteTransactionList(lwm2m_context_t * context)
{
    while (NULL != context->transactionList)
    {
        lwm2m_transaction_t * transaction;

        transaction = context->transactionList;
        context->transactionList = context->transactionList->next;
        transaction_free(transaction);
    }
}

void lwm2m_close(lwm2m_context_t * contextP)
{
#ifdef LWM2M_CLIENT_MODE

    LOG("Entering");
    lwm2m_deregister(contextP);
    prv_deleteServerList(contextP);
    prv_deleteBootstrapServerList(contextP);
    prv_deleteObservedList(contextP);
    lwm2m_free(contextP->endpointName);
    if (contextP->msisdn != NULL)
    {
        lwm2m_free(contextP->msisdn);
    }
    if (contextP->altPath != NULL)
    {
        lwm2m_free(contextP->altPath);
    }

#endif

#ifdef LWM2M_SERVER_MODE
    while (NULL != contextP->clientList)
    {
        lwm2m_client_t * clientP;

        clientP = contextP->clientList;
        contextP->clientList = contextP->clientList->next;

        registration_freeClient(clientP);
    }
#endif

    prv_deleteTransactionList(contextP);
    lwm2m_free(contextP);
}

#ifdef LWM2M_CLIENT_MODE
static int prv_refreshServerList(lwm2m_context_t * contextP)
{
    lwm2m_server_t * targetP;
    lwm2m_server_t * nextP;

    // Remove all servers marked as dirty
    targetP = contextP->bootstrapServerList;
    contextP->bootstrapServerList = NULL;
    while (targetP != NULL)
    {
        nextP = targetP->next;
        targetP->next = NULL;
        if (!targetP->dirty)
        {
            targetP->status = STATE_DEREGISTERED;
            contextP->bootstrapServerList = (lwm2m_server_t *)LWM2M_LIST_ADD(contextP->bootstrapServerList, targetP);
        }
        else
        {
            prv_deleteServer(targetP, contextP->userData);
        }
        targetP = nextP;
    }
    targetP = contextP->serverList;
    contextP->serverList = NULL;
    while (targetP != NULL)
    {
        nextP = targetP->next;
        targetP->next = NULL;
        if (!targetP->dirty)
        {
            // TODO: Should we revert the status to STATE_DEREGISTERED ?
            contextP->serverList = (lwm2m_server_t *)LWM2M_LIST_ADD(contextP->serverList, targetP);
        }
        else
        {
            prv_deleteServer(targetP, contextP->userData);
        }
        targetP = nextP;
    }

    return object_getServers(contextP, false);
}

int lwm2m_configure(lwm2m_context_t * contextP,
                    const char * endpointName,
                    const char * msisdn,
                    const char * altPath,
                    uint16_t numObject,
                    lwm2m_object_t * objectList[])
{
    int i;
    uint8_t found;

    LOG_ARG("endpointName: \"%s\", msisdn: \"%s\", altPath: \"%s\", numObject: %d",
            STR_NULL2EMPTY(endpointName),
            STR_NULL2EMPTY(msisdn),
            STR_NULL2EMPTY(altPath),
            numObject);
    // This API can be called only once for now
    if (contextP->endpointName != NULL || contextP->objectList != NULL) return COAP_400_BAD_REQUEST;

    if (endpointName == NULL) return COAP_400_BAD_REQUEST;
    if (numObject < 3) return COAP_400_BAD_REQUEST;
    // Check that mandatory objects are present
    found = 0;
    for (i = 0 ; i < numObject ; i++)
    {
        if (objectList[i]->objID == LWM2M_SECURITY_OBJECT_ID) found |= 0x01;
        if (objectList[i]->objID == LWM2M_SERVER_OBJECT_ID) found |= 0x02;
        if (objectList[i]->objID == LWM2M_DEVICE_OBJECT_ID) found |= 0x04;
    }
    if (found != 0x07) return COAP_400_BAD_REQUEST;
    if (altPath != NULL)
    {
        if (0 == utils_isAltPathValid(altPath))
        {
            return COAP_400_BAD_REQUEST;
        }
        if (altPath[1] == 0)
        {
            altPath = NULL;
        }
    }
    contextP->endpointName = lwm2m_strdup(endpointName);
    if (contextP->endpointName == NULL)
    {
        return COAP_500_INTERNAL_SERVER_ERROR;
    }

    if (msisdn != NULL)
    {
        contextP->msisdn = lwm2m_strdup(msisdn);
        if (contextP->msisdn == NULL)
        {
            return COAP_500_INTERNAL_SERVER_ERROR;
        }
    }

    if (altPath != NULL)
    {
        contextP->altPath = lwm2m_strdup(altPath);
        if (contextP->altPath == NULL)
        {
            return COAP_500_INTERNAL_SERVER_ERROR;
        }
    }

    for (i = 0; i < numObject; i++)
    {
        objectList[i]->next = NULL;
        contextP->objectList = (lwm2m_object_t *)LWM2M_LIST_ADD(contextP->objectList, objectList[i]);
    }

    return COAP_NO_ERROR;
}

int lwm2m_add_object(lwm2m_context_t * contextP,
                     lwm2m_object_t * objectP)
{
    lwm2m_object_t * targetP;

    LOG_ARG("ID: %d", objectP->objID);
    targetP = (lwm2m_object_t *)LWM2M_LIST_FIND(contextP->objectList, objectP->objID);
    if (targetP != NULL) return COAP_406_NOT_ACCEPTABLE;
    objectP->next = NULL;

    contextP->objectList = (lwm2m_object_t *)LWM2M_LIST_ADD(contextP->objectList, objectP);

    if (contextP->state == STATE_READY)
    {
        return lwm2m_update_registration(contextP, 0, true);
    }

    return COAP_NO_ERROR;
}

int lwm2m_remove_object(lwm2m_context_t * contextP,
                        uint16_t id)
{
    lwm2m_object_t * targetP;

    LOG_ARG("ID: %d", id);
    contextP->objectList = (lwm2m_object_t *)LWM2M_LIST_RM(contextP->objectList, id, &targetP);

    if (targetP == NULL) return COAP_404_NOT_FOUND;

    if (contextP->state == STATE_READY)
    {
        return lwm2m_update_registration(contextP, 0, true);
    }

    return 0;
}

#endif


int lwm2m_step(lwm2m_context_t * contextP,
               time_t * timeoutP)
{
    time_t tv_sec;

    LOG_ARG("timeoutP: %d", (int) *timeoutP);
    tv_sec = lwm2m_gettime();
    if (tv_sec < 0) return COAP_500_INTERNAL_SERVER_ERROR;

#ifdef LWM2M_CLIENT_MODE
    LOG_ARG("State: %s", STR_STATE(contextP->state));
    // state can also be modified in bootstrap_handleCommand().
    printf("111\n");
next_step:
    switch (contextP->state)
    {
    case STATE_INITIAL:
        if (0 != prv_refreshServerList(contextP)) return COAP_503_SERVICE_UNAVAILABLE;
        if (contextP->serverList != NULL)
        {
            contextP->state = STATE_REGISTER_REQUIRED;
            printf("STATE_INITIAL 222-1\n");
        }
        else
        {
            // Bootstrapping
        	printf("STATE_INITIAL 222-2\n");
            contextP->state = STATE_BOOTSTRAP_REQUIRED;
        }
        printf("STATE_INITIAL 222\n");
        goto next_step;

    case STATE_BOOTSTRAP_REQUIRED:
#ifdef LWM2M_BOOTSTRAP
        if (contextP->bootstrapServerList != NULL)
        {
            bootstrap_start(contextP);
            contextP->state = STATE_BOOTSTRAPPING;
            bootstrap_step(contextP, tv_sec, timeoutP);
            printf("STATE_BOOTSTRAP_REQUIRED 333-1\n");
        }
        else
#endif
        {
        	printf("STATE_BOOTSTRAP_REQUIRED 333-2\n");
            return COAP_503_SERVICE_UNAVAILABLE;
        }
    	printf("STATE_BOOTSTRAP_REQUIRED 333\n");
        break;

#ifdef LWM2M_BOOTSTRAP
    case STATE_BOOTSTRAPPING:
        switch (bootstrap_getStatus(contextP))
        {
        case STATE_BS_FINISHED:
            contextP->state = STATE_INITIAL;
        	printf("STATE_BOOTSTRAPPING 444-1\n");
            goto next_step;

        case STATE_BS_FAILED:
        	printf("STATE_BOOTSTRAPPING 444-2\n");
            return COAP_503_SERVICE_UNAVAILABLE;

        default:
            // keep on waiting
        	printf("STATE_BOOTSTRAPPING 444-3\n");
            bootstrap_step(contextP, tv_sec, timeoutP);
            break;
        }
    	printf("STATE_BOOTSTRAPPING 444\n");
        break;
#endif
    case STATE_REGISTER_REQUIRED:
    {
        int result = registration_start(contextP, true);
        if (COAP_NO_ERROR != result) return result;
        contextP->state = STATE_REGISTERING;
        printf("STATE_REGISTER_REQUIRED 555\n");
    }

    break;

    case STATE_REGISTERING:
    {
        switch (registration_getStatus(contextP))
        {
        case STATE_REGISTERED:
        	printf(" STATE_REGISTERED 666-1\n");
            contextP->state = STATE_READY;
            break;

        case STATE_REG_FAILED:
        	printf(" STATE_REG_FAILED 666-2\n");
            // TODO avoid infinite loop by checking the bootstrap info is different
            contextP->state = STATE_BOOTSTRAP_REQUIRED;
            goto next_step;

        case STATE_REG_PENDING:
        	printf("STATE_REG_PENDING 666-3\n");
        	break;
        default:
        	printf(" STATE_REG_PENDING 666-4\n");
            // keep on waiting
            break;
        }
        printf(" STATE_REGISTERING 666\n");
    }

    break;

    case STATE_READY:
        if (registration_getStatus(contextP) == STATE_REG_FAILED)
        {
            // TODO avoid infinite loop by checking the bootstrap info is different
            contextP->state = STATE_BOOTSTRAP_REQUIRED;
        	printf("STATE_READY 777-1\n");
            goto next_step;
            break;
        }
    	printf("STATE_READY 777-2\n");
        break;

    default:
    	printf("STATE_READY 777\n");
        // do nothing
        break;
    }
	printf("aaa\n");
    observe_step(contextP, tv_sec, timeoutP);
#endif

	printf("bbb\n");

    registration_step(contextP, tv_sec, timeoutP);

	printf("ccc\n");

    transaction_step(contextP, tv_sec, timeoutP);

	printf("ddd\n");

    LOG_ARG("Final timeoutP: %d", (int) *timeoutP);

#ifdef LWM2M_CLIENT_MODE
    LOG_ARG("Final state: %s", STR_STATE(contextP->state));
#endif
    return 0;
}
