/*##############################################################################

    Copyright (C) 2011 HPCC Systems.

    All rights reserved. This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
############################################################################## */

#ifndef _THGRAPH_HPP
#define _THGRAPH_HPP

#ifdef _WIN32
    #ifdef GRAPH_EXPORTS
        #define graph_decl __declspec(dllexport)
    #else
        #define graph_decl __declspec(dllimport)
    #endif
#else
    #define graph_decl
#endif

#undef barrier

#define LONGTIMEOUT (25*60*1000)
#define MEDIUMTIMEOUT 30000

#include "jlib.hpp"
#include "jarray.tpp"
#include "jexcept.hpp"
#include "jhash.hpp"
#include "jsuperhash.hpp"
#include "jset.hpp"

#include "mpcomm.hpp"

#include "mptag.hpp"

#include "thor.hpp"
#include "eclhelper.hpp"
#include "thmem.hpp"
#include "thorplugin.hpp"

#define THORDATALINK_STOPPED            (RCMAX&~(RCMAX>>1))                         // dataLinkStop() was called
#define THORDATALINK_STARTED            (RCMAX&~THORDATALINK_STOPPED&~(RCMAX>>2))   // dataLinkStart() was called
#define THORDATALINK_COUNT_MASK         (RCMAX>>2)                                  // mask to extract count value only



enum ActivityAttributes { ActAttr_Source=1, ActAttr_Sink=2 };

#define INVALID_UNIQ_ID -1;
typedef activity_id unique_id;

enum msgids
{
    QueryInit,
    QueryDone,
    Shutdown,
    GraphInit,
    GraphEnd,
    GraphAbort
};

interface ICodeContextExt : extends ICodeContext
{
    virtual IConstWUResult *getExternalResult(const char * wuid, const char *name, unsigned sequence) = 0;
    virtual IConstWUResult *getResultForGet(const char *name, unsigned sequence) = 0;
};

interface IDiskUsage : extends IInterface
{
    virtual void increase(offset_t usage, const char *key=NULL) = 0;
    virtual void decrease(offset_t usage, const char *key=NULL) = 0;
};

interface IBackup;
interface IFileInProgressHandler;
interface IThorFileCache;
interface IThorResource
{
    virtual IThorFileCache &queryFileCache() = 0;
    virtual IBackup &queryBackup() = 0;
    virtual IFileInProgressHandler &queryFileInProgressHandler() = 0;
};

interface IBarrier : extends IInterface
{
    virtual const mptag_t queryTag() const = 0;
    virtual bool wait(bool exception, unsigned timeout=INFINITE) = 0;
    virtual void cancel() = 0;
};

graph_decl IThorResource &queryThor();
graph_decl void setIThorResource(IThorResource &r);

interface IRowWriterMultiReader;
interface IThorResult : extends IInterface
{
    virtual IRowWriter *getWriter() = 0;
    virtual void setResultStream(IRowWriterMultiReader *stream, rowcount_t count) = 0;
    virtual IRowStream *getRowStream() = 0;
    virtual IOutputMetaData *queryMeta() = 0;
    virtual const bool isLocal() const = 0;
    virtual void getResult(size32_t & retSize, void * & ret) = 0;
    virtual void getLinkedResult(unsigned & count, byte * * & ret) = 0;
};

// JCSMORE - based on IHThorGraphResults
interface IThorGraphResults : extends IEclGraphResults
{
    virtual void clear() = 0;
    virtual IThorResult *queryResult(unsigned id) = 0;
    virtual IThorResult *getResult(unsigned id) = 0;
    virtual IThorResult *createResult(CActivityBase &activity, unsigned id, IRowInterfaces *rowIf, bool local=false) = 0;
    virtual IThorResult *createResult(CActivityBase &activity, IRowInterfaces *rowIf, bool local=false) = 0;
    virtual void setResult(unsigned id, IThorResult *result) = 0;
    virtual unsigned count() = 0;
};

class CGraphBase;
interface IThorBoundLoopGraph : extends IInterface
{
    virtual void prepareLoopResults(CActivityBase &activity, IThorGraphResults *results) = 0;
    virtual void prepareCounterResult(CActivityBase &activity, IThorGraphResults *results, unsigned loopCounter, unsigned pos) = 0;
    virtual IRowStream *execute(CActivityBase &activity, unsigned counter, IRowWriterMultiReader *rowStream, unsigned rowStreamCount, size32_t parentExtractSz, const byte * parentExtract) = 0;
    virtual void execute(CActivityBase &activity, unsigned counter, IThorGraphResults * graphLoopResults, size32_t parentExtractSz, const byte * parentExtract) = 0;
    virtual CGraphBase *queryGraph() = 0;
};

class CFileUsageEntry : public CInterface
{
    StringAttr name;
    unsigned usage;
    graph_id graphId;
    WUFileKind fileKind;
public:
    CFileUsageEntry(const char *_name, graph_id _graphId, WUFileKind _fileKind, unsigned _usage) :name(_name), graphId(_graphId), fileKind(_fileKind), usage(_usage) { }
    const unsigned queryUsage() const { return usage; }
    const graph_id queryGraphId() const { return graphId; }
    const WUFileKind queryKind() const { return fileKind; }
    const char *queryName() const { return name.get(); }
    void decUsage() { --usage; }

    const char *queryFindString() const { return name; }
};

interface IFileUsageIterator : extends IIteratorOf<CFileUsageEntry> 
{
};
interface IGraphTempHandler : extends IInterface
{
    virtual void registerFile(const char *name, graph_id graphId, unsigned usageCount, bool temp, WUFileKind fileKind=WUFileStandard, StringArray *clusters=NULL) = 0;
    virtual void deregisterFile(const char *name, bool kept=false) = 0;
    virtual void clearTemps() = 0;
    virtual IFileUsageIterator *getIterator() = 0;
};

class CGraphDependency : public CInterface
{
public:
    Linked<CGraphBase> graph;
    int controlId;

    CGraphDependency(CGraphBase *_graph, int _controlId) : graph(_graph), controlId(_controlId) { }
};

typedef CIArrayOf<CGraphBase> CGraphArray;
typedef CIArrayOf<CGraphDependency> CGraphDependencyArray;

