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
#include <stdlib.h>
#include <math.h>

#include "raylib.h"
#include "raymath.h"

//#define PLATFORM_WEB

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

//--------------------------------------
// types/structures declaration
//--------------------------------------

typedef enum tform_space_e {
    TFORM_LOCAL = 0,
    TFORM_WORLD = 1,
} tform_space_t;

typedef enum tform_dirty_e {
    TFORM_DIRTY_LOCAL=1,
    TFORM_DIRTY_WORLD=2
} tform_dirty_t;

typedef struct tform_s {
    Quaternion rot;
    Vector3 pos, scale;
    Matrix mat;
} tform_t;

typedef struct entity_s {
    struct entity_s *parent, *children, *succ, *pred, *last_child;
    bool visible, enabled;
    const char *name;
    tform_dirty_t dirty;
    tform_t local;
    tform_t world;
} entity_t;

static entity_t *entity_orphans = NULL;
static entity_t *entity_last_orphan = NULL;

//--------------------------------------
// public entity functions declaration
//--------------------------------------
entity_t *create_entity();
entity_t *copy_entity(const entity_t *e);
void free_entity(entity_t *e);

void entity_set_parent(entity_t *e, entity_t *p);
void entity_set_name(entity_t *e, const char *name);
void entity_set_visible(entity_t *e, bool visible);
void entity_set_enabled(entity_t *e, bool enabled);
entity_t *entity_get_parent(entity_t *e);
const char *entity_get_name(entity_t *e);
entity_t *entity_get_children(entity_t *e);
entity_t *entity_get_successor(entity_t *e);

// entity transform functions
void entity_set_position(entity_t *e, Vector3 pos, tform_space_t global);
void entity_set_scale(entity_t *e, Vector3 scale, tform_space_t global);
void entity_set_rotation(entity_t *e, Quaternion rot, tform_space_t global);
Vector3 entity_get_position(entity_t *e, tform_space_t global);
Vector3 entity_get_scale(entity_t *e, tform_space_t global);
Quaternion entity_get_rotation(entity_t *e, tform_space_t global);

void move_entity(entity_t *e, float x, float y, float z);
void turn_entity(entity_t *e, float p, float y, float r, tform_space_t global);
void translate_entity(entity_t *e, float x, float y, float z, tform_space_t global);
void position_entity(entity_t *e, float x, float y, float z, tform_space_t global);
void scale_entity(entity_t *e, float x, float y, float z, tform_space_t global);
void rotate_entity(entity_t *e, float p, float y, float r, tform_space_t global);
void point_entity(entity_t *e, entity_t *t, float roll);
void align_entity(entity_t *e, float nx, float ny, float nz, int axis, float rate);

//void entity_enum_visible(entity_t *e, vector<entity_t*> &out); //TODO: need list
//void entity_enum_enabled(entity_t *e, vector<entity_t*> &out); //TODO: need list

//--------------------------------------
// private entity functions declaration
//--------------------------------------
void entity_insert(entity_t *e);
void entity_remove(entity_t *e);
void entity_invalidate_tform(entity_t *e, tform_space_t global);
void entity_set_tform(entity_t *e, Matrix mat, tform_space_t global);
Matrix entity_get_tform(entity_t *e, tform_space_t global);

//--------------------------------------
// public entity functions definition
//--------------------------------------

entity_t *create_entity() {
    entity_t *e = (entity_t*)MemAlloc(sizeof(entity_t));
    if (e) {
        e->parent = NULL;
        e->children = NULL;
        e->succ = NULL;
        e->pred = NULL;
        e->last_child = NULL;

        e->visible = true;
        e->enabled = true;
        e->local.pos = Vector3Zero();
        e->local.scale = Vector3One();
        e->local.rot = QuaternionIdentity();
        e->dirty = TFORM_DIRTY_LOCAL|TFORM_DIRTY_WORLD;
        entity_insert(e);
    } else {
        TraceLog(LOG_ERROR, TextFormat("unable allocate memory to create new entity!"));
        exit(1);
    }
    return e;
}

entity_t *copy_entity(const entity_t *e) {
    entity_t *cp = create_entity();
    if (cp) {
        cp->name = e->name;
        cp->visible = e->visible;
        cp->enabled = e->enabled;
        cp->local.pos = e->local.pos;
        cp->local.scale = e->local.scale;
        cp->local.rot = e->local.rot;
        cp->dirty = TFORM_DIRTY_LOCAL|TFORM_DIRTY_WORLD;
    }
    return cp;
}

void free_entity(entity_t *e) {
    while (e->children) {
        MemFree(e->children);
        e->children = NULL;
    }
    entity_remove(e);
}

