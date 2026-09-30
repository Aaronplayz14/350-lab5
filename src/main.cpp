#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <vector>

#include <SFML/Graphics.hpp>

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 800;
const int FPS_LIMIT = 30;

using Point2D = sf::Vector2f;

struct BezierCurve {
    std::vector<Point2D> points;
    float animationT = 0.0f;
};

BezierCurve makeCurve(float verticalOffset = 0.0f) {
    return {{
        {100.0f, 400.0f + verticalOffset},
        {250.0f, 100.0f + verticalOffset},
        {550.0f, 700.0f + verticalOffset},
        {700.0f, 400.0f + verticalOffset}
    }, 0.0f};
}

std::vector<BezierCurve> curves = {makeCurve()};
std::size_t selectedCurve = 0;
int draggedPoint = -1;

Point2D getPoint(const std::vector<sf::Vector2f>& pts, float t){
    float u = 1.0f - t;

    return u * u * u * pts[0] + 3.0f * u * u * t * pts[1] + 3.0f * u * t * t * pts[2] + t * t * t * pts[3];
}

Point2D getSlope(const std::vector<sf::Vector2f>& pts, float t){ 
    float u = 1.0f - t;

    return 3.0f * u * u * (pts[1] - pts[0]) + 6.0f * u * t * (pts[2] - pts[1]) + 3.0f * t * t * (pts[3] - pts[2]);
}

void drawLine(sf::RenderWindow& window, const Point2D& a, const Point2D& b, sf::Color color = sf::Color::White){
    sf::VertexArray line(sf::PrimitiveType::Lines, 2);

    line[0] = sf::Vertex{a, color};
    line[1] = sf::Vertex{b, color};

    window.draw(line);
}

// The join is a control point shared by two segments (index 0, 3, 6, ...).
// When one of its handles moves, swing the other one around so the two
// segments still leave the join in the same direction.
void matchSlope(BezierCurve& curve, int join, int movedHandle) {
    if (join <= 0 || join % 3 != 0 || join + 1 >= static_cast<int>(curve.points.size())) {
        return;
    }

    const int otherHandle = movedHandle < join ? join + 1 : join - 1;
    const Point2D direction = curve.points[join] - curve.points[movedHandle];

    if (direction.lengthSquared() < 0.0001f) {
        return;
    }

    const float otherLength = (curve.points[otherHandle] - curve.points[join]).length();
    curve.points[otherHandle] = curve.points[join] + direction.normalized() * otherLength;
}

void addPoints(BezierCurve& curve){
    Point2D end = curve.points.back();
    Point2D direction;

    if (curve.points.size() >= 2) {
        direction = end - curve.points[curve.points.size() - 2];
    } else {
        direction = {100.0f, 0.0f};
    }
    if (direction.lengthSquared() < 0.0001f) {
        direction = {100.0f, 0.0f};
    } else {
        direction = direction.normalized() * 100.0f;
    }
    Point2D p4 = end + direction;
    Point2D p5 = end + direction * 2.0f;
    Point2D p6 = end + direction * 3.0f;

    curve.points.push_back(p4);
    curve.points.push_back(p5);
    curve.points.push_back(p6);
}

void removePoints(BezierCurve& curve){
    if (curve.points.size() <= 4) {
        return;
    }
    curve.points.resize(curve.points.size() - 3);
    draggedPoint = -1;
}

void addCurve() {
    constexpr float offsets[] = {-60.0f, 60.0f, -30.0f, 30.0f};
    const float offset = offsets[(curves.size() - 1) % std::size(offsets)];
    curves.push_back(makeCurve(offset));
    selectedCurve = curves.size() - 1;
    draggedPoint = -1;
}

void removeSelectedCurve() {
    if (curves.size() <= 1) {
        std::cout << "At least one curve must remain.\n";
        return;
    }

    curves.erase(curves.begin() + static_cast<std::ptrdiff_t>(selectedCurve));
    selectedCurve = std::min(selectedCurve, curves.size() - 1);
    draggedPoint = -1;
}

