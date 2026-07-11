#pragma once

class InputManager;

class GameInput
{
public:
    struct MenuFrame
    {
        bool escape = false;
        bool up = false;
        bool down = false;
        bool left = false;
        bool right = false;
        bool confirm = false;
        bool escapePressed = false;
        bool upPressed = false;
        bool downPressed = false;
        bool leftPressed = false;
        bool rightPressed = false;
        bool confirmPressed = false;
    };

    explicit GameInput(InputManager& input) : input_(input) {}

    void Update();
    void SuppressConfirm();
    const MenuFrame& Menu() const { return menu_; }

private:
    InputManager& input_;
    MenuFrame menu_;
};
