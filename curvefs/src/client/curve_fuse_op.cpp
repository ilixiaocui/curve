/*
 *  Copyright (c) 2021 NetEase Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */


/*
 * Project: curve
 * Created Date: Thur May 27 2021
 * Author: xuchaojie
 */
#include "curvefs/src/client/curve_fuse_op.h"
#include "curvefs/src/client/fuse_client.h"
#include "curvefs/src/client/error_code.h"
#include "curvefs/src/client/config.h"
#include "src/common/configuration.h"

using ::curvefs::client::FuseClient;
using ::curvefs::client::SimpleExtentManager;
using ::curvefs::client::InodeCacheManager;
using ::curvefs::client::DentryCacheManager;
using ::curvefs::client::DirBuffer;
using ::curvefs::client::MetaServerClientImpl;
using ::curvefs::client::MdsClientImpl;
using ::curvefs::client::BlockDeviceClientImpl;
using ::curvefs::client::CURVEFS_ERROR;
using ::curvefs::client::SpaceAllocServerClientImpl;
using ::curve::common::Configuration;
using ::curvefs::client::FuseClientOption;
using ::curvefs::client::MDSBaseClient;
using ::curvefs::client::MetaServerBaseClient;
using ::curvefs::client::SpaceBaseClient;

static FuseClient *g_ClientInstance = nullptr;

int InitFuseClient(const char* confPath) {
    Configuration conf;
    conf.SetConfigPath(confPath);

    // 打印参数
    conf.PrintConfig();

    FuseClientOption option;
    curvefs::client::InitFuseClientOption(&conf, &option);

    auto mdsClient = std::make_shared<MdsClientImpl>();
    MDSBaseClient *mdsBase = new MDSBaseClient();
    CURVEFS_ERROR ret = mdsClient->Init(option.mdsOpt, mdsBase);
    if (ret != CURVEFS_ERROR::OK) {
        return (int)ret;
    }

    auto metaClient = std::make_shared<MetaServerClientImpl>();
    MetaServerBaseClient *metaBase = new MetaServerBaseClient();
    ret = metaClient->Init(option.metaOpt, metaBase);
    if (ret != CURVEFS_ERROR::OK) {
        return (int)ret;
    }

    auto spaceClient = std::make_shared<SpaceAllocServerClientImpl>();
    SpaceBaseClient *spaceBase = new SpaceBaseClient();
    ret = spaceClient->Init(option.spaceOpt, spaceBase);
    if (ret != CURVEFS_ERROR::OK) {
        return (int)ret;
    }

    auto blockDeviceClient = std::make_shared<BlockDeviceClientImpl>();
    auto inodeManager = std::make_shared<InodeCacheManager>(metaClient);
    auto dentryManager = std::make_shared<DentryCacheManager>(metaClient);
    auto extManager = std::make_shared<SimpleExtentManager>();
    auto dirBuf = std::make_shared<DirBuffer>();
    g_ClientInstance = new FuseClient(mdsClient,
        metaClient,
        spaceClient,
        blockDeviceClient,
        inodeManager,
        dentryManager,
        extManager,
        dirBuf);

    return (int)g_ClientInstance->Init(option);
}

void UnInitFuseClient() {
    g_ClientInstance->UnInit();
    delete g_ClientInstance;
}

void curve_ll_init(void *userdata, struct fuse_conn_info *conn) {
    g_ClientInstance->init(userdata, conn);
}

void curve_ll_destroy(void *userdata) {
    g_ClientInstance->destroy(userdata);
}

void fuse_reply_err_by_errcode(
    fuse_req_t req, CURVEFS_ERROR errcode) {
    switch (errcode) {
        case CURVEFS_ERROR::OK:
            fuse_reply_err(req, 0);
            break;
        case CURVEFS_ERROR::NO_SPACE:
            fuse_reply_err(req, ENOSPC);
            break;
        default:
            fuse_reply_err(req, EIO);
            break;
    }
}

void curve_ll_lookup(fuse_req_t req, fuse_ino_t parent, const char *name) {
    fuse_entry_param e;
    CURVEFS_ERROR ret = g_ClientInstance->lookup(req, parent, name, &e);
    if (ret != CURVEFS_ERROR::OK) {
        fuse_reply_err_by_errcode(req, ret);
    }
    fuse_reply_entry(req, &e);
}

