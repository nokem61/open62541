/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2017-2022 Fraunhofer IOSB (Author: Andreas Ebner)
 * Copyright (c) 2019-2021 Kalycito Infotech Private Limited
 * Copyright (c) 2020 Yannick Wallerer, Siemens AG
 * Copyright (c) 2020-2022 Thomas Fischer, Siemens AG
 * Copyright (c) 2022 Linutronix GmbH (Author: Muddasir Shakil)
 */

#include "ua_pubsub_ns0.h"

#ifdef UA_ENABLE_PUBSUB_FILE_CONFIG
#include "ua_pubsub_config.h"
#endif

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL /* conditional compilation */

typedef struct {
    UA_NodeId parentNodeId;
    UA_UInt32 parentClassifier;
    UA_UInt32 elementClassiefier;
} UA_NodePropertyContext;

static UA_StatusCode
writePubSubNs0VariableArray(UA_Server *server, UA_UInt32 id, void *v,
                      size_t length, const UA_DataType *type) {
    UA_Variant var;
    UA_Variant_init(&var);
    UA_Variant_setArray(&var, v, length, type);
    return UA_Server_writeValue(server, UA_NODEID_NUMERIC(0, id), var);
}

static UA_NodeId
findSingleChildNode(UA_Server *server, UA_QualifiedName targetName,
                    UA_NodeId referenceTypeId, UA_NodeId startingNode){
    UA_NodeId resultNodeId;
    UA_RelativePathElement rpe;
    UA_RelativePathElement_init(&rpe);
    rpe.referenceTypeId = referenceTypeId;
    rpe.isInverse = false;
    rpe.includeSubtypes = false;
    rpe.targetName = targetName;
    UA_BrowsePath bp;
    UA_BrowsePath_init(&bp);
    bp.startingNode = startingNode;
    bp.relativePath.elementsSize = 1;
    bp.relativePath.elements = &rpe;
    UA_BrowsePathResult bpr = UA_Server_translateBrowsePathToNodeIds(server, &bp);
    if(bpr.statusCode != UA_STATUSCODE_GOOD ||
       bpr.targetsSize < 1)
        return UA_NODEID_NULL;
    UA_StatusCode res = UA_NodeId_copy(&bpr.targets[0].targetId.nodeId, &resultNodeId);
    if(res != UA_STATUSCODE_GOOD){
        UA_BrowsePathResult_clear(&bpr);
        return UA_NODEID_NULL;
    }
    UA_BrowsePathResult_clear(&bpr);
    return resultNodeId;
}

static void
onRead(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext,
       const UA_NodeId *nodeid, void *context,
       const UA_NumericRange *range, const UA_DataValue *data) {
    UA_Variant value;
    UA_Variant_init(&value);
    const UA_NodePropertyContext *nodeContext = (const UA_NodePropertyContext*)context;
    const UA_NodeId *myNodeId = &nodeContext->parentNodeId;
    UA_PublishedVariableDataType *pvd = NULL;
    UA_PublishedDataSet *publishedDataSet = NULL;

    switch(nodeContext->parentClassifier){
    case UA_NS0ID_PUBSUBCONNECTIONTYPE: {
        UA_PubSubConnection *pubSubConnection =
            UA_PubSubConnection_findConnectionbyId(server, *myNodeId);
        switch(nodeContext->elementClassiefier) {
        case UA_NS0ID_PUBSUBCONNECTIONTYPE_PUBLISHERID:
            switch (pubSubConnection->config->publisherIdType) {
                case UA_PUBLISHERIDTYPE_BYTE:
                    UA_Variant_setScalar(&value, &pubSubConnection->config->publisherId.byte,
                                         &UA_TYPES[UA_TYPES_BYTE]);
                    break;
                case UA_PUBLISHERIDTYPE_UINT16:
                    UA_Variant_setScalar(&value, &pubSubConnection->config->publisherId.uint16,
                                         &UA_TYPES[UA_TYPES_UINT16]);
                    break;
                case UA_PUBLISHERIDTYPE_UINT32:
                    UA_Variant_setScalar(&value, &pubSubConnection->config->publisherId.uint32,
                                         &UA_TYPES[UA_TYPES_UINT32]);
                    break;
                case UA_PUBLISHERIDTYPE_UINT64:
                    UA_Variant_setScalar(&value, &pubSubConnection->config->publisherId.uint64,
                                         &UA_TYPES[UA_TYPES_UINT64]);
                    break;
                case UA_PUBLISHERIDTYPE_STRING:
                    UA_Variant_setScalar(&value, &pubSubConnection->config->publisherId.string,
                                         &UA_TYPES[UA_TYPES_STRING]);
                    break;
                default:
                    UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                           "Read error! Unknown PublisherId type.");
                }
            break;
        default:
            UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                           "Read error! Unknown property.");
        }
        break;
    }
    case UA_NS0ID_DATASETREADERTYPE: {
        UA_DataSetReader *dataSetReader = UA_ReaderGroup_findDSRbyId(server, *myNodeId);
        if(!dataSetReader)
            return;

        switch(nodeContext->elementClassiefier) {
        case UA_NS0ID_DATASETREADERTYPE_PUBLISHERID:
            UA_Variant_setScalar(&value, dataSetReader->config.publisherId.data,
                                 dataSetReader->config.publisherId.type);
            break;
        default:
            UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                           "Read error! Unknown property.");
        }
        break;
    }
    case UA_NS0ID_WRITERGROUPTYPE: {
        UA_WriterGroup *writerGroup = UA_WriterGroup_findWGbyId(server, *myNodeId);
        if(!writerGroup)
            return;
        switch(nodeContext->elementClassiefier){
        case UA_NS0ID_WRITERGROUPTYPE_PUBLISHINGINTERVAL:
            UA_Variant_setScalar(&value, &writerGroup->config.publishingInterval,
                                 &UA_TYPES[UA_TYPES_DURATION]);
            break;
        default:
            UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                           "Read error! Unknown property.");
        }
        break;
    }
    case UA_NS0ID_DATASETWRITERTYPE: {
        UA_DataSetWriter *dataSetWriter = UA_DataSetWriter_findDSWbyId(server, *myNodeId);
        if(!dataSetWriter)
            return;

        switch(nodeContext->elementClassiefier) {
            case UA_NS0ID_DATASETWRITERTYPE_DATASETWRITERID:
                UA_Variant_setScalar(&value, &dataSetWriter->config.dataSetWriterId,
                                     &UA_TYPES[UA_TYPES_UINT16]);
                break;
            default:
                UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                               "Read error! Unknown property.");
        }
        break;
    }
    case UA_NS0ID_PUBLISHEDDATAITEMSTYPE: {
        publishedDataSet = UA_PublishedDataSet_findPDSbyId(server, *myNodeId);
        if(!publishedDataSet)
            return;
        switch(nodeContext->elementClassiefier) {
        case UA_NS0ID_PUBLISHEDDATAITEMSTYPE_PUBLISHEDDATA: {
            pvd = (UA_PublishedVariableDataType *)
                UA_calloc(publishedDataSet->fieldSize, sizeof(UA_PublishedVariableDataType));
            size_t counter = 0;
            UA_DataSetField *field;
            TAILQ_FOREACH(field, &publishedDataSet->fields, listEntry) {
                pvd[counter].attributeId = UA_ATTRIBUTEID_VALUE;
                pvd[counter].publishedVariable = field->config.field.variable.publishParameters.publishedVariable;
                //UA_NodeId_copy(&field->config.field.variable.publishParameters.publishedVariable, &pvd[counter].publishedVariable);
                counter++;
            }
            UA_Variant_setArray(&value, pvd, publishedDataSet->fieldSize,
                                &UA_TYPES[UA_TYPES_PUBLISHEDVARIABLEDATATYPE]);
            break;
        }
        case UA_NS0ID_PUBLISHEDDATAITEMSTYPE_DATASETMETADATA: {
            UA_Variant_setScalar(&value, &publishedDataSet->dataSetMetaData, &UA_TYPES[UA_TYPES_DATASETMETADATATYPE]);
            break;
        }
        case UA_NS0ID_PUBLISHEDDATAITEMSTYPE_CONFIGURATIONVERSION: {
            UA_Variant_setScalar(&value, &publishedDataSet->dataSetMetaData.configurationVersion,
                                     &UA_TYPES[UA_TYPES_CONFIGURATIONVERSIONDATATYPE]);
            break;
        }
        case UA_NS0ID_STANDALONESUBSCRIBEDDATASETREFDATATYPE: {
            UA_StandaloneSubscribedDataSet *sds = UA_StandaloneSubscribedDataSet_findSDSbyId(server, *myNodeId);
            switch(nodeContext->elementClassiefier) {
                case UA_NS0ID_STANDALONESUBSCRIBEDDATASETTYPE_ISCONNECTED: {
                    UA_Variant_setScalar(&value, &sds->config.isConnected, &UA_TYPES[UA_TYPES_BOOLEAN]);
                    break;
                }
                case UA_NS0ID_STANDALONESUBSCRIBEDDATASETTYPE_DATASETMETADATA: {
                    UA_Variant_setScalar(&value, &sds->config.dataSetMetaData, &UA_TYPES[UA_TYPES_DATASETMETADATATYPE]);
                    break;
                }
                default:
                UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                                "Read error! Unknown property.");
            }
            break;
        }
        default:
            UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                           "Read error! Unknown property.");
        }
        break;
    }
    default:
        UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                       "Read error! Unknown parent element.");
    }
    UA_Server_writeValue(server, *nodeid, value);
    if(pvd && publishedDataSet) {
        UA_Array_delete(pvd, publishedDataSet->fieldSize, &UA_TYPES[UA_TYPES_PUBLISHEDVARIABLEDATATYPE]);
    }
}

static void
onWrite(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext,
        const UA_NodeId *nodeId, void *nodeContext,
        const UA_NumericRange *range, const UA_DataValue *data){
    UA_Variant value;
    UA_NodeId myNodeId;
    UA_WriterGroup *writerGroup = NULL;
    switch(((UA_NodePropertyContext *) nodeContext)->parentClassifier){
        case UA_NS0ID_PUBSUBCONNECTIONTYPE:
            //no runtime writable attributes
            break;
        case UA_NS0ID_WRITERGROUPTYPE:
            myNodeId = ((UA_NodePropertyContext *) nodeContext)->parentNodeId;
            writerGroup = UA_WriterGroup_findWGbyId(server, myNodeId);
            UA_WriterGroupConfig writerGroupConfig;
            memset(&writerGroupConfig, 0, sizeof(writerGroupConfig));
            if(!writerGroup)
                return;
            switch(((UA_NodePropertyContext *) nodeContext)->elementClassiefier){
                case UA_NS0ID_WRITERGROUPTYPE_PUBLISHINGINTERVAL:
                    UA_Server_getWriterGroupConfig(server, writerGroup->identifier, &writerGroupConfig);
                    writerGroupConfig.publishingInterval = *((UA_Duration *) data->value.data);
                    UA_Server_updateWriterGroupConfig(server, writerGroup->identifier, &writerGroupConfig);
                    UA_Variant_setScalar(&value, data->value.data, &UA_TYPES[UA_TYPES_DURATION]);
                    UA_WriterGroupConfig_clear(&writerGroupConfig);
                    break;
                default:
                    UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                                   "Write error! Unknown property element.");
            }
            break;
        default:
            UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                           "Read error! Unknown parent element.");
    }
}

static UA_StatusCode
addVariableValueSource(UA_Server *server, UA_ValueCallback valueCallback,
                       UA_NodeId node, UA_NodePropertyContext *context){
    UA_Server_setNodeContext(server, node, context);
    return UA_Server_setVariableNode_valueCallback(server, node, valueCallback);
}

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
static UA_StatusCode
addPubSubConnectionConfig(UA_Server *server, UA_PubSubConnectionDataType *pubsubConnection,
                          UA_NodeId *connectionId){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_NetworkAddressUrlDataType networkAddressUrl;
    memset(&networkAddressUrl, 0, sizeof(networkAddressUrl));
    UA_ExtensionObject *eo = &pubsubConnection->address;
    if(eo->encoding == UA_EXTENSIONOBJECT_DECODED &&
       eo->content.decoded.type == &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]) {
        void *data = eo->content.decoded.data;
        retVal =
            UA_NetworkAddressUrlDataType_copy((UA_NetworkAddressUrlDataType *)data,
                                              &networkAddressUrl);
        if(retVal != UA_STATUSCODE_GOOD)
            return retVal;
    }

    UA_PubSubConnectionConfig connectionConfig;
    memset(&connectionConfig, 0, sizeof(UA_PubSubConnectionConfig));
    connectionConfig.transportProfileUri = pubsubConnection->transportProfileUri;
    connectionConfig.name = pubsubConnection->name;
    //TODO set real connection state
    connectionConfig.enabled = pubsubConnection->enabled;
    //connectionConfig.enabled = pubSubConnection.enabled;
    UA_Variant_setScalar(&connectionConfig.address, &networkAddressUrl,
                         &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);

    if (pubsubConnection->publisherId.type == &UA_TYPES[UA_TYPES_BYTE]) {
        connectionConfig.publisherIdType = UA_PUBLISHERIDTYPE_BYTE;
        connectionConfig.publisherId.byte = *((UA_Byte*)pubsubConnection->publisherId.data);
    } else if (pubsubConnection->publisherId.type == &UA_TYPES[UA_TYPES_UINT16]) {
        connectionConfig.publisherIdType = UA_PUBLISHERIDTYPE_UINT16;
        connectionConfig.publisherId.uint16 = *((UA_UInt16*)pubsubConnection->publisherId.data);
    } else if (pubsubConnection->publisherId.type == &UA_TYPES[UA_TYPES_UINT32]) {
        connectionConfig.publisherIdType = UA_PUBLISHERIDTYPE_UINT32;
        connectionConfig.publisherId.uint32 = *((UA_UInt32*)pubsubConnection->publisherId.data);
    } else if (pubsubConnection->publisherId.type == &UA_TYPES[UA_TYPES_UINT64]) {
        connectionConfig.publisherIdType = UA_PUBLISHERIDTYPE_UINT64;
        connectionConfig.publisherId.uint64 = *((UA_UInt64*)pubsubConnection->publisherId.data);
    } else if (pubsubConnection->publisherId.type == &UA_TYPES[UA_TYPES_STRING]) {
        connectionConfig.publisherIdType = UA_PUBLISHERIDTYPE_STRING;
        UA_String_copy((UA_String *) pubsubConnection->publisherId.data, &connectionConfig.publisherId.string);
    } else {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER, "Unsupported PublisherId Type used.");
        return UA_STATUSCODE_BADCONFIGURATIONERROR;
    }

    retVal |= UA_Server_addPubSubConnection(server, &connectionConfig, connectionId);
    UA_NetworkAddressUrlDataType_clear(&networkAddressUrl);
    return retVal;
}

