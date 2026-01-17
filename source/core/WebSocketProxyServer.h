//---------------------------------------------------------------------------
#ifndef WebSocketProxyServerH
#define WebSocketProxyServerH
//---------------------------------------------------------------------------
#include <System.hpp>
#include <windows.h>
#include <winhttp.h>
//---------------------------------------------------------------------------
class TWebSocketProxyServer
{
public:
  __fastcall TWebSocketProxyServer(
    const UnicodeString & Url, 
    const UnicodeString & UserName, 
    const UnicodeString & Password);
  __fastcall ~TWebSocketProxyServer();
  
  bool __fastcall Start();
  void __fastcall Stop();
  int __fastcall GetLocalPort() const;
  bool __fastcall IsRunning() const { return FRunning; }

private:
  UnicodeString FUrl;
  UnicodeString FUserName;
  UnicodeString FPassword;
  UnicodeString FToken;
  
  SOCKET FListenSocket;
  SOCKET FClientSocket;
  HINTERNET FSessionHandle;
  HINTERNET FConnectionHandle;
  HINTERNET FWebSocketHandle;
  HANDLE FBridgeThread;
  DWORD FBridgeThreadId;
  int FLocalPort;
  bool FRunning;
  bool FStopping;
  
  bool __fastcall AuthenticateHttp();
  bool __fastcall StartTcpListener();
  bool __fastcall ConnectWebSocket();
  void __fastcall BridgeData();
  void __fastcall CleanupSockets();
  void __fastcall CleanupWinHttp();
  
  static DWORD WINAPI BridgeThreadProc(LPVOID lpParam);
};
//---------------------------------------------------------------------------
#endif
