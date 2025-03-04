/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *    Copyright 2019 (c) Fraunhofer IOSB (Author: Klaus Schick)
 *    Copyright 2014-2018 (c) Fraunhofer IOSB (Author: Julius Pfrommer)
 *    Copyright 2014, 2017 (c) Florian Palm
 *    Copyright 2015-2016 (c) Sten Grüner
 *    Copyright 2015 (c) Chris Iatrou
 *    Copyright 2015-2016 (c) Oleksiy Vasylyev
 *    Copyright 2016-2017 (c) Stefan Profanter, fortiss GmbH
 *    Copyright 2017 (c) Julian Grothoff
 *    Copyright 2019 (c) Kalycito Infotech Private Limited
 *    Copyright 2019 (c) HMS Industrial Networks AB (Author: Jonas Green)
 *    Copyright 2021 (c) Fraunhofer IOSB (Author: Andreas Ebner)
 *    Copyright 2022 (c) Christian von Arnim, ISW University of Stuttgart (for VDW and umati)
 */

#ifndef UA_SERVER_INTERNAL_H_
#define UA_SERVER_INTERNAL_H_

#define UA_INTERNAL
#include <open62541/server.h>
#include <open62541/plugin/nodestore.h>

#include "ua_session.h"
#include "ua_server_async.h"
#include "ua_util_internal.h"
#include "ziptree.h"

_UA_BEGIN_DECLS

#ifdef UA_ENABLE_PUBSUB
#include "ua_pubsub_manager.h"
#endif

#ifdef UA_ENABLE_DISCOVERY
#include "ua_discovery_manager.h"
#endif

#ifdef UA_ENABLE_SUBSCRIPTIONS
#include "ua_subscription.h"

typedef struct {
    UA_MonitoredItem monitoredItem;
    void *context;
    union {
        UA_Server_DataChangeNotificationCallback dataChangeCallback;
        /* UA_Server_EventNotificationCallback eventCallback; */
    } callback;
} UA_LocalMonitoredItem;

#endif

typedef enum {
    UA_DIAGNOSTICEVENT_CLOSE,
    UA_DIAGNOSTICEVENT_REJECT,
    UA_DIAGNOSTICEVENT_SECURITYREJECT,
    UA_DIAGNOSTICEVENT_TIMEOUT,
    UA_DIAGNOSTICEVENT_ABORT,
    UA_DIAGNOSTICEVENT_PURGE
} UA_DiagnosticEvent;

typedef struct channel_entry {
    TAILQ_ENTRY(channel_entry) pointers;
    UA_SecureChannel channel;
    UA_DiagnosticEvent closeEvent;
} channel_entry;

typedef struct session_list_entry {
    UA_DelayedCallback cleanupCallback;
    LIST_ENTRY(session_list_entry) pointers;
    UA_Session session;
} session_list_entry;

typedef enum {
    UA_SERVERLIFECYCLE_FRESH,
    UA_SERVERLIFECYCLE_STOPPED,
    UA_SERVERLIFECYCLE_STARTED,
    UA_SERVERLIFECYCLE_STOPPING
} UA_ServerLifecycle;

/* Maximum numbers of sockets to listen on */
#define UA_MAXSERVERCONNECTIONS 16

typedef struct {
    UA_ConnectionState state;
    uintptr_t connectionId;
    UA_ConnectionManager *connectionManager;
} UA_ServerConnection;

struct UA_Server {
    /* Config */
    UA_ServerConfig config;

    /* Runtime state */
    UA_DateTime startTime;
    UA_DateTime endTime; /* Zeroed out. If a time is set, then the server shuts
                          * down once the time has been reached */

    UA_ServerLifecycle state;
    UA_UInt64 houseKeepingCallbackId;

    UA_ServerConnection serverConnections[UA_MAXSERVERCONNECTIONS];
    size_t serverConnectionsSize;

    UA_ConnectionConfig tcpConnectionConfig; /* Extracted from the server config
                                              * parameters */

    /* SecureChannels */
    TAILQ_HEAD(, channel_entry) channels;
    UA_UInt32 lastChannelId;
    UA_UInt32 lastTokenId;

#if UA_MULTITHREADING >= 100
    UA_AsyncManager asyncManager;
#endif