/**
 * **WriterGroup handling**
 *
 * The WriterGroup (WG) is part of the connection and contains the primary
 * configuration parameters for the message creation. */
static UA_StatusCode
addWriterGroupConfig(UA_Server *server, UA_NodeId connectionId,
                     UA_WriterGroupDataType *writerGroup, UA_NodeId *writerGroupId){
    /* Now we create a new WriterGroupConfig and add the group to the existing
     * PubSubConnection. */
    UA_WriterGroupConfig writerGroupConfig;
    memset(&writerGroupConfig, 0, sizeof(UA_WriterGroupConfig));
    writerGroupConfig.name = writerGroup->name;
    writerGroupConfig.publishingInterval = writerGroup->publishingInterval;
    writerGroupConfig.enabled = writerGroup->enabled;
    writerGroupConfig.writerGroupId = writerGroup->writerGroupId;
    //TODO remove hard coded UADP
    writerGroupConfig.encodingMimeType = UA_PUBSUB_ENCODING_UADP;
    writerGroupConfig.priority = writerGroup->priority;

    UA_UadpWriterGroupMessageDataType writerGroupMessage;
    UA_ExtensionObject *eoWG = &writerGroup->messageSettings;
    if(eoWG->encoding == UA_EXTENSIONOBJECT_DECODED){
        writerGroupConfig.messageSettings.encoding  = UA_EXTENSIONOBJECT_DECODED;
        if(eoWG->content.decoded.type == &UA_TYPES[UA_TYPES_UADPWRITERGROUPMESSAGEDATATYPE]){
            if(UA_UadpWriterGroupMessageDataType_copy((UA_UadpWriterGroupMessageDataType *) eoWG->content.decoded.data,
                                                        &writerGroupMessage) != UA_STATUSCODE_GOOD){
                return UA_STATUSCODE_BADOUTOFMEMORY;
            }
            writerGroupConfig.messageSettings.content.decoded.type = &UA_TYPES[UA_TYPES_UADPWRITERGROUPMESSAGEDATATYPE];
            writerGroupConfig.messageSettings.content.decoded.data = &writerGroupMessage;
        }
    }

    return UA_Server_addWriterGroup(server, connectionId, &writerGroupConfig, writerGroupId);
}

/**
 * **DataSetWriter handling**
 *
 * A DataSetWriter (DSW) is the glue between the WG and the PDS. The DSW is
 * linked to exactly one PDS and contains additional informations for the
 * message generation. */
static UA_StatusCode
addDataSetWriterConfig(UA_Server *server, const UA_NodeId *writerGroupId,
                       UA_DataSetWriterDataType *dataSetWriter,
                       UA_NodeId *dataSetWriterId) {
    UA_NodeId publishedDataSetId = UA_NODEID_NULL;
    UA_PublishedDataSet *tmpPDS;
    TAILQ_FOREACH(tmpPDS, &server->pubSubManager.publishedDataSets, listEntry){
        if(UA_String_equal(&dataSetWriter->dataSetName, &tmpPDS->config.name)) {
            publishedDataSetId = tmpPDS->identifier;
            break;
        }
    }

    if(UA_NodeId_isNull(&publishedDataSetId))
        return UA_STATUSCODE_BADPARENTNODEIDINVALID;

    /* We need now a DataSetWriter within the WriterGroup. This means we must
     * create a new DataSetWriterConfig and add call the addWriterGroup function. */
    UA_DataSetWriterConfig dataSetWriterConfig;
    memset(&dataSetWriterConfig, 0, sizeof(UA_DataSetWriterConfig));
    dataSetWriterConfig.name = dataSetWriter->name;
    dataSetWriterConfig.dataSetWriterId = dataSetWriter->dataSetWriterId;
    dataSetWriterConfig.keyFrameCount = dataSetWriter->keyFrameCount;
    dataSetWriterConfig.dataSetFieldContentMask =  dataSetWriter->dataSetFieldContentMask;
    return UA_Server_addDataSetWriter(server, *writerGroupId, publishedDataSetId,
                                      &dataSetWriterConfig, dataSetWriterId);
}

/**
 * **ReaderGroup**
 *
 * ReaderGroup is used to group a list of DataSetReaders. All ReaderGroups are
 * created within a PubSubConnection and automatically deleted if the connection
 * is removed. All network message related filters are only available in the DataSetReader. */
/* Add ReaderGroup to the created connection */
static UA_StatusCode
addReaderGroupConfig(UA_Server *server, UA_NodeId connectionId,
                     UA_ReaderGroupDataType *readerGroup,
                     UA_NodeId *readerGroupId) {
    UA_ReaderGroupConfig readerGroupConfig;
    memset(&readerGroupConfig, 0, sizeof(UA_ReaderGroupConfig));
    readerGroupConfig.name = readerGroup->name;
    return UA_Server_addReaderGroup(server, connectionId,
                                    &readerGroupConfig, readerGroupId);
}

/**
 * **SubscribedDataSet**
 *
 * Set SubscribedDataSet type to TargetVariables data type.
 * Add subscribedvariables to the DataSetReader */
static UA_StatusCode
addSubscribedVariables(UA_Server *server, UA_NodeId dataSetReaderId,
                       UA_DataSetReaderDataType *dataSetReader,
                       UA_DataSetMetaDataType *pMetaData) {
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_ExtensionObject *eoTargetVar = &dataSetReader->subscribedDataSet;
    if(eoTargetVar->encoding != UA_EXTENSIONOBJECT_DECODED ||
       eoTargetVar->content.decoded.type != &UA_TYPES[UA_TYPES_TARGETVARIABLESDATATYPE])
        return UA_STATUSCODE_BADUNEXPECTEDERROR;

    const UA_TargetVariablesDataType *targetVars =
        (UA_TargetVariablesDataType*)eoTargetVar->content.decoded.data;

    UA_NodeId folderId;
    UA_String folderName = pMetaData->name;
    UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
    UA_QualifiedName folderBrowseName;
    if(folderName.length > 0) {
        oAttr.displayName.locale = UA_STRING("");
        oAttr.displayName.text = folderName;
        folderBrowseName.namespaceIndex = 1;
        folderBrowseName.name = folderName;
    } else {
        oAttr.displayName = UA_LOCALIZEDTEXT("", "Subscribed Variables");
        folderBrowseName = UA_QUALIFIEDNAME(1, "Subscribed Variables");
    }

    UA_Server_addObjectNode(server, UA_NODEID_NULL,
                            UA_NODEID_NUMERIC (0, UA_NS0ID_OBJECTSFOLDER),
                            UA_NODEID_NUMERIC (0, UA_NS0ID_ORGANIZES),
                            folderBrowseName,
                            UA_NODEID_NUMERIC (0, UA_NS0ID_BASEOBJECTTYPE),
                            oAttr, NULL, &folderId);

    /* The SubscribedDataSet option TargetVariables defines a list of Variable
     * mappings between received DataSet fields and target Variables in the
     * Subscriber AddressSpace. The values subscribed from the Publisher are
     * updated in the value field of these variables */

    /* Create the TargetVariables with respect to DataSetMetaData fields */
    UA_FieldTargetVariable *targetVarsData = (UA_FieldTargetVariable *)
        UA_calloc(targetVars->targetVariablesSize, sizeof(UA_FieldTargetVariable));
    for(size_t i = 0; i < targetVars->targetVariablesSize; i++) {
        /* Prepare the output structure */
        UA_FieldTargetDataType_init(&targetVarsData[i].targetVariable);
        targetVarsData[i].targetVariable.attributeId  = targetVars->targetVariables[i].attributeId;

        /* Add variable for the field */
        UA_VariableAttributes vAttr = UA_VariableAttributes_default;
        vAttr.description = pMetaData->fields[i].description;
        vAttr.displayName.locale = UA_STRING("");
        vAttr.displayName.text = pMetaData->fields[i].name;
        vAttr.dataType = pMetaData->fields[i].dataType;
        UA_QualifiedName varname = {1, pMetaData->fields[i].name};
        retVal |= UA_Server_addVariableNode(server, targetVars->targetVariables[i].targetNodeId,
                                            folderId,
                                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                            varname,
                                            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                            vAttr, NULL,
                                            &targetVarsData[i].targetVariable.targetNodeId);

    }
    retVal = UA_Server_DataSetReader_createTargetVariables(server, dataSetReaderId,
                                                           targetVars->targetVariablesSize,
                                                           targetVarsData);
    for(size_t j = 0; j < targetVars->targetVariablesSize; j++)
        UA_FieldTargetDataType_clear(&targetVarsData[j].targetVariable);
    UA_free(targetVarsData);
    return retVal;
}

/**
 * **DataSetReader**
 *
 * DataSetReader can receive NetworkMessages with the DataSetMessage
 * of interest sent by the Publisher. DataSetReader provides
 * the configuration necessary to receive and process DataSetMessages
 * on the Subscriber side. DataSetReader must be linked with a
 * SubscribedDataSet and be contained within a ReaderGroup. */
/* Add DataSetReader to the ReaderGroup */
static UA_StatusCode
addDataSetReaderConfig(UA_Server *server, UA_NodeId readerGroupId,
                       UA_DataSetReaderDataType *dataSetReader,
                       UA_NodeId *dataSetReaderId) {
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_DataSetReaderConfig readerConfig;
    memset(&readerConfig, 0, sizeof(UA_DataSetReaderConfig));
    readerConfig.name = dataSetReader->name;
    readerConfig.publisherId = dataSetReader->publisherId;
    readerConfig.writerGroupId = dataSetReader->writerGroupId;
    readerConfig.dataSetWriterId = dataSetReader->dataSetWriterId;

    /* Setting up Meta data configuration in DataSetReader */
    UA_DataSetMetaDataType *pMetaData;
    pMetaData = &readerConfig.dataSetMetaData;
    UA_DataSetMetaDataType_init (pMetaData);
    pMetaData->name =  dataSetReader->dataSetMetaData.name;
    pMetaData->fieldsSize = dataSetReader->dataSetMetaData.fieldsSize;
    pMetaData->fields = (UA_FieldMetaData*)UA_Array_new (pMetaData->fieldsSize,
                        &UA_TYPES[UA_TYPES_FIELDMETADATA]);
    for(size_t i = 0; i < pMetaData->fieldsSize; i++){
        UA_FieldMetaData_init (&pMetaData->fields[i]);
        UA_NodeId_copy (&dataSetReader->dataSetMetaData.fields[i].dataType,
                        &pMetaData->fields[i].dataType);
        pMetaData->fields[i].builtInType = dataSetReader->dataSetMetaData.fields[i].builtInType;
        pMetaData->fields[i].name = dataSetReader->dataSetMetaData.fields[i].name;
        pMetaData->fields[i].valueRank = dataSetReader->dataSetMetaData.fields[i].valueRank;
    }

    retVal |= UA_Server_addDataSetReader(server, readerGroupId,
                                         &readerConfig, dataSetReaderId);
    if(retVal != UA_STATUSCODE_GOOD) {
        UA_free(pMetaData->fields);
        return retVal;
    }

    retVal |= addSubscribedVariables(server, *dataSetReaderId, dataSetReader, pMetaData);
    UA_free(pMetaData->fields);
    return retVal;
}
#endif

