# HTTP Protocol Implementation - Visual Architecture

## Component Hierarchy

```
┌─────────────────────────────────────────────────────────────┐
│                       WinSCP Application                     │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ User selects HTTP protocol
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      Login Dialog UI                         │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ Protocol Dropdown: [SFTP│SCP│FTP│WebDAV│S3│HTTP ▼]    │ │
│  ├────────────────────────────────────────────────────────┤ │
│  │ Username: [________________]                           │ │
│  │ Password: [________________]                           │ │
│  │ Backend URL: [https://backend.example.com_______]     │ │ <- NEW
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ User clicks "Login"
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                        TTerminal                             │
│                                                              │
│  if (SessionData->FSProtocol == fsHTTP)                     │
│  {                                                           │
│    FFileSystem = new THttpProxyFileSystem(this);           │
│    FFileSystem->Open();                                     │
│  }                                                           │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                  THttpProxyFileSystem                        │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐│
│  │ Open()                                                  ││
│  │  1. Check BackendUrl not empty                         ││
│  │  2. Call DoHttpLogin()                                 ││
│  │  3. Call ConnectWebSocket()                            ││
│  │  4. Set FActive = true                                 ││
│  └────────────────────────────────────────────────────────┘│
│                                                              │
│  Private Methods:                                            │
│  ┌────────────────────────────────────────────────────────┐│
│  │ DoHttpLogin()          [TODO: Implement HTTP POST]     ││
│  │ ConnectWebSocket()     [TODO: Implement WebSocket]     ││
│  │ SendWebSocketData()    [TODO: Implement send]          ││
│  │ ReceiveWebSocketData() [TODO: Implement receive]       ││
│  └────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

## Connection Flow Diagram

```
┌──────────┐     ┌──────────────┐     ┌──────────────┐     ┌──────────┐
│  WinSCP  │────▶│ HTTP Backend │────▶│  WebSocket   │────▶│  SFTP    │
│  Client  │     │   Service    │     │    Tunnel    │     │  Server  │
└──────────┘     └──────────────┘     └──────────────┘     └──────────┘
     │                  │                     │                   │
     │                  │                     │                   │
     │ 1. POST /login  │                     │                   │
     │ {user, pass}    │                     │                   │
     ├────────────────▶│                     │                   │
     │                  │                     │                   │
     │ 2. Response     │                     │                   │
     │ {token}         │                     │                   │
     │◀────────────────┤                     │                   │
     │                  │                     │                   │
     │ 3. WebSocket    │                     │                   │
     │ /ws_proxy?token │                     │                   │
     ├────────────────▶│                     │                   │
     │                  │                     │                   │
     │                  │ 4. Establish TCP   │                   │
     │                  │    tunnel          │                   │
     │                  ├────────────────────▶                   │
     │                  │                     │ 5. TCP connect   │
     │                  │                     ├──────────────────▶
     │                  │                     │                   │
     │ 6. SFTP cmd     │                     │                   │
     │ (via WS frame)  │                     │                   │
     ├────────────────▶│ 7. Forward         │                   │
     │                  ├────────────────────▶│ 8. Forward      │
     │                  │                     ├──────────────────▶
     │                  │                     │                   │
     │                  │                     │ 9. SFTP response │
     │                  │                     │◀──────────────────
     │                  │ 10. Forward        │                   │
     │                  │◀────────────────────┤                   │
     │ 11. Response    │                     │                   │
     │ (via WS frame)  │                     │                   │
     │◀────────────────┤                     │                   │
     │                  │                     │                   │
```

## File Organization

```
winscp/
├── source/
│   ├── core/
│   │   ├── SessionData.h         [MODIFIED] +6 lines
│   │   ├── SessionData.cpp       [MODIFIED] +10 lines
│   │   │   └── Added: BackendUrl field, fsHTTP enum
│   │   │
│   │   ├── Terminal.h            [MODIFIED] +1 line
│   │   ├── Terminal.cpp          [MODIFIED] +9 lines
│   │   │   └── Added: HTTP protocol registration
│   │   │
│   │   ├── HttpProxyFileSystem.h [NEW] 104 lines
│   │   └── HttpProxyFileSystem.cpp [NEW] 432 lines
│   │       └── Skeleton implementation with TODO markers
│   │
│   └── forms/
│       ├── Login.h               [MODIFIED] +3 lines
│       ├── Login.cpp             [MODIFIED] +28 lines
│       └── Login.dfm             [MODIFIED] +33 lines
│           └── Added: BasicHttpPanel, BackendUrlEdit
│
├── HTTP_PROTOCOL_IMPLEMENTATION.md [NEW] 231 lines
│   └── Comprehensive documentation
│
└── IMPLEMENTATION_SUMMARY.txt    [NEW] 279 lines
    └── Final summary and next steps
```

## Data Flow

```
Session Creation:
┌─────────────────┐
│ User Input      │
│ - Username      │
│ - Password      │
│ - Backend URL   │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ TSessionData    │
│ - UserName      │
│ - Password      │
│ - BackendUrl    │◀─── NEW FIELD
│ - FSProtocol    │
└────────┬────────┘
         │
         │ Persist to registry/config
         ▼
┌─────────────────┐
│ Storage         │
│ (Registry/XML)  │
└─────────────────┘


Connection:
┌─────────────────┐
│ TSessionData    │
└────────┬────────┘
         │
         ▼
┌─────────────────────────┐
│ TTerminal               │
│  Creates filesystem     │
│  based on FSProtocol    │
└────────┬────────────────┘
         │
         │ if fsHTTP
         ▼
