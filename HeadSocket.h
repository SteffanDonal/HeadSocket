/*/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

***** HeadSocket v0.1, created by Jan Pinter **** Minimalistic header only WebSocket server implementation in C++ *****
                    Sources: https://github.com/P-i-N/HeadSocket, contact: Pinter.Jan@gmail.com
                     PUBLIC DOMAIN - no warranty implied or offered, use this at your own risk

-----------------------------------------------------------------------------------------------------------------------

Usage:
- use this as a regular header file, but in EXACTLY one of your C++ files (ie. main.cpp) you must define
  HEADSOCKET_IMPLEMENTATION beforehand, like this:

    #define HEADSOCKET_IMPLEMENTATION
    #include <HeadSocket.h>

/*/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef __HEADSOCKET_H__
#define __HEADSOCKET_H__

#include <stdint.h>

#ifndef HEADSOCKET_PLATFORM_OVERRIDE
#ifdef _WIN32
#define HEADSOCKET_PLATFORM_WINDOWS
#elif __ANDROID__
#define HEADSOCKET_PLATFORM_ANDROID
#define HEADSOCKET_PLATFORM_NIX
#elif __APPLE__
#include "TargetConditionals.h"
#ifdef TARGET_OS_MAC
#define HEADSOCKET_PLATFORM_MAC
#endif
#elif __linux
#define HEADSOCKET_PLATFORM_NIX
#elif __unix
#define HEADSOCKET_PLATFORM_NIX
#elif __posix
#define HEADSOCKET_PLATFORM_NIX
#else
#error Unsupported platform!
#endif
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define HEADSOCKET_LOCK_SUFFIX(var, suffix) std::lock_guard<decltype(var)> __scopeLock##suffix(var);
#define HEADSOCKET_LOCK_SUFFIX2(var, suffix) HEADSOCKET_LOCK_SUFFIX(var, suffix)
#define HEADSOCKET_LOCK(var) HEADSOCKET_LOCK_SUFFIX2(var, __LINE__)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace headsocket {

/* Forward declarations */
struct ConnectionParams;
class BaseTcpServer;
class BaseTcpClient;
class TcpClient;
class AsyncTcpClient;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace detail {
  
/* Forward declarations */
struct BaseTcpServerImpl;
struct BaseTcpClientImpl;
struct AsyncTcpClientImpl;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
struct Holder
{
  T *ptr;

  Holder(T *p): ptr(p) { }
  ~Holder() { if (ptr) delete ptr; }
  T *operator->() const { return ptr; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class Enumerator
{
public:
  explicit Enumerator(const BaseTcpServer *server)
    : _server(server)
  {
    _count = _server->acquireClients();
  }

  ~Enumerator()
  {
    _server->releaseClients();
  }

  const BaseTcpServer *getServer() const
  {
    return _server;
  }

  size_t getCount() const
  {
    return _count;
  }

  struct Iterator
  {
    Enumerator *enumerator;
    size_t index;

    Iterator(Enumerator *e, size_t i)
      : enumerator(e)
      , index(i)
    {

    }

    bool operator==(const Iterator &iter) const
    {
      return iter.index == index && iter.enumerator == enumerator;
    }

    bool operator!=(const Iterator &iter) const
    {
      return iter.index != index || iter.enumerator != enumerator;
    }

    Iterator &operator++()
    {
      ++index;
      return *this;
    }

    T *operator*() const
    {
      return reinterpret_cast<T *>(enumerator->getServer()->getClient(index));
    }
  };

  Iterator begin()
  {
    return Iterator(this, 0);
  }

  Iterator end()
  {
    return Iterator(this, _count);
  }

private:
  enum { T_IsBaseTcpClient = T::IsBaseTcpClient };

  const BaseTcpServer *_server;
  size_t _count;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool webSocketHandshake(ConnectionParams *params);

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum class Opcode
{
  Continuation = 0x00,
  Text = 0x01,
  Binary = 0x02,
  ConnectionClose = 0x08,
  Ping = 0x09,
  Pong = 0x0A
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct DataBlock
{
  Opcode opcode;
  size_t offset;
  size_t length = 0;
  bool isCompleted = false;

  DataBlock(Opcode op, size_t off)
    : opcode(op)
    , offset(off)
  {

  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class Connection
{
public:

private:
  struct ConnectionImpl *_p;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define HEADSOCKET_SERVER_CTOR(className, baseClassName) \
  className(int port): baseClassName(port) { }

class BaseTcpServer
{
public:
  virtual ~BaseTcpServer();

  void stop();
  bool isRunning() const;
  bool disconnect(BaseTcpClient *client);

protected:
  explicit BaseTcpServer(int port);

  virtual bool connectionHandshake(headsocket::ConnectionParams *params) = 0;
  virtual BaseTcpClient *clientAccept(headsocket::ConnectionParams *params) = 0;
  virtual void clientConnected(headsocket::BaseTcpClient *client) = 0;
  virtual void clientDisconnected(headsocket::BaseTcpClient *client) = 0;

  detail::Holder<detail::BaseTcpServerImpl> _p;

private:
  template <typename T> friend class detail::Enumerator;

  void removeAllDisconnected() const;

  size_t acquireClients() const;
  void releaseClients() const;

  BaseTcpClient *getClient(size_t index) const;
  size_t getNumClients() const;

  void acceptThread();
  void disconnectThread();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class TcpServer : public BaseTcpServer
{
public:
  typedef BaseTcpServer Base;
  typedef T Client;

  explicit TcpServer(int port) : Base(port)
  {

  }

  virtual ~TcpServer()
  {
    stop();
  }

  detail::Enumerator<T> enumerateClients() const
  {
    return detail::Enumerator<T>(this);
  }

protected:
  bool connectionHandshake(ConnectionParams *params) override
  {
    return true;
  }

  virtual void clientConnected(Client *client)
  {

  }

  virtual void clientDisconnected(Client *client)
  {

  }

private:
  enum { T_IsBaseTcpClient = T::IsBaseTcpClient };

  BaseTcpClient *clientAccept(ConnectionParams *params) override
  {
    T *newClient = new T(this, params);

    if (!newClient->isConnected())
    {
      delete newClient;
      newClient = nullptr;
    }

    return newClient;
  }

  void clientConnected(headsocket::BaseTcpClient *client) override
  {
    clientConnected(reinterpret_cast<T *>(client));
  }

  void clientDisconnected(headsocket::BaseTcpClient *client) override
  {
    clientDisconnected(reinterpret_cast<T *>(client));
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define HEADSOCKET_CLIENT_CTOR(className, baseClassName) \
  className(const char *address, int port): baseClassName(address, port) { } \
  className(headsocket::BaseTcpServer *server, headsocket::ConnectionParams *params): baseClassName(server, params) { }

class BaseTcpClient
{
public:
  enum { IsBaseTcpClient };

  static const size_t InvalidOperation = static_cast<size_t>(-1);

  virtual ~BaseTcpClient();

  bool disconnect();
  bool isConnected() const;

  BaseTcpServer *getServer() const;
  size_t getID() const;

protected:
  friend class BaseTcpServer;

  BaseTcpClient(const char *address, int port);
  BaseTcpClient(headsocket::BaseTcpServer *server, headsocket::ConnectionParams *params);

  detail::Holder<detail::BaseTcpClientImpl> _p;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class TcpClient : public BaseTcpClient
{
public:
  typedef BaseTcpClient Base;
  enum { IsTcpClient };

  TcpClient(const char *address, int port);
  TcpClient(headsocket::BaseTcpServer *server, headsocket::ConnectionParams *params);

  virtual ~TcpClient();

  virtual size_t write(const void *ptr, size_t length);
  virtual size_t read(void *ptr, size_t length);

  bool forceWrite(const void *ptr, size_t length);
  size_t readLine(void *ptr, size_t length);
  bool forceRead(void *ptr, size_t length);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class AsyncTcpClient : public BaseTcpClient
{
public:
  typedef BaseTcpClient Base;
  enum { IsAsyncTcpClient };

  AsyncTcpClient(const char *address, int port);
  AsyncTcpClient(headsocket::BaseTcpServer *server, headsocket::ConnectionParams *params);

  virtual ~AsyncTcpClient();

  void pushData(const void *ptr, size_t length);
  void pushData(const char *text);
  size_t peekData() const;
  size_t popData(void *ptr, size_t length);

protected:
  virtual void initAsyncThreads();
  virtual size_t asyncWriteHandler(uint8_t *ptr, size_t length);
  virtual size_t asyncReadHandler(uint8_t *ptr, size_t length);

  virtual bool asyncReceivedData(const headsocket::DataBlock &db, uint8_t *ptr, size_t length)
  {
    return false;
  }

  virtual void pushData(const void *ptr, size_t length, Opcode opcode);

  void killThreads();

  detail::Holder<detail::AsyncTcpClientImpl> _ap;

private:
  void writeThread();
  void readThread();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class WebSocketClient : public AsyncTcpClient
{
public:
  static const size_t FrameSizeLimit = 128 * 1024;

  typedef AsyncTcpClient Base;
  enum { IsWebSocketClient };

  WebSocketClient(const char *address, int port);
  WebSocketClient(headsocket::BaseTcpServer *server, headsocket::ConnectionParams *params);

  virtual ~WebSocketClient();

  size_t peekData(Opcode *opcode) const;

protected:
  size_t asyncWriteHandler(uint8_t *ptr, size_t length) override;
  size_t asyncReadHandler(uint8_t *ptr, size_t length) override;

private:
  struct FrameHeader
  {
    bool fin;
    Opcode opcode;
    bool masked;
    size_t payloadLength;
    uint32_t maskingKey;
  };

  size_t parseFrameHeader(uint8_t *ptr, size_t length, FrameHeader &header);
  size_t writeFrameHeader(uint8_t *ptr, size_t length, FrameHeader &header);

  size_t _payloadSize = 0;
  FrameHeader _currentHeader;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class WebSocketServer : public TcpServer<T>
{
public:
  typedef TcpServer<T> Base;

  WebSocketServer(int port)
    : Base(port)
  {

  }

  virtual ~WebSocketServer()
  {
    stop();
  }

protected:
  bool connectionHandshake(ConnectionParams *params) override
  {
    return detail::webSocketHandshake(params);
  }

private:
  enum { T_IsWebSocketClient = T::IsWebSocketClient };
};

}

#endif // __HEADSOCKET_H__

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef HEADSOCKET_IMPLEMENTATION
#ifndef __HEADSOCKET_H_IMPL__
#define __HEADSOCKET_H_IMPL__

#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include <memory>

#if defined(HEADSOCKET_PLATFORM_WINDOWS)
#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <Windows.h>
#include <ws2tcpip.h>
#elif defined(HEADSOCKET_PLATFORM_ANDROID) || defined(HEADSOCKET_PLATFORM_NIX)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <netdb.h>
#endif

namespace headsocket {

#if defined(HEADSOCKET_PLATFORM_WINDOWS)
namespace detail {

typedef SOCKET Socket;
static const int SocketError = SOCKET_ERROR;
static const SOCKET InvalidSocket = INVALID_SOCKET;

void CloseSocket(Socket s)
{
  closesocket(s);
}

}
#define HEADSOCKET_SPRINTF sprintf_s
#elif defined(HEADSOCKET_PLATFORM_ANDROID) || defined(HEADSOCKET_PLATFORM_NIX)
namespace detail {

typedef int Socket;
static const int SocketError = -1;
static const int InvalidSocket = -1;

void CloseSocket(Socket s)
{
  close(s);
}

}
#define HEADSOCKET_SPRINTF sprintf
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace detail {

class SHA1
{
public:
  typedef uint32_t Digest32[5];
  typedef uint8_t Digest8[20];

  inline static uint32_t rotateLeft(uint32_t value, size_t count)
  {
    return (value << count) ^ (value >> (32 - count));
  }

  SHA1()
  {
    _digest[0] = 0x67452301;
    _digest[1] = 0xEFCDAB89;
    _digest[2] = 0x98BADCFE;
    _digest[3] = 0x10325476;
    _digest[4] = 0xC3D2E1F0;
  }

  ~SHA1()
  {

  }

  void processByte(uint8_t octet)
  {
    _block[_blockByteIndex++] = octet;
    ++_byteCount;

    if (_blockByteIndex == 64)
    {
      _blockByteIndex = 0;
      processBlock();
    }
  }

  void processBlock(const void *start, const void *end)
  {
    const uint8_t *begin = static_cast<const uint8_t *>(start);

    while (begin != end)
      processByte(*begin++);
  }

  void processBytes(const void *data, size_t len)
  {
    processBlock(data, static_cast<const uint8_t *>(data) + len);
  }

  const uint32_t *getDigest(Digest32 digest)
  {
    size_t bitCount = _byteCount * 8;
    processByte(0x80);

    if (_blockByteIndex > 56)
    {
      while (_blockByteIndex != 0)
        processByte(0);

      while (_blockByteIndex < 56)
        processByte(0);
    }
    else
      while (_blockByteIndex < 56)
        processByte(0);

    processByte(0);
    processByte(0);
    processByte(0);
    processByte(0);

    for (int i = 24; i >= 0; i -= 8)
      processByte(static_cast<unsigned char>((bitCount >> i) & 0xFF));

    memcpy(digest, _digest, 5 * sizeof(uint32_t));
    return digest;
  }

  const uint8_t *getDigestBytes(Digest8 digest)
  {
    Digest32 d32;
    getDigest(d32);
    size_t s[] = { 24, 16, 8, 0 };

    for (size_t i = 0, j = 0; i < 20; ++i, j = i % 4)
      digest[i] = ((d32[i >> 2] >> s[j]) & 0xFF);

    return digest;
  }

private:
  void processBlock()
  {
    uint32_t w[80], s[] = { 24, 16, 8, 0 };

    for (size_t i = 0, j = 0; i < 64; ++i, j = i % 4)
      w[i / 4] = j ? (w[i / 4] | (_block[i] << s[j])) : (_block[i] << s[j]);

    for (size_t i = 16; i < 80; i++)
      w[i] = rotateLeft((w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]), 1);

    Digest32 dig = { _digest[0], _digest[1], _digest[2], _digest[3], _digest[4] };

    for (size_t f, k, i = 0; i < 80; ++i)
    {
      if (i < 20)
        f = (dig[1] & dig[2]) | (~dig[1] & dig[3]), k = 0x5A827999;
      else if (i < 40)
        f = dig[1] ^ dig[2] ^ dig[3], k = 0x6ED9EBA1;
      else if (i < 60)
        f = (dig[1] & dig[2]) | (dig[1] & dig[3]) | (dig[2] & dig[3]), k = 0x8F1BBCDC;
      else
        f = dig[1] ^ dig[2] ^ dig[3], k = 0xCA62C1D6;

      uint32_t temp = rotateLeft(dig[0], 5) + f + dig[4] + k + w[i];
      dig[4] = dig[3];
      dig[3] = dig[2];
      dig[2] = rotateLeft(dig[1], 30);
      dig[1] = dig[0];
      dig[0] = temp;
    }

    for (size_t i = 0; i < 5; ++i)
      _digest[i] += dig[i];
  }

  Digest32 _digest;
  uint8_t _block[64];
  size_t _blockByteIndex = 0;
  size_t _byteCount = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Encoding
{
  static size_t base64(const void *src, size_t srcLength, void *dst, size_t dstLength)
  {
    if (!src || !srcLength || !dst || !dstLength)
      return 0;

    static const char *encodingTable = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    static size_t modTable[] = { 0, 2, 1 }, result = 4 * ((srcLength + 2) / 3);

    if (result <= dstLength - 1)
    {
      const uint8_t *input = reinterpret_cast<const uint8_t *>(src);
      uint8_t *output = reinterpret_cast<uint8_t *>(dst);

      for (size_t i = 0, j = 0, triplet = 0; i < srcLength; triplet = 0)
      {
        for (size_t k = 0; k < 3; ++k)
          triplet = (triplet << 8) | (i < srcLength ? static_cast<uint8_t>(input[i++]) : 0);

        for (size_t k = 4; k--;)
          output[j++] = encodingTable[(triplet >> k * 6) & 0x3F];
      }

      for (size_t i = 0; i < modTable[srcLength % 3]; i++)
        output[result - 1 - i] = '=';

      output[result] = 0;
    }

    return result;
  }

  static size_t xor32(uint32_t key, void *ptr, size_t length)
  {
    uint8_t *data = reinterpret_cast<uint8_t *>(ptr);
    uint8_t *mask = reinterpret_cast<uint8_t *>(&key);

    for (size_t i = 0; i < length; ++i, ++data)
      *data = (*data) ^ mask[i % 4];

    return length;
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Endian
{
  static uint16_t swap16bits(uint16_t x)
  {
    return ((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8);
  }

  static uint32_t swap32bits(uint32_t x)
  {
    return ((x & 0x000000FF) << 24) | ((x & 0x0000FF00) << 8) | ((x & 0x00FF0000) >> 8) | ((x & 0xFF000000) >> 24);
  }

  static uint64_t swap64bits(uint64_t x)
  {
    return
      ((x & 0x00000000000000FFULL) << 56) | ((x & 0x000000000000FF00ULL) << 40) | ((x & 0x0000000000FF0000ULL) << 24) |
      ((x & 0x00000000FF000000ULL) << 8) | ((x & 0x000000FF00000000ULL) >> 8) | ((x & 0x0000FF0000000000ULL) >> 24) |
      ((x & 0x00FF000000000000ULL) >> 40) | ((x & 0xFF00000000000000ULL) >> 56);
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct CriticalSection
{
  mutable std::atomic_bool consumerLock;

  CriticalSection()
  {
    consumerLock = false;
  }
  void lock() const
  {
    while (consumerLock.exchange(true));
  }
  void unlock() const
  {
    consumerLock = false;
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T, typename M = CriticalSection>
struct LockableValue : M
{
  T value;
  T *operator->()
  {
    return &value;
  }
  const T *operator->() const
  {
    return &value;
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Semaphore
{
  mutable std::atomic_size_t count;
  mutable std::mutex mutex;
  mutable std::condition_variable cv;

  Semaphore()
  {
    count = 0;
  }

  void lock() const
  {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&]()->bool { return count > 0; });
    lock.release();
  }
  void unlock()
  {
    mutex.unlock();
  }
  void notify()
  {
    {
      std::lock_guard<std::mutex> lock(mutex);
      ++count;
    }
    cv.notify_one();
  }
  void consume() const
  {
    if (count)
      --count;
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct DataBlockBuffer
{
  std::vector<DataBlock> blocks;
  std::vector<uint8_t> buffer;

  DataBlockBuffer()
  {
    buffer.reserve(65536);
  }

  DataBlock &blockBegin(Opcode op)
  {
    blocks.emplace_back(op, buffer.size());
    return blocks.back();
  }

  DataBlock &blockEnd()
  {
    blocks.back().isCompleted = true;
    return blocks.back();
  }

  void blockRemove()
  {
    if (blocks.empty())
      return;

    buffer.resize(blocks.back().offset);
    blocks.pop_back();
  }

  void writeData(const void *ptr, size_t length)
  {
    if (!length)
      return;

    buffer.resize(buffer.size() + length);
    memcpy(buffer.data() + buffer.size() - length, reinterpret_cast<const char *>(ptr), length);
    blocks.back().length += length;
  }

  size_t readData(void *ptr, size_t length)
  {
    if (!ptr || blocks.empty() || !blocks.front().isCompleted)
      return 0;

    DataBlock &db = blocks.front();
    size_t result = db.length >= length ? length : db.length;

    if (result)
    {
      memcpy(ptr, buffer.data() + db.offset, result);
      buffer.erase(buffer.begin(), buffer.begin() + result);
    }

    if (!(db.length -= result))
      blocks.erase(blocks.begin());
    else
      blocks.front().opcode = Opcode::Continuation;

    if (result) for (auto &block : blocks) if (block.offset > db.offset)
          block.offset -= result;

    return result;
  }

  size_t peekData(Opcode *op = nullptr) const
  {
    if (blocks.empty() || !blocks.front().isCompleted)
      return 0;

    if (op)
      *op = blocks.front().opcode;

    return blocks.front().length;
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef HEADSOCKET_PLATFORM_WINDOWS
void setThreadName(const char *name)
{
#pragma pack(push,8)
  typedef struct tagTHREADNAME_INFO
  {
    DWORD dwType;
    LPCSTR szName;
    DWORD dwThreadID;
    DWORD dwFlags;
  } THREADNAME_INFO;
#pragma pack(pop)
  THREADNAME_INFO info = { 0x1000, name, static_cast<DWORD>(-1), 0 };

  __try
  {
    RaiseException(0x406D1388, 0, sizeof(info) / sizeof(ULONG_PTR), reinterpret_cast<ULONG_PTR *>(&info));
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {

  }
}
#else
void setThreadName(const char *name)
{

}
#endif

} // namespace detail;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct ConnectionParams
{
  detail::Socket clientSocket;
  sockaddr_in from;
  size_t id;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
size_t socketReadLine(detail::Socket socket, void *ptr, size_t length)
{
  if (!ptr || !length)
    return 0;

  size_t result = 0;

  while (result < length - 1)
  {
    char ch;
    int r = recv(socket, &ch, 1, 0);

    if (!r || r == detail::SocketError)
      return 0;

    if (r != 1 || ch == '\n')
      break;

    if (ch != '\r')
      reinterpret_cast<char *>(ptr)[result++] = ch;
  }

  reinterpret_cast<char *>(ptr)[result++] = 0;

  return result;
}

//---------------------------------------------------------------------------------------------------------------------
bool detail::webSocketHandshake(ConnectionParams *params)
{
  std::string key;
  char lineBuffer[256];

  while (true)
  {
    size_t result = socketReadLine(params->clientSocket, lineBuffer, 256);

    if (result <= 1)
      break;

    if (!memcmp(lineBuffer, "Sec-WebSocket-Key: ", 19))
      key = lineBuffer + 19;
  }

  if (key.empty())
    return false;

  key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

  detail::SHA1 sha;
  detail::SHA1::Digest8 digest;

  sha.processBytes(key.c_str(), key.length());
  sha.getDigestBytes(digest);
  detail::Encoding::base64(digest, 20, lineBuffer, 256);

  std::string response = "HTTP/1.1 101 Switching Protocols\nUpgrade: websocket\nConnection: Upgrade\nSec-WebSocket-Accept: ";
  response += lineBuffer;
  response += "\n\n";

  return send(params->clientSocket, response.c_str(), response.length(), 0) == response.length();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace detail {

struct BaseTcpClientRef
{
  size_t refCount = 0;
  BaseTcpClient *client = nullptr;

  BaseTcpClientRef(BaseTcpClient *c)
    : client(c)
  {
  
  }
};

struct BaseTcpServerImpl
{
  std::atomic_bool isRunning;
  std::atomic_bool disconnectThreadQuit;
  sockaddr_in local;
  detail::LockableValue<std::vector<BaseTcpClientRef>> connections;
  detail::Semaphore disconnectSemaphore;
  int port = 0;
  detail::Socket serverSocket = InvalidSocket;
  std::unique_ptr<std::thread> acceptThread;
  std::unique_ptr<std::thread> disconnectThread;
  size_t nextClientID = 1;

  BaseTcpServerImpl()
  {
    isRunning = false;
    disconnectThreadQuit = false;
  }
};

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
BaseTcpServer::BaseTcpServer(int port)
  : _p(new detail::BaseTcpServerImpl())
{
#ifdef HEADSOCKET_PLATFORM_WINDOWS
  WSADATA wsaData;
  WSAStartup(0x101, &wsaData);
#endif

  _p->local.sin_family = AF_INET;
  _p->local.sin_addr.s_addr = INADDR_ANY;
  _p->local.sin_port = htons(static_cast<unsigned short>(port));

  _p->serverSocket = socket(AF_INET, SOCK_STREAM, 0);

  if (bind(_p->serverSocket, reinterpret_cast<sockaddr *>(&_p->local), sizeof(_p->local)) != 0)
    return;

  if (listen(_p->serverSocket, 8))
    return;

  _p->isRunning = true;
  _p->port = port;
  _p->acceptThread = std::make_unique<std::thread>(std::bind(&BaseTcpServer::acceptThread, this));
  _p->disconnectThread = std::make_unique<std::thread>(std::bind(&BaseTcpServer::disconnectThread, this));
}

//---------------------------------------------------------------------------------------------------------------------
BaseTcpServer::~BaseTcpServer()
{
  stop();

#ifdef HEADSOCKET_PLATFORM_WINDOWS
  WSACleanup();
#endif
}

//---------------------------------------------------------------------------------------------------------------------
void BaseTcpServer::stop()
{
  if (_p->isRunning.exchange(false))
  {
    detail::CloseSocket(_p->serverSocket);

    {
      detail::Enumerator<BaseTcpClient> e(this);

      for (auto client : e)
        client->disconnect();
    }

    if (_p->acceptThread)
    {
      _p->acceptThread->join();
      _p->acceptThread = nullptr;
    }

    if (_p->disconnectThread)
    {
      _p->disconnectThreadQuit = true;
      _p->disconnectSemaphore.notify();
      _p->disconnectThread->join();
      _p->disconnectThread = nullptr;
    }
  }
}

//---------------------------------------------------------------------------------------------------------------------
bool BaseTcpServer::isRunning() const
{
  return _p->isRunning;
}

//---------------------------------------------------------------------------------------------------------------------
bool BaseTcpServer::disconnect(BaseTcpClient *client)
{
  bool found = false;

  if (client)
  {
    {
      HEADSOCKET_LOCK(_p->connections);
      for (size_t i = 0, S = _p->connections->size(); i < S; ++i)
        if (_p->connections->at(i).client == client)
        {
          found = true;
          break;
        }
    }

    if (found && !client->disconnect())
    {
      clientDisconnected(client);
      _p->disconnectSemaphore.notify();
    }
  }

  return found;
}

//---------------------------------------------------------------------------------------------------------------------
BaseTcpClient *BaseTcpServer::getClient(size_t index) const
{
  HEADSOCKET_LOCK(_p->connections);
  return index < _p->connections->size() ? _p->connections->at(index).client : nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
size_t BaseTcpServer::getNumClients() const
{
  HEADSOCKET_LOCK(_p->connections);
  return _p->connections->size();
}

//---------------------------------------------------------------------------------------------------------------------
size_t BaseTcpServer::acquireClients() const
{
  HEADSOCKET_LOCK(_p->connections);

  for (auto &clientRef : _p->connections.value)
    ++clientRef.refCount;

  return _p->connections->size();
}

//---------------------------------------------------------------------------------------------------------------------
void BaseTcpServer::releaseClients() const
{
  HEADSOCKET_LOCK(_p->connections);

  for (auto &clientRef : _p->connections.value)
    --clientRef.refCount;

  removeAllDisconnected();
}

//---------------------------------------------------------------------------------------------------------------------
void BaseTcpServer::removeAllDisconnected() const
{
  size_t i = 0;

  while (i < _p->connections->size())
  {
    detail::BaseTcpClientRef clientRef = _p->connections.value[i];

    if (!clientRef.client->isConnected() && clientRef.refCount == 0)
    {
      _p->connections->erase(_p->connections->begin() + i);
      delete clientRef.client;
    }
    else
      ++i;
  }
}

//---------------------------------------------------------------------------------------------------------------------
void BaseTcpServer::acceptThread()
{
  detail::setThreadName("BaseTcpServer::acceptThread");

  while (_p->isRunning)
  {
    ConnectionParams params;
    params.clientSocket = accept(_p->serverSocket, reinterpret_cast<struct sockaddr *>(&params.from), nullptr);
    params.id = _p->nextClientID++;

    if (!_p->nextClientID)
      ++_p->nextClientID;

    if (!_p->isRunning)
      break;

    if (params.clientSocket != detail::InvalidSocket)
    {
      BaseTcpClient *newClient = nullptr;
      bool failed = false;

      if (connectionHandshake(&params))
      {
        HEADSOCKET_LOCK(_p->connections);

        if (newClient = clientAccept(&params))
          _p->connections->push_back(newClient);
        else
          failed = true;
      }
      else
        failed = true;

      if (failed)
      {
        detail::CloseSocket(params.clientSocket);
        --_p->nextClientID;

        if (!_p->nextClientID)
          --_p->nextClientID;
      }
      else
        clientConnected(newClient);
    }
  }
}

//---------------------------------------------------------------------------------------------------------------------
void BaseTcpServer::disconnectThread()
{
  detail::setThreadName("BaseTcpServer::disconnectThread");

  while (!_p->disconnectThreadQuit)
  {
    {
      HEADSOCKET_LOCK(_p->disconnectSemaphore);
      HEADSOCKET_LOCK(_p->connections);

      removeAllDisconnected();
      _p->disconnectSemaphore.consume();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace detail {

struct BaseTcpClientImpl
{
  std::atomic_int refCount;
  std::atomic_bool isConnected;
  sockaddr_in from;
  size_t id = 0;
  BaseTcpServer *server = nullptr;
  Socket clientSocket = InvalidSocket;
  std::string address = "";
  int port = 0;

  BaseTcpClientImpl()
  {
    refCount = 0;
    isConnected = false;
  }
};

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
BaseTcpClient::BaseTcpClient(const char *address, int port)
  : _p(new detail::BaseTcpClientImpl())
{
  struct addrinfo *result = nullptr, *ptr = nullptr, hints;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  char buff[16];
  HEADSOCKET_SPRINTF(buff, "%d", port);

  if (getaddrinfo(address, buff, &hints, &result))
    return;

  for (ptr = result; ptr != nullptr; ptr = ptr->ai_next)
  {
    _p->clientSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

    if (_p->clientSocket == detail::InvalidSocket)
      return;

    if (connect(_p->clientSocket, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == detail::SocketError)
    {
      detail::CloseSocket(_p->clientSocket);
      _p->clientSocket = detail::InvalidSocket;
      continue;
    }

    break;
  }

  freeaddrinfo(result);

  if (_p->clientSocket == detail::InvalidSocket)
    return;

  _p->address = address;
  _p->port = port;
  _p->isConnected = true;
}

//---------------------------------------------------------------------------------------------------------------------
BaseTcpClient::BaseTcpClient(BaseTcpServer *server, ConnectionParams *params)
  : _p(new detail::BaseTcpClientImpl())
{
  _p->server = server;
  _p->clientSocket = params->clientSocket;
  _p->from = params->from;
  _p->id = params->id;
  _p->isConnected = true;
}

//---------------------------------------------------------------------------------------------------------------------
BaseTcpClient::~BaseTcpClient()
{
  disconnect();
}

//---------------------------------------------------------------------------------------------------------------------
bool BaseTcpClient::disconnect()
{
  bool wasConnected = _p->isConnected.exchange(false);

  if (wasConnected)
  {
    if (_p->clientSocket != detail::InvalidSocket)
    {
      detail::CloseSocket(_p->clientSocket);
      _p->clientSocket = detail::InvalidSocket;
    }

    if (_p->server)
      _p->server->disconnect(this);
  }

  return wasConnected;
}

//---------------------------------------------------------------------------------------------------------------------
bool BaseTcpClient::isConnected() const
{
  return _p->isConnected;
}

//---------------------------------------------------------------------------------------------------------------------
BaseTcpServer *BaseTcpClient::getServer() const
{
  return _p->server;
}

//---------------------------------------------------------------------------------------------------------------------
size_t BaseTcpClient::getID() const
{
  return _p->id;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
TcpClient::TcpClient(const char *address, int port)
  : Base(address, port)
{

}

//---------------------------------------------------------------------------------------------------------------------
TcpClient::TcpClient(BaseTcpServer *server, ConnectionParams *params)
  : Base(server, params)
{

}

//---------------------------------------------------------------------------------------------------------------------
TcpClient::~TcpClient()
{

}

//---------------------------------------------------------------------------------------------------------------------
size_t TcpClient::write(const void *ptr, size_t length)
{
  if (!ptr || !length)
    return 0;

  int result = send(_p->clientSocket, static_cast<const char *>(ptr), length, 0);

  if (!result || result == detail::SocketError)
    return 0;

  return static_cast<size_t>(result);
}

//---------------------------------------------------------------------------------------------------------------------
bool TcpClient::forceWrite(const void *ptr, size_t length)
{
  if (!ptr)
    return true;

  const char *chPtr = static_cast<const char *>(ptr);

  while (length)
  {
    int result = send(_p->clientSocket, chPtr, length, 0);

    if (!result || result == detail::SocketError)
      return false;

    length -= static_cast<size_t>(result);
    chPtr += result;
  }

  return true;
}

//---------------------------------------------------------------------------------------------------------------------
size_t TcpClient::read(void *ptr, size_t length)
{
  if (!ptr || !length)
    return 0;

  int result = recv(_p->clientSocket, static_cast<char *>(ptr), length, 0);

  if (!result || result == detail::SocketError)
    return 0;

  return static_cast<size_t>(result);
}

//---------------------------------------------------------------------------------------------------------------------
size_t TcpClient::readLine(void *ptr, size_t length)
{
  if (!ptr || !length)
    return 0;

  size_t result = 0;

  while (result < length - 1)
  {
    char ch;
    int r = recv(_p->clientSocket, &ch, 1, 0);

    if (!r || r == detail::SocketError)
      return 0;

    if (r != 1 || ch == '\n')
      break;

    if (ch != '\r')
      reinterpret_cast<char *>(ptr)[result++] = ch;
  }

  reinterpret_cast<char *>(ptr)[result++] = 0;

  return result;
}

//---------------------------------------------------------------------------------------------------------------------
bool TcpClient::forceRead(void *ptr, size_t length)
{
  if (!ptr)
    return true;

  char *chPtr = static_cast<char *>(ptr);

  while (length)
  {
    int result = recv(_p->clientSocket, chPtr, length, 0);

    if (!result || result == detail::SocketError)
      return false;

    length -= static_cast<size_t>(result);
    chPtr += result;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace detail {

struct AsyncTcpClientImpl
{
  detail::Semaphore writeSemaphore;
  detail::LockableValue<detail::DataBlockBuffer> writeBlocks;
  detail::LockableValue<detail::DataBlockBuffer> readBlocks;
  std::unique_ptr<std::thread> writeThread;
  std::unique_ptr<std::thread> readThread;
};

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
AsyncTcpClient::AsyncTcpClient(const char *address, int port)
  : Base(address, port)
  , _ap(new detail::AsyncTcpClientImpl())
{
  initAsyncThreads();
}

//---------------------------------------------------------------------------------------------------------------------
AsyncTcpClient::AsyncTcpClient(BaseTcpServer *server, ConnectionParams *params)
  : Base(server, params)
  , _ap(new detail::AsyncTcpClientImpl())
{
  initAsyncThreads();
}

//---------------------------------------------------------------------------------------------------------------------
AsyncTcpClient::~AsyncTcpClient()
{
  disconnect();

  _ap->writeSemaphore.notify();
  _ap->writeThread->join();
  _ap->readThread->join();
}

//---------------------------------------------------------------------------------------------------------------------
void AsyncTcpClient::pushData(const void *ptr, size_t length, Opcode opcode)
{
  if (!ptr)
    return;

  {
    HEADSOCKET_LOCK(_ap->writeBlocks);
    _ap->writeBlocks->blockBegin(opcode);
    _ap->writeBlocks->writeData(ptr, length);
    _ap->writeBlocks->blockEnd();
  }

  _ap->writeSemaphore.notify();
}

//---------------------------------------------------------------------------------------------------------------------
void AsyncTcpClient::pushData(const void *ptr, size_t length)
{
  pushData(ptr, length, Opcode::Binary);
}

//---------------------------------------------------------------------------------------------------------------------
void AsyncTcpClient::pushData(const char *text)
{
  pushData(text, text ? strlen(text) : 0, Opcode::Text);
}

//---------------------------------------------------------------------------------------------------------------------
size_t AsyncTcpClient::peekData() const
{
  HEADSOCKET_LOCK(_ap->readBlocks);
  return _ap->readBlocks->peekData(nullptr);
}

//---------------------------------------------------------------------------------------------------------------------
size_t AsyncTcpClient::popData(void *ptr, size_t length)
{
  if (!ptr)
    return InvalidOperation;

  if (!length)
    return 0;

  HEADSOCKET_LOCK(_ap->readBlocks);
  return _ap->readBlocks->readData(ptr, length);
}

//---------------------------------------------------------------------------------------------------------------------
void AsyncTcpClient::initAsyncThreads()
{
  _ap->writeThread = std::make_unique<std::thread>(std::bind(&AsyncTcpClient::writeThread, this));
  _ap->readThread = std::make_unique<std::thread>(std::bind(&AsyncTcpClient::readThread, this));
}

//---------------------------------------------------------------------------------------------------------------------
void AsyncTcpClient::writeThread()
{
  detail::setThreadName("AsyncTcpClient::writeThread");

  std::vector<uint8_t> buffer(1024 * 1024);

  while (_p->isConnected)
  {
    size_t written = 0;
    {
      HEADSOCKET_LOCK(_ap->writeSemaphore);

      if (!_p->isConnected)
        break;

      written = asyncWriteHandler(buffer.data(), buffer.size());
    }

    if (written == InvalidOperation)
      break;

    if (!written)
      buffer.resize(buffer.size() * 2);
    else
    {
      const char *cursor = reinterpret_cast<const char *>(buffer.data());

      while (written)
      {
        int result = send(_p->clientSocket, cursor, written, 0);

        if (!result || result == detail::SocketError)
          break;

        cursor += result;
        written -= static_cast<size_t>(result);
      }
    }
  }

  killThreads();
}

//---------------------------------------------------------------------------------------------------------------------
size_t AsyncTcpClient::asyncWriteHandler(uint8_t *ptr, size_t length)
{
  HEADSOCKET_LOCK(_ap->writeBlocks);
  // TODO
  return length;
}

//---------------------------------------------------------------------------------------------------------------------
size_t AsyncTcpClient::asyncReadHandler(uint8_t *ptr, size_t length)
{
  HEADSOCKET_LOCK(_ap->readBlocks);
  // TODO
  return length;
}

//---------------------------------------------------------------------------------------------------------------------
void AsyncTcpClient::readThread()
{
  detail::setThreadName("AsyncTcpClient::readThread");

  std::vector<uint8_t> buffer(1024 * 1024);
  size_t bufferBytes = 0, consumed = 0;

  while (_p->isConnected)
  {
    while (true)
    {
      int result = static_cast<size_t>(bufferBytes);

      if (!result || !consumed)
      {
        result = recv(_p->clientSocket, reinterpret_cast<char *>(buffer.data() + bufferBytes), buffer.size() - bufferBytes, 0);

        if (!result || result == detail::SocketError)
        {
          consumed = InvalidOperation;
          break;
        }

        bufferBytes += static_cast<size_t>(result);
      }

      consumed = asyncReadHandler(buffer.data(), bufferBytes);

      if (!consumed)
      {
        if (bufferBytes == buffer.size())
          buffer.resize(buffer.size() * 2);
      }
      else
        break;
    }

    if (consumed == InvalidOperation)
      break;

    bufferBytes -= consumed;

    if (bufferBytes)
      memcpy(buffer.data(), buffer.data() + consumed, bufferBytes);
  }

  killThreads();
}

//---------------------------------------------------------------------------------------------------------------------
void AsyncTcpClient::killThreads()
{
  if (std::this_thread::get_id() == _ap->readThread->get_id())
    _ap->writeSemaphore.notify();

  disconnect();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
WebSocketClient::WebSocketClient(const char *address, int port)
  : Base(address, port)
{

}

//---------------------------------------------------------------------------------------------------------------------
WebSocketClient::WebSocketClient(BaseTcpServer *server, ConnectionParams *params)
  : Base(server, params)
{

}

//---------------------------------------------------------------------------------------------------------------------
WebSocketClient::~WebSocketClient()
{

}

//---------------------------------------------------------------------------------------------------------------------
size_t WebSocketClient::peekData(Opcode *opcode) const
{
  HEADSOCKET_LOCK(_ap->readBlocks);
  return _ap->readBlocks->peekData(opcode);
}

//---------------------------------------------------------------------------------------------------------------------
size_t WebSocketClient::asyncWriteHandler(uint8_t *ptr, size_t length)
{
  uint8_t *cursor = ptr;
  HEADSOCKET_LOCK(_ap->writeBlocks);

  while (length >= 16)
  {
    Opcode op;
    size_t toWrite = _ap->writeBlocks->peekData(&op);
    size_t toConsume = (length - 15) > FrameSizeLimit ? FrameSizeLimit : (length - 15);
    toConsume = toConsume > toWrite ? toWrite : toConsume;

    FrameHeader header;
    header.fin = (toWrite - toConsume) == 0;
    header.opcode = op;
    header.masked = false;
    header.payloadLength = toConsume;

    size_t headerSize = writeFrameHeader(cursor, length, header);
    cursor += headerSize;
    length -= headerSize;
    _ap->writeBlocks->readData(cursor, toConsume);
    cursor += toConsume;
    length -= toConsume;

    if (header.fin)
      _ap->writeSemaphore.consume();

    if (!_ap->writeBlocks->peekData(&op))
      break;
  }

  return cursor - ptr;
}

//---------------------------------------------------------------------------------------------------------------------
size_t WebSocketClient::asyncReadHandler(uint8_t *ptr, size_t length)
{
  uint8_t *cursor = ptr;
  HEADSOCKET_LOCK(_ap->readBlocks);

  if (!_payloadSize)
  {
    Opcode prevOpcode = _currentHeader.opcode;
    size_t headerSize = parseFrameHeader(cursor, length, _currentHeader);

    if (!headerSize)
      return 0;
    else if (headerSize == InvalidOperation)
      return InvalidOperation;

    _payloadSize = _currentHeader.payloadLength;
    cursor += headerSize;
    length -= headerSize;

    if (_currentHeader.opcode != Opcode::Continuation)
      _ap->readBlocks->blockBegin(_currentHeader.opcode);
    else
      _currentHeader.opcode = prevOpcode;
  }

  if (_payloadSize)
  {
    size_t toConsume = length >= _payloadSize ? _payloadSize : length;

    if (toConsume)
    {
      _ap->readBlocks->writeData(cursor, toConsume);
      _payloadSize -= toConsume;
      cursor += toConsume;
      length -= toConsume;
    }
  }

  if (!_payloadSize)
  {
    if (_currentHeader.masked)
    {
      DataBlock &db = _ap->readBlocks->blocks.back();
      size_t len = _currentHeader.payloadLength;
      detail::Encoding::xor32(_currentHeader.maskingKey, _ap->readBlocks->buffer.data() + _ap->readBlocks->buffer.size() - len, len);
    }

    if (_currentHeader.fin)
    {
      DataBlock &db = _ap->readBlocks->blocks.back();

      switch (_currentHeader.opcode)
      {
        case Opcode::Ping:
          pushData(_ap->readBlocks->buffer.data() + db.offset, db.length, Opcode::Pong);
          break;

        case Opcode::Text:
          _ap->readBlocks->buffer.push_back(0);
          ++db.length;
          break;

        case Opcode::ConnectionClose:
          killThreads();
          break;
      }

      if (_currentHeader.opcode == Opcode::Text || _currentHeader.opcode == Opcode::Binary)
      {
        _ap->readBlocks->blockEnd();

        if (asyncReceivedData(db, _ap->readBlocks->buffer.data() + db.offset, db.length))
          _ap->readBlocks->blockRemove();
      }
    }
  }

  return cursor - ptr;
}

//---------------------------------------------------------------------------------------------------------------------
#define HAVE_ENOUGH_BYTES(num) if (length < num) return 0; else length -= num;
size_t WebSocketClient::parseFrameHeader(uint8_t *ptr, size_t length, FrameHeader &header)
{
  uint8_t *cursor = ptr;
  HAVE_ENOUGH_BYTES(2);
  header.fin = ((*cursor) & 0x80) != 0;
  header.opcode = static_cast<Opcode>((*cursor++) & 0x0F);

  header.masked = ((*cursor) & 0x80) != 0;
  uint8_t byte = (*cursor++) & 0x7F;

  if (byte < 126)
    header.payloadLength = byte;
  else if (byte == 126)
  {
    HAVE_ENOUGH_BYTES(2);
    header.payloadLength = detail::Endian::swap16bits(*(reinterpret_cast<uint16_t *>(cursor)));
    cursor += 2;
  }
  else if (byte == 127)
  {
    HAVE_ENOUGH_BYTES(8);
    uint64_t length64 = detail::Endian::swap64bits(*(reinterpret_cast<uint64_t *>(cursor))) & 0x7FFFFFFFFFFFFFFFULL;
    header.payloadLength = static_cast<size_t>(length64);
    cursor += 8;
  }

  if (header.masked)
  {
    HAVE_ENOUGH_BYTES(4);
    header.maskingKey = *(reinterpret_cast<uint32_t *>(cursor));
    cursor += 4;
  }

  return cursor - ptr;
}

//---------------------------------------------------------------------------------------------------------------------
size_t WebSocketClient::writeFrameHeader(uint8_t *ptr, size_t length, FrameHeader &header)
{
  uint8_t *cursor = ptr;
  HAVE_ENOUGH_BYTES(2);
  *cursor = header.fin ? 0x80 : 0x00;
  *cursor++ |= static_cast<uint8_t>(header.opcode);

  *cursor = header.masked ? 0x80 : 0x00;

  if (header.payloadLength < 126)
    *cursor++ |= static_cast<uint8_t>(header.payloadLength);
  else if (header.payloadLength < 65536)
  {
    HAVE_ENOUGH_BYTES(2);
    *cursor++ |= 126;
    *reinterpret_cast<uint16_t *>(cursor) = detail::Endian::swap16bits(static_cast<uint16_t>(header.payloadLength));
    cursor += 2;
  }
  else
  {
    HAVE_ENOUGH_BYTES(8);
    *cursor++ |= 127;
    *reinterpret_cast<uint64_t *>(cursor) = detail::Endian::swap64bits(static_cast<uint64_t>(header.payloadLength));
    cursor += 8;
  }

  if (header.masked)
  {
    HAVE_ENOUGH_BYTES(4);
    *reinterpret_cast<uint32_t *>(cursor) = header.maskingKey;
    cursor += 4;
  }

  return cursor - ptr;
}
#undef HAVE_ENOUGH_BYTES

}
#endif
#endif
