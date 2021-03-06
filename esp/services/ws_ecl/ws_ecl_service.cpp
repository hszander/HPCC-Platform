#include <memory>

#include "daclient.hpp"
#include "environment.hpp"
#include "workunit.hpp"
#include "wujobq.hpp"
#include "fileview.hpp"
#include "ws_ecl_service.hpp"
#include "wswuinfo.hpp"
#include "xsdparser.hpp"
#include "httpclient.hpp"
#include "xpp/XmlPullParser.h"

#define SDS_LOCK_TIMEOUT (5*60*1000) // 5mins, 30s a bit short


const char *wsEclXsdTypes[] = {
    "xsd:string",
    "xsd:string",
    "xsd:boolean",
    "xsd:decimal",
    "xsd:float",
    "xsd:double",
    "xsd:duration",
    "xsd:dateTime",
    "xsd:time",
    "xsd:date",
    "xsd:gYearMonth",
    "xsd:gYear",
    "xsd:gMonthDay",
    "xsd:gDay",
    "xsd:gMonth",
    "xsd:hexBinary",
    "xsd:base64Binary",
    "xsd:anyURI",
    "xsd:QName",
    "xsd:NOTATION",
    "xsd:normalizedString",
    "xsd:token",
    "xsd:language",
    "xsd:NMTOKEN",
    "xsd:NMTOKENS",
    "xsd:Name",
    "xsd:NCName",
    "xsd:ID",
    "xsd:IDREF",
    "xsd:IDREFS",
    "xsd:ENTITY",
    "xsd:ENTITIES",
    "xsd:integer",
    "xsd:nonPositiveInteger",
    "xsd:negativeInteger",
    "xsd:long",
    "xsd:int",
    "xsd:short",
    "xsd:byte",
    "xsd:nonNegativeInteger",
    "xsd:unsignedLong",
    "xsd:unsignedInt",
    "xsd:unsignedShort",
    "xsd:unsignedByte",
    "xsd:positiveInteger",
    "tns:RawDataFile",
    "tns:CsvDataFile",
    "tns:EspStringArray",
    "tns:EspIntArray",
    "tns:XmlDataSet"
};

typedef MapStringTo<wsEclType> MapStringToWsEclType;

class wsEclTypeTranslator
{
private:
    MapStringToWsEclType typemap;
public:
    wsEclTypeTranslator();
    wsEclType translate(const char *type);
};

wsEclTypeTranslator::wsEclTypeTranslator()
{
    typemap.setValue("xsd:string",              xsdString);
    typemap.setValue("xsd:boolean",             xsdBoolean);
    typemap.setValue("xsd:decimal",             xsdDecimal);
    typemap.setValue("xsd:float",               xsdFloat);
    typemap.setValue("xsd:double",              xsdDouble);
    typemap.setValue("xsd:duration",            xsdDuration);
    typemap.setValue("xsd:dateTime",            xsdDateTime);
    typemap.setValue("xsd:time",                xsdTime);
    typemap.setValue("xsd:date",                xsdDate);
    typemap.setValue("xsd:gyearmonth",          xsdYearMonth);
    typemap.setValue("xsd:gyear",               xsdYear);
    typemap.setValue("xsd:gmonthday",           xsdMonthDay);
    typemap.setValue("xsd:gday",                xsdDay);
    typemap.setValue("xsd:gmonth",              xsdMonth);
    typemap.setValue("xsd:hexbinary",           xsdHexBinary);
    typemap.setValue("xsd:base64binary",        xsdBase64Binary);
    typemap.setValue("xsd:anyuri",              xsdAnyURI);
    typemap.setValue("xsd:qname",               xsdQName);
    typemap.setValue("xsd:notation",            xsdNOTATION);
    typemap.setValue("xsd:normalizedstring",    xsdNormalizedString);
    typemap.setValue("xsd:token",               xsdToken);
    typemap.setValue("xsd:language",            xsdLanguage);
    typemap.setValue("xsd:nmtoken",             xsdNMTOKEN);
    typemap.setValue("xsd:nmtokens",            xsdNMTOKENS);
    typemap.setValue("xsd:name",                xsdName);
    typemap.setValue("xsd:ncname",              xsdNCName);
    typemap.setValue("xsd:id",                  xsdID);
    typemap.setValue("xsd:idref",               xsdIDREF);
    typemap.setValue("xsd:idrefs",              xsdIDREFS);
    typemap.setValue("xsd:entity",              xsdENTITY);
    typemap.setValue("xsd:entities",            xsdENTITIES);
    typemap.setValue("xsd:integer",             xsdInteger);
    typemap.setValue("xsd:nonpositiveinteger",  xsdNonPositiveInteger);
    typemap.setValue("xsd:negativeinteger",     xsdNegativeInteger);
    typemap.setValue("xsd:long",                xsdLong);
    typemap.setValue("xsd:int",                 xsdInt);
    typemap.setValue("xsd:short",               xsdShort);
    typemap.setValue("xsd:byte",                xsdByte);
    typemap.setValue("xsd:nonnegativeinteger",  xsdNonNegativeInteger);
    typemap.setValue("xsd:unsignedlong",        xsdUnsignedLong);
    typemap.setValue("xsd:unsignedint",         xsdUnsignedInt);
    typemap.setValue("xsd:unsignedshort",       xsdUnsignedShort);
    typemap.setValue("xsd:unsignedbyte",        xsdUnsignedByte);
    typemap.setValue("xsd:positiveinteger",     xsdPositiveInteger);

    typemap.setValue("tns:rawdatafile",         tnsRawDataFile);
    typemap.setValue("tns:csvdatafile",         tnsCsvDataFile);
    typemap.setValue("tns:espstringarray",      tnsEspStringArray);
    typemap.setValue("tns:espintarray",         tnsEspIntArray);
    typemap.setValue("tns:xmldataset",          tnsXmlDataSet);
}

wsEclType wsEclTypeTranslator::translate(const char *type)
{
    if (!type || !*type)
        return wsEclTypeUnknown;

    StringBuffer value(type);
    wsEclType *ret = typemap.getValue(value.toLowerCase().str());

    return (ret) ? *ret : wsEclTypeUnknown;
}


const char *wsEclToXsdTypes(wsEclType from);

static wsEclTypeTranslator *translator = NULL;

const char *wsEclToXsdTypes(wsEclType from)
{
    if (from < maxWsEclType)
        return wsEclXsdTypes[from];
    return wsEclXsdTypes[wsEclTypeUnknown];
}

const char *translateXsdType(const char *from)
{
    return wsEclToXsdTypes(translator->translate(from));
}


// Interestingly, only single quote needs to HTML escape.
// ", <, >, & don't need escape.
static void escapeSingleQuote(StringBuffer& src, StringBuffer& escaped)
{
    for (const char* p = src.str(); *p!=0; p++)
    {
        if (*p == '\'')
            escaped.append("&apos;");
        else
            escaped.append(*p);
    }
}



bool CWsEclService::init(const char * name, const char * type, IPropertyTree * cfg, const char * process)
{
    Owned<IEnvironmentFactory> factory = getEnvironmentFactory();
    Owned<IConstEnvironment> environment = factory->openEnvironmentByFile();
    Owned<IPropertyTree> pRoot = &environment->getPTree();

    roxies.setown(createProperties());

    Owned<IPropertyTreeIterator> it = pRoot->getElements("Software/RoxieCluster");
    ForEach(*it)
    {
        IPropertyTree &roxie = it->query();
        const char *name = roxie.queryProp("@name");
        IPropertyTree *server = roxie.queryPropTree("RoxieServerProcess[1]");
        if (server)
        {
            const char *ip = server->queryProp("@netAddress");
            const char *port = server->queryProp("@port");
            VStringBuffer addr("%s:%s", ip, port ? port : "9876");
            roxies->setProp(name, addr.str());
        }
    }

    StringBuffer xpath;
    xpath.appendf("Software/EspProcess[@name='%s']/Authentication/@method", process);
    auth_method.set(cfg->queryProp(xpath));

    xpath.clear().appendf("Software/EspProcess[@name='%s']/@portalurl", process);
    portal_URL.set(cfg->queryProp(xpath.str()));

    translator = new wsEclTypeTranslator();
    return true;
}

CWsEclService::~CWsEclService()
{
    if (translator)
        delete translator;
}


void CWsEclBinding::getNavigationData(IEspContext &context, IPropertyTree & data)
{
    DBGLOG("CScrubbedXmlBinding::getNavigationData");

    StringArray wsModules;
    StringBuffer mode;

    data.addProp("@viewType", "wsecl_tree");
    data.addProp("@action", "NavMenuEvent");
    data.addProp("@appName", "WsECL 3.0");

    ensureNavDynFolder(data, "QuerySets", "QuerySets", "root=true", NULL);
}

IPropertyTree * getQueryRegistries()
{
    Owned<IRemoteConnection> conn = querySDS().connect("/QuerySets/", myProcessSession(), RTM_LOCK_READ|RTM_CREATE_QUERY, SDS_LOCK_TIMEOUT);
    return conn->getRoot();
}


void CWsEclBinding::getRootNavigationFolders(IEspContext &context, IPropertyTree & data)
{
    DBGLOG("CScrubbedXmlBinding::getNavigationData");

    StringArray wsModules;
    StringBuffer mode;

    data.addProp("@viewType", "wsecl_tree");
    data.addProp("@action", "NavMenuEvent");
    data.addProp("@appName", "WsECL 3.0");

    Owned<IPropertyTree> qstree = getQueryRegistries();
    Owned<IPropertyTreeIterator> querysets = qstree->getElements("QuerySet");

    ForEach(*querysets)
    {
        IPropertyTree &qs = querysets->query();
        const char *name=qs.queryProp("@id");
        VStringBuffer parms("queryset=%s", name);
        ensureNavDynFolder(data, name, name, parms.str(), NULL);
    }
}

void CWsEclBinding::getDynNavData(IEspContext &context, IProperties *params, IPropertyTree & data)
{
    if (!params)
        return;

    data.setPropBool("@volatile", true);
    if (params->getPropBool("root", false))
    {
        getRootNavigationFolders(context, data);
    }
    else if (params->hasProp("queryset"))
    {
        const char *setname = params->queryProp("queryset");

        Owned<IPropertyTree> settree = getQueryRegistry(setname, true);
        Owned<IPropertyTreeIterator> iter = settree->getElements("Query");
        ForEach(*iter)
        {
            IPropertyTree &query = iter->query();
            const char *qname = query.queryProp("@id");
            const char *wuid = query.queryProp("@wuid");
            StringBuffer navPath;
            navPath.appendf("/WsEcl/tabview/wuid/%s?qset=%s&qname=%s", wuid, setname, qname);
            ensureNavLink(data, qname, navPath.str(), qname, "menu2", navPath.str());
        }
    }
}


static inline bool isPathSeparator(char sep)
{
    return (sep=='\\')||(sep=='/');
}

static inline const char *skipPathNodes(const char *&s, int skip)
{
    if (s) {
        while (*s) {
            if (isPathSeparator(*s++))
                if (!skip--)
                    return s;
        }
    }
    return NULL;
}

static inline const char *nextPathNode(const char *&s, StringBuffer &node, int skip=0)
{
    if (skip)
        skipPathNodes(s, skip);
    if (s) while (*s) {
        if (isPathSeparator(*s))
            return s++;
        node.append(*s++);
    }
    return NULL;
}

static inline const char *firstPathNode(const char *&s, StringBuffer &node)
{
    if (s && isPathSeparator(*s))
        s++;
    return nextPathNode(s, node);
}


static void splitPathTailAndExt(const char *s, StringBuffer &path, StringBuffer &tail, StringBuffer *ext)
{
    if (s)
    {
        const char *finger=s;
        while (*finger++);
        const char *extpos=finger;
        const char *tailpos=s;
        while (s!=finger && tailpos==s)
        {
            switch (*finger)
            {
                case '.':
                    if (ext && *extpos!='.')
                        extpos=finger;
                    break;
                case '/':
                case '\\':
                    tailpos=finger;
                    break;
            }
            finger--;
        }
        if (ext && *extpos=='.')
            ext->append(extpos+1);
        if (tailpos!=s)
            path.append(tailpos - s, s);
        if (strchr("\\/", *tailpos))
            tailpos++;
        tail.append(extpos - tailpos, tailpos);
    }
}

static void splitLookupInfo(IProperties *parms, const char *&s, StringBuffer &wuid, StringBuffer &qs, StringBuffer &qid)
{
    StringBuffer lookup;
    nextPathNode(s, lookup);

    if (strieq(lookup.str(), "query"))
    {
        nextPathNode(s, qs);
        nextPathNode(s, qid);
    }
    else if (strieq(lookup.str(), "wuid"))
    {
        nextPathNode(s, wuid);
        qs.append(parms->queryProp("qset"));
        qid.append(parms->queryProp("qname"));
    }
}

void CWsEclBinding::xsltTransform(const char* xml, unsigned int len, const char* xslFileName, IProperties *params, StringBuffer& ret)
{
    Owned<IXslProcessor> proc  = getXslProcessor();
    Owned<IXslTransform> trans = proc->createXslTransform();

    trans->setXmlSource(xml, len);

    StringBuffer xslpath(getCFD());
    xslpath.append(xslFileName);
    trans->loadXslFromFile(xslpath.str());

    if (params)
    {
        Owned<IPropertyIterator> it = params->getIterator();
        for (it->first(); it->isValid(); it->next())
        {
            const char *key = it->getPropKey();
            //set parameter in the XSL transform skipping over the @ prefix, if any
            const char* paramName = *key == '@' ? key+1 : key;
            trans->setParameter(paramName, StringBuffer().append('\'').append(params->queryProp(key)).append('\'').str());
        }
    }

    trans->transform(ret);
}

StringBuffer &CWsEclBinding::generateNamespace(IEspContext &context, CHttpRequest* request, const char *serv, const char *method, StringBuffer &ns)
{
    ns.append("urn:hpccsystems:ecl:");
    if (method && *method)
        ns.appendLower(strlen(method), method);
    return ns;
}


#define REQXML_ROOT         0x0001
#define REQXML_SAMPLE_DATA  0x0002
#define REQXML_TRIM         0x0004
#define REQXML_ESCAPEFORMATTERS 0x0008

static void buildReqXml(StringStack& parent, IXmlType* type, StringBuffer& out, const char* tag, IPropertyTree *parmtree, unsigned flags, const char* ns=NULL)
{
    assertex(type!=NULL);

    if (!parmtree && (flags & REQXML_TRIM) && !(flags & REQXML_ROOT))
        return;

    const char* typeName = type->queryName();
    if (type->isComplexType())
    {
        if (typeName && std::find(parent.begin(),parent.end(),typeName) != parent.end())
            return; // recursive

        int startlen = out.length();
        out.appendf("<%s", tag);
        if (ns)
            out.append(' ').append(ns);
        int taglen=out.length()+1;
        for (size_t i=0; i<type->getAttrCount(); i++)
        {
            IXmlAttribute* attr = type->queryAttr(i);
            if (parmtree)
            {
                StringBuffer attrpath("@");
                const char *attrval = parmtree->queryProp(attrpath.append(attr->queryName()).str());
                if (attrval)
                    out.appendf(" %s='", attr->queryName()).append(attrval);
            }
            else
            {
                out.appendf(" %s='", attr->queryName());
                attr->getSampleValue(out);
            }
            out.append('\'');
        }
        out.append('>');
        if (typeName)
            parent.push_back(typeName);

        int flds = type->getFieldCount();
        switch (type->getSubType())
        {
        case SubType_Complex_SimpleContent:
            assertex(flds==0);
            if (parmtree)
            {
                const char *attrval = parmtree->queryProp(NULL);
                if (attrval)
                    out.append(attrval);
            }
            else if (flags & REQXML_SAMPLE_DATA)
                type->queryFieldType(0)->getSampleValue(out,tag);
            break;

        default:
            for (int idx=0; idx<flds; idx++)
            {
                IPropertyTree *childtree = NULL;
                const char *childname = type->queryFieldName(idx);
                if (parmtree)
                    childtree = parmtree->queryPropTree(childname);
                buildReqXml(parent,type->queryFieldType(idx), out, childname, childtree, flags & ~REQXML_ROOT);
            }
            break;
        }

        if (typeName)
            parent.pop_back();
        if ((flags & REQXML_TRIM) && !(flags & REQXML_ROOT) && out.length()==taglen)
            out.setLength(startlen);
        else
            out.appendf("</%s>",tag);
    }
    else if (type->isArray())
    {
        if (typeName && std::find(parent.begin(),parent.end(),typeName) != parent.end())
            return; // recursive

        const char* itemName = type->queryFieldName(0);
        IXmlType*   itemType = type->queryFieldType(0);
        if (!itemName || !itemType)
            throw MakeStringException(-1,"*** Invalid array definition: tag=%s, itemName=%s", tag, itemName?itemName:"NULL");

        if (typeName)
            parent.push_back(typeName);

        int startlen = out.length();
        out.appendf("<%s", tag);
        if (ns)
            out.append(' ').append(ns);
        out.append(">");
        int taglen=out.length();
        if (parmtree)
        {
            VStringBuffer countpath("%s/itemcount", itemName);
            const char *countstr=parmtree->queryProp(countpath.str());
            if (countstr && *countstr)
            {
                int count = atoi(countstr);
                for (int idx=0; idx<count; idx++)
                {
                    StringBuffer itempath;
                    itempath.append(itemName).append(idx);
                    IPropertyTree *itemtree = parmtree->queryPropTree(itempath.str());
                    if (itemtree)
                        buildReqXml(parent,itemType,out,itemName, itemtree, flags & ~REQXML_ROOT);
                }
            }
            else
            {
                Owned<IPropertyTreeIterator> items = parmtree->getElements(itemName);
                ForEach(*items)
                    buildReqXml(parent,itemType,out,itemName, &items->query(), flags & ~REQXML_ROOT);
            }
        }
        else
            buildReqXml(parent,itemType,out,itemName, NULL, flags & ~REQXML_ROOT);

        if (typeName)
            parent.pop_back();
        if ((flags & REQXML_TRIM) && !(flags & REQXML_ROOT) && out.length()==taglen)
            out.setLength(startlen);
        else
            out.appendf("</%s>",tag);
    }
    else // simple type
    {
        StringBuffer parmval;
        if (parmtree)
            parmval.append(parmtree->queryProp(NULL));
        if (!parmval.length() && (flags & REQXML_SAMPLE_DATA))
            type->getSampleValue(parmval, NULL);
        
        if (parmval.length() || !(flags&REQXML_TRIM))
        {
            if (strieq(typeName, "boolean"))
            {
                if (!strieq(parmval, "default"))
                {
                    out.appendf("<%s>", tag);
                    if (parmval.length())
                        out.append((strieq(parmval.str(),"1")||strieq(parmval.str(),"true")||strieq(parmval.str(), "on")) ? '1' : '0');
                    out.appendf("</%s>", tag);
                }
            }
            else
            {
                out.appendf("<%s>", tag);
                out.append(parmval);
                out.appendf("</%s>", tag);
            }
        }
    }
}

inline void indenter(StringBuffer &s, int count)
{
    s.appendN(count*3, ' ');
}


inline const char *jsonNewline(unsigned flags){return ((flags & REQXML_ESCAPEFORMATTERS) ? "\\n" : "\n");}

static void buildJsonMsg(StringStack& parent, IXmlType* type, StringBuffer& out, const char* tag, IPropertyTree *parmtree, unsigned flags, int &indent)
{
    assertex(type!=NULL);

    if (flags & REQXML_ROOT)
    {
        out.append("{");
        out.append(jsonNewline(flags));
        indent++;
    }

    const char* typeName = type->queryName();
    if (type->isComplexType())
    {
        if (typeName && std::find(parent.begin(),parent.end(),typeName) != parent.end())
            return; // recursive

        int startlen = out.length();
        indenter(out, indent++);
        if (tag)
            out.appendf("\"%s\": {", tag).append(jsonNewline(flags));
        else
            out.append("{").append(jsonNewline(flags));
        int taglen=out.length()+1;
        if (typeName)
            parent.push_back(typeName);
        if (type->getSubType()==SubType_Complex_SimpleContent)
        {
            if (parmtree)
            {
                const char *attrval = parmtree->queryProp(NULL);
                indenter(out, indent);
                out.appendf("\"%s\" ", (attrval) ? attrval : "");
            }
            else if (flags & REQXML_SAMPLE_DATA)
            {
                indenter(out, indent);
                out.append("\"");
                type->queryFieldType(0)->getSampleValue(out,tag);
                out.append("\" ");
            }
        }
        else
        {
            bool first=true;
            int flds = type->getFieldCount();
            for (int idx=0; idx<flds; idx++)
            {
                if (first)
                    first=false;
                else
                    out.append(",").append(jsonNewline(flags));
                IPropertyTree *childtree = NULL;
                const char *childname = type->queryFieldName(idx);
                if (parmtree)
                    childtree = parmtree->queryPropTree(childname);
                buildJsonMsg(parent, type->queryFieldType(idx), out, childname, childtree, flags & ~REQXML_ROOT, indent);
            }
            out.append(jsonNewline(flags));
        }

        if (typeName)
            parent.pop_back();
        indenter(out, indent--);
        out.append("}");
    }
    else if (type->isArray())
    {
        if (typeName && std::find(parent.begin(),parent.end(),typeName) != parent.end())
            return; // recursive

        const char* itemName = type->queryFieldName(0);
        IXmlType*   itemType = type->queryFieldType(0);
        if (!itemName || !itemType)
            throw MakeStringException(-1,"*** Invalid array definition: tag=%s, itemName=%s", tag, itemName?itemName:"NULL");

        if (typeName)
            parent.push_back(typeName);

        int startlen = out.length();
        indenter(out, indent++);
        if (tag)
            out.appendf("\"%s\": {%s", tag, jsonNewline(flags));
        else
            out.append("{").append(jsonNewline(flags));
        indenter(out, indent++);
        out.appendf("\"%s\": [", itemName).append(jsonNewline(flags));
        indent++;
        int taglen=out.length();
        if (parmtree)
        {
            VStringBuffer countpath("%s/itemcount", itemName);
            const char *countstr=parmtree->queryProp(countpath.str());
            if (countstr && *countstr)
            {
                bool first=true;
                int count = atoi(countstr);
                for (int idx=0; idx<count; idx++)
                {
                    if (first)
                        first=false;
                    else
                        out.append(",").append(jsonNewline(flags));
                    StringBuffer itempath;
                    itempath.append(itemName).append(idx);
                    IPropertyTree *itemtree = parmtree->queryPropTree(itempath.str());
                    if (itemtree)
                        buildJsonMsg(parent,itemType,out, NULL, itemtree, flags & ~REQXML_ROOT, indent);
                }
                out.append(jsonNewline(flags));
            }
            else
            {
                Owned<IPropertyTreeIterator> items = parmtree->getElements(itemName);
                bool first=true;
                ForEach(*items)
                {
                    if (first)
                        first=false;
                    else
                        out.append(",").append(jsonNewline(flags));
                    buildJsonMsg(parent,itemType,out, NULL, &items->query(), flags & ~REQXML_ROOT, indent);
                }
                out.append(jsonNewline(flags));
            }
        }
        else
            buildJsonMsg(parent, itemType, out, NULL, NULL, flags & ~REQXML_ROOT, indent);

        indenter(out, indent--);
        out.append("]").append(jsonNewline(flags));

        if (typeName)
            parent.pop_back();
        indenter(out, indent--);
        out.append("}");
    }
    else // simple type
    {
        const char *parmval = (parmtree) ? parmtree->queryProp(NULL) : NULL;
        indenter(out, indent);
        out.appendf("\"%s\": ", tag);
        if (parmval)
        {
            const char *tname = type->queryName();
            //TBD: HACK
            if (!strnicmp(tname, "int", 3) ||
                !strnicmp(tname, "real", 4) ||
                !strnicmp(tname, "dec", 3) ||
                !strnicmp(tname, "double", 6) ||
                !strnicmp(tname, "float", 5))
                out.append(parmval);
            else if (!strnicmp(tname, "bool", 4))
                out.append(((*parmval)=='1' || strieq(parmval, "true"))? "true" : "false");
            else
                out.appendf("\"%s\"", parmval);
        }
        else if (flags & REQXML_SAMPLE_DATA)
        {
            out.append('\"');
            type->getSampleValue(out,NULL);
            out.append('\"');
        }
        else
            out.append("null");
    }

    if (flags & REQXML_ROOT)
        out.append(jsonNewline(flags)).append("}");

}

static inline StringBuffer &appendNamespaceSpecificString(StringBuffer &dest, const char *src)
{
    if (src)
        while(*src){
            dest.append((const char)(isspace(*src) ? '_' : tolower(*src)));
            src++;
        }
    return dest;
}

void buildSampleDataset(StringBuffer &xml, IPropertyTree *xsdtree, const char *service, const char *method, const char *resultname)
{
    StringBuffer schemaXml;
    toXML(xsdtree, schemaXml);

    Owned<IXmlSchema> schema = createXmlSchemaFromString(schemaXml);
    if (schema.get())
    {
        IXmlType* type = schema->queryElementType("Dataset");
        if (type)
        {
            StringBuffer ns("xmlns=\"urn:hpccsystems:ecl:");
            appendNamespaceSpecificString(ns, method).append(":result:");
            appendNamespaceSpecificString(ns, resultname);
            ns.append('\"');
            StringStack parent;
            buildReqXml(parent, type, xml, "Dataset", NULL, REQXML_SAMPLE_DATA, ns.str());
        }
    }

}

void CWsEclBinding::buildSampleResponseXml(StringBuffer& msg, IEspContext &context, CHttpRequest* request, WsWuInfo &wsinfo)
{
    StringBuffer element;
    element.append(wsinfo.queryname.sget()).append("Response");

    StringBuffer xsds;
    wsinfo.getSchemas(xsds);

    msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    if (context.queryRequestParameters()->hasProp("display"))
        msg.append("<?xml-stylesheet type=\"text/xsl\" href=\"/esp/xslt/xmlformatter.xsl\"?>");

    msg.append('<').append(element.str()).append(" xmlns=\"urn:hpccsystems:ecl:");
    msg.appendLower(wsinfo.queryname.length(), wsinfo.queryname.sget()).append("\">");
    msg.append("<Results><Result>");

    Owned<IPropertyTree> xsds_tree;
    if (xsds.length())
        xsds_tree.setown(createPTreeFromXMLString(xsds.str()));

    if (xsds_tree)
    {
        Owned<IPropertyTreeIterator> result_xsds =xsds_tree->getElements("Result");
        ForEach (*result_xsds)
            buildSampleDataset(msg, result_xsds->query().queryPropTree("xs:schema"), wsinfo.qsetname.sget(), wsinfo.queryname.sget(), result_xsds->query().queryProp("@name"));
    }

    msg.append("</Result></Results>");
    msg.append("</").append(element.str()).append('>');
}


int CWsEclBinding::getWsEclLinks(IEspContext &context, CHttpRequest* request, CHttpResponse* response, WsWuInfo &wsinfo)
{
    StringBuffer xml;
    xml.append("<links>");
    xml.append("<version>3</version>");
    xml.append("<path>").append(wsinfo.qsetname.sget()).append("</path>");
    xml.append("<query>").append(wsinfo.queryname.sget()).append("</query>");

    StringBuffer xsds;
    wsinfo.getSchemas(xsds);

    Owned<IPropertyTree> xsdtree;
    if (xsds.length())
        xsdtree.setown(createPTreeFromXMLString(xsds.str()));

    if (xsdtree)
    {
        xml.append("<input_datasets>");
        Owned<IPropertyTreeIterator> input_xsds =xsdtree->getElements("Input");
        ForEach (*input_xsds)
        {
            xml.append("<dataset>");
            xml.append("<name>").append(input_xsds->query().queryProp("@name")).append("</name>");
            xml.append("</dataset>");
        }
        xml.append("</input_datasets>");
        xml.append("<result_datasets>");
        Owned<IPropertyTreeIterator> result_xsds =xsdtree->getElements("Result");
        ForEach (*result_xsds)
        {
            xml.append("<dataset>");
            xml.append("<name>").append(result_xsds->query().queryProp("@name")).append("</name>");
            xml.append("</dataset>");
        }
        xml.append("</result_datasets>");
    }

    xml.append("</links>");

    Owned<IXslProcessor> xslp = getXslProcessor();
    Owned<IXslTransform> xform = xslp->createXslTransform();
    xform->loadXslFromFile(StringBuffer(getCFD()).append("./xslt/wsecl3_links.xslt").str());
    xform->setXmlSource(xml.str(), xml.length());

    StringBuffer page;
    xform->transform(page);

    response->setContent(page);
    response->setContentType("text/html; charset=UTF-8");
    response->setStatus(HTTP_STATUS_OK);
    response->send();

    return 0;
}


int CWsEclBinding::getWsEcl2TabView(CHttpRequest* request, CHttpResponse* response, const char *thepath)
{
    IEspContext *context = request->queryContext();
    IProperties *parms = request->queryParameters();

    StringBuffer viewtype;
    nextPathNode(thepath, viewtype);

    if (!stricmp(viewtype.str(), "wuid"))
    {
        StringBuffer wuid;
        nextPathNode(thepath, wuid);
        Owned<WsWuInfo> wsinfo = new WsWuInfo(wuid.str(), parms->queryProp("qset"), parms->queryProp("qname"), context->queryUserId(), context->queryPassword());

        StringBuffer xml;
        xml.append("<tabview>");
        xml.append("<version>3</version>");
        xml.appendf("<wuid>%s</wuid>", wuid.str());
        xml.appendf("<qset>%s</qset>", wsinfo->qsetname.sget());
        xml.appendf("<qname>%s</qname>", wsinfo->queryname.sget());

        StringBuffer xsds;
        wsinfo->getSchemas(xsds);
        Owned<IPropertyTree> xsdtree;
        if (xsds.length())
            xsdtree.setown(createPTreeFromXMLString(xsds.str()));

        if (xsdtree)
        {
            xml.append("<input_datasets>");
            Owned<IPropertyTreeIterator> input_xsds =xsdtree->getElements("Input");
            ForEach (*input_xsds)
            {
                xml.append("<dataset>");
                xml.append("<name>").append(input_xsds->query().queryProp("@name")).append("</name>");
                xml.append("</dataset>");
            }
            xml.append("</input_datasets>");
            xml.append("<result_datasets>");
            Owned<IPropertyTreeIterator> result_xsds =xsdtree->getElements("Result");
            ForEach (*result_xsds)
            {
                xml.append("<dataset>");
                xml.append("<name>").append(result_xsds->query().queryProp("@name")).append("</name>");
                xml.append("</dataset>");
            }
            xml.append("</result_datasets>");
        }


        xml.append("</tabview>");

        StringBuffer html;
        xsltTransform(xml.str(), xml.length(), "./xslt/wsecl3_tabview.xsl", NULL, html);
        response->setStatus("200 OK");
        response->setContent(html.str());
        response->setContentType("text/html");
        response->send();
    }

    return 0;
}

void CWsEclBinding::appendSchemaNamespaces(IPropertyTree *namespaces, IEspContext &ctx, CHttpRequest* req, const char *service, const char *method)
{
    WsWuInfo *wsinfo = (WsWuInfo *) ctx.getBindingValue();
    if (wsinfo)
        appendSchemaNamespaces(namespaces, ctx, req, *wsinfo);
}


void CWsEclBinding::appendSchemaNamespaces(IPropertyTree *namespaces, IEspContext &ctx, CHttpRequest* req, WsWuInfo &wsinfo)
{
    IProperties *parms = ctx.queryRequestParameters();
        StringBuffer xsds;
        wsinfo.getSchemas(xsds);

        Owned<IPropertyTree> xsdtree;
        if (xsds.length())
            xsdtree.setown(createPTreeFromXMLString(xsds.str()));

        if (xsdtree)
        {
            Owned<IPropertyTreeIterator> result_xsds =xsdtree->getElements("Result");
            int count=1;
            ForEach (*result_xsds)
            {
                const char *resultname = result_xsds->query().queryProp("@name");
                StringBuffer urn("urn:hpccsystems:ecl:");
                appendNamespaceSpecificString(urn, wsinfo.queryname.get()).append(":result:");
                appendNamespaceSpecificString(urn, resultname);
                VStringBuffer nsxml("<namespace nsvar=\"ds%d\" ns=\"%s\" import=\"1\" location=\"../result/%s.xsd\"/>", count++, urn.toLowerCase().str(), resultname);
                namespaces->addPropTree("namespace", createPTreeFromXMLString(nsxml.str()));
            }
        }
}

StringBuffer &appendEclXsdName(StringBuffer &content, const char *name, bool istype=false)
{
    if (name)
    {
        if (!strnicmp(name, "xs:", 3))
            content.append("xsd:").append(name+3);
        else
        {
            if (istype && !strchr(name, ':'))
                content.append("tns:");
            content.append(name);
        }
    }
    return content;
}

void appendEclXsdStartTag(StringBuffer &content, IPropertyTree *element, int indent, const char *attrstr=NULL, bool forceclose=false)
{
    //while (indent--)
    //  content.append('\t');
    const char *name = element->queryName();
    appendEclXsdName(content.append('<'), name);
    if (strieq(name, "xs:element"))
    {
        const char *elname=element->queryProp("@name");
        if (!elname || !*elname) //ecl bug?
            element->setProp("@name", "__unknown");
        if (!element->hasProp("@minOccurs"))
            content.append(' ').append("minOccurs=\"0\"");
    }
    if (attrstr)
        content.append(' ').append(attrstr);
    Owned<IAttributeIterator> attrs = element->getAttributes();
    ForEach(*attrs)
    {
        const char *attrname=attrs->queryName()+1;
        appendEclXsdName(content.append(' '), attrname);
        appendEclXsdName(content.append("=\""), attrs->queryValue(), !stricmp(attrname, "type")).append('\"');
    }
    if (forceclose || !element->hasChildren())
        content.append('/');
    content.append(">");
}



void appendEclXsdComplexType(StringArray &names, StringBuffer &content, IPropertyTree *element, int indent=0)
{
    StringBuffer name("t_");
    ForEachItemIn(idx, names)
    {
        name.append(names.item(idx));
    }
    content.appendf("<xsd:complexType name=\"%s\">", name.str());

    Owned<IPropertyTreeIterator> children = element->getElements("*");
    ForEach(*children)
    {
        IPropertyTree &child = children->query();
        appendEclXsdStartTag(content, &child, indent);
        if (strieq(child.queryName(), "xs:sequence") || strieq(child.queryName(), "xs:all"))
        {
            Owned<IPropertyTreeIterator> els = child.getElements("xs:element");
            ForEach(*els)
            {
                IPropertyTree &el = els->query();
                StringBuffer typeattr;
                if (!el.hasProp("@type") && el.hasProp("xs:complexType") && el.hasProp("@name"))
                    typeattr.appendf("type=\"tns:%s%s\"", name.str(), el.queryProp("@name"));
                appendEclXsdStartTag(content, &el, indent, typeattr.str(), true);
            }
        }
        if (child.hasChildren())
        {
            content.appendf("</");
            appendEclXsdName(content, child.queryName());
            content.append(">");
        }
    }

    content.append("</xsd:complexType>");
}



void appendEclXsdNestedComplexTypes(StringArray &names, StringBuffer &content, IPropertyTree *element, int indent=0)
{
    if (element->hasChildren())
    {
        Owned<IPropertyTreeIterator> children = element->getElements("*");
        ForEach(*children)
        {
            if (element->hasProp("@name"))
                names.append(element->queryProp("@name"));
            appendEclXsdNestedComplexTypes(names, content, &children->query(), indent+1);
            if (element->hasProp("@name"))
                names.pop();
        }
        if (strieq(element->queryName(), "xs:complexType"))
            appendEclXsdComplexType(names, content, element, indent);
    }
}

void appendEclXsdSection(StringBuffer &content, IPropertyTree *element, int indent=0)
{
    appendEclXsdStartTag(content, element, indent);
    if (element->hasChildren())
    {
        Owned<IPropertyTreeIterator> children = element->getElements("*");
        ForEach(*children)
        {
            appendEclXsdSection(content, &children->query(), indent+1);
        }
        appendEclXsdName(content.append("</"), element->queryName()).append('>');
    }
}

void appendEclInputXsds(StringBuffer &content, IPropertyTree *xsd, BoolHash &added)
{
    Owned<IPropertyTreeIterator> it = xsd->getElements("xs:schema/*");
    const char *schema_name=xsd->queryProp("@name");
    if (schema_name && *schema_name)
    {
        ForEach (*it)
        {
            IPropertyTree &item = it->query();
            StringArray names;
            names.append(schema_name);
            appendEclXsdNestedComplexTypes(names, content, &item, 1);

            const char *aname = item.queryProp("@name");
            StringBuffer temp;
            const char *elname = item.getName(temp).str();
            if (!stricmp(aname, "dataset") || !added.getValue(aname))
            {
                StringBuffer temp;
                if (!stricmp(aname, "dataset"))
                {
                    /*
                    content.appendf("<xsd:complexType name=\"%s\"><xsd:sequence>", schema_name);
                    IPropertyTreeIterator *children = item.getElements("xs:complexType/xs:sequence/*");
                    ForEach(*children)
                    {
                        IPropertyTree &child = children->query();
                        if (child.hasProp("@name") && !stricmp(child.queryProp("@name"), "Row"))
                        {
                            child.setProp("@minOccurs", "0");
                            child.setProp("@maxOccurs", "unbounded");
                        }
                        //appendEclXsdSection(content, &child, 2);
                        //toXML(&child, content);
                    }
                    content.appendf("</xsd:sequence></xsd:complexType>", aname);
                    */
                }
                else
                {
                    added.setValue(aname, true);
                    appendEclXsdSection(content, &item, 1);
                    //toXML(&item, content);
                }
            }
        }
    }
}

void CWsEclBinding::SOAPSectionToXsd(WsWuInfo &wsinfo, const char *parmXml, StringBuffer &schema, bool isRequest, IPropertyTree *xsdtree)
{
    Owned<IPropertyTree> tree = createPTreeFromXMLString(parmXml, ipt_none, (XmlReaderOptions)(xr_ignoreWhiteSpace|xr_noRoot));

    schema.appendf("<xsd:element name=\"%s%s\">", wsinfo.queryname.sget(), isRequest ? "Request" : "Response");
    schema.append("<xsd:complexType>");
    schema.append("<xsd:all>");
    Owned<IPropertyTreeIterator> parts = tree->getElements("part");
    if (parts)
    {
        ForEach(*parts)
        {
            IPropertyTree &part = parts->query();
            const char *name=part.queryProp("@name");
            const char *ptype=part.queryProp("@type");
            StringBuffer type;
            if (!strnicmp(ptype, "xsd:", 4))
            {
                type.append(translateXsdType(part.queryProp("@type")));
            }
            else
            {
                StringBuffer xpath;
                StringBuffer xname(name);
                xpath.appendf("Input[@name='%s']",xname.toLowerCase().str());
                if (xsdtree->hasProp(xpath.str()))
                    type.append("tns:t_").append(xname).append("Dataset");
                else
                    type.append(translateXsdType(part.queryProp("@type")));
            }

            schema.appendf("<xsd:element minOccurs=\"0\" maxOccurs=\"1\" name=\"%s\" type=\"%s\"/>", name, type.str());
        }
    }
    schema.append("</xsd:all>");
    schema.append("</xsd:complexType>");
    schema.append("</xsd:element>");
}


int CWsEclBinding::getXsdDefinition(IEspContext &context, CHttpRequest *request, StringBuffer &content, const char *service, const char *method, bool mda)
{
    WsWuInfo *wsinfo = (WsWuInfo *) context.getBindingValue();
    if (wsinfo)
        getXsdDefinition(context, request, content, *wsinfo);
    return 0;
}

int CWsEclBinding::getXsdDefinition(IEspContext &context, CHttpRequest *request, StringBuffer &content, WsWuInfo &wsinfo)
{
    IProperties *httpparms=request->queryParameters();

    if (wsecl)
    {
        StringBuffer xsds;
        wsinfo.getSchemas(xsds);
        Owned<IPropertyTree> xsdtree;
        if (xsds.length())
            xsdtree.setown(createPTreeFromXMLString(xsds.str()));

        //common types
        content.append(
            "<xsd:complexType name=\"EspStringArray\">"
                "<xsd:sequence>"
                    "<xsd:element name=\"Item\" type=\"xsd:string\" minOccurs=\"0\" maxOccurs=\"unbounded\"/>"
                "</xsd:sequence>"
            "</xsd:complexType>"
            "<xsd:complexType name=\"EspIntArray\">"
                "<xsd:sequence>"
                    "<xsd:element name=\"Item\" type=\"xsd:int\" minOccurs=\"0\" maxOccurs=\"unbounded\"/>"
                "</xsd:sequence>"
            "</xsd:complexType>"
            "<xsd:simpleType name=\"XmlDataSet\">"
                "<xsd:restriction base=\"xsd:string\"/>"
            "</xsd:simpleType>"
            "<xsd:simpleType name=\"CsvDataFile\">"
                "<xsd:restriction base=\"xsd:string\"/>"
            "</xsd:simpleType>"
            "<xsd:simpleType name=\"RawDataFile\">"
                "<xsd:restriction base=\"xsd:base64Binary\"/>"
            "</xsd:simpleType>");

        if (wsinfo.queryname.length()>0)
        {
            StringBuffer parmXml;
            if (wsinfo.getWsResource("SOAP", parmXml))
            {
                if (xsdtree)
                {
                    BoolHash added;
                    Owned<IPropertyTreeIterator> input_xsds =xsdtree->getElements("Input");
                    ForEach (*input_xsds)
                    {
                        appendEclInputXsds(content, &input_xsds->query(), added);
                    }
                }
                SOAPSectionToXsd(wsinfo, parmXml.str(), content, true, xsdtree);
            }

            content.appendf("<xsd:element name=\"%sResponse\">", wsinfo.queryname.sget());
            content.append("<xsd:complexType>");
            content.append("<xsd:all>");
            content.append("<xsd:element name=\"Exceptions\" type=\"tns:ArrayOfEspException\" minOccurs=\"0\"/>");

            Owned<IPropertyTreeIterator> result_xsds =xsdtree->getElements("Result");
            if (!result_xsds->first())
            {
                content.append("<xsd:element name=\"Results\" type=\"xsd:string\" minOccurs=\"0\"/>");
            }
            else
            {
            content.append(
                "<xsd:element name=\"Results\" minOccurs=\"0\">"
                    "<xsd:complexType>"
                        "<xsd:all>"
                            "<xsd:element name=\"Result\">"
                                "<xsd:complexType>"
                                    "<xsd:all>");
                int count=1;
                ForEach (*result_xsds)
                {
                    content.appendf("<xsd:element ref=\"ds%d:Dataset\" minOccurs=\"0\"/>", count++);
                }
                            content.append(
                                "</xsd:all>"
                                "</xsd:complexType>"
                            "</xsd:element>"
                        "</xsd:all>"
                    "</xsd:complexType>"
                "</xsd:element>");
            }

            content.append("</xsd:all>");
            content.append("<xsd:attribute name=\"sequence\" type=\"xsd:int\"/>");
            content.append("</xsd:complexType>");
            content.append("</xsd:element>");
        }
    }

    return 0;
}


bool CWsEclBinding::getSchema(StringBuffer& schema, IEspContext &ctx, CHttpRequest* req, WsWuInfo &wsinfo)
{
    Owned<IPropertyTree> namespaces = createPTree();
    appendSchemaNamespaces(namespaces, ctx, req, wsinfo);
    Owned<IPropertyTreeIterator> nsiter = namespaces->getElements("namespace");
    
    StringBuffer urn("urn:hpccsystems:ecl:");
    urn.appendLower(wsinfo.queryname.length(), wsinfo.queryname.sget());
    schema.appendf("<xsd:schema elementFormDefault=\"qualified\" targetNamespace=\"%s\" ", urn.str());
    schema.appendf(" xmlns:tns=\"%s\"  xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\"", urn.str());
    ForEach(*nsiter)
    {
        IPropertyTree &ns = nsiter->query();
        schema.appendf(" xmlns:%s=\"%s\"", ns.queryProp("@nsvar"), ns.queryProp("@ns"));
    }
    schema.append(">\n");
    ForEach(*nsiter)
    {
        IPropertyTree &ns = nsiter->query();
        if (ns.hasProp("@import"))
            schema.appendf("<xsd:import namespace=\"%s\" schemaLocation=\"%s\"/>", ns.queryProp("@ns"), ns.queryProp("@location"));
    }


    schema.append(
        "<xsd:complexType name=\"EspException\">"
            "<xsd:all>"
                "<xsd:element name=\"Code\" type=\"xsd:string\"  minOccurs=\"0\"/>"
                "<xsd:element name=\"Audience\" type=\"xsd:string\" minOccurs=\"0\"/>"
                "<xsd:element name=\"Source\" type=\"xsd:string\"  minOccurs=\"0\"/>"
                "<xsd:element name=\"Message\" type=\"xsd:string\" minOccurs=\"0\"/>"
            "</xsd:all>"
        "</xsd:complexType>\n"
        "<xsd:complexType name=\"ArrayOfEspException\">"
            "<xsd:sequence>"
                "<xsd:element name=\"Source\" type=\"xsd:string\"  minOccurs=\"0\"/>"
                "<xsd:element name=\"Exception\" type=\"tns:EspException\" minOccurs=\"0\" maxOccurs=\"unbounded\"/>"
            "</xsd:sequence>"
        "</xsd:complexType>\n"
        "<xsd:element name=\"Exceptions\" type=\"tns:ArrayOfEspException\"/>\n"
    );

    getXsdDefinition(ctx, req, schema, wsinfo);

    schema.append("<xsd:element name=\"string\" nillable=\"true\" type=\"xsd:string\" />\n");
    schema.append("</xsd:schema>");

    return true;
}

int CWsEclBinding::getGenForm(IEspContext &context, CHttpRequest* request, CHttpResponse* response, WsWuInfo &wsinfo)
{
    IProperties *parms = request->queryParameters();

    StringBuffer page;
    Owned<IXslProcessor> xslp = getXslProcessor();

    Owned<IWuWebView> web = createWuWebView(*wsinfo.wu.get(), wsinfo.queryname.get(), getCFD(), true);

    StringBuffer v;
    StringBuffer formxml("<FormInfo>");
    appendXMLTag(formxml, "WUID", wsinfo.wuid.sget());
    appendXMLTag(formxml, "QuerySet", wsinfo.qsetname.sget());
    appendXMLTag(formxml, "QueryName", wsinfo.queryname.sget());
    appendXMLTag(formxml, "ClientVersion", v.appendf("%g",context.getClientVersion()).str());
    appendXMLTag(formxml, "RequestElement", v.clear().append(wsinfo.queryname).append("Request").str());
    appendXMLTag(formxml, "Help", web->aggregateResources("HELP", v.clear()).str());
    appendXMLTag(formxml, "Info", web->aggregateResources("INFO", v.clear()).str());

    context.addOptions(ESPCTX_ALL_ANNOTATION);
    getSchema(formxml, context, request, wsinfo);

    StringArray views;
    web->getResultViewNames(views);
    formxml.append("<CustomViews>");
    ForEachItemIn(i, views)
        appendXMLTag(formxml, "Result", views.item(i));
    formxml.append("</CustomViews>");
    formxml.append("</FormInfo>");

    Owned<IXslTransform> xform = xslp->createXslTransform();

    StringBuffer xslfile(getCFD());
    xform->loadXslFromFile(xslfile.append("./xslt/wsecl3_form.xsl").str());
    xform->setXmlSource(formxml.str(), formxml.length()+1);

    // pass params to form (excluding form and __querystring)
    StringBuffer params;
    if (!getUrlParams(context.queryRequestParameters(),params))
        params.appendf("%cver_=%g",(params.length()>0) ? '&' : '?', context.getClientVersion());
    xform->setStringParameter("queryParams", params.str());
    xform->setParameter("formOptionsAccess", "1");
    xform->setParameter("includeSoapTest", "1");

    // set the prop noDefaultValue param
    IProperties* props = context.queryRequestParameters();
    bool formInitialized = false;
    if (props) {
        Owned<IPropertyIterator> it = props->getIterator();
        for (it->first(); it->isValid(); it->next()) {
            const char* key = it->getPropKey();
            if (*key=='.') {
                formInitialized = true;
                break;
            }
        }
    }
    xform->setParameter("noDefaultValue", formInitialized ? "1" : "0");

    xform->transform(page);
    response->setContentType("text/html");

    response->setContent(page.str());
    response->send();

    return 0;
}

