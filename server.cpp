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
#include <atomic>

using namespace std;
namespace fs = std::filesystem;
atomic<int> clientCount(0);
atomic<bool> isRunning(true);
int listenSocket;
string docsPath;

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

void readConfig() {
    ifstream config("../config.txt");
    if (!config) {
        cerr << "[ERROR] config.txt 파일을 열 수 없습니다." << endl;
        exit(1);
    }
    docsPath = "docs";
    string key;
    string equal;
    while (config >> key) {
        if (key == "docs_directory") {
            config >>equal>> docsPath;
            if (!fs::exists(docsPath)) {
                fs::create_directories(docsPath);
            }
        }
    }
    config.close();
}

string getFilePath(const string& title) {
    return docsPath + "/" + title + ".txt";
}

void handleClient(int connectionSocket, sockaddr_in clientAddr) {
    char clientIP[INET_ADDRSTRLEN]; //ip주소 저장
    inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
    int clientPort = ntohs(clientAddr.sin_port);

    ++clientCount;

    cout << "🌐 클라이언트 연결됨!" << endl;
    cout << "IP 주소: " << clientIP << endl;
    cout << "포트 번호: " << clientPort << endl;
    while (true) {
        char buffer[1024];
        string command;
        // 명령어 수신 루프
        ssize_t bytesRead = recv(connectionSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) break;
        buffer[bytesRead] = '\0';
        command += buffer;
        if (command.empty()) break;  // 클라이언트 종료 감지
        vector<string> cmd = splitCommand(command);
        string comments;

        if (cmd.size()==0) {
            comments="명령어를 입력해주세요...\n";
            send(connectionSocket, comments.c_str(), comments.length(), 0);
            continue;
        }
        if (cmd[0]=="create") {
            cout<<"create"<<endl;

            const string filename = getFilePath(cmd[1]);
            ifstream infile(filename);
            if (infile.is_open()) {
                comments="이미 동일한 이름의 파일이 있습니다\n";
                send(connectionSocket, comments.c_str(), comments.length(), 0);
                continue;
            }
            if(cmd.size()<3){
                comments="create 형식이 올바르지 않습니다.\n";
                send(connectionSocket, comments.c_str(), comments.length(), 0);
                continue;
            }
            int content = stoi( cmd[2]);
            if (cmd.size()!=(content+3)) {
                comments="create 형식이 올바르지 않습니다.\n";
                send(connectionSocket, comments.c_str(), comments.length(), 0);
                continue;
            }
            if(content>10){
                comments="섹션은 최대 10개 입니다\n";
                send(connectionSocket, comments.c_str(), comments.length(), 0);
                continue;
            }
            if(cmd[1].size()>64){
                comments="파일 이름은 64바이트 이하로 제한됩니다\n";
                send(connectionSocket, comments.c_str(), comments.length(), 0);
                continue;
            }
            bool isCreate=true;
            for (int i = 0; i < content; i++) {
                if (cmd[3+i].size()>64) {
                    comments="섹션 제목은 64바이트 이하로 제한됩니다\n";
                    send(connectionSocket, comments.c_str(), comments.length(), 0);
                    isCreate=false;
                    break;
                }
            }
            if(!isCreate) continue;
            ofstream file(filename);

            for (int i = 0; i < content; i++) {
                file<<"[Section "<<i+1<<". "<<cmd[3+i]<<"]\n"<<endl;
            }
            file.close();
            string response = "create success "+filename+"\n";
            send(connectionSocket, response.c_str(), response.length(), 0);
         }
         else if (cmd[0]=="read") {
             string contents;
             if (cmd.size() == 1) {
                     for (const auto &entry : filesystem::directory_iterator(docsPath)) {
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
                    const string filename = getFilePath(cmd[1]);
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
             string filename = getFilePath(cmd[1]);
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
             string askLineCount = "몇 줄을 입력할 지 숫자만 입력해주세요(최대 10줄, 초과 시 10줄로 제한됩니다): ";
             send(connectionSocket, askLineCount.c_str(), askLineCount.length(), 0);

             // 줄 개수 입력 받기
             char countBuf[10];
             ssize_t countRead = recv(connectionSocket, countBuf, sizeof(countBuf) - 1, 0);
             countBuf[countRead] = '\0';
             int lineCountInt = stoi(countBuf);
             if (lineCountInt > 10) lineCountInt = 10;

             string input="내용을 입력해주세요\n";
             send(connectionSocket, input.c_str(), input.length(), 0);
             vector<string> sentences;
             for (int i = 0; i < lineCountInt; ++i) {
                 char lineBuf[1024];
                 ssize_t len = recv(connectionSocket, lineBuf, sizeof(lineBuf) - 1, 0);
                 if (len <= 0) {
                     perror("recv");
                     break;
                 }
                 lineBuf[len] = '\0';
                 string sentence(lineBuf);

                 // 줄 길이 제한 (64바이트)
                 if (sentence.size() > 64) {
                     sentence = sentence.substr(0, 64);
                 }

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
             string response = "다음에 봅시다";
             send(connectionSocket, response.c_str(), response.length(), 0);
             close(connectionSocket);
             clientCount-=1;
             if (clientCount==0) {
                 isRunning=false;
                 close(listenSocket);
             }
             return;
         }else {
             comments="유효하지 않은 명령어 입니다\n";
             send(connectionSocket, comments.c_str(), comments.length(), 0);
         }
     }
}

int main(){
    filesystem::create_directory("docs");
    readConfig();
    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
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

    while (isRunning) {
        struct sockaddr_in clientAddr={};
        socklen_t len = sizeof(clientAddr);//최대 받을 있는 크기
        int connectionSocket = accept(listenSocket, (struct sockaddr *)&clientAddr, &len);
        if (connectionSocket < 0) {
            cout<<"연결된 클라이언트 소켓이 모두 종료되었습니다"<<endl;
            continue;
        }
        thread t(handleClient, connectionSocket, clientAddr);
        t.detach();
    }
    cout<<"서버가 종료되었습니다!"<<endl;
    return 0;
}