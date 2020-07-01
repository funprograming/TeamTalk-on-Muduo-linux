/*
 * LoginConn.cpp
 *
 *  Created on: 2013-6-21
 *      Author: ziteng@mogujie.com
 *  Refactoring on: 2020-6-4
 * 		Author: fwp@sznari.com
 */
#include "LoginConn.h"
#include "pb/protocol/login_msg_server.pb.h"
#include "pb/protocol/server_base.pb.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include <functional>   // std::bind
#include "codec.h"
#include "util.h"

using namespace std::placeholders;
using namespace muduo::net;
using namespace std;
using namespace muduo;
using namespace login_msg_server;
using namespace server_base;

LoginConn::LoginConn(EventLoop* loop,const InetAddress& listenAddr,int idleSeconds)
	: server_(loop, listenAddr, "LoginConn"),
	  dispatcher_(bind(&LoginConn::onUnknownMessage, this, _1, _2, _3)),
	  codec_(bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3))
     // connectionBuckets_(idleSeconds)
{    
	dispatcher_.registerMessageCallback<login_msg_server::IMMsgServInfo>(
		bind(&LoginConn::onMsgServInfo, this, _1, _2, _3));
	dispatcher_.registerMessageCallback<login_msg_server::IMUserCntUpdate>(
		bind(&LoginConn::onUserCntUpdat, this, _1, _2, _3));
	dispatcher_.registerMessageCallback<login_msg_server::IMMsgServRsp>(
		bind(&LoginConn::onMsgServReq, this, _1, _2, _3));

	server_.setConnectionCallback(
		bind(&LoginConn::onConnection, this, _1));
	server_.setMessageCallback(
		bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));

	loop->runEvery(1.0, bind(&LoginConn::onTimer, this));
}
std::unordered_map<muduo::net::TcpConnectionPtr, std::shared_ptr<msg_serv_info_t>> LoginConn::g_msg_serv_info_;

void LoginConn::onUnknownMessage(const TcpConnectionPtr& conn,
                               const MessagePtr& msg,
                               Timestamp)
{
	std::cout << "onUnknownMessageType: " << msg->GetTypeName() << std::endl;
}
void LoginConn::onConnection(const TcpConnectionPtr& conn)
{
    if (conn->connected())
    {
		EntryPtr entry(new Entry(conn));
		connectionBuckets_.back().insert(entry);
		dumpConnectionBuckets();
		WeakEntryPtr weakEntry(entry);
		conn->setContext(weakEntry);
		server_.getLoop()->runEvery(1.0, bind(&LoginConn::HeartBeart, this, conn));
    //   SessionPtr session(new Session(conn));
    //   conn->setContext(session);
    }
    else
    {
		// remove all user count from this message server
				unordered_map<TcpConnectionPtr, shared_ptr<msg_serv_info_t>>::iterator it = LoginConn::g_msg_serv_info_.find(conn);
			if (it != LoginConn::g_msg_serv_info_.end()) {
					shared_ptr<msg_serv_info_t> pMsgServInfo = it->second;

			total_online_user_cnt_ -= pMsgServInfo->cur_conn_cnt;
		    printf("onclose from MsgServer: %s:%u ", pMsgServInfo->hostname.c_str(), pMsgServInfo->port);
//			delete pMsgServInfo;
			LoginConn::g_msg_serv_info_.erase(it);
		}
		assert(!conn->getContext().empty());
		WeakEntryPtr weakEntry(boost::any_cast<WeakEntryPtr>(conn->getContext()));
		LOG_DEBUG << "Entry use_count = " << weakEntry.use_count();
    }
}
void LoginConn::onMsgServInfo(const TcpConnectionPtr& conn,
							    const MsgServInfoPtr& message,
							    Timestamp)
{
	LOG_INFO << "onMsgServInfo:\n" << message->GetTypeName() << message->DebugString();
//	msg_serv_info_t* pMsgServInfo = new msg_serv_info_t;
	shared_ptr<msg_serv_info_t> pMsgServInfo = make_shared<msg_serv_info_t>();
	pMsgServInfo->ip_addr1 = message->ip_addr1();
	pMsgServInfo->ip_addr2 = message->ip_addr2();
	pMsgServInfo->port = message->port();
	pMsgServInfo->max_conn_cnt = message->max_conn_cnt();
	pMsgServInfo->cur_conn_cnt = message->cur_conn_cnt();
	pMsgServInfo->hostname = message->hostname();
	LoginConn::g_msg_serv_info_.insert(make_pair(conn, pMsgServInfo));


	printf("MsgServInfo, ip_addr1=%s, ip_addr2=%s, port=%d, max_conn_cnt=%d, cur_conn_cnt=%d, "\
		"hostname: %s. ",
		pMsgServInfo->ip_addr1.c_str(), pMsgServInfo->ip_addr2.c_str(), pMsgServInfo->port,pMsgServInfo->max_conn_cnt,
		pMsgServInfo->cur_conn_cnt, pMsgServInfo->hostname.c_str());

}

void LoginConn::onMsgServReq(const TcpConnectionPtr& conn,
                               const MsgServReqPtr& msg,
                               Timestamp)
{
	log("HandleMsgServReq. ");

	// no MessageServer available
	if (LoginConn::g_msg_serv_info_.size() == 0) {
        IMMsgServRsp msg;
        msg.set_result_code(REFUSE_REASON_NO_MSG_SERVER);
		codec_.send(conn,msg);
    	conn->shutdown();
		return;
	}

	// return a message server with minimum concurrent connection count
	shared_ptr<msg_serv_info_t> pMsgServInfo;
	uint32_t min_user_cnt = (uint32_t)-1;
	unordered_map<TcpConnectionPtr, shared_ptr<msg_serv_info_t>>::iterator it_min_conn = LoginConn::g_msg_serv_info_.end(),it;

	for (it = LoginConn::g_msg_serv_info_.begin() ; it != g_msg_serv_info_.end(); it++) {
		pMsgServInfo = it->second;
		if ( (pMsgServInfo->cur_conn_cnt < pMsgServInfo->max_conn_cnt) &&
			 (pMsgServInfo->cur_conn_cnt < min_user_cnt))
        {
			it_min_conn = it;
			min_user_cnt = pMsgServInfo->cur_conn_cnt;
		}
	}

	if (it_min_conn == LoginConn::g_msg_serv_info_.end()) {
		printf("All TCP MsgServer are full ");
        IMMsgServRsp msg;
        msg.set_result_code(REFUSE_REASON_MSG_SERVER_FULL);
		codec_.send(conn, msg);
	}
    else
    {
        IMMsgServRsp msg;
        msg.set_result_code(REFUSE_REASON_NONE);
        msg.set_prior_ip(it_min_conn->second->ip_addr1);
        msg.set_backip_ip(it_min_conn->second->ip_addr2);
        msg.set_port(it_min_conn->second->port);
		codec_.send(conn, msg);
    }
	conn->shutdown();
}
void LoginConn::onUserCntUpdat(const TcpConnectionPtr& conn,
                                 const UserCntUpdatePtr& message,
                                 Timestamp)
{
	unordered_map<TcpConnectionPtr, shared_ptr<msg_serv_info_t>>::iterator it = LoginConn::g_msg_serv_info_.find(conn);

	if (it != LoginConn::g_msg_serv_info_.end()) {
		shared_ptr<msg_serv_info_t> pMsgServInfo = it->second;

		uint32_t action = message->user_action();
		if (action == USER_CNT_INC) {
			pMsgServInfo->cur_conn_cnt++;
			total_online_user_cnt_++;
		} else {
			pMsgServInfo->cur_conn_cnt--;
			total_online_user_cnt_--;
		}

		printf("%s:%d, cur_cnt=%u, total_cnt=%u ", pMsgServInfo->hostname.c_str(),
            pMsgServInfo->port, pMsgServInfo->cur_conn_cnt, total_online_user_cnt_);
	}
}

void LoginConn::onTimer(void)
{
	connectionBuckets_.push_back(Bucket());
	dumpConnectionBuckets();
}

void LoginConn::HeartBeart(const TcpConnectionPtr& conn)
{
	IMHeartBeat msg;
	codec_.send(conn, msg);
}

void LoginConn::onMessage(const TcpConnectionPtr& conn,
                           Buffer* buf,
                           Timestamp time)
{
  string msg(buf->retrieveAllAsString());
  LOG_INFO << conn->name() << " echo " << msg.size()
           << " bytes at " << time.toString();
  conn->send(msg);
}

void LoginConn::dumpConnectionBuckets() const
{
  LOG_INFO << "size = " << connectionBuckets_.size();
  int idx = 0;
  for (WeakConnectionList::const_iterator bucketI = connectionBuckets_.begin();
      bucketI != connectionBuckets_.end();
      ++bucketI, ++idx)
  {
    const Bucket& bucket = *bucketI;
    printf("[%d] len = %zd : ", idx, bucket.size());
    for (Bucket::const_iterator it = bucket.begin();
        it != bucket.end();
        ++it)
    {
      bool connectionDead = (*it)->weakConn_.expired();
      printf("%p(%ld)%s, ", get_pointer(*it), it->use_count(),
          connectionDead ? " DEAD" : "");
    }
    puts("");
  }
}

 

