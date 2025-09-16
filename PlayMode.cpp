//Credit to jumpking
//Chat GPT help debug
//Author: Alex Ding

#include "PlayMode.hpp"
#include "DrawLines.hpp"
#include "LitColorTextureProgram.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

// ----- Load jumpingland meshes and scene -----
GLuint jumpingland_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > jumpingland_meshes(LoadTagDefault, []() -> MeshBuffer const * {
    MeshBuffer const *ret = new MeshBuffer(data_path("jumpingland.pnct"));
    jumpingland_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
    return ret;
});

float jump_hold_time = 0.0f;
const float max_jump_height = 6.0f;
const float jump_angle_deg = 77.5f;


Load< Scene > jumpingland_scene(LoadTagDefault, []() -> Scene const * {
    return new Scene(data_path("jumpingland.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
        Mesh const &mesh = jumpingland_meshes->lookup(mesh_name);

        scene.drawables.emplace_back(transform);
        Scene::Drawable &drawable = scene.drawables.back();

        drawable.pipeline = lit_color_texture_program_pipeline;
        drawable.pipeline.vao = jumpingland_meshes_for_lit_color_texture_program;
        drawable.pipeline.type = mesh.type;
        drawable.pipeline.start = mesh.start;
        drawable.pipeline.count = mesh.count;
    });
});

Load< Sound::Sample > BGM_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("BGM.wav"));
});


Load< Sound::Sample > chargesound_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("chargesound.wav"));
});

Load< Sound::Sample > landsound_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("landsound.wav"));
});

Load< Sound::Sample > jumpsound_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("jumpsound.wav"));
});

Load< Sound::Sample > start_trace_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("start_trace.wav"));
});

Load< Sound::Sample > close_trace_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("close_trace.wav"));
});

Load< Sound::Sample > jump_scare_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("jumpscare.wav"));
});



// ----- PlayMode Implementation -----
PlayMode::PlayMode() : scene(*jumpingland_scene) {
    if (scene.cameras.empty()) throw std::runtime_error("No camera found in scene.");
    camera = &scene.cameras.front();

    // find "Head" in transforms:
    for (auto &t : scene.transforms) {
        if (t.name == "Head") {
            head = &t;
            break;
        }
        
    }
    for (auto &t : scene.transforms) {
        if (t.name == "Ghost.001") {
            ghost = &t;
            break;
        }
        
    }
    if (!head) throw std::runtime_error("No transform named 'Head' found in scene.");
    // Load cube file
    for (auto &t : scene.transforms) {
        if (t.name.find("block") != std::string::npos) {
            blocks.push_back(&t);
            if (t.name == "block.012") {
            win_block = &t;
            }
        }
    }
    bgm_loop=Sound::loop(*BGM_sample, 1.0f,0.0f);
}


PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &) {
    if (evt.type == SDL_EVENT_KEY_DOWN) {
        if (evt.key.key == SDLK_A&& on_ground) {
            left_pressed = true;
            last_input_dir = glm::vec3(-1.0f, 0.0f, 0.0f); // left
        }
        if (evt.key.key == SDLK_D&& on_ground) {
            right_pressed = true;
            last_input_dir = glm::vec3(1.0f, 0.0f, 0.0f); // right
        }
        if (evt.key.key == SDLK_SPACE && on_ground && !charging_jump) {
            charging_jump = true;
            jump_charge = 0.0f; // reset
            // Sound effect
            charge_sound = Sound::play(*chargesound_sample, 1.0f, 0.0f);
        }
}
	if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_A) left_pressed = false;
		if (evt.key.key == SDLK_D) right_pressed = false;

		if (evt.key.key == SDLK_SPACE && charging_jump) {
			charging_jump = false;
			on_ground = false;
            if (charge_sound) charge_sound->stop();
            jump_sound = Sound::play(*jumpsound_sample, 1.0f, 0.0f);


			// Decide jump direction
            //GPT Debug
		float angle_rad = glm::radians(jump_angle_deg);

		glm::vec3 jump_dir(0.0f, 0.0f, 1.0f); // default straight up

		if (glm::length(last_input_dir) > 0.1f) {
			glm::vec3 horiz = glm::normalize(glm::vec3(last_input_dir.x, 0.0f, 0.0f));

			// scale by cos/sin of angle
			jump_dir = horiz * glm::cos(angle_rad) + glm::vec3(0.0f, 0.0f, 1.0f) * glm::sin(angle_rad);
		}

		// normalize to be safe
		jump_dir = glm::normalize(jump_dir);

		// apply charged jump force
		velocity = jump_dir * std::min(jump_charge, max_jump_force);

		}
	}
    return false;
}

