#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

void InitWindow(int width, int height, const char *title) {
    printf("Init Window\n");
    printf("width: %i, height: %i, title: %s\n", width, height, title);
}

bool WindowShouldClose(void) {
    printf("Window Should Close\n");
    return false;
}

void BeginDrawing(void) {
    printf("Begin Drawing\n");
}

typedef struct Color {
    unsigned char r; // Color red value
    unsigned char g; // Color green value
    unsigned char b; // Color blue value
    unsigned char a; // Color alpha value
} Color;

void ClearBackground(Color color) {
    printf("Clear Background\n");
    printf("color.(r, g, b, a): (%d, %d, %d, %d)\n", color.r, color.g, color.b, color.a);
}

void DrawText(const char *text, int posX, int posY, int fontSize, Color color) {
    printf("Draw Text\n");
    printf("text: %s\n", text);
    printf("pos: (%d, %d)\n", posX, posY);
    printf("fontSize: %d\n", fontSize);
    printf("color.(r, g, b, a): (%d, %d, %d, %d)\n", color.r, color.g, color.b, color.a);
}

void EndDrawing(void) {
    printf("End Drawing\n");
}

void CloseWindow(void) {
    printf("Close Window\n");
}

typedef struct {
    int x, y;
    int z;
} TestStruct;

TestStruct test_struct(const TestStruct t1, const TestStruct t2) {
    // TestStruct result = {t1.x + t2.x, t1.y + t2.y};
    // return result;
    TestStruct result = {0};
    return result;
}
