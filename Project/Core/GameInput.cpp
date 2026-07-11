#include "GameInput.h"

#include "InputManager.h"

#include <Windows.h>

void GameInput::Update()
{
    menu_.escape = input_.IsDown(VK_ESCAPE);
    menu_.up = input_.IsAnyDown('W', VK_UP);
    menu_.down = input_.IsAnyDown('S', VK_DOWN);
    menu_.left = input_.IsAnyDown('A', VK_LEFT);
    menu_.right = input_.IsAnyDown('D', VK_RIGHT);
    menu_.confirm = input_.IsDown(VK_RETURN) || input_.IsDown(VK_SPACE) ||
        input_.IsDown('J') || input_.IsDown('Z');
    menu_.escapePressed = input_.WasPressed(VK_ESCAPE);
    menu_.upPressed = input_.WasAnyPressed('W', VK_UP);
    menu_.downPressed = input_.WasAnyPressed('S', VK_DOWN);
    menu_.leftPressed = input_.WasAnyPressed('A', VK_LEFT);
    menu_.rightPressed = input_.WasAnyPressed('D', VK_RIGHT);
    menu_.confirmPressed = input_.WasPressed(VK_RETURN) || input_.WasPressed(VK_SPACE) ||
        input_.WasPressed('J') || input_.WasPressed('Z');
}

void GameInput::SuppressConfirm()
{
    input_.ConsumePress(VK_RETURN);
    input_.ConsumePress(VK_SPACE);
    input_.ConsumePress('J');
    input_.ConsumePress('Z');
    Update();
}