void PlayMode::update(float elapsed) {
    // Set game end
    if (game_won) {
    // Stop all movement
    velocity = glm::vec3(0.0f);
    left_pressed = right_pressed = charging_jump = false;
    if (close_trace) close_trace->stop();
    if (start_trace) start_trace->stop();

    return; // freeze game
    }
    if (game_over) {
    // Stop all movement
    velocity = glm::vec3(0.0f);
    left_pressed = right_pressed = charging_jump = false;
    if (close_trace) close_trace->stop();
    if (start_trace) start_trace->stop();
    if (!jumpscare_sound){
        jump_scare = Sound::play(*jump_scare_sample, 1.0f, 0.0f);
        jumpscare_sound= true;
    }
    if (jumpscare_active && camera && ghost) {
    // Place ghost right in front of the camera
    ghost->position.x = camera->transform->position.x;
    ghost->position.z = camera->transform->position.z-3.0f;

    ghost->scale = glm::vec3(3.0f); 
    ghost->position.y = camera->transform->position.y + 12.0f;
    jumpscare_timer -= elapsed;
    if (jumpscare_timer <= 0.0f) {
            jumpscare_active = false;
        }
    }

    return; // freeze game
    }

    if (!head) return;

    // horizontal movement:
    if (on_ground) {
    float speed = 5.0f;
    if (left_pressed) head->position.x -= speed * elapsed;
    if (right_pressed) head->position.x += speed * elapsed;
    }
    if (head->position.x < -3.0f) head->position.x = -3.0f;
    if (head->position.x > 11.0f) head->position.x = 11.0f;
	if (charging_jump && on_ground) {
    // Increase charge while holding
    //GPT Debug
    jump_charge += charge_rate * elapsed;
    if (jump_charge > max_jump_force) jump_charge = max_jump_force;
	}

    // gravity:
    float gravity = -20.0f;

	// apply gravity
	velocity.z += gravity * elapsed;
	head->position += velocity * elapsed;
    for (auto *block : blocks) {
    glm::vec3 block_pos = block->position;
    glm::vec3 block_size = glm::vec3(3.0f, 2.0f, 4.0f); 
    glm::vec3 head_size  = glm::vec3(1.0f, 1.0f, 1.0f);
    // Check distance
    // GPT Debug
    bool overlapX = std::abs(head->position.x - block_pos.x) < (head_size.x + block_size.x) * 0.5f;
    bool overlapY = std::abs(head->position.y - block_pos.y) < (head_size.y + block_size.y) * 0.5f;
    bool overlapZ = std::abs(head->position.z - block_pos.z) < (head_size.z + block_size.z) * 0.5f;
    // Set knight location
    // GPT Debug
    if (overlapX && overlapY && overlapZ) {
        // Land on top of cube
        if (velocity.z <= 0.0f && head->position.z > block_pos.z) {
            head->position.z = block_pos.z + (block_size.z + head_size.z) * 0.5f;
            velocity = glm::vec3(0.0f);
            on_ground = true;
            // Land sound effect
            // Check if knight get top
            if (block == win_block) {
            game_won = true;
            }
        }
    }
    if (!was_on_ground && on_ground) {
        // just landed
        land_sound = Sound::play(*landsound_sample, 1.0f, 0.0f);
    }


    // Update last frame state
    was_on_ground = on_ground;
}
    // Check trigger condition
    if (head && ghost && !ghost_tracing && head->position.z >= 15.0f) {
        // Rotate ghost 180 degrees on Z
        ghost->rotation = glm::angleAxis(glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)) * ghost->rotation;
        
        // Start tracing
        ghost_tracing = true;

        // Optional: play sound when trace starts
        if (bgm_loop) bgm_loop->stop();
        start_trace = Sound::loop(*start_trace_sample, 1.0f, 0.0f);
    }
        if (ghost_tracing && ghost && head) {
            glm::vec2 direction_xz = glm::vec2(
            head->position.x - ghost->position.x,
            head->position.z - ghost->position.z
        );
        float dist_xz = glm::length(direction_xz);
        if (dist_xz > 0.1f) {
        glm::vec2 dir_norm = glm::normalize(direction_xz);
        ghost->position.x += dir_norm.x * ghost_speed * elapsed;
        ghost->position.z += dir_norm.y * ghost_speed * elapsed;
        }
        if (dist_xz < 5.0f && !close_tracing ){
            long_tracing=false;
            close_tracing = true;
            if (start_trace) start_trace->stop();
            close_trace= Sound::loop(*close_trace_sample, 1.0f, 0.0f);
            
        }
        if (dist_xz > 5.0f && !long_tracing && close_tracing){
            close_tracing = false;
            long_tracing=true;
            if (close_trace) close_trace->stop();
            start_trace= Sound::loop(*start_trace_sample, 1.0f, 0.0f);
        }
        if (dist_xz < 0.5f) {  
        game_over = true;
        ghost_tracing = false;
        jumpscare_active = true;
        jumpscare_timer = 1.0f;
    }
    }

    // simple ground check:
    if (head->position.z <= 1.2f) {
        head->position.z = 1.2f;
        velocity = glm::vec3(0.0f);
        on_ground = true;
    }
    // Camera follows head if head goes out of view (jumping up):
    // GPT Debug
    if (head && camera) {
        float new_cam_z = camera->transform->position.z;

        // If head jumps above camera view
        if (head->position.z > camera->transform->position.z + 5.0f) {
            new_cam_z = camera->transform->position.z + 10.0f;
        }
        // If head falls below camera viewa
        else if (head->position.z < camera->transform->position.z-5.0f) {
            new_cam_z = camera->transform->position.z - 10.0f;
        }

        // Clamp so camera.z never goes below 4.0f
        if (new_cam_z < 4.0f) new_cam_z = 4.0f;

        camera->transform->position.z = new_cam_z;
    }
    
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
    camera->aspect = float(drawable_size.x) / float(drawable_size.y);

    // light:
    glUseProgram(lit_color_texture_program->program);
    glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
    glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
    glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
    glUseProgram(0);

    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    scene.draw(*camera);
    	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("AD to move, hold SPACE to Jump, reach the top to escape",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("AD to move, hold SPACE to Jump, reach the top to escape",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
    // game end 
    // GPT Debug
    if (game_won) {
    constexpr float H = 0.15f; 
    float aspect = float(drawable_size.x) / float(drawable_size.y);

    DrawLines lines(glm::mat4(
        1.0f / aspect, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    ));

    float text_width = 7.0f * H;

    glm::vec3 pos(-0.5f * text_width, 0.0f, 0.0f);

    lines.draw_text("YOU WIN",
        pos,
        glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
        glm::u8vec4(0x00, 0x00, 0x00, 0x00));

    float ofs = 2.0f / drawable_size.y;
    lines.draw_text("YOU WIN",
        pos + glm::vec3(ofs, ofs, 0.0f),
        glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
        glm::u8vec4(0xff, 0xff, 0xff, 0x00));
}
    if (game_over) {
    constexpr float H = 0.15f;
    float aspect = float(drawable_size.x) / float(drawable_size.y);

    DrawLines lines(glm::mat4(
        1.0f / aspect, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    ));

    float text_width = 9.0f * H; 

    glm::vec3 pos(-0.5f * text_width, 0.0f, 0.0f);

    lines.draw_text("GAME OVER",
        pos,
        glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
        glm::u8vec4(0x00, 0x00, 0x00, 0x00));

    float ofs = 2.0f / drawable_size.y;
    lines.draw_text("GAME OVER",
        pos + glm::vec3(ofs, ofs, 0.0f),
        glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
        glm::u8vec4(0xff, 0xff, 0xff, 0x00));
}

    GL_ERRORS();
}