    /* Session Management */
    LIST_HEAD(session_list, session_list_entry) sessions;
    UA_UInt32 sessionCount;
    UA_UInt32 activeSessionCount;
    UA_Session adminSession; /* Local access to the services (for startup and
                              * maintenance) uses this Session with all possible
                              * access rights (Session Id: 1) */

    /* Namespaces */
    size_t namespacesSize;
    UA_String *namespaces;

    /* For bootstrapping, omit some consistency checks, creating a reference to
     * the parent and member instantiation */
    UA_Boolean bootstrapNS0;

    /* Discovery */
#ifdef UA_ENABLE_DISCOVERY
    UA_DiscoveryManager discoveryManager;
#endif

    /* Subscriptions */
#ifdef UA_ENABLE_SUBSCRIPTIONS
    size_t subscriptionsSize;  /* Number of active subscriptions */
    size_t monitoredItemsSize; /* Number of active monitored items */
    LIST_HEAD(, UA_Subscription) subscriptions; /* All subscriptions in the
                                                 * server. They may be detached
                                                 * from a session. */
    UA_UInt32 lastSubscriptionId; /* To generate unique SubscriptionIds */

    /* To be cast to UA_LocalMonitoredItem to get the callback and context */
    LIST_HEAD(, UA_MonitoredItem) localMonitoredItems;
    UA_UInt32 lastLocalMonitoredItemId;

# ifdef UA_ENABLE_SUBSCRIPTIONS_ALARMS_CONDITIONS
    LIST_HEAD(, UA_ConditionSource) conditionSources;
# endif

#endif

    /* Publish/Subscribe */
#ifdef UA_ENABLE_PUBSUB
    UA_PubSubManager pubSubManager;
#endif

#if UA_MULTITHREADING >= 100
    UA_Lock serviceMutex;
#endif

    /* Statistics */
    UA_SecureChannelStatistics secureChannelStatistics;
    UA_ServerDiagnosticsSummaryDataType serverDiagnosticsSummary;
};

/***********************/
/* References Handling */
/***********************/

extern const struct aa_head refNameTree;

/**************************/
/* SecureChannel Handling */
/**************************/

/* Remove all securechannels */
void
UA_Server_deleteSecureChannels(UA_Server *server);

/* Remove timed out securechannels with a delayed callback. So all currently
 * scheduled jobs with a pointer to a securechannel can finish first. */
void
UA_Server_cleanupTimedOutSecureChannels(UA_Server *server, UA_DateTime nowMonotonic);


UA_StatusCode
sendServiceFault(UA_SecureChannel *channel, UA_UInt32 requestId,
                 UA_UInt32 requestHandle, UA_StatusCode statusCode);

/* Called only from within the network callback */
UA_StatusCode
createServerSecureChannel(UA_Server *server, UA_ConnectionManager *cm,
                          uintptr_t connectionId, UA_SecureChannel **outChannel);

UA_StatusCode
configServerSecureChannel(void *application, UA_SecureChannel *channel,
                          const UA_AsymmetricAlgorithmSecurityHeader *asymHeader);

/* Can be called at any time */
void
shutdownServerSecureChannel(UA_Server *server, UA_SecureChannel *channel,
                            UA_DiagnosticEvent event);

/* Called only from within the network callback */
void
deleteServerSecureChannel(UA_Server *server, UA_SecureChannel *channel);

/* Gets the a pointer to the context of a security policy supported by the
 * server matched by the security policy uri. */
UA_SecurityPolicy *
getSecurityPolicyByUri(const UA_Server *server,
                       const UA_ByteString *securityPolicyUri);


/********************/
/* Session Handling */
/********************/

UA_StatusCode
getNamespaceByName(UA_Server *server, const UA_String namespaceUri,
                   size_t *foundIndex);

UA_StatusCode
getNamespaceByIndex(UA_Server *server, const size_t namespaceIndex,
                    UA_String *foundUri);

UA_StatusCode
getBoundSession(UA_Server *server, const UA_SecureChannel *channel,
                const UA_NodeId *token, UA_Session **session);

UA_StatusCode
UA_Server_createSession(UA_Server *server, UA_SecureChannel *channel,
                        const UA_CreateSessionRequest *request, UA_Session **session);

void
UA_Server_removeSession(UA_Server *server, session_list_entry *sentry,
                        UA_DiagnosticEvent event);

