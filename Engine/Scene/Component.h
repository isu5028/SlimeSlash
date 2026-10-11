#pragma once

class GameObject;
class Renderer;

class Component
{
public:
    explicit Component(GameObject& owner)
        : owner_(owner)
    {

    }

    virtual ~Component() = default;

    virtual void Update()
    {

    }

    virtual void Draw(Renderer& renderer)
    {

    }

protected:
    GameObject& GetOwner()
    {
        return owner_;
    }

private:
    GameObject& owner_;
};