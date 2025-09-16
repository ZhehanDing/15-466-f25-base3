#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <memory>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;
	
	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;
	Scene::Transform *head = nullptr;
	//hexapod leg to wobble:
	Scene::Transform *hip = nullptr;
	Scene::Transform *upper_leg = nullptr;
	Scene::Transform *lower_leg = nullptr;
	Scene::Transform *win_block = nullptr;
	bool game_won = false;
	glm::quat hip_base_rotation;
	glm::quat upper_leg_base_rotation;
	glm::quat lower_leg_base_rotation;
	float wobble = 0.0f;

	bool left_pressed = false;
	bool right_pressed = false;
	bool jump_pressed = false;

	glm::vec3 velocity = glm::vec3(0.0f); 
	bool on_ground = true;

	bool charging_jump = false;
	float jump_charge = 0.0f;
	const float max_jump_force = 15.0f;
	const float charge_rate = 12.0f;
	glm::vec3 last_input_dir = glm::vec3(0.0f);
	glm::vec3 get_leg_tip_position();
	void reset_game();
	bool was_on_ground = true;

	//music coming from the tip of the leg (as a demonstration):
	std::shared_ptr< Sound::PlayingSample > charge_sound;
	//block add
	std::vector<Scene::Transform*> blocks;
	//car honk sound:
	std::shared_ptr< Sound::PlayingSample > jump_sound;
	std::shared_ptr< Sound::PlayingSample > land_sound;
	std::shared_ptr< Sound::PlayingSample > bgm_loop;
	
	//camera:
	Scene::Camera *camera = nullptr;

};
