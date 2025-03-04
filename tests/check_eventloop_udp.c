/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <open62541/plugin/eventloop.h>
#include <open62541/plugin/log_stdout.h>
#include "open62541/types.h"
#include "open62541/types_generated.h"

#include "testing_clock.h"
#include <time.h>
#include <check.h>

static UA_EventLoop *el;
static char *testMsg = "open62541";
static uintptr_t clientId;
static UA_Boolean received;

typedef struct TestContext {
    unsigned connCount;
} TestContext;

static void
connectionCallback(UA_ConnectionManager *cm, uintptr_t connectionId,
                   void *application, void **connectionContext,
                   UA_ConnectionState status,
                   const UA_KeyValueMap *params,
                   UA_ByteString msg) {
    TestContext *ctx = (TestContext*) *connectionContext;
    if(status == UA_CONNECTIONSTATE_CLOSING) {
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Closing connection %u", (unsigned)connectionId);
    } else {
        if(msg.length == 0) {
            UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                         "Opening connection %u", (unsigned)connectionId);
        } else {
            UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                         "Received a message of length %u", (unsigned)msg.length);
        }
    }

    if(msg.length == 0 && status == UA_CONNECTIONSTATE_ESTABLISHED) {
        ctx->connCount++;
        clientId = connectionId;

        /* The remote-hostname is set during the first callback */
        if(!UA_KeyValueMap_isEmpty(params)) {
            const void *hn =
                UA_KeyValueMap_getScalar(params,
                                         UA_QUALIFIEDNAME(0, "remote-hostname"),
                                         &UA_TYPES[UA_TYPES_STRING]);
            ck_assert(hn != NULL);
        }
    }

    if(status == UA_CONNECTIONSTATE_CLOSING)
        ctx->connCount--;

    if(msg.length > 0) {
        UA_ByteString rcv = UA_BYTESTRING(testMsg);
        ck_assert(UA_String_equal(&msg, &rcv));
        received = true;
    }
}

START_TEST(listenUDP) {
    UA_ConnectionManager *cm = UA_ConnectionManager_new_POSIX_UDP(UA_STRING("udpCM"));
    el = UA_EventLoop_new_POSIX(UA_Log_Stdout);
    el->registerEventSource(el, &cm->eventSource);
    el->start(el);

    TestContext testContext = {0};

    UA_UInt16 port = 4840;
    UA_Variant portVar;
    UA_Variant_setScalar(&portVar, &port, &UA_TYPES[UA_TYPES_UINT16]);

    UA_KeyValuePair params[1];
    params[0].key = UA_QUALIFIEDNAME(0, "listen-port");
    params[0].value = portVar;
    UA_KeyValueMap paramsMap;
    paramsMap.map = params;
    paramsMap.mapSize = 1;

    cm->openConnection(cm, &paramsMap, NULL, &testContext, connectionCallback);

    ck_assert(testContext.connCount > 0);

    for(size_t i = 0; i < 10; i++) {
        UA_DateTime next = el->run(el, 1);
        UA_fakeSleep((UA_UInt32)((next - UA_DateTime_now()) / UA_DATETIME_MSEC));
    }

    /* Stop the EventLoop */
    int max_stop_iteration_count = 1000;
    int iteration = 0;
    el->stop(el);
    while(el->state != UA_EVENTLOOPSTATE_STOPPED &&
          iteration < max_stop_iteration_count) {
        UA_DateTime next = el->run(el, 1);
        UA_fakeSleep((UA_UInt32)((next - UA_DateTime_now()) / UA_DATETIME_MSEC));
        iteration++;
    }
    el->free(el);
    el = NULL;

    ck_assert_uint_eq(testContext.connCount, 0);
} END_TEST

