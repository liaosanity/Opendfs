#ifndef EDIT_LOG_SM_H_
#define EDIT_LOG_SM_H_

#include "phxpaxos/sm.h"
#include "phxpaxos/options.h"
#include <stdio.h>
#include <unistd.h>

using namespace phxpaxos;
using namespace std;

class PhxEditlogSMCtx
{
public:
    int iExecuteRet;
	uint64_t llInstanceID;
	void *data;

    PhxEditlogSMCtx()
    {
        iExecuteRet = -1;
		llInstanceID = 0;
		data = NULL;
    }
};

class PhxEditlogSM : public StateMachine
{
public:
    PhxEditlogSM();
    ~PhxEditlogSM();

    bool Execute(const int iGroupIdx, const uint64_t llInstanceID, 
            const string & sPaxosValue, SMCtx * poSMCtx);

    const int SMID() const;

    const uint64_t GetCheckpointInstanceID(const int iGroupIdx) const;
    int SyncCheckpointInstanceID(const uint64_t llInstanceID);

private:
    uint64_t m_llCheckpointInstanceID;
};

#endif

