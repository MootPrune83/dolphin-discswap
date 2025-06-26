#include "Core/HTTP/DiscSwapServer.h"

#include <SFML/Network.hpp>
#include <array>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>
#include <thread>

#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Common/FileUtil.h"
#include "Common/SocketContext.h"
#include "Core/Core.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/System.h"

namespace Core {
namespace {
Common::Flag s_running;
std::thread s_server_thread;

std::string UrlDecode(const std::string& in)
{
  std::string out;
  out.reserve(in.size());
  for (size_t i = 0; i < in.size(); ++i)
  {
    char c = in[i];
    if (c == '%' && i + 2 < in.size())
    {
      char hex[3] = {in[i + 1], in[i + 2], 0};
      out += static_cast<char>(std::strtol(hex, nullptr, 16));
      i += 2;
    }
    else if (c == '+')
    {
      out += ' ';
    }
    else
    {
      out += c;
    }
  }
  return out;
}

void SendResponse(sf::TcpSocket& sock, const std::string& status, const std::string& body)
{
  std::ostringstream out;
  out << "HTTP/1.1 " << status << "\r\n"
      << "Content-Type: text/plain\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n\r\n" << body;
  const std::string resp = out.str();
  const sf::Socket::Status send_status = sock.send(resp.c_str(), resp.size());
  if (send_status != sf::Socket::Status::Done)
  {
    ERROR_LOG_FMT(CORE, "Failed to send HTTP response: {}", static_cast<int>(send_status));
  }
}

void HandleRequest(sf::TcpSocket& sock)
{
  std::array<char, 2048> buffer{};
  std::size_t received = 0;
  if (sock.receive(buffer.data(), buffer.size() - 1, received) != sf::Socket::Status::Done)
  {
    SendResponse(sock, "400 Bad Request", "Bad Request");
    return;
  }
  buffer[received] = '\0';
  std::string request(buffer.data());

  auto line_end = request.find("\r\n");
  if (line_end == std::string::npos)
  {
    SendResponse(sock, "400 Bad Request", "Bad Request");
    return;
  }
  std::istringstream line(request.substr(0, line_end));
  std::string method, target;
  line >> method >> target;

  if (method != "GET" && method != "POST")
  {
    SendResponse(sock, "405 Method Not Allowed", "Method Not Allowed");
    return;
  }

  std::string path;
  const std::string prefix = "/swap?path=";
  if (target.rfind(prefix, 0) == 0)
    path = UrlDecode(target.substr(prefix.size()));

  if (path.empty())
  {
    SendResponse(sock, "400 Bad Request", "Bad Request");
    return;
  }

  if (!File::Exists(path))
  {
    SendResponse(sock, "404 Not Found", "File Not Found");
    return;
  }

  INFO_LOG_FMT(CORE, "Swapping disc to {} via API", path);
  QueueHostJob([path](Core::System& system) {
    RunOnCPUThread(system, [path, &system] {
      system.GetDVDInterface().ChangeDisc(Core::CPUThreadGuard{system}, path);
    }, true);
  });

  SendResponse(sock, "200 OK", "OK");
}

void ServerThread()
{
  Common::SetCurrentThreadName("DiscSwapServer");
  Common::SocketContext ctx;

  sf::TcpListener listener;
  if (listener.listen(8394, sf::IpAddress::LocalHost) != sf::Socket::Status::Done)
  {
    ERROR_LOG_FMT(CORE, "Failed to start disc swap server on port 8394");
    return;
  }

  listener.setBlocking(false);
  sf::TcpSocket socket;
  while (s_running.IsSet())
  {
    if (listener.accept(socket) == sf::Socket::Status::Done)
    {
      HandleRequest(socket);
      socket.disconnect();
    }
    else
    {
      Common::SleepCurrentThread(10);
    }
  }
}
} // namespace

void StartDiscSwapServer(System& system)
{
  if (s_running.IsSet())
    return;
  s_running.Set();
  s_server_thread = std::thread(ServerThread);
}

void StopDiscSwapServer()
{
  if (!s_running.IsSet())
    return;
  s_running.Clear();
  if (s_server_thread.joinable())
    s_server_thread.join();
}

} // namespace Core