START_TEST(connectUDPValidationSucceeds) {
        UA_ConnectionManager *cm = UA_ConnectionManager_new_POSIX_UDP(UA_STRING("udpCM"));
        el = UA_EventLoop_new_POSIX(UA_Log_Stdout);
        el->registerEventSource(el, &cm->eventSource);

        UA_UInt16 port = 30000;
        UA_Variant portVar;
        UA_Variant_setScalar(&portVar, &port, &UA_TYPES[UA_TYPES_UINT16]);

        UA_Boolean validate = true;
        UA_Variant validateVar;
        UA_Variant_setScalar(&validateVar, &validate, &UA_TYPES[UA_TYPES_BOOLEAN]);


        UA_KeyValuePair params[3];
        params[0].key = UA_QUALIFIEDNAME(0, "listen-port");
        params[0].value = portVar;

        UA_String hostname = UA_STRING("127.0.0.1");
        params[1].key = UA_QUALIFIEDNAME(0, "listen-hostnames");
        UA_Variant_setArray(&params[1].value, &hostname, 1, &UA_TYPES[UA_TYPES_STRING]);

        params[2].key = UA_QUALIFIEDNAME(0, "validate");
        params[2].value = validateVar;

        TestContext testContext;
        testContext.connCount = 0;

        UA_KeyValueMap paramsMap;
        paramsMap.map = params;
        paramsMap.mapSize = 3;

        UA_StatusCode retval =
            cm->openConnection(cm, &paramsMap, NULL, &testContext, connectionCallback);
        ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);

        /* Open a client connection */
        clientId = 0;
        UA_String targetHost = UA_STRING("localhost");
        params[0].key = UA_QUALIFIEDNAME(0, "port");
        params[0].value = portVar;
        params[1].key = UA_QUALIFIEDNAME(0, "hostname");
        UA_Variant_setScalar(&params[1].value, &targetHost, &UA_TYPES[UA_TYPES_STRING]);

        params[2].key = UA_QUALIFIEDNAME(0, "validate");
        params[2].value = validateVar;

        paramsMap.map = params;
        paramsMap.mapSize = 3;

        retval = cm->openConnection(cm, &paramsMap, NULL, &testContext, connectionCallback);
        ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);
        el->free(el);
        el = NULL;
    } END_TEST

START_TEST(connectUDPValidationFails) {
        UA_ConnectionManager *cm = UA_ConnectionManager_new_POSIX_UDP(UA_STRING("udpCM"));
        el = UA_EventLoop_new_POSIX(UA_Log_Stdout);
        el->registerEventSource(el, &cm->eventSource);

        UA_UInt16 port = 30000;
        UA_Variant portVar;
        UA_Variant_setScalar(&portVar, &port, &UA_TYPES[UA_TYPES_UINT16]);

        UA_Boolean validate = true;
        UA_Variant validateVar;
        UA_Variant_setScalar(&validateVar, &validate, &UA_TYPES[UA_TYPES_BOOLEAN]);


        UA_KeyValuePair params[3];
        params[0].key = UA_QUALIFIEDNAME(0, "listen-port");
        params[0].value = portVar;

        UA_String hostname = UA_STRING("300.300.300.300");
        params[1].key = UA_QUALIFIEDNAME(0, "listen-hostnames");
        UA_Variant_setArray(&params[1].value, &hostname, 1, &UA_TYPES[UA_TYPES_STRING]);

        params[2].key = UA_QUALIFIEDNAME(0, "validate");
        params[2].value = validateVar;

        TestContext testContext;
        testContext.connCount = 0;

        UA_KeyValueMap paramsMap;
        paramsMap.map = params;
        paramsMap.mapSize = 3;

        UA_StatusCode retval =
            cm->openConnection(cm, &paramsMap, NULL, &testContext, connectionCallback);
        ck_assert_uint_eq(retval, UA_STATUSCODE_BADCONNECTIONREJECTED);

        /* Open a client connection */
        clientId = 0;
        UA_String targetHost = UA_STRING("localho");
        params[0].key = UA_QUALIFIEDNAME(0, "port");
        params[0].value = portVar;
        params[1].key = UA_QUALIFIEDNAME(0, "hostname");
        UA_Variant_setScalar(&params[1].value, &targetHost, &UA_TYPES[UA_TYPES_STRING]);

        params[2].key = UA_QUALIFIEDNAME(0, "validate");
        params[2].value = validateVar;

        retval = cm->openConnection(cm, &paramsMap, NULL, &testContext, connectionCallback);
        ck_assert_uint_eq(retval, UA_STATUSCODE_BADCONNECTIONREJECTED);
        el->free(el);
        el = NULL;
    } END_TEST

