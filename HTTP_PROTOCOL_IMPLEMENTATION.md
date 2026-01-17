# HTTP Protocol Implementation Summary

## Overview

This implementation adds HTTP protocol support to WinSCP, enabling connections through an HTTP-based WebSocket proxy to backend SFTP servers.

## Changes Made

### 1. Core Protocol Support (source/core/)

#### SessionData.h & SessionData.cpp
- Added `fsHTTP = 8` to `TFSProtocol` enum
- Updated `FSPROTOCOL_COUNT` to include HTTP
- Added protocol constants: `HttpProtocol` and `HttpsProtocol`
- Added `BackendUrl` field to `TSessionData` class for storing the backend service URL
- Implemented getter/setter methods for `BackendUrl`
- Updated persistence methods (Load/Save) to store/retrieve `BackendUrl`
- Modified `DefaultPort()` function to return port 80 for HTTP protocol
- Updated `FSProtocolNames` array to include "HTTP"

#### Terminal.h & Terminal.cpp
- Added `cfsHTTP` to `TCurrentFSProtocol` enum
- Registered HTTP protocol in Terminal connection logic
- Added include for `HttpProxyFileSystem.h`
- Implemented protocol switch case to instantiate `THttpProxyFileSystem`

#### HttpProxyFileSystem.h & HttpProxyFileSystem.cpp (NEW)
- Created new filesystem implementation inheriting from `TCustomFileSystem`
- Implemented all required virtual methods from base class
- Added private methods:
  - `DoHttpLogin()` - Performs HTTP authentication to backend
  - `ConnectWebSocket()` - Establishes WebSocket connection
  - `SendWebSocketData()` - Sends data through WebSocket
  - `ReceiveWebSocketData()` - Receives data from WebSocket
- Key features:
  - Supports authentication to HTTP backend
  - WebSocket connection for SFTP command proxying
  - Skeleton implementations for file operations (marked with TODO)

### 2. User Interface (source/forms/)

#### Login.h, Login.cpp & Login.dfm
- Added "HTTP" option to `TransferProtocolCombo` dropdown
- Created `BasicHttpPanel` with:
  - `BackendUrlLabel` - Label for backend URL field
  - `BackendUrlEdit` - Text input for backend service URL
- Updated `FSOrder` array to include `fsHTTP`
- Modified `UpdateControls()` to show/hide HTTP-specific fields
- Updated `LoadSession()` to populate `BackendUrlEdit` from session data
- Updated `SaveSession()` to save `BackendUrlEdit` value to session data
- Adjusted panel visibility logic to handle HTTP protocol

## Architecture

### Connection Flow
```
1. User selects HTTP protocol in login dialog
2. User enters:
   - Username
   - Password
   - Host/Port (for reference, not used directly)
   - Backend URL (e.g., https://backend.example.com)
3. On connection:
   a. THttpProxyFileSystem::Open() is called
   b. DoHttpLogin() sends POST to {BackendUrl}/login with credentials
   c. Receives session token from backend
   d. ConnectWebSocket() connects to ws://{BackendUrl}/ws_proxy?token={token}
   e. Backend establishes TCP tunnel to actual SFTP server
   f. All SFTP commands/data flow through WebSocket tunnel
```

### Protocol Handling
- **Authentication**: HTTP POST to `/login` endpoint
- **WebSocket**: Connection to `/ws_proxy` endpoint with token parameter
- **Data Flow**: SFTP protocol commands sent as WebSocket binary frames
- **Responses**: SFTP responses received through WebSocket

## Implementation Status

### Completed ✓
- Core protocol enumeration and constants
- Session data structure with BackendUrl field
- Protocol registration in Terminal
- Login UI with HTTP option and backend URL input
- HttpProxyFileSystem skeleton with all required methods
- Basic connection lifecycle (Open/Close)

### TODO / Not Implemented
The following features have skeleton implementations with TODO markers:

1. **HTTP Authentication** (`DoHttpLogin`)
   - Need to implement actual HTTP POST request
   - Parse JSON response to extract session token
   - Handle authentication errors