UA_StatusCode
UA_Server_removeSessionByToken(UA_Server *server, const UA_NodeId *token,
                               UA_DiagnosticEvent event);

void
UA_Server_cleanupSessions(UA_Server *server, UA_DateTime nowMonotonic);

UA_Session *
getSessionByToken(UA_Server *server, const UA_NodeId *token);

UA_Session *
getSessionById(UA_Server *server, const UA_NodeId *sessionId);

/*****************/
/* Node Handling */
/*****************/

/* Calls the callback with the node retrieved from the nodestore on top of the
 * stack. Either a copy or the original node for in-situ editing. Depends on
 * multithreading and the nodestore.*/
typedef UA_StatusCode (*UA_EditNodeCallback)(UA_Server*, UA_Session*,
                                             UA_Node *node, void*);
UA_StatusCode UA_Server_editNode(UA_Server *server, UA_Session *session,
                                 const UA_NodeId *nodeId,
                                 UA_EditNodeCallback callback,
                                 void *data);

/*********************/
/* Utility Functions */
/*********************/

void setupNs1Uri(UA_Server *server);
UA_UInt16 addNamespace(UA_Server *server, const UA_String name);

UA_Boolean
UA_Node_hasSubTypeOrInstances(const UA_NodeHead *head);

/* Recursively searches "upwards" in the tree following specific reference types */
UA_Boolean
isNodeInTree(UA_Server *server, const UA_NodeId *leafNode,
             const UA_NodeId *nodeToFind, const UA_ReferenceTypeSet *relevantRefs);

/* Convenience function with just a single ReferenceTypeIndex */
UA_Boolean
isNodeInTree_singleRef(UA_Server *server, const UA_NodeId *leafNode,
                       const UA_NodeId *nodeToFind, const UA_Byte relevantRefTypeIndex);

/* Returns an array with the hierarchy of nodes. The start nodes can be returned
 * as well. The returned array starts at the leaf and continues "upwards" or
 * "downwards". Duplicate entries are removed. */
UA_StatusCode
browseRecursive(UA_Server *server, size_t startNodesSize, const UA_NodeId *startNodes,
                UA_BrowseDirection browseDirection, const UA_ReferenceTypeSet *refTypes,
                UA_UInt32 nodeClassMask, UA_Boolean includeStartNodes,
                size_t *resultsSize, UA_ExpandedNodeId **results);

/* Get the bitfield indices of a ReferenceType and possibly its subtypes.
 * refType must point to a ReferenceTypeNode. */
UA_StatusCode
referenceTypeIndices(UA_Server *server, const UA_NodeId *refType,
                     UA_ReferenceTypeSet *indices, UA_Boolean includeSubtypes);

/* Returns the recursive type and interface hierarchy of the node */
UA_StatusCode
getParentTypeAndInterfaceHierarchy(UA_Server *server, const UA_NodeId *typeNode,
                                   UA_NodeId **typeHierarchy, size_t *typeHierarchySize);

/* Returns the recursive interface hierarchy of the node */
UA_StatusCode
getAllInterfaceChildNodeIds(UA_Server *server, const UA_NodeId *objectNode, const UA_NodeId *objectTypeNode,
                                   UA_NodeId **interfaceChildNodes, size_t *interfaceChildNodesSize);

#ifdef UA_ENABLE_SUBSCRIPTIONS_ALARMS_CONDITIONS

UA_StatusCode
UA_getConditionId(UA_Server *server, const UA_NodeId *conditionNodeId,
                  UA_NodeId *outConditionId);

void
UA_ConditionList_delete(UA_Server *server);

UA_Boolean
isConditionOrBranch(UA_Server *server,
                    const UA_NodeId *condition,
                    const UA_NodeId *conditionSource,
                    UA_Boolean *isCallerAC);

#endif /* UA_ENABLE_SUBSCRIPTIONS_ALARMS_CONDITIONS */

/* Returns the type node from the node on the stack top. The type node is pushed
 * on the stack and returned. */
const UA_Node *
getNodeType(UA_Server *server, const UA_NodeHead *nodeHead);

UA_StatusCode
sendResponse(UA_Server *server, UA_Session *session, UA_SecureChannel *channel,
             UA_UInt32 requestId, UA_Response *response, const UA_DataType *responseType);