START_TEST(connectUDP) {
    UA_ConnectionManager *cm = UA_ConnectionManager_new_POSIX_UDP(UA_STRING("udpCM"));
    el = UA_EventLoop_new_POSIX(UA_Log_Stdout);
    el->registerEventSource(el, &cm->eventSource);
    el->start(el);

    UA_UInt16 port = 30000;
    UA_Variant portVar;
    UA_Variant_setScalar(&portVar, &port, &UA_TYPES[UA_TYPES_UINT16]);

    UA_KeyValuePair params[2];
    params[0].key = UA_QUALIFIEDNAME(0, "listen-port");
    params[0].value = portVar;
    UA_KeyValueMap paramsMap;
    paramsMap.map = params;
    paramsMap.mapSize = 1;

    TestContext testContext;
    testContext.connCount = 0;

    UA_StatusCode retval =
        cm->openConnection(cm, &paramsMap, NULL, &testContext, connectionCallback);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);

    size_t listenSockets = testContext.connCount;

    /* Open a client connection */
    clientId = 0;
    UA_String targetHost = UA_STRING("localhost");
    params[0].key = UA_QUALIFIEDNAME(0, "port");
    params[0].value = portVar;
    params[1].key = UA_QUALIFIEDNAME(0, "hostname");
    UA_Variant_setScalar(&params[1].value, &targetHost, &UA_TYPES[UA_TYPES_STRING]);

    paramsMap.mapSize = 2;

    retval = cm->openConnection(cm, &paramsMap, NULL, &testContext, connectionCallback);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);
    for(size_t i = 0; i < 2; i++) {
        UA_DateTime next = el->run(el, 1);
        UA_fakeSleep((UA_UInt32)((next - UA_DateTime_now()) / UA_DATETIME_MSEC));
    }
    ck_assert(clientId != 0);
    ck_assert_uint_eq(testContext.connCount, listenSockets + 1);

    /* Send a message from the client */
    received = false;
    UA_ByteString snd;
    retval = cm->allocNetworkBuffer(cm, clientId, &snd, strlen(testMsg));
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);
    memcpy(snd.data, testMsg, strlen(testMsg));
    retval = cm->sendWithConnection(cm, clientId, &UA_KEYVALUEMAP_NULL, &snd);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);
    for(size_t i = 0; i < 2; i++) {
        UA_DateTime next = el->run(el, 1);
        UA_fakeSleep((UA_UInt32)((next - UA_DateTime_now()) / UA_DATETIME_MSEC));
    }
    ck_assert(received);

    /* Close the connection */
    retval = cm->closeConnection(cm, clientId);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_uint_eq(testContext.connCount, listenSockets + 1);
    for(size_t i = 0; i < 2; i++) {
        UA_DateTime next = el->run(el, 1);
        UA_fakeSleep((UA_UInt32)((next - UA_DateTime_now()) / UA_DATETIME_MSEC));
    }
    ck_assert_uint_eq(testContext.connCount, listenSockets);

    /* Stop the EventLoop */
    int max_stop_iteration_count = 10;
    int iteration = 0;
    el->stop(el);
    while(el->state != UA_EVENTLOOPSTATE_STOPPED &&
          iteration < max_stop_iteration_count) {
        UA_DateTime next = el->run(el, 1);
        UA_fakeSleep((UA_UInt32)((next - UA_DateTime_now()) / UA_DATETIME_MSEC));
        iteration++;
    }
    ck_assert(el->state == UA_EVENTLOOPSTATE_STOPPED);
    el->free(el);
    el = NULL;
} END_TEST