bool exportCurvesAsCpp() {
    std::ofstream output("bezier_curves.hpp", std::ios::trunc);
    if (!output) {
        return false;
    }

    output << "#pragma once\n\n"
              "#include <SFML/System/Vector2.hpp>\n"
              "#include <vector>\n\n"
              "namespace bezier_export {\n"
              "inline const std::vector<std::vector<sf::Vector2f>> curves = {\n";
    output << std::fixed << std::setprecision(3);
    for (const BezierCurve& curve : curves) {
        output << "    {\n";
        for (const Point2D& point : curve.points) {
            output << "        {" << point.x << "f, " << point.y << "f},\n";
        }
        output << "    },\n";
    }
    output << "};\n}\n";
    output.close();
    return !output.fail();
}

void handleInput(sf::Window& window, bool& shouldQuit) {
    while (const std::optional<sf::Event> event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window.close();
            shouldQuit = true;
        } else if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (mouse->button == sf::Mouse::Button::Left) {
                BezierCurve& curve = curves[selectedCurve];
                Point2D mousePosition{static_cast<float>(mouse->position.x), static_cast<float>(mouse->position.y)};
                float closestDistanceSquared = std::numeric_limits<float>::max();
                int closestIndex = -1;

                for (int i = 0; i < static_cast<int>(curve.points.size()); ++i) {
                    Point2D difference = curve.points[i] - mousePosition;
                    float distanceSquared = difference.lengthSquared();

                    if (distanceSquared < closestDistanceSquared) {
                        closestDistanceSquared = distanceSquared;
                        closestIndex = i;
                    }
                }

                const float grabRadius = 15.0f;

                if (closestIndex != -1 && closestDistanceSquared <= grabRadius * grabRadius) {
                    draggedPoint = closestIndex;
                }
            }
        } else if (const auto* mouse = event->getIf<sf::Event::MouseButtonReleased>()) {
            if (mouse->button == sf::Mouse::Button::Left) {
                draggedPoint = -1;
            }
        } else if (const auto* mouse = event->getIf<sf::Event::MouseMoved>()) {
            if (draggedPoint != -1) {
                BezierCurve& curve = curves[selectedCurve];
                Point2D mousePosition{static_cast<float>(mouse->position.x), static_cast<float>(mouse->position.y)};
                Point2D movement = mousePosition - curve.points[draggedPoint];
                curve.points[draggedPoint] = mousePosition;

                for (int join = 3; join < static_cast<int>(curve.points.size()) - 1; join += 3) {
                    if (draggedPoint == join - 1 || draggedPoint == join + 1) {
                        matchSlope(curve, join, draggedPoint);
                    } else if (draggedPoint == join) {
                        curve.points[join - 1] += movement;
                        curve.points[join + 1] += movement;
                    }
                }
            }
        } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::N) {
                addCurve();
                std::cout << "Added curve " << selectedCurve + 1 << ".\n";
            } else if (key->code == sf::Keyboard::Key::Tab) {
                selectedCurve = (selectedCurve + 1) % curves.size();
                draggedPoint = -1;
                std::cout << "Selected curve " << selectedCurve + 1 << ".\n";
            } else if (key->code == sf::Keyboard::Key::Delete) {
                removeSelectedCurve();
                std::cout << "Selected curve " << selectedCurve + 1 << ".\n";
            } else if (key->code == sf::Keyboard::Key::E) {
                if (exportCurvesAsCpp()) {
                    std::cout << "Exported curves to bezier_curves.hpp in the working directory.\n";
                } else {
                    std::cerr << "Could not write bezier_curves.hpp.\n";
                }
            } else if (key->code == sf::Keyboard::Key::Equal || key->code == sf::Keyboard::Key::Add) {
                addPoints(curves[selectedCurve]);
            } else if (key->code == sf::Keyboard::Key::Hyphen || key->code == sf::Keyboard::Key::Subtract) {
                removePoints(curves[selectedCurve]);
            }
        }
    }
}

void drawBezierSegment(sf::RenderWindow& window, const std::vector<Point2D>& segment, sf::Color color) {
    constexpr int NUM_SAMPLES = 100;
    Point2D previous = getPoint(segment, 0.0f);

    for (int i = 1; i <= NUM_SAMPLES; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(NUM_SAMPLES);
        Point2D point = getPoint(segment, t);
        drawLine(window, previous, point, color);
        previous = point;
    }
}

