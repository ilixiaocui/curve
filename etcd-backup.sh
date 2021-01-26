#!/bin/bash

# 备份路径
backupPath=/etc/curve/etcd
rmdays=7

# 获取leader etcd
leader_etcd=`curve_ops_tool etcd-status | grep "current etcd:" | awk '{print $3}'`
if [ -z "$leader_etcd"]
then
    echo "get etcd leader fail"
    exit 1
fi

# 备份leader的数据库
sudo etcdctl --endpoints ${leader_etcd} snapshot save ${backupPath}/snapshot_`date +%Y%m%d%H%M`.db
if [ $? -ne 0 ]
then
    echo "backup etcd fail"
    exit 1
fi

# 7天前备份数据删除
sudo find ${backupPath} -name "snapshot_*" -ctime +${rmdays} | xargs rm -f
if [ $? -ne 0 ]
then
    echo "remove history backup fail"
fi

