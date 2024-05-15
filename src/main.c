#include "maps.h"
#include "raylib.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH (800.0)
#define HEIGHT (600.0)

#define TEXTURE_WIDTH (WIDTH / 2.0)
#define TEXTURE_HEIGHT (HEIGHT / 2.0)

typedef struct
{
    Texture2D texture;
    struct
    {
        struct
        {
            Texture tex;
            struct
            {
                uint8_t *buffer;
                uint16_t width;
                uint16_t height;
            } data;
        } height;
        struct
        {
            Texture tex;
            struct
            {
                uint32_t *buffer;
                uint16_t width;
                uint16_t height;
            } data;
        } color;
    } maps;
    uint16_t *pixels;
    float *hbuffer;
    struct
    {
        double phi;
        uint16_t distance;
        Vector2 pos;
        double height;
        uint16_t scale_height;
        uint16_t horizon;
    } state;
} voxel_space_t;

void main_loop (void *arg);
void render (voxel_space_t *voxel_space);
double get_height (voxel_space_t *voxel_space, const Vector2 *map_pos, int z);

int main (int argc, char const **argv)
{
    voxel_space_t voxel_space = { 0 };
    voxel_space.state.phi = 0.0;
    voxel_space.state.distance = 1000;
    voxel_space.state.height = 50;
    voxel_space.state.scale_height = 200;
    voxel_space.state.horizon = HEIGHT / 2.0;
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

        Image height_map = LoadImageFromMemory (
            ".png", map_1_height_png, map_1_height_png_size);
        voxel_space.maps.height.tex = LoadTextureFromImage (height_map);
        Image color_map = LoadImageFromMemory (
            ".png", map_1_color_png, map_1_color_png_size);
        voxel_space.maps.color.tex = LoadTextureFromImage (color_map);
        voxel_space.maps.height.data.buffer
            = calloc (height_map.width * height_map.height, sizeof (uint8_t));
        voxel_space.maps.color.data.buffer
            = calloc (color_map.width * color_map.height, sizeof (uint32_t));
        memcpy (voxel_space.maps.height.data.buffer,
            height_map.data,
            height_map.width * height_map.height * sizeof (uint8_t));
        memcpy (voxel_space.maps.color.data.buffer,
            color_map.data,
            color_map.width * color_map.height * sizeof (uint32_t));

        voxel_space.maps.height.data.width = height_map.width;
        voxel_space.maps.height.data.height = height_map.height;
        voxel_space.maps.color.data.width = color_map.width;
        voxel_space.maps.color.data.height = color_map.height;

        UnloadImage (height_map);
        UnloadImage (color_map);
    }
    voxel_space.pixels
        = calloc (TEXTURE_WIDTH * TEXTURE_HEIGHT, sizeof (uint16_t));
    voxel_space.hbuffer = calloc (TEXTURE_WIDTH, sizeof (float));
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg (main_loop, &voxel_space, 0, 1);
#else
    while (!WindowShouldClose ())
    {
        main_loop (&voxel_space);
    }
#endif
    UnloadTexture (voxel_space.texture);
    UnloadTexture (voxel_space.maps.height.tex);
    UnloadTexture (voxel_space.maps.color.tex);

    free (voxel_space.maps.height.data.buffer);
    free (voxel_space.maps.color.data.buffer);

    CloseWindow ();
    free (voxel_space.pixels);
    return 0;
}

void main_loop (void *arg)
{
    voxel_space_t *voxel_space = arg;
    memset (voxel_space->pixels,
        0,
        TEXTURE_HEIGHT * TEXTURE_WIDTH * sizeof (uint16_t));

    render (voxel_space);
    UpdateTexture (voxel_space->texture, voxel_space->pixels);
    BeginDrawing ();
    {
        DrawTexturePro (voxel_space->texture,
            (Rectangle){ 0,
                0,
                voxel_space->texture.width,
                voxel_space->texture.height },
            (Rectangle){ 0, 0, GetScreenWidth (), GetScreenHeight () },
            (Vector2){ 0, 0 },
            0,
            WHITE);
        DrawFPS (10, 10);
    }
    EndDrawing ();
#ifdef __EMSCRIPTEN__
    if (WindowShouldClose ())
    {
        emscripten_cancel_main_loop ();
    }
#endif
}

void render (voxel_space_t *voxel_space)
{
    // precalculate viewing angle parameters
    const double sinPhi = sin (voxel_space->state.phi);
    const double cosPhi = cos (voxel_space->state.phi);

    for (int i = 0; i < TEXTURE_WIDTH; i++)
    {
        voxel_space->hbuffer[i] = TEXTURE_HEIGHT;
    }

    double dz = 0.1;
    double z = 1.0;
    while (z < voxel_space->state.distance)
    {
        Vector2 pleft
            = { (-cosPhi * z - sinPhi * z) + voxel_space->state.pos.x,
                  (sinPhi * z - cosPhi * z) + voxel_space->state.pos.y };
        Vector2 pright
            = { (cosPhi * z - sinPhi * z) + voxel_space->state.pos.x,
                  (-sinPhi * z - cosPhi * z) + voxel_space->state.pos.y };

        const double dx = (pright.x - pleft.x) / (double)TEXTURE_WIDTH;
        const double dy = (pright.y - pleft.y) / (double)TEXTURE_HEIGHT;

        for (int i = 0; i < TEXTURE_WIDTH; i++)
        {
            const Vector2 map_pos = {
                ((uint32_t)(pleft.x)
                    + (voxel_space->maps.height.data.width
                        * ((uint32_t)((uint32_t)(pleft.x
                                                 / voxel_space->maps.height
                                                       .data.width))
                            + 1)))
                    % voxel_space->maps.height.data.width,
                ((uint32_t)(pleft.y)
                    + (voxel_space->maps.height.data.height
                        * ((uint32_t)((uint32_t)(pleft.y
                                                 / voxel_space->maps.height
                                                       .data.height))
                            + 1)))
                    % voxel_space->maps.height.data.height,
            };
            uint16_t height_on_screen = get_height (voxel_space, &map_pos, z);
            if (voxel_space->hbuffer[i] >= height_on_screen)
            {
                if (height_on_screen < 0)
                {
                    height_on_screen = 0;
                }
                const uint16_t color = voxel_space->maps.color.data.buffer[(
                    int)(map_pos.y * voxel_space->maps.color.data.width
                         + map_pos.x)];
                const uint8_t r
                    = ((double)((color >> 16) & 0xFF) / 255.0) * 0b11111;
                const uint8_t g
                    = ((double)((color >> 8) & 0xFF) / 255.0) * 0b111111;
                const uint8_t b
                    = ((double)((color >> 0) & 0xFF) / 255.0) * 0b11111;
                for (int y = (int)voxel_space->hbuffer[i];
                     y >= height_on_screen;
                     y--)
                {
                    voxel_space->pixels[y * (int)TEXTURE_WIDTH + i]
                        = (r << 11) | (g << 5) | b;
                }
            }
            if (height_on_screen < voxel_space->hbuffer[i])
            {
                voxel_space->hbuffer[i] = height_on_screen;
            }
            pleft.x = pleft.x + dx;
            pleft.y = pleft.y + dy;
        }
        z += dz;
        dz += 0.1;
    }
}

double get_height (voxel_space_t *voxel_space, const Vector2 *map_pos, int z)
{
    return (voxel_space->state.height
               - voxel_space->maps.height.data
                     .buffer[(int)map_pos->y
                                 * voxel_space->maps.height.data.width
                             + (int)map_pos->x])
             / z * voxel_space->state.scale_height
         + voxel_space->state.horizon;
}
