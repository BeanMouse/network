//
// Created by 이주희 on 25. 5. 7.
//
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

using namespace std;

int main() {
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        perror("socket");
        exit(1);
    }
    cout<<"소켓 생성 성공"<<endl;
    struct sockaddr_in serverAddress = {};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(12345);
    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);
    if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("connect");
        exit(1);
    }
    cout<<"연결 성공!"<<endl;
    string message;
    cout<<"보낼 메시지"<<endl;
    getline(cin, message);
    send(clientSocket, message.c_str(), message.length(), 0);
    cout<<"메시지 전송 성공!"<<endl;
    close(clientSocket);
    return 0;
}