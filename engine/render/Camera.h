#pragma once

#include <cmath>

#include <glm/glm.hpp>

namespace ember {

// View + projection. Right-handed, +Y up, the camera looks down -Z in view space. Reversed-Z with an
// infinite far plane: depth is 1 at the near plane and falls towards 0 with distance, which keeps
// depth precision even over kilometres.
struct Camera {
  glm::mat4 view{1.0f};
  glm::mat4 projection{1.0f};
  glm::vec3 position{0.0f};
  glm::vec3 right{1.0f, 0.0f, 0.0f};    // world-space camera right
  glm::vec3 forward{0.0f, 0.0f, -1.0f};

  glm::mat4 viewProjection() const { return projection * view; }

  // upHint: the world direction that shows as up on screen; it must not be parallel to the line of sight (a camera
  // looking straight down passes the board's far side, -z).
  static Camera lookAt(glm::vec3 eye, glm::vec3 target, float verticalFovRadians, float aspect, float nearPlane = 0.1f,
                       glm::vec3 upHint = {0.0f, 1.0f, 0.0f}) {
    Camera c;
    c.position = eye;
    c.forward = glm::normalize(target - eye);
    c.right = glm::normalize(glm::cross(c.forward, upHint));
    const glm::vec3 up = glm::cross(c.right, c.forward);
    c.view = glm::mat4(1.0f);
    c.view[0] = glm::vec4(c.right.x, up.x, -c.forward.x, 0.0f);
    c.view[1] = glm::vec4(c.right.y, up.y, -c.forward.y, 0.0f);
    c.view[2] = glm::vec4(c.right.z, up.z, -c.forward.z, 0.0f);
    c.view[3] = glm::vec4(-glm::dot(c.right, eye), -glm::dot(up, eye), glm::dot(c.forward, eye), 1.0f);
    const float f = 1.0f / std::tan(verticalFovRadians * 0.5f);
    c.projection = glm::mat4(0.0f);
    c.projection[0][0] = f / aspect;
    c.projection[1][1] = -f;  // Vulkan clip space: +Y is down
    c.projection[2][3] = -1.0f;
    c.projection[3][2] = nearPlane;  // depth = near / distance
    return c;
  }
};

}  // namespace ember
