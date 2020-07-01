/*
 * HttpConn.cpp
 *
 *  Created on: 2013-9-29
 *      Author: ziteng@mogujie.com
 */

#include "HttpConn.h"
#include "LoginConn.h"
#include "util.h"
#include "jsoncpp/json/json.h"
#include <string>
#include <functional>   // std::bind

using namespace std;
using namespace muduo::net;
using namespace std::placeholders;

LoginHttpConn::LoginHttpConn(EventLoop *loop,
    const InetAddress& listenAddr)
    : server_(loop, listenAddr, "LoginHttpConn")
{
//    server_.setHttpCallback(
//        bind(&LoginHttpConn::onRequest, this, _1, _2));
    server_.setHttpCallback(
	[this](const HttpRequest& req, HttpResponse* resp){this->onRequest(req,resp);});
}

void LoginHttpConn::onRequest(const HttpRequest& req, HttpResponse* resp)
{
    if (strncmp(req.path().c_str(), "/msg_server", 11) == 0) {
        // string content = m_cHttpParser.GetBodyContent();
        _HandleMsgServRequest(resp);
    } else {
        printf("url unknown, url=%s ", req.path().c_str());
        // Close();
    }
}
void LoginHttpConn::_HandleMsgServRequest(HttpResponse* resp)
{
    shared_ptr<msg_serv_info_t> pMsgServInfo;
    uint32_t min_user_cnt = (uint32_t)-1;
    unordered_map<TcpConnectionPtr, shared_ptr<msg_serv_info_t>>::iterator it_min_conn = LoginConn::g_msg_serv_info_.end();
    unordered_map<TcpConnectionPtr, shared_ptr<msg_serv_info_t>>::iterator it;

    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType("text/html");
    // resp->addHeader("Server", "Muduo");

    if(LoginConn::g_msg_serv_info_.size() <= 0)
    {
        Json::Value value;
        value["code"] = 1;
        value["msg"] = "没有msg_server";
        string strContent = value.toStyledString();
        // char* szContent = new char[HTTP_RESPONSE_HTML_MAX];
        // snprintf(szContent, HTTP_RESPONSE_HTML_MAX, HTTP_RESPONSE_HTML, strContent.length(), strContent.c_str());
        // Send((void*)szContent, strlen(szContent));
        // delete [] szContent;
        resp->setBody(strContent);
        return ;
    }
    
    for (it = LoginConn::g_msg_serv_info_.begin() ; it != LoginConn::g_msg_serv_info_.end(); it++) {
        pMsgServInfo = it->second;
        if ( (pMsgServInfo->cur_conn_cnt < pMsgServInfo->max_conn_cnt) &&
            (pMsgServInfo->cur_conn_cnt < min_user_cnt)) {
            it_min_conn = it;
            min_user_cnt = pMsgServInfo->cur_conn_cnt;
        }
    }
    
    if (it_min_conn == LoginConn::g_msg_serv_info_.end()) {
        log("All TCP MsgServer are full ");
        Json::Value value;
        value["code"] = 2;
        value["msg"] = "负载过高";
        string strContent = value.toStyledString();
        // char* szContent = new char[HTTP_RESPONSE_HTML_MAX];
        // snprintf(szContent, HTTP_RESPONSE_HTML_MAX, HTTP_RESPONSE_HTML, strContent.length(), strContent.c_str());
        // Send((void*)szContent, strlen(szContent));
        // conn->shutdown();
        // delete [] szContent;
        resp->setBody(strContent);
        return;
    } else {
        Json::Value value;
        value["code"] = 0;
        value["msg"] = "";
        value["priorIP"] = string(it_min_conn->second->ip_addr1);
        value["backupIP"] = string(it_min_conn->second->ip_addr2);
        value["port"] = to_string(it_min_conn->second->port);
        string strContent = value.toStyledString();
        // char* szContent = new char[HTTP_RESPONSE_HTML_MAX];
        // uint32_t nLen = strContent.length();
        // snprintf(szContent, HTTP_RESPONSE_HTML_MAX, HTTP_RESPONSE_HTML, nLen, strContent.c_str());
        // Send((void*)szContent, strlen(szContent));
        // conn->shutdown();
        // delete [] szContent;
        resp->setBody(strContent);
        return;
    }
}
