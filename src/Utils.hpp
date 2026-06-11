#pragma once

#include <SFML/System/Vector2.hpp>
#include <cmath>
#include <random>

inline constexpr float PI = 3.14159265358979f;

inline std::mt19937& rng()
{
    // thread_local: each worker thread (see headless training) gets its own
    // independent stream, so parallel simulations don't race on one generator
    thread_local std::mt19937 gen{ std::random_device{}() };
    return gen;
}

inline float frand(float lo, float hi)
{
    return std::uniform_real_distribution<float>(lo, hi)(rng());
}

inline float nrand(float mean, float sigma)
{
    return std::normal_distribution<float>(mean, sigma)(rng());
}

inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float len(sf::Vector2f v)
{
    return std::sqrt(v.x * v.x + v.y * v.y);
}

inline float dist2(sf::Vector2f a, sf::Vector2f b)
{
    const float dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy;
}

inline sf::Vector2f norm(sf::Vector2f v)
{
    const float l = len(v);
    return l > 1e-5f ? sf::Vector2f(v.x / l, v.y / l) : sf::Vector2f(1.f, 0.f);
}

// wrap an angle into [-PI, PI]
inline float wrapAngle(float a)
{
    while (a >  PI) a -= 2.f * PI;
    while (a < -PI) a += 2.f * PI;
    return a;
}