/*************************************************/
/*            PubSubConnection                   */
/*************************************************/
UA_StatusCode
addPubSubConnectionRepresentation(UA_Server *server, UA_PubSubConnection *connection) {
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    if(connection->config->name.length > 512)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    char connectionName[513];
    memcpy(connectionName, connection->config->name.data, connection->config->name.length);
    connectionName[connection->config->name.length] = '\0';

    UA_ObjectAttributes attr = UA_ObjectAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT("", connectionName);
    retVal |= UA_Server_addNode_begin(server, UA_NODECLASS_OBJECT,
                                      UA_NODEID_NUMERIC(1, 0), /* Generate a new id */
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE),
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_HASPUBSUBCONNECTION),
                                      UA_QUALIFIEDNAME(0, connectionName),
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_PUBSUBCONNECTIONTYPE),
                                      (const UA_NodeAttributes*)&attr,
                                      &UA_TYPES[UA_TYPES_OBJECTATTRIBUTES],
                                      NULL, &connection->identifier);

    attr.displayName = UA_LOCALIZEDTEXT("", "Address");
    retVal |= UA_Server_addObjectNode(server, UA_NODEID_NUMERIC(1, 0),
                                   connection->identifier,
                                   UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                   UA_QUALIFIEDNAME(0, "Address"),
                                   UA_NODEID_NUMERIC(0, UA_NS0ID_NETWORKADDRESSURLTYPE),
                                   attr, NULL, NULL);

    UA_Server_addNode_finish(server, connection->identifier);

    UA_NodeId addressNode, urlNode, interfaceNode, publisherIdNode,
        connectionPropertieNode, transportProfileUri;
    addressNode = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "Address"),
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                      connection->identifier);
    urlNode = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "Url"),
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                  addressNode);
    interfaceNode = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "NetworkInterface"),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                        addressNode);
    publisherIdNode = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "PublisherId"),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                                        connection->identifier);
    connectionPropertieNode = findSingleChildNode(server,
                                                  UA_QUALIFIEDNAME(0, "ConnectionProperties"),
                                                  UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                                                  connection->identifier);
    transportProfileUri = findSingleChildNode(server,
                                              UA_QUALIFIEDNAME(0, "TransportProfileUri"),
                                              UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                              connection->identifier);

    if(UA_NodeId_isNull(&addressNode) || UA_NodeId_isNull(&urlNode) ||
       UA_NodeId_isNull(&interfaceNode) || UA_NodeId_isNull(&publisherIdNode) ||
       UA_NodeId_isNull(&connectionPropertieNode) ||
       UA_NodeId_isNull(&transportProfileUri)) {
        return UA_STATUSCODE_BADNOTFOUND;
    }

    retVal |= writePubSubNs0VariableArray(server, connectionPropertieNode.identifier.numeric,
                                          connection->config->connectionProperties,
                                          connection->config->connectionPropertiesSize,
                                          &UA_TYPES[UA_TYPES_KEYVALUEPAIR]);

    UA_NetworkAddressUrlDataType *networkAddressUrl=
        ((UA_NetworkAddressUrlDataType*)connection->config->address.data);
    UA_Variant value;
    UA_Variant_init(&value);
    UA_Variant_setScalar(&value, &networkAddressUrl->url,
                         &UA_TYPES[UA_TYPES_STRING]);
    UA_Server_writeValue(server, urlNode, value);
    UA_Variant_setScalar(&value, &networkAddressUrl->networkInterface,
                         &UA_TYPES[UA_TYPES_STRING]);
    UA_Server_writeValue(server, interfaceNode, value);
    UA_Variant_setScalar(&value, &connection->config->transportProfileUri,
                         &UA_TYPES[UA_TYPES_STRING]);
    UA_Server_writeValue(server, transportProfileUri, value);

    UA_NodePropertyContext *connectionPublisherIdContext = (UA_NodePropertyContext *)
        UA_malloc(sizeof(UA_NodePropertyContext));
    connectionPublisherIdContext->parentNodeId = connection->identifier;
    connectionPublisherIdContext->parentClassifier = UA_NS0ID_PUBSUBCONNECTIONTYPE;
    connectionPublisherIdContext->elementClassiefier =
        UA_NS0ID_PUBSUBCONNECTIONTYPE_PUBLISHERID;
    UA_ValueCallback valueCallback;
    valueCallback.onRead = onRead;
    valueCallback.onWrite = NULL;
    retVal |= addVariableValueSource(server, valueCallback, publisherIdNode,
                                     connectionPublisherIdContext);

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
    retVal |= UA_Server_addReference(server, connection->identifier,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_PUBSUBCONNECTIONTYPE_ADDWRITERGROUP),
                                     true);
    retVal |= UA_Server_addReference(server, connection->identifier,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_PUBSUBCONNECTIONTYPE_ADDREADERGROUP),
                                     true);
    retVal |= UA_Server_addReference(server, connection->identifier,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_PUBSUBCONNECTIONTYPE_REMOVEGROUP),
                                     true);
#endif
    return retVal;
}

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
static UA_StatusCode
addPubSubConnectionAction(UA_Server *server,
                          const UA_NodeId *sessionId, void *sessionHandle,
                          const UA_NodeId *methodId, void *methodContext,
                          const UA_NodeId *objectId, void *objectContext,
                          size_t inputSize, const UA_Variant *input,
                          size_t outputSize, UA_Variant *output) {
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_PubSubConnectionDataType *pubSubConnection =
        (UA_PubSubConnectionDataType *) input[0].data;

    //call API function and create the connection
    UA_NodeId connectionId;
    retVal |= addPubSubConnectionConfig(server, pubSubConnection, &connectionId);
    if(retVal != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "addPubSubConnection failed");
        return retVal;
    }

    for(size_t i = 0; i < pubSubConnection->writerGroupsSize; i++) {
        UA_NodeId writerGroupId;
        UA_WriterGroupDataType *writerGroup = &pubSubConnection->writerGroups[i];
        retVal |= addWriterGroupConfig(server, connectionId, writerGroup, &writerGroupId);
        if(retVal != UA_STATUSCODE_GOOD) {
            UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                         "addWriterGroup failed");
            return retVal;
        }

        for(size_t j = 0; j < writerGroup->dataSetWritersSize; j++) {
            UA_DataSetWriterDataType *dataSetWriter = &writerGroup->dataSetWriters[j];
            retVal |= addDataSetWriterConfig(server, &writerGroupId, dataSetWriter, NULL);
            if(retVal != UA_STATUSCODE_GOOD) {
                UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                             "addDataSetWriter failed");
                return retVal;
            }
        }

        /* TODO: Need to handle the UA_Server_setWriterGroupOperational based on
         * the status variable in information model */
        if(pubSubConnection->enabled) {
            UA_Server_freezeWriterGroupConfiguration(server, writerGroupId);
            UA_Server_setWriterGroupOperational(server, writerGroupId);
        } else {
            UA_Server_setWriterGroupDisabled(server, writerGroupId);
        }
    }

    for(size_t i = 0; i < pubSubConnection->readerGroupsSize; i++){
        UA_NodeId readerGroupId;
        UA_ReaderGroupDataType *readerGroup = &pubSubConnection->readerGroups[i];
        retVal |= addReaderGroupConfig(server, connectionId, readerGroup, &readerGroupId);
        if(retVal != UA_STATUSCODE_GOOD) {
            UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                         "addReaderGroup failed");
            return retVal;
        }

        for(size_t j = 0; j < readerGroup->dataSetReadersSize; j++) {
            UA_NodeId dataSetReaderId;
            UA_DataSetReaderDataType *dataSetReader = &readerGroup->dataSetReaders[j];
            retVal |= addDataSetReaderConfig(server, readerGroupId,
                                             dataSetReader, &dataSetReaderId);
            if(retVal != UA_STATUSCODE_GOOD) {
                UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                             "addDataSetReader failed");
                return retVal;
            }

        }

        /* TODO: Need to handle the UA_Server_setReaderGroupOperational based on
         * the status variable in information model */
        if(pubSubConnection->enabled) {
            UA_Server_freezeReaderGroupConfiguration(server, readerGroupId);
            UA_Server_setReaderGroupOperational(server, readerGroupId);
        } else {
            UA_Server_setReaderGroupDisabled(server, readerGroupId);
        }
    }

    /* Set ouput value */
    UA_Variant_setScalarCopy(output, &connectionId, &UA_TYPES[UA_TYPES_NODEID]);
    return UA_STATUSCODE_GOOD;
}
#endif

UA_StatusCode
removePubSubConnectionRepresentation(UA_Server *server,
                                     UA_PubSubConnection *connection) {
    return deleteNode(server, connection->identifier, true);
}

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
static UA_StatusCode
removeConnectionAction(UA_Server *server,
                       const UA_NodeId *sessionId, void *sessionHandle,
                       const UA_NodeId *methodId, void *methodContext,
                       const UA_NodeId *objectId, void *objectContext,
                       size_t inputSize, const UA_Variant *input,
                       size_t outputSize, UA_Variant *output){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_NodeId nodeToRemove = *((UA_NodeId *) input[0].data);
    retVal |= UA_Server_removePubSubConnection(server, nodeToRemove);
    if(retVal == UA_STATUSCODE_BADNOTFOUND)
        retVal = UA_STATUSCODE_BADNODEIDUNKNOWN;
    return retVal;
}
#endif

/**********************************************/
/*               DataSetReader                */
/**********************************************/
UA_StatusCode
addDataSetReaderRepresentation(UA_Server *server, UA_DataSetReader *dataSetReader){
    if(dataSetReader->config.name.length > 512)
        return UA_STATUSCODE_BADCONFIGURATIONERROR;

    char dsrName[513];
    memcpy(dsrName, dataSetReader->config.name.data, dataSetReader->config.name.length);
    dsrName[dataSetReader->config.name.length] = '\0';

    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_NodeId publisherIdNode, writerGroupIdNode, dataSetwriterIdNode;

    UA_ObjectAttributes object_attr = UA_ObjectAttributes_default;
    object_attr.displayName = UA_LOCALIZEDTEXT("", dsrName);
    retVal = UA_Server_addObjectNode(server, UA_NODEID_NUMERIC(1, 0), /* create an id */
                                   dataSetReader->linkedReaderGroup,
                                   UA_NODEID_NUMERIC(0, UA_NS0ID_HASDATASETREADER),
                                   UA_QUALIFIEDNAME(0, dsrName),
                                   UA_NODEID_NUMERIC(0, UA_NS0ID_DATASETREADERTYPE),
                                   object_attr, NULL, &dataSetReader->identifier);

    /* Add childNodes such as PublisherId, WriterGroupId and DataSetWriterId in
     * DataSetReader object */
    publisherIdNode = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "PublisherId"),
                                          UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                                          dataSetReader->identifier);
    writerGroupIdNode = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "WriterGroupId"),
                                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                                            dataSetReader->identifier);
    dataSetwriterIdNode = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "DataSetWriterId"),
                                              UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                                              dataSetReader->identifier);

    if(UA_NodeId_isNull(&publisherIdNode) ||
       UA_NodeId_isNull(&writerGroupIdNode) ||
       UA_NodeId_isNull(&dataSetwriterIdNode)) {
        return UA_STATUSCODE_BADNOTFOUND;
    }

    UA_NodePropertyContext *dataSetReaderPublisherIdContext =
        (UA_NodePropertyContext *) UA_malloc(sizeof(UA_NodePropertyContext));
    dataSetReaderPublisherIdContext->parentNodeId = dataSetReader->identifier;
    dataSetReaderPublisherIdContext->parentClassifier = UA_NS0ID_DATASETREADERTYPE;
    dataSetReaderPublisherIdContext->elementClassiefier = UA_NS0ID_DATASETREADERTYPE_PUBLISHERID;
    UA_ValueCallback valueCallback;
    valueCallback.onRead = onRead;
    valueCallback.onWrite = NULL;
    retVal |= addVariableValueSource(server, valueCallback, publisherIdNode,
                                     dataSetReaderPublisherIdContext);

    /* Update childNode with values from Publisher */
    UA_Variant value;
    UA_Variant_init(&value);
    UA_Variant_setScalar(&value, &dataSetReader->config.writerGroupId,
                         &UA_TYPES[UA_TYPES_UINT16]);
    UA_Server_writeValue(server, writerGroupIdNode, value);
    UA_Variant_setScalar(&value, &dataSetReader->config.dataSetWriterId,
                         &UA_TYPES[UA_TYPES_UINT16]);
    UA_Server_writeValue(server, dataSetwriterIdNode, value);
    return retVal;
}

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
static UA_StatusCode
addDataSetReaderAction(UA_Server *server,
                       const UA_NodeId *sessionId, void *sessionHandle,
                       const UA_NodeId *methodId, void *methodContext,
                       const UA_NodeId *objectId, void *objectContext,
                       size_t inputSize, const UA_Variant *input,
                       size_t outputSize, UA_Variant *output){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_ReaderGroup *rg = UA_ReaderGroup_findRGbyId(server, *objectId);
    if(rg->configurationFrozen) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "addDataSetReader cannot be done because ReaderGroup config frozen");
        return UA_STATUSCODE_BAD;
    }

    UA_NodeId dataSetReaderId;
    UA_DataSetReaderDataType *dataSetReader= (UA_DataSetReaderDataType *) input[0].data;
    retVal |= addDataSetReaderConfig(server, *objectId, dataSetReader, &dataSetReaderId);
    if(retVal != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER, "addDataSetReader failed");
        return retVal;
    }

    UA_Variant_setScalarCopy(output, &dataSetReaderId, &UA_TYPES[UA_TYPES_NODEID]);
    return retVal;
}
#endif

UA_StatusCode
removeDataSetReaderRepresentation(UA_Server *server, UA_DataSetReader* dataSetReader) {
    return deleteNode(server, dataSetReader->identifier, true);
}

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
static UA_StatusCode
removeDataSetReaderAction(UA_Server *server,
                          const UA_NodeId *sessionId, void *sessionHandle,
                          const UA_NodeId *methodId, void *methodContext,
                          const UA_NodeId *objectId, void *objectContext,
                          size_t inputSize, const UA_Variant *input,
                          size_t outputSize, UA_Variant *output){
    UA_NodeId nodeToRemove = *((UA_NodeId *)input[0].data);
    return UA_Server_removeDataSetReader(server, nodeToRemove);
}
#endif

/*************************************************/
/*                PublishedDataSet               */
/*************************************************/
#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
static UA_StatusCode
addDataSetFolderAction(UA_Server *server,
                       const UA_NodeId *sessionId, void *sessionHandle,
                       const UA_NodeId *methodId, void *methodContext,
                       const UA_NodeId *objectId, void *objectContext,
                       size_t inputSize, const UA_Variant *input,
                       size_t outputSize, UA_Variant *output){
    /* defined in R 1.04 9.1.4.5.7 */
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_String newFolderName = *((UA_String *) input[0].data);
    UA_NodeId generatedId;
    UA_ObjectAttributes objectAttributes = UA_ObjectAttributes_default;
    UA_LocalizedText name = {UA_STRING(""), newFolderName};
    objectAttributes.displayName = name;
    retVal |= UA_Server_addObjectNode(server, UA_NODEID_NULL, *objectId,
                                      UA_NODEID_NUMERIC(0,UA_NS0ID_ORGANIZES),
                                      UA_QUALIFIEDNAME(0, "DataSetFolder"),
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE),
                                      objectAttributes, NULL, &generatedId);
    UA_Variant_setScalarCopy(output, &generatedId, &UA_TYPES[UA_TYPES_NODEID]);