/* Many services come as an array of operations. This function generalizes the
 * processing of the operations. */
typedef void (*UA_ServiceOperation)(UA_Server *server, UA_Session *session,
                                    const void *context,
                                    const void *requestOperation,
                                    void *responseOperation);

UA_StatusCode
UA_Server_processServiceOperations(UA_Server *server, UA_Session *session,
                                   UA_ServiceOperation operationCallback,
                                   const void *context,
                                   const size_t *requestOperations,
                                   const UA_DataType *requestOperationsType,
                                   size_t *responseOperations,
                                   const UA_DataType *responseOperationsType)
    UA_FUNC_ATTR_WARN_UNUSED_RESULT;

/* Processing for the binary protocol (SecureChannel) */
void
UA_Server_networkCallback(UA_ConnectionManager *cm, uintptr_t connectionId,
                          void *application, void **connectionContext,
                          UA_ConnectionState state,
                          const UA_KeyValueMap *params,
                          UA_ByteString msg);

/******************************************/
/* Internal function calls, without locks */
/******************************************/
UA_StatusCode
deleteNode(UA_Server *server, const UA_NodeId nodeId,
           UA_Boolean deleteReferences);

UA_StatusCode
addNode(UA_Server *server, const UA_NodeClass nodeClass,
        const UA_NodeId *requestedNewNodeId,
        const UA_NodeId *parentNodeId, const UA_NodeId *referenceTypeId,
        const UA_QualifiedName browseName, const UA_NodeId *typeDefinition,
        const UA_NodeAttributes *attr, const UA_DataType *attributeType,
        void *nodeContext, UA_NodeId *outNewNodeId);

UA_StatusCode
addRef(UA_Server *server, UA_Session *session, const UA_NodeId *sourceId,
       const UA_NodeId *referenceTypeId, const UA_NodeId *targetId,
       UA_Boolean forward);

UA_StatusCode
setVariableNode_dataSource(UA_Server *server, const UA_NodeId nodeId,
                           const UA_DataSource dataSource);

UA_StatusCode
setVariableNode_valueCallback(UA_Server *server, const UA_NodeId nodeId,
                              const UA_ValueCallback callback);

UA_StatusCode
setMethodNode_callback(UA_Server *server, const UA_NodeId methodNodeId,
                       UA_MethodCallback methodCallback);

UA_StatusCode
writeAttribute(UA_Server *server, UA_Session *session,
               const UA_NodeId *nodeId, const UA_AttributeId attributeId,
               const void *attr, const UA_DataType *attr_type);

static UA_INLINE UA_StatusCode
writeValueAttribute(UA_Server *server, UA_Session *session,
                    const UA_NodeId *nodeId, const UA_Variant *value) {
    return writeAttribute(server, session, nodeId, UA_ATTRIBUTEID_VALUE,
                          value, &UA_TYPES[UA_TYPES_VARIANT]);
}

static UA_INLINE UA_StatusCode
writeIsAbstractAttribute(UA_Server *server, UA_Session *session,
                         const UA_NodeId *nodeId, UA_Boolean value) {
    return writeAttribute(server, session, nodeId, UA_ATTRIBUTEID_ISABSTRACT,
                          &value, &UA_TYPES[UA_TYPES_BOOLEAN]);
}

UA_DataValue
readAttribute(UA_Server *server, const UA_ReadValueId *item,
              UA_TimestampsToReturn timestamps);

UA_StatusCode
readWithReadValue(UA_Server *server, const UA_NodeId *nodeId,
                  const UA_AttributeId attributeId, void *v);

UA_StatusCode
readObjectProperty(UA_Server *server, const UA_NodeId objectId,
                   const UA_QualifiedName propertyName,
                   UA_Variant *value);

UA_BrowsePathResult
translateBrowsePathToNodeIds(UA_Server *server, const UA_BrowsePath *browsePath);

#ifdef UA_ENABLE_SUBSCRIPTIONS

void monitoredItem_sampleCallback(UA_Server *server, UA_MonitoredItem *monitoredItem);

UA_Subscription *
UA_Server_getSubscriptionById(UA_Server *server, UA_UInt32 subscriptionId);

#ifdef UA_ENABLE_SUBSCRIPTIONS_EVENTS

UA_StatusCode
createEvent(UA_Server *server, const UA_NodeId eventType,
            UA_NodeId *outNodeId);

