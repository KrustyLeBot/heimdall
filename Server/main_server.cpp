#include "../include/raylib.h"
#include "../Heimdall/heimdall.h"

const int screenWidth = 200;
const int screenHeight = 200;

#define SERVER_PORT 8888

int main()
{
    InitWindow(screenWidth, screenHeight, "Heimdall Server");
    SetTargetFPS(60);

    Heimdall heimdall;
    heimdall.InitServer(SERVER_PORT);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(WHITE);

        heimdall.UpdateServer();

        EndDrawing();
    }

    heimdall.UnInitServer();

    CloseWindowRaylib();

    return 0;
}