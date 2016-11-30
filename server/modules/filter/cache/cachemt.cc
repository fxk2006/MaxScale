/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl.
 *
 * Change Date: 2019-07-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#define MXS_MODULE_NAME "cache"
#include "cachemt.h"
#include "storage.h"
#include "storagefactory.h"

using std::tr1::shared_ptr;

CacheMT::CacheMT(const std::string&  name,
                 const CACHE_CONFIG* pConfig,
                 SCacheRules         sRules,
                 SStorageFactory     sFactory,
                 Storage*            pStorage)
    : CacheSimple(name, pConfig, sRules, sFactory, pStorage)
{
    spinlock_init(&m_lockPending);

    MXS_NOTICE("Created multi threaded cache.");
}

CacheMT::~CacheMT()
{
}

CacheMT* CacheMT::Create(const std::string& name, const CACHE_CONFIG* pConfig)
{
    ss_dassert(pConfig);

    CacheMT* pCache = NULL;

    CacheRules* pRules = NULL;
    StorageFactory* pFactory = NULL;

    if (CacheSimple::Create(*pConfig, &pRules, &pFactory))
    {
        shared_ptr<CacheRules> sRules(pRules);
        shared_ptr<StorageFactory> sFactory(pFactory);

        pCache = Create(name, pConfig, sRules, sFactory);
    }

    return pCache;
}

bool CacheMT::must_refresh(const CACHE_KEY& key, const SessionCache* pSessionCache)
{
    LockGuard guard(&m_lockPending);

    return do_must_refresh(key, pSessionCache);
}

void CacheMT::refreshed(const CACHE_KEY& key,  const SessionCache* pSessionCache)
{
    LockGuard guard(&m_lockPending);

    do_refreshed(key, pSessionCache);
}

// static
CacheMT* CacheMT::Create(const std::string&  name,
                         const CACHE_CONFIG* pConfig,
                         SCacheRules         sRules,
                         SStorageFactory     sFactory)
{
    CacheMT* pCache = NULL;

    uint32_t ttl = pConfig->ttl;
    uint32_t maxCount = pConfig->max_count;
    uint32_t maxSize = pConfig->max_size;

    int argc = pConfig->storage_argc;
    char** argv = pConfig->storage_argv;

    Storage* pStorage = sFactory->createStorage(CACHE_THREAD_MODEL_MT, name.c_str(),
                                                ttl, maxCount, maxSize,
                                                argc, argv);

    if (pStorage)
    {
        CPP_GUARD(pCache = new CacheMT(name,
                                       pConfig,
                                       sRules,
                                       sFactory,
                                       pStorage));

        if (!pCache)
        {
            delete pStorage;
        }
    }

    return pCache;
}