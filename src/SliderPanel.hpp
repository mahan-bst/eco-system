#pragma once

#include <SFML/Graphics.hpp>
#include <vector>

// A small overlay of draggable sliders used to tweak simulation parameters
// while it runs. Each slider is bound directly to a float (see cfg::tune).
class SliderPanel
{
public:
    void add(const char* name, float* value, float lo, float hi, const char* fmt);
    void setPosition(sf::Vector2f pos) { m_pos = pos; }

    float width() const;
    float height() const;

    // mouse positions in window (default-view) coordinates;
    // onMousePressed returns true if the click landed on the panel
    bool onMousePressed(sf::Vector2f p);
    void onMouseMoved(sf::Vector2f p);
    void onMouseReleased() { m_active = -1; }

    void draw(sf::RenderTarget& rt, const sf::Font* font) const;

    bool visible = false;

private:
    struct Slider
    {
        const char* name;
        float* value;
        float lo, hi;
        const char* fmt;   // printf format for the value label
    };

    sf::FloatRect trackRect(std::size_t i) const;
    void applyDrag(std::size_t i, float x);

    std::vector<Slider> m_sliders;
    sf::Vector2f m_pos;
    int m_active = -1;     // slider currently being dragged
};