class CActivityBase;
class CJobBase;
interface IThorGraphIterator;
interface IThorGraphDependencyIterator;
class graph_decl CGraphElementBase : public CInterface, implements IInterface
{
protected:
    Owned<IHThorArg> baseHelper;
    ThorActivityKind kind;
    activity_id id, ownerId;
    UnsignedArray outputInputIndexes;


    StringAttr eclText;
    Owned<IPropertyTree> xgmml;
    bool isLocal, isGrouped, sink, prepared, onCreateCalled, onStartCalled, onlyUpdateIfChanged, nullAct;
    Owned<CActivityBase> activity;
    CGraphBase *resultsGraph, *owner;
    CGraphDependencyArray dependsOn;
    Owned<IThorBoundLoopGraph> loopGraph; // really only here as master and slave derivatives set/use
    MemoryBuffer createCtxMb, startCtxMb;
    bool haveCreateCtx, haveStartCtx;

public:
    IMPLEMENT_IINTERFACE;

    bool alreadyUpdated, isEof, newWhichBranch;
    EclHelperFactory helperFactory;

    UnsignedArray connectedInputsIndex;
    CIArrayOf<CGraphElementBase> connectedInputs;
    UnsignedArray connectedInputsInputOutIdx;

    UnsignedArray connectedOutputsIndex;
    CopyCIArrayOf<CGraphElementBase> connectedOutputs;
    UnsignedArray connectedOutputsInputIndex;

    CopyCIArrayOf<CGraphElementBase> outputs;
    CGraphArray associatedChildGraphs;
    CIArrayOf<CGraphElementBase> inputs;
    UnsignedArray inputOutIndexes;
    unsigned whichBranch;
    Owned<IBitSet> whichBranchBitSet;
    Owned<IBitSet> sentActInitData;

    CGraphElementBase(CGraphBase &_owner, IPropertyTree &_xgmml);
    ~CGraphElementBase();

    void doconnect();
    void addInput(CGraphElementBase *input, unsigned inputOutIdx)
    {
        inputs.append(*LINK(input));
        inputOutIndexes.append(inputOutIdx); // the OUTPUT index *from* the source input
    }
    void addOutput(CGraphElementBase *output, unsigned outputInIdx)
    {
        outputs.append(* output);
        outputInputIndexes.append(outputInIdx); // the INPUT index *on* the target
    }
    void removeInput(unsigned which);
    virtual void setInput(unsigned which, CGraphElementBase &input, unsigned inputOutIdx);
    void setResultsGraph(CGraphBase *_resultsGraph) { resultsGraph = _resultsGraph; }
    void addAssociatedChildGraph(CGraphBase *childGraph) { associatedChildGraphs.append(*LINK(childGraph)); }
    void releaseIOs();
    void addDependsOn(CGraphBase *graph, int controlId);
    IThorGraphDependencyIterator *getDependsIterator();
    void ActPrintLog(const char *format, ...)  __attribute__((format(printf, 2, 3)));
    void ActPrintLog(IException *e, const char *format, ...) __attribute__((format(printf, 3, 4)));

    void setBoundGraph(IThorBoundLoopGraph *graph) { loopGraph.set(graph); }
    IThorBoundLoopGraph *queryLoopGraph() { return loopGraph; }
    bool executeDependencies(size32_t parentExtractSz, const byte *parentExtract, int controlId, bool async);
    virtual void deserializeCreateContext(MemoryBuffer &mb);
    virtual void deserializeStartContext(MemoryBuffer &mb);
    virtual void serializeCreateContext(MemoryBuffer &mb); // called after onCreate and create() (of activity)
    virtual void serializeStartContext(MemoryBuffer &mb);
    virtual bool checkUpdate() { return false; }
    virtual void reset();
    void onStart(size32_t parentExtractSz, const byte *parentExtract);
    void onCreate();
    void abort(IException *e);
    virtual void preStart(size32_t parentExtractSz, const byte *parentExtract);
    virtual void start();
    const bool &isOnCreated() const { return onCreateCalled; }
    const bool &isPrepared() const { return prepared; }
    CGraphBase &queryOwner() const { return *owner; }
    CGraphBase *queryResultsGraph() const { return resultsGraph; }
    IThorGraphIterator *getAssociatedChildGraphs();
    IGraphTempHandler *queryTempHandler() const;
    CJobBase &queryJob() const;
    unsigned getInputs() const { return inputs.ordinality(); } 
    unsigned getOutputs() const { return outputs.ordinality(); }
    bool isSource() const { return isActivitySource(kind); }
    bool isSink() const { return sink; }
    bool queryLocal() const { return isLocal; }
    bool queryGrouped() const { return isGrouped; }
    bool queryLocalOrGrouped() { return isLocal || isGrouped; }

    CGraphElementBase *queryInput(unsigned index) const
    {
        if (inputs.isItem(index))
            return &inputs.item(index);
        return NULL;
    }
    IHThorArg *queryHelper() const { return baseHelper; }

    IPropertyTree &queryXGMML() const { return *xgmml; }
    CActivityBase *queryActivity() const { return activity; }
    const activity_id &queryOwnerId() const { return ownerId; }
    void createActivity(size32_t parentExtractSz, const byte *parentExtract);
//
    const ThorActivityKind getKind() const { return kind; }
    const activity_id &queryId() const { return id; }
    StringBuffer &getEclText(StringBuffer& dst) const
    {
        dst.append(eclText.get());
        return dst;
    }
    virtual bool prepareContext(size32_t parentExtractSz, const byte *parentExtract, bool checkDependencies, bool shortCircuit, bool async);
//
    virtual void initActivity();
    virtual CActivityBase *factory(ThorActivityKind kind) { assertex(false); return NULL; }
    virtual CActivityBase *factory() { return factory(getKind()); }
    virtual CActivityBase *factorySet(ThorActivityKind kind) { CActivityBase *_activity = factory(kind); activity.setown(_activity); return _activity; }
    virtual ICodeContext *queryCodeContext();
};

typedef CIArrayOf<CGraphElementBase> CGraphElementArray;
interface IThorActivityIterator : extends IIteratorOf<CGraphElementBase> { };

class graph_decl CGraphElementTableCopy : public SuperHashTableOf<CGraphElementBase, activity_id>
{
public:
    IMPLEMENT_SUPERHASHTABLEOF_REF_FIND(CGraphElementBase, activity_id);

    ~CGraphElementTableCopy() { kill(); }