void entity_set_parent(entity_t *e, entity_t *p) {
    if (e->parent == p) return;
    entity_remove(e);
    e->parent = p;
    entity_insert(e);
    entity_invalidate_tform(e, TFORM_WORLD);
}

void entity_set_name(entity_t *e, const char *name) { e->name = name; }
void entity_set_visible(entity_t *e, bool visible) { e->visible = visible; }
void entity_set_enabled(entity_t *e, bool enabled) { e->enabled = enabled; }
entity_t *entity_get_parent(entity_t *e) { return e->parent; }
const char *entity_get_name(entity_t *e) { return e->name; }
entity_t *entity_get_children(entity_t *e) { return e->children; }
entity_t *entity_get_successor(entity_t *e) { return e->succ; }

// entity transformation functions
void entity_set_position(entity_t *e, Vector3 pos, tform_space_t global) {
    if (global) {
        entity_set_position(e, (e->parent) ? Vector3Transform(pos, MatrixInvert(entity_get_tform(e->parent, TFORM_WORLD))) : pos, TFORM_LOCAL);
    } else {
        e->local.pos = pos; 
        entity_invalidate_tform(e, TFORM_LOCAL);
    }
}

void entity_set_scale(entity_t *e, Vector3 scale, tform_space_t global) {
    if (global) {
        entity_set_scale(e, (e->parent) ? Vector3Divide(scale, entity_get_scale(e->parent, TFORM_WORLD)) : scale, TFORM_LOCAL);
    } else {
        e->local.scale = scale; 
        entity_invalidate_tform(e, TFORM_LOCAL);
    }
}

void entity_set_rotation(entity_t *e, Quaternion rot, tform_space_t global) {
    if (global) {
        entity_set_rotation(e, (e->parent) ? QuaternionMultiply(QuaternionInvert(entity_get_rotation(e->parent, TFORM_WORLD)), rot) : rot, TFORM_LOCAL);
    } else {
        e->local.rot = QuaternionNormalize(rot); 
        entity_invalidate_tform(e, TFORM_LOCAL);
    }
}

Vector3 entity_get_position(entity_t *e, tform_space_t global) {
    if (global) {
        Matrix mat = entity_get_tform(e, TFORM_WORLD);
        return (Vector3){mat.m12, mat.m13, mat.m14};
    } else {
        return e->local.pos; 
    }
}

Vector3 entity_get_scale(entity_t *e, tform_space_t global) {
    if (global) {
        return (e->parent) ? Vector3Multiply(entity_get_scale(e->parent, TFORM_WORLD), e->local.scale) : e->local.scale;
    } else {
        return e->local.scale;
    }
}

Quaternion entity_get_rotation(entity_t *e, tform_space_t global) {
    if (global) {
        return (e->parent) ? QuaternionMultiply(entity_get_rotation(e->parent, TFORM_WORLD), e->local.rot) : e->local.rot;
    } else {
        return e->local.rot;
    }
}

void move_entity(entity_t *e, float x, float y, float z) {
    Vector3 pos = (Vector3){x, y, z};
    entity_set_position(e, Vector3Add(entity_get_position(e, TFORM_LOCAL), Vector3RotateByQuaternion(pos, entity_get_rotation(e, TFORM_LOCAL))), TFORM_LOCAL);
}

void turn_entity(entity_t *e, float p, float y, float r, tform_space_t global) {
    Quaternion rot = QuaternionFromEuler(p * DEG2RAD, y * DEG2RAD, r * DEG2RAD);
    global ?
    entity_set_rotation(e, QuaternionMultiply(rot, entity_get_rotation(e, TFORM_WORLD)), TFORM_WORLD):
    entity_set_rotation(e, QuaternionMultiply(entity_get_rotation(e, TFORM_LOCAL), rot), TFORM_LOCAL);
}

void translate_entity(entity_t *e, float x, float y, float z, tform_space_t global) {
    Vector3 pos = (Vector3){x, y, z};
    entity_set_position(e, Vector3Add(entity_get_position(e, global), pos), global);
}

void position_entity(entity_t *e, float x, float y, float z, tform_space_t global) {
    Vector3 pos = (Vector3){x, y, z};
    entity_set_position(e, pos, global);
}

void scale_entity(entity_t *e, float x, float y, float z, tform_space_t global) {
    Vector3 scale = (Vector3){x, y, z};
    entity_set_scale(e, scale, global);
}

void rotate_entity(entity_t *e, float p, float y, float r, tform_space_t global) {
    Quaternion rot = QuaternionFromEuler(p * DEG2RAD, y * DEG2RAD, r * DEG2RAD);
    entity_set_rotation(e, rot, global);
}

void point_entity(entity_t *e, entity_t *t, float roll) {
    Vector3 v = Vector3Subtract(entity_get_position(t, TFORM_WORLD), entity_get_position(e, TFORM_WORLD));
    entity_set_rotation(e, QuaternionFromEuler(-atan2f(v.y, sqrtf(v.x*v.x+v.y*v.y)), -atan2f(v.x, v.z), roll * DEG2RAD), TFORM_WORLD);
}

//--------------------------------------
// private entity functions definition
//--------------------------------------

void entity_insert(entity_t *e) {
    if (e) {
        e->succ = NULL;
        if (e->parent) {
            if ((e->pred = e->parent->last_child)) e->pred->succ = e;
            else e->parent->children = e;
            e->parent->last_child = e;
        } else {
            if ((e->pred = entity_last_orphan)) e->pred->succ = e;
            else entity_orphans = e;
            entity_last_orphan = e;
        }
    }
}

void entity_remove(entity_t *e) {
    if (e) {
        if (e->parent) {
            if(e->parent->children == e) e->parent->children = e->succ;
            if(e->parent->last_child == e) e->parent->last_child = e->pred;
        } else {
            if(entity_orphans == e) entity_orphans = e->succ;
            if(entity_last_orphan == e) entity_last_orphan = e->pred;
        }
        if(e->succ) e->succ->pred = e->pred;
        if(e->pred) e->pred->succ = e->succ;
    }
}

void entity_invalidate_tform(entity_t *e, tform_space_t global) {
    if (global) {
        if (e->dirty & TFORM_DIRTY_WORLD) return;
        e->dirty |= TFORM_DIRTY_WORLD;
        for(entity_t *c = e->children; c; c = c->succ)
            entity_invalidate_tform(c, TFORM_WORLD);
    } else {
        e->dirty |= TFORM_DIRTY_LOCAL;
        entity_invalidate_tform(e, TFORM_WORLD);
    }
}

void entity_set_tform(entity_t *e, Matrix mat, tform_space_t global) {
    if (global) {
        entity_set_tform(e, (e->parent) ? MatrixMultiply(mat, MatrixInvert(entity_get_tform(e->parent, TFORM_WORLD))) : mat, TFORM_LOCAL);
    } else {
        e->local.pos = (Vector3){mat.m12, mat.m13, mat.m14};
        e->local.rot = QuaternionFromMatrix(mat);
        e->local.scale = (Vector3){
            Vector3Length((Vector3){mat.m0, mat.m1, mat.m2}),
            Vector3Length((Vector3){mat.m4, mat.m5, mat.m6}),
            Vector3Length((Vector3){mat.m8, mat.m9, mat.m10})
        };
        entity_invalidate_tform(e, TFORM_LOCAL);
    }
}

Matrix entity_get_tform(entity_t *e, tform_space_t global) {
    if (global) {
        if (e->dirty & TFORM_DIRTY_WORLD) {
            e->world.mat = (e->parent) ? MatrixMultiply(entity_get_tform(e, TFORM_LOCAL), entity_get_tform(e->parent, TFORM_WORLD)) : entity_get_tform(e, TFORM_LOCAL);
            e->dirty &=~TFORM_DIRTY_WORLD;
        }
        return e->world.mat;
    } else {
        if (e->dirty & TFORM_DIRTY_LOCAL) {
            Matrix rot, scl, pos; 
            Vector3 axis; float angle;

            // Calculate transformation matrix from local transform
            QuaternionToAxisAngle(e->local.rot, &axis, &angle);
            rot = MatrixRotate(axis, angle);
            scl = MatrixScale(e->local.scale.x, e->local.scale.y, e->local.scale.z);
            pos = MatrixTranslate(e->local.pos.x, e->local.pos.y, e->local.pos.z);
            
            // Get transform matrix (rotation -> scale -> translation)
            e->local.mat = MatrixMultiply(MatrixMultiply(scl, rot), pos);

            e->dirty &=~TFORM_DIRTY_LOCAL;
        }
        return e->local.mat;
    }
}

//--------------------------------------
// Global Variables Definition
//--------------------------------------
static int screen_width = 1024;
static int screen_height = 768;

static Camera camera = {0};
static entity_t *center = NULL;
static entity_t *child1 = NULL;
static entity_t *child2 = NULL;
static entity_t *child3 = NULL;

static Model cube;
static Model sphere;

static float dt = 0.f;

//--------------------------------------
// Module Functions Declaration
//--------------------------------------
void UpdateDrawFrame(void);     // Update and Draw one frame
void DrawEntityModel(entity_t *e, Model model, Color tint);
void DrawEntityOrbit(entity_t *e, Color tint);

//----------------------------------------------------------------------------------
// Main Enry Point
//----------------------------------------------------------------------------------
int main()
{
    //--------------------------------------
    // Initialization
    //--------------------------------------
    InitWindow(screen_width, screen_height, "raylib: entity system");

    // camera 3d
    camera.position = (Vector3){0.f, 10.f, 15.f};
    camera.target = (Vector3){0.f, 0.f, 0.f};
    camera.up = (Vector3){0.f, 1.f, 0.f};
    camera.fovy = 45.f;
    camera.projection = CAMERA_PERSPECTIVE;

    // entities
    center = create_entity();
    child1 = create_entity();
    child2 = create_entity();
    child3 = create_entity();

    entity_set_parent(child1, center);
    position_entity(child1, 0.f, 0.f, -5.f, TFORM_LOCAL);

    entity_set_parent(child2, child1);
    scale_entity(child2, 0.5f, 0.5f, 0.5f, TFORM_LOCAL);
    position_entity(child2, 3.f, 0.f, 0.f, TFORM_LOCAL);

    entity_set_parent(child3, child2);
    scale_entity(child3, 0.25f, 0.25f, 0.25f, TFORM_LOCAL);
    position_entity(child3, 0.f, 0.f, -2.f, TFORM_LOCAL);

    // models
    cube = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    sphere = LoadModelFromMesh(GenMeshSphere(1.0f, 12, 8));

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    SetTargetFPS(60);   // Set our game to run at 60 frames-per-second
    
    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        UpdateDrawFrame();
    }
#endif
    //--------------------------------------
    // De-Initialization
    //--------------------------------------
    if (center) free_entity(center);

    CloseWindow();        // Close window and OpenGL context

    return 0;
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
void UpdateDrawFrame(void)
{
    //--------------------------------------
    // Update
    //--------------------------------------
    dt = GetFrameTime();

    turn_entity(center,0.f, .3f, 0.f, TFORM_LOCAL);
    turn_entity(child1,0.f, .6f, 0.f, TFORM_LOCAL);
    turn_entity(child2,0.f,-2.f, 0.f, TFORM_LOCAL);

    //--------------------------------------
    // Draw
    //--------------------------------------
    BeginDrawing();

        ClearBackground(RAYWHITE);

        BeginMode3D(camera);
            DrawEntityModel(center, cube, RED);
            DrawEntityModel(child1, cube, GREEN);
            DrawEntityModel(child2, sphere, BLUE);
            DrawEntityModel(child3, sphere, MAGENTA);

            DrawEntityOrbit(child1, GREEN);
            DrawEntityOrbit(child2, BLUE);
            DrawEntityOrbit(child3, MAGENTA);

            DrawGrid(10, 1.f);
        EndMode3D();

        // draw texts
        DrawFPS(0, 0);

    EndDrawing();
}

// Draw a entity model
void DrawEntityModel(entity_t *e, Model model, Color tint) {
    model.transform = entity_get_tform(e, TFORM_WORLD);

    for (int i = 0; i < model.meshCount; i++)
    {
        Color color = model.materials[model.meshMaterial[i]].maps[MATERIAL_MAP_DIFFUSE].color;

        Color colorTint = WHITE;
        colorTint.r = (unsigned char)((((float)color.r/255.0)*((float)tint.r/255.0))*255.0f);
        colorTint.g = (unsigned char)((((float)color.g/255.0)*((float)tint.g/255.0))*255.0f);
        colorTint.b = (unsigned char)((((float)color.b/255.0)*((float)tint.b/255.0))*255.0f);
        colorTint.a = (unsigned char)((((float)color.a/255.0)*((float)tint.a/255.0))*255.0f);

        model.materials[model.meshMaterial[i]].maps[MATERIAL_MAP_DIFFUSE].color = colorTint;
        DrawMesh(model.meshes[i], model.materials[model.meshMaterial[i]], model.transform);
        model.materials[model.meshMaterial[i]].maps[MATERIAL_MAP_DIFFUSE].color = color;
    }
}

void DrawEntityOrbit(entity_t *e, Color tint) {
    if (e->parent) {
        Vector3 axis; 
        float angle, length;
        length = Vector3Length(Vector3Subtract(entity_get_position(e->parent, TFORM_WORLD), entity_get_position(e, TFORM_WORLD)));
        QuaternionToAxisAngle(entity_get_rotation(e->parent, TFORM_WORLD), &axis, &angle);
        DrawCircle3D(entity_get_position(e->parent, TFORM_WORLD), length, (Vector3){1.f, 0.f, 0.f}, 90.f, tint); //FIXME: axis, angle!
    }
}