#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
    retVal |= UA_Server_addReference(server, generatedId,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_ADDPUBLISHEDDATAITEMS), true);
    retVal |= UA_Server_addReference(server, generatedId,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_REMOVEPUBLISHEDDATASET), true);
    retVal |= UA_Server_addReference(server, generatedId,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_ADDDATASETFOLDER), true);
    retVal |= UA_Server_addReference(server, generatedId,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_REMOVEDATASETFOLDER), true);
#endif
    return retVal;
}
#endif

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
static UA_StatusCode
removeDataSetFolderAction(UA_Server *server,
                          const UA_NodeId *sessionId, void *sessionHandle,
                          const UA_NodeId *methodId, void *methodContext,
                          const UA_NodeId *objectId, void *objectContext,
                          size_t inputSize, const UA_Variant *input,
                          size_t outputSize, UA_Variant *output) {
    UA_NodeId nodeToRemove = *((UA_NodeId *) input[0].data);
    return UA_Server_deleteNode(server, nodeToRemove, true);
}
#endif

UA_StatusCode
addPublishedDataItemsRepresentation(UA_Server *server,
                                    UA_PublishedDataSet *publishedDataSet) {
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    if(publishedDataSet->config.name.length > 512)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    char pdsName[513];
    memcpy(pdsName, publishedDataSet->config.name.data, publishedDataSet->config.name.length);
    pdsName[publishedDataSet->config.name.length] = '\0';

    UA_ObjectAttributes object_attr = UA_ObjectAttributes_default;
    object_attr.displayName = UA_LOCALIZEDTEXT("", pdsName);
    retVal = UA_Server_addObjectNode(server, UA_NODEID_NUMERIC(1, 0), /* Create a new id */
                       UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_PUBLISHEDDATASETS),
                       UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                       UA_QUALIFIEDNAME(0, pdsName),
                       UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHEDDATAITEMSTYPE),
                       object_attr, NULL, &publishedDataSet->identifier);
    UA_CHECK_STATUS(retVal, return retVal);

    UA_ValueCallback valueCallback;
    valueCallback.onRead = onRead;
    valueCallback.onWrite = NULL;
    //ToDo: Need to move the browse name from namespaceindex 0 to 1
    UA_NodeId configurationVersionNode =
        findSingleChildNode(server, UA_QUALIFIEDNAME(0, "ConfigurationVersion"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                            publishedDataSet->identifier);
    if(UA_NodeId_isNull(&configurationVersionNode))
        return UA_STATUSCODE_BADNOTFOUND;

    UA_NodePropertyContext *configurationVersionContext = (UA_NodePropertyContext *)
        UA_malloc(sizeof(UA_NodePropertyContext));
    configurationVersionContext->parentNodeId = publishedDataSet->identifier;
    configurationVersionContext->parentClassifier = UA_NS0ID_PUBLISHEDDATAITEMSTYPE;
    configurationVersionContext->elementClassiefier =
        UA_NS0ID_PUBLISHEDDATAITEMSTYPE_CONFIGURATIONVERSION;
    retVal |= addVariableValueSource(server, valueCallback, configurationVersionNode,
                                     configurationVersionContext);

    UA_NodeId publishedDataNode =
        findSingleChildNode(server, UA_QUALIFIEDNAME(0, "PublishedData"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                            publishedDataSet->identifier);
    if(UA_NodeId_isNull(&publishedDataNode))
        return UA_STATUSCODE_BADNOTFOUND;

    UA_NodePropertyContext * publishingIntervalContext = (UA_NodePropertyContext *)
        UA_malloc(sizeof(UA_NodePropertyContext));
    publishingIntervalContext->parentNodeId = publishedDataSet->identifier;
    publishingIntervalContext->parentClassifier = UA_NS0ID_PUBLISHEDDATAITEMSTYPE;
    publishingIntervalContext->elementClassiefier = UA_NS0ID_PUBLISHEDDATAITEMSTYPE_PUBLISHEDDATA;
    retVal |= addVariableValueSource(server, valueCallback, publishedDataNode,
                                     publishingIntervalContext);

    UA_NodeId dataSetMetaDataNode =
        findSingleChildNode(server, UA_QUALIFIEDNAME(0, "DataSetMetaData"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                            publishedDataSet->identifier);
    if(UA_NodeId_isNull(&dataSetMetaDataNode))
        return UA_STATUSCODE_BADNOTFOUND;

    UA_NodePropertyContext *metaDataContext = (UA_NodePropertyContext *)
        UA_malloc(sizeof(UA_NodePropertyContext));
    metaDataContext->parentNodeId = publishedDataSet->identifier;
    metaDataContext->parentClassifier = UA_NS0ID_PUBLISHEDDATAITEMSTYPE;
    metaDataContext->elementClassiefier = UA_NS0ID_PUBLISHEDDATAITEMSTYPE_DATASETMETADATA;
    retVal |= addVariableValueSource(server, valueCallback,
                                     dataSetMetaDataNode, metaDataContext);

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
    retVal |= UA_Server_addReference(server, publishedDataSet->identifier,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_PUBLISHEDDATAITEMSTYPE_ADDVARIABLES), true);
    retVal |= UA_Server_addReference(server, publishedDataSet->identifier,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_PUBLISHEDDATAITEMSTYPE_REMOVEVARIABLES), true);
#endif
    return retVal;
}

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
static UA_StatusCode
addPublishedDataItemsAction(UA_Server *server,
                            const UA_NodeId *sessionId, void *sessionHandle,
                            const UA_NodeId *methodId, void *methodContext,
                            const UA_NodeId *objectId, void *objectContext,
                            size_t inputSize, const UA_Variant *input,
                            size_t outputSize, UA_Variant *output){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    size_t fieldNameAliasesSize = input[1].arrayLength;
    UA_String * fieldNameAliases = (UA_String *) input[1].data;
    size_t fieldFlagsSize = input[2].arrayLength;
    UA_DataSetFieldFlags * fieldFlags = (UA_DataSetFieldFlags *) input[2].data;
    size_t variablesToAddSize = input[3].arrayLength;
    UA_PublishedVariableDataType *eoAddVar =
        (UA_PublishedVariableDataType *)input[3].data;

    if(fieldNameAliasesSize != fieldFlagsSize ||
       fieldFlagsSize != variablesToAddSize)
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    UA_PublishedDataSetConfig publishedDataSetConfig;
    memset(&publishedDataSetConfig, 0, sizeof(publishedDataSetConfig));
    publishedDataSetConfig.name = *((UA_String *) input[0].data);
    publishedDataSetConfig.publishedDataSetType = UA_PUBSUB_DATASET_PUBLISHEDITEMS;

    UA_NodeId dataSetItemsNodeId;
    retVal |= UA_Server_addPublishedDataSet(server, &publishedDataSetConfig,
                                            &dataSetItemsNodeId).addResult;
    if(retVal != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "addPublishedDataset failed");
        return retVal;
    }

    UA_DataSetFieldConfig dataSetFieldConfig;
    for(size_t j = 0; j < variablesToAddSize; ++j) {
        memset(&dataSetFieldConfig, 0, sizeof(UA_DataSetFieldConfig));
        dataSetFieldConfig.dataSetFieldType = UA_PUBSUB_DATASETFIELD_VARIABLE;
        dataSetFieldConfig.field.variable.fieldNameAlias = fieldNameAliases[j];
        dataSetFieldConfig.field.variable.publishParameters = eoAddVar[j];
        if(fieldFlags[j] == UA_DATASETFIELDFLAGS_PROMOTEDFIELD)
            dataSetFieldConfig.field.variable.promotedField = true;
        retVal |= UA_Server_addDataSetField(server, dataSetItemsNodeId,
                                            &dataSetFieldConfig, NULL).result;
        if(retVal != UA_STATUSCODE_GOOD) {
           UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                        "addDataSetField failed");
           return retVal;
        }
    }

    UA_Variant_setScalarCopy(output, &dataSetItemsNodeId, &UA_TYPES[UA_TYPES_NODEID]);
    return retVal;
}
#endif

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
static UA_StatusCode
addVariablesAction(UA_Server *server,
                   const UA_NodeId *sessionId, void *sessionHandle,
                   const UA_NodeId *methodId, void *methodContext,
                   const UA_NodeId *objectId, void *objectContext,
                   size_t inputSize, const UA_Variant *input,
                   size_t outputSize, UA_Variant *output){
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
removeVariablesAction(UA_Server *server,
                      const UA_NodeId *sessionId, void *sessionHandle,
                      const UA_NodeId *methodId, void *methodContext,
                      const UA_NodeId *objectId, void *objectContext,
                      size_t inputSize, const UA_Variant *input,
                      size_t outputSize, UA_Variant *output){
    return UA_STATUSCODE_GOOD;
}
#endif


UA_StatusCode
removePublishedDataSetRepresentation(UA_Server *server,
                                     UA_PublishedDataSet *publishedDataSet) {
    return deleteNode(server, publishedDataSet->identifier, true);
}

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
static UA_StatusCode
removePublishedDataSetAction(UA_Server *server,
                             const UA_NodeId *sessionId, void *sessionHandle,
                             const UA_NodeId *methodId, void *methodContext,
                             const UA_NodeId *objectId, void *objectContext,
                             size_t inputSize, const UA_Variant *input,
                             size_t outputSize, UA_Variant *output){
    UA_NodeId nodeToRemove = *((UA_NodeId *) input[0].data);
    return UA_Server_removePublishedDataSet(server, nodeToRemove);
}
#endif

/**********************************************/
/*       StandaloneSubscribedDataSet          */
/**********************************************/

UA_StatusCode
addStandaloneSubscribedDataSetRepresentation(UA_Server *server, UA_StandaloneSubscribedDataSet *subscribedDataSet) {
    UA_StatusCode ret = UA_STATUSCODE_GOOD;
    if(subscribedDataSet->config.name.length > 512)
        return UA_STATUSCODE_BADCONFIGURATIONERROR;

    UA_STACKARRAY(char, sdsName, sizeof(char) * subscribedDataSet->config.name.length +1);
    memcpy(sdsName, subscribedDataSet->config.name.data, subscribedDataSet->config.name.length);
    sdsName[subscribedDataSet->config.name.length] = '\0';

    UA_ObjectAttributes object_attr = UA_ObjectAttributes_default;
    object_attr.displayName = UA_LOCALIZEDTEXT("", sdsName);
    UA_Server_addObjectNode(server, UA_NODEID_NUMERIC(1, 0), /* Create a new id */
                                   UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_SUBSCRIBEDDATASETS),
                                   UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                   UA_QUALIFIEDNAME(0, sdsName),
                                   UA_NODEID_NUMERIC(0, UA_NS0ID_STANDALONESUBSCRIBEDDATASETTYPE),
                                   object_attr, NULL, &subscribedDataSet->identifier);
    UA_NodeId sdsObjectNode = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "SubscribedDataSet"),
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                      subscribedDataSet->identifier);
    UA_NodeId metaDataId = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "DataSetMetaData"),
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                                      subscribedDataSet->identifier);
    UA_NodeId connectedId = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "IsConnected"),
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                                      subscribedDataSet->identifier);

    if(UA_NodeId_equal(&sdsObjectNode, &UA_NODEID_NULL) ||
       UA_NodeId_equal(&metaDataId, &UA_NODEID_NULL) ||
       UA_NodeId_equal(&connectedId, &UA_NODEID_NULL)) {
        return UA_STATUSCODE_BADNOTFOUND;
    }
    if(subscribedDataSet->config.subscribedDataSetType == UA_PUBSUB_SDS_TARGET){
        UA_VariableAttributes attr = UA_VariableAttributes_default;
        UA_NodeId targetVarsId;
        attr.displayName = UA_LOCALIZEDTEXT("", "TargetVariables");
        attr.dataType = UA_TYPES[UA_TYPES_FIELDTARGETDATATYPE].typeId;
        attr.valueRank = UA_VALUERANK_ONE_DIMENSION;
        attr.arrayDimensionsSize = 1;
        UA_UInt32 arrayDimensions[1];
        arrayDimensions[0] = (UA_UInt32) subscribedDataSet->config.subscribedDataSet.target.targetVariablesSize;
        attr.arrayDimensions = arrayDimensions;
        attr.accessLevel = UA_ACCESSLEVELMASK_READ;
        UA_Variant_setArray(&attr.value, subscribedDataSet->config.subscribedDataSet.target.targetVariables,
                            subscribedDataSet->config.subscribedDataSet.target.targetVariablesSize,
                            &UA_TYPES[UA_TYPES_FIELDTARGETDATATYPE]);
        ret |= UA_Server_addVariableNode(server, UA_NODEID_NULL, sdsObjectNode, UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                                         UA_QUALIFIEDNAME(0, "TargetVariables"), UA_NODEID_NUMERIC(0, UA_NS0ID_PROPERTYTYPE),
                                         attr, NULL, &targetVarsId);
    }

    UA_NodePropertyContext *isConnectedNodeContext = (UA_NodePropertyContext *) UA_malloc(sizeof(UA_NodePropertyContext));
    isConnectedNodeContext->parentNodeId = subscribedDataSet->identifier;
    isConnectedNodeContext->parentClassifier = UA_NS0ID_STANDALONESUBSCRIBEDDATASETREFDATATYPE;
    isConnectedNodeContext->elementClassiefier = UA_NS0ID_STANDALONESUBSCRIBEDDATASETTYPE_ISCONNECTED;

    UA_ValueCallback valueCallback;
    valueCallback.onRead = onRead;
    valueCallback.onWrite = NULL;
    ret |= addVariableValueSource(server, valueCallback, connectedId, isConnectedNodeContext);

    UA_NodePropertyContext *metaDataContext = (UA_NodePropertyContext *)
        UA_malloc(sizeof(UA_NodePropertyContext));
    metaDataContext->parentNodeId = subscribedDataSet->identifier;
    metaDataContext->parentClassifier = UA_NS0ID_STANDALONESUBSCRIBEDDATASETREFDATATYPE;
    metaDataContext->elementClassiefier = UA_NS0ID_STANDALONESUBSCRIBEDDATASETTYPE_DATASETMETADATA;
    ret |= addVariableValueSource(server, valueCallback, metaDataId, metaDataContext);

    return ret;
}

