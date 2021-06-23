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

#ifndef CURVEFS_SRC_CLIENT_FUSE_CLIENT_H_
#define CURVEFS_SRC_CLIENT_FUSE_CLIENT_H_

#include <memory>

#include "curvefs/src/client/fuse_common.h"
#include "curvefs/src/client/inode_cache_manager.h"
#include "curvefs/src/client/dentry_cache_manager.h"
#include "curvefs/src/client/metaserver_client.h"
#include "curvefs/src/client/block_device_client.h"
#include "curvefs/src/client/mds_client.h"
#include "curvefs/src/client/dir_buffer.h"
#include "curvefs/src/client/extent_manager.h"
#include "curvefs/src/client/space_client.h"
#include "curvefs/src/client/config.h"

namespace curvefs {
namespace client {

class FuseClient {
 public:
    FuseClient(std::shared_ptr<MdsClient> mdsClient,
        std::shared_ptr<MetaServerClient> metaClient,
        std::shared_ptr<SpaceClient> spaceClient,
        std::shared_ptr<BlockDeviceClient> blockDeviceClient,
        std::shared_ptr<InodeCacheManager> inodeManager,
        std::shared_ptr<DentryCacheManager> dentryManager,
        std::shared_ptr<ExtentManager> extManager,
        std::shared_ptr<DirBuffer> dirBuf)
          : mdsClient_(mdsClient),
            metaClient_(metaClient),
            spaceClient_(spaceClient),
            blockDeviceClient_(blockDeviceClient),
            inodeManager_(inodeManager),
            dentryManager_(dentryManager),
            extManager_(extManager),
            dirBuf_(dirBuf),
            fsInfo_(nullptr) {}

    void init(void *userdata, struct fuse_conn_info *conn);

    void destroy(void *userdata);

    CURVEFS_ERROR lookup(fuse_req_t req, fuse_ino_t parent, 
        const char *name, fuse_entry_param *e);

    CURVEFS_ERROR write(fuse_req_t req, fuse_ino_t ino,
        const char *buf, size_t size, off_t off,
        struct fuse_file_info *fi, size_t *wSize);

    CURVEFS_ERROR read(fuse_req_t req,
            fuse_ino_t ino, size_t size, off_t off,
            struct fuse_file_info *fi,
            char **buffer,
            size_t *rSize);

    CURVEFS_ERROR open(fuse_req_t req, fuse_ino_t ino,
              struct fuse_file_info *fi);

    CURVEFS_ERROR create(fuse_req_t req, fuse_ino_t parent,
        const char *name, mode_t mode, struct fuse_file_info *fi,
        fuse_entry_param *e);

    CURVEFS_ERROR mknod(fuse_req_t req, fuse_ino_t parent,
            const char *name, mode_t mode, dev_t rdev,
            fuse_entry_param *e);

    CURVEFS_ERROR mkdir(fuse_req_t req, fuse_ino_t parent,
            const char *name, mode_t mode,
            fuse_entry_param *e);

    CURVEFS_ERROR unlink(fuse_req_t req, fuse_ino_t parent, const char *name);

    CURVEFS_ERROR rmdir(fuse_req_t req, fuse_ino_t parent, const char *name);

    CURVEFS_ERROR opendir(fuse_req_t req, fuse_ino_t ino,
             struct fuse_file_info *fi);

    CURVEFS_ERROR readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
            struct fuse_file_info *fi,
            char **buffer,
            size_t *rSize);

    CURVEFS_ERROR getattr(fuse_req_t req, fuse_ino_t ino,
             struct fuse_file_info *fi, struct stat *attr);

    CURVEFS_ERROR setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr,
            int to_set, struct fuse_file_info *fi, struct stat *attrOut);

    void SetFsInfo(std::shared_ptr<FsInfo> fsInfo) {
        fsInfo_ = fsInfo;
    }

    std::shared_ptr<FsInfo> GetFsInfo() {
        return fsInfo_;
    }

 private:
    void GetDentryParamFromInode(const Inode &inode, fuse_entry_param *param);

    void GetAttrFromInode(const Inode &inode, struct stat *attr);

    CURVEFS_ERROR GetMointPoint(const std::string &str, MountPoint *mp);

    CURVEFS_ERROR MakeNode(fuse_req_t req, fuse_ino_t parent,
            const char *name, mode_t mode, FsFileType type,
            fuse_entry_param *e);

    CURVEFS_ERROR RemoveNode(fuse_req_t req, fuse_ino_t parent,
        const char *name);

 private:
    // mds client
    std::shared_ptr<MdsClient> mdsClient_;

    // metaserver client
    std::shared_ptr<MetaServerClient> metaClient_;

    // space client
    std::shared_ptr<SpaceClient> spaceClient_;

    // curve client
    std::shared_ptr<BlockDeviceClient> blockDeviceClient_;

    // inode cache manager
    std::shared_ptr<InodeCacheManager> inodeManager_;

    // dentry cache manager
    std::shared_ptr<DentryCacheManager> dentryManager_;

    // extent manager
    std::shared_ptr<ExtentManager> extManager_;

    // dir buffer
    std::shared_ptr<DirBuffer> dirBuf_;

    // filesystem info
    std::shared_ptr<FsInfo> fsInfo_;
};

}  // namespace client
}  // namespace curvefs

#endif  // CURVEFS_SRC_CLIENT_FUSE_CLIENT_H_