void curve_ll_getattr(fuse_req_t req, fuse_ino_t ino,
         struct fuse_file_info *fi) {
    struct stat attr;
    CURVEFS_ERROR ret = g_ClientInstance->getattr(req, ino, fi, &attr);
    if (ret != CURVEFS_ERROR::OK) {
        fuse_reply_err_by_errcode(req, ret);
    }
    fuse_reply_attr(req, &attr, 1.0);
}

void curve_ll_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
         struct fuse_file_info *fi) {
    char *buffer;
    size_t rSize = 0;
    CURVEFS_ERROR ret = g_ClientInstance->readdir(req, ino, size, off, fi,
        &buffer, &rSize);
    if (ret != CURVEFS_ERROR::OK) {
        fuse_reply_err_by_errcode(req, ret);
    }
    fuse_reply_buf(req, buffer, rSize);
}

void curve_ll_open(fuse_req_t req, fuse_ino_t ino,
          struct fuse_file_info *fi) {
    CURVEFS_ERROR ret = g_ClientInstance->open(req, ino, fi);
    if (ret != CURVEFS_ERROR::OK) {
        fuse_reply_err_by_errcode(req, ret);
    }
    fuse_reply_open(req, fi);
}

void curve_ll_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
          struct fuse_file_info *fi) {
    char *buffer;
    size_t rSize = 0;
    CURVEFS_ERROR ret = g_ClientInstance->read(req, ino, size, off, fi,
        &buffer, &rSize);
    if (ret != CURVEFS_ERROR::OK) {
        fuse_reply_err_by_errcode(req, ret);
    }
    fuse_reply_buf(req, buffer, rSize);
    free(buffer);
}

void curve_ll_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
           size_t size, off_t off, struct fuse_file_info *fi) {
    size_t wSize = 0;
    CURVEFS_ERROR ret = g_ClientInstance->write(req, ino, buf, size, off,
        fi, &wSize);
    if (ret != CURVEFS_ERROR::OK) {
        fuse_reply_err_by_errcode(req, ret);
    }
    fuse_reply_write(req, wSize);
}

void curve_ll_create(fuse_req_t req, fuse_ino_t parent, const char *name,
        mode_t mode, struct fuse_file_info *fi) {
    fuse_entry_param e;
    CURVEFS_ERROR ret = g_ClientInstance->create(req, parent, name,
        mode, fi, &e);
    if (ret != CURVEFS_ERROR::OK) {
        fuse_reply_err_by_errcode(req, ret);
    }
    fuse_reply_create(req, &e, fi);
}

void curve_ll_mknod(fuse_req_t req, fuse_ino_t parent, const char *name,
           mode_t mode, dev_t rdev) {
    fuse_entry_param e;
    CURVEFS_ERROR ret = g_ClientInstance->mknod(req, parent, name,
        mode, rdev, &e);
    if (ret != CURVEFS_ERROR::OK) {
        fuse_reply_err_by_errcode(req, ret);
    }
    fuse_reply_entry(req, &e);
}

void curve_ll_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name,
           mode_t mode) {
    fuse_entry_param e;
    CURVEFS_ERROR ret = g_ClientInstance->mkdir(req, parent, name,
        mode, &e);
    if (ret != CURVEFS_ERROR::OK) {
        fuse_reply_err_by_errcode(req, ret);
    }
    fuse_reply_entry(req, &e);
}

void curve_ll_unlink(fuse_req_t req, fuse_ino_t parent, const char *name) {
    CURVEFS_ERROR ret = g_ClientInstance->unlink(req, parent, name);
    fuse_reply_err_by_errcode(req, ret);
}

void curve_ll_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name) {
    CURVEFS_ERROR ret = g_ClientInstance->rmdir(req, parent, name);
    fuse_reply_err_by_errcode(req, ret);
}

void curve_ll_opendir(fuse_req_t req, fuse_ino_t ino,
         struct fuse_file_info *fi) {
    CURVEFS_ERROR ret = g_ClientInstance->opendir(req, ino, fi);
    if (ret != CURVEFS_ERROR::OK) {
        fuse_reply_err_by_errcode(req, ret);
    }
    fuse_reply_open(req, fi);
}

void curve_ll_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr,
         int to_set, struct fuse_file_info *fi) {
    struct stat attrOut;
    CURVEFS_ERROR ret = g_ClientInstance->setattr(req, ino, attr, to_set,
        fi, &attrOut);
    if (ret != CURVEFS_ERROR::OK) {
        fuse_reply_err_by_errcode(req, ret);
    }
    fuse_reply_attr(req, &attrOut, 1.0);
}

