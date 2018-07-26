#include "HTTPRequestHandler.hpp"
#include <iostream>
#include <sstream>
#include <thread>
#include <algorithm>

int MAX_QUEUE_CAPACITY = 50;
int MAX_SEND_AT_ONCE = 30;

namespace {

timeval
curlTimeoutToTimeval(long curlTimeoutMs, long maxMs = 1000)
{
  // default to 1 second in case curl_multi_timeout() didn't work and
  // curlTimeoutMs is negative
  timeval timeout = {1, 0};

  if (curlTimeoutMs >= 0)
  {
    // don't wait longer than max
    if (curlTimeoutMs > maxMs)
    {
      curlTimeoutMs = maxMs;
    }

    timeout.tv_sec = curlTimeoutMs / 1000;
    timeout.tv_usec = (curlTimeoutMs % 1000) * 1000;
  }

  return timeout;
}

}

HTTPRequestHandler::HTTPRequestHandler()
{
  m_CurlMultiHandle = curl_multi_init();

  curl_multi_setopt(m_CurlMultiHandle, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
  curl_multi_setopt(m_CurlMultiHandle, CURLMOPT_MAX_TOTAL_CONNECTIONS, 15);
  curl_multi_perform(m_CurlMultiHandle, &m_StillRunning);
}

void HTTPRequestHandler::performRequest()
{
  CURL* handle = curl_easy_init();

  curl_easy_setopt(handle, CURLOPT_TIMEOUT, 10L);
  curl_easy_setopt(handle, CURLOPT_DNS_CACHE_TIMEOUT, 60L);
  curl_easy_setopt(handle, CURLOPT_URL, "https://my-url/api/");
  //curl_easy_setopt(handle, CURLOPT_URL, "https://1.1.1.1/api/");
  curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);  // follow http 30x redirects
  curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, ""); // accept all compression algorithms we know

  curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(handle, CURLOPT_PIPEWAIT, 1L);
  curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
  curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(handle, CURLOPT_HTTPGET, 1);

  std::lock_guard<std::mutex> lock(m_Mutex);
  curl_multi_add_handle(m_CurlMultiHandle, handle);
  m_Requests.push_back(handle);
}

bool HTTPRequestHandler::sendHttpRequest()
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  if (m_requestsWaiting > MAX_QUEUE_CAPACITY) /// max capacity reached
  {
    return false;
  }

  m_requestsWaiting++;

  return true;
}



void HTTPRequestHandler::run()
{
  while (true)
  {
    size_t canSendCount = getCountCanSend(MAX_SEND_AT_ONCE);
    if (canSendCount > 0)
    {
      performRequests(popWaitingRequests(canSendCount));
    }

    fd_set fdRead;
    fd_set fdWrite;
    fd_set fdExcep;

    FD_ZERO(&fdRead);
    FD_ZERO(&fdWrite);
    FD_ZERO(&fdExcep);

    // set a suitable timeout to play around with
    long curlTimeout = -1;
    curl_multi_timeout(m_CurlMultiHandle, &curlTimeout);

    const long maxTimeoutMs = 1000;
    timeval timeout = curlTimeoutToTimeval(curlTimeout, maxTimeoutMs);

    // get file descriptors from the transfers
    int maxfd = -1;
    {
      CURLMcode code = curl_multi_fdset(m_CurlMultiHandle, &fdRead, &fdWrite, &fdExcep, &maxfd);
      if (CURLM_OK != code)
      {
        continue;
      }
    }

    int rc = 0;
    if (-1 == maxfd)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    else
    {
      rc = select(maxfd + 1, &fdRead, &fdWrite, &fdExcep, &timeout);
    }

    if (-1 != rc)
    {
      curl_multi_perform(m_CurlMultiHandle, &m_StillRunning);

      CURLMsg *msg = nullptr;
      do
      {
        int numMessages = 0;
        msg = curl_multi_info_read(m_CurlMultiHandle, &numMessages);

        if (msg && (CURLMSG_DONE == msg->msg))
        {

          {
            // remove finished request
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Requests.erase( 
                    std::remove(m_Requests.begin(), m_Requests.end(), msg->easy_handle)
                    , m_Requests.end());
          }

          curl_multi_remove_handle(m_CurlMultiHandle, msg->easy_handle);
          curl_easy_cleanup(msg->easy_handle);
          msg->easy_handle = nullptr;
        }
      } while (msg);
    }
    else
    {
      // select error
    }
  };
}

int
HTTPRequestHandler::popWaitingRequests(size_t count)
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  int result;
  if ( m_requestsWaiting > count) 
  {
      m_requestsWaiting -= count;
      result = count;
  }
  else {
      result = m_requestsWaiting;
      m_requestsWaiting = 0;
  }

  return result;
}

void HTTPRequestHandler::performRequests(int requests)
{
  if (requests == 0) 
  {
      return;
  }

  std::cout << "Sending " << requests<<"\n";

  for (int i=0;i<requests;++i) 
  {
    performRequest();
  }
}

size_t
HTTPRequestHandler::getCountCanSend(const size_t maxCount)
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  if (m_Requests.size() >= maxCount)
  {
    return 0;
  }

  return maxCount - m_Requests.size();
}