UA_StatusCode
triggerEvent(UA_Server *server, const UA_NodeId eventNodeId,
             const UA_NodeId origin, UA_ByteString *outEventId,
             const UA_Boolean deleteEventNode);

/* Filters the given event with the given filter and writes the results into a
 * notification */
UA_StatusCode
filterEvent(UA_Server *server, UA_Session *session,
            const UA_NodeId *eventNode, UA_EventFilter *filter,
            UA_EventFieldList *efl, UA_EventFilterResult *result);

#endif /* UA_ENABLE_SUBSCRIPTIONS_EVENTS */

#endif /* UA_ENABLE_SUBSCRIPTIONS */

UA_BrowsePathResult
browseSimplifiedBrowsePath(UA_Server *server, const UA_NodeId origin,
                           size_t browsePathSize, const UA_QualifiedName *browsePath);

UA_StatusCode
writeObjectProperty(UA_Server *server, const UA_NodeId objectId,
                    const UA_QualifiedName propertyName, const UA_Variant value);

UA_StatusCode
writeObjectProperty_scalar(UA_Server *server, const UA_NodeId objectId,
                                     const UA_QualifiedName propertyName,
                                     const void *value, const UA_DataType *type);

UA_StatusCode
getNodeContext(UA_Server *server, UA_NodeId nodeId, void **nodeContext);

UA_StatusCode
setNodeContext(UA_Server *server, UA_NodeId nodeId, void *nodeContext);

void
removeCallback(UA_Server *server, UA_UInt64 callbackId);

UA_StatusCode
changeRepeatedCallbackInterval(UA_Server *server, UA_UInt64 callbackId,
                               UA_Double interval_ms);

UA_StatusCode
addRepeatedCallback(UA_Server *server, UA_ServerCallback callback,
                    void *data, UA_Double interval_ms, UA_UInt64 *callbackId);

#ifdef UA_ENABLE_DISCOVERY
UA_StatusCode
register_server_with_discovery_server(UA_Server *server, void *client,
                                      const UA_Boolean isUnregister,
                                      const char* semaphoreFilePath);
#endif

/***********/
/* RefTree */
/***********/

/* A RefTree is a sorted set of NodeIds that ensures we consider each node just
 * once. It holds a single array for both the ExpandedNodeIds and the entries of
 * a tree-structure for fast lookup. A single realloc operation (with some
 * pointer repairing) can be used to increase the capacity of the RefTree.
 *
 * When the RefTree is complete, the tree-part at the end of the targets array
 * can be ignored / cut away to use it as a simple ExpandedNodeId array.
 *
 * The layout of the targets array is as follows:
 *
 * | Targets [ExpandedNodeId, n times] | Tree [RefEntry, n times] | */

#define UA_REFTREE_INITIAL_SIZE 16

typedef struct RefEntry {
    ZIP_ENTRY(RefEntry) zipfields;
    const UA_ExpandedNodeId *target;
    UA_UInt32 targetHash; /* Hash of the target nodeid */
} RefEntry;

ZIP_HEAD(RefHead, RefEntry);
typedef struct RefHead RefHead;

typedef struct {
    UA_ExpandedNodeId *targets;
    RefHead head;
    size_t capacity; /* available space */
    size_t size;     /* used space */
} RefTree;

UA_StatusCode UA_FUNC_ATTR_WARN_UNUSED_RESULT
RefTree_init(RefTree *rt);

void RefTree_clear(RefTree *rt);

UA_StatusCode UA_FUNC_ATTR_WARN_UNUSED_RESULT
RefTree_addNodeId(RefTree *rt, const UA_NodeId *target, UA_Boolean *duplicate);

UA_Boolean
RefTree_contains(RefTree *rt, const UA_ExpandedNodeId *target);

UA_Boolean
RefTree_containsNodeId(RefTree *rt, const UA_NodeId *target);

/***************************************/
/* Check Information Model Consistency */
/***************************************/

/* Read a node attribute in the context of a "checked-out" node. So the
 * attribute will not be copied when possible. The variant then points into the
 * node and has UA_VARIANT_DATA_NODELETE set. */
void
ReadWithNode(const UA_Node *node, UA_Server *server, UA_Session *session,
             UA_TimestampsToReturn timestampsToReturn,
             const UA_ReadValueId *id, UA_DataValue *v);

