//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "WebSocketProxyServer.h"
#include "Common.h"
#include "Exceptions.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <memory>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
__fastcall TWebSocketProxyServer::TWebSocketProxyServer(
  const UnicodeString & Url,
  const UnicodeString & UserName,
  const UnicodeString & Password)
  : FUrl(Url),
    FUserName(UserName),
    FPassword(Password),
    FListenSocket(INVALID_SOCKET),
    FClientSocket(INVALID_SOCKET),
    FSessionHandle(NULL),
    FConnectionHandle(NULL),
    FWebSocketHandle(NULL),
    FBridgeThread(NULL),
    FBridgeThreadId(0),
    FLocalPort(0),
    FRunning(false),
    FStopping(false)
{
  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);
}
//---------------------------------------------------------------------------
__fastcall TWebSocketProxyServer::~TWebSocketProxyServer()
{
  Stop();
  WSACleanup();
}
//---------------------------------------------------------------------------
bool __fastcall TWebSocketProxyServer::Start()
{
  try
  {
    if (!AuthenticateHttp())
    {
      return false;
    }
    
    if (!StartTcpListener())
    {
      return false;
    }
    
    if (!ConnectWebSocket())
    {
      CleanupSockets();
      return false;
    }
    
    // Create bridge thread
    FBridgeThread = CreateThread(NULL, 0, BridgeThreadProc, this, 0, &FBridgeThreadId);
    if (FBridgeThread == NULL)
    {
      CleanupSockets();
      CleanupWinHttp();
      return false;
    }
    
    FRunning = true;
    return true;
  }
  catch (...)
  {
    Stop();
    return false;
  }
}
//---------------------------------------------------------------------------
void __fastcall TWebSocketProxyServer::Stop()
{
  if (!FRunning)
  {
    return;
  }
  
  FStopping = true;
  FRunning = false;
  
  // Wait for bridge thread to exit
  if (FBridgeThread != NULL)
  {
    WaitForSingleObject(FBridgeThread, 5000);
    CloseHandle(FBridgeThread);
    FBridgeThread = NULL;
  }
  
  CleanupSockets();
  CleanupWinHttp();
}
//---------------------------------------------------------------------------
int __fastcall TWebSocketProxyServer::GetLocalPort() const
{
  return FLocalPort;
}
//---------------------------------------------------------------------------
bool __fastcall TWebSocketProxyServer::AuthenticateHttp()
{
  // Parse URL to extract host and path
  UnicodeString Host = FUrl;
  UnicodeString Path = L"/";
  
  int SlashPos = Host.Pos(L"://");
  if (SlashPos > 0)
  {
    Host = Host.SubString(SlashPos + 3, Host.Length());
  }
  
  SlashPos = Host.Pos(L"/");
  if (SlashPos > 0)
  {
    Path = Host.SubString(SlashPos, Host.Length());
    Host = Host.SubString(1, SlashPos - 1);
  }
  
  // Initialize WinHTTP
  FSessionHandle = WinHttpOpen(
    L"WinSCP-WebSocket-Proxy/1.0",
    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
    WINHTTP_NO_PROXY_NAME,
    WINHTTP_NO_PROXY_BYPASS,
    0);
    
  if (FSessionHandle == NULL)
  {
    return false;
  }
  
  // Extract port from host if present
  int Port = 80;
  int ColonPos = Host.Pos(L":");
  if (ColonPos > 0)
  {
    Port = StrToIntDef(Host.SubString(ColonPos + 1, Host.Length()), 80);
    Host = Host.SubString(1, ColonPos - 1);
  }
  
  FConnectionHandle = WinHttpConnect(
    FSessionHandle,
    Host.c_str(),
    Port,
    0);
    
  if (FConnectionHandle == NULL)
  {
    CleanupWinHttp();
    return false;
  }
  
  // Create POST request
  HINTERNET hRequest = WinHttpOpenRequest(
    FConnectionHandle,
    L"POST",
    (Path + L"login").c_str(),
    NULL,
    WINHTTP_NO_REFERER,
    WINHTTP_DEFAULT_ACCEPT_TYPES,
    0);
    
  if (hRequest == NULL)
  {
    CleanupWinHttp();
    return false;
  }
  
  // Prepare JSON body: {"username":"xxx","password":"xxx"}
  UnicodeString JsonBody = L"{\"username\":\"" + FUserName + L"\",\"password\":\"" + FPassword + L"\"}";
  UTF8String Utf8Body = UTF8String(JsonBody);
  
  // Set content-type header
  WinHttpAddRequestHeaders(
    hRequest,
    L"Content-Type: application/json",
    -1L,
    WINHTTP_ADDREQ_FLAG_ADD);
  
  // Send request
  BOOL bResults = WinHttpSendRequest(
    hRequest,
    WINHTTP_NO_ADDITIONAL_HEADERS,
    0,
    (LPVOID)Utf8Body.c_str(),
    Utf8Body.Length(),
    Utf8Body.Length(),
    0);
    
  if (!bResults)
  {
    WinHttpCloseHandle(hRequest);
    CleanupWinHttp();
    return false;
  }
  
  // Receive response
  bResults = WinHttpReceiveResponse(hRequest, NULL);
  if (!bResults)
  {
    WinHttpCloseHandle(hRequest);
    CleanupWinHttp();
    return false;
  }
  
  // Read response body
  DWORD dwSize = 0;
  DWORD dwDownloaded = 0;
  std::unique_ptr<char[]> pszOutBuffer;
  UnicodeString ResponseBody;
  
  do
  {
    dwSize = 0;
    if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
    {
      break;
    }
    
    if (dwSize == 0)
    {
      break;
    }
    
    pszOutBuffer.reset(new char[dwSize + 1]);
    ZeroMemory(pszOutBuffer.get(), dwSize + 1);
    
    if (!WinHttpReadData(hRequest, pszOutBuffer.get(), dwSize, &dwDownloaded))
    {
      break;
    }
    
    ResponseBody += UnicodeString(UTF8String(pszOutBuffer.get()));
  }
  while (dwSize > 0);
  
  WinHttpCloseHandle(hRequest);
  
  // Parse token from JSON response: {"token":"xxx"}
  int TokenPos = ResponseBody.Pos(L"\"token\"");
  if (TokenPos > 0)
  {
    int StartQuote = ResponseBody.Pos(L"\"", TokenPos + 7);
    if (StartQuote > 0)
    {
      int EndQuote = ResponseBody.Pos(L"\"", StartQuote + 1);
      if (EndQuote > 0)
      {
        FToken = ResponseBody.SubString(StartQuote + 1, EndQuote - StartQuote - 1);
        return !FToken.IsEmpty();
      }
    }
  }
  
  return false;
}
//---------------------------------------------------------------------------
bool __fastcall TWebSocketProxyServer::StartTcpListener()
{
  FListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (FListenSocket == INVALID_SOCKET)
  {
    return false;
  }
  
  // Bind to 127.0.0.1 with automatic port selection
  sockaddr_in service;
  service.sin_family = AF_INET;
  service.sin_addr.s_addr = inet_addr("127.0.0.1");
  service.sin_port = 0; // Let OS choose port
  
  if (bind(FListenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR)
  {
    closesocket(FListenSocket);
    FListenSocket = INVALID_SOCKET;
    return false;
  }
  
  // Get the assigned port
  int nameLen = sizeof(service);
  if (getsockname(FListenSocket, (SOCKADDR*)&service, &nameLen) == SOCKET_ERROR)
  {
    closesocket(FListenSocket);
    FListenSocket = INVALID_SOCKET;
    return false;
  }
  
  FLocalPort = ntohs(service.sin_port);
  
  // Listen for connections
  if (listen(FListenSocket, 1) == SOCKET_ERROR)
  {
    closesocket(FListenSocket);
    FListenSocket = INVALID_SOCKET;
    return false;
  }
  
  // Accept one connection (blocking)
  FClientSocket = accept(FListenSocket, NULL, NULL);
  if (FClientSocket == INVALID_SOCKET)
  {
    closesocket(FListenSocket);
    FListenSocket = INVALID_SOCKET;
    return false;
  }
  
  return true;
}
//---------------------------------------------------------------------------
bool __fastcall TWebSocketProxyServer::ConnectWebSocket()
{
  // Parse URL for WebSocket connection
  UnicodeString Host = FUrl;
  UnicodeString Path = L"/";
  
  int SlashPos = Host.Pos(L"://");
  if (SlashPos > 0)
  {
    Host = Host.SubString(SlashPos + 3, Host.Length());
  }
  
  SlashPos = Host.Pos(L"/");
  if (SlashPos > 0)
  {
    Path = Host.SubString(SlashPos, Host.Length());
    Host = Host.SubString(1, SlashPos - 1);
  }
  
  // Extract port from host if present
  int Port = 80;
  int ColonPos = Host.Pos(L":");
  if (ColonPos > 0)
  {
    Port = StrToIntDef(Host.SubString(ColonPos + 1, Host.Length()), 80);
    Host = Host.SubString(1, ColonPos - 1);
  }
  
  // Create WebSocket request
  HINTERNET hRequest = WinHttpOpenRequest(
    FConnectionHandle,
    L"GET",
    (Path + L"ws_proxy?token=" + FToken).c_str(),
    NULL,
    WINHTTP_NO_REFERER,
    WINHTTP_DEFAULT_ACCEPT_TYPES,
    0);
    
  if (hRequest == NULL)
  {
    return false;
  }
  
  // Upgrade to WebSocket
  BOOL bResults = WinHttpSetOption(
    hRequest,
    WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET,
    NULL,
    0);
    
  if (!bResults)
  {
    WinHttpCloseHandle(hRequest);
    return false;
  }
  
  // Send request
  bResults = WinHttpSendRequest(
    hRequest,
    WINHTTP_NO_ADDITIONAL_HEADERS,
    0,
    WINHTTP_NO_REQUEST_DATA,
    0,
    0,
    0);
    
  if (!bResults)
  {
    WinHttpCloseHandle(hRequest);
    return false;
  }
  
  // Receive response
  bResults = WinHttpReceiveResponse(hRequest, NULL);
  if (!bResults)
  {
    WinHttpCloseHandle(hRequest);
    return false;
  }
  
  // Complete WebSocket upgrade
  FWebSocketHandle = WinHttpWebSocketCompleteUpgrade(hRequest, 0);
  WinHttpCloseHandle(hRequest);
  
  if (FWebSocketHandle == NULL)
  {
    return false;
  }
  
  return true;
}
//---------------------------------------------------------------------------
void __fastcall TWebSocketProxyServer::BridgeData()
{
  const int BUFFER_SIZE = 8192;
  std::unique_ptr<char[]> buffer(new char[BUFFER_SIZE]);
  
  // Set non-blocking mode for client socket
  u_long mode = 1;
  ioctlsocket(FClientSocket, FIONBIO, &mode);
  
  while (!FStopping)
  {
    // Check for data from TCP client
    int bytesRead = recv(FClientSocket, buffer.get(), BUFFER_SIZE, 0);
    if (bytesRead > 0)
    {
      // Forward to WebSocket
      DWORD dwError = WinHttpWebSocketSend(
        FWebSocketHandle,
        WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE,
        buffer.get(),
        bytesRead);
        
      if (dwError != ERROR_SUCCESS)
      {
        break;
      }
    }
    else if (bytesRead == 0)
    {
      // Connection closed
      break;
    }
    
    // Check for data from WebSocket
    BYTE wsBuffer[BUFFER_SIZE];
    DWORD dwBytesRead = 0;
    WINHTTP_WEB_SOCKET_BUFFER_TYPE eBufferType;
    
    DWORD dwError = WinHttpWebSocketReceive(
      FWebSocketHandle,
      wsBuffer,
      BUFFER_SIZE,
      &dwBytesRead,
      &eBufferType);
      
    if (dwError == ERROR_SUCCESS && dwBytesRead > 0)
    {
      // Forward to TCP client
      send(FClientSocket, (const char*)wsBuffer, dwBytesRead, 0);
    }
    else if (dwError != ERROR_WINHTTP_TIMEOUT)
    {
      // Error or connection closed
      if (dwError != ERROR_SUCCESS)
      {
        break;
      }
    }
    
    // Small delay to avoid busy waiting
    Sleep(1);
  }
}
//---------------------------------------------------------------------------
void __fastcall TWebSocketProxyServer::CleanupSockets()
{
  if (FClientSocket != INVALID_SOCKET)
  {
    closesocket(FClientSocket);
    FClientSocket = INVALID_SOCKET;
  }
  
  if (FListenSocket != INVALID_SOCKET)
  {
    closesocket(FListenSocket);
    FListenSocket = INVALID_SOCKET;
  }
}
//---------------------------------------------------------------------------
void __fastcall TWebSocketProxyServer::CleanupWinHttp()
{
  if (FWebSocketHandle != NULL)
  {
    WinHttpWebSocketClose(FWebSocketHandle, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, NULL, 0);
    WinHttpCloseHandle(FWebSocketHandle);
    FWebSocketHandle = NULL;
  }
  
  if (FConnectionHandle != NULL)
  {
    WinHttpCloseHandle(FConnectionHandle);
    FConnectionHandle = NULL;
  }
  
  if (FSessionHandle != NULL)
  {
    WinHttpCloseHandle(FSessionHandle);
    FSessionHandle = NULL;
  }
}
//---------------------------------------------------------------------------
DWORD WINAPI TWebSocketProxyServer::BridgeThreadProc(LPVOID lpParam)
{
  TWebSocketProxyServer * proxy = static_cast<TWebSocketProxyServer*>(lpParam);
  if (proxy != NULL)
  {
    proxy->BridgeData();
  }
  return 0;
}
//---------------------------------------------------------------------------
