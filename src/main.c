#include "maps.h"
#include "raylib.h"
#include "raymath.h"

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
                uint16_t *buffer;
                uint16_t width;
                uint16_t height;
            } data;
        } color;
    } maps;
    uint16_t *pixels;
    float *hbuffer;
    uint16_t width;
    uint16_t height;
    struct
    {
        double phi;
        uint16_t distance;
        Vector2 pos;
        double height;
        uint16_t scale_height;
        float horizon;
        Vector2 view[3];
    } state;
    double delta_time;
} voxel_space_t;

void main_loop (void *arg);
void update (voxel_space_t *voxel_space);
void prevent_underground (voxel_space_t *voxel_space, const Vector2 *offset);
void render (voxel_space_t *voxel_space);
double get_height (voxel_space_t *voxel_space, const Vector2 *map_pos, int z);

void init (voxel_space_t *voxel_space);
void clear (voxel_space_t *voxel_space);

int main (int argc, char const **argv)
{
    voxel_space_t voxel_space = { 0 };
    InitWindow (WIDTH, HEIGHT, "VoxelSpace");
    init (&voxel_space);
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg (main_loop, &voxel_space, 0, 1);
#else
    SetTargetFPS (60);
    while (!WindowShouldClose ())
    {
        main_loop (&voxel_space);
    }
#endif
    clear (&voxel_space);
    CloseWindow ();
    return 0;
}

