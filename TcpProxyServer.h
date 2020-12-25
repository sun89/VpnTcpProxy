#ifndef TCPPROXYSERVER_H
#define TCPPROXYSERVER_H

bool TcpProxyServer_begin(const char *destServer, unsigned short _reservedPort);
bool TcpProxyServer_start();
bool TcpProxyServer_setReservedPort(unsigned short port);
bool TcpProxyServer_setDestinationServer(const char *destServer);

#endif
