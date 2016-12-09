[中文介绍](https://github.com/liaosanity/Opendfs/blob/master/README.zh_CN.md)

# Overview
Opendfs is written in C/C++, it is a distributed file storage system, which is highly fault-tolerant, high concurrency, high throughput, high performance, and easy to scale-out and scale-in, the structure of file and directory stored in the cluster, is very similar with the Linux file system.

Like the HDFS, an Opendfs cluster also contain DFSClient, Namenode, Datanode of three kinds of roles, a file will be cut into multiple data blocks storage to the cluster by DFSClient, while the Datanode is responsible to keep the data blocks, Namenode is responsible to maintain the file mapping relationship, which consists of how many pieces, and these blocks have been stored in which Datanode, an overall architecture of cluster is:
![image](https://github.com/liaosanity/Opendfs/raw/master/images/overall_architecture.png)
##Each role is introduced as follows:
 * Namenode, the Metadata storage nodes, responsible for the management of metadata. Each metadata operation(write, delete) is not only been updated in the memory, but also been logged to a local disk file(editlog), before writing to disk it will be synchronized to other Namenodes through paxos protocol. At a checkpoint time(configurable), each Namenode will do the same thing, dump the memory image to a local disk file(fsimage), then the editlog is not always too big. Every time of rebooting Namenode, the metadata mapping relationship will be rebuilt through reconstruction of the fsimage and editlog.
 * Datanode, the Data storage node, responsible for the management of the file data blocks. Each block will be regarded as a file stored to the local file system. Datanode will register to all Namenodes when it starts, then it will report some informations(healthy, remaining capacity, activity connections, the current storage blocks, and whether the block is damaged, etc) to all Namenodes through the heartbeat request, at the same time receive some operational orders from Namenode, such as duplicate, move, delete block, etc.
 * DFSClient, the important way of user interaction with the cluster. Provide some commands like Linux file system(ls, rm, rmr...), and the Posix interface etc. Responsible to cut the file into multiple pieces, then write to Datanode in parallel. By reading data blocks from different Datanodes, then merge them into a complete file before return to the application layer.

# Features
 * The files stored in Opendfs, their metadata and file content will be stored separately, Unlike HDFS, the Namenode can not only help scale systems, but can do it in a fault tolerant way. Through the paxos protocol, metadata can be synchronized between primary and standby in the end. In a network model processing, HDFS can only handle one connection at the same time in one thread, But Opendfs can handle multiple connections at the same time in one thread by epoll, so as to improve the efficiency of concurrent access of the network.
 * Files in the cluster will be cut into multiple data blocks(each block 256M by default, configurable), each block will be stored in different Datanode in 3 copies(3 by default, configurable). When the block damage or loss, they will be automatic repaired, to ensure the reliability of the data storage. When sending a block to the DFSClient, it will be copied from disk to network directly by sendfile, to reduce user mode and kernel mode context switching, so as to improve the efficiency of data transmission.
 * In HDFS, Before writing block to Datanode, DFSClient will create a Pipeline data flow(Datanode1 -> Datanode2 -> Datanode3) firstly, then transfer the data in serialization, but in Opendfs, data blocks will be writing in parallel.

# A simple file writing process
![images](https://github.com/liaosanity/Opendfs/raw/master/images/writing_process.png)

# A simple file reading process
![images](https://github.com/liaosanity/Opendfs/raw/master/images/reading_process.png)

# Building
 * Environment dependence:   
   Linux, GCC4.8+  
 * Software dependence:  
   yum -y install pcre-devel  
 * Source compile:  
   ./configure --prefix=/home/opendfs  
   make  
   make install  

# Running a real cluster
We'll run all the servers on localhost, first need to create three configuration files. You can base yours off of xxx.conf.default, or the following will work:
 * namenode.conf  
   Server server;  
   server.daemon = ALLOW;  
   server.workers = 8;  
   server.connections = 65536;  
   server.bind_for_cli = "0.0.0.0:8000";  
   server.bind_for_dn = "0.0.0.0:8001";  
   server.my_paxos = "0.0.0.0:8002";  
   server.ot_paxos = "0.0.0.0:8002";  
   server.paxos_group_num = 1;  
   server.checkpoint_num = 10000;  
   server.index_num = 1000000;  
   server.editlog_dir = "/data00/namenode/editlog";  
   server.fsimage_dir = "/data00/namenode/fsimage";  
   server.error_log = "/data00/namenode/logs/error.log";  
   server.pid_file = "/data00/namenode/pid/namenode.pid";  
   server.coredump_dir = "/data00/namenode/coredump";  
   server.log_level = LOG_INFO;  
   server.recv_buff_len = 64KB;  
   server.send_buff_len = 64KB;  
   server.max_tqueue_len = 1000;  
   server.dn_timeout = 600;  
 
 * datanode.conf  
   Server server;  
   server.daemon = ALLOW;  
   server.workers = 8;  
   server.connections = 65536;  
   server.bind_for_cli = "0.0.0.0:8100";  
   server.ns_srv = "127.0.0.1:8001";  
   server.data_dir = "/data01/block,/data02/block,/data03/block";  
   server.error_log = "/data00/datanode/logs/error.log";  
   server.pid_file = "/data00/datanode/pid/datanode.pid";  
   server.coredump_dir = "/data00/datanode/coredump/";  
   server.log_level = LOG_INFO;  
   server.recv_buff_len = 64KB;  
   server.send_buff_len = 64KB;  
   server.max_tqueue_len = 1000;  
   server.heartbeat_interval = 3;  
   server.block_report_interval = 3600;  

 * dfscli.conf  
   Server server;  
   server.daemon = ALLOW;  
   server.namenode_addr = "127.0.0.1:8000";  
   server.error_log = "";  
   server.log_level = LOG_INFO;  
   server.recv_buff_len = 64KB;  
   server.send_buff_len = 64KB;  
   server.blk_sz = 256MB;  
   server.blk_rep = 3;  

Secondary, run the app:
 * Namenode:  
   sbin/namenode  
 * Datanode:  
   sbin/datanode  

# Running basic tests:  
```
$ sbin/dfscli  
Usage: sbin/dfscli cmd...  
       -mkdir <path>   
       -rmr <path>   
       -ls <path>   
       -put <local path> <remote path>   
       -get <remote path> <local path>   
       -rm <path>  
```

# Thanks
Thank you [phxpaxos](https://github.com/tencent-wechat/phxpaxos), it makes our work much easier.
