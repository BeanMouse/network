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
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <map>
using namespace std;
namespace fs = std::filesystem;

struct SectionLock {
    mutex m;
    condition_variable cv;
    queue<int> waitingClients;
    bool isLocked=false;
};
map<string, SectionLock> sectionLocks;

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

void handleClient(int connectionSocket, sockaddr_in clientAddr) {
    char clientIP[INET_ADDRSTRLEN]; //ip주소 저장
    inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
    int clientPort = ntohs(clientAddr.sin_port);

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
        string comments;
        if (cmd.size()==0) {
            comments="명령어를 입력해주세요...\n";
            send(connectionSocket, comments.c_str(), comments.length(), 0);
            continue;
        }
        if (cmd[0]=="create") {
            cout<<"create"<<endl;

            const string& filename = "docs/"+cmd[1]+".txt";
            ifstream infile(filename);
            if (infile.is_open()) {
                comments="이미 동일한 이름의 파일이 있습니다\n";
                send(connectionSocket, comments.c_str(), comments.length(), 0);
                continue;
            }
            ofstream file(filename);
            int content = stoi( cmd[2]);
            if (cmd.size()!=(content+3)) {
                comments="create 형식이 올바르지 않습니다.\n";
                send(connectionSocket, comments.c_str(), comments.length(), 0);
                continue;
            }
            for (int i = 0; i < content; i++) {
                file<<"[Section "<<i+1<<". "<<cmd[3+i]<<"]\n"<<endl;
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
                             else if (inSection && !content.empty()){
                                 contents +="\t"+ content + "\n";
                             }
                         }
                         if (!inSection) {
                             comments=cmd[2]+" 제목의 세션이 존재하지 않습니다\n";
                             send(connectionSocket, comments.c_str(), comments.length(), 0);
                             continue;
                         }
                         infile.close();
                     }else {
                         comments=cmd[1]+" 제목의 파일이 존재하지 않습니다\n";
                         send(connectionSocket, comments.c_str(), comments.length(), 0);
                         continue;
                     }
                 }
             else {
                 contents="올바른 read 형식이 아닙니다 \n \"read\" 또는 read \"제목\" \"섹션\"을 입력해주세요\n";
                 send(connectionSocket, contents.c_str(), contents.length(), 0);
                 continue;
             }

             send(connectionSocket, contents.c_str(), contents.length(), 0);
             cout<<"read"<<endl;
         }else if (cmd[0] == "write") {
             if (cmd.size() != 3) {
                 comments="write 형식은 write \"제목\" \"섹션\"입니다.\n";
                 send(connectionSocket, comments.c_str(), comments.length(), 0);
                 continue;
             }
             string filename = "docs/" + cmd[1] + ".txt";
             ifstream file(filename);
             if (!file.is_open()) {
                 comments = cmd[1]+" 제목의 파일이 존재하지 않습니다\n";
                 send(connectionSocket, comments.c_str(), comments.length(), 0);
                 continue;
             }
             string key = cmd[1] + "/" + cmd[2];
             SectionLock& lock = sectionLocks[key];

             {
                 unique_lock<mutex> lk(lock.m);
                 if (lock.isLocked) {
                     lock.waitingClients.push(connectionSocket);
                     comments = "다른 사용자가 이용 중 입니다.\n";
                     send(connectionSocket, comments.c_str(), comments.length(), 0);

                     lock.cv.wait(lk, [&] {
                         return !lock.isLocked && lock.waitingClients.front() == connectionSocket;
                     });

                     lock.waitingClients.pop();  // 내 차례니까 큐에서 빠짐
                 }
                 lock.isLocked = true;
             }

             vector<string> contents;
             string content;
             while (getline(file, content)) {
                 contents.push_back(content);
             }
             file.close();

             bool inSection = false;
             bool sectionFound = false;
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
                             sectionFound = true;
                             continue;
                         }else if (inSection) {
                             inSection = false;
                         }
                     }
                 }
                 if (!inSection) {
                     newContents.push_back(content);
                 }
             }
             if (!sectionFound) {
                 comments=cmd[2]+" 제목의 섹션이 존재하지 않습니다\n";
                 send(connectionSocket, comments.c_str(), comments.length(), 0);
                 lock.isLocked=false;
                 lock.cv.notify_all();
                 continue;
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

             lock.isLocked=false;
             lock.cv.notify_all();
             string msg = "write success\n";
             send(connectionSocket, msg.c_str(), msg.length(), 0);

         }
         else if (cmd[0]=="bye") {
             cout<<"bye"<<endl;
             string response = "다음에 봅시다";
             send(connectionSocket, response.c_str(), response.length(), 0);
             close(connectionSocket);
             return;
         }else {
             comments="유효하지 않은 명령어 입니다\n";
             send(connectionSocket, comments.c_str(), comments.length(), 0);
         }
     }
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

    while (true) {
        struct sockaddr_in clientAddr={};
        socklen_t len = sizeof(clientAddr);//최대 받을 있는 크기
        int connectionSocket = accept(listenSocket, (struct sockaddr *)&clientAddr, &len);
        if (connectionSocket < 0) {
            perror("accept");
            exit(1);
        }
        thread t(handleClient, connectionSocket, clientAddr);
        t.detach();
    }

    return 0;
}

