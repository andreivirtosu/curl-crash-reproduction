#ifndef __REQUEST_HANDLER_HPP__
#define __REQUEST_HANDLER_HPP__

#include <atomic>
#include <curl/curl.h>
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <mutex>

class HTTPRequestHandler
{
    public:
      HTTPRequestHandler();

      bool sendHttpRequest();

      void run();

    private:
      HTTPRequestHandler(const HTTPRequestHandler &) = delete;
      HTTPRequestHandler &operator=(const HTTPRequestHandler &) = delete;

      int popWaitingRequests(size_t count);
      void performRequest();

      void performRequests(int requests);

      size_t getCountCanSend(const size_t maxCount);

    private:
      mutable std::mutex m_Mutex;
      CURLM *m_CurlMultiHandle = nullptr;
      int32_t m_StillRunning;
      int m_requestsWaiting = 0;
      std::vector<CURL *> m_Requests;
};

#endif
