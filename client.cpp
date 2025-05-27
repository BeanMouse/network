//
// Created by 이주희 on 25. 5. 7.
//
#include <arpa/inet.h> // inet_pton 함수를 사용하기 위해 필요
#include <fstream> // <fstream> 헤더 파일을 포함하여 파일 입출력을 사용
#include <iostream>
#include <netinet/in.h> //struct sockaddr_in 구조체를 사용하기 위해 필요
#include <sys/socket.h> // 소켓을 사용하기 위해 필요
#include <unistd.h> // close 함수를 사용하기 위해 필요

using namespace std;

int main() {
    string serverIP;
    int serverPort;
    string equal;
    // config 에서 서버의 IP주소와 포트 번호를 받아옴
    ifstream config("config.txt");
    string key;
    while (config >> key) {
        if (key == "docs_server") { // docs_server를 찾아 서버 정보에 대해서 불러옴
            config >> equal >> serverIP >>
                    serverPort; // server = IP 주소, 포트번호로 되어있기 때문에 equal을 통해 = 을 제거
            cout << serverIP << " " << serverPort << endl;
        }
    }
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0); // IPv4바탕, TCP 소켓 생성
    if (clientSocket < 0) {
        perror("socket");
        exit(1);
    }
    cout << "소켓 생성 성공" << endl;

    struct sockaddr_in serverAddress = {}; // IPv4용 주소 구조체
    serverAddress.sin_family = AF_INET; // IPv4 사용
    serverAddress.sin_port = htons(serverPort); // 포트 번호 설정
    inet_pton(AF_INET, serverIP.c_str(), &serverAddress.sin_addr); // 문자열 IP주소를 숫자로 변혈해서 addr에 넣어줌

    if (connect(clientSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) // 서버에 연결    {
        perror("connect");
    exit(1);
}
cout << "연결 성공!" << endl;
while (true) // 계속해서 메시지를 보내고 받는 루프
{
    string msg; // 메시지를 저장할 변수
    cout << "보낼 메시지:";
    getline(cin, msg); // 사용자로부터 메시지를 입력받음
    if (msg.empty()) {
        cout << "메시지를 입력하지 않았습니다. 다시 입력해주세요." << endl;
        continue; // 메시지가 비어있으면 다시 입력받음
    }
    send(clientSocket, msg.c_str(), msg.length(),
         0); // 메시지를 서버로 전송 (클라이언트 소켓, 메시지 포인터, 메시지 길이, 옵션)
    if (msg == "bye") // 메시지로 bye를 받으면 소켓 닫기
    {
        close(clientSocket); // 소켓을 닫음
        exit(0);
    }
    char buffer[1024]; // 서버로부터 받은 메시지를 저장할 버퍼

    if (msg.substr(0, 5) == "write") // 만약 메시지가 write로 시작할때
    {
        while (true) {
            ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0); // 서버로부터 메시지를 받음
            if (bytesRead <= 0) {
                cout << "서버와 연결이 끊겼습니다." << endl;
                break;
            }

            buffer[bytesRead] = '\0'; // 문자열의 끝을 나타내기 위해 널 문자 추가
            string serverMsg(buffer); // 받은 메시지를 문자열로 변환
            cout << serverMsg; // 서버로부터 받은 메시지를 출력

            if (serverMsg.find("다른 사용자가 이용 중") != string::npos) // npos는 문자열이 없다는 의미
            {
                continue;
            } // 만약 서버로부터 받은 메시지에 다른 사용자가 이용 중이라는 메시지가 포함되어 있다면 다시 메시지를
              // 받음

            if (serverMsg.find("몇 줄을 입력할 지") != string::npos) {
                string countMsg;
                getline(cin, countMsg);
                send(clientSocket, countMsg.c_str(), countMsg.length(), 0);
                bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1,
                                 0); //(클라이언트 소켓, 버퍼, 버퍼 크기 - 1, 옵션)
                // 이 소켓으로 들어온 메시지를 버퍼에 저장
                buffer[bytesRead] = '\0';
                cout << buffer;
                int count = stoi(countMsg);

                if (count > 10)
                    count = 10;
                for (int i = 0; i < count; ++i) {
                    cout << "입력 줄 " << (i + 1) << ": ";
                    string line;
                    getline(cin, line);
                    line += '\n';
                    send(clientSocket, line.c_str(), line.length(), 0); // 입력한 줄을 서버로 전송
                }
                bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0); // 서버로부터 응답을 받음
                buffer[bytesRead] = '\0';
                cout << buffer;
                break;
            }

            if (serverMsg.find("존재하지 않습니다") != string::npos || serverMsg.find("형식") != string::npos ||
                serverMsg.find("유효하지 않은") != string::npos) {
                break;
            } // 만약 서버로부터 받은 메시지에 존재하지 않습니다, 형식, 유효하지 않은 등의 메시지가 포함되어 있다면
              // 루프를 종료
        }
    } else // 그외 의 메시지
    {
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0); // 서버로부터 메시지를 받음
        if (bytesRead > 0) // 만약 받은 바이트 수가 0보다 크면
        {
            buffer[bytesRead] = '\0';
            cout << buffer;
        }
    }
}
return 0;
}