UA_StatusCode
readValueAttribute(UA_Server *server, UA_Session *session,
                   const UA_VariableNode *vn, UA_DataValue *v);

/* Test whether the value matches a variable definition given by
 * - datatype
 * - valuerank
 * - array dimensions.
 * Sometimes it can be necessary to transform the content of the value, e.g.
 * byte array to bytestring or uint32 to some enum. If editableValue is non-NULL,
 * we try to create a matching variant that points to the original data.
 *
 * The reason is set whenever the return value is false */
UA_Boolean
compatibleValue(UA_Server *server, UA_Session *session, const UA_NodeId *targetDataTypeId,
                UA_Int32 targetValueRank, size_t targetArrayDimensionsSize,
                const UA_UInt32 *targetArrayDimensions, const UA_Variant *value,
                const UA_NumericRange *range, const char **reason);

/* Is the DataType compatible */
UA_Boolean
compatibleDataTypes(UA_Server *server, const UA_NodeId *dataType,
                    const UA_NodeId *constraintDataType);

/* Set to the target type if compatible */
void
adjustValueType(UA_Server *server, UA_Variant *value,
                const UA_NodeId *targetDataTypeId);

/* Is the Value compatible with the DataType? Can perform additional checks
 * compared to compatibleDataTypes. */
UA_Boolean
compatibleValueDataType(UA_Server *server, const UA_DataType *dataType,
                        const UA_NodeId *constraintDataType);


UA_Boolean
compatibleArrayDimensions(size_t constraintArrayDimensionsSize,
                          const UA_UInt32 *constraintArrayDimensions,
                          size_t testArrayDimensionsSize,
                          const UA_UInt32 *testArrayDimensions);

UA_Boolean
compatibleValueArrayDimensions(const UA_Variant *value, size_t targetArrayDimensionsSize,
                               const UA_UInt32 *targetArrayDimensions);

UA_Boolean
compatibleValueRankArrayDimensions(UA_Server *server, UA_Session *session,
                                   UA_Int32 valueRank, size_t arrayDimensionsSize);

UA_Boolean
compatibleValueRanks(UA_Int32 valueRank, UA_Int32 constraintValueRank);

struct BrowseOpts {
    UA_UInt32 maxReferences;
    UA_Boolean recursive;
};

void
Operation_Browse(UA_Server *server, UA_Session *session, const UA_UInt32 *maxrefs,
                 const UA_BrowseDescription *descr, UA_BrowseResult *result);

UA_DataValue
UA_Server_readWithSession(UA_Server *server, UA_Session *session,
                          const UA_ReadValueId *item,
                          UA_TimestampsToReturn timestampsToReturn);

/*****************************/
/* AddNodes Begin and Finish */
/*****************************/

/* Creates a new node in the nodestore. */
UA_StatusCode
AddNode_raw(UA_Server *server, UA_Session *session, void *nodeContext,
            const UA_AddNodesItem *item, UA_NodeId *outNewNodeId);

/* Check the reference to the parent node; Add references. */
UA_StatusCode
AddNode_addRefs(UA_Server *server, UA_Session *session, const UA_NodeId *nodeId,
                const UA_NodeId *parentNodeId, const UA_NodeId *referenceTypeId,
                const UA_NodeId *typeDefinitionId);

/* Type-check type-definition; Run the constructors */
UA_StatusCode
AddNode_finish(UA_Server *server, UA_Session *session, const UA_NodeId *nodeId);

/**********************/
/* Create Namespace 0 */
/**********************/

UA_StatusCode UA_Server_initNS0(UA_Server *server);

UA_StatusCode writeNs0VariableArray(UA_Server *server, UA_UInt32 id, void *v,
                      size_t length, const UA_DataType *type);

#ifdef UA_ENABLE_DIAGNOSTICS
void createSessionObject(UA_Server *server, UA_Session *session);

void createSubscriptionObject(UA_Server *server, UA_Session *session,
                              UA_Subscription *sub);

UA_StatusCode
readDiagnostics(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext,
                const UA_NodeId *nodeId, void *nodeContext, UA_Boolean sourceTimestamp,
                const UA_NumericRange *range, UA_DataValue *value);