UA_StatusCode
removeStandaloneSubscribedDataSetRepresentation(UA_Server *server, UA_StandaloneSubscribedDataSet *subscribedDataSet) {
    return deleteNode(server, subscribedDataSet->identifier, true);
}

/**********************************************/
/*               WriterGroup                  */
/**********************************************/

static UA_StatusCode
readContentMask(UA_Server *server, const UA_NodeId *sessionId,
                void *sessionContext, const UA_NodeId *nodeId,
                void *nodeContext, UA_Boolean includeSourceTimeStamp,
                const UA_NumericRange *range, UA_DataValue *value) {
    UA_WriterGroup *writerGroup = (UA_WriterGroup*)nodeContext;
    if((writerGroup->config.messageSettings.encoding != UA_EXTENSIONOBJECT_DECODED &&
        writerGroup->config.messageSettings.encoding != UA_EXTENSIONOBJECT_DECODED_NODELETE) ||
       writerGroup->config.messageSettings.content.decoded.type !=
       &UA_TYPES[UA_TYPES_UADPWRITERGROUPMESSAGEDATATYPE])
        return UA_STATUSCODE_BADINTERNALERROR;
    UA_UadpWriterGroupMessageDataType *wgm = (UA_UadpWriterGroupMessageDataType*)
        writerGroup->config.messageSettings.content.decoded.data;

    UA_Variant_setScalarCopy(&value->value, &wgm->networkMessageContentMask,
                             &UA_TYPES[UA_TYPES_UADPNETWORKMESSAGECONTENTMASK]);
    value->hasValue = true;
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
writeContentMask(UA_Server *server, const UA_NodeId *sessionId,
                 void *sessionContext, const UA_NodeId *nodeId,
                 void *nodeContext, const UA_NumericRange *range,
                 const UA_DataValue *value) {
    UA_WriterGroup *writerGroup = (UA_WriterGroup*)nodeContext;
    if((writerGroup->config.messageSettings.encoding != UA_EXTENSIONOBJECT_DECODED &&
        writerGroup->config.messageSettings.encoding != UA_EXTENSIONOBJECT_DECODED_NODELETE) ||
       writerGroup->config.messageSettings.content.decoded.type !=
       &UA_TYPES[UA_TYPES_UADPWRITERGROUPMESSAGEDATATYPE])
        return UA_STATUSCODE_BADINTERNALERROR;
    UA_UadpWriterGroupMessageDataType *wgm = (UA_UadpWriterGroupMessageDataType*)
        writerGroup->config.messageSettings.content.decoded.data;

    if(!value->value.type)
        return UA_STATUSCODE_BADTYPEMISMATCH;
    if(value->value.type->typeKind != UA_DATATYPEKIND_ENUM &&
       value->value.type->typeKind != UA_DATATYPEKIND_INT32)
        return UA_STATUSCODE_BADTYPEMISMATCH;

    wgm->networkMessageContentMask = *(UA_UadpNetworkMessageContentMask*)value->value.data;
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
addWriterGroupRepresentation(UA_Server *server, UA_WriterGroup *writerGroup){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    if(writerGroup->config.name.length > 512)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    char wgName[513];
    memcpy(wgName, writerGroup->config.name.data, writerGroup->config.name.length);
    wgName[writerGroup->config.name.length] = '\0';

    UA_ObjectAttributes object_attr = UA_ObjectAttributes_default;
    object_attr.displayName = UA_LOCALIZEDTEXT("", wgName);
    retVal = UA_Server_addObjectNode(server, UA_NODEID_NUMERIC(1, 0), /* create a new id */
                                     writerGroup->linkedConnection->identifier,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_QUALIFIEDNAME(0, wgName),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_WRITERGROUPTYPE),
                                     object_attr, NULL, &writerGroup->identifier);

    UA_NodeId keepAliveNode =
        findSingleChildNode(server, UA_QUALIFIEDNAME(0, "KeepAliveTime"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                            writerGroup->identifier);
    UA_NodeId publishingIntervalNode =
        findSingleChildNode(server, UA_QUALIFIEDNAME(0, "PublishingInterval"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                            writerGroup->identifier);
    if(UA_NodeId_isNull(&keepAliveNode) ||
       UA_NodeId_isNull(&publishingIntervalNode))
        return UA_STATUSCODE_BADNOTFOUND;

    UA_NodePropertyContext * publishingIntervalContext = (UA_NodePropertyContext *)
        UA_malloc(sizeof(UA_NodePropertyContext));
    publishingIntervalContext->parentNodeId = writerGroup->identifier;
    publishingIntervalContext->parentClassifier = UA_NS0ID_WRITERGROUPTYPE;
    publishingIntervalContext->elementClassiefier = UA_NS0ID_WRITERGROUPTYPE_PUBLISHINGINTERVAL;
    UA_ValueCallback valueCallback;
    valueCallback.onRead = onRead;
    valueCallback.onWrite = onWrite;
    retVal |= addVariableValueSource(server, valueCallback,
                                     publishingIntervalNode, publishingIntervalContext);
    UA_Server_writeAccessLevel(server, publishingIntervalNode,
                               UA_ACCESSLEVELMASK_READ ^ UA_ACCESSLEVELMASK_WRITE);

    UA_NodeId priorityNode =
        findSingleChildNode(server, UA_QUALIFIEDNAME(0, "Priority"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                            writerGroup->identifier);
    UA_NodeId writerGroupIdNode =
        findSingleChildNode(server, UA_QUALIFIEDNAME(0, "WriterGroupId"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                            writerGroup->identifier);

    UA_Variant value;
    UA_Variant_init(&value);
    UA_Variant_setScalar(&value, &writerGroup->config.publishingInterval, &UA_TYPES[UA_TYPES_DURATION]);
    UA_Server_writeValue(server, publishingIntervalNode, value);
    UA_Variant_setScalar(&value, &writerGroup->config.keepAliveTime, &UA_TYPES[UA_TYPES_DURATION]);
    UA_Server_writeValue(server, keepAliveNode, value);
    UA_Variant_setScalar(&value, &writerGroup->config.priority, &UA_TYPES[UA_TYPES_BYTE]);
    UA_Server_writeValue(server, priorityNode, value);
    UA_Variant_setScalar(&value, &writerGroup->config.writerGroupId, &UA_TYPES[UA_TYPES_UINT16]);
    UA_Server_writeValue(server, writerGroupIdNode, value);

    object_attr.displayName = UA_LOCALIZEDTEXT("", "MessageSettings");
    retVal |= UA_Server_addObjectNode(server, UA_NODEID_NUMERIC(1, 0),
                                      writerGroup->identifier,
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                      UA_QUALIFIEDNAME(0, "MessageSettings"),
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_UADPWRITERGROUPMESSAGETYPE),
                                      object_attr, NULL, NULL);

    /* Find the variable with the content mask */

    UA_NodeId messageSettingsId =
        findSingleChildNode(server, UA_QUALIFIEDNAME(0, "MessageSettings"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                            writerGroup->identifier);
    UA_NodeId contentMaskId =
        findSingleChildNode(server, UA_QUALIFIEDNAME(0, "NetworkMessageContentMask"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY), messageSettingsId);
    if(!UA_NodeId_isNull(&contentMaskId)) {
        /* Set the callback */
        UA_DataSource ds;
        ds.read = readContentMask;
        ds.write = writeContentMask;
        UA_Server_setVariableNode_dataSource(server, contentMaskId, ds);
        UA_Server_setNodeContext(server, contentMaskId, writerGroup);

        /* Make writable */
        UA_Server_writeAccessLevel(server, contentMaskId,
                                   UA_ACCESSLEVELMASK_WRITE | UA_ACCESSLEVELMASK_READ);

    }

    /* Add reference to methods */
#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
    retVal |= UA_Server_addReference(server, writerGroup->identifier,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_WRITERGROUPTYPE_ADDDATASETWRITER), true);
    retVal |= UA_Server_addReference(server, writerGroup->identifier,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_WRITERGROUPTYPE_REMOVEDATASETWRITER), true);
#endif

    return retVal;
}

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
static UA_StatusCode
addWriterGroupAction(UA_Server *server,
                             const UA_NodeId *sessionId, void *sessionHandle,
                             const UA_NodeId *methodId, void *methodContext,
                             const UA_NodeId *objectId, void *objectContext,
                             size_t inputSize, const UA_Variant *input,
                             size_t outputSize, UA_Variant *output){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_WriterGroupDataType *writerGroup = ((UA_WriterGroupDataType *) input[0].data);
    UA_NodeId writerGroupId;
    retVal |= addWriterGroupConfig(server, *objectId, writerGroup, &writerGroupId);
    if(retVal != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER, "addWriterGroup failed");
        return retVal;
    }
    //TODO: Need to handle the UA_Server_setWriterGroupOperational based on the status variable in information model

    UA_Variant_setScalarCopy(output, &writerGroupId, &UA_TYPES[UA_TYPES_NODEID]);
    return retVal;
}
#endif

UA_StatusCode
removeGroupRepresentation(UA_Server *server, UA_WriterGroup *writerGroup) {
    return deleteNode(server, writerGroup->identifier, true);
}

UA_StatusCode
removeReaderGroupRepresentation(UA_Server *server, UA_ReaderGroup *readerGroup) {
    return deleteNode(server, readerGroup->identifier, true);
}

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
static UA_StatusCode
removeGroupAction(UA_Server *server,
                  const UA_NodeId *sessionId, void *sessionHandle,
                  const UA_NodeId *methodId, void *methodContext,
                  const UA_NodeId *objectId, void *objectContext,
                  size_t inputSize, const UA_Variant *input,
                  size_t outputSize, UA_Variant *output){
    UA_NodeId nodeToRemove = *((UA_NodeId *)input->data);
    if(UA_WriterGroup_findWGbyId(server, nodeToRemove)) {
        UA_WriterGroup *wg = UA_WriterGroup_findWGbyId(server, nodeToRemove);
        if(wg->configurationFrozen)
            UA_Server_unfreezeWriterGroupConfiguration(server, nodeToRemove);
        return UA_Server_removeWriterGroup(server, nodeToRemove);
    } else {
        UA_ReaderGroup *rg = UA_ReaderGroup_findRGbyId(server, nodeToRemove);
        if(rg->configurationFrozen)
            UA_Server_unfreezeReaderGroupConfiguration(server, nodeToRemove);
        return UA_Server_removeReaderGroup(server, nodeToRemove);
    }
}
#endif

/**********************************************/
/*               ReserveIds                   */
/**********************************************/

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
static UA_StatusCode
addReserveIdsAction(UA_Server *server,
                             const UA_NodeId *sessionId, void *sessionHandle,
                             const UA_NodeId *methodId, void *methodContext,
                             const UA_NodeId *objectId, void *objectContext,
                             size_t inputSize, const UA_Variant *input,
                             size_t outputSize, UA_Variant *output){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_String transportProfileUri = *((UA_String *)input[0].data);
    UA_UInt16 numRegWriterGroupIds = *((UA_UInt16 *)input[1].data);
    UA_UInt16 numRegDataSetWriterIds = *((UA_UInt16 *)input[2].data);

    UA_UInt16 *writerGroupIds;
    UA_UInt16 *dataSetWriterIds;

    retVal |= UA_PubSubManager_reserveIds(server, *sessionId, numRegWriterGroupIds, numRegDataSetWriterIds,
                                          transportProfileUri, &writerGroupIds, &dataSetWriterIds);
    if(retVal != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER, "addReserveIds failed");
        return retVal;
    }

    /* Check the transportProfileUri */
    UA_String profile_1 = UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-mqtt-uadp");
    UA_String profile_2 = UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-mqtt-json");

    if(UA_String_equal(&transportProfileUri, &profile_1) || UA_String_equal(&transportProfileUri, &profile_2)) {
        UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_SERVER, "ApplicationUri: %.*s",
                    (int)server->config.applicationDescription.applicationUri.length,
                    server->config.applicationDescription.applicationUri.data);
        retVal |= UA_Variant_setScalarCopy(&output[0], &server->config.applicationDescription.applicationUri,
                                           &UA_TYPES[UA_TYPES_STRING]);
    }
    else
        retVal |= UA_Variant_setScalarCopy(&output[0], &server->pubSubManager.defaultPublisherId, &UA_TYPES[UA_TYPES_UINT64]);
    UA_Variant_setArray(&output[1], writerGroupIds, numRegWriterGroupIds, &UA_TYPES[UA_TYPES_UINT16]);
    UA_Variant_setArray(&output[2], dataSetWriterIds, numRegDataSetWriterIds, &UA_TYPES[UA_TYPES_UINT16]);

    return retVal;
}
#endif

/**********************************************/
/*               ReaderGroup                  */
/**********************************************/

