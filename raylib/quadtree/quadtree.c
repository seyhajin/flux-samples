/*******************************************************************************************
*
*   raylib: quadtree (adapted for HTML5 platform)
*
*   This example is prepared to compile for PLATFORM_WEB, PLATFORM_DESKTOP and PLATFORM_RPI
*   As you will notice, code structure is slightly diferent to the other examples...
*   To compile it for PLATFORM_WEB just uncomment #define PLATFORM_WEB at beginning
*
*   This example has been created using raylib 3.7 (www.raylib.com)
*   raylib is licensed under an unmodified zlib/libpng license (View raylib.h for details)
*
*   Copyright (c) 2015 Ramon Santamaria (@raysan5)
*   Copyright (c) 2021 Christophe TES (@seyhajin)
*
********************************************************************************************/

#include <stdio.h>
#include <math.h>

#include "raylib.h"
#include "raymath.h"

//#define PLATFORM_WEB

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

//----- Frustum

static Vector2 fl, fr; // left/right planes

//----- Camera

typedef struct camera_s {
    Vector2 pos;
    float fov;
} camera_t;

camera_t create_camera(float x, float y, float fov) {
    return (camera_t){
        .pos = (Vector2){x, y},
        .fov = fov
    };
}

int point_in_frustum_left(camera_t cam, float px, float py) {
    return (-(px - cam.pos.x) * (fl.y - cam.pos.y) + (py - cam.pos.y) * (fl.x - cam.pos.x) >= 0.0f);
}

int point_in_frustum_right(camera_t cam, float px, float py) {
    return (-(px - cam.pos.x) * (fr.y - cam.pos.y) + (py - cam.pos.y) * (fr.x - cam.pos.x) <= 0.0f);
}

//----- Quadtree

typedef enum quad_child_e {
    CHILD_TL, // top-left
    CHILD_BL, // bottom-left
    CHILD_BR, // bottom-right
    CHILD_TR, // top-right
    CHILD_COUNT
} quad_child_t;

typedef struct quadtree_s {
    Vector2 min;
    Vector2 max;
    struct quadtree_s *childs[CHILD_COUNT];
} quadtree_t;

quadtree_t *create_quadtree(float xmin, float ymin, float xmax, float ymax, int depth) {
    quadtree_t *qt = (quadtree_t*)MemAlloc(sizeof(quadtree_t));
    qt->min = (Vector2){xmin, ymin};
    qt->max = (Vector2){xmax, ymax};
    if (depth > 0) {
        float xavg = (xmin + xmax)*0.5f;
        float yavg = (ymin + ymax)*0.5f;
        depth--;
        qt->childs[CHILD_TL] = create_quadtree(xmin, ymin, xavg, yavg, depth);
        qt->childs[CHILD_BL] = create_quadtree(xmin, yavg, xavg, ymax, depth);
        qt->childs[CHILD_BR] = create_quadtree(xavg, yavg, xmax, ymax, depth);
        qt->childs[CHILD_TR] = create_quadtree(xavg, ymin, xmax, yavg, depth);
    }
    return qt;
}

void free_quadtree(quadtree_t *qt) {
    for(int i=0; i<CHILD_COUNT; i++)
        MemFree(qt->childs[i]);
    MemFree(qt);
}

bool quad_in_frustum(quadtree_t *qt, camera_t cam) {
    // left plane
    int inside = 0;
    inside+= point_in_frustum_left(cam, qt->min.x, qt->min.y); // top-left
    inside+= point_in_frustum_left(cam, qt->min.x, qt->max.y); // bottom-left
    inside+= point_in_frustum_left(cam, qt->max.x, qt->min.y); // top-right
    inside+= point_in_frustum_left(cam, qt->max.x, qt->max.y); // botton-right
    if (inside == 0) return false;

    // right plane
    inside = 0;
    inside+= point_in_frustum_right(cam, qt->min.x, qt->min.y); // top-left 
    inside+= point_in_frustum_right(cam, qt->min.x, qt->max.y); // bottom-left
    inside+= point_in_frustum_right(cam, qt->max.x, qt->min.y); // top-right
    inside+= point_in_frustum_right(cam, qt->max.x, qt->max.y); // botton-right
    if (inside == 0) return false;

    return true;
}

