/*
 *  Copyright (c) 2020 NetEase Inc.
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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "curvefs/src/client/fuse_client.h"
#include "curvefs/test/client/mock_mds_client.h"
#include "curvefs/test/client/mock_metaserver_client.h"
#include "curvefs/test/client/mock_block_device_client.h"
#include "curvefs/test/client/mock_extent_manager.h"
#include "curvefs/test/client/mock_space_client.h"

namespace curvefs {
namespace client {

using ::testing::Return;
using ::testing::_;
using ::testing::Contains;
using ::testing::SetArgPointee;
using ::curve::common::Configuration;


class TestFuseClient : public ::testing::Test {
 protected:
    TestFuseClient() {}
    ~TestFuseClient() {}

    virtual void SetUp() {
        mdsClient_ = std::make_shared<MockMdsClient>();
        metaClient_ = std::make_shared<MockMetaServerClient>();
        spaceClient_ = std::make_shared<MockSpaceClient>();
        blockDeviceClient_ = std::make_shared<MockBlockDeviceClient>();
        inodeManager_ = std::make_shared<InodeCacheManager>(metaClient_);
        dentryManager_ = std::make_shared<DentryCacheManager>(metaClient_);
        extManager_ = std::make_shared<MockExtentManager>();
        auto dirBuf_ = std::make_shared<DirBuffer>();
        client_  = std::make_shared<FuseClient>(mdsClient_,
            metaClient_,
            spaceClient_,
            blockDeviceClient_,
            inodeManager_,
            dentryManager_,
            extManager_,
            dirBuf_);
        PrepareFsInfo();
    }

    virtual void TearDown() {
        mdsClient_ = nullptr;
        metaClient_ = nullptr;
        spaceClient_ = nullptr;
        blockDeviceClient_ = nullptr;
        extManager_ = nullptr;
    }

    void PrepareFsInfo() {
        auto fsInfo = std::make_shared<FsInfo>();
        fsInfo->set_fsid(fsId);
        fsInfo->set_fsname("xxx");

        client_->SetFsInfo(fsInfo);
        inodeManager_->SetFsId(fsId);
        dentryManager_->SetFsId(fsId);
    }

 protected:
    const uint32_t fsId = 100u;

    std::shared_ptr<MockMdsClient> mdsClient_;
    std::shared_ptr<MockMetaServerClient> metaClient_;
    std::shared_ptr<MockSpaceClient> spaceClient_;
    std::shared_ptr<MockBlockDeviceClient> blockDeviceClient_;
    std::shared_ptr<InodeCacheManager> inodeManager_;
    std::shared_ptr<DentryCacheManager> dentryManager_;
    std::shared_ptr<MockExtentManager> extManager_;
    std::shared_ptr<FuseClient> client_;
};

TEST_F(TestFuseClient, init_when_fs_exist) {
    MountOption mOpts;
    mOpts.mountPoint = "LocalHost:/test";
    mOpts.volume = "xxx";

    std::string fsName = mOpts.volume;

    EXPECT_CALL(*mdsClient_, GetFsInfo(fsName, _))
        .WillOnce(Return(CURVEFS_ERROR::OK));

    FsInfo fsInfoExp;
    fsInfoExp.set_fsid(200);
    fsInfoExp.set_fsname(fsName);
    EXPECT_CALL(*mdsClient_, MountFs(fsName, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(fsInfoExp),
                Return(CURVEFS_ERROR::OK)));

    client_->init(&mOpts, nullptr);

    auto fsInfo = client_->GetFsInfo();
    ASSERT_NE(fsInfo, nullptr);

    ASSERT_EQ(fsInfo->fsid(), fsInfoExp.fsid());
    ASSERT_EQ(fsInfo->fsname(), fsInfoExp.fsname());
}

TEST_F(TestFuseClient, init_when_fs_not_exist) {
    MountOption mOpts;
    mOpts.mountPoint = "host1:/test";
    mOpts.volume = "xxx";

    std::string fsName = mOpts.volume;

    EXPECT_CALL(*mdsClient_, GetFsInfo(fsName, _))
        .WillOnce(Return(CURVEFS_ERROR::NOTEXIST));

    EXPECT_CALL(*mdsClient_, CreateFs(_, _, _))
        .WillOnce(Return(CURVEFS_ERROR::OK));

    FsInfo fsInfoExp;
    fsInfoExp.set_fsid(100);
    fsInfoExp.set_fsname(fsName);
    EXPECT_CALL(*mdsClient_, MountFs(fsName, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(fsInfoExp),
                Return(CURVEFS_ERROR::OK)));

    client_->init(&mOpts, nullptr);

    auto fsInfo = client_->GetFsInfo();
    ASSERT_NE(fsInfo, nullptr);

    ASSERT_EQ(fsInfo->fsid(), fsInfoExp.fsid());
    ASSERT_EQ(fsInfo->fsname(), fsInfoExp.fsname());
}

TEST_F(TestFuseClient, destroy) {
    MountOption mOpts;
    mOpts.mountPoint = "host1:/test";
    mOpts.volume = "xxx";

    std::string fsName = mOpts.volume;

    EXPECT_CALL(*mdsClient_, UmountFs(fsName, _))
        .WillOnce(Return(CURVEFS_ERROR::OK));

    client_->destroy(&mOpts);
}

TEST_F(TestFuseClient, lookup) {
    fuse_req_t req;
    fuse_ino_t parent = 1;
    std::string name = "test";

    fuse_ino_t inodeid = 2;

    Dentry dentry;
    dentry.set_fsid(fsId);
    dentry.set_name(name);
    dentry.set_parentinodeid(parent);
    dentry.set_inodeid(inodeid);

    EXPECT_CALL(*metaClient_, GetDentry(fsId, parent, name, _))
        .WillOnce(DoAll(SetArgPointee<3>(dentry),
                Return(CURVEFS_ERROR::OK)));

    EXPECT_CALL(*metaClient_, GetInode(fsId, inodeid, _))
        .WillOnce(Return(CURVEFS_ERROR::OK));

    fuse_entry_param e;
    CURVEFS_ERROR ret = client_->lookup(req, parent, name.c_str(), &e);
    ASSERT_EQ(CURVEFS_ERROR::OK, ret);
}

extern const uint64_t kMinAllocSize;

TEST_F(TestFuseClient, write) {

    fuse_req_t req;
    fuse_ino_t ino = 1;
    const char *buf = "xxx";
    size_t size = 4;
    off_t off = 0; 
    struct fuse_file_info *fi;
    size_t wSize = 0;

    Inode inode;
    inode.set_inodeid(ino);
    inode.set_length(0);
    EXPECT_CALL(*metaClient_, GetInode(fsId, ino, _))
        .WillOnce(DoAll(SetArgPointee<2>(inode),
                Return(CURVEFS_ERROR::OK)));

    std::list<ExtentAllocInfo> toAllocExtents;
    ExtentAllocInfo allocInfo;
    allocInfo.lOffset = 0;
    allocInfo.pOffsetLeft = 0;
    allocInfo.len = kMinAllocSize;
    toAllocExtents.push_back(allocInfo);
    EXPECT_CALL(*extManager_, GetToAllocExtents(_, off, size, _))
        .WillOnce(DoAll(SetArgPointee<3>(toAllocExtents),
                Return(CURVEFS_ERROR::OK)));

    std::list<Extent> allocatedExtents;
    Extent ext;
    ext.set_offset(0);
    ext.set_length(kMinAllocSize);
    EXPECT_CALL(*spaceClient_, AllocExtents(fsId, _, AllocateType::SMALL, _))
        .WillOnce(DoAll(SetArgPointee<3>(allocatedExtents),
            Return(CURVEFS_ERROR::OK)));

    VolumeExtentList *vlist = new VolumeExtentList();
    VolumeExtent *vext = vlist->add_volumeextents();
    vext->set_fsoffset(0);
    vext->set_volumeoffset(0);
    vext->set_length(kMinAllocSize);
    vext->set_isused(false);
    inode.set_allocated_volumeextentlist(vlist);

    EXPECT_CALL(*extManager_, MergeAllocedExtents(_, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(*vlist),
            Return(CURVEFS_ERROR::OK)));

    std::list<PExtent> pExtents;
    PExtent pext;
    pext.pOffset = 0;
    pext.len = kMinAllocSize;
    pExtents.push_back(pext);
    EXPECT_CALL(*extManager_, DivideExtents(_, off, size, _))
        .WillOnce(DoAll(SetArgPointee<3>(pExtents),
            Return(CURVEFS_ERROR::OK)));

    EXPECT_CALL(*blockDeviceClient_, Write(_, 0, kMinAllocSize))
        .WillOnce(Return(CURVEFS_ERROR::OK));

    EXPECT_CALL(*extManager_, MarkExtentsWritten(off, size, _))
        .WillOnce(Return(CURVEFS_ERROR::OK));


    EXPECT_CALL(*metaClient_, UpdateInode(_))
        .WillOnce(Return(CURVEFS_ERROR::OK));

    CURVEFS_ERROR ret = client_->write(req, ino, buf, size, off, fi, &wSize);

    ASSERT_EQ(CURVEFS_ERROR::OK, ret);
    ASSERT_EQ(size, wSize);
}

TEST_F(TestFuseClient, read) {
    fuse_req_t req;
    fuse_ino_t ino = 1;
    size_t size = 4; 
    off_t off = 0;
    struct fuse_file_info *fi;
    char *buffer;
    size_t rSize = 0;

    Inode inode;
    inode.set_fsid(fsId);
    inode.set_inodeid(ino);
    inode.set_length(4096);
    EXPECT_CALL(*metaClient_, GetInode(fsId, ino, _))
        .WillOnce(DoAll(SetArgPointee<2>(inode),
                Return(CURVEFS_ERROR::OK)));

    std::list<PExtent> pExtents;
    PExtent pext1, pext2;
    pext1.pOffset = 0;
    pext1.len = 4;
    pext1.UnWritten = false;
    pext2.pOffset = 4;
    pext2.len = 4096;
    pext2.UnWritten = true;
    pExtents.push_back(pext1);
    pExtents.push_back(pext2);

    EXPECT_CALL(*extManager_, DivideExtents(_, off, size, _))
        .WillOnce(DoAll(SetArgPointee<3>(pExtents),
            Return(CURVEFS_ERROR::OK)));

    EXPECT_CALL(*blockDeviceClient_, Read(_, 0, 4))
        .WillOnce(Return(CURVEFS_ERROR::OK));

    CURVEFS_ERROR ret = client_->read(req, ino, size, off, fi,
        &buffer, &rSize);
    ASSERT_EQ(CURVEFS_ERROR::OK, ret);
    ASSERT_EQ(4, rSize);
}

TEST_F(TestFuseClient, open) {
    fuse_req_t req;
    fuse_ino_t ino = 1;
    struct fuse_file_info *fi;

    Inode inode;
    inode.set_fsid(fsId);
    inode.set_inodeid(ino);
    inode.set_length(4096);
    inode.set_type(FsFileType::TYPE_FILE);
    EXPECT_CALL(*metaClient_, GetInode(fsId, ino, _))
        .WillOnce(DoAll(SetArgPointee<2>(inode),
                Return(CURVEFS_ERROR::OK)));

    CURVEFS_ERROR ret = client_->open(req, ino, fi);
    ASSERT_EQ(CURVEFS_ERROR::OK, ret);
}

TEST_F(TestFuseClient, create) {
    fuse_req_t req;
    fuse_ino_t parent = 1;
    const char *name = "xxx";
    mode_t mode = 1; 
    struct fuse_file_info *fi;

    fuse_ino_t ino = 2;
    Inode inode;
    inode.set_fsid(fsId);
    inode.set_inodeid(ino);
    inode.set_length(4096);
    EXPECT_CALL(*metaClient_, CreateInode(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(inode),
                Return(CURVEFS_ERROR::OK)));

    EXPECT_CALL(*metaClient_, CreateDentry(_))
        .WillOnce(Return(CURVEFS_ERROR::OK));

    fuse_entry_param e;
    CURVEFS_ERROR ret = client_->create(req, parent, name, mode, fi, &e);
    ASSERT_EQ(CURVEFS_ERROR::OK, ret);
}

TEST_F(TestFuseClient, unlink) {
    fuse_req_t req;
    fuse_ino_t parent = 1;
    std::string name = "xxx";

    fuse_ino_t inodeid = 2;

    Dentry dentry;
    dentry.set_fsid(fsId);
    dentry.set_name(name);
    dentry.set_parentinodeid(parent);
    dentry.set_inodeid(inodeid);

    EXPECT_CALL(*metaClient_, GetDentry(fsId, parent, name, _))
        .WillOnce(DoAll(SetArgPointee<3>(dentry),
                Return(CURVEFS_ERROR::OK)));

    EXPECT_CALL(*metaClient_, DeleteDentry(fsId, parent, name))
        .WillOnce(Return(CURVEFS_ERROR::OK));

    EXPECT_CALL(*metaClient_, DeleteInode(fsId, inodeid))
        .WillOnce(Return(CURVEFS_ERROR::OK));

    CURVEFS_ERROR ret = client_->unlink(req, parent, name.c_str());
    ASSERT_EQ(CURVEFS_ERROR::OK, ret);
}


TEST_F(TestFuseClient, opendir) {
    fuse_req_t req;
    fuse_ino_t ino = 1;
    struct fuse_file_info *fi;

    Inode inode;
    inode.set_fsid(fsId);
    inode.set_inodeid(ino);
    inode.set_length(4);
    inode.set_type(FsFileType::TYPE_DIRECTORY);
    EXPECT_CALL(*metaClient_, GetInode(fsId, ino, _))
        .WillOnce(DoAll(SetArgPointee<2>(inode),
                Return(CURVEFS_ERROR::OK)));

    CURVEFS_ERROR ret = client_->opendir(req, ino, fi);
    ASSERT_EQ(CURVEFS_ERROR::OK, ret);
}

TEST_F(TestFuseClient, openreaddir) {
    fuse_req_t req;
    fuse_ino_t ino = 1; 
    size_t size = 100;
    off_t off = 0;
    struct fuse_file_info *fi = new fuse_file_info();
    fi->fh = 0;
    char *buffer;
    size_t rSize = 0;

    Inode inode;
    inode.set_fsid(fsId);
    inode.set_inodeid(ino);
    inode.set_length(0);
    inode.set_type(FsFileType::TYPE_DIRECTORY);
    EXPECT_CALL(*metaClient_, GetInode(fsId, ino, _))
        .WillOnce(DoAll(SetArgPointee<2>(inode),
                Return(CURVEFS_ERROR::OK)));

    CURVEFS_ERROR ret = client_->opendir(req, ino, fi);
    ASSERT_EQ(CURVEFS_ERROR::OK, ret);


    std::list<Dentry> dentryList;
    Dentry dentry;
    dentry.set_fsid(fsId);
    dentry.set_name("xxx");
    dentry.set_parentinodeid(ino);
    dentry.set_inodeid(2);
    dentryList.push_back(dentry);

    EXPECT_CALL(*metaClient_, ListDentry(fsId, ino, "", _, _))
        .WillOnce(DoAll(SetArgPointee<4>(dentryList),
                Return(CURVEFS_ERROR::OK)));

    ret = client_->readdir(req, ino, size, off, fi,
        &buffer, &rSize);
    ASSERT_EQ(CURVEFS_ERROR::OK, ret);

    delete fi;
}


TEST_F(TestFuseClient, getattr) {
    fuse_req_t req; 
    fuse_ino_t ino = 1;
    struct fuse_file_info *fi; 
    struct stat attr;

    Inode inode;
    inode.set_inodeid(ino);
    inode.set_length(0);
    EXPECT_CALL(*metaClient_, GetInode(fsId, ino, _))
        .WillOnce(DoAll(SetArgPointee<2>(inode),
                Return(CURVEFS_ERROR::OK)));

    CURVEFS_ERROR ret = client_->getattr(req, ino, fi, &attr);
    ASSERT_EQ(CURVEFS_ERROR::OK, ret);
}

TEST_F(TestFuseClient, setattr) {
    fuse_req_t req;
    fuse_ino_t ino = 1;
    struct stat attr;
    int to_set;
    struct fuse_file_info *fi;
    struct stat attrOut;

    Inode inode;
    inode.set_inodeid(ino);
    inode.set_length(0);
    EXPECT_CALL(*metaClient_, GetInode(fsId, ino, _))
        .WillOnce(DoAll(SetArgPointee<2>(inode),
                Return(CURVEFS_ERROR::OK)));

    EXPECT_CALL(*metaClient_, UpdateInode(_))
        .WillOnce(Return(CURVEFS_ERROR::OK));

    attr.st_mode = 1;
    attr.st_uid = 2;
    attr.st_gid = 3;
    attr.st_size = 4;
    attr.st_atime = 5;
    attr.st_mtime = 6;
    attr.st_ctime = 7;

    to_set = FUSE_SET_ATTR_MODE |
             FUSE_SET_ATTR_UID |
             FUSE_SET_ATTR_GID |
             FUSE_SET_ATTR_SIZE |
             FUSE_SET_ATTR_ATIME |
             FUSE_SET_ATTR_MTIME |
             FUSE_SET_ATTR_CTIME;

    CURVEFS_ERROR ret = client_->setattr(req, ino, &attr, to_set, fi, &attrOut);
    ASSERT_EQ(CURVEFS_ERROR::OK, ret);
    ASSERT_EQ(attr.st_mode,  attrOut.st_mode);
    ASSERT_EQ(attr.st_uid,  attrOut.st_uid);
    ASSERT_EQ(attr.st_gid,  attrOut.st_gid);
    ASSERT_EQ(attr.st_size,  attrOut.st_size);
    ASSERT_EQ(attr.st_atime,  attrOut.st_atime);
    ASSERT_EQ(attr.st_mtime,  attrOut.st_mtime);
    ASSERT_EQ(attr.st_ctime,  attrOut.st_ctime);
}

}  // namespace client
}  // namespace curvefs