START_TEST(udpTalkerAndListener) {
    /* create listener eventloop */
    UA_EventLoop *elListener = UA_EventLoop_new_POSIX(UA_Log_Stdout);
    UA_ConnectionManager *cmListener = UA_ConnectionManager_new_POSIX_UDP(UA_STRING("udpCM"));
    elListener->registerEventSource(elListener, &cmListener->eventSource);
    elListener->start(elListener);

    UA_EventLoop *elTalker = UA_EventLoop_new_POSIX(UA_Log_Stdout);
    UA_ConnectionManager *cmTalker = UA_ConnectionManager_new_POSIX_UDP(UA_STRING("udpCM"));
    elTalker->registerEventSource(elTalker, &cmTalker->eventSource);
    elTalker->start(elTalker);

    /* Open a listener connection */
    UA_UInt16 port = 30000;
    UA_Variant portVar;
    UA_Variant_setScalar(&portVar, &port, &UA_TYPES[UA_TYPES_UINT16]);
    UA_KeyValuePair params[2];
    params[0].key = UA_QUALIFIEDNAME(0, "listen-port");
    params[0].value = portVar;

    UA_KeyValueMap paramsMap;
    paramsMap.map = params;
    paramsMap.mapSize = 1;

    TestContext testContext;
    testContext.connCount = 0;

    UA_StatusCode retval =
        cmListener->openConnection(cmListener, &paramsMap, NULL, &testContext,
                                   connectionCallback);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);

    size_t listenSockets = testContext.connCount;

    /* Open a talker connection */
    clientId = 0;

    UA_String targetHost = UA_STRING("localhost");
    params[0].key = UA_QUALIFIEDNAME(0, "port");
    params[0].value = portVar;
    params[1].key = UA_QUALIFIEDNAME(0, "hostname");
    UA_Variant_setScalar(&params[1].value, &targetHost, &UA_TYPES[UA_TYPES_STRING]);

    paramsMap.mapSize = 2;

    retval = cmTalker->openConnection(cmTalker, &paramsMap, NULL, &testContext,
                                      connectionCallback);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);

    /* The talker el should receive a signal "ready to be written on" */
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);
    for(size_t i = 0; i < 2; i++) {
        UA_DateTime next = elTalker->run(elTalker, 1);
        UA_fakeSleep((UA_UInt32)((next - UA_DateTime_now()) / UA_DATETIME_MSEC));
    }
    ck_assert_uint_ne(clientId, 0);
    ck_assert_uint_eq(testContext.connCount, listenSockets + 1);

    /* Send a message from the talker */
    received = false;
    UA_ByteString snd;
    retval = cmTalker->allocNetworkBuffer(cmTalker, clientId, &snd, strlen(testMsg));
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);
    memcpy(snd.data, testMsg, strlen(testMsg));
    retval = cmTalker->sendWithConnection(cmTalker, clientId, &UA_KEYVALUEMAP_NULL, &snd);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);
    for(size_t i = 0; i < 2; i++) {
        UA_DateTime next = elListener->run(elListener, 1);
        UA_fakeSleep((UA_UInt32)((next - UA_DateTime_now()) / UA_DATETIME_MSEC));
    }
    ck_assert(received);

    /* Close the connection */
    retval = cmTalker->closeConnection(cmTalker, clientId);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_uint_eq(testContext.connCount, listenSockets + 1);
    for(size_t i = 0; i < 2; i++) {
        UA_DateTime next = elTalker->run(elTalker, 1);
        UA_fakeSleep((UA_UInt32)((next - UA_DateTime_now()) / UA_DATETIME_MSEC));
    }
    ck_assert_uint_eq(testContext.connCount, listenSockets);

    /* Stop the Talker EventLoop */
    int max_stop_iteration_count = 10;
    int iteration = 0;
    elTalker->stop(elTalker);
    while(elTalker->state != UA_EVENTLOOPSTATE_STOPPED &&
          iteration < max_stop_iteration_count) {
        UA_DateTime next = elTalker->run(elTalker, 1);
        UA_fakeSleep((UA_UInt32)((next - UA_DateTime_now()) / UA_DATETIME_MSEC));
        iteration++;
    }
    ck_assert_int_eq(elTalker->state, UA_EVENTLOOPSTATE_STOPPED);
    elTalker->free(elTalker);
    elTalker = NULL;

    /* Stop the Listener EventLoop */
    max_stop_iteration_count = 10;
    iteration = 0;
    elListener->stop(elListener);
    while(elListener->state != UA_EVENTLOOPSTATE_STOPPED &&
          iteration < max_stop_iteration_count) {
        UA_DateTime next = elListener->run(elListener, 1);
        UA_fakeSleep((UA_UInt32)((next - UA_DateTime_now()) / UA_DATETIME_MSEC));
        iteration++;
    }
    ck_assert(elListener->state == UA_EVENTLOOPSTATE_STOPPED);
    elListener->free(elListener);
    elListener = NULL;

    ck_assert_uint_eq(testContext.connCount, 0);
} END_TEST

