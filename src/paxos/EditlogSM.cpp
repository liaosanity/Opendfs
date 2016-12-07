#include "EditlogSM.h"
#include "dfs_types.h"
#include "dfs_error_log.h"
#include "nn_cycle.h"
#include "nn_file_index.h"

PhxEditlogSM::PhxEditlogSM() : m_llCheckpointInstanceID(NoCheckpoint)
{
}

PhxEditlogSM::~PhxEditlogSM()
{
}

bool PhxEditlogSM::Execute(const int iGroupIdx, const uint64_t llInstanceID, 
	const string & sPaxosValue, SMCtx * poSMCtx)
{
    dfs_log_error(dfs_cycle->error_log, DFS_LOG_DEBUG, 0, 
        "[SM Execute] ok, smid: %d, instanceid: %lu, value: %s", 
        SMID(), llInstanceID, sPaxosValue.c_str());

    if (poSMCtx != nullptr && poSMCtx->m_pCtx != nullptr)
    {
        PhxEditlogSMCtx * poPhxEditlogSMCtx = (PhxEditlogSMCtx *)poSMCtx->m_pCtx;
        poPhxEditlogSMCtx->iExecuteRet = DFS_OK;
        poPhxEditlogSMCtx->llInstanceID = llInstanceID;

		update_fi_cache_mgmt(llInstanceID, sPaxosValue, poPhxEditlogSMCtx->data);
    }
	else 
	{
        update_fi_cache_mgmt(llInstanceID, sPaxosValue, NULL);
	}

    return DFS_TRUE;
}

const int PhxEditlogSM::SMID() const 
{ 
    return 1; 
}

const uint64_t PhxEditlogSM::GetCheckpointInstanceID(const int iGroupIdx) const
{
    return m_llCheckpointInstanceID;
}

int PhxEditlogSM::SyncCheckpointInstanceID(const uint64_t llInstanceID)
{
    m_llCheckpointInstanceID = llInstanceID;

    return DFS_OK;
}