UA_StatusCode
addReaderGroupRepresentation(UA_Server *server, UA_ReaderGroup *readerGroup) {
    if(readerGroup->config.name.length > 512)
        return UA_STATUSCODE_BADCONFIGURATIONERROR;
    char rgName[513];
    memcpy(rgName, readerGroup->config.name.data, readerGroup->config.name.length);
    rgName[readerGroup->config.name.length] = '\0';

    UA_ObjectAttributes object_attr = UA_ObjectAttributes_default;
    object_attr.displayName = UA_LOCALIZEDTEXT("", rgName);
    UA_StatusCode retVal = UA_Server_addObjectNode(server, UA_NODEID_NUMERIC(1, 0), /* create an id */
                                   readerGroup->linkedConnection,
                                   UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                   UA_QUALIFIEDNAME(0, rgName),
                                   UA_NODEID_NUMERIC(0, UA_NS0ID_READERGROUPTYPE),
                                   object_attr, NULL, &readerGroup->identifier);
#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
    retVal |= UA_Server_addReference(server, readerGroup->identifier,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_READERGROUPTYPE_ADDDATASETREADER), true);
    retVal |= UA_Server_addReference(server, readerGroup->identifier,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_READERGROUPTYPE_REMOVEDATASETREADER), true);
#endif
    return retVal;
}

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
static UA_StatusCode
addReaderGroupAction(UA_Server *server,
                     const UA_NodeId *sessionId, void *sessionHandle,
                     const UA_NodeId *methodId, void *methodContext,
                     const UA_NodeId *objectId, void *objectContext,
                     size_t inputSize, const UA_Variant *input,
                     size_t outputSize, UA_Variant *output){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_ReaderGroupDataType *readerGroup = ((UA_ReaderGroupDataType *) input->data);
    UA_NodeId readerGroupId;
    retVal |= addReaderGroupConfig(server, *objectId, readerGroup, &readerGroupId);
    if(retVal != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER, "addReaderGroup failed");
        return retVal;
    }
    //TODO: Need to handle the UA_Server_setReaderGroupOperational based on the status variable in information model

    UA_Variant_setScalarCopy(output, &readerGroupId, &UA_TYPES[UA_TYPES_NODEID]);
    return retVal;
}
#endif

#ifdef UA_ENABLE_PUBSUB_SKS
#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL
static UA_Boolean
isValidParentNode(UA_Server *server, UA_NodeId parentId) {
    UA_Boolean retval = true;
    const UA_Node *parentNodeType;
    const UA_NodeId parentNodeTypeId =
        UA_NODEID_NUMERIC(0, UA_NS0ID_SECURITYGROUPFOLDERTYPE);
    const UA_Node *parentNode = UA_NODESTORE_GET(server, &parentId);

    if(parentNode) {
        parentNodeType = getNodeType(server, &parentNode->head);
        if(parentNodeType) {
            retval = UA_NodeId_equal(&parentNodeType->head.nodeId, &parentNodeTypeId);
            UA_NODESTORE_RELEASE(server, parentNodeType);
        }
        UA_NODESTORE_RELEASE(server, parentNode);
    }
    return retval;
}

static UA_StatusCode
updateSecurityGroupProperties(UA_Server *server, UA_NodeId *securityGroupNodeId,
                              UA_SecurityGroupConfig *config) {
    UA_StatusCode retval = UA_STATUSCODE_BAD;
    UA_Variant value;
    UA_Variant_init(&value);
    UA_Variant_setScalar(&value, &config->securityGroupName, &UA_TYPES[UA_TYPES_STRING]);
    retval = writeObjectProperty(server, *securityGroupNodeId,
                                 UA_QUALIFIEDNAME(0, "SecurityGroupId"), value);
    if(retval != UA_STATUSCODE_GOOD)
        return retval;

    /*AddValueCallback*/
    UA_Variant_setScalar(&value, &config->securityPolicyUri, &UA_TYPES[UA_TYPES_STRING]);
    retval = writeObjectProperty(server, *securityGroupNodeId,
                                 UA_QUALIFIEDNAME(0, "SecurityPolicyUri"), value);
    if(retval != UA_STATUSCODE_GOOD)
        return retval;

    UA_Variant_setScalar(&value, &config->keyLifeTime, &UA_TYPES[UA_TYPES_DURATION]);
    retval = writeObjectProperty(server, *securityGroupNodeId,
                                 UA_QUALIFIEDNAME(0, "KeyLifetime"), value);
    if(retval != UA_STATUSCODE_GOOD)
        return retval;

    UA_Variant_setScalar(&value, &config->maxFutureKeyCount, &UA_TYPES[UA_TYPES_UINT32]);
    retval = writeObjectProperty(server, *securityGroupNodeId,
                                 UA_QUALIFIEDNAME(0, "MaxFutureKeyCount"), value);
    if(retval != UA_STATUSCODE_GOOD)
        return retval;

    UA_Variant_setScalar(&value, &config->maxPastKeyCount, &UA_TYPES[UA_TYPES_UINT32]);
    retval = writeObjectProperty(server, *securityGroupNodeId,
                                 UA_QUALIFIEDNAME(0, "MaxPastKeyCount"), value);
    if(retval != UA_STATUSCODE_GOOD)
        return retval;

    return retval;
}

UA_StatusCode
addSecurityGroupRepresentation(UA_Server *server, UA_SecurityGroup *securityGroup) {
    UA_LOCK_ASSERT(&server->serviceMutex, 1);
    UA_StatusCode retval = UA_STATUSCODE_BAD;

    UA_SecurityGroupConfig *securityGroupConfig = &securityGroup->config;
    if(!isValidParentNode(server, securityGroup->securityGroupFolderId))
        return UA_STATUSCODE_BADPARENTNODEIDINVALID;

    if(securityGroupConfig->securityGroupName.length <= 0)
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    size_t sgNamelength = securityGroupConfig->securityGroupName.length;
    char *sgName = (char *)UA_malloc(sgNamelength);
    if(!sgName)
        return UA_STATUSCODE_BADOUTOFMEMORY;

    memset(sgName, 0, sgNamelength);
    memcpy(sgName, securityGroupConfig->securityGroupName.data,
           securityGroupConfig->securityGroupName.length);
    sgName[securityGroupConfig->securityGroupName.length] = '\0';

    UA_ObjectAttributes object_attr = UA_ObjectAttributes_default;
    object_attr.displayName = UA_LOCALIZEDTEXT("", sgName);
    UA_NodeId refType = UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT);
    UA_NodeId nodeType = UA_NODEID_NUMERIC(0, UA_NS0ID_SECURITYGROUPTYPE);
    retval = addNode(
        server, UA_NODECLASS_OBJECT, &UA_NODEID_NULL,
        &securityGroup->securityGroupFolderId, &refType, UA_QUALIFIEDNAME(0, sgName),
        &nodeType, (const UA_NodeAttributes *)&object_attr,
        &UA_TYPES[UA_TYPES_OBJECTATTRIBUTES], NULL, &securityGroup->securityGroupNodeId);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "Add SecurityGroup failed with error: %s.",
                     UA_StatusCode_name(retval));
        UA_free(sgName);
        return retval;
    }

    retval = updateSecurityGroupProperties(server, &securityGroup->securityGroupNodeId,
                                           securityGroupConfig);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "Add SecurityGroup failed with error: %s.",
                     UA_StatusCode_name(retval));
        UA_removeSecurityGroupRepresentation(server, securityGroup);
    }
    return retval;
}

UA_StatusCode
UA_removeSecurityGroupRepresentation(UA_Server *server, UA_SecurityGroup *securityGroup) {
    UA_StatusCode retval = deleteNode(server, securityGroup->securityGroupNodeId, true);
    return  retval;
}
#endif /* UA_ENABLE_PUBSUB_INFORMATIONMODEL*/
#endif /* UA_ENABLE_PUBSUB_SKS */

/**********************************************/
/*               DataSetWriter                */
/**********************************************/

UA_StatusCode
addDataSetWriterRepresentation(UA_Server *server, UA_DataSetWriter *dataSetWriter) {
    UA_LOCK_ASSERT(&server->serviceMutex, 1);

    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    if(dataSetWriter->config.name.length > 512)
        return UA_STATUSCODE_BADOUTOFMEMORY;

    char dswName[513];
    memcpy(dswName, dataSetWriter->config.name.data, dataSetWriter->config.name.length);
    dswName[dataSetWriter->config.name.length] = '\0';

    UA_UNLOCK(&server->serviceMutex);

    UA_ObjectAttributes object_attr = UA_ObjectAttributes_default;
    object_attr.displayName = UA_LOCALIZEDTEXT("", dswName);
    retVal = UA_Server_addObjectNode(server, UA_NODEID_NUMERIC(1, 0), /* create an id */
                                   dataSetWriter->linkedWriterGroup,
                                   UA_NODEID_NUMERIC(0, UA_NS0ID_HASDATASETWRITER),
                                   UA_QUALIFIEDNAME(0, dswName),
                                   UA_NODEID_NUMERIC(0, UA_NS0ID_DATASETWRITERTYPE),
                                   object_attr, NULL, &dataSetWriter->identifier);
    //if connected dataset is null this means it's configured for heartbeats
    if(!UA_NodeId_isNull(&dataSetWriter->connectedDataSet)){
        retVal |= UA_Server_addReference(server, dataSetWriter->connectedDataSet,
                                         UA_NODEID_NUMERIC(0, UA_NS0ID_DATASETTOWRITER),
                                         UA_EXPANDEDNODEID_NODEID(dataSetWriter->identifier),
                                         true);
    }

    UA_NodeId dataSetWriterIdNode =
        findSingleChildNode(server, UA_QUALIFIEDNAME(0, "DataSetWriterId"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                            dataSetWriter->identifier);
    UA_NodeId keyFrameNode =
        findSingleChildNode(server, UA_QUALIFIEDNAME(0, "KeyFrameCount"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                            dataSetWriter->identifier);
    UA_NodeId dataSetFieldContentMaskNode =
        findSingleChildNode(server, UA_QUALIFIEDNAME(0, "DataSetFieldContentMask"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                            dataSetWriter->identifier);

    UA_NodePropertyContext *dataSetWriterIdContext = (UA_NodePropertyContext *)
        UA_malloc(sizeof(UA_NodePropertyContext));
    dataSetWriterIdContext->parentNodeId = dataSetWriter->identifier;
    dataSetWriterIdContext->parentClassifier = UA_NS0ID_DATASETWRITERTYPE;
    dataSetWriterIdContext->elementClassiefier = UA_NS0ID_DATASETWRITERTYPE_DATASETWRITERID;
    UA_ValueCallback valueCallback;
    valueCallback.onRead = onRead;
    valueCallback.onWrite = NULL;
    retVal |= addVariableValueSource(server, valueCallback,
                                     dataSetWriterIdNode, dataSetWriterIdContext);

    UA_Variant value;
    UA_Variant_init(&value);
    UA_Variant_setScalar(&value, &dataSetWriter->config.dataSetWriterId,
                         &UA_TYPES[UA_TYPES_UINT16]);
    UA_Server_writeValue(server, dataSetWriterIdNode, value);
    UA_Variant_setScalar(&value, &dataSetWriter->config.keyFrameCount,
                         &UA_TYPES[UA_TYPES_UINT32]);
    UA_Server_writeValue(server, keyFrameNode, value);
    UA_Variant_setScalar(&value, &dataSetWriter->config.dataSetFieldContentMask,
                         &UA_TYPES[UA_TYPES_DATASETFIELDCONTENTMASK]);
    UA_Server_writeValue(server, dataSetFieldContentMaskNode, value);

    object_attr.displayName = UA_LOCALIZEDTEXT("", "MessageSettings");
    retVal |= UA_Server_addObjectNode(server, UA_NODEID_NUMERIC(1, 0),
                                      dataSetWriter->identifier,
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                      UA_QUALIFIEDNAME(0, "MessageSettings"),
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_UADPDATASETWRITERMESSAGETYPE),
                                      object_attr, NULL, NULL);

    UA_LOCK(&server->serviceMutex);
    return retVal;
}

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
static UA_StatusCode
addDataSetWriterAction(UA_Server *server,
                       const UA_NodeId *sessionId, void *sessionHandle,
                       const UA_NodeId *methodId, void *methodContext,
                       const UA_NodeId *objectId, void *objectContext,
                       size_t inputSize, const UA_Variant *input,
                       size_t outputSize, UA_Variant *output){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_WriterGroup *wg = UA_WriterGroup_findWGbyId(server, *objectId);
    if(!wg) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "Not a WriterGroup");
        return UA_STATUSCODE_BAD;
    }
    if(wg->configurationFrozen) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "addDataSetWriter cannot be done because writergroup config frozen");
        return UA_STATUSCODE_BAD;
    }

    UA_NodeId dataSetWriterId;
    UA_DataSetWriterDataType *dataSetWriterData = (UA_DataSetWriterDataType *)input->data;
    retVal |= addDataSetWriterConfig(server, objectId, dataSetWriterData, &dataSetWriterId);
    if(retVal != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "addDataSetWriter failed");
        return retVal;
    }

    UA_Variant_setScalarCopy(output, &dataSetWriterId, &UA_TYPES[UA_TYPES_NODEID]);
    return UA_STATUSCODE_GOOD;
}
#endif

UA_StatusCode
removeDataSetWriterRepresentation(UA_Server *server,
                                  UA_DataSetWriter *dataSetWriter) {
    return deleteNode(server, dataSetWriter->identifier, true);
}

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
static UA_StatusCode
removeDataSetWriterAction(UA_Server *server,
                          const UA_NodeId *sessionId, void *sessionHandle,
                          const UA_NodeId *methodId, void *methodContext,
                          const UA_NodeId *objectId, void *objectContext,
                          size_t inputSize, const UA_Variant *input,
                          size_t outputSize, UA_Variant *output){
    UA_NodeId nodeToRemove = *((UA_NodeId *) input[0].data);
    return UA_Server_removeDataSetWriter(server, nodeToRemove);
}
#endif

#if defined(UA_ENABLE_PUBSUB_SKS) && defined(UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS)
/**
 * @note The user credentials and permissions are checked in the AccessControl plugin
 * before this callback is executed.
 */
