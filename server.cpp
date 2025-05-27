//
// Created by 이주희 on 25. 5. 7.
//
#include <arpa/inet.h>// inet_ntop 함수를 사용하기 위해 필요
#include <atomic>// atomic 변수를 사용하기 위해 필요-> 멀티스레드 환경에서 안전하게 변수의 값을 변경할 수 있음
#include <condition_variable>// condition_variable을 사용하여 스레드 간의 동기화를 처리
#include <filesystem>// filesystem을 사용하여 파일 시스템 작업을 처리
#include <fstream>// <fstream> 헤더 파일을 포함하여 파일 입출력을 사용
#include <iostream>// <iostream> 헤더 파일을 포함하여 입출력 스트림을 사용
#include <map>// map을 사용하여 섹션 잠금 정보를 저장
#include <mutex>// mutex를 사용하여 스레드 간의 동기화를 처리
#include <netinet/in.h>// struct sockaddr_in 구조체를 사용하기 위해 필요
#include <queue>// queue를 사용하여 대기 중인 클라이언트를 관리
#include <sstream>// sstream을 사용하여 문자열 스트림을 처리
#include <string>// string을 사용하여 문자열을 처리
#include <sys/socket.h>// 소켓을 사용하기 위해 필요
#include <thread>// thread를 사용하여 멀티스레딩을 구현
#include <unistd.h>// close 함수를 사용하기 위해 필요
#include <vector>// vector를 사용하여 명령어와 섹션 제목을 저장

using namespace std;

namespace fs = std::filesystem;
//atomic은 모든 클라이언트가 공유하는 변수
atomic<int> clientCount(0);// 클라이언트 수를 저장하는 atomic 변수
atomic<bool> isRunning(true);// 서버가 실행 중인지 여부를 저장하는 atomic 변수
int listenSocket;// 서버 소켓을 저장하는 변수
string docsPath;// 문서 디렉토리 경로를 저장하는 변수

struct SectionLock // 섹션 잠금 정보를 저장하는 구조체
{
    mutex m; // 섹션 잠금을 위한 뮤텍스
    condition_variable cv;// 대기 중인 클라이언트를 위한 조건 변수
    queue<int> waitingClients;// 대기 중인 클라이언트 소켓을 저장하는 큐
    bool isLocked = false;// 섹션이 잠겨 있는지 여부를 저장하는 변수
};

map<string, SectionLock> sectionLocks; // 섹션 잠금 정보를 저장하는 맵 -> string 키는 "제목/섹션" 형식으로 섹션을 식별 vaule는 SectionLock 구조체


vector<string> splitCommand(const string &input) // 명령어를 공백으로 분리하는 함수
{
    vector<string> tokens;// 명령어를 저장할 벡터
    stringstream ss(input);// 문자열 스트림을 사용하여 입력 문자열을 처리/
    string token;// 토큰을 저장할 문자열 변수
    while (ss >> token) // 문자열 스트림에서 공백으로 구분된 토큰을 읽어옴
    {
        if (token.find('"') == 0) {
            token = token.substr(1, token.length() - 2);// 첫 번째와 마지막 따옴표를 제거
            tokens.push_back(token);// 따옴표가 있는 토큰을 벡터에 추가
        } else {
            tokens.push_back(token);// 따옴표가 없는 토큰을 벡터에 추가
        }
    }
    return tokens;
}

void readConfig() // config.txt 파일에서 docs_directory 경로를 읽어오는 함수
{
    ifstream config("config.txt");
    if (!config) {
        cerr << "config.txt 파일을 열 수 없습니다." << endl;
        exit(1);
    }
    string key;
    string equal;// docs_directory = 경로 형식으로 되어있기 때문에 equal을 통해 = 을 제거
    while (config >> key) {
        if (key == "docs_directory") {
            config >> equal >> docsPath;
            if (!fs::exists(docsPath)) // docsPath가 존재하지 않으면
            {
                fs::create_directories(docsPath);// docsPath 디렉토리를 생성
            }
        }
    }
    config.close();// config.txt 파일을 닫음
}

string getFilePath(const string &title) { return docsPath + "/" + title + ".txt"; }// 제목에 해당하는 파일 경로를 반환하는 함수

