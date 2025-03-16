# Simple TCP Server & Client in C

This project implements a basic **TCP server** and **client** in C.  
It includes two server implementations:  

- **IOCP-based server** (Efficient for high-performance networking on Windows)
- **select-based server** (Simpler, portable approach)

## 📂 Files

- `IOCP_server.c` → TCP server using **IOCP** (Windows-only)
- `select_server.c` → TCP server using **select()** (Cross-platform)
- `client.c` → TCP client to connect and communicate with the server

📝 Notes

- Ensure the server is running before starting the client

📜 MIT License
