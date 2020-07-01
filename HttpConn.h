/*
 * HttpConn.h
 *
 *  Created on: 2013-9-29
 *      Author: ziteng
 */

#ifndef __HTTP_CONN_H__
#define __HTTP_CONN_H__

#include "muduo/net/http/HttpServer.h"
#include "muduo/net/http/HttpRequest.h"
#include "muduo/net/http/HttpResponse.h"
#include "muduo/net/EventLoop.h"

class LoginHttpConn
{
public:
    LoginHttpConn(muduo::net::EventLoop *loop, const muduo::net::InetAddress& listenAddr);
    void onRequest(const muduo::net::HttpRequest& req, muduo::net::HttpResponse* resp);
    void start()
    {
	server_.start();
    }
private:
    void _HandleMsgServRequest(muduo::net::HttpResponse* resp);
    muduo::net::HttpServer server_;

};
#endif /* IMCONN_H_ */
