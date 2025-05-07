//
// Created by 이주희 on 25. 5. 7.
//
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
using namespace std;
int main(){
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) {
        perror("socket");
        exit(1);
    }
    cout<<"생성 성공"<<endl;
    struct sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(12345);
    if (::bind(listenSocket,(struct sockaddr *)&address, sizeof(address)) < 0)//::는 functional과 헷갈림 방지
    {
        perror("bind");
        exit(1);
    }
    cout<<"바인딩 성공"<<endl;
    if (::listen(listenSocket, 5) < 0) {
        perror("listen");
        exit(1);
    }
    cout<<"연결 대기 성공!"<<endl;
    ::listen(listenSocket, 5);
    int clientSocket = ::accept(listenSocket, nullptr, nullptr);
    if (clientSocket<0) {
        perror("accept");
        exit(1);
    }
    char clientIP[INET_ADDRSTRLEN]; //ip주소 저장
    inet_ntop(AF_INET, &(address.sin_addr), clientIP, INET_ADDRSTRLEN);
    int clientPort = ntohs(address.sin_port);
    cout << "🌐 클라이언트 연결됨!" << endl;
    cout << "IP 주소: " << clientIP << endl;
    cout << "포트 번호: " << clientPort << endl;

    const char *msg = "안녕하세요 클라이언트님!\n";
    send(clientSocket, msg, strlen(msg), 0);

    char buffer[1024];
    ssize_t bytesRead=recv(clientSocket, buffer, sizeof(buffer)-1, 0);
    if (bytesRead<0) {
        perror("recv");
        exit(1);
    }
    buffer[bytesRead] = '\0';
    cout<<"받은 메시지: "<<buffer<<endl;

    close(clientSocket);
    close(listenSocket);
    return 0;
}