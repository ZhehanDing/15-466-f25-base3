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
    bgm_loop=Sound::loop(*BGM_sample, 1.0f,1.0f);
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
    if (game_won) {
    constexpr float H = 0.15f; // bigger size than instructions
    float aspect = float(drawable_size.x) / float(drawable_size.y);

    DrawLines lines(glm::mat4(
        1.0f / aspect, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    ));

    // measure width: "YOU WIN" has 7 chars
    float text_width = 7.0f * H;

    // center horizontally at x = -text_width/2, y = 0
    glm::vec3 pos(-0.5f * text_width, 0.0f, 0.0f);

    // black shadow
    lines.draw_text("YOU WIN",
        pos,
        glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
        glm::u8vec4(0x00, 0x00, 0x00, 0x00));

    // white foreground
    float ofs = 2.0f / drawable_size.y;
    lines.draw_text("YOU WIN",
        pos + glm::vec3(ofs, ofs, 0.0f),
        glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
        glm::u8vec4(0xff, 0xff, 0xff, 0x00));
}

    GL_ERRORS();
}