START_TEST(udpTalkerAndListenerDifferentDestination) {
    /* create listener eventloop */
    UA_EventLoop *elListener = UA_EventLoop_new_POSIX(UA_Log_Stdout);
    UA_ConnectionManager *cmListener = UA_ConnectionManager_new_POSIX_UDP(UA_STRING("udpCM"));
    elListener->registerEventSource(elListener, &cmListener->eventSource);
    elListener->start(elListener);

    UA_EventLoop *elTalker = UA_EventLoop_new_POSIX(UA_Log_Stdout);
    UA_ConnectionManager *cmTalker = UA_ConnectionManager_new_POSIX_UDP(UA_STRING("udpCM"));
    elTalker->registerEventSource(elTalker, &cmTalker->eventSource);
    elTalker->start(elTalker);

    /* Open a listener connection */
    UA_UInt16 port = 30000;
    UA_Variant portVar;
    UA_Variant_setScalar(&portVar, &port, &UA_TYPES[UA_TYPES_UINT16]);
    UA_KeyValuePair listenParams[2];
    listenParams[0].key = UA_QUALIFIEDNAME(0, "listen-port");
    listenParams[0].value = portVar;
    UA_KeyValueMap listenParamsMap;
    listenParamsMap.map = listenParams;
    listenParamsMap.mapSize = 1;

    TestContext testContext;
    testContext.connCount = 0;

    UA_StatusCode retval =
        cmListener->openConnection(cmListener, &listenParamsMap, NULL, &testContext,
                                   connectionCallback);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);

    size_t listenSockets = testContext.connCount;

    /* Open a talker connection */
    clientId = 0;

    UA_KeyValuePair connectionParams[2];
    UA_String connectionTargetHost = UA_STRING("localhost");
    connectionParams[0].key = UA_QUALIFIEDNAME(0, "port");
    connectionParams[0].value = portVar;
    connectionParams[1].key = UA_QUALIFIEDNAME(0, "hostname");
    UA_Variant_setScalar(&connectionParams[1].value, &connectionTargetHost, &UA_TYPES[UA_TYPES_STRING]);
    UA_KeyValueMap connectionParamsMap;
    connectionParamsMap.map = connectionParams;
    connectionParamsMap.mapSize = 2;

    retval = cmTalker->openConnection(cmTalker, &connectionParamsMap, NULL, &testContext,
                                      connectionCallback);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);

    /* The talker el should receive a signal "ready to be written on" */
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);
    for(size_t i = 0; i < 2; i++) {
        UA_DateTime next = elTalker->run(elTalker, 1);
        UA_fakeSleep((UA_UInt32)((next - UA_DateTime_now()) / UA_DATETIME_MSEC));
    }
    ck_assert_uint_ne(clientId, 0);
    ck_assert_uint_eq(testContext.connCount, listenSockets + 1);

    /* Send a message from the talker */
    received = false;
    UA_ByteString snd;
    retval = cmTalker->allocNetworkBuffer(cmTalker, clientId, &snd, strlen(testMsg));
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);
    memcpy(snd.data, testMsg, strlen(testMsg));

    UA_KeyValuePair sendParams[2];
    UA_String sendTargetHost = UA_STRING("127.0.0.1");
    sendParams[0].key = UA_QUALIFIEDNAME(0, "port");
    sendParams[0].value = portVar;
    sendParams[1].key = UA_QUALIFIEDNAME(0, "hostname");
    UA_Variant_setScalar(&sendParams[1].value, &sendTargetHost, &UA_TYPES[UA_TYPES_STRING]);

    UA_KeyValueMap sendParamsMap;
    sendParamsMap.map = sendParams;
    sendParamsMap.mapSize = 2;

    retval = cmTalker->sendWithConnection(cmTalker, clientId, &sendParamsMap, &snd);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);
    for(size_t i = 0; i < 2; i++) {
        UA_DateTime next = elListener->run(elListener, 1);
        UA_fakeSleep((UA_UInt32)((next - UA_DateTime_now()) / UA_DATETIME_MSEC));
    }
    ck_assert(received);

    /* Close the connection */
    retval = cmTalker->closeConnection(cmTalker, clientId);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_uint_eq(testContext.connCount, listenSockets + 1);
    for(size_t i = 0; i < 2; i++) {
        UA_DateTime next = elTalker->run(elTalker, 1);
        UA_fakeSleep((UA_UInt32)((next - UA_DateTime_now()) / UA_DATETIME_MSEC));
    }
    ck_assert_uint_eq(testContext.connCount, listenSockets);

    /* Stop the Talker EventLoop */
    int max_stop_iteration_count = 10;
    int iteration = 0;
    elTalker->stop(elTalker);
    while(elTalker->state != UA_EVENTLOOPSTATE_STOPPED &&
          iteration < max_stop_iteration_count) {
        UA_DateTime next = elTalker->run(elTalker, 1);
        UA_fakeSleep((UA_UInt32)((next - UA_DateTime_now()) / UA_DATETIME_MSEC));
        iteration++;
    }
    ck_assert_int_eq(elTalker->state, UA_EVENTLOOPSTATE_STOPPED);
    elTalker->free(elTalker);
    elTalker = NULL;

    /* Stop the Listener EventLoop */
    max_stop_iteration_count = 10;
    iteration = 0;
    elListener->stop(elListener);
    while(elListener->state != UA_EVENTLOOPSTATE_STOPPED &&
          iteration < max_stop_iteration_count) {
        UA_DateTime next = elListener->run(elListener, 1);
        UA_fakeSleep((UA_UInt32)((next - UA_DateTime_now()) / UA_DATETIME_MSEC));
        iteration++;
    }
    ck_assert(elListener->state == UA_EVENTLOOPSTATE_STOPPED);
    elListener->free(elListener);
    elListener = NULL;

    ck_assert_uint_eq(testContext.connCount, 0);
} END_TEST

int main(void) {
    Suite *s  = suite_create("Test UDP EventLoop");
    TCase *tc = tcase_create("test cases");
    tcase_add_test(tc, listenUDP);
    tcase_add_test(tc, connectUDP);
    tcase_add_test(tc, connectUDPValidationFails);
    tcase_add_test(tc, connectUDPValidationSucceeds);
    tcase_add_test(tc, udpTalkerAndListener);
    tcase_add_test(tc, udpTalkerAndListenerDifferentDestination);
    suite_add_tcase(s, tc);

    SRunner *sr = srunner_create(s);
    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all (sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
