#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum Camera_Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
};

class Camera {
public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    float Yaw;
    float Pitch;

    float MovementSpeed = 3.0f;
    float MouseSensitivity = 0.1f;

    // границы комнаты
    float RoomLimit = 2.8f;
    float EyeHeight = 1.5f;

    Camera(glm::vec3 position)
    {
        Position = position;
        WorldUp = glm::vec3(0.0f, 1.0f, 0.0f);
        Yaw = -90.0f;
        Pitch = 0.0f;
        updateCameraVectors();
    }

    glm::mat4 GetViewMatrix()
    {
        return glm::lookAt(Position, Position + Front, Up);
    }

    void ProcessKeyboard(Camera_Movement direction, float deltaTime)
    {
        float velocity = MovementSpeed * deltaTime;

        // Убираем вертикальное движение
        glm::vec3 flatFront = glm::normalize(glm::vec3(Front.x, 0.0f, Front.z));
        glm::vec3 flatRight = glm::normalize(glm::vec3(Right.x, 0.0f, Right.z));

        if (direction == FORWARD)
            Position += flatFront * velocity;

        if (direction == BACKWARD)
            Position -= flatFront * velocity;

        if (direction == LEFT)
            Position -= flatRight * velocity;

        if (direction == RIGHT)
            Position += flatRight * velocity;

        // фиксируем высоту
        Position.y = EyeHeight;

        // ограничиваем движение по комнате
        Position.x = glm::clamp(Position.x, -RoomLimit, RoomLimit);
        Position.z = glm::clamp(Position.z, -RoomLimit, RoomLimit);
    }

    void ProcessMouseMovement(float xoffset, float yoffset)
    {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Yaw += xoffset;
        Pitch += yoffset;

        if (Pitch > 89.0f) Pitch = 89.0f;
        if (Pitch < -89.0f) Pitch = -89.0f;

        updateCameraVectors();
    }

private:
    void updateCameraVectors()
    {
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);

        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up = glm::normalize(glm::cross(Right, Front));
    }
};

#endif