UA_StatusCode
readSubscriptionDiagnosticsArray(UA_Server *server,
                                 const UA_NodeId *sessionId, void *sessionContext,
                                 const UA_NodeId *nodeId, void *nodeContext,
                                 UA_Boolean sourceTimestamp,
                                 const UA_NumericRange *range, UA_DataValue *value);

UA_StatusCode
readSessionDiagnosticsArray(UA_Server *server,
                            const UA_NodeId *sessionId, void *sessionContext,
                            const UA_NodeId *nodeId, void *nodeContext,
                            UA_Boolean sourceTimestamp,
                            const UA_NumericRange *range, UA_DataValue *value);

UA_StatusCode
readSessionSecurityDiagnostics(UA_Server *server,
                               const UA_NodeId *sessionId, void *sessionContext,
                               const UA_NodeId *nodeId, void *nodeContext,
                               UA_Boolean sourceTimestamp,
                               const UA_NumericRange *range, UA_DataValue *value);
#endif

/***************************/
/* Nodestore Access Macros */
/***************************/

#define UA_NODESTORE_NEW(server, nodeClass)                             \
    server->config.nodestore.newNode(server->config.nodestore.context, nodeClass)

#define UA_NODESTORE_DELETE(server, node)                               \
    server->config.nodestore.deleteNode(server->config.nodestore.context, node)

/* Get the node with all attributes and references */
static UA_INLINE const UA_Node *
UA_NODESTORE_GET(UA_Server *server, const UA_NodeId *nodeId) {
    return server->config.nodestore.
        getNode(server->config.nodestore.context, nodeId, UA_NODEATTRIBUTESMASK_ALL,
                UA_REFERENCETYPESET_ALL, UA_BROWSEDIRECTION_BOTH);
}

/* Get the node with all attributes and references */
static UA_INLINE const UA_Node *
UA_NODESTORE_GETFROMREF(UA_Server *server, UA_NodePointer target) {
    return server->config.nodestore.
        getNodeFromPtr(server->config.nodestore.context, target, UA_NODEATTRIBUTESMASK_ALL,
                       UA_REFERENCETYPESET_ALL, UA_BROWSEDIRECTION_BOTH);
}

#define UA_NODESTORE_GET_SELECTIVE(server, nodeid, attrMask, refs, refDirs) \
    server->config.nodestore.getNode(server->config.nodestore.context,      \
                                     nodeid, attrMask, refs, refDirs)

#define UA_NODESTORE_GETFROMREF_SELECTIVE(server, target, attrMask, refs, refDirs) \
    server->config.nodestore.getNodeFromPtr(server->config.nodestore.context,      \
                                            target, attrMask, refs, refDirs)

#define UA_NODESTORE_RELEASE(server, node)                              \
    server->config.nodestore.releaseNode(server->config.nodestore.context, node)

#define UA_NODESTORE_GETCOPY(server, nodeid, outnode)                      \
    server->config.nodestore.getNodeCopy(server->config.nodestore.context, \
                                         nodeid, outnode)

#define UA_NODESTORE_INSERT(server, node, addedNodeId)                    \
    server->config.nodestore.insertNode(server->config.nodestore.context, \
                                        node, addedNodeId)

#define UA_NODESTORE_REPLACE(server, node)                              \
    server->config.nodestore.replaceNode(server->config.nodestore.context, node)

#define UA_NODESTORE_REMOVE(server, nodeId)                             \
    server->config.nodestore.removeNode(server->config.nodestore.context, nodeId)

#define UA_NODESTORE_GETREFERENCETYPEID(server, index)                  \
    server->config.nodestore.getReferenceTypeId(server->config.nodestore.context, \
                                                index)

/* Handling of Locales */

/* Returns a shallow copy */
UA_LocalizedText
UA_Session_getNodeDisplayName(const UA_Session *session,
                              const UA_NodeHead *head);

UA_LocalizedText
UA_Session_getNodeDescription(const UA_Session *session,
                              const UA_NodeHead *head);

UA_StatusCode
UA_Node_insertOrUpdateDisplayName(UA_NodeHead *head,
                                  const UA_LocalizedText *value);

UA_StatusCode
UA_Node_insertOrUpdateDescription(UA_NodeHead *head,
                                  const UA_LocalizedText *value);

_UA_END_DECLS

#endif /* UA_SERVER_INTERNAL_H_ */
