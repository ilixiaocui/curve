#include <glog/logging.h>
#include "src/mds/nameserver2/namespace_storage.h"

DEFINE_string(etcdaddr,
    "10.197.17.10:12379,10.197.17.11:12379,10.197.17.12:12379",
    "etcd addr");
DEFINE_uint64(inodeid, 3527, "inodeid");
DEFINE_bool(delete, false, "delete");

using namespace ::curve::mds;

int main(int argc, char **argv) {
    google::ParseCommandLineFlags(&argc, &argv, false);
    auto client = std::make_shared<EtcdClientImp>();
    std::string addr = FLAGS_etcdaddr;
    char* endpoints = new char[addr.size()];
    EtcdConf conf = {endpoints, addr.size(), 10000};
    std::memcpy(conf.Endpoints, addr.c_str(), addr.size());
    auto res = client->Init(conf, 10000, 5);
    if (res != EtcdErrCode::EtcdOK) {
        LOG(ERROR) << "init client error: " << addr;
        return -1;
    }

    auto cache = std::make_shared<LRUCache>(0);
    auto storage = std::make_shared<NameServerStorageImp>(client, cache);

    // list snapshot
    std::vector<FileInfo> listRes;
    auto res2 = storage->ListSnapshotFile(FLAGS_inodeid, FLAGS_inodeid + 1, &listRes);
    if (res2 != StoreStatus::OK)) {
        LOG(ERROR) << "list error: " << FLAGS_inodeid << " " << res2;
        return -1;
    }

    LOG(INFO) << "snap number: " << listRes.size();
    if (listRes.size() != 1) {
        LOG(ERROR) << "snap number more than 1";
        return -1;
    }

    LOG(INFO) << "snap fileinfo: " << listRes[0].DebugString();


    if (!FLAGS_delete) {
        LOG(INFO) << "get info ok, exit";
        return 0;
    }

    LOG(INFO) << "begin to delete snapshot file info";
    res2 = storage->DeleteSnapshotFile(listRes[0].parentid(), listRes[0].filename());
    if (res != EtcdErrCode::EtcdOK) {
        LOG(ERROR) << "delete snapfile error: " << res2;
        return -1;
    }

    LOG(INFO) << "delete ok";
}