2. **WebSocket Connection** (`ConnectWebSocket`)
   - Integrate WebSocket library (WinHTTP or libwebsockets)
   - Implement WebSocket handshake
   - Handle connection errors and timeouts

3. **WebSocket Data Transfer** (`SendWebSocketData`, `ReceiveWebSocketData`)
   - Implement WebSocket send/receive operations
   - Handle binary frame encoding/decoding
   - Implement buffering and flow control

4. **SFTP Protocol Handling**
   - `ReadDirectory()` - List directory contents through WebSocket
   - `CopyToLocal()` / `Sink()` - Download files
   - `CopyToRemote()` / `Source()` - Upload files
   - `DeleteFile()` - Delete files
   - `RenameFile()` - Rename files
   - `CreateDirectory()` - Create directories
   - `ChangeFileProperties()` - Change permissions/attributes

5. **Error Handling**
   - Network errors
   - Authentication failures
   - WebSocket disconnection/reconnection
   - SFTP command errors

6. **Advanced Features**
   - SSL/TLS support for HTTPS backend URLs
   - WebSocket keep-alive/ping
   - Connection pooling
   - Progress reporting for file transfers

## Security Considerations

### Current Implementation
- Session token stored in memory only
- Credentials transmitted via HTTP POST (should use HTTPS)
- No explicit token expiration handling

### Recommendations
1. **Always use HTTPS** for backend URL to encrypt credentials
2. **Validate SSL certificates** when using HTTPS
3. **Implement token refresh** mechanism for long sessions
4. **Clear sensitive data** from memory on disconnect
5. **Add rate limiting** for authentication attempts
6. **Validate WebSocket origin** to prevent CSRF
7. **Implement timeout** for idle connections

## Testing

### Manual Testing Checklist
- [ ] HTTP protocol appears in login dialog
- [ ] Backend URL field is visible when HTTP selected
- [ ] Backend URL is saved and loaded correctly
- [ ] Connection attempt logs appropriate messages
- [ ] Error handling for missing Backend URL

### Integration Testing (Requires Backend Service)
- [ ] Successful authentication with valid credentials
- [ ] Authentication failure with invalid credentials
- [ ] WebSocket connection establishment
- [ ] Directory listing through WebSocket
- [ ] File download through WebSocket
- [ ] File upload through WebSocket
- [ ] Network error handling
- [ ] Connection timeout handling

## Backend Service Requirements

The implementation assumes a backend service with the following endpoints:

### POST /login
**Request:**
```json
{
  "username": "user",
  "password": "pass"
}
```

**Response:**
```json
{
  "token": "session-token-here",
  "expires": 3600
}
```

### WebSocket /ws_proxy
**Connection:** `ws://backend.example.com/ws_proxy?token=session-token-here`

**Behavior:**
- Accepts WebSocket upgrade request with valid token
- Establishes TCP connection to target SFTP server
- Proxies all WebSocket binary frames to/from SFTP server
- Maintains connection until client disconnects or timeout

## Build Instructions

This implementation requires:
- Embarcadero C++ Builder 11 Professional
- Build Tools for Visual Studio 2022
- WebSocket library (to be integrated)

To build:
```batch
cd /path/to/winscp
build.bat
```

## Future Enhancements

1. **Multiple Authentication Methods**
   - OAuth 2.0 support
   - API key authentication
   - Certificate-based authentication

2. **Connection Options**
   - Configurable connection timeout
   - Automatic reconnection
   - Connection pooling

3. **Performance**
   - Compression for WebSocket data
   - Parallel transfers
   - Chunk size optimization

4. **Monitoring**
   - Connection statistics
   - Transfer speed metrics
   - Error logging

## References

- WinSCP Architecture: See existing `WebDAVFileSystem` and `S3FileSystem` implementations
- WebSocket RFC: RFC 6455
- SFTP Protocol: SSH File Transfer Protocol (draft-ietf-secsh-filexfer)