    virtual void onAdd(void *et) { }
    virtual void onRemove(void *et) { }
    virtual unsigned getHashFromElement(const void *et) const
    {
        return hashc((const unsigned char *) &(((CGraphElementBase *) et)->queryId()), sizeof(activity_id), 0);
    }
    virtual unsigned getHashFromFindParam(const void *fp) const
    {
        return hashc((const unsigned char *) fp, sizeof(activity_id), 0);
    }
    virtual const void *getFindParam(const void *et) const
    {
        return &(((CGraphElementBase *)et)->queryId());
    }
    virtual bool matchesFindParam(const void *et, const void *fp, unsigned fphash) const
    {
        return (((CGraphElementBase *) et)->queryId()) == *(activity_id *)fp;
    }
};

class graph_decl CGraphElementTable : public CGraphElementTableCopy
{
public:
    ~CGraphElementTable() { kill(); }
    virtual void onRemove(void *et) { ((CGraphElementBase *)et)->Release(); }
};

class CGraphBase;
class graph_decl CGraphTableCopy : public SuperHashTableOf<CGraphBase, graph_id>
{
public:
    IMPLEMENT_SUPERHASHTABLEOF_REF_FIND(CGraphBase, graph_id);

    ~CGraphTableCopy() { kill(); }

    virtual void onAdd(void *et) { }
    virtual void onRemove(void *et) { }
    virtual unsigned getHashFromElement(const void *et) const;
    virtual unsigned getHashFromFindParam(const void *fp) const;
    virtual const void *getFindParam(const void *et) const;
    virtual bool matchesFindParam(const void *et, const void *fp, unsigned fphash) const;
};

class graph_decl CGraphTable : public CGraphTableCopy
{
public:
    ~CGraphTable() { kill(); }
    virtual void onRemove(void *et);
};

interface IThorGraphIterator : extends IIteratorOf<CGraphBase>
{
};

interface IThorGraphDependencyIterator : extends IIteratorOf<CGraphDependency>
{
};

// Stolen from eclagent.ipp 'EclCounterMeta'
class CThorEclCounterMeta : public CInterface, implements IOutputMetaData
{
public:
    IMPLEMENT_IINTERFACE

    virtual size32_t getRecordSize(const void *rec)         { return sizeof(thor_loop_counter_t); }
    virtual size32_t getMinRecordSize() const               { return sizeof(thor_loop_counter_t); }
    virtual size32_t getFixedSize() const                   { return sizeof(thor_loop_counter_t); }
    virtual void toXML(const byte * self, IXmlWriter & out) { }
    virtual unsigned getVersion() const                     { return OUTPUTMETADATA_VERSION; }
    virtual unsigned getMetaFlags()                         { return 0; }
    virtual void destruct(byte * self) {}
    virtual IOutputRowSerializer * createRowSerializer(ICodeContext * ctx, unsigned activityId) { return NULL; }
    virtual IOutputRowDeserializer * createRowDeserializer(ICodeContext * ctx, unsigned activityId) { return NULL; }
    virtual ISourceRowPrefetcher * createRowPrefetcher(ICodeContext * ctx, unsigned activityId) { return NULL; }
    virtual IOutputMetaData * querySerializedMeta() { return this; }
    virtual void walkIndirectMembers(const byte * self, IIndirectMemberVisitor & visitor) {}
};

typedef OwningStringSuperHashTableOf<CFileUsageEntry> CFileUsageTable;
class graph_decl CGraphTempHandler : public CInterface, implements IGraphTempHandler
{
protected:
    CFileUsageTable tmpFiles;
    CJobBase &job;
public:
    IMPLEMENT_IINTERFACE;

    CGraphTempHandler(CJobBase &_job) : job(_job) { }
    ~CGraphTempHandler()
    {
    }
    virtual void beforeDispose()
    {
        clearTemps();
    }
    virtual bool removeTemp(const char *name) = 0;
// IGraphTempHandler
    virtual void registerFile(const char *name, graph_id graphId, unsigned usageCount, bool temp, WUFileKind fileKind, StringArray *clusters);
    virtual void deregisterFile(const char *name, bool kept=false);
    virtual void clearTemps();
    virtual IFileUsageIterator *getIterator()
    {
        class CIterator : public CInterface, implements IFileUsageIterator
        {
            SuperHashIteratorOf<CFileUsageEntry> iter;
        public:
            IMPLEMENT_IINTERFACE;
            CIterator(CFileUsageTable &table) : iter(table) { }
            virtual bool first() { return iter.first(); }
            virtual bool next() { return iter.next(); }
            virtual bool isValid() { return iter.isValid(); }
            virtual CFileUsageEntry & query() { return iter.query(); }
        };
        return new CIterator(tmpFiles);
    }
};

interface IGraphCallback
{
    virtual void runSubgraph(CGraphBase &graph, size32_t parentExtractSz, const byte *parentExtract) = 0;
};

class CJobBase;
interface IPropertyTree;
class graph_decl CGraphBase : public CInterface, implements ILocalGraph, implements IThorChildGraph, implements IExceptionHandler
{
    CriticalSection crit;
    CriticalSection evaluateCrit;
    CGraphElementTable containers;
    CGraphElementArray sinks;
    bool sink, complete, global;
    activity_id parentActivityId;
    IPropertyTree *xgmml;
    CGraphTable childGraphs;
    Owned<IThorGraphResults> localResults, graphLoopResults;
    Owned<IGraphTempHandler> tmpHandler;

    void clean();

