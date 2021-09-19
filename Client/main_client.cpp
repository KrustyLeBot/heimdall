#include "../include/raylib.h"
#include "../Heimdall/heimdall.h"
#include "../Utils/utils.h"
#include <iostream>

const int screenWidth = 1280;
const int screenHeight = 720;

#define SERVER_PORT 8888
#define SERVER_ADDR "127.0.0.1"

int main()
{
    InitWindow(screenWidth, screenHeight, "Heimdall Client");
    SetTargetFPS(10);

    Heimdall heimdall;
    heimdall.InitClient(SERVER_ADDR, SERVER_PORT);

    ClientInfo info;
    info.color = 0;
    info.x = 1;
    info.y = 2;
    info.z = 3;

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(WHITE);

        heimdall.SendInfoToServer(info);
        info.color += 1;

        //draw local client info
        CustomDrawText(TextFormat("Client: color %d, x %d, y %d, z %d", info.color, info.x, info.y, info.z), 10, 20, 20, GREEN);


        //draw clients info from server
        int posX = 10;
        int posY = 40;
        for (const auto &clientInfo : heimdall.m_clientInfos)
        {
            CustomDrawText(TextFormat("Client: color %d, x %d, y %d, z %d", clientInfo.color, clientInfo.x, clientInfo.y, clientInfo.z), posX, posY, 20, RED);
            posY += 20;
        }

        EndDrawing();
    }

    heimdall.UnInitClient();

    CloseWindowRaylib();

    return 0;
}