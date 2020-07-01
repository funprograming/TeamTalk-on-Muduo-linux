/*
 * LoginConn.h
 *
 *  Created on: 2013-6-21
 *      Author: jianqingdu
 */

#ifndef LOGINCONN_H_
#define LOGINCONN_H_

#include <memory>
#include "dispatcher.h"
#include "muduo/net/TcpServer.h"
#include "muduo/net/TcpConnection.h"
#include "pb/protocol/login_msg_server.pb.h"
#include <boost/circular_buffer.hpp>
#include <unordered_set>
#include <unordered_map>
#include "codec.h"

typedef struct  {
    std::string		ip_addr1;	// 电信IP
    std::string		ip_addr2;	// 网通IP
    uint16_t	port;
    uint32_t	max_conn_cnt;
    uint32_t	cur_conn_cnt;
    std::string 		hostname;	// 消息服务器的主机名
} msg_serv_info_t;

//std::unordered_map<int, std::shared_ptr<msg_serv_info_t>> g_msg_serv_info_;
typedef std::shared_ptr<login_msg_server::IMMsgServInfo> MsgServInfoPtr;
typedef std::shared_ptr<login_msg_server::IMUserCntUpdate> UserCntUpdatePtr;
typedef std::shared_ptr<login_msg_server::IMMsgServRsp> MsgServReqPtr;

class LoginConn
{
public:
	LoginConn(muduo::net::EventLoop* loop, const muduo::net::InetAddress& listenAddr,int idleSecond);
	static std::unordered_map<muduo::net::TcpConnectionPtr, std::shared_ptr<msg_serv_info_t>> g_msg_serv_info_;
	void start()
	{
		server_.start();
	}


private:
	void onConnection(const muduo::net::TcpConnectionPtr& conn);
	void onMsgServInfo(const muduo::net::TcpConnectionPtr& conn,const MsgServInfoPtr& msg,muduo::Timestamp time);
	void onMsgServReq(const muduo::net::TcpConnectionPtr& conn,const MsgServReqPtr& msg,muduo::Timestamp);
	void onUserCntUpdat(const muduo::net::TcpConnectionPtr& conn,const UserCntUpdatePtr& message,muduo::Timestamp);
	void onTimer(void);
	void HeartBeart(const muduo::net::TcpConnectionPtr& conn);
	void onMessage(const muduo::net::TcpConnectionPtr& conn,muduo::net::Buffer* buf,muduo::Timestamp time);
	void onUnknownMessage(const muduo::net::TcpConnectionPtr& conn,const MessagePtr& msg, muduo::Timestamp);
	muduo::net::TcpServer server_;
  	ProtobufDispatcher dispatcher_;
    ProtobufCodec codec_;
	int total_online_user_cnt_;
	void dumpConnectionBuckets() const;

	typedef std::weak_ptr<muduo::net::TcpConnection> WeakTcpConnectionPtr;

	struct Entry : public muduo::copyable
	{
		explicit Entry(const WeakTcpConnectionPtr& weakConn)
		: weakConn_(weakConn)
		{
		}

		~Entry()
		{
			muduo::net::TcpConnectionPtr conn = weakConn_.lock();
			if (conn)
			{
				conn->shutdown();
			}
		}

		WeakTcpConnectionPtr weakConn_;
	};
	typedef std::shared_ptr<Entry> EntryPtr;
	typedef std::weak_ptr<Entry> WeakEntryPtr;
	typedef std::unordered_set<EntryPtr> Bucket;
	typedef boost::circular_buffer<Bucket> WeakConnectionList;

	WeakConnectionList connectionBuckets_;

};

#endif /* LOGINCONN_H_ */