    class CGraphCodeContext : implements ICodeContextExt
    {
        ICodeContextExt *ctx;
        CGraphBase *graph;
    public:
        CGraphCodeContext() : graph(NULL), ctx(NULL) { }
        void setContext(CGraphBase *_graph, ICodeContextExt *_ctx)
        {
            graph = _graph;
            ctx = _ctx;
        }
        virtual const char *loadResource(unsigned id) { return ctx->loadResource(id); }
        virtual void setResultBool(const char *name, unsigned sequence, bool value) { ctx->setResultBool(name, sequence, value); }
        virtual void setResultData(const char *name, unsigned sequence, int len, const void * data) { ctx->setResultData(name, sequence, len, data); }
        virtual void setResultDecimal(const char * stepname, unsigned sequence, int len, int precision, bool isSigned, const void *val) { ctx->setResultDecimal(stepname, sequence, len, precision, isSigned, val); }
        virtual void setResultInt(const char *name, unsigned sequence, __int64 value) { ctx->setResultInt(name, sequence, value); }
        virtual void setResultRaw(const char *name, unsigned sequence, int len, const void * data) { ctx->setResultRaw(name, sequence, len, data); }
        virtual void setResultReal(const char * stepname, unsigned sequence, double value) { ctx->setResultReal(stepname, sequence, value); }
        virtual void setResultSet(const char *name, unsigned sequence, bool isAll, size32_t len, const void * data, ISetToXmlTransformer * transformer) { ctx->setResultSet(name, sequence, isAll, len, data, transformer); }
        virtual void setResultString(const char *name, unsigned sequence, int len, const char * str) { ctx->setResultString(name, sequence, len, str); }
        virtual void setResultUInt(const char *name, unsigned sequence, unsigned __int64 value) { ctx->setResultUInt(name, sequence, value); }
        virtual void setResultUnicode(const char *name, unsigned sequence, int len, UChar const * str) { ctx->setResultUnicode(name, sequence, len, str); }
        virtual void setResultVarString(const char * name, unsigned sequence, const char * value) { ctx->setResultVarString(name, sequence, value); }
        virtual void setResultVarUnicode(const char * name, unsigned sequence, UChar const * value) { ctx->setResultVarUnicode(name, sequence, value); }
        virtual bool getResultBool(const char * name, unsigned sequence) { return ctx->getResultBool(name, sequence); }
        virtual void getResultData(unsigned & tlen, void * & tgt, const char * name, unsigned sequence) { ctx->getResultData(tlen, tgt, name, sequence); }
        virtual void getResultDecimal(unsigned tlen, int precision, bool isSigned, void * tgt, const char * stepname, unsigned sequence) { ctx->getResultDecimal(tlen, precision, isSigned, tgt, stepname, sequence); }
        virtual void getResultRaw(unsigned & tlen, void * & tgt, const char * name, unsigned sequence, IXmlToRowTransformer * xmlTransformer, ICsvToRowTransformer * csvTransformer) { ctx->getResultRaw(tlen, tgt, name, sequence, xmlTransformer, csvTransformer); }
        virtual void getResultSet(bool & isAll, size32_t & tlen, void * & tgt, const char * name, unsigned sequence, IXmlToRowTransformer * xmlTransformer, ICsvToRowTransformer * csvTransformer) { ctx->getResultSet(isAll, tlen, tgt, name, sequence, xmlTransformer, csvTransformer); }
        virtual __int64 getResultInt(const char * name, unsigned sequence) { return ctx->getResultInt(name, sequence); }
        virtual double getResultReal(const char * name, unsigned sequence) { return ctx->getResultReal(name, sequence); }
        virtual void getResultString(unsigned & tlen, char * & tgt, const char * name, unsigned sequence) { ctx->getResultString(tlen, tgt, name, sequence); }
        virtual void getResultStringF(unsigned tlen, char * tgt, const char * name, unsigned sequence) { ctx->getResultStringF(tlen, tgt, name, sequence); }
        virtual void getResultUnicode(unsigned & tlen, UChar * & tgt, const char * name, unsigned sequence) { ctx->getResultUnicode(tlen, tgt, name, sequence); }
        virtual char *getResultVarString(const char * name, unsigned sequence) { return ctx->getResultVarString(name, sequence); }
        virtual UChar *getResultVarUnicode(const char * name, unsigned sequence) { return ctx->getResultVarUnicode(name, sequence); }
        virtual unsigned getResultHash(const char * name, unsigned sequence) { return ctx->getResultHash(name, sequence); }
        virtual char *getWuid() { return ctx->getWuid(); }
        virtual void getExternalResultRaw(unsigned & tlen, void * & tgt, const char * wuid, const char * stepname, unsigned sequence, IXmlToRowTransformer * xmlTransformer, ICsvToRowTransformer * csvTransformer) { ctx->getExternalResultRaw(tlen, tgt, wuid, stepname, sequence, xmlTransformer, csvTransformer); }
        virtual char *getDaliServers() { return ctx->getDaliServers(); }
        virtual void executeGraph(const char * graphName, bool realThor, size32_t parentExtractSize, const void * parentExtract) { ctx->executeGraph(graphName, realThor, parentExtractSize, parentExtract); }
        virtual __int64 countDiskFile(const char * lfn, unsigned recordSize) { return ctx->countDiskFile(lfn, recordSize); }
        virtual __int64 countIndex(__int64 activityId, IHThorCountIndexArg & arg) { return ctx->countIndex(activityId, arg); }
        virtual __int64 countDiskFile(__int64 activityId, IHThorCountFileArg & arg) { return ctx->countDiskFile(activityId, arg); }
        virtual char * getExpandLogicalName(const char * logicalName) { return ctx->getExpandLogicalName(logicalName); }
        virtual void addWuException(const char * text, unsigned code, unsigned severity) { ctx->addWuException(text, code, severity); }
        virtual void addWuAssertFailure(unsigned code, const char * text, const char * filename, unsigned lineno, unsigned column, bool isAbort) { ctx->addWuAssertFailure(code, text, filename, lineno, column, isAbort); }
        virtual IUserDescriptor *queryUserDescriptor() { return ctx->queryUserDescriptor(); }
        virtual IThorChildGraph * resolveChildQuery(__int64 activityId, IHThorArg * colocal) { return ctx->resolveChildQuery(activityId, colocal); }
        virtual unsigned __int64 getDatasetHash(const char * name, unsigned __int64 hash) { return ctx->getDatasetHash(name, hash); }
        virtual unsigned getRecoveringCount() { return ctx->getRecoveringCount(); }
        virtual unsigned getNodes() { return ctx->getNodes(); }
        virtual unsigned getNodeNum() { return ctx->getNodeNum(); }
        virtual char *getFilePart(const char *logicalPart, bool create) { return ctx->getFilePart(logicalPart, create); }
        virtual unsigned __int64 getFileOffset(const char *logicalPart) { return ctx->getFileOffset(logicalPart); }
        virtual IDistributedFileTransaction *querySuperFileTransaction() { return ctx->querySuperFileTransaction(); }
        virtual char *getJobName() { return ctx->getJobName(); }
        virtual char *getJobOwner() { return ctx->getJobOwner(); }
        virtual char *getClusterName() { return ctx->getClusterName(); }
        virtual char *getGroupName() { return ctx->getGroupName(); }
        virtual char * queryIndexMetaData(char const * lfn, char const * xpath) { return ctx->queryIndexMetaData(lfn, xpath); }
        virtual unsigned getPriority() const { return ctx->getPriority(); }
        virtual char *getPlatform() { return ctx->getPlatform(); }
        virtual char *getEnv(const char *name, const char *defaultValue) const { return ctx->getEnv(name, defaultValue); }
        virtual char *getOS() { return ctx->getOS(); }
        virtual ILocalGraph * resolveLocalQuery(__int64 activityId) { return ctx->resolveLocalQuery(activityId); }
        virtual char *getEnv(const char *name, const char *defaultValue) { return ctx->getEnv(name, defaultValue); }
        virtual unsigned logString(const char * text) const { return ctx->logString(text); }
        virtual const IContextLogger &queryContextLogger() const { return ctx->queryContextLogger(); }
        virtual IEngineRowAllocator * getRowAllocator(IOutputMetaData * meta, unsigned activityId) const { return ctx->getRowAllocator(meta, activityId); }
        virtual void getResultRowset(size32_t & tcount, byte * * & tgt, const char * name, unsigned sequence, IEngineRowAllocator * _rowAllocator, IOutputRowDeserializer * deserializer, bool isGrouped, IXmlToRowTransformer * xmlTransformer, ICsvToRowTransformer * csvTransformer) { ctx->getResultRowset(tcount, tgt, name, sequence, _rowAllocator, deserializer, isGrouped, xmlTransformer, csvTransformer); }
        virtual void getRowXML(size32_t & lenResult, char * & result, IOutputMetaData & info, const void * row, unsigned flags) { convertRowToXML(lenResult, result, info, row, flags); }
        virtual unsigned getGraphLoopCounter() const
        {
            return graph->queryLoopCounter();           // only called if value is valid
        }
        virtual IConstWUResult *getExternalResult(const char * wuid, const char *name, unsigned sequence) { return ctx->getExternalResult(wuid, name, sequence); }
        virtual IConstWUResult *getResultForGet(const char *name, unsigned sequence) { return ctx->getResultForGet(name, sequence); }
        virtual const void * fromXml(IEngineRowAllocator * _rowAllocator, size32_t len, const char * utf8, IXmlToRowTransformer * xmlTransformer, bool stripWhitespace)
        {
            return ctx->fromXml(_rowAllocator, len, utf8, xmlTransformer, stripWhitespace);
        }
    } graphCodeContext;

protected:
    CGraphBase *owner, *parent;
    Owned<IException> abortException;
    CopyCIArrayOf<CGraphElementBase> ifs;
    Owned<IPropertyTree> node;
    IBarrier *startBarrier, *waitBarrier, *doneBarrier;
    mptag_t mpTag, startBarrierTag, waitBarrierTag, doneBarrierTag;
    bool created, connected, started, aborted, graphDone, prepared;
    bool reinit, sentInitData, sentStartCtx;
    CJobBase &job;
    graph_id graphId;
    mptag_t executeReplyTag;
    size32_t parentExtractSz; // keep track of sz when passed in, as may need to serialize later
    MemoryBuffer parentExtractMb; // retain copy, used if slave transmits to master (child graph 1st time initialization of global graph)
    unsigned counter;

