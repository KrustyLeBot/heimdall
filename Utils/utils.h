#include "../include/raylib.h"
#include <string>

void CustomDrawText(std::string text, int posX, int posY, int fontSize, Color color)
{
    Vector2 position = {(float)posX, (float)posY};
    Vector2 origin = {(float)0, (float)0};
    int defaultFontSize = 10; // Default Font chars height in pixel
    int spacing = fontSize / defaultFontSize;
    DrawTextPro(GetFontDefault(), text.c_str(), position, origin, 0.0f, fontSize, spacing, color);
}