inline void appendParameterNode(StringBuffer &xpath, StringBuffer &node)
{
    if (node.length())
    {
        if (isdigit(node.charAt(0)))
            xpath.setLength(xpath.length()-1);
        xpath.append(node);
        node.clear();
    }
}

void buildParametersXml(IPropertyTree *parmtree, IProperties *parms)
{
    Owned<IPropertyIterator> it = parms->getIterator();
    ForEach(*it)
    {
        const char *key = it->getPropKey();
        const char *val = parms->queryProp(key);
        StringBuffer xpath;
        if (key && *key && val && *val)
        {
            bool isidx=false;
            StringBuffer node;
            for (int pos=0; key[pos]!=0; pos++)
            {
                if (key[pos]!='.')
                    node.append(key[pos]);
                else
                {
                    appendParameterNode(xpath, node);
                    xpath.append('/');
                }
            }
            appendParameterNode(xpath, node);

            ensurePTree(parmtree, xpath.str());
            parmtree->setProp(xpath.str(), val);
        }
    }
    StringBuffer xml;
    toXML(parmtree, xml);
    DBGLOG("parmtree: %s", xml.str());
}


void CWsEclBinding::getWsEcl2XmlRequest(StringBuffer& soapmsg, IEspContext &context, CHttpRequest* request, WsWuInfo &wsinfo, const char *xmltype, const char *ns, unsigned flags)
{
    Owned<IPropertyTree> parmtree = createPTree();
    IProperties *parms = context.queryRequestParameters();

    buildParametersXml(parmtree, parms);

    StringBuffer element;
    element.append(wsinfo.queryname.sget());
        element.append("Request");

    StringBuffer schemaXml;
    getSchema(schemaXml, context, request, wsinfo);
    DBGLOG("request schema: %s", schemaXml.str());
    Owned<IXmlSchema> schema = createXmlSchemaFromString(schemaXml);
    if (schema.get())
    {
        IXmlType* type = schema->queryElementType(element);
        if (type)
        {
            StringStack parent;
            buildReqXml(parent, type, soapmsg, (!stricmp(xmltype, "roxiexml")) ? wsinfo.queryname.sget() : element.str(), parmtree, flags|REQXML_ROOT, ns);
        }
    }
}

void CWsEclBinding::getWsEclJsonRequest(StringBuffer& jsonmsg, IEspContext &context, CHttpRequest* request, WsWuInfo &wsinfo, const char *xmltype, const char *ns, unsigned flags)
{
    Owned<IPropertyTree> parmtree = createPTree();
    IProperties *parms = context.queryRequestParameters();

    buildParametersXml(parmtree, parms);

    StringBuffer element;
    element.append(wsinfo.queryname.sget());
        element.append("Request");

    StringBuffer schemaXml;
    getSchema(schemaXml, context, request, wsinfo);
    DBGLOG("request schema: %s", schemaXml.str());
    Owned<IXmlSchema> schema = createXmlSchemaFromString(schemaXml);
    if (schema.get())
    {
        IXmlType* type = schema->queryElementType(element);
        if (type)
        {
            StringStack parent;
            int indent=0;
            buildJsonMsg(parent, type, jsonmsg, wsinfo.queryname.sget(), parmtree, flags|REQXML_ROOT|REQXML_ESCAPEFORMATTERS, indent);
        }
    }
}

void CWsEclBinding::getWsEclJsonResponse(StringBuffer& jsonmsg, IEspContext &context, CHttpRequest *request, const char *xml, WsWuInfo &wsinfo)
{
    Owned<IPropertyTree> parmtree = createPTreeFromXMLString(xml, ipt_none, (XmlReaderOptions)(xr_ignoreWhiteSpace|xr_ignoreNameSpaces));

    StringBuffer element;
    element.append(wsinfo.queryname.sget());
    element.append("Response");

    VStringBuffer xpath("Body/%s/Result/Exception", element.str());
    Owned<IPropertyTreeIterator> exceptions = parmtree->getElements(xpath.str());

    jsonmsg.appendf("{\n  \"%s\": {\n    \"Results\": {\n", element.str());

    if (exceptions && exceptions->first())
    {
        jsonmsg.append("      \"Exceptions\": {\n        \"Exception\": [\n");
        bool first=true;
        ForEach(*exceptions)
        {
            if (first)
                first=false;
            else
                jsonmsg.append(",\n");
        jsonmsg.appendf("          {\n            \"Code\": %d,\n            \"Message\": \"%s\"\n          }", exceptions->query().getPropInt("Code"), exceptions->query().queryProp("Message"));
        }
        jsonmsg.append("\n        ]\n      }\n");
    }

    xpath.clear().appendf("Body/%s/Results/Result/Dataset", element.str());
    Owned<IPropertyTreeIterator> datasets = parmtree->getElements(xpath.str());

    ForEach(*datasets)
    {
        IPropertyTree &ds = datasets->query();
        const char *dsname = ds.queryProp("@name");
        if (dsname && *dsname)
        {
            StringBuffer schemaResult;
            wsinfo.getOutputSchema(schemaResult, dsname);
            if (schemaResult.length())
            {
                Owned<IXmlSchema> schema = createXmlSchemaFromString(schemaResult);
                if (schema.get())
                {
                    IXmlType* type = schema->queryElementType("Dataset");
                    if (type)
                    {
                        StringStack parent;
                        int indent=4;
                        StringBuffer outname(dsname);
                        buildJsonMsg(parent, type, jsonmsg, outname.replace(' ', '_').str(), &ds, 0, indent);
                    }
                }
            }
        }
    }

    jsonmsg.append("    }\n  }\n}");

}


void CWsEclBinding::getSoapMessage(StringBuffer& soapmsg, IEspContext &context, CHttpRequest* request, WsWuInfo &wsinfo, unsigned flags)
{
    soapmsg.append(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope\""
          " xmlns:SOAP-ENC=\"http://schemas.xmlsoap.org/soap/encoding\">"
            " <soap:Body>"
        );

    StringBuffer ns;
    ns.append("xmlns=\"urn:hpccsystems:ecl:").appendLower(wsinfo.queryname.length(), wsinfo.queryname.sget()).append('\"');
    getWsEcl2XmlRequest(soapmsg, context, request, wsinfo, "soap", ns.str(), flags);

    soapmsg.append("</soap:Body></soap:Envelope>");
}

int CWsEclBinding::getXmlTestForm(IEspContext &context, CHttpRequest* request, CHttpResponse* response, const char *formtype, WsWuInfo &wsinfo)
{
    getXmlTestForm(context, request, response, wsinfo, formtype);
    return 0;
};