    class CGraphGraphActElementIterator : public CInterface, implements IThorActivityIterator
    {
    protected:
        CGraphBase &graph;
        IPropertyTree &xgmml;
        Owned<IPropertyTreeIterator> iter;
        CGraphElementBase *current;
    public:
        IMPLEMENT_IINTERFACE;

        CGraphGraphActElementIterator(CGraphBase &_graph, IPropertyTree &_xgmml) : graph(_graph), xgmml(_xgmml)
        {
            iter.setown(xgmml.getElements("node"));
        }
        virtual bool first()
        {
            if (iter->first())
            {
                IPropertyTree &node = iter->query();
                current = graph.queryElement(node.getPropInt("@id"));
                if (current)
                    return true;
                else if (next())
                    return true;
            }
            current = NULL;
            return false;
        }
        virtual bool next()
        {
            loop
            {
                if (!iter->next())
                    break;
                IPropertyTree &node = iter->query();
                current = graph.queryElement(node.getPropInt("@id"));
                if (current)
                    return true;
            }
            current = NULL;
            return false;
        }
        virtual bool isValid() { return NULL!=current; }
        virtual CGraphElementBase & query()
        {
            return *current;
        }
        CGraphElementBase & get() { CGraphElementBase &c = query(); c.Link(); return c; }
    };
    class CGraphCreatedIterator : public CGraphGraphActElementIterator
    {
    public:
        CGraphCreatedIterator(CGraphBase &graph) : CGraphGraphActElementIterator(graph, graph.queryXGMML()) { }
        virtual bool first()
        {
            if (!CGraphGraphActElementIterator::first())
                return false;
            do
            {
                if (current->isOnCreated())
                    return true;
            }
            while (CGraphGraphActElementIterator::next());
            return false;
        }
        virtual bool next()
        {
            loop
            {
                if (!CGraphGraphActElementIterator::next())
                    return false;
                if (current->isOnCreated())
                    return true;
            }
        }
    };
    class CGraphElementIterator : public CInterface, implements IThorActivityIterator
    {
        SuperHashIteratorOf<CGraphElementBase> iter;
    public:
        IMPLEMENT_IINTERFACE;

        CGraphElementIterator(CGraphElementTable &table) : iter(table) { }
        virtual bool first() { return iter.first(); }
        virtual bool next() { return iter.next(); }
        virtual bool isValid() { return iter.isValid(); }
        virtual CGraphElementBase & query() { return iter.query(); }
                CGraphElementBase & get() { CGraphElementBase &c = query(); c.Link(); return c; }
    };
    typedef ArrayIteratorOf<CGraphElementArray, CGraphElementBase &> ITAArrayIterator;
    class CGraphElementArrayIterator : public CInterface, public ITAArrayIterator, implements IThorActivityIterator
    {
    public:
        IMPLEMENT_IINTERFACE;