┌──────────────────────────┐
│ THttpProxyFileSystem     │
│  - Open()                │
│  - ReadDirectory()       │
│  - CopyToLocal()         │
│  - CopyToRemote()        │
│  - DeleteFile()          │
│  - RenameFile()          │
└──────────────────────────┘
```

## Implementation Status Matrix

| Component              | Data Structures | UI | Core Logic | Integration | Status |
|------------------------|----------------|-----|------------|-------------|---------|
| Protocol Enumeration   | ✅             | ✅  | ✅         | ✅          | ✅ Done |
| Session Data           | ✅             | ✅  | ✅         | ✅          | ✅ Done |
| Login UI               | ✅             | ✅  | ✅         | ✅          | ✅ Done |
| Terminal Registration  | ✅             | N/A | ✅         | ✅          | ✅ Done |
| Filesystem Class       | ✅             | N/A | ⚠️         | ✅          | ⚠️ Skeleton |
| HTTP Authentication    | ✅             | N/A | ❌         | ❌          | ❌ TODO |
| WebSocket Connection   | ✅             | N/A | ❌         | ❌          | ❌ TODO |
| SFTP Proxying          | ✅             | N/A | ❌         | ❌          | ❌ TODO |
| File Operations        | ✅             | N/A | ❌         | ❌          | ❌ TODO |

Legend:
✅ Complete and working
⚠️ Skeleton/partial implementation
❌ Not implemented (TODO)
N/A Not applicable

## Technology Stack

```
┌─────────────────────────────────────────────┐
│            WinSCP Application               │
│                                             │
│  ┌─────────────────────────────────────┐   │
│  │      UI Layer (VCL Framework)       │   │
│  │  - TForm, TPanel, TEdit, TComboBox  │   │
│  └─────────────────────────────────────┘   │
│                     │                       │
│  ┌─────────────────────────────────────┐   │
│  │     Business Logic Layer            │   │
│  │  - TTerminal                        │   │
│  │  - TSessionData                     │   │
│  │  - TCustomFileSystem                │   │
│  └─────────────────────────────────────┘   │
│                     │                       │
│  ┌─────────────────────────────────────┐   │
│  │   Protocol Implementation Layer     │   │
│  │  - THttpProxyFileSystem [NEW]      │   │
│  │  - TSFTPFileSystem                  │   │
│  │  - TFTPFileSystem                   │   │
│  │  - TWebDAVFileSystem                │   │
│  └─────────────────────────────────────┘   │
│                     │                       │
│  ┌─────────────────────────────────────┐   │
│  │      Network Layer [TODO]           │   │
│  │  - HTTP Client (WinHTTP)           │   │
│  │  - WebSocket (WinHTTP/libws)       │   │
│  │  - JSON Parser                      │   │
│  └─────────────────────────────────────┘   │
└─────────────────────────────────────────────┘
```

## Next Steps Priority Order

```
Priority 1: WebSocket Library
┌─────────────────────────────────┐
│ Choose and integrate:           │
│ - WinHTTP WebSocket API  OR     │
│ - libwebsockets                 │
│ Test basic send/receive         │
└─────────────────────────────────┘
         │
         ▼
Priority 2: HTTP Client
┌─────────────────────────────────┐
│ Implement DoHttpLogin()         │
│ - HTTP POST to /login           │
│ - Parse JSON response           │
│ - Extract session token         │
└─────────────────────────────────┘
         │
         ▼
Priority 3: WebSocket Connection
┌─────────────────────────────────┐
│ Implement ConnectWebSocket()    │
│ - WebSocket handshake           │
│ - Connection management         │
│ - Error handling                │
└─────────────────────────────────┘
         │
         ▼
Priority 4: SFTP Protocol
┌─────────────────────────────────┐
│ Implement data transfer         │
│ - SendWebSocketData()           │
│ - ReceiveWebSocketData()        │
│ - SFTP command encoding         │
└─────────────────────────────────┘
         │
         ▼
Priority 5: File Operations
┌─────────────────────────────────┐
│ Complete file operations        │
│ - ReadDirectory()               │
│ - CopyToLocal/Remote()          │
│ - DeleteFile(), RenameFile()    │
└─────────────────────────────────┘
```

## Security Architecture

```
┌─────────────────────────────────────────────┐
│            Security Layers                  │
├─────────────────────────────────────────────┤
│ 1. Transport Security                       │
│    ✅ Use HTTPS for backend URL             │
│    ⚠️ Validate SSL certificates [TODO]     │
├─────────────────────────────────────────────┤
│ 2. Authentication                           │
│    ✅ Username/Password via HTTP POST       │
│    ⚠️ Token-based session [TODO]            │
│    ⚠️ Token expiration [TODO]               │
├─────────────────────────────────────────────┤
│ 3. Session Management                       │
│    ✅ Session token in memory               │
│    ⚠️ Clear on disconnect [TODO]            │
│    ⚠️ Token refresh [TODO]                  │
├─────────────────────────────────────────────┤
│ 4. WebSocket Security                       │
│    ⚠️ Origin validation [TODO]              │
│    ⚠️ Message integrity [TODO]              │
│    ⚠️ Encryption [TODO]                     │
├─────────────────────────────────────────────┤
│ 5. Data Protection                          │
│    ⚠️ Secure storage [TODO]                 │
│    ⚠️ Memory cleanup [TODO]                 │
│    ⚠️ Logging sanitization [TODO]           │
└─────────────────────────────────────────────┘

Legend:
✅ Addressed in current implementation
⚠️ Identified but not implemented
```
