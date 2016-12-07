#ifndef FS_EDIT_LOG_H_
#define FS_EDIT_LOG_H_

#include "phxpaxos/node.h"
#include "EditlogSM.h"
#include <string>
#include <vector>

using namespace phxpaxos;
using namespace std;

class FSEditlog
{
public:
    FSEditlog(const NodeInfo & oMyNode, const NodeInfoList & vecNodeList, 
        string & sPaxosLogPath, int iGroupCount);
    ~FSEditlog();

    void setCheckpointInstanceID(const uint64_t llInstanceID);
	
    int RunPaxos();

    const NodeInfo GetMaster(const string & sKey);
    const bool IsIMMaster(const string & sKey);

	int Propose(const string & sKey, const string & sPaxosValue, 
        PhxEditlogSMCtx & oEditlogSMCtx);

private:
    int MakeLogStoragePath(string & sLogStoragePath);
    int GetGroupIdx(const string & sKey);
    
private:
    NodeInfo m_oMyNode;
    NodeInfoList m_vecNodeList;
    string m_sPaxosLogPath;
    int m_iGroupCount;

    Node * m_poPaxosNode;
    PhxEditlogSM m_oEditlogSM;
};

#endif