void drawGalagaScreenFill(sf::RenderWindow& window) {
    constexpr sf::Vector2f screenSize{300.0f, 600.0f};
    constexpr sf::Vector2f screenPosition{
        (WINDOW_WIDTH - screenSize.x) / 2.0f,
        (WINDOW_HEIGHT - screenSize.y) / 2.0f
    };

    sf::RectangleShape screen(screenSize);
    screen.setPosition(screenPosition);
    screen.setFillColor(sf::Color(10, 14, 18));
    window.draw(screen);
}

void drawGalagaScreenBorder(sf::RenderWindow& window) {
    constexpr sf::Vector2f screenSize{300.0f, 600.0f};
    constexpr sf::Vector2f screenPosition{
        (WINDOW_WIDTH - screenSize.x) / 2.0f,
        (WINDOW_HEIGHT - screenSize.y) / 2.0f
    };

    sf::RectangleShape border(screenSize);
    border.setPosition(screenPosition);
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineThickness(2.0f);
    border.setOutlineColor(sf::Color(70, 220, 235));
    window.draw(border);
}

void render(sf::RenderWindow& window) {
    window.clear(sf::Color::Black);
    drawGalagaScreenFill(window);

    for (std::size_t curveIndex = 0; curveIndex < curves.size(); ++curveIndex) {
        const BezierCurve& curve = curves[curveIndex];
        const bool isSelected = curveIndex == selectedCurve;
        const sf::Color curveColor = isSelected ? sf::Color::White : sf::Color(150, 160, 170);
        const sf::Color handleColor = isSelected ? sf::Color(110, 110, 110) : sf::Color(80, 88, 94);

        for (std::size_t i = 0; i + 3 < curve.points.size(); i += 3) {
            std::vector<Point2D> segment = {
                curve.points[i], curve.points[i + 1], curve.points[i + 2], curve.points[i + 3]
            };
            drawBezierSegment(window, segment, curveColor);
            drawLine(window, curve.points[i], curve.points[i + 1], handleColor);
            drawLine(window, curve.points[i + 2], curve.points[i + 3], handleColor);
        }
    }

    for (BezierCurve& curve : curves) {
        // animationT runs across the whole curve, so scale it to a single segment.
        const int segmentCount = (curve.points.size() - 1) / 3;
        const float scaledT = std::clamp(curve.animationT, 0.0f, 1.0f) * segmentCount;
        const int segmentIndex = std::min(static_cast<int>(scaledT), segmentCount - 1);
        const float localT = scaledT - segmentIndex;
        const auto start = curve.points.begin() + segmentIndex * 3;
        const std::vector<Point2D> segment(start, start + 4);

        Point2D position = getPoint(segment, localT);
        Point2D slope = getSlope(segment, localT);
        sf::RectangleShape square({16.0f, 16.0f});
        square.setOrigin({8.0f, 8.0f});
        square.setPosition(position);

        if (slope.lengthSquared() > 0.0001f) {
            square.setRotation(slope.angle());
        }
        square.setFillColor(sf::Color::Green);
        window.draw(square);

        curve.animationT += 0.005f;
        if (curve.animationT > 1.0f) {
            curve.animationT = 0.0f;
        }
    }

    for (std::size_t curveIndex = 0; curveIndex < curves.size(); ++curveIndex) {
        const BezierCurve& curve = curves[curveIndex];
        const bool isSelected = curveIndex == selectedCurve;
        for (std::size_t i = 0; i < curve.points.size(); ++i) {
            sf::CircleShape circle(8.0f);
            circle.setOrigin({8.0f, 8.0f});
            circle.setPosition(curve.points[i]);

            if (isSelected && static_cast<int>(i) == draggedPoint) {
                circle.setFillColor(sf::Color::Red);
            } else {
                circle.setFillColor(isSelected ? sf::Color::Yellow : sf::Color(120, 145, 150));
            }
            window.draw(circle);
        }
    }

    drawGalagaScreenBorder(window);
    window.display();
}

int main() {
    sf::RenderWindow window;

    try {
        // Initialize window
        window.create(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "Bezier Curve Editor");
        window.setFramerateLimit(FPS_LIMIT);
        // Prevent key repeats.
        window.setKeyRepeatEnabled(false);

        bool shouldQuit = false;
        // Main game loop
        while (window.isOpen()) {
            handleInput(window, shouldQuit);
            if (shouldQuit) {
                break;
            }
            render(window);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
