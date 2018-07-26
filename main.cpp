#include <thread>
#include <chrono>

#include "HTTPRequestHandler.hpp"

int
main()
{
   curl_global_init( CURL_GLOBAL_ALL );

   HTTPRequestHandler reqHandler;
   std::thread t1(&HTTPRequestHandler::run, &reqHandler);

   while(true)
   {
       for (int i=0;i<100;++i)
       {
           reqHandler.sendHttpRequest();
       }
       std::this_thread::sleep_for(std::chrono::seconds(1));
   }

   t1.join();

   return 0;
}