static UA_StatusCode
setSecurityKeysAction(UA_Server *server, const UA_NodeId *sessionId, void *sessionHandle,
                      const UA_NodeId *methodId, void *methodContext,
                      const UA_NodeId *objectId, void *objectContext, size_t inputSize,
                      const UA_Variant *input, size_t outputSize, UA_Variant *output) {

    /*Check whether the channel is encrypted according to specification*/
    session_list_entry *session_entry;
    LIST_FOREACH(session_entry, &server->sessions, pointers) {
        if(UA_NodeId_equal(&session_entry->session.sessionId, sessionId)) {
            if(session_entry->session.header.channel->securityMode !=
               UA_MESSAGESECURITYMODE_SIGNANDENCRYPT)
                return UA_STATUSCODE_BADSECURITYMODEINSUFFICIENT;
        }
    }

    if(!server || !input)
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    if(inputSize < 7)
        return UA_STATUSCODE_BADARGUMENTSMISSING;
    if(inputSize > 7 || outputSize > 0)
        return UA_STATUSCODE_BADTOOMANYARGUMENTS;

    /*check for types*/
    if(!UA_Variant_hasScalarType(&input[0], &UA_TYPES[UA_TYPES_STRING]) || /*SecurityGroupId*/
        !UA_Variant_hasScalarType(&input[1], &UA_TYPES[UA_TYPES_STRING]) || /*SecurityPolicyUri*/
        !UA_Variant_hasScalarType(&input[2], &UA_TYPES[UA_TYPES_UINT32]) || /*CurrentTokenId*/
        !UA_Variant_hasScalarType(&input[3], &UA_TYPES[UA_TYPES_BYTESTRING]) || /*CurrentKey*/
        !UA_Variant_hasArrayType(&input[4], &UA_TYPES[UA_TYPES_BYTESTRING]) || /*FutureKeys*/
        (!UA_Variant_hasScalarType(&input[5], &UA_TYPES[UA_TYPES_DURATION]) &&
        !UA_Variant_hasScalarType(&input[5], &UA_TYPES[UA_TYPES_DOUBLE])) || /*TimeToNextKey*/
        (!UA_Variant_hasScalarType(&input[6], &UA_TYPES[UA_TYPES_DURATION]) &&
        !UA_Variant_hasScalarType(&input[6], &UA_TYPES[UA_TYPES_DOUBLE]))) /*TimeToNextKey*/
        return UA_STATUSCODE_BADTYPEMISMATCH;

    UA_StatusCode retval = UA_STATUSCODE_BAD;
    UA_Duration callbackTime;
    UA_String *securityGroupId = (UA_String *)input[0].data;
    UA_String *securityPolicyUri = (UA_String *)input[1].data;
    UA_UInt32 currentKeyId = *(UA_UInt32 *)input[2].data;
    UA_ByteString *currentKey = (UA_ByteString *)input[3].data;
    UA_ByteString *futureKeys = (UA_ByteString *)input[4].data;
    size_t futureKeySize = input[4].arrayLength;
    UA_Duration msTimeToNextKey = *(UA_Duration *)input[5].data;
    UA_Duration msKeyLifeTime = *(UA_Duration *)input[6].data;

    UA_PubSubKeyStorage *ks =
        UA_Server_findKeyStorage(server, *securityGroupId);
    if(!ks)
        return UA_STATUSCODE_BADNOTFOUND;

    if(!UA_String_equal(securityPolicyUri, &ks->policy->policyUri))
        return UA_STATUSCODE_BADSECURITYPOLICYREJECTED;

    if(ks->keyListSize == 0) {
        retval = UA_PubSubKeyStorage_storeSecurityKeys(
            server, ks, currentKeyId, currentKey, futureKeys, futureKeySize,
            msKeyLifeTime);
        if(retval != UA_STATUSCODE_GOOD)
            return retval;
    } else {
        retval = UA_PubSubKeyStorage_update(server, ks, currentKey, currentKeyId,
                                            futureKeySize, futureKeys, msKeyLifeTime);
        if(retval != UA_STATUSCODE_GOOD)
            return retval;
    }

    retval = UA_PubSubKeyStorage_activateKeyToChannelContext(server, UA_NODEID_NULL,
                                                           ks->securityGroupID);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_INFO(
            &server->config.logger, UA_LOGCATEGORY_SERVER,
            "Failed to import Symmetric Keys into PubSub Channel Context with %s \n",
            UA_StatusCode_name(retval));
        return retval;
    }

    callbackTime = msKeyLifeTime;
    if(msTimeToNextKey > 0)
        callbackTime = msTimeToNextKey;

    /*move to setSecurityKeysAction*/
    retval = UA_PubSubKeyStorage_addKeyRolloverCallback(
        server, ks, (UA_ServerCallback)UA_PubSubKeyStorage_keyRolloverCallback, callbackTime,
        &ks->callBackId);
    return retval;
}

#endif

#if defined(UA_ENABLE_PUBSUB_SKS) && defined(UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS)
static UA_StatusCode
getSecurityKeysAction(UA_Server *server, const UA_NodeId *sessionId, void *sessionHandle,
                      const UA_NodeId *methodId, void *methodContext,
                      const UA_NodeId *objectId, void *objectContext, size_t inputSize,
                      const UA_Variant *input, size_t outputSize, UA_Variant *output) {
    /*Check whether the channel is encrypted according to specification*/
    session_list_entry *session_entry;
    LIST_FOREACH(session_entry, &server->sessions, pointers) {
        if(UA_NodeId_equal(&session_entry->session.sessionId, sessionId)) {
            if(session_entry->session.header.channel->securityMode !=
               UA_MESSAGESECURITYMODE_SIGNANDENCRYPT)
                return UA_STATUSCODE_BADSECURITYMODEINSUFFICIENT;
        }
    }

    if(!server || !input)
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    if(inputSize < 3 || outputSize < 5)
        return UA_STATUSCODE_BADARGUMENTSMISSING;
    if(inputSize > 3 || outputSize > 5)
        return UA_STATUSCODE_BADTOOMANYARGUMENTS;

    /*check for types*/
    if(!UA_Variant_hasScalarType(&input[0],
                                 &UA_TYPES[UA_TYPES_STRING]) || /*SecurityGroupId*/
       !UA_Variant_hasScalarType(&input[1],
                                 &UA_TYPES[UA_TYPES_UINT32]) || /*StartingTokenId*/
       !UA_Variant_hasScalarType(&input[2],
                                 &UA_TYPES[UA_TYPES_UINT32])) /*RequestedKeyCount*/
        return UA_STATUSCODE_BADTYPEMISMATCH;

    UA_StatusCode retval = UA_STATUSCODE_BAD;
    UA_UInt32 currentKeyCount = 1;
    /* input */
    UA_String *securityGroupId = (UA_String *)input[0].data;
    UA_UInt32 startingTokenId = *(UA_UInt32 *)input[1].data;
    UA_UInt32 requestedKeyCount = *(UA_UInt32 *)input[2].data;

    UA_PubSubKeyStorage *ks =
        UA_Server_findKeyStorage(server, *securityGroupId);
    if(!ks)
        return UA_STATUSCODE_BADNOTFOUND;

    UA_Boolean executable = false;
    UA_SecurityGroup *sg = UA_SecurityGroup_findSGbyName(server, *securityGroupId);
    void *sgNodeCtx;
    getNodeContext(server, sg->securityGroupNodeId, (void **)&sgNodeCtx);
    executable = server->config.accessControl.getUserExecutableOnObject(
        server, &server->config.accessControl, sessionId, sessionHandle, methodId,
        methodContext, &sg->securityGroupNodeId, sgNodeCtx);

    if(!executable)
        return UA_STATUSCODE_BADUSERACCESSDENIED;

    /* If the caller requests a number larger than the Security Key Service permits, then
     * the SKS shall return the maximum it allows.*/
    if(requestedKeyCount > sg->config.maxFutureKeyCount)
        requestedKeyCount =(UA_UInt32) sg->keyStorage->keyListSize;
    else
        requestedKeyCount = requestedKeyCount + currentKeyCount; /* Add Current keyCount */

    /*The current token is requested by passing 0.*/
    UA_PubSubKeyListItem *startingItem = NULL;
    if(startingTokenId == 0) {
        /* currentItem is always set by the server when a security group is added */
        UA_assert(sg->keyStorage->currentItem != NULL);
        startingItem = sg->keyStorage->currentItem;
    } else {
        retval = UA_PubSubKeyStorage_getKeyByKeyID(
            startingTokenId, sg->keyStorage, &startingItem);
        /*If the StartingTokenId is unknown, the oldest (firstItem) available tokens are
         * returned. */
        if(retval == UA_STATUSCODE_BADNOTFOUND)
            startingItem =  TAILQ_FIRST(&sg->keyStorage->keyList);
    }

    /*SecurityPolicyUri*/
    retval = UA_Variant_setScalarCopy(&output[0], &(sg->keyStorage->policy->policyUri),
                         &UA_TYPES[UA_TYPES_STRING]);
    if(retval != UA_STATUSCODE_GOOD)
        return retval;

    /*FirstTokenId*/
    retval = UA_Variant_setScalarCopy(&output[1], &startingItem->keyID,
                                      &UA_TYPES[UA_TYPES_INTEGERID]);
    if(retval != UA_STATUSCODE_GOOD)
        return retval;

    /*TimeToNextKey*/
    UA_DateTime baseTime = sg->baseTime;
    UA_DateTime currentTime = UA_DateTime_nowMonotonic();
    UA_Duration interval = sg->config.keyLifeTime;
    UA_Duration timeToNextKey =
        (UA_Duration)((currentTime - baseTime) / UA_DATETIME_MSEC);
    timeToNextKey = interval - timeToNextKey;
    retval = UA_Variant_setScalarCopy(&output[3], &timeToNextKey,
                                      &UA_TYPES[UA_TYPES_DURATION]);
    if(retval != UA_STATUSCODE_GOOD)
        return retval;

    /*KeyLifeTime*/
    retval = UA_Variant_setScalarCopy(&output[4], &sg->config.keyLifeTime,
                         &UA_TYPES[UA_TYPES_DURATION]);
    if(retval != UA_STATUSCODE_GOOD)
        return retval;

    /*Keys*/
    UA_PubSubKeyListItem *iterator = startingItem;
    output[2].data = (UA_ByteString *)UA_calloc(requestedKeyCount, startingItem->key.length);
    if(!output[2].data)
        return UA_STATUSCODE_BADOUTOFMEMORY;

    UA_ByteString *requestedKeys = (UA_ByteString *)output[2].data;
    UA_UInt32 retkeyCount = 0;
    for(size_t i = 0; i < requestedKeyCount; i++) {
        UA_ByteString_copy(&iterator->key, &requestedKeys[i]);
        ++retkeyCount;
        iterator = TAILQ_NEXT(iterator, keyListEntry);
        if(!iterator) {
            requestedKeyCount = retkeyCount;
            break;
        }
    }

    UA_Variant_setArray(&output[2], requestedKeys, requestedKeyCount, &UA_TYPES[UA_TYPES_BYTESTRING]);
    return retval;
}
#endif

/**********************************************/
/*                Destructors                 */
/**********************************************/

static void
connectionTypeDestructor(UA_Server *server,
                         const UA_NodeId *sessionId, void *sessionContext,
                         const UA_NodeId *typeId, void *typeContext,
                         const UA_NodeId *nodeId, void **nodeContext) {
    UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_USERLAND,
                "Connection destructor called!");
    UA_NodeId publisherIdNode =
        findSingleChildNode(server, UA_QUALIFIEDNAME(0, "PublisherId"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY), *nodeId);
    UA_NodePropertyContext *ctx;
    UA_Server_getNodeContext(server, publisherIdNode, (void **)&ctx);
    if(!UA_NodeId_isNull(&publisherIdNode))
        UA_free(ctx);
}

static void
writerGroupTypeDestructor(UA_Server *server,
                          const UA_NodeId *sessionId, void *sessionContext,
                          const UA_NodeId *typeId, void *typeContext,
                          const UA_NodeId *nodeId, void **nodeContext) {
    UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_USERLAND,
                "WriterGroup destructor called!");
    UA_NodeId intervalNode =
        findSingleChildNode(server, UA_QUALIFIEDNAME(0, "PublishingInterval"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY), *nodeId);
    UA_NodePropertyContext *ctx;
    UA_Server_getNodeContext(server, intervalNode, (void **)&ctx);
    if(!UA_NodeId_isNull(&intervalNode))
        UA_free(ctx);
}

static void
readerGroupTypeDestructor(UA_Server *server,
                          const UA_NodeId *sessionId, void *sessionContext,
                          const UA_NodeId *typeId, void *typeContext,
                          const UA_NodeId *nodeId, void **nodeContext) {
    UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_USERLAND,
                "ReaderGroup destructor called!");
}

static void
dataSetWriterTypeDestructor(UA_Server *server,
                            const UA_NodeId *sessionId, void *sessionContext,
                            const UA_NodeId *typeId, void *typeContext,
                            const UA_NodeId *nodeId, void **nodeContext) {
    UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_USERLAND,
                "DataSetWriter destructor called!");
    UA_NodeId dataSetWriterIdNode =
        findSingleChildNode(server, UA_QUALIFIEDNAME(0, "DataSetWriterId"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY), *nodeId);
    UA_NodePropertyContext *ctx;
    UA_Server_getNodeContext(server, dataSetWriterIdNode, (void **)&ctx);
    if(!UA_NodeId_isNull(&dataSetWriterIdNode))
        UA_free(ctx);
}