void main_loop (void *arg)
{
    voxel_space_t *voxel_space = arg;
    voxel_space->delta_time = GetFrameTime ();
    update (voxel_space);
    memset (voxel_space->pixels,
        0,
        voxel_space->height * voxel_space->width * sizeof (uint16_t));
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
        // DrawTexturePro (voxel_space->maps.color.tex,
        //     (Rectangle){ 0,
        //         0,
        //         voxel_space->maps.color.tex.width,
        //         voxel_space->maps.color.tex.height },
        //     (Rectangle){ 0, 0, GetScreenWidth () / 4, GetScreenHeight () / 4
        //     }, (Vector2){ 0, 0 }, 0, WHITE);
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

void update (voxel_space_t *voxel_space)
{
#define ROTATE_SPEED 1
#define LOOK_SPEED 20
#define MOVE_SPEED 10
    double lerp_amount = 1.0 - pow (0.01, voxel_space->delta_time);
    if (IsKeyDown (KEY_LEFT))
    {
        voxel_space->state.phi = Lerp (voxel_space->state.phi,
            voxel_space->state.phi + ROTATE_SPEED,
            lerp_amount);
    }
    if (IsKeyDown (KEY_RIGHT))
    {
        voxel_space->state.phi = Lerp (voxel_space->state.phi,
            voxel_space->state.phi - ROTATE_SPEED,
            lerp_amount);
    }
    if (IsKeyDown (KEY_W))
    {
        voxel_space->state.horizon = Lerp (voxel_space->state.horizon,
            voxel_space->state.horizon + LOOK_SPEED,
            lerp_amount);
        if (voxel_space->state.horizon > voxel_space->height)
        {
            voxel_space->state.horizon = voxel_space->height - 1;
        }
        TraceLog (LOG_INFO, "horizon: %d", voxel_space->state.horizon);
    }
    if (IsKeyDown (KEY_S))
    {
        voxel_space->state.horizon = Lerp (voxel_space->state.horizon,
            voxel_space->state.horizon - LOOK_SPEED,
            lerp_amount);
        if (voxel_space->state.horizon < 0)
        {
            voxel_space->state.horizon = 0;
        }
    }
    if (IsKeyDown (KEY_R))
    {
        clear (voxel_space);
        init (voxel_space);
    }
    {
        Vector2 offset
            = { -sin (voxel_space->state.phi), -cos (voxel_space->state.phi) };
        if (IsKeyDown (KEY_UP))
        {
            voxel_space->state.pos = Vector2Lerp (voxel_space->state.pos,
                Vector2Add (voxel_space->state.pos,
                    Vector2Multiply (
                        offset, (Vector2){ MOVE_SPEED, MOVE_SPEED })),
                lerp_amount);
        }
        if (IsKeyDown (KEY_DOWN))
        {
            voxel_space->state.pos = Vector2Lerp (voxel_space->state.pos,
                Vector2Subtract (voxel_space->state.pos,
                    Vector2Multiply (
                        offset, (Vector2){ MOVE_SPEED, MOVE_SPEED })),
                lerp_amount);
        }
        if (IsKeyDown (KEY_Q))
        {
            voxel_space->state.height = Lerp (voxel_space->state.height,
                voxel_space->state.height - MOVE_SPEED,
                lerp_amount);
        }
        if (IsKeyDown (KEY_E))
        {
            voxel_space->state.height = Lerp (voxel_space->state.height,
                voxel_space->state.height + MOVE_SPEED,
                lerp_amount);
        }
        prevent_underground (voxel_space, &offset);
    }
}

void prevent_underground (voxel_space_t *voxel_space, const Vector2 *offset)
{
    const float map_height
        = voxel_space->maps.height.data
              .buffer[((int)(voxel_space->state.pos.y + (offset->y))
                          + (voxel_space->maps.height.data.height * 2))
                          % voxel_space->maps.height.data.height
                          * voxel_space->maps.height.data.width
                      + ((int)(voxel_space->state.pos.x + (offset->x))
                            + (voxel_space->maps.height.data.width * 2))
                            % voxel_space->maps.height.data.width];
    const uint8_t DISTANCE_TO_GROUND = 2;
    if (map_height + DISTANCE_TO_GROUND >= voxel_space->state.height)
    {
        voxel_space->state.height = map_height + DISTANCE_TO_GROUND;
    }
}

void render (voxel_space_t *voxel_space)
{
    // precalculate viewing angle parameters
    const double sinPhi = sin (voxel_space->state.phi);
    const double cosPhi = cos (voxel_space->state.phi);

    for (int i = 0; i < voxel_space->width; i++)
    {
        voxel_space->hbuffer[i] = voxel_space->height;
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

        const double dx = (pright.x - pleft.x) / (double)voxel_space->width;
        const double dy = (pright.y - pleft.y) / (double)voxel_space->width;

        for (int i = 0; i < voxel_space->width; i++)
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
            if (((int)z == (voxel_space->state.distance - 1) || (int)z == 1)
                && (i == 0 || i == voxel_space->width - 1))
            {
                voxel_space->state.view[z == 1.0 ? 0 : (i == 0 ? 1 : 2)].x
                    = map_pos.x;
                voxel_space->state.view[z == 1.0 ? 0 : (i == 0 ? 1 : 2)].y
                    = map_pos.y;
            }
            uint16_t height_on_screen = get_height (voxel_space, &map_pos, z);
            if (voxel_space->hbuffer[i] >= height_on_screen)
            {
                if (height_on_screen < 0)
                {
                    height_on_screen = 0;
                }
                if (voxel_space->hbuffer[i] >= voxel_space->height)
                {
                    voxel_space->hbuffer[i] = voxel_space->height - 1;
                }
                const uint16_t color = voxel_space->maps.color.data.buffer[(
                    int)(map_pos.y * voxel_space->maps.color.data.width
                         + map_pos.x)];
                const int start = (int)voxel_space->hbuffer[i];
                for (int y = start; y > height_on_screen; y--)
                {
                    const int tw = (int)voxel_space->width;
                    const int index = y * tw + i;
                    voxel_space->pixels[index] = color;
                }
            }
            if (height_on_screen < (int)voxel_space->hbuffer[i])
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

void init (voxel_space_t *voxel_space)
{
    memset (voxel_space, 0, sizeof (voxel_space_t));
    voxel_space->state.phi = 0.0;
    voxel_space->state.distance = 1000;
    voxel_space->state.height = 50;
    voxel_space->state.scale_height = 200;
    voxel_space->width
        = ((TEXTURE_WIDTH < TEXTURE_HEIGHT ? TEXTURE_WIDTH : TEXTURE_HEIGHT)
              / 512.0)
        * 512.0;
    voxel_space->height = voxel_space->width;
    voxel_space->state.horizon = voxel_space->height / 2.0;
    {
        TraceLog (LOG_INFO, "Loading buffer...");
        Image image
            = { .data = calloc (voxel_space->width * voxel_space->height,
                    sizeof (uint16_t)),
                  .width = voxel_space->width,
                  .height = voxel_space->height,
                  .format = PIXELFORMAT_UNCOMPRESSED_R5G6B5,
                  .mipmaps = 1 };
        voxel_space->texture = LoadTextureFromImage (image);
        UnloadImage (image);
    }
    {
        TraceLog (LOG_INFO, "Loading height_map...");
        Image height_map = LoadImageFromMemory (
            ".png", map_1_height_png, map_1_height_png_size);
        ImageFormat (&height_map, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
        voxel_space->maps.height.tex = LoadTextureFromImage (height_map);
        voxel_space->maps.height.data.buffer
            = calloc (height_map.width * height_map.height, sizeof (uint8_t));
        memcpy (voxel_space->maps.height.data.buffer,
            height_map.data,
            height_map.width * height_map.height * sizeof (uint8_t));
        voxel_space->maps.height.data.width = height_map.width;
        voxel_space->maps.height.data.height = height_map.height;
        UnloadImage (height_map);
    }
    {
        TraceLog (LOG_INFO, "Loading color_map...");
        Image color_map = LoadImageFromMemory (
            ".png", map_1_color_png, map_1_color_png_size);
        ImageFormat (&color_map, PIXELFORMAT_UNCOMPRESSED_R5G6B5);
        voxel_space->maps.color.tex = LoadTextureFromImage (color_map);
        voxel_space->maps.color.data.buffer
            = calloc (color_map.width * color_map.height, sizeof (uint16_t));
        memcpy (voxel_space->maps.color.data.buffer,
            color_map.data,
            color_map.width * color_map.height * sizeof (uint16_t));

        voxel_space->maps.color.data.width = color_map.width;
        voxel_space->maps.color.data.height = color_map.height;

        UnloadImage (color_map);
    }
    voxel_space->pixels
        = calloc (voxel_space->width * voxel_space->height, sizeof (uint16_t));
    voxel_space->hbuffer = calloc (voxel_space->width, sizeof (float));
}

void clear (voxel_space_t *voxel_space)
{
    UnloadTexture (voxel_space->texture);
    UnloadTexture (voxel_space->maps.height.tex);
    UnloadTexture (voxel_space->maps.color.tex);

    free (voxel_space->maps.height.data.buffer);
    free (voxel_space->maps.color.data.buffer);

    free (voxel_space->pixels);
}