int CWsEclBinding::getXmlTestForm(IEspContext &context, CHttpRequest* request, CHttpResponse* response, WsWuInfo &wsinfo, const char *formtype)
{
    IProperties *parms = context.queryRequestParameters();

    StringBuffer soapmsg, pageName;
    getSoapMessage(soapmsg, context, request, wsinfo, 0);

    StringBuffer params;
    const char* excludes[] = {"soap_builder_",NULL};
    getEspUrlParams(context,params,excludes);

    StringBuffer header("Content-Type: text/xml; charset=UTF-8");

    Owned<IXslProcessor> xslp = getXslProcessor();
    Owned<IXslTransform> xform = xslp->createXslTransform();
    xform->loadXslFromFile(StringBuffer(getCFD()).append("./xslt/wsecl3_xmltest.xsl").str());

    StringBuffer srcxml;
    srcxml.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?><srcxml><soapbody><![CDATA[");
    srcxml.append(soapmsg.str());
    srcxml.append("]]></soapbody></srcxml>");
    xform->setXmlSource(srcxml.str(), srcxml.length());

    if (!stricmp(formtype, "roxiexml"))
    {
        xform->setStringParameter("showhttp", "true()");
        pageName.append("ROXIE XML Test");
    }
    else if (!stricmp(formtype, "roxiesoap"))
    {
        xform->setStringParameter("showhttp", "true()");
        pageName.append("ROXIE SOAP Test");
    }
    else
    {
        xform->setStringParameter("showhttp", "true()");
        pageName.append("SOAP Test");
    }

    // params
    xform->setStringParameter("pageName", pageName.str());
    xform->setStringParameter("serviceName", wsinfo.qsetname.sget());
    xform->setStringParameter("methodName", wsinfo.queryname.sget());
    xform->setStringParameter("wuid", wsinfo.wuid.sget());
    xform->setStringParameter("header", header.str());

    ISecUser* user = context.queryUser();
    bool inhouse = user && (user->getStatus()==SecUserStatus_Inhouse);
    xform->setParameter("inhouseUser", inhouse ? "true()" : "false()");

    VStringBuffer url("/WsEcl/%s/wuid/%s?qset=%s&qname=%s&%s", formtype, wsinfo.wuid.sget(), wsinfo.qsetname.sget(), wsinfo.queryname.sget(), params.str());
    xform->setStringParameter("destination", url.str());

    StringBuffer page;
    xform->transform(page);

    response->setContent(page);
    response->setContentType("text/html; charset=UTF-8");
    response->setStatus(HTTP_STATUS_OK);
    response->send();

    return 0;
}

int CWsEclBinding::getJsonTestForm(IEspContext &context, CHttpRequest* request, CHttpResponse* response, WsWuInfo &wsinfo, const char *formtype)
{
    IProperties *parms = context.queryRequestParameters();

    StringBuffer jsonmsg, pageName;
    getWsEclJsonRequest(jsonmsg, context, request, wsinfo, "json", NULL, 0);

    StringBuffer params;
    const char* excludes[] = {"soap_builder_",NULL};
    getEspUrlParams(context,params,excludes);

    StringBuffer header("Content-Type: application/json; charset=UTF-8");

    Owned<IXslProcessor> xslp = getXslProcessor();
    Owned<IXslTransform> xform = xslp->createXslTransform();
    xform->loadXslFromFile(StringBuffer(getCFD()).append("./xslt/wsecl3_jsontest.xsl").str());

    StringBuffer srcxml;
    srcxml.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?><srcxml><jsonreq><![CDATA[");
    srcxml.append(jsonmsg.str());
    srcxml.append("]]></jsonreq></srcxml>");
    xform->setXmlSource(srcxml.str(), srcxml.length());

    xform->setStringParameter("showhttp", "true()");
    pageName.append("JSON Test");

    // params
    xform->setStringParameter("pageName", pageName.str());
    xform->setStringParameter("serviceName", wsinfo.qsetname.sget());
    xform->setStringParameter("methodName", wsinfo.queryname.sget());
    xform->setStringParameter("wuid", wsinfo.wuid.sget());
    xform->setStringParameter("header", header.str());

    ISecUser* user = context.queryUser();
    bool inhouse = user && (user->getStatus()==SecUserStatus_Inhouse);
    xform->setParameter("inhouseUser", inhouse ? "true()" : "false()");

    VStringBuffer url("/WsEcl/%s/wuid/%s?qset=%s&qname=%s&%s", formtype, wsinfo.wuid.sget(), wsinfo.qsetname.sget(), wsinfo.queryname.sget(), params.str());
    xform->setStringParameter("destination", url.str());

    StringBuffer page;
    xform->transform(page);

    response->setContent(page);
    response->setContentType("text/html; charset=UTF-8");
    response->setStatus(HTTP_STATUS_OK);
    response->send();

    return 0;
}

int CWsEclBinding::onGetSoapBuilder(IEspContext &context, CHttpRequest* request, CHttpResponse* response, WsWuInfo &wsinfo)
{
    return getXmlTestForm(context, request, response, wsinfo, "soap");
}


int CWsEclBinding::getWsEcl2Form(CHttpRequest* request, CHttpResponse* response, const char *thepath)
{
    IEspContext *context = request->queryContext();

    StringBuffer formtype;
    nextPathNode(thepath, formtype);

    StringBuffer wuid;
    StringBuffer qs;
    StringBuffer qid;
    splitLookupInfo(request->queryParameters(), thepath, wuid, qs, qid);
    WsWuInfo wsinfo(wuid.str(), qs.str(), qid.str(), context->queryUserId(), context->queryPassword());

    if (strieq(formtype.str(), "ecl"))
        return getGenForm(*context, request, response, wsinfo);
    else if (strieq(formtype.str(), "soap"))
        return getXmlTestForm(*context, request, response, "soap", wsinfo);
    else if (strieq(formtype.str(), "json"))
        return getJsonTestForm(*context, request, response, wsinfo, "json");

    return 0;
}

void CWsEclBinding::addParameterToWorkunit(IWorkUnit * workunit, IConstWUResult &vardef, IResultSetMetaData &metadef, const char *varname, IPropertyTree *valtree)
{
    if (!varname || !*varname)
        return;

    Owned<IWUResult> var = workunit->updateVariableByName(varname);
    if (!vardef.isResultScalar())
    {
        StringBuffer ds;
        if (valtree->hasChildren())
            toXML(valtree, ds);
        else
        {
            const char *val = valtree->queryProp(NULL);
            if (val)
                decodeXML(val, ds);
        }
        if (ds.length())
            var->setResultRaw(ds.length(), ds.str(), ResultFormatXml);
    }
    else
    {
        const char *val = valtree->queryProp(NULL);
        if (val && *val)
        {
            switch (metadef.getColumnDisplayType(0))
            {
                case TypeBoolean:
                    var->setResultBool(strieq(val, "1") || strieq(val, "true") || strieq(val, "on"));
                    break;
                case TypeInteger:
                    var->setResultInt(_atoi64(val));
                    break;
                case TypeUnsignedInteger:
                    var->setResultInt(_atoi64(val));
                    break;
                case TypeReal:
                    var->setResultReal(atof(val));
                    break;
                case TypeSet:
                case TypeDataset:
                case TypeData:
                    var->setResultRaw(strlen(val), val, ResultFormatRaw);
                    break;
                case TypeUnicode: {
                    MemoryBuffer target;
                    convertUtf(target, UtfReader::Utf16le, strlen(val), val, UtfReader::Utf8);
                    var->setResultUnicode(target.toByteArray(), (target.length()>1) ? target.length()/2 : 0);
                    }
                    break;
                case TypeString:
                case TypeUnknown:
                default:
                    var->setResultString(val, strlen(val));
                    break;
                    break;
            }

            var->setResultStatus(ResultStatusSupplied);
        }
    }
}


int CWsEclBinding::submitWsEclWorkunit(IEspContext & context, WsWuInfo &wsinfo, const char *xml, StringBuffer &out, const char *viewname, const char *xsltname)
{
    Owned <IWorkUnitFactory> factory = getSecWorkUnitFactory(*context.querySecManager(), *context.queryUser());
    Owned <IWorkUnit> workunit = factory->createWorkUnit(NULL, "wsecl", context.queryUserId());

    IExtendedWUInterface *ext = queryExtendedWU(workunit);
    ext->copyWorkUnit(wsinfo.wu);

    workunit->clearExceptions();
    workunit->resetWorkflow();
    workunit->setClusterName(wsinfo.qsetname.sget());
    workunit->setUser(context.queryUserId());
    
    SCMStringBuffer wuid;
    workunit->getWuid(wuid);

    SCMStringBuffer token;
    createToken(wuid.str(), context.queryUserId(), context.queryPassword(), token);
    workunit->setSecurityToken(token.str());
    workunit->setState(WUStateSubmitted);
    workunit->commit();

    Owned<IPropertyTree> req = createPTreeFromXMLString(xml, ipt_none, (XmlReaderOptions)(xr_ignoreWhiteSpace|xr_ignoreNameSpaces));
    IPropertyTree *start = req.get();
    if (start->hasProp("Envelope"))
        start=start->queryPropTree("Envelope");
    if (start->hasProp("Body"))
        start=start->queryPropTree("Body/*[1]");

    Owned<IResultSetFactory> resultSetFactory(getResultSetFactory(context.queryUserId(), context.queryPassword()));
    Owned<IPropertyTreeIterator> it = start->getElements("*");
    ForEach(*it)
    {
        IPropertyTree &eclparm=it->query();
        const char *varname = eclparm.queryName();

        IConstWUResult *vardef = wsinfo.wu->getVariableByName(varname);
        if (vardef)
        {
            Owned<IResultSetMetaData> metadef = resultSetFactory->createResultSetMeta(vardef);
            if (metadef)
                addParameterToWorkunit(workunit.get(), *vardef, *metadef, varname, &eclparm);
        }
    }

    workunit->schedule();
    workunit.clear();

    runWorkUnit(wuid.str(), wsinfo.qsetname.sget());

    //don't wait indefinately, in case submitted to an inactive queue wait max + 5 mins
    int wutimeout = 300000;
    if (waitForWorkUnitToComplete(wuid.str(), wutimeout))
    {
        if (viewname)
        {
            Owned<IWuWebView> web = createWuWebView(wuid.str(), wsinfo.queryname.get(), getCFD(), true);
            web->renderResults(viewname, out);
        }
        else if (xsltname)
        {
            Owned<IWuWebView> web = createWuWebView(wuid.str(), wsinfo.queryname.get(), getCFD(), true);
            web->applyResultsXSLT(xsltname, out);
        }
        else
        {
            Owned<IConstWorkUnit> cw = factory->openWorkUnit(wuid.str(), false);
            StringBufferAdaptor result(out);
            getFullWorkUnitResultsXML(context.queryUserId(), context.queryPassword(), cw.get(), result, false, ExceptionSeverityError);
            cw.clear();
        }
    }
    else
    {
        DBGLOG("WS-ECL request timed out, WorkUnit %s", wuid.str());
    }

    DBGLOG("WS-ECL Request processed [using Doxie]");
    return true;
}

void xppToXmlString(XmlPullParser &xpp, StartTag &stag, StringBuffer &buffer)
{
    int level = 1; //assumed due to the way gotonextdataset works.
    int type = XmlPullParser::END_TAG;
    const char * content = "";
    const char *tag = NULL;
    EndTag etag;

    tag = stag.getLocalName();
    if (tag && *tag)
    {
        buffer.appendf("<%s", tag);
        for (int idx=0; idx<stag.getLength(); idx++)
            buffer.appendf(" %s=\"%s\"", stag.getRawName(idx), stag.getValue(idx));
        buffer.append(">");
    }

    do  
    {
        type = xpp.next();
        switch(type) 
        {
            case XmlPullParser::START_TAG:
            {
                xpp.readStartTag(stag);
                ++level;
                tag = stag.getLocalName();
                if (tag && *tag)
                {
                    buffer.appendf("<%s", tag);
                    for (int idx=0; idx<stag.getLength(); idx++)
                        buffer.appendf(" %s=\"%s\"", stag.getRawName(idx), stag.getValue(idx));
                    buffer.append(">");
                }
                break;
            }
            case XmlPullParser::END_TAG:
                xpp.readEndTag(etag);
                tag = etag.getLocalName();
                if (tag && *tag)
                    buffer.appendf("</%s>", tag);
                --level;
            break;
            case XmlPullParser::CONTENT:
                content = xpp.readContent();
                encodeUtf8XML(content, buffer);
                break;
            case XmlPullParser::END_DOCUMENT:
                level=0;
            break;
        }
    }
    while (level > 0);
}

bool xppGotoTag(XmlPullParser &xppx, const char *tagname, StartTag &stag)
{
    int level = 0;
    int type = XmlPullParser::END_TAG;
    do  
    {
        type = xppx.next();
        switch(type) 
        {
            case XmlPullParser::START_TAG:
            {
                xppx.readStartTag(stag);
                ++level;
                const char *tag = stag.getLocalName();
                if (tag && strieq(tag, tagname))
                    return true;
                break;
            }
            case XmlPullParser::END_TAG:
                --level;
            break;
            case XmlPullParser::END_DOCUMENT:
                level=0;
            break;
        }
    }
    while (level > 0);
    return false;
}

static const char * getResultsXmlBody(const char * xmlin, StringBuffer &results)
{
    StartTag stag;

    int pos = 0;
    auto_ptr<XmlPullParser> xpp(new XmlPullParser());

    xpp->setSupportNamespaces(true);
    xpp->setInput(xmlin, strlen(xmlin));

    xppGotoTag(*xpp, "Result", stag);

    int level = 1;
    int type = XmlPullParser::END_TAG;
    do  
    {
        type = xpp->next();
        switch(type) 
        {
            case XmlPullParser::START_TAG:
            {
                xpp->readStartTag(stag);
                ++level;
                xppToXmlString(*xpp, stag, results);
                break;
            }
            case XmlPullParser::END_TAG:
                --level;
            break;
            case XmlPullParser::END_DOCUMENT:
                level=0;
            break;
        }
    }
    while (level > 0);
    
    return results.str();
}

int CWsEclBinding::onSubmitQueryOutputXML(IEspContext &context, CHttpRequest* request, CHttpResponse* response, WsWuInfo &wsinfo)
{
    StringBuffer soapmsg;

    getSoapMessage(soapmsg, context, request, wsinfo, REQXML_TRIM|REQXML_ROOT);
    DBGLOG("submitQuery soap: %s", soapmsg.str());

    const char *thepath = request->queryPath();

    StringBuffer status;
    StringBuffer output;

    output.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    if (context.queryRequestParameters()->hasProp("display"))
        output.append("<?xml-stylesheet type=\"text/xsl\" href=\"/esp/xslt/xmlformatter.xsl\"?>");

    SCMStringBuffer clustertype;
    wsinfo.wu->getDebugValue("targetclustertype", clustertype);

    if (strieq(clustertype.str(), "roxie"))
    {
        const char *addr = wsecl->roxies->queryProp(wsinfo.qsetname.sget());
        if (!addr)
            throw MakeStringException(-1, "cluster matching query set not found: %s", wsinfo.qsetname.sget());

        StringBuffer content(request->queryContent());
        SocketEndpoint ep(addr);// = wsecl->getRoxieEndpoint();

        Owned<IHttpClientContext> httpctx = getHttpClientContext();
        StringBuffer url("http://");
        ep.getIpText(url);
        url.append(':').append(ep.port);
        Owned<IHttpClient> httpclient = httpctx->createHttpClient(NULL, url);

        StringBuffer roxieresp;
        httpclient->sendRequest("POST", "text/xml", soapmsg, roxieresp, status);
        getResultsXmlBody(roxieresp.str(), output);
    }
    else
    {
        output.appendf("<%sResponse>", wsinfo.queryname.sget());
        submitWsEclWorkunit(context, wsinfo, soapmsg.str(), output);
        output.appendf("</%sResponse>", wsinfo.queryname.sget());
    }

    response->setContent(output.str());
    response->setContentType("text/xml");
    response->setStatus("200 OK");
    response->send();

    return 0;
}

int CWsEclBinding::onSubmitQueryOutputView(IEspContext &context, CHttpRequest* request, CHttpResponse* response, WsWuInfo &wsinfo)
{
    StringBuffer soapmsg;

    getSoapMessage(soapmsg, context, request, wsinfo, REQXML_TRIM|REQXML_ROOT);
    DBGLOG("submitQuery soap: %s", soapmsg.str());

    const char *thepath = request->queryPath();

    StringBuffer output;
    StringBuffer status;
    StringBuffer html;

    SCMStringBuffer clustertype;
    wsinfo.wu->getDebugValue("targetclustertype", clustertype);

    StringBuffer xsltfile(getCFD());
    xsltfile.append("xslt/wsecl3_result.xslt");
    const char *view = context.queryRequestParameters()->queryProp("view");
    if (strieq(clustertype.str(), "roxie"))
    {
        const char *addr = wsecl->roxies->queryProp(wsinfo.qsetname.sget());
        if (!addr)
            throw MakeStringException(-1, "cluster matching query set not found: %s", wsinfo.qsetname.sget());

        StringBuffer content(request->queryContent());
        SocketEndpoint ep(addr);

        Owned<IHttpClientContext> httpctx = getHttpClientContext();
        StringBuffer url("http://");
        ep.getIpText(url);
        url.append(':').append(ep.port);
        Owned<IHttpClient> httpclient = httpctx->createHttpClient(NULL, url);

        httpclient->sendRequest("POST", "text/xml", soapmsg, output, status);
        Owned<IWuWebView> web = createWuWebView(*wsinfo.wu, NULL, getCFD(), true);
        if (!view)
            web->applyResultsXSLT(xsltfile.str(), output.str(), html);
        else
            web->renderResults(view, output.str(), html);
    }
    else
    {
        submitWsEclWorkunit(context, wsinfo, soapmsg.str(), html, view, xsltfile.str());
    }

    response->setContent(html.str());
    response->setContentType("text/html; charset=utf-8");
    response->setStatus("200 OK");
    response->send();

    return 0;
}


int CWsEclBinding::getWsdlMessages(IEspContext &context, CHttpRequest *request, StringBuffer &content, const char *service, const char *method, bool mda)
{
    WsWuInfo *wsinfo = (WsWuInfo *) context.getBindingValue();
    if (wsinfo)
    {
        content.appendf("<message name=\"%sSoapIn\">", wsinfo->queryname.sget());
        content.appendf("<part name=\"parameters\" element=\"tns:%sRequest\"/>", wsinfo->queryname.sget());
        content.append("</message>");

        content.appendf("<message name=\"%sSoapOut\">", wsinfo->queryname.sget());
        content.appendf("<part name=\"parameters\" element=\"tns:%sResponse\"/>", wsinfo->queryname.sget());
        content.append("</message>");
    }

    return 0;
}

int CWsEclBinding::getWsdlPorts(IEspContext &context, CHttpRequest *request, StringBuffer &content, const char *service, const char *method, bool mda)
{
    WsWuInfo *wsinfo = (WsWuInfo *) context.getBindingValue();
    if (wsinfo)
    {
        content.appendf("<portType name=\"%sServiceSoap\">", wsinfo->qsetname.sget());
        content.appendf("<operation name=\"%s\">", wsinfo->queryname.sget());
        content.appendf("<input message=\"tns:%sSoapIn\"/>", wsinfo->queryname.sget());
        content.appendf("<output message=\"tns:%sSoapOut\"/>", wsinfo->queryname.sget());
        content.append("</operation>");
        content.append("</portType>");
    }
    return 0;
}

int CWsEclBinding::getWsdlBindings(IEspContext &context, CHttpRequest *request, StringBuffer &content, const char *service, const char *method, bool mda)
{
    WsWuInfo *wsinfo = (WsWuInfo *) context.getBindingValue();
    if (wsinfo)
    {
        content.appendf("<binding name=\"%sServiceSoap\" type=\"tns:%sServiceSoap\">", wsinfo->qsetname.sget(), wsinfo->qsetname.sget());
        content.append("<soap:binding transport=\"http://schemas.xmlsoap.org/soap/http\" style=\"document\"/>");

        content.appendf("<operation name=\"%s\">", wsinfo->queryname.sget());
        content.appendf("<soap:operation soapAction=\"/%s/%s?ver_=1.0\" style=\"document\"/>", wsinfo->qsetname.sget(), wsinfo->queryname.sget());
        content.append("<input>");
        content.append("<soap:body use=\"literal\"/>");
        content.append("</input>");
        content.append("<output><soap:body use=\"literal\"/></output>");
        content.append("</operation>");
        content.append("</binding>");
    }

    return 0;
}


int CWsEclBinding::onGetWsdl(IEspContext &context, CHttpRequest* request, CHttpResponse* response, WsWuInfo &wsinfo)
{
    context.setBindingValue(&wsinfo);
    EspHttpBinding::onGetWsdl(context, request, response, wsinfo.qsetname.sget(), wsinfo.queryname.sget());
    context.setBindingValue(NULL);
    return 0;
}

int CWsEclBinding::onGetXsd(IEspContext &context, CHttpRequest* request, CHttpResponse* response, WsWuInfo &wsinfo)
{
    context.setBindingValue(&wsinfo);
    EspHttpBinding::onGetXsd(context, request, response, wsinfo.qsetname.sget(), wsinfo.queryname.sget());
    context.setBindingValue(NULL);

    return 0;
}


int CWsEclBinding::getWsEclDefinition(CHttpRequest* request, CHttpResponse* response, const char *thepath)
{
    IEspContext *context = request->queryContext();
    IProperties *parms = context->queryRequestParameters();

    StringBuffer wuid;
    StringBuffer qs;
    StringBuffer qid;
    splitLookupInfo(parms, thepath, wuid, qs, qid);
    WsWuInfo wsinfo(wuid.str(), qs.str(), qid.str(), context->queryUserId(), context->queryPassword());

    StringBuffer scope; //main, input, result, etc.
    nextPathNode(thepath, scope);

    StringBuffer respath;
    StringBuffer resname;
    StringBuffer restype;

    if (strieq(scope.str(), "resource"))
    {
        StringBuffer ext;
        splitPathTailAndExt(thepath, respath, resname, &ext);
    }
    else
        splitPathTailAndExt(thepath, respath, resname, &restype);

    if (strieq(scope.str(), "resource"))
    {
        StringBuffer blockStr("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
        if (request->getParameters()->hasProp("display"))
            blockStr.append("<?xml-stylesheet type=\"text/xsl\" href=\"/esp/xslt/xmlformatter.xsl\"?>");

        wsinfo.getWsResource(respath.str(), blockStr);

        if (blockStr.length())
        {
            response->setStatus("200 OK");
            response->setContent(blockStr.str());
            response->setContentType("text/xml");
            response->send();
        }
    }
    else if (strieq(restype.str(), "wsdl"))
    {
        if (strieq(scope.str(), "main"))
        {
            VStringBuffer dest("/WsEcl/soap/query/%s/%s", qs.str(), qid.str());
            parms->setProp("multiple_resp_schemas", 1);
            parms->setProp("wsdl_destination_path", dest.str());
            return onGetWsdl(*context, request, response, wsinfo);
        }
    }
    else if (strieq(restype.str(), "xsd"))
    {
        if (strieq(scope.str(), "main"))
        {
            parms->setProp("multiple_resp_schemas", 1);
            return onGetXsd(*context, request, response, wsinfo);
        }
        else if (strieq(scope.str(), "input") || strieq(scope.str(), "result"))
        {
            StringBuffer output;
            StringBuffer xsds;
            wsinfo.getSchemas(xsds);

            Owned<IPropertyTree> xsds_tree;
            if (xsds.length())
                xsds_tree.setown(createPTreeFromXMLString(xsds.str()));
            if (xsds_tree)
            {
                StringBuffer xpath;
                Owned<IPropertyTree> selected_xsd;
                StringBuffer urn("urn:hpccsystems:ecl:");
                appendNamespaceSpecificString(urn, wsinfo.queryname.get());
                urn.appendf(":%s:", scope.toLowerCase().str());
                appendNamespaceSpecificString(urn, resname.str());

                if (!stricmp(scope.str(), "input"))
                    xpath.appendf("Input[@name='%s']/xs:schema", resname.str());
                else if (!stricmp(scope.str(), "result"))
                    xpath.appendf("Result[@name='%s']/xs:schema",resname.str());
                if (xpath.length())
                    selected_xsd.setown(xsds_tree->getPropTree(xpath.str()));
                if (selected_xsd)
                {
                    selected_xsd->setProp("@targetNamespace", urn.str());
                    selected_xsd->setProp("@xmlns", urn.str());
                    IPropertyTree *dstree = selected_xsd->queryPropTree("xs:element[@name='Dataset']/xs:complexType");
                    if (dstree && !dstree->hasProp("xs:attribute[@name='name']"))
                        dstree->addPropTree("xs:attribute", createPTreeFromXMLString("<xs:attribute name=\"name\" type=\"xs:string\"/>"));
                    Owned<IPropertyTreeIterator> elements = selected_xsd->getElements("xs:element//xs:element");
                    ForEach(*elements)
                        elements->query().setPropInt("@minOccurs", 0);
                    output.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
                    if (context->queryRequestParameters()->hasProp("display"))
                        output.append("<?xml-stylesheet type=\"text/xsl\" href=\"/esp/xslt/xmlformatter.xsl\"?>");
                    toXML(selected_xsd, output);
                }
            }
            if (output.length())
            {
                response->setStatus("200 OK");
                response->setContent(output.str());
                response->setContentType("text/xml");
                response->send();
            }
        }
    }
    return 0;
}

int CWsEclBinding::getWsEclExample(CHttpRequest* request, CHttpResponse* response, const char *thepath)
{
    IProperties *parms = request->queryParameters();
    IEspContext *context = request->queryContext();

    StringBuffer exampletype;
    nextPathNode(thepath, exampletype);

    StringBuffer wuid;
    StringBuffer qs;
    StringBuffer qid;
    splitLookupInfo(parms, thepath, wuid, qs, qid);
    WsWuInfo wsinfo(wuid.str(), qs.str(), qid.str(), context->queryUserId(), context->queryPassword());

    context->setBindingValue(&wsinfo);
    if (!stricmp(exampletype.str(), "request"))
        return onGetReqSampleXml(*context, request, response, qs.str(), qid.str());
    else if (!stricmp(exampletype.str(), "response"))
    {
        StringBuffer output;
        buildSampleResponseXml(output, *context, request, wsinfo);
        if (output.length())
            {
                response->setStatus("200 OK");
                response->setContent(output.str());
                response->setContentType("text/xml");
                response->send();
            }
    }
    context->setBindingValue(NULL);
    return 0;
}

int CWsEclBinding::onRelogin(IEspContext &context, CHttpRequest* request, CHttpResponse* response)
{
    const char* build_level = getBuildLevel();
    if (!build_level || !*build_level || streq(build_level, "COMMUNITY") || 
        strieq(wsecl->auth_method.sget(), "none") || strieq(wsecl->auth_method.sget(), "local"))
    {
        StringBuffer html;

        html.append(
          "<html>"
            "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" /><title>Advanced feature in Enterprise Edition</title></head>"
            "<body>"
              "<h3 style=\"text-align:centre;\">Advanced feature in the Enterprise Edition</h4>"
              "<p style=\"text-align:centre;\">This feature is only available with the Enterprise Edition. Further information can be found at ");
        html.appendf("<a href=\"%s\" target=\"_blank\">%s</a>.", wsecl->portal_URL.sget(), wsecl->portal_URL.sget());
        html.append(
              "</p>"
            "</body>"
          "</html>");
        response->setContent(html.str());
        response->setContentType(HTTP_TYPE_TEXT_HTML_UTF8);
        response->send();
        return 0;
    }
    
    return EspHttpBinding::onRelogin(context, request, response);
}



int CWsEclBinding::onGet(CHttpRequest* request, CHttpResponse* response)
{
    Owned<IMultiException> me = MakeMultiException("WsEcl");

    try
    {
        IEspContext *context = request->queryContext();
        IProperties *parms = request->queryParameters();
        const char *thepath = request->queryPath();

        StringBuffer serviceName;
        firstPathNode(thepath, serviceName);

        if (stricmp(serviceName.str(), "WsEcl"))
            return EspHttpBinding::onGet(request, response);

        StringBuffer methodName;
        nextPathNode(thepath, methodName);

        if (!stricmp(methodName.str(), "tabview"))
        {
            return getWsEcl2TabView(request, response, thepath);
        }
        else if (!stricmp(methodName.str(), "forms"))
        {
            return getWsEcl2Form(request, response, thepath);
        }
        else if (!stricmp(methodName.str(), "submit"))
        {
            StringBuffer wuid;
            StringBuffer qs;
            StringBuffer qid;

            splitLookupInfo(parms, thepath, wuid, qs, qid);
            WsWuInfo wsinfo(wuid.str(), qs.str(), qid.str(), context->queryUserId(), context->queryPassword());

            return onSubmitQueryOutputXML(*context, request, response, wsinfo);
        }
        else if (!stricmp(methodName.str(), "xslt"))
        {
            StringBuffer wuid;
            StringBuffer qs;
            StringBuffer qid;

            splitLookupInfo(parms, thepath, wuid, qs, qid);
            WsWuInfo wsinfo(wuid.str(), qs.str(), qid.str(), context->queryUserId(), context->queryPassword());

            return onSubmitQueryOutputView(*context, request, response, wsinfo);
        }
        else if (!stricmp(methodName.str(), "example"))
        {
            return getWsEclExample(request, response, thepath);
        }
        else if (!stricmp(methodName.str(), "definitions"))
        {
            return getWsEclDefinition(request, response, thepath);
        }
        else if (!stricmp(methodName.str(), "links"))
        {
            StringBuffer wuid;
            StringBuffer qs;
            StringBuffer qid;

            splitLookupInfo(parms, thepath, wuid, qs, qid);
            WsWuInfo wsinfo(wuid.str(), qs.str(), qid.str(), context->queryUserId(), context->queryPassword());
            return getWsEclLinks(*context, request, response, wsinfo);
        }
    }
    catch (IMultiException* mex)
    {
        me->append(*mex);
        mex->Release();
    }
    catch (IException* e)
    {
        me->append(*e);
    }
    catch (...)
    {
        me->append(*MakeStringExceptionDirect(-1, "Unknown Exception"));
    }
    
    response->handleExceptions(getXslProcessor(), me, "WsEcl", "", StringBuffer(getCFD()).append("./smc_xslt/exceptions.xslt").str());
    return 0;
}

void checkForXmlResponseName(StartTag &starttag, StringBuffer &respname, int &soaplevel)
{
    if (respname.length())
        return;

    switch(soaplevel)
    {
        case 0:
        {
            if (!stricmp(starttag.getLocalName(), "Envelope"))
                soaplevel=1;
            else
                respname.append(starttag.getLocalName());
            break;
        }
        case 1:
            if (!stricmp(starttag.getLocalName(), "Body"))
                soaplevel=2;
            break;
        case 2:
            respname.append(starttag.getLocalName());
        default:
            break;
    }

    int len=respname.length();
    if (len>8 && !stricmp(respname.str()+len-8, "Response"))
        respname.setLength(len-8);
}


//using namespace xpp;
void cleanupWsEclSoapResponse(StringBuffer &roxiesoap, StringBuffer &corrected)
{
    XmlPullParser xpp;
    xpp.setSupportNamespaces(true);
    xpp.setInput(roxiesoap.str(), roxiesoap.length());

    StartTag starttag;
    EndTag endtag;

    BoolHash addedNs;
    int nscount=0;
    bool decl_def_ns=false;
    int soaplevel=0;
    StringBuffer respname;

    int xpptype = 0;
    while (xpptype!=XmlPullParser::START_TAG && xpptype!=XmlPullParser::END_DOCUMENT)
        xpptype = xpp.next();

    int level=1;
    while (level>0 && xpptype!=XmlPullParser::END_DOCUMENT)
    {
        switch (xpptype)
        {
            case XmlPullParser::START_TAG:
            {
                level++;
                StringBuffer dsname;
                xpp.readStartTag(starttag);

                checkForXmlResponseName(starttag, respname, soaplevel);

                corrected.appendf("<%s", starttag.getQName());
                if (!decl_def_ns && !stricmp(starttag.getQName(), starttag.getLocalName()))
                {
                    const char *uri = starttag.getUri();
                    if (uri && *uri)
                        corrected.appendf(" xmlns=\"%s\"", uri);
                    decl_def_ns=true;
                }

                if (xpp.getNsCount()>nscount)
                {
                    map< string, const SXT_CHAR* >::iterator nsit;
                    for ( nsit=xpp.getNsBegin(); nsit != xpp.getNsEnd(); nsit++ )
                    {
                        if (!addedNs.getValue((*nsit).first.c_str()))
                        {
                            addedNs.setValue((*nsit).first.c_str(), true);
                            if (stricmp((*nsit).first.c_str(), "xml")) //xml should not be defined
                                corrected.appendf(" xmlns:%s=\"%s\"", (*nsit).first.c_str(), (*nsit).second);
                        }
                    }
                    nscount=xpp.getNsCount();
                }

                if (!stricmp(starttag.getLocalName(), "Dataset"))
                {
                    StringBuffer dsname(starttag.getValue("name"));
                    dsname.replace(' ', '_');
                    if (respname.length() && dsname)
                    {
                        corrected.append(" xmlns=\"urn:hpccsystems:ecl:");
                        appendNamespaceSpecificString(corrected, respname.str());
                        corrected.append(":result:");
                        appendNamespaceSpecificString(corrected, dsname.str());
                        corrected.append('\"');
                    }
                }

                for (int attnbr=0; attnbr<starttag.getLength(); attnbr++)
                {
                    corrected.append(' ').append(starttag.getRawName(attnbr)).append("=\"");
                    encodeUtf8XML(starttag.getValue(attnbr), corrected);
                    corrected.append('\"');
                }
                corrected.append('>');
                break;
            }
            case XmlPullParser::END_TAG:
            {
                level--;
                xpp.readEndTag(endtag);
                corrected.appendf("</%s>", endtag.getQName());
                break;
            }
            case XmlPullParser::CONTENT:
            {
                encodeUtf8XML(xpp.readContent(), corrected);
                break;
            }
            case XmlPullParser::END_DOCUMENT:
            default:
                break;
        }

        xpptype = xpp.next();
    }
}

void createPTreeFromJsonString(const char *json, bool caseInsensitive, StringBuffer &xml, const char *tail);


void CWsEclBinding::handleHttpPost(CHttpRequest *request, CHttpResponse *response)
{
    StringBuffer ct;
    request->getContentType(ct);
    if (!strnicmp(ct.str(), "application/json", 16))
    {
        IEspContext *ctx = request->queryContext();
        IProperties *parms = request->queryParameters();

        const char *thepath = request->queryPath();

        StringBuffer serviceName;
        firstPathNode(thepath, serviceName);

        if (!strieq(serviceName.str(), "WsEcl"))
            EspHttpBinding::handleHttpPost(request, response);

        StringBuffer action;
        nextPathNode(thepath, action);

        StringBuffer lookup;
        nextPathNode(thepath, lookup);

        StringBuffer wuid;
        StringBuffer queryset;
        StringBuffer queryname;

        if (strieq(lookup.str(), "wuid"))
        {
            nextPathNode(thepath, wuid);
            queryset.append(parms->queryProp("qset"));
            queryname.append(parms->queryProp("qname"));
        }
        else if (strieq(lookup.str(), "query"))
        {
            nextPathNode(thepath, queryset);
            nextPathNode(thepath, queryname);
        }

        WsWuInfo wsinfo(wuid.str(), queryset.str(), queryname.str(), ctx->queryUserId(), ctx->queryPassword());

        StringBuffer content(request->queryContent());
        StringBuffer jsonresp;
        StringBuffer status;
        StringBuffer soapfromjson;
        soapfromjson.append(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope\""
              " xmlns:SOAP-ENC=\"http://schemas.xmlsoap.org/soap/encoding\">"
                " <soap:Body>"
            );
        createPTreeFromJsonString(content.str(), false, soapfromjson, "Request");
        soapfromjson.append("</soap:Body></soap:Envelope>");
        DBGLOG("soap from json req: %s", soapfromjson.str());

        StringBuffer soapresp;

    SCMStringBuffer clustertype;
    wsinfo.wu->getDebugValue("targetclustertype", clustertype);
    if (strieq(clustertype.str(), "roxie"))
    {
        const char *addr = wsecl->roxies->queryProp(wsinfo.qsetname.sget());
        if (!addr)
            throw MakeStringException(-1, "cluster matching query set not found: %s", wsinfo.qsetname.sget());

        StringBuffer xmlfromjson(request->queryContent());
        SocketEndpoint ep(addr);// = wsecl->getRoxieEndpoint();

        Owned<IHttpClientContext> httpctx = getHttpClientContext();
        StringBuffer url("http://");
        ep.getIpText(url);
        url.append(':').append(ep.port);
        Owned<IHttpClient> httpclient = httpctx->createHttpClient(NULL, url);

        StringBuffer output;
        httpclient->sendRequest("POST", "text/xml", soapfromjson, output, status);
        soapresp.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
        cleanupWsEclSoapResponse(output, soapresp);
    }
    else
    {
        soapresp.append(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope\""
              " xmlns:SOAP-ENC=\"http://schemas.xmlsoap.org/soap/encoding\">"
                " <soap:Body>"
            );

        StringBuffer ns;
        ns.append("xmlns=\"urn:hpccsystems:ecl:").appendLower(wsinfo.queryname.length(), wsinfo.queryname.sget()).append('\"');
        soapresp.appendf("<%sResponse %s><Results>", wsinfo.queryname.sget(), ns.str());
        submitWsEclWorkunit(*ctx, wsinfo, soapfromjson.str(), soapresp);
        soapresp.appendf("</Results></%sResponse>", wsinfo.queryname.sget());
        soapresp.append("</soap:Body></soap:Envelope>");
    }

    DBGLOG("HandleSoapRequest response: %s", soapresp.str());

        getWsEclJsonResponse(jsonresp, *ctx, request, soapresp.str(), wsinfo);

        response->setContent(jsonresp.str());
        response->setContentType("application/json");
        response->setStatus("200 OK");
        response->send();
    }
    else
    {
        EspHttpBinding::handleHttpPost(request, response);
    }
}

int CWsEclBinding::HandleSoapRequest(CHttpRequest* request, CHttpResponse* response)
{
    IEspContext *ctx = request->queryContext();
    IProperties *parms = request->queryParameters();

    const char *thepath = request->queryPath();

    StringBuffer serviceName;
    firstPathNode(thepath, serviceName);

    if (!strieq(serviceName.str(), "WsEcl"))
        return CHttpSoapBinding::HandleSoapRequest(request, response);

    StringBuffer action;
    nextPathNode(thepath, action);

    StringBuffer lookup;
    nextPathNode(thepath, lookup);

    StringBuffer wuid;
    StringBuffer queryset;
    StringBuffer queryname;

    if (strieq(lookup.str(), "wuid"))
    {
        nextPathNode(thepath, wuid);
        queryset.append(parms->queryProp("qset"));
        queryname.append(parms->queryProp("qname"));
    }
    else if (strieq(lookup.str(), "query"))
    {
        nextPathNode(thepath, queryset);
        nextPathNode(thepath, queryname);
    }

    WsWuInfo wsinfo(wuid.str(), queryset.str(), queryname.str(), ctx->queryUserId(), ctx->queryPassword());

    StringBuffer content(request->queryContent());
    StringBuffer soapresp;
    StringBuffer status;

    SCMStringBuffer clustertype;
    wsinfo.wu->getDebugValue("targetclustertype", clustertype);
    if (strieq(clustertype.str(), "roxie"))
    {
        const char *addr = wsecl->roxies->queryProp(wsinfo.qsetname.sget());
        if (!addr)
            throw MakeStringException(-1, "cluster matching query set not found: %s", wsinfo.qsetname.sget());

        StringBuffer content(request->queryContent());
        SocketEndpoint ep(addr);// = wsecl->getRoxieEndpoint();

        Owned<IHttpClientContext> httpctx = getHttpClientContext();
        StringBuffer url("http://");
        ep.getIpText(url);
        url.append(':').append(ep.port);
        Owned<IHttpClient> httpclient = httpctx->createHttpClient(NULL, url);

        StringBuffer output;
        httpclient->sendRequest("POST", "text/xml", content, output, status);
        soapresp.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
        cleanupWsEclSoapResponse(output, soapresp);
    }
    else
    {
        soapresp.append(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope\""
              " xmlns:SOAP-ENC=\"http://schemas.xmlsoap.org/soap/encoding\">"
                " <soap:Body>"
            );

        StringBuffer ns;
        ns.append("xmlns=\"urn:hpccsystems:ecl:").appendLower(wsinfo.queryname.length(), wsinfo.queryname.sget()).append('\"');
        soapresp.appendf("<%sResponse %s><Results>", wsinfo.queryname.sget(), ns.str());
        submitWsEclWorkunit(*ctx, wsinfo, content.str(), soapresp);
        soapresp.appendf("</Results></%sResponse>", wsinfo.queryname.sget());
        soapresp.append("</soap:Body></soap:Envelope>");
    }

    DBGLOG("HandleSoapRequest response: %s", soapresp.str());

    response->setContent(soapresp.str());
    response->setContentType("text/xml");
    response->setStatus("200 OK");
    response->send();

    return 0;
}

