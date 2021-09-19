default:
	g++ -o main_client.exe Client/*.cpp Heimdall/*.cpp include/*.h -g -O2 -I ./include/ -L ./lib/ -lraylib -lopengl32 -lgdi32 -lwinmm -lws2_32 -lwinmm
	g++ -o main_server.exe Server/*.cpp Heimdall/*.cpp include/*.h -g -O2 -I ./include/ -L ./lib/ -lraylib -lopengl32 -lgdi32 -lwinmm -lws2_32 -lwinmm