        CGraphElementArrayIterator(CGraphElementArray &array) : ITAArrayIterator(array) { }
        virtual bool first() { return ITAArrayIterator::first(); }
        virtual bool next() { return ITAArrayIterator::next(); }
        virtual bool isValid() { return ITAArrayIterator::isValid(); }
        virtual CGraphElementBase & query() { return ITAArrayIterator::query(); }
                CGraphElementBase & get() { CGraphElementBase &c = query(); c.Link(); return c; }
    };

public:
    IMPLEMENT_IINTERFACE;

    PooledThreadHandle poolThreadHandle;
    CopyCIArrayOf<CGraphBase> dependentSubGraphs;

    CGraphBase(CJobBase &job);
    ~CGraphBase();
    
    virtual void init() { }
    IThorActivityIterator *getTraverseIterator(bool connected=true);
    IThorActivityIterator *getTraverseIteratorCond();
    void GraphPrintLog(const char *msg, ...) __attribute__((format(printf, 2, 3)));
    void GraphPrintLog(IException *e, const char *msg, ...) __attribute__((format(printf, 3, 4)));
    void createFromXGMML(IPropertyTree *node, CGraphBase *owner, CGraphBase *parent, CGraphBase *resultsGraph);
    const bool &queryAborted() const { return aborted; }
    CJobBase &queryJob() const { return job; }
    IGraphTempHandler *queryTempHandler() const { assertex(tmpHandler.get()); return tmpHandler; }
    CGraphBase *queryOwner() { return owner; }
    CGraphBase *queryParent() { return parent?parent:this; }
    const bool isComplete() const { return complete; }
    const bool isPrepared() const { return prepared; }
    const bool isGlobal() const { return global; }
    const bool isCreated() const { return created; }
    const bool isStarted() const { return started; }
    bool isLocalOnly();
    void setCompleteEx(bool tf=true) { complete = tf; }
    const byte *setParentCtx(size32_t _parentExtractSz, const byte *parentExtract)
    {
        parentExtractSz = _parentExtractSz;
        MemoryBuffer newParentExtract(parentExtractSz, parentExtract);
        parentExtractMb.swapWith(newParentExtract);
        return (const byte *)parentExtractMb.toByteArray();
    }
    virtual ICodeContext *queryCodeContext() { return &graphCodeContext; }
    void setLoopCounter(unsigned _counter) { counter = _counter; }
    const unsigned queryLoopCounter() const { return counter; }
    virtual void setComplete(bool tf=true) { complete=tf; }
    virtual void deserializeCreateContexts(MemoryBuffer &mb);
    virtual void deserializeStartContexts(MemoryBuffer &mb);
    virtual void serializeCreateContexts(MemoryBuffer &mb, bool created);
    virtual void serializeStartContexts(MemoryBuffer &mb);
    void reset();
    void disconnectActivities()
    {
        CGraphElementIterator iter(containers);
        ForEach(iter)
        {
            CGraphElementBase &element = iter.query();
            element.releaseIOs();
        }
    }
    virtual void executeSubGraph(size32_t parentExtractSz, const byte *parentExtract);
    void join();
    virtual void execute(size32_t parentExtractSz, const byte *parentExtract, bool checkDependencies, bool async);
    IThorActivityIterator *getIterator()
    {
        return new CGraphGraphActElementIterator(*this, *xgmml);
    }
    IThorActivityIterator *getCreatedIterator()
    {
        return new CGraphCreatedIterator(*this);
    }
    IThorActivityIterator *getSinkIterator()
    {
        return new CGraphElementArrayIterator(sinks);
    }
    IPropertyTree &queryXGMML() const { return *xgmml; }
    void addActivity(CGraphElementBase *element)
    {
        if (containers.find((activity_id &)element->queryId()))
        {
            element->Release();
            return;
        }

        containers.replace(*element);
        if (element->isSink())
            sinks.append(*LINK(element));
    }
    bool removeActivity(CGraphElementBase *element)
    {
        bool res = containers.removeExact(element);
        sinks.zap(* element);
        return res;
    }
    unsigned activityCount() const
    {
        Owned<IPropertyTreeIterator> iter = xgmml->getElements("node");
        unsigned count=0;
        ForEach(*iter)
        {
            ThorActivityKind kind = (ThorActivityKind) iter->query().getPropInt("att[@name=\"_kind\"]/@value", TAKnone);
            if (TAKsubgraph != kind)
                ++count;
        }
        return count;
    }
    CGraphElementBase *queryElement(activity_id id) const
    {
        CGraphElementBase *activity = containers.find(id);
        if (activity)
            return activity;
        if (owner)
            return owner->queryElement(id);
        return NULL;
    }
    bool isSink() const { return sink; }
    void setSink(bool tf)
    {
        sink = tf;
        xgmml->setPropBool("att[@name=\"rootGraph\"]/@value", tf);
    }
    const activity_id &queryParentActivityId() const { return parentActivityId; }
    const graph_id &queryGraphId() const { return graphId; }
    void addChildGraph(CGraphBase &graph);
    unsigned queryChildGraphCount() { return childGraphs.count(); }
    CGraphBase *getChildGraph(graph_id gid)
    {
        CriticalBlock b(crit);
        return LINK(childGraphs.find(gid));
    }
    IThorGraphIterator *getChildGraphs();

    void executeChildGraphs(size32_t parentExtractSz, const byte *parentExtract);
    void doExecute(size32_t parentExtractSz, const byte *parentExtract, bool checkDependencies);
    void doExecuteChild(size32_t parentExtractSz, const byte *parentExtract);
    void executeChild(size32_t & retSize, void * & ret, size32_t parentExtractSz, const byte *parentExtract);
    void setResults(IThorGraphResults *results);
    virtual void executeChild(size32_t parentExtractSz, const byte *parentExtract, IThorGraphResults *results, IThorGraphResults *graphLoopResults);
    virtual void executeChild(size32_t parentExtractSz, const byte *parentExtract);
    virtual void serializeStats(MemoryBuffer &mb) { }
    virtual bool prepare(size32_t parentExtractSz, const byte *parentExtract, bool checkDependencies, bool shortCircuit, bool async);
    virtual void create(size32_t parentExtractSz, const byte *parentExtract);
    virtual bool preStart(size32_t parentExtractSz, const byte *parentExtract);
    virtual void start();
    virtual void postStart() { }
    virtual bool wait(unsigned timeout);
    virtual void done();
    virtual void end();
    virtual void abort(IException *e);

// IExceptionHandler
    virtual bool fireException(IException *e);

    virtual IThorResult *queryResult(unsigned id);
    virtual IThorResult *getResult(unsigned id);
    virtual IThorResult *queryGraphLoopResult(unsigned id);
    virtual IThorResult *getGraphLoopResult(unsigned id);
    virtual IThorResult *createResult(CActivityBase &activity, unsigned id, IRowInterfaces *rowIf, bool local=false);
    virtual IThorResult *createGraphLoopResult(CActivityBase &activity, IRowInterfaces *rowIf, bool local=false);

// ILocalGraph
    virtual void getResult(size32_t & len, void * & data, unsigned id);
    virtual void getLinkedResult(unsigned & count, byte * * & ret, unsigned id);

// IThorChildGraph
//  virtual void getResult(size32_t & retSize, void * & ret, unsigned id);
//  virtual void getLinkedResult(unsigned & count, byte * * & ret, unsigned id);
    virtual IEclGraphResults *evaluate(unsigned parentExtractSz, const byte * parentExtract);
friend class CGraphElementBase;
};

interface IGraphExecutor : extends IInterface
{
    virtual void add(CGraphBase *subGraph, IGraphCallback &callback, size32_t parentExtractSz, const byte *parentExtract) = 0;
    virtual IThreadPool &queryGraphPool() = 0 ;
    virtual void wait() = 0;
};

interface ILoadedDllEntry;
interface IConstWorkUnit;
class CThorCodeContextBase;
class graph_decl CJobBase : public CInterface, implements IDiskUsage, implements IExceptionHandler, implements IGraphCallback
{
protected:
    Owned<IGraphExecutor> graphExecutor;
    CriticalSection crit;
    Owned<ILoadedDllEntry> querySo;
    CThorCodeContextBase *codeCtx;
    IUserDescriptor *userDesc;
    offset_t maxDiskUsage, diskUsage;
    StringAttr key, graphName;
    bool aborted, pausing, resumed;
    StringBuffer wuid, user, scope, token;
    mutable CriticalSection wuDirty;
    mutable bool dirty;
    CGraphTable subGraphs;
    CGraphTableCopy allGraphs; // for lookup, includes all childGraphs
    mptag_t mpJobTag, slavemptag;
    Owned<IGroup> jobGroup, slaveGroup;
    Owned<ICommunicator> jobComm;
    rank_t myrank;
    ITimeReporter *timeReporter;
    Owned<IPropertyTree> xgmml;
    Owned<IGraphTempHandler> tmpHandler;
    bool timeActivities;
    class CThorPluginCtx : public SimplePluginCtx
    {
    public:
        virtual int ctxGetPropInt(const char *propName, int defaultValue) const
        {
            return globals->getPropInt(propName, defaultValue);
        }
        virtual const char *ctxQueryProp(const char *propName) const
        {
            return globals->queryProp(propName);
        }
    } pluginCtx;
    SafePluginMap *pluginMap;
    void removeAssociates(CGraphBase &graph)
    {
        CriticalBlock b(crit);
        allGraphs.removeExact(&graph);
        Owned<IThorGraphIterator> iter = graph.getChildGraphs();
        ForEach(*iter)
        {
            CGraphBase &child = iter->query();
            removeAssociates(child);
        }
    }
public:
    IMPLEMENT_IINTERFACE;

    CJobBase(const char *graphName);
    ~CJobBase();
    void clean();
    void init();
    void setXGMML(IPropertyTree *_xgmml) { xgmml.set(_xgmml); }
    IPropertyTree *queryXGMML() { return xgmml; }
    const bool &queryAborted() const { return aborted; }
    const char *queryKey() const { return key; }
    const char *queryGraphName() const { return graphName; }
    ITimeReporter &queryTimeReporter() { return *timeReporter; }
    virtual IGraphTempHandler *createTempHandler() = 0;
    virtual CGraphBase *createGraph() = 0;
    void joinGraph(CGraphBase &graph);
    void startGraph(CGraphBase &graph, IGraphCallback &callback, size32_t parentExtractSize, const byte *parentExtract);

    void addDependencies(IPropertyTree *xgmml, bool failIfMissing=true);
    void addSubGraph(CGraphBase &graph)
    {
        CriticalBlock b(crit);
        subGraphs.replace(graph);
        allGraphs.replace(graph);
    }
    void associateGraph(CGraphBase &graph)
    {
        CriticalBlock b(crit);
        allGraphs.replace(graph);
    }
    void removeSubGraph(CGraphBase &graph)
    {
        CriticalBlock b(crit);
        removeAssociates(graph);
        subGraphs.removeExact(&graph);
    }
    IThorGraphIterator *getSubGraphs();
    CGraphBase *getGraph(graph_id gid)
    {
        CriticalBlock b(crit);
        return LINK(allGraphs.find(gid));
    }

    bool queryUseCheckpoints() const;
    const bool &queryPausing() const { return pausing; }
    const bool &queryResumed() const { return resumed; }
    IGraphTempHandler *queryTempHandler() const { return tmpHandler; }
    ILoadedDllEntry &queryDllEntry() const { return *querySo; }
    ICodeContext &queryCodeContext() const;
    IUserDescriptor *queryUserDescriptor() const { return userDesc; }
    virtual IConstWorkUnit &queryWorkUnit() const { throwUnexpected(); }
    virtual void markWuDirty() { };
    virtual __int64 getWorkUnitValueInt(const char *prop, __int64 defVal) const = 0;
    virtual StringBuffer &getWorkUnitValue(const char *prop, StringBuffer &str) const = 0;
    const char *queryWuid() const { return wuid.str(); }
    const char *queryUser() const { return user.str(); }
    const char *queryScope() const { return scope.str(); }
    IDiskUsage &queryIDiskUsage() const { return *(IDiskUsage *)this; }
    void setDiskUsage(offset_t _diskUsage) { diskUsage = _diskUsage; }
    const offset_t queryMaxDiskUsage() const { return maxDiskUsage; }
    mptag_t querySlaveMpTag() const { return slavemptag; }
    mptag_t queryJobMpTag() const { return mpJobTag; }
    const unsigned querySlaves() const { return slaveGroup->ordinality(); }
    ICommunicator &queryJobComm() const { return *jobComm; }
    IGroup &queryJobGroup() const { return *jobGroup; }
    const bool &queryTimeActivities() const { return timeActivities; }
    IGroup &querySlaveGroup() const { return *slaveGroup; }
    const rank_t &queryMyRank() const { return myrank; }
    mptag_t allocateMPTag();
    void freeMPTag(mptag_t tag);
    mptag_t deserializeMPTag(MemoryBuffer &mb);

    virtual void abort(IException *e);
    virtual IBarrier *createBarrier(mptag_t tag) { UNIMPLEMENTED; return NULL; }


//
    virtual void addCreatedFile(const char *file) { assertex(false); }
    virtual __int64 addNodeDiskUsage(unsigned node, __int64 sz) { assertex(false); return 0; }

// IDiskUsage
    virtual void increase(offset_t usage, const char *key=NULL);
    virtual void decrease(offset_t usage, const char *key=NULL);

// IExceptionHandler
    virtual bool fireException(IException *e) = 0;
// IGraphCallback
    virtual void runSubgraph(CGraphBase &graph, size32_t parentExtractSz, const byte *parentExtract);
};

interface IOutputMetaData;

class graph_decl CActivityBase : public CInterface, implements IExceptionHandler, implements IRowInterfaces
{
    Owned<IThorRowAllocator> rowAllocator;
    Owned<IOutputRowSerializer> rowSerializer;
    Owned<IOutputRowDeserializer> rowDeserializer;
    CSingletonLock CABallocatorlock;
    CSingletonLock CABserializerlock;
    CSingletonLock CABdeserializerlock;

protected:
    CGraphElementBase &container;
    Linked<IHThorArg> baseHelper;
    mptag_t mpTag; // to be used by any direct inter master<->slave communication
    bool abortSoon, actStarted;
    const bool &timeActivities; // purely for access efficiency
    size32_t parentExtractSz;
    const byte *parentExtract;
    bool receiving, cancelledReceive;

public:
    IMPLEMENT_IINTERFACE;
    CActivityBase(CGraphElementBase *container);
    ~CActivityBase();
    CGraphElementBase &queryContainer() const { return container; }
    inline const mptag_t queryMpTag() const { return mpTag; }
    inline const bool &queryAbortSoon() const { return abortSoon; }
    inline IHThorArg *queryHelper() const { return baseHelper; }
    inline const bool &queryTimeActivities() const { return timeActivities; } 
    void onStart(size32_t _parentExtractSz, const byte *_parentExtract) { parentExtractSz = _parentExtractSz; parentExtract = _parentExtract; }
    bool receiveMsg(CMessageBuffer &mb, const rank_t rank, const mptag_t mpTag, rank_t *sender=NULL, unsigned timeout=MP_WAIT_FOREVER);
    void cancelReceiveMsg(const rank_t rank, const mptag_t mpTag);


    virtual void setInput(unsigned index, CActivityBase *inputActivity, unsigned inputOutIdx) { }
    virtual void releaseIOs() { }
    virtual void preStart(size32_t parentExtractSz, const byte *parentExtract) { }
    virtual void startProcess() { actStarted = true; }
    virtual bool wait(unsigned timeout) { return true; } // NB: true == success
    virtual void reset() { receiving = abortSoon = actStarted = cancelledReceive = false; }
    virtual void done() { }
    virtual void kill() { }
    virtual void abort();
    virtual MemoryBuffer &queryInitializationData(unsigned slave) const = 0;
    virtual MemoryBuffer &getInitializationData(unsigned slave, MemoryBuffer &mb) const = 0;

    void ActPrintLog(const char *format, ...) __attribute__((format(printf, 2, 3)));
    void ActPrintLog(IException *e, const char *format, ...) __attribute__((format(printf, 3, 4)));

// IExceptionHandler
    bool fireException(IException *e);

    virtual IEngineRowAllocator * queryRowAllocator();  
    virtual IOutputRowSerializer * queryRowSerializer(); 
    virtual IOutputRowDeserializer * queryRowDeserializer(); 
    virtual IOutputMetaData *queryRowMetaData() { return baseHelper->queryOutputMeta(); }
    virtual unsigned queryActivityId() { return (unsigned)container.queryId(); }
    virtual ICodeContext *queryCodeContext() { return container.queryCodeContext(); }
};

interface IFileInProgressHandler : extends IInterface
{
    virtual void add(const char *fip) = 0;
    virtual void remove(const char *fip) = 0;
};

class CFIPScope
{
    StringAttr fip;
public:
    CFIPScope() { }
    CFIPScope(const char *_fip) : fip(_fip)
    {
        queryThor().queryFileInProgressHandler().add(fip);
    }
    ~CFIPScope()
    {
        if (fip)
            queryThor().queryFileInProgressHandler().remove(fip);
    }
    void set(const char *_fip)
    {
        fip.set(_fip);
    }
    void clear()
    {
        fip.clear();
    }
};

interface IDelayedFile;
interface IExpander;
interface IThorFileCache : extends IInterface
{
    virtual bool remove(IDelayedFile &dFile) = 0;
    virtual IDelayedFile *lookup(CActivityBase &activity, IPartDescriptor &partDesc, IExpander *expander=NULL) = 0;
};

class graph_decl CThorResourceBase : public CInterface, implements IThorResource
{
public:
    IMPLEMENT_IINTERFACE;

// IThorResource
    virtual IThorFileCache &queryFileCache() { UNIMPLEMENTED; return *((IThorFileCache *)NULL); }
    virtual IBackup &queryBackup() { UNIMPLEMENTED; return *((IBackup *)NULL); }
    virtual IFileInProgressHandler &queryFileInProgressHandler() { UNIMPLEMENTED; return *((IFileInProgressHandler *)NULL); }
};

class CGraphElementBase;
typedef CGraphElementBase *(*CreateFunc)(IPropertyTree &node, CGraphBase &owner, CGraphBase *resultsGraph);
extern graph_decl void registerCreateFunc(CreateFunc func);
extern graph_decl CGraphElementBase *createGraphElement(IPropertyTree &node, CGraphBase &owner, CGraphBase *resultsGraph);
extern graph_decl IThorGraphResults *createThorGraphResults(unsigned num);
extern graph_decl IThorBoundLoopGraph *createBoundLoopGraph(CGraphBase *graph, IOutputMetaData *resultMeta, unsigned activityId);
extern graph_decl bool isDiskInput(ThorActivityKind kind);


#endif

