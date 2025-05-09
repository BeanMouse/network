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
    int connectionSocket = ::accept(listenSocket, nullptr, nullptr);
    if (connectionSocket<0) {
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
         ssize_t bytesRead=recv(connectionSocket,buffer,sizeof(buffer)-1,0);
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
            send(connectionSocket, response.c_str(), response.length(), 0);
         }
         else if (cmd[0]=="read") {
             string contents;
             if (cmd.size() == 1) {
                     for (const auto &entry : filesystem::directory_iterator("docs")) {
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
                     }
                 }
             else if (cmd.size()==3) {
                     string filename="docs/"+cmd[1]+".txt";
                     ifstream infile(filename);
                     cout<<filename<<endl;
                     if (infile.is_open()) {
                         string content;
                         bool inSection = false;
                         while (getline(infile, content)) {
                             if (content.starts_with("[Section")) {
                                 size_t dot = content.find('.');
                                 size_t end = content.find(']');
                                 if (dot != string::npos && end != string::npos) {
                                     string s_title = content.substr(dot + 2, end - (dot + 2));
                                     if (s_title == cmd[2]) {
                                         inSection = true;
                                         contents+=content.substr(dot-1,end-(dot-1))+"\n";
                                     }
                                     else if (inSection) break;
                                 }
                             }
                             else if (inSection && !content.empty()) {
                                 cout<<content<<endl;
                                 contents +="\t"+ content + "\n";
                                 cout<<contents<<endl;
                             }
                         }
                         infile.close();
                     }
                 }
             else {
                 contents="올바른 read 형식이 아닙니다 \n \"read\" 또는 read \"제목\" \"섹션\"을 입력해주세요";
             }

             send(connectionSocket, contents.c_str(), contents.length(), 0);
             cout<<"read"<<endl;
         }else if (cmd[0] == "write") {
             string filename = "docs/" + cmd[1] + ".txt";
             ifstream file(filename);
             if (!file.is_open()) {
                 string err = "파일 열기 실패";
                 send(connectionSocket, err.c_str(), err.length(), 0);
                 return -1;
             }
             vector<string> contents;
             string content;
             while (getline(file, content)) {
                 contents.push_back(content);
             }
             file.close();

             bool inSection = false;
             vector<string> newContents;
             for (size_t i = 0; i < contents.size(); i++) {
                 string& content = contents[i];
                 if (content.starts_with("[Section")) {
                     size_t dot = content.find('.');
                     size_t end = content.find(']');
                     if (dot != string::npos && end != string::npos) {
                         string s_title = content.substr(dot + 2, end - (dot + 2));
                         if (s_title == cmd[2]) {
                             inSection = true;
                             newContents.push_back(content);
                             continue;
                         }if (inSection) {
                             inSection = false;
                         }
                     }
                 }
                 if (!inSection) {
                     newContents.push_back(content);
                 }
             }
             string input="내용을 입력해주세요:";
             send(connectionSocket, input.c_str(), input.length(), 0);
             char text[1024];
             ssize_t bytesRead=recv(connectionSocket, text, 1024, 0);
             if (bytesRead<=0) {
                 perror("recv");
                 exit(1);
             }
             text[bytesRead]='\0';
             string response = text;

             istringstream iss(response);
             string sentence;
             vector<string> sentences;
             while (getline(iss,sentence)) {
                 sentences.push_back(sentence);
             }
             auto it = newContents.begin();
             while (it != newContents.end()) {
                 if (it->starts_with("[Section")) {
                     size_t dot = it->find('.');
                     size_t end = it->find(']');
                     if (dot != string::npos && end != string::npos) {
                         string s_title = it->substr(dot + 2, end - (dot + 2));
                         if (s_title == cmd[2]) {
                             ++it;
                             newContents.insert(it, sentences.begin(), sentences.end());
                             break;
                         }
                     }
                 }
                 ++it;
             }
             ofstream outFile(filename);
             for (const auto &line : newContents) {
                 outFile<<line<<"\n";
             }
             outFile.close();

             string msg = "write success\n";
             send(connectionSocket, msg.c_str(), msg.length(), 0);
         }
         else if (cmd[0]=="bye") {
             cout<<"bye"<<endl;
             string response = "bye success";
             send(connectionSocket, response.c_str(), response.length(), 0);
             close(connectionSocket);
             break;
         }
     }
    return 0;
}


