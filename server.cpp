// server.cpp

#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

using namespace std;

volatile bool g_running = true;

void sigint_handler(int) { g_running = false; }

// load username:password pairs from users.db (format: user:pass, lines starting with # ignored)
map<string,string> load_users(const string &path) {
    map<string,string> m;
    ifstream in(path);
    string line;
    while (getline(in, line)) {
        if (line.empty()) continue;
        if (line[0] == '#') continue;
        auto pos = line.find(':');
        if (pos == string::npos) continue;
        string u = line.substr(0, pos);
        string p = line.substr(pos + 1);
        m[u] = p;
    }
    return m;
}

// read a single line terminated by '\n' from socket fd (strips '\r' and '\n')
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

// send file bytes to socket after sending "OK <size>"
void send_file_bytes(int fd, const string &path) {
    ifstream in(path, ios::binary);
    if (!in) {
        sendline(fd, "ERR file not found");
        return;
    }
    in.seekg(0, ios::end);
    size_t sz = (size_t)in.tellg();
    in.seekg(0);
    sendline(fd, "OK " + to_string(sz));
    const size_t BUF = 8192;
    char buf[BUF];
    while (in) {
        in.read(buf, BUF);
        streamsize r = in.gcount();
        if (r > 0) {
            if (!sendall(fd, buf, (size_t)r)) return;
        }
    }
}

// receive exactly `size` bytes from socket and write to `path` (atomic write using temp file)
bool recv_file_bytes(int fd, const string &path, size_t size) {
    string tmp = path + ".part";
    ofstream out(tmp, ios::binary);
    if (!out) return false;
    const size_t BUF = 8192;
    char buf[BUF];
    size_t remaining = size;
    while (remaining > 0) {
        ssize_t r = recv(fd, buf, (size_t)min(remaining, (size_t)BUF), 0);
        if (r <= 0) { out.close(); unlink(tmp.c_str()); return false; }
        out.write(buf, r);
        remaining -= (size_t)r;
    }
    out.close();
    // atomic rename
    if (rename(tmp.c_str(), path.c_str()) != 0) {
        unlink(tmp.c_str());
        return false;
    }
    return true;
}

void handle_client(int clientfd, const map<string,string> &users, const string &share_dir) {
    string authed_user;
    while (true) {
        string line = readline_sock(clientfd);
        if (line.empty()) break; // client closed
        // parse command and argument
        istringstream iss(line);
        string cmd;
        iss >> cmd;
        string arg;
        getline(iss, arg);
        if (!arg.empty() && arg[0] == ' ') arg.erase(0,1);

        if (cmd == "AUTH") {
            string user, pass;
            istringstream a(arg);
            a >> user >> pass;
            auto it = users.find(user);
            if (it != users.end() && it->second == pass) {
                authed_user = user;
                sendline(clientfd, "OK");
            } else {
                sendline(clientfd, "ERR auth failed");
            }
        } else if (cmd == "LIST") {
            if (authed_user.empty()) { sendline(clientfd, "ERR not authed"); continue; }
            DIR *d = opendir(share_dir.c_str());
            if (!d) { sendline(clientfd, "ERR cannot open share"); continue; }
            struct dirent *ent;
            while ((ent = readdir(d)) != NULL) {
                if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
                string full = share_dir + "/" + ent->d_name;
                struct stat st;
                if (stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
                    sendline(clientfd, string("F ") + ent->d_name + " " + to_string((long long)st.st_size));
                }
            }
            closedir(d);
            sendline(clientfd, "END");
        } else if (cmd == "GET") {
            if (authed_user.empty()) { sendline(clientfd, "ERR not authed"); continue; }
            // prevent directory traversal by refusing paths with ..
            if (arg.find("..") != string::npos || arg.find('/') != string::npos) { sendline(clientfd, "ERR invalid filename"); continue; }
            string path = share_dir + "/" + arg;
            send_file_bytes(clientfd, path);
        } else if (cmd == "PUT") {
            if (authed_user.empty()) { sendline(clientfd, "ERR not authed"); continue; }
            // arg: filename size
            string fname; long long sz;
            istringstream a(arg); a >> fname >> sz;
            if (fname.empty() || sz <= 0) { sendline(clientfd, "ERR invalid args"); continue; }
            if (fname.find("..") != string::npos || fname.find('/') != string::npos) { sendline(clientfd, "ERR invalid filename"); continue; }
            sendline(clientfd, "OK");
            string dest = share_dir + "/" + fname;
            bool ok = recv_file_bytes(clientfd, dest, (size_t)sz);
            if (ok) sendline(clientfd, "OK");
            else sendline(clientfd, "ERR write failed");
        } else if (cmd == "QUIT") {
            break;
        } else {
            sendline(clientfd, "ERR unknown cmd");
        }
    }
    close(clientfd);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <port> <share_dir>\n";
        cerr << "Example: ./server 9000 server_files\n";
        return 1;
    }
    signal(SIGINT, sigint_handler);
    int port = stoi(argv[1]);
    string share = argv[2];

    // ensure share exists
    mkdir(share.c_str(), 0755);

    map<string,string> users = load_users("users.db");

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) { perror("socket"); return 1; }
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(listenfd, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(listenfd, 10) < 0) { perror("listen"); return 1; }
    cout << "Server listening on port " << port << ", sharing: " << share << endl;

    while (g_running) {
        sockaddr_in cli{};
        socklen_t clilen = sizeof(cli);
        int clientfd = accept(listenfd, (sockaddr*)&cli, &clilen);
        if (clientfd < 0) {
            if (!g_running) break;
            perror("accept");
            continue;
        }
        pid_t pid = fork();
        if (pid == 0) {
            // child
            close(listenfd);
            handle_client(clientfd, users, share);
            exit(0);
        } else if (pid > 0) {
            // parent
            close(clientfd);
            // clean reaped children
            while (waitpid(-1, NULL, WNOHANG) > 0) {}
        } else {
            perror("fork");
            close(clientfd);
        }
    }

    close(listenfd);
    return 0;
}