void render_quadtree(quadtree_t *qt, camera_t cam, int depth) {
    if (quad_in_frustum(qt, cam)) {
        if (depth > 1) {
            float xavg = (qt->min.x + qt->max.x)*0.5f;
            float yavg = (qt->min.y + qt->max.y)*0.5f;
            DrawLine(xavg, qt->min.y, xavg, qt->max.y, GRAY);
            DrawLine(qt->min.x, yavg, qt->max.x, yavg, GRAY);
            
            depth--;
            render_quadtree(qt->childs[CHILD_TL], cam, depth);
            render_quadtree(qt->childs[CHILD_BL], cam, depth);
            render_quadtree(qt->childs[CHILD_BR], cam, depth);
            render_quadtree(qt->childs[CHILD_TR], cam, depth);
        } else {
            DrawRectangle(qt->min.x, qt->min.y, qt->max.x-qt->min.x, qt->max.y-qt->min.y, LIGHTGRAY);
            DrawRectangleLines(qt->min.x-1, qt->min.y-1, qt->max.x-qt->min.x+1, qt->max.y-qt->min.y+1, GRAY);
        }
    }
}

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
static int screen_width = 512;
static int screen_height = 512;

// Quadtree
static int quad_depth = 6; // depth
static int quad_size = 512; // size
static quadtree_t *root;

// Camera
static float cam_speed = 2; // speed
static float cam_fov = 60.0f; // field of view
static float view_line = 300; // frustum plane
static camera_t camera;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
void UpdateDrawFrame(void);     // Update and Draw one frame

//----------------------------------------------------------------------------------
// Main Enry Point
//----------------------------------------------------------------------------------
int main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    InitWindow(screen_width, screen_height, "raylib: quadtree");

    // Create camera & quadtree
    fl = Vector2Zero();
    fr = Vector2Zero();
    camera = create_camera(quad_size/2, quad_size/2, cam_fov);
    root = create_quadtree(0, 0, quad_size, quad_size, quad_depth);

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    SetTargetFPS(60);   // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------
    
    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        UpdateDrawFrame();
    }
#endif

    // De-Initialization
    free_quadtree(root);
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
void UpdateDrawFrame(void)
{
    // Update
    //----------------------------------------------------------------------------------
    // TODO: Update your variables here
    //----------------------------------------------------------------------------------
    //Vector2 mouse_pos = GetMousePosition();

    // update camera position
    if (IsKeyDown(KEY_LEFT)) { 
        camera.pos.x-=cam_speed; 
    } else if (IsKeyDown(KEY_RIGHT)) { 
        camera.pos.x+=cam_speed; 
    }
    if (IsKeyDown(KEY_UP)) { 
        camera.pos.y-=cam_speed; 
    } else if (IsKeyDown(KEY_DOWN)) { 
        camera.pos.y+=cam_speed; 
    }

    // camera angle
    float px = (float)GetMouseX() - camera.pos.x;
    float py = (float)GetMouseY() - camera.pos.y;
    float angle = atan2f(py, px);
    float cam_fov_rad = (camera.fov / 2.0f) * DEG2RAD;
    Vector2 cam_dir = (Vector2){
        .x = camera.pos.x + (view_line * 0.5f) * cosf(angle),
        .y = camera.pos.y + (view_line * 0.5f) * sinf(angle)
    };

    // update frustum left position
    fl.x = camera.pos.x + view_line * cosf(angle - cam_fov_rad);
    fl.y = camera.pos.y + view_line * sinf(angle - cam_fov_rad);
    // update frustum right position
    fr.x = camera.pos.x + view_line * cosf(angle + cam_fov_rad);
    fr.y = camera.pos.y + view_line * sinf(angle + cam_fov_rad);

    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

        ClearBackground(WHITE);

        // draw quadtree
        render_quadtree(root, camera, quad_depth);
        DrawRectangleLines(root->min.x, root->min.y, root->max.x-root->min.x, root->max.y-root->min.y, RED);

        // draw frustum left plane
        DrawLine(camera.pos.x, camera.pos.y, fl.x, fl.y, GREEN);
        // draw frustum right plane
        DrawLine(camera.pos.x, camera.pos.y, fr.x, fr.y, RED);

        //  draw camera
        DrawLine(camera.pos.x, camera.pos.y, cam_dir.x, cam_dir.y, YELLOW);
        DrawCircle(camera.pos.x, camera.pos.y, 10, BLUE);

        // draw texts
        //DrawText(TextFormat("mouse.pos: (%f, %f)", px, py), 0, 0, 20, DARKGRAY);
        //DrawText(TextFormat("camera.pos: (%f, %f)", camera.pos.x, camera.pos.y), 0, 25, 20, DARKGRAY);

    EndDrawing();
    //----------------------------------------------------------------------------------
}