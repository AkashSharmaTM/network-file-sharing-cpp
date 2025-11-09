# network-file-sharing-cpp
File sharing application with a server and client architecture, enabling file transfers over sockets. 

# 🚀 Network File Sharing System (C++ + Docker)

A simple **Network File Sharing Application** built in **C++** using the **Client–Server architecture**.  
It enables **file upload, download, authentication, and directory listing** through TCP sockets.  
The entire system is containerized with **Docker** for easy setup and cross-platform compatibility.

---

## 🧠 **Overview**

The project demonstrates fundamental concepts of **computer networking**, **socket programming**, and **containerization**.  
Clients can authenticate with the server, view available files, download them, or upload new ones.

---

## 🧩 **Features**

- 🔐 **User Authentication** (via `users.db`)
- 📁 **File Listing** (`LIST` command)
- ⬇️ **Download Files** from Server (`GET filename`)
- ⬆️ **Upload Files** to Server (`PUT filename`)
- 👋 **Graceful Disconnection** (`QUIT`)
- 🧱 **Dockerized** for portability (runs on any OS with Docker)

---

## 🧰 **Tech Stack**

| Component | Technology |
|------------|-------------|
| Language | C++17 |
| Networking | POSIX TCP Sockets |
| OS | Ubuntu (inside Docker) |
| Build System | Makefile |
| Containerization | Docker & Docker Compose |

---

## 🗂️ **Project Structure**

```
network-file-sharing-cpp/
├── client.cpp
├── server.cpp
├── Makefile
├── users.db
├── server_files/         # Shared folder (contains uploaded/downloaded files)
├── Dockerfile.server
├── Dockerfile.client
├── docker-compose.yml
└── README.md
```

---

## ⚙️ **Setup Instructions**

### 🐋 Prerequisites

- Install **Docker Desktop**
- Ensure **Docker Engine** is running
- (Optional) Install **WSL 2** if using Windows

---

### 🧱 Build and Run with Docker

1. **Open Terminal / PowerShell** and navigate to your project folder:
   ```bash
   cd "C:\Users\<YourName>\Desktop\network-file-sharing-cpp"
   ```

2. **Build Docker images**:
   ```bash
   docker-compose build
   ```

3. **Start the server**:
   ```bash
   docker-compose up
   ```

   You should see:
   ```
   server_1  | Server listening on port 9000, sharing: /app/server_files
   ```

4. **Run the client** (in a new terminal):
   ```bash
   docker-compose run --rm client
   ```

---

## 💻 **Client Commands**

| Command | Description |
|----------|-------------|
| `AUTH username password` | Authenticate the user |
| `LIST` | Display files on the server |
| `GET <filename>` | Download a file from server |
| `PUT <filename>` | Upload a file to server |
| `QUIT` | Disconnect the client |

### Example Session:
```
Username: alice
Password: alicepass

cmd> LIST
F notes.txt 45
END

cmd> GET notes.txt
Saved notes.txt

cmd> PUT newfile.txt
OK
cmd> QUIT
```

---

## 🧩 **users.db Example**

```
# format: username:password
alice:alicepass
bob:bobpass
```

---

## 📂 **Server Shared Folder**

All server files are stored in:
```
server_files/
```

When a client uploads a file (`PUT`), it appears in this folder.  
When a client downloads a file (`GET`), it is fetched from here.

---

## 🧹 **Stopping Containers**

After testing, stop and remove containers safely:
```bash
docker-compose down
```

---

## 🧪 **Testing Locally (Without Docker)**

You can also run it manually in Linux or WSL:

```bash
make
./server 9000 server_files
./client 127.0.0.1 9000
```

Then use the same commands (`LIST`, `GET`, `PUT`, `QUIT`).

---

## 🛡️ **Security Notes**

- This is an **educational** implementation — it uses **plain TCP** (no encryption).
- Do **not** use it on public networks.
- For production, use **TLS (OpenSSL)** and **hashed passwords**.

---

## 👨‍💻 **Author**

**Akash Kumar Sharma**  
B.Tech (CSIT), 7th Semester — ITER  
*Network File Sharing Project (C++ + Docker)*
