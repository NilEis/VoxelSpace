#include "raylib.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include <stdint.h>
#include <stdlib.h>

#define WIDTH 800
#define HEIGHT 600

#define TEXTURE_WIDTH 400
#define TEXTURE_HEIGHT 300

void main_loop (void *arg);

typedef struct
{
    Texture2D texture;
    uint16_t *pixels;
    int i;
} voxel_space_t;

int main (int argc, char const **argv)
{
    voxel_space_t voxel_space = { 0 };
    InitWindow (WIDTH, HEIGHT, "VoxelSpace");
    {
        Image image = { .data = calloc (
                            TEXTURE_WIDTH * TEXTURE_HEIGHT, sizeof (uint16_t)),
            .width = TEXTURE_WIDTH,
            .height = TEXTURE_HEIGHT,
            .format = PIXELFORMAT_UNCOMPRESSED_R5G6B5,
            .mipmaps = 1 };
        voxel_space.texture = LoadTextureFromImage (image);
        UnloadImage (image);
    }
    voxel_space.pixels
        = calloc (TEXTURE_WIDTH * TEXTURE_HEIGHT, sizeof (uint16_t));
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg (main_loop, &voxel_space, 0, 1);
#else
    while (!WindowShouldClose ())
    {
        main_loop (&voxel_space);
    }
#endif
    UnloadTexture (voxel_space.texture);
    CloseWindow ();
    free (voxel_space.pixels);
    return 0;
}

void main_loop (void *arg)
{
    voxel_space_t *voxel_space = arg;
    voxel_space->i++;
    for (int i = 0; i < TEXTURE_WIDTH; i++)
    {
        voxel_space
            ->pixels[(voxel_space->i % TEXTURE_HEIGHT) * TEXTURE_WIDTH + i]
            = i^voxel_space->i;
    }
    UpdateTexture (voxel_space->texture, voxel_space->pixels);
    BeginDrawing ();
    DrawTexturePro (voxel_space->texture,
        (Rectangle){
            0, 0, voxel_space->texture.width, voxel_space->texture.height },
        (Rectangle){ 0, 0, GetScreenWidth (), GetScreenHeight () },
        (Vector2){ 0, 0 },
        0,
        WHITE);
    EndDrawing ();
#ifdef __EMSCRIPTEN__
    if (WindowShouldClose ())
    {
        emscripten_cancel_main_loop ();
    }
#endif
}
