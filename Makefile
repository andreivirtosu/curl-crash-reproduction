make:
	 g++ -g -std=c++11 -fpermissive -Wall main.cpp HTTPRequestHandler.cpp -lcurl -lpthread -lcares -o app
