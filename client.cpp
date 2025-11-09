// client.cpp
// Simple TCP client to interact with server: AUTH, LIST, GET, PUT, QUIT

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

bool sendall(int fd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

bool sendline(int fd, const string &line) {
    string out = line + "\n";
    return sendall(fd, out.data(), out.size());
}

string readline_sock(int fd) {
    string s;
    char c;
    while (true) {
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0) return string();
        if (c == '\n') break;
        if (c != '\r') s.push_back(c);
    }
    return s;
}

bool recv_file_to_path(int fd, const string &path, size_t size) {
    ofstream out(path, ios::binary);
    if (!out) return false;
    const size_t BUF = 8192; char buf[BUF];
    size_t remaining = size;
    while (remaining > 0) {
        ssize_t r = recv(fd, buf, (size_t)min(remaining, (size_t)BUF), 0);
        if (r <= 0) { out.close(); return false; }
        out.write(buf, r);
        remaining -= (size_t)r;
    }
    out.close();
    return true;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <server_ip> <port>\n";
        return 1;
    }
    const char* server = argv[1];
    int port = stoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server, &addr.sin_addr) <= 0) { cerr << "Invalid address\n"; return 1; }

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("connect"); return 1; }
    cout << "Connected to " << server << ":" << port << endl;

    // Authenticate
    string username, password;
    cout << "Username: "; getline(cin, username);
    cout << "Password: "; getline(cin, password);

    sendline(sock, string("AUTH ") + username + " " + password);
    string resp = readline_sock(sock);
    if (resp.rfind("OK", 0) != 0) { cerr << "Auth failed: " << resp << endl; close(sock); return 1; }
    cout << "Authenticated\n";

    while (true) {
        cout << "cmd> ";
        string cmdline;
        if (!getline(cin, cmdline)) break;
        if (cmdline.empty()) continue;
        if (cmdline == "QUIT") { sendline(sock, "QUIT"); break; }

        if (cmdline == "LIST") {
            sendline(sock, "LIST");
            while (true) {
                string l = readline_sock(sock);
                if (l.empty()) { cerr << "Connection closed\n"; goto finish; }
                if (l == "END") break;
                cout << l << endl;
            }
        } else if (cmdline.rfind("GET ", 0) == 0) {
            string fname = cmdline.substr(4);
            sendline(sock, "GET " + fname);
            string r = readline_sock(sock);
            if (r.empty()) { cerr << "Connection closed\n"; break; }
            if (r.rfind("OK ", 0) == 0) {
                size_t sz = stoull(r.substr(3));
                cout << "Receiving " << sz << " bytes...\n";
                bool ok = recv_file_to_path(sock, fname, sz);
                if (ok) cout << "Saved " << fname << "\n"; else cout << "Failed to receive file\n";
            } else {
                cout << r << endl;
            }
        } else if (cmdline.rfind("PUT ", 0) == 0) {
            string fname = cmdline.substr(4);
            ifstream in(fname, ios::binary);
            if (!in) { cout << "Local file not found\n"; continue; }
            in.seekg(0, ios::end);
            size_t sz = (size_t)in.tellg();
            in.seekg(0);
            sendline(sock, "PUT " + fname + " " + to_string(sz));
            string r = readline_sock(sock);
            if (r != "OK") { cout << "Server refused: " << r << "\n"; continue; }
            const size_t BUF = 8192; char buf[BUF];
            while (in) {
                in.read(buf, BUF);
                streamsize n = in.gcount();
                if (n > 0) {
                    if (!sendall(sock, buf, (size_t)n)) { cerr << "Send failed\n"; goto finish; }
                }
            }
            string final = readline_sock(sock);
            cout << final << "\n";
        } else {
            cout << "Commands: LIST, GET <filename>, PUT <localfile>, QUIT\n";
        }
    }

finish:
    close(sock);
    return 0;
}