static void
dataSetReaderTypeDestructor(UA_Server *server,
                            const UA_NodeId *sessionId, void *sessionContext,
                            const UA_NodeId *typeId, void *typeContext,
                            const UA_NodeId *nodeId, void **nodeContext) {
    UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_USERLAND,
                "DataSetReader destructor called!");

    /* Deallocate the memory allocated for publisherId */
    UA_NodeId publisherIdNode =
        findSingleChildNode(server, UA_QUALIFIEDNAME(0, "PublisherId"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY), *nodeId);
    UA_NodePropertyContext *ctx;
    UA_Server_getNodeContext(server, publisherIdNode, (void **)&ctx);
    if(!UA_NodeId_isNull(&publisherIdNode))
        UA_free(ctx);
}

static void
publishedDataItemsTypeDestructor(UA_Server *server,
                            const UA_NodeId *sessionId, void *sessionContext,
                            const UA_NodeId *typeId, void *typeContext,
                            const UA_NodeId *nodeId, void **nodeContext) {
    UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_USERLAND,
                "PublishedDataItems destructor called!");
    void *childContext;
    UA_NodeId node = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "PublishedData"),
                                         UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY), *nodeId);
    UA_Server_getNodeContext(server, node, (void**)&childContext);
    if(!UA_NodeId_isNull(&node))
        UA_free(childContext);

    node = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "ConfigurationVersion"),
                               UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                               *nodeId);
    UA_Server_getNodeContext(server, node, (void**)&childContext);
    if(!UA_NodeId_isNull(&node))
        UA_free(childContext);

    node = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "DataSetMetaData"),
                               UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY), *nodeId);
    UA_Server_getNodeContext(server, node, (void**)&childContext);
    if(!UA_NodeId_isNull(&node))
        UA_free(childContext);
}

static void
standaloneSubscribedDataSetTypeDestructor(UA_Server *server,
                            const UA_NodeId *sessionId, void *sessionContext,
                            const UA_NodeId *typeId, void *typeContext,
                            const UA_NodeId *nodeId, void **nodeContext) {
    UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_USERLAND,
                "Standalone SubscribedDataSet destructor called!");
    void *childContext;

    UA_NodeId node =
        findSingleChildNode(server, UA_QUALIFIEDNAME(0, "DataSetMetaData"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY), *nodeId);
    UA_Server_getNodeContext(server, node, (void**)&childContext);
    if(!UA_NodeId_equal(&UA_NODEID_NULL , &node))
        UA_free(childContext);

    node = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "IsConnected"),
                               UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                               *nodeId);
    UA_Server_getNodeContext(server, node, (void**)&childContext);
    if(!UA_NodeId_equal(&UA_NODEID_NULL , &node))
        UA_free(childContext);
}

/*************************************/
/*         PubSub configurator       */
/*************************************/

#if defined(UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS) && defined(UA_ENABLE_PUBSUB_FILE_CONFIG)

/* Callback function that will be executed when the method "PubSub configurator
 * (replace config)" is called. */
static UA_StatusCode
UA_loadPubSubConfigMethodCallback(UA_Server *server,
                                  const UA_NodeId *sessionId, void *sessionHandle,
                                  const UA_NodeId *methodId, void *methodContext,
                                  const UA_NodeId *objectId, void *objectContext,
                                  size_t inputSize, const UA_Variant *input,
                                  size_t outputSize, UA_Variant *output) {
    if(inputSize == 1) {
        UA_ByteString *inputStr = (UA_ByteString*)input->data;
        return UA_PubSubManager_loadPubSubConfigFromByteString(server, *inputStr);
    } else if(inputSize > 1) {
        return UA_STATUSCODE_BADTOOMANYARGUMENTS;
    } else {
        return UA_STATUSCODE_BADARGUMENTSMISSING;
    }
}

/* Adds method node to server. This method is used to load binary files for
 * PubSub configuration and delete / replace old PubSub configurations. */
static UA_StatusCode
UA_addLoadPubSubConfigMethod(UA_Server *server) {
    UA_Argument inputArgument;
    UA_Argument_init(&inputArgument);
    inputArgument.description = UA_LOCALIZEDTEXT("", "PubSub config binfile");
    inputArgument.name = UA_STRING("BinFile");
    inputArgument.dataType = UA_TYPES[UA_TYPES_BYTESTRING].typeId;
    inputArgument.valueRank = UA_VALUERANK_SCALAR;

    UA_MethodAttributes configAttr = UA_MethodAttributes_default;
    configAttr.description = UA_LOCALIZEDTEXT("","Load binary configuration file");
    configAttr.displayName = UA_LOCALIZEDTEXT("","LoadPubSubConfigurationFile");
    configAttr.executable = true;
    configAttr.userExecutable = true;
    return UA_Server_addMethodNode(server, UA_NODEID_NULL,
                                   UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE),
                                   UA_NODEID_NUMERIC(0, UA_NS0ID_HASORDEREDCOMPONENT),
                                   UA_QUALIFIEDNAME(1, "PubSub configuration"),
                                   configAttr, &UA_loadPubSubConfigMethodCallback,
                                   1, &inputArgument, 0, NULL, NULL, NULL);
}

/* Callback function that will be executed when the method "PubSub configurator
 *  (delete config)" is called. */
static UA_StatusCode
UA_deletePubSubConfigMethodCallback(UA_Server *server,
                                    const UA_NodeId *sessionId, void *sessionHandle,
                                    const UA_NodeId *methodId, void *methodContext,
                                    const UA_NodeId *objectId, void *objectContext,
                                    size_t inputSize, const UA_Variant *input,
                                    size_t outputSize, UA_Variant *output) {
    UA_PubSubManager_delete(server, &(server->pubSubManager));
    return UA_STATUSCODE_GOOD;
}

/* Adds method node to server. This method is used to delete the current PubSub
 * configuration. */
static UA_StatusCode
UA_addDeletePubSubConfigMethod(UA_Server *server) {
    UA_MethodAttributes configAttr = UA_MethodAttributes_default;
    configAttr.description = UA_LOCALIZEDTEXT("","Delete current PubSub configuration");
    configAttr.displayName = UA_LOCALIZEDTEXT("","DeletePubSubConfiguration");
    configAttr.executable = true;
    configAttr.userExecutable = true;
    return UA_Server_addMethodNode(server, UA_NODEID_NULL,
                                   UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE),
                                   UA_NODEID_NUMERIC(0, UA_NS0ID_HASORDEREDCOMPONENT),
                                   UA_QUALIFIEDNAME(1, "Delete PubSub config"),
                                   configAttr, &UA_deletePubSubConfigMethodCallback,
                                   0, NULL, 0, NULL, NULL, NULL);
}

#endif /* defined(UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS) && defined(UA_ENABLE_PUBSUB_FILE_CONFIG) */

UA_StatusCode
UA_Server_initPubSubNS0(UA_Server *server) {
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_String profileArray[1];
    profileArray[0] = UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp");

    retVal |= writePubSubNs0VariableArray(server, UA_NS0ID_PUBLISHSUBSCRIBE_SUPPORTEDTRANSPORTPROFILES,
                                    profileArray, 1, &UA_TYPES[UA_TYPES_STRING]);

    UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
    oAttr.displayName = UA_LOCALIZEDTEXT("", "SubscribedDataSets");
    UA_Server_addObjectNode(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_SUBSCRIBEDDATASETS),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE), UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                            UA_QUALIFIEDNAME(0, "SubscribedDataSets"), UA_NODEID_NUMERIC(0, UA_NS0ID_SUBSCRIBEDDATASETFOLDERTYPE),
                            oAttr, NULL, NULL);
#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
    /* Add missing references */
    retVal |= UA_Server_addReference(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_PUBLISHEDDATASETS),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_ADDDATASETFOLDER), true);
    retVal |= UA_Server_addReference(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_PUBLISHEDDATASETS),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_ADDPUBLISHEDDATAITEMS), true);
    retVal |= UA_Server_addReference(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_PUBLISHEDDATASETS),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_REMOVEPUBLISHEDDATASET), true);
    retVal |= UA_Server_addReference(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_PUBLISHEDDATASETS),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_REMOVEDATASETFOLDER), true);

    /* Set method callbacks */
    retVal |= UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_ADDCONNECTION), addPubSubConnectionAction);
    retVal |= UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_REMOVECONNECTION), removeConnectionAction);
    retVal |= UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_ADDDATASETFOLDER), addDataSetFolderAction);
    retVal |= UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_REMOVEDATASETFOLDER), removeDataSetFolderAction);
    retVal |= UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_ADDPUBLISHEDDATAITEMS), addPublishedDataItemsAction);
    retVal |= UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_REMOVEPUBLISHEDDATASET), removePublishedDataSetAction);
    retVal |= UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHEDDATAITEMSTYPE_ADDVARIABLES), addVariablesAction);
    retVal |= UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHEDDATAITEMSTYPE_REMOVEVARIABLES), removeVariablesAction);
    retVal |= UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBSUBCONNECTIONTYPE_ADDWRITERGROUP), addWriterGroupAction);
    retVal |= UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBSUBCONNECTIONTYPE_ADDREADERGROUP), addReaderGroupAction);
    retVal |= UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBSUBCONNECTIONTYPE_REMOVEGROUP), removeGroupAction);
    retVal |= UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, UA_NS0ID_WRITERGROUPTYPE_ADDDATASETWRITER), addDataSetWriterAction);
    retVal |= UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, UA_NS0ID_WRITERGROUPTYPE_REMOVEDATASETWRITER), removeDataSetWriterAction);
    retVal |= UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, UA_NS0ID_READERGROUPTYPE_ADDDATASETREADER), addDataSetReaderAction);
    retVal |= UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, UA_NS0ID_READERGROUPTYPE_REMOVEDATASETREADER), removeDataSetReaderAction);
    retVal |= UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_PUBSUBCONFIGURATION_RESERVEIDS), addReserveIdsAction);
#ifdef UA_ENABLE_PUBSUB_SKS
    retVal |= UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_SETSECURITYKEYS), setSecurityKeysAction);
    retVal |= UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_GETSECURITYKEYS), getSecurityKeysAction);
#endif

#ifdef UA_ENABLE_PUBSUB_FILE_CONFIG
    retVal |= UA_addLoadPubSubConfigMethod(server);
    retVal |= UA_addDeletePubSubConfigMethod(server);
#endif

#else
    /* Remove methods */
    retVal |= UA_Server_deleteReference(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), true,
                                        UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_ADDCONNECTION),
                                        false);
    retVal |= UA_Server_deleteReference(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), true,
                                        UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_REMOVECONNECTION),
                                        false);
#endif

    /* Set the object-type destructors */
    UA_NodeTypeLifecycle lifeCycle;
    lifeCycle.constructor = NULL;

    lifeCycle.destructor = connectionTypeDestructor;
    retVal |= UA_Server_setNodeTypeLifecycle(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBSUBCONNECTIONTYPE), lifeCycle);

    lifeCycle.destructor = writerGroupTypeDestructor;
    retVal |= UA_Server_setNodeTypeLifecycle(server, UA_NODEID_NUMERIC(0, UA_NS0ID_WRITERGROUPTYPE), lifeCycle);

    lifeCycle.destructor = readerGroupTypeDestructor;
    retVal |= UA_Server_setNodeTypeLifecycle(server, UA_NODEID_NUMERIC(0, UA_NS0ID_READERGROUPTYPE), lifeCycle);

    lifeCycle.destructor = dataSetWriterTypeDestructor;
    retVal |= UA_Server_setNodeTypeLifecycle(server, UA_NODEID_NUMERIC(0, UA_NS0ID_DATASETWRITERTYPE), lifeCycle);

    lifeCycle.destructor = publishedDataItemsTypeDestructor;
    retVal |= UA_Server_setNodeTypeLifecycle(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHEDDATAITEMSTYPE), lifeCycle);

    lifeCycle.destructor = dataSetReaderTypeDestructor;
    retVal |= UA_Server_setNodeTypeLifecycle(server, UA_NODEID_NUMERIC(0, UA_NS0ID_DATASETREADERTYPE), lifeCycle);

    lifeCycle.destructor = standaloneSubscribedDataSetTypeDestructor;
    UA_Server_setNodeTypeLifecycle(server, UA_NODEID_NUMERIC(0, UA_NS0ID_STANDALONESUBSCRIBEDDATASETTYPE),lifeCycle);

    return retVal;
}

UA_StatusCode
connectDataSetReaderToDataSet(UA_Server *server, UA_NodeId dsrId, UA_NodeId standaloneSdsId) {
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;

    UA_NodeId dataSetMetaDataOnDsrId = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "DataSetMetaData"),
                                UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY), dsrId);
    UA_NodeId subscribedDataSetOnDsrId = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "SubscribedDataSet"),
                                UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), dsrId);
    UA_NodeId dataSetMetaDataOnSdsId = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "DataSetMetaData"),
                                UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY), standaloneSdsId);
    UA_NodeId subscribedDataSetOnSdsId = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "SubscribedDataSet"),
                                UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), standaloneSdsId);

    if (UA_NodeId_isNull(&dataSetMetaDataOnDsrId) || UA_NodeId_isNull(&subscribedDataSetOnDsrId)
        || UA_NodeId_isNull(&dataSetMetaDataOnSdsId) || UA_NodeId_isNull(&subscribedDataSetOnSdsId))
    {
        return UA_STATUSCODE_BADNOTFOUND;
    }

    UA_NODESTORE_REMOVE(server, &dataSetMetaDataOnDsrId);
    UA_NODESTORE_REMOVE(server, &subscribedDataSetOnDsrId);

    retVal |= UA_Server_addReference(server, dsrId, UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
        UA_EXPANDEDNODEID_NUMERIC(dataSetMetaDataOnSdsId.namespaceIndex, dataSetMetaDataOnSdsId.identifier.numeric), true);
    retVal |= UA_Server_addReference(server, dsrId, UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
        UA_EXPANDEDNODEID_NUMERIC(subscribedDataSetOnSdsId.namespaceIndex, subscribedDataSetOnSdsId.identifier.numeric), true);

    return retVal;
}

#endif /* UA_ENABLE_PUBSUB_INFORMATIONMODEL */
