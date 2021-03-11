#include "../SDL_main.cc"

#include "cp_lib/basic.cc"
#include "cp_lib/array.cc"
#include "cp_lib/vector.cc"
#include "cp_lib/memory.cc"
#include <stdlib.h>
#include <unistd.h>

#include "../draw.cc"
#include "wireframe_test.cc"

using namespace cp;

sbuff<Tuple<vec3f, vec4f>, 8> cube_vertices = {{
    { { -1, -1, -1 }, {{1, 1, 0, 0}} },
	{ { 1, -1, -1 }, {{1, 0, 1, 0}} },
	{ { 1, 1, -1 }, {{1, 0, 0, 1}} },
	{ { -1, 1, -1 }, {{1, 1, 0, 0}} },
	{ { -1, 1, 1 }, {{1, 0, 1, 0}} },
	{ { 1, 1, 1 }, {{1, 0, 0, 1}} },
	{ { 1, -1, 1 }, {{1, 1, 0, 0}} },
	{ { -1, -1, 1 }, {{1, 0, 0, 1}} }
}};
sbuff<u32[3], 10> cube_triangles = {{
    //{0, 2, 1}, //face front
    //{0, 3, 2},
    {2, 3, 4}, //face top
    {2, 4, 5},
    {1, 2, 5}, //face right
    {1, 5, 6},
    {0, 7, 4}, //face left
    {0, 4, 3},
    {5, 4, 7}, //face back
    {5, 7, 6},
    {0, 6, 7}, //face bottom
    {0, 1, 6}
}};

Mesh cube_mesh = {{(u8*)cube_vertices.buffer, cap(&cube_vertices) * (u32)sizeof(Tuple<vec3f, vec4f>)}, {cube_triangles.buffer, cap(&cube_triangles)}};

vec3f cube_position = { 0, 0, 3};
quat cube_rotation = {1, 0, 0, 0};

bool is_ortho = true;


dbuff<vec3f> proj_buffer;

dbuff<u8> proc_buffer;

float line_color[2][4] = {{1, 0.9, 0, 0.9}, {1, 0, 0.9, 0.9}};
dbuff2f line_color_buffer = {(f32*)line_color, 2, 4};

// float triangle_color[3][4] = {{1, 0.9, 0, 0.9}, {1, 0, 0.9, 0.9}, {1, 0.9, 0.9, 0}};
float triangle_color[3][4] = {{1, 1, 0, 0}, {1, 0, 0, 1}, {1, 0, 1, 0}};
dbuff2f triangle_color_buffer = {(f32*)triangle_color, 3, 4};


dbuff2f z_buffer;

static quat rot_x;
static quat rot_y;
static quat rot_z;


void test_color_itpl_vsh(void* handle_p) {
    auto handle = (Vertex_Shader_Handle*)handle_p;

    auto vertex = (Tuple<vec3f, vec4f>*)handle->vertex;
    
    auto t_args = (Tuple<Render_Object*, vec2f(*)(vec3f)>*)handle->args;
    Render_Object* obj = t_args->get<0>();
    vec2f(*project_lmd)(vec3f) = t_args->get<1>();

    vec3f p = *obj->rotation * vertex->get<0>() + *obj->position;
    vec2f pr = project_lmd(p);

    handle->out_vertex_itpl_vector[0] = p.z;
    *(vec4f*)&handle->out_vertex_itpl_vector[1] = vertex->get<1>();
    *handle->out_vertex_position = space_to_screen_coord(pr, window_size/4, {25, 25});
}


void test_color_itpl_fsh(void* args) {
    auto handle = (Fragment_Shader_Handle*)args;

    float z = handle->itpl_vector[0];
    dbuff2f *z_buffer = (dbuff2f*)handle->args;
    f32* prev_z;
    if (!z_buffer->sget(&prev_z, handle->point.y, handle->point.x) || z > *prev_z)
        return;
    

    vec4f *fcolor = (vec4f*)&handle->itpl_vector[1];
    Color color;
    if (abs(z) != 0) { 
        color = to_color( *fcolor / z);
    } else 
        color = {0xffffffff};
    handle->set_pixel_color_lmd(handle->out_frame_buffer, handle->point, color);
    *prev_z = z;
}


void game_init() {
    input_init();

    window_max_size = {1920, 1080};
    window_size = {1280, 720};

    proj_buffer.init(8);
    proc_buffer.init(80000);
    // create buffer

    frame_buffer.init(window_size.y/4, window_size.x/4);
    z_buffer.init(window_size.y/4, window_size.x/4);

    rot_x.init(normalized(vec3f{1, 0, 0}), M_PI/10);
    rot_y.init(normalized(vec3f{0, 1, 0}), M_PI/10);
    rot_z.init(normalized(vec3f{0, 0, 1}), M_PI/10);
}

void game_shut() {
    input_shut();
    proj_buffer.shut();
    proc_buffer.shut();
    frame_buffer.shut();
    z_buffer.shut();
}