void handleClient(int connectionSocket, sockaddr_in clientAddr) //연결 하는 소켓, 클라이언트 주소를 인자로 받음
{
    char clientIP[INET_ADDRSTRLEN];//INET_ADDRSTRLEN은 IPv4 주소의 최대 길이 -> IPv4 주소를 문자열로 저장하기 위한 버퍼
    inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);// 클라이언트의 IP 주소를 문자열로 변환하여 clientIP에 저장
    int clientPort = ntohs(clientAddr.sin_port);
    ++clientCount;// 클라이언트 수를 증가시킴 atomic 변수이기 때문에 멀티스레드 환경에서도 안전하게 증가시킬 수 있음
    cout << "🌐 클라이언트 연결됨!" << endl;
    cout << "IP 주소: " << clientIP << endl;
    cout << "포트 번호: " << clientPort << endl;
    while (true) // 클라이언트와의 연결이 유지되는 동안 명령어를 처리하는 루프
    {
        char buffer[1024];
        string command;
        ssize_t bytesRead = recv(connectionSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0)// 클라이언트가 연결을 종료했거나 오류가 발생한 경우
            break;
        buffer[bytesRead] = '\0';
        command += buffer;
        string comments;
        if (command.empty())// 클라이언트가 아무것도 입력하지 않은 경우{
        {
            comments = "명령어를 입력해주세요...\n";
            send(connectionSocket, comments.c_str(), comments.length(), 0);
            continue;
            
        }
        vector<string> cmd = splitCommand(command);// 명령어 문자열을 공백과 따옴표 기준으로 나누어 벡터로 저장

        if (cmd.size() == 0) // 명령어가 비어있는 경우
        {
            comments = "명령어를 입력해주세요...\n";
            send(connectionSocket, comments.c_str(), comments.length(), 0);
            continue;
        }
        if (cmd[0] == "create") // create 명령어 처리
        {
            cout << "create" << endl;

            const string filename = getFilePath(cmd[1]);
            ifstream infile(filename);
            //예외 처리 
            if (infile.is_open()) {
                comments = "이미 동일한 이름의 파일이 있습니다\n";
                send(connectionSocket, comments.c_str(), comments.length(), 0);
                continue;
            }
            if (cmd.size() < 3) {
                comments = "create 형식이 올바르지 않습니다.\n";
                send(connectionSocket, comments.c_str(), comments.length(), 0);
                continue;
            }
            int content = stoi(cmd[2]);
            if (cmd.size() != (content + 3)) {
                comments = "create 형식이 올바르지 않습니다.\n";
                send(connectionSocket, comments.c_str(), comments.length(), 0);
                continue;
            }
            if (content > 10) {
                comments = "섹션은 최대 10개 입니다\n";
                send(connectionSocket, comments.c_str(), comments.length(), 0);
                continue;
            }
            if (cmd[1].size() > 64) {
                comments = "파일 이름은 64바이트 이하로 제한됩니다\n";
                send(connectionSocket, comments.c_str(), comments.length(), 0);
                continue;
            }
            bool isCreate = true;
            for (int i = 0; i < content; i++) {
                if (cmd[3 + i].size() > 64) {
                    comments = "섹션 제목은 64바이트 이하로 제한됩니다\n";
                    send(connectionSocket, comments.c_str(), comments.length(), 0);
                    isCreate = false;// 섹션 제목이 64바이트를 초과하면 생성하지 않음
                    break;
                }
            }
            if (!isCreate)
                continue;
            ofstream file(filename);

            for (int i = 0; i < content; i++) {
                file << "[Section " << i + 1 << ". " << cmd[3 + i] << "]\n" << endl;// 섹션 제목을 파일에 작성
            }
            file.close();// 파일을 닫음
            string response = "create success " + filename + "\n";// 파일 생성 성공 메시지
            send(connectionSocket, response.c_str(), response.length(), 0);
        } else if (cmd[0] == "read") // read 명령어 처리
        {
            string contents; 
            if (cmd.size() == 1) //read 유형 1
            {
                for (const auto &entry: filesystem::directory_iterator(docsPath)) // docsPath 디렉토리 내의 모든 파일을 읽음
                {
                    if (entry.is_regular_file() && entry.path().extension() == ".txt") // .txt 확장자를 가진 파일만 처리
                    {
                        ifstream file(entry.path());
                        if (file.is_open()) {
                            string title = entry.path().filename().string();
                            contents += title.substr(0, title.length() - 4) + "\n";// 파일 이름에서 .txt 확장자를 제거하고 제목으로 사용
                            string content;
                            while (getline(file, content)) {
                                if (content.starts_with("[Section")) //섹션 제목을 찾아줌
                                {
                                    string s_title = content.substr(9, content.length() - 10);// 섹션 제목을 추출
                                    contents += "\t" + s_title + "\n";
                                }
                            }
                            contents += "\n";
                            file.close();
                        }
                    }
                }
            } else if (cmd.size() == 3) //read 유형2
            {
                const string filename = getFilePath(cmd[1]);// 제목에 해당하는 파일 경로를 가져옴
                size_t start = cmd[1].find('"');
                size_t end = cmd[1].find('"', start + 1);
                string title = cmd[1].substr(start + 1, end - start - 1);
                contents += title + "\n";// 제목을 출력
                ifstream infile(filename);
                cout << filename << endl;
                if (infile.is_open()) {
                    string content;
                    bool inSection = false;
                    while (getline(infile, content))// 파일에서 한 줄씩 읽음
                    {
                        if (content.starts_with("[Section")) //섹션 제목 찾기
                        {
                            size_t dot = content.find('.');
                            size_t end = content.find(']');
                            if (dot != string::npos && end != string::npos) {
                                string s_title = content.substr(dot + 2, end - (dot + 2));
                                if (s_title == cmd[2]) {
                                    inSection = true;
                                    contents += "\t" + content.substr(dot - 1, end - (dot - 1)) + "\n";
                                } else if (inSection)
                                    break;
                            }
                        } else if (inSection && !content.empty()) {
                            contents += "\t\t" + content + "\n";// 섹션 내용 출력
                        }
                    }
                    if (!inSection) {
                        comments = cmd[2] + " 제목의 세션이 존재하지 않습니다\n";
                        send(connectionSocket, comments.c_str(), comments.length(), 0);
                        continue;
                    }
                    infile.close();
                } else {
                    comments = cmd[1] + " 제목의 파일이 존재하지 않습니다\n";
                    send(connectionSocket, comments.c_str(), comments.length(), 0);
                    continue;
                }
            } else {
                contents = "올바른 read 형식이 아닙니다 \n \"read\" 또는 read \"제목\" \"섹션\"을 입력해주세요\n";
                send(connectionSocket, contents.c_str(), contents.length(), 0);
                continue;
            }

            send(connectionSocket, contents.c_str(), contents.length(), 0);
            cout << "read" << endl;
        } else if (cmd[0] == "write") // write 명령어 처리
        {
            //예외 처리
            if (cmd.size() != 3) {
                comments = "write 형식은 write \"제목\" \"섹션\"입니다.\n";
                send(connectionSocket, comments.c_str(), comments.length(), 0);
                continue;
            }
            string filename = getFilePath(cmd[1]);
            ifstream file(filename);
            if (!file.is_open()) {
                comments = cmd[1] + " 제목의 파일이 존재하지 않습니다\n";
                send(connectionSocket, comments.c_str(), comments.length(), 0);
                continue;
            }
            string key = cmd[1] + "/" + cmd[2];// 섹션 잠금을 위한 키 생성
            SectionLock &lock = sectionLocks[key];// 섹션 잠금을 위한 SectionLock 구조체 참조

            {
                unique_lock<mutex> lk(lock.m);
                if (lock.isLocked) {
                    lock.waitingClients.push(connectionSocket);
                    comments = "다른 사용자가 이용 중 입니다.\n";
                    send(connectionSocket, comments.c_str(), comments.length(), 0);
                    char flushBuf[1024];
                    recv(connectionSocket, flushBuf, sizeof(flushBuf) - 1,
                         MSG_DONTWAIT); // 클라이언트가 잘못 보낸 입력 제거

                    lock.cv.wait(lk, [&] { return !lock.isLocked && lock.waitingClients.front() == connectionSocket; });

                    lock.waitingClients.pop(); // 내 차례니까 큐에서 빠짐
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
                string &content = contents[i];
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
                        } else if (inSection) {
                            inSection = false;
                        }
                    }
                }
                if (!inSection) {
                    newContents.push_back(content);
                }
            }
            if (!sectionFound) {
                comments = cmd[2] + " 제목의 섹션이 존재하지 않습니다\n";
                send(connectionSocket, comments.c_str(), comments.length(), 0);
                lock.isLocked = false;
                lock.cv.notify_all();
                continue;
            }

            //줄을 받아서 입력을 받아옴
            string askLineCount = "몇 줄을 입력할 지 숫자만 입력해주세요(최대 10줄, 초과 시 10줄로 제한됩니다): ";
            send(connectionSocket, askLineCount.c_str(), askLineCount.length(), 0);
            char countBuf[10];
            ssize_t countRead = recv(connectionSocket, countBuf, sizeof(countBuf) - 1, 0);
            countBuf[countRead] = '\0';
            int lineCountInt = stoi(countBuf);
            if (lineCountInt > 10)
                lineCountInt = 10;

            string input = "내용을 입력해주세요\n";
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
                if (sentence.size() > 64) {
                    sentence = sentence.substr(0, 64);
                }

                sentences.push_back(sentence);
            }
            //섹션에 입력된 줄을 삽입
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
            for (const auto &line: newContents) {
                outFile << line << "\n";
            }
            outFile.close();

            lock.isLocked = false;
            lock.cv.notify_all();
            string msg = "write success\n";
            send(connectionSocket, msg.c_str(), msg.length(), 0);

        } else if (cmd[0] == "bye") // 클라이언트가 연결을 종료할 때
        {
            string response = "다음에 봅시다";
            send(connectionSocket, response.c_str(), response.length(), 0);
            close(connectionSocket);
            clientCount -= 1;// 클라이언트 수를 감소시킴
            if (clientCount == 0) // 모든 클라이언트가 연결 종료
            {
                isRunning = false;
                close(listenSocket);
            }
            return;
        } else { // 유효하지 않은 명령어 예외 처리
            comments = "유효하지 않은 명령어 입니다\n";
            send(connectionSocket, comments.c_str(), comments.length(), 0);
        }
    }
}

int main(int argc, char *argv[]) //argc로 프로그램 실행 시 인자 개수를 확인하고, *argv로 IP주소와 포트번호를 받아옴
{
    if (argc != 3) // 프로그램 실행 시 IP주소와 포트번호가 입력되지 않았는지 확인
    {
        cerr << "IP주소와 포트번호가 입력되지 않았습니다" << endl;
        return 1;
    }
    string serverIP = argv[1]; // 서버 IP 주소를 받아옴
    int serverPort = stoi(argv[2]);// 서버 포트 번호를 받아옴
    readConfig();// config.txt 파일에서 docs_directory 경로를 읽어옴
    ofstream config("config.txt");// config.txt 파일을 열어 서버 IP 주소와 포트 번호, docs_directory 경로를 저장
    config << "docs_server = " << serverIP << " " << serverPort << endl;
    config << "docs_directory = " << docsPath << endl;
    config.close();
    listenSocket = socket(AF_INET, SOCK_STREAM, 0);// IPv4 바탕, TCP listen 소켓 생성
    if (listenSocket < 0) {
        perror("socket");
        exit(1);
    }
    cout << "생성 성공" << endl;
    struct sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(serverIP.c_str());
    address.sin_port = htons(serverPort);
    if (::bind(listenSocket, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("bind");
        exit(1);
    }
    cout << "바인딩 성공" << endl;
    if (::listen(listenSocket, 5) < 0) {
        perror("listen");
        exit(1);
    }
    cout << "연결 대기 성공!" << endl;

    while (isRunning) // 서버가 실행 중인 동안 클라이언트 연결을 기다림
    {
        struct sockaddr_in clientAddr = {};// 클라이언트 주소 구조체 초기화
        socklen_t len = sizeof(clientAddr);// 클라이언트 주소 구조체의 크기를 저장
        int connectionSocket = accept(listenSocket, (struct sockaddr *) &clientAddr, &len);//connectionSocket에 클라이언트 소켓을 저장
        if (connectionSocket < 0) {
            cout << "연결된 클라이언트 소켓이 모두 종료되었습니다" << endl;
            continue;
        }
        thread t(handleClient, connectionSocket, clientAddr);// 클라이언트 소켓을 처리하는 스레드를 생성
        t.detach();// 스레드를 분리하여 독립적으로 실행되도록 함
    }
    cout << "서버가 종료되었습니다!" << endl;
    return 0;
}
