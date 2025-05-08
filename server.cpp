//
// Created by 이주희 on 25. 5. 7.
//
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <filesystem>
using namespace std;
namespace fs = std::filesystem;

struct

vector<string> splitCommand(const string &input) {
    vector<string> tokens;
    stringstream ss(input);
    string token;
    while (ss >> token) {
        if (token.find('"')==0) {
            token=token.substr(1,token.length()-2);
            tokens.push_back(token);
        }
        else {
            tokens.push_back(token);
        }
    }
    return tokens;
}

int main(){
    filesystem::create_directory("docs");
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
     while (true) {
         char buffer[1024];
         ssize_t bytesRead=recv(clientSocket,buffer,sizeof(buffer)-1,0);
         if (bytesRead<=0) break;
         buffer[bytesRead]='\0';
         string command(buffer);
         vector<string> cmd = splitCommand(command);

        if (cmd[0]=="create") {
            cout<<"create"<<endl;

            const string& filename = "docs/"+cmd[1]+".txt";
            ofstream file(filename);
            int content = stoi( cmd[2]);
            for (int i = 0; i < content; i++) {
                file<<"[Section "<<i+1<<". "<<cmd[3+i]<<" ]\n"<<endl;
            }
            file.close();
            string response = "create success "+filename;
            send(clientSocket, response.c_str(), response.length(), 0);
         }
         else if (cmd[0]=="read") {

                 string contents;
                 for (const auto &entry : filesystem::directory_iterator("docs")) {
                     if (cmd.size() == 1) {
                         if (entry.is_regular_file()&&entry.path().extension()==".txt") {
                             ifstream file(entry.path());
                             if (file.is_open()) {
                                 string title=entry.path().filename().string();
                                 contents+=title.substr(0,title.length()-4)+"\n";
                                 string content;
                                 while (getline(file, content)) {
                                     if (content.starts_with("[Section")) {
                                         string s_title=content.substr(9,content.length()-10);
                                         contents+="\t"+s_title+"\n";
                                     }
                                 }
                                 contents+="\n";
                                 file.close();
                             }
                         }
                     }else if (cmd.size()==3) {
                         if (entry.is_regular_file()
                             &&entry.path().extension()==".txt"
                             &&entry.path().filename().string()==(cmd[1]+".txt")) {
                             ifstream file(entry.path());
                             if (file.is_open()) {
                                 string content;
                                 bool inSection = false;
                                 while (getline(file, content)) {
                                     if (content.starts_with("[Section")) {
                                         size_t dot = content.find('.');
                                         size_t end = content.find(']');
                                         if (dot != string::npos && end != string::npos) {
                                             string s_title = content.substr(dot + 2, end - (dot + 2));
                                             if (s_title == cmd[2]) {
                                                 inSection = true;
                                                 continue;
                                             }
                                             if (inSection) break;
                                         }
                                     }
                                     if (inSection && !content.empty()) {
                                         contents += content + "\n";
                                     }
                                 }
                                 file.close();
                             }
                         }
                     }
                 }
             send(clientSocket, contents.c_str(), contents.length(), 0);
             cout<<"read"<<endl;

         }else if (cmd[0]=="write") {
             cout<<"write"<<endl;
             string response = "write success";
             send(clientSocket, response.c_str(), response.length(), 0);
         }
         else if (cmd[0]=="bye") {
             cout<<"bye"<<endl;
             string response = "bye success";
             send(clientSocket, response.c_str(), response.length(), 0);
             break;
         }
     }


    close(clientSocket);
    close(listenSocket);
    return 0;
}