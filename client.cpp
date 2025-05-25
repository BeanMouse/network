//
// Created by 이주희 on 25. 5. 7.
//
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <fstream>

using namespace std;

int main() {
    // config.txt 읽기
    string serverIP;
    int serverPort;
    string equal;

    ifstream config("../config.txt");
    string key;
    while (config >> key) {
        if (key == "docs_server") {
            config >>equal>> serverIP >> serverPort;
            cout<< serverIP << " " << serverPort << endl;
        }
    }
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        perror("socket");
        exit(1);
    }
    cout << "소켓 생성 성공" << endl;

    struct sockaddr_in serverAddress = {};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);
    inet_pton(AF_INET, serverIP.c_str(), &serverAddress.sin_addr);

    if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("connect");
        exit(1);
    }
    cout << "연결 성공!" << endl;
    while (true) {
        string msg;
        cout<<"보낼 메시지:";
        getline(cin, msg);
        send(clientSocket, msg.c_str(), msg.length(), 0);
        if (msg == "bye") {
            close(clientSocket);
            exit(0);
        }
        char buffer[1024];
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            cout << buffer ;
        }
        if (msg.substr(0, 5) == "write") {
            string countMsg;
            getline(cin, countMsg);
            send(clientSocket, countMsg.c_str(), countMsg.length(), 0);

            bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            buffer[bytesRead] = '\0';
            cout << buffer <<endl;

            int count = stoi(countMsg);
            if (count > 10) count = 10;

            for (int i = 0; i < count; ++i) {
                cout << "입력 줄 " << (i + 1) << ": ";
                string line;
                getline(cin, line);
                line += '\n';
                send(clientSocket, line.c_str(), line.length(), 0);
            }

            bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                cout << buffer << endl;
            }
        }
    }
   return 0;

}