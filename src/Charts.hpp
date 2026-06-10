#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include "History.hpp"

// One line in a chart, pointing at a field of Sample.
struct Series
{
    float Sample::*member;
    sf::Color   color;
    const char* name;
    bool        integer = false;   // legend formatting
};

// Draws a line chart with title, legend (showing the latest value of each
// series) and time axis labels.
//   sharedScale = true  : all series share one 0-based y scale (populations)
//   sharedScale = false : each series is auto-scaled to its own min..max,
//                         which makes *trends* of differently-scaled stats
//                         comparable in a single chart
void drawChart(sf::RenderTarget& rt, const sf::FloatRect& rect, const char* title,
               const std::vector<Sample>& samples, const std::vector<Series>& series,
               bool sharedScale, const sf::Font* font);