void game_update() {


    if (get_bit(Input::keys_down, 'q')) {
        is_running = false;
    }
    if (get_bit(Input::keys_down, 'o')) {
        is_ortho = !is_ortho;
    }

    if (get_bit(Input::keys_hold, 'w')) {
        cube_position += vec3f(0, 0, 0.1);
    }
    if (get_bit(Input::keys_hold, 's')) {
        cube_position += vec3f(0, 0, -0.1);
    }
    if (get_bit(Input::keys_hold, 'a')) {
        cube_position += vec3f(-0.1, 0, 0);
    }
    if (get_bit(Input::keys_hold, 'd')) {
        cube_position += vec3f(0.1, 0, 0);
    }
    if (get_bit(Input::keys_hold, 't')) {
        cube_rotation = rot_x * cube_rotation;
    }    
    if (get_bit(Input::keys_hold, 'g')) {
        cube_rotation = inverse(rot_x) * cube_rotation;
    }
    if (get_bit(Input::keys_hold, 'f')) {
        cube_rotation = rot_y * cube_rotation;
    }
    if (get_bit(Input::keys_hold, 'h')) {
        cube_rotation = inverse(rot_y) * cube_rotation;
    }

    // write to buffer

    for (auto p = begin(&frame_buffer); p < end(&frame_buffer); p++) {
        *p = 0xff223344;
    }
    for (auto p = begin(&z_buffer); p < end(&z_buffer); p++) {
        *p = INT_MAX;
    }

    // Color c = {0xff5533ff};


    // if (pointer_local_pos.x < window_size.x && pointer_local_pos.y < window_size.y)
    //     rasterize_line(frame_buffer, {500, 500}, pointer_local_pos - vec2i(100, 100), 
    //     line_color_buffer, color_itpl_frag_shader, null, set_pixel_color);
    
    // if (pointer_local_pos.x < window_size.x && pointer_local_pos.y < window_size.y)
    //     rasterize_triangle_scanline(frame_buffer, {500, 500}, {300, 200}, pointer_local_pos, {0xff00ff00}, proc_buffer, set_pixel_color);

    // if (pointer_local_pos.x < window_size.x && pointer_local_pos.y < window_size.y)
    //     rasterize_triangle_scanline(frame_buffer, {500, 500}, {300, 200}, pointer_local_pos, 
    //         triangle_color_buffer, color_itpl_frag_shader, null, proc_buffer, set_pixel_color);

    // rasterize_triangle_scanline(frame_buffer, {500, 500}, {300, 200}, {700, 600}, 
    //         triangle_color_buffer, color_itpl_frag_shader, null, proc_buffer, set_pixel_color);

    // for (i32 i = 1; i < 200; i++) {
    //     rasterize_line(frame_buffer, {1800, 100 + i}, {300, 900 + i}, {0xffff55ff + i}, set_pixel_color);
    // }

    // if (is_ortho) {
    //     render_wireframe({&cube_mesh, &cube_position, &cube_rotation}, project_xy_orthogonal, _proj_buffer, 
    //                 frame_buffer, {0xffffffff}, window_size, {100, 100});
    // } else {
    //     render_wireframe({&cube_mesh, &cube_position, &cube_rotation}, project_xy_perspective, _proj_buffer, 
    //                 frame_buffer, {0xffffffff}, window_size, {100, 100});
    // }
    // if (is_ortho) {
    //     render_wireframe({&cube_mesh, &cube_position, &cube_rotation}, project_xy_orthogonal, _proj_buffer, 
    //                 frame_buffer, z_buffer, window_size, {100, 100});
    // } else {
    //     render_wireframe({&cube_mesh, &cube_position, &cube_rotation}, project_xy_perspective, _proj_buffer, 
    //                 frame_buffer, z_buffer, window_size, {100, 100});
    // }
    // if (is_ortho) {
    //     render_mesh({&cube_mesh, &cube_position, &cube_rotation}, project_xy_orthogonal, proc_buffer, 
    //                 &frame_buffer, z_buffer, window_size/4, {25, 25});
    // } else {
    //     render_mesh({&cube_mesh, &cube_position, &cube_rotation}, project_xy_perspective, proc_buffer, 
    //                 &frame_buffer, z_buffer, window_size/4, {100, 100});
    // }


    Render_Object robj = {&cube_mesh, &cube_position, &cube_rotation};
    Vertex_Buffer vb = {{(u8*)robj.mesh->vertex_buffer.buffer, robj.mesh->vertex_buffer.cap}, sizeof(Tuple<vec3f, vec4f>)};

    Tuple<Render_Object*, vec2f(*)(vec3f)> vsh_args;
    vsh_args.get<0>() = &robj;
    if (is_ortho) {
        vsh_args.get<1>() = project_xy_orthogonal;
    } else 
        vsh_args.get<1>() = project_xy_perspective;


    Shader_Pack shaders = {test_color_itpl_vsh, &vsh_args, test_color_itpl_fsh, &z_buffer, 5};
    draw_triangles(&frame_buffer, &vb, &robj.mesh->index_buffer, &shaders, &proc_buffer);


    vec2i pointer_local_pos = Input::mouse_pos/4;
    if (0 < pointer_local_pos.x - 5 && pointer_local_pos.x - 5 < window_size.x/4 && 0 < pointer_local_pos.y - 5 && pointer_local_pos.y - 5 < window_size.y/4) {
        frame_buffer.get(pointer_local_pos.y-5, pointer_local_pos.x-5) = 0xffff0000;
        printf("%i %i\n", pointer_local_pos.x-5, pointer_local_pos.y-5);
    }
    



}

