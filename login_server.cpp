/*
 * login_server.cpp
 *
 *  Created on: 2013-6-21
 *      Author: ziteng@mogujie.com
 */


#include "pb/protocol/login_msg_server.pb.h"
#include "muduo/base/Logging.h"
#include "LoginConn.h"
#include "HttpConn.h"
#include "ConfigFileReader.h"
#include "muduo/net/EventLoop.h"


using namespace muduo;
using namespace muduo::net;


int main(int argc, char* argv[])
{
    EventLoop loop;

	if ((argc == 2) && (strcmp(argv[1], "-v") == 0)) {
//		printf("Server Version: LoginServer/%s\n", VERSION);
		printf("Server Build: %s %s\n", __DATE__, __TIME__);
		return 0;
	}
	log("pid:%d\n",getpid());

	// signal(SIGPIPE, SIG_IGN);

	CConfigFileReader config_file("loginserver.conf");

    char* http_listen_ip = config_file.GetConfigName("HttpListenIP");
    char* str_http_port = config_file.GetConfigName("HttpPort");
	char* msg_server_listen_ip = config_file.GetConfigName("MsgServerListenIP");
	char* str_msg_server_port = config_file.GetConfigName("MsgServerPort");

	if (!msg_server_listen_ip || !str_msg_server_port || !http_listen_ip
        || !str_http_port) {
		log("config item missing, exit... ");
		return -1;
	}

	uint16_t msg_server_port = atoi(str_msg_server_port);
    uint16_t http_port = atoi(str_http_port);
    


	CStrExplode msg_server_listen_ip_list(msg_server_listen_ip, ';');
	for (uint32_t i = 0; i < msg_server_listen_ip_list.GetItemCnt(); i++) {
		LoginConn loginConn(&loop, InetAddress(msg_server_listen_ip_list.GetItem(i), msg_server_port),5);
		loginConn.start();
	}
    
    CStrExplode http_listen_ip_list(http_listen_ip, ';');
    for (uint32_t i = 0; i < http_listen_ip_list.GetItemCnt(); i++) {
		LoginHttpConn loginhttpServer(&loop, InetAddress(http_listen_ip_list.GetItem(i), http_port));
		loginhttpServer.start();
    }

	log("now enter the event loop...\n");
    
	loop.loop();

	return 0;
}
