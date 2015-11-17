#include "Polygon.h"

#include <polypartition/polypartition.h>

#include "Assert.h"
#include "units/Polar.h"
#include "CPMath.h"
#include "SimUtilities.h"

namespace sim {

Polygon::Polygon() {
}

Polygon::Polygon(const Polygon& polygon) {
    m_vertices = polygon.getVertices();
    m_triangles = polygon.getTriangles();
}

Polygon::Polygon(const std::vector<Cartesian>& vertices) {
    ASSERT_LE(3, vertices.size());
    m_vertices = vertices;
    m_triangles = triangulate(vertices);
}

Polygon Polygon::createCirclePolygon(const Cartesian& position, const Distance& radius, int numberOfEdges) {
    ASSERT_LE(3, numberOfEdges);
    std::vector<Cartesian> vertices;
    for (int i = 0; i < numberOfEdges; i += 1) {
        vertices.push_back(Polar(radius, Radians(i * M_TWOPI / numberOfEdges)) + position);
    }
    return Polygon(vertices);
}

std::vector<Cartesian> Polygon::getVertices() const {
    return m_vertices;
}

std::vector<Triangle> Polygon::getTriangles() const {
    return m_triangles;
}

std::vector<std::pair<Cartesian, Cartesian>> Polygon::getLineSegments() const {
    std::vector<std::pair<Cartesian, Cartesian>> segments;
    if (2 < m_vertices.size()) {
        Cartesian previousPoint = m_vertices.back();
        for (Cartesian currentPoint : m_vertices) {
            segments.push_back(std::make_pair(previousPoint, currentPoint));
            previousPoint = currentPoint;
        }
    }
    return segments;
}

MetersSquared Polygon::area() const {

    // See http://mathworld.wolfram.com/PolygonArea.html

    double area = 0.0;
    for (int i = 0; i < m_vertices.size(); i += 1) {
        int j = (i + 1) % m_vertices.size();
        area += m_vertices.at(i).getX().getMeters() * m_vertices.at(j).getY().getMeters();
        area -= m_vertices.at(i).getY().getMeters() * m_vertices.at(j).getX().getMeters();
    }
    area /= 2.0;
    return MetersSquared(std::abs(area));
}

Cartesian Polygon::centroid() const {

    // See http://en.wikipedia.org/wiki/Centroid#Centroid_of_polygon

    Meters cx(0.0);
    Meters cy(0.0);
    for (int i = 0; i < m_vertices.size(); i += 1) {
        int j = (i + 1) % m_vertices.size();
        cx += (m_vertices.at(i).getX() + m_vertices.at(j).getX())
            * (m_vertices.at(i).getX() * m_vertices.at(j).getY() - m_vertices.at(j).getX() * m_vertices.at(i).getY()).getMetersSquared();
        cy += (m_vertices.at(i).getY() + m_vertices.at(j).getY())
            * (m_vertices.at(i).getX() * m_vertices.at(j).getY() - m_vertices.at(j).getX() * m_vertices.at(i).getY()).getMetersSquared();
    }
    cx = Meters(std::abs(cx.getMeters()));
    cy = Meters(std::abs(cy.getMeters()));
    return Cartesian(cx / (area() * 6).getMetersSquared(), cy / (area() * 6).getMetersSquared());
}

Polygon Polygon::translate(const Coordinate& translation) const {

    // Memoization fixes graphics tearing
    std::map<Cartesian, Cartesian> cache;

    std::vector<Cartesian> vertices;
    for (Cartesian vertex : m_vertices) {
        vertices.push_back(memoizedTranslateVertex(&cache, vertex, translation));
    }

    std::vector<Triangle> triangles;
    for (Triangle triangle : m_triangles) {
        triangles.push_back({
            memoizedTranslateVertex(&cache, triangle.p1, translation),
            memoizedTranslateVertex(&cache, triangle.p2, translation),
            memoizedTranslateVertex(&cache, triangle.p3, translation),
        });
    }

    return Polygon(vertices, triangles);
}

Polygon Polygon::rotateAroundPoint(const Angle& angle, const Coordinate& point) const {

    // Memoization fixes graphics tearing
    std::map<Cartesian, Cartesian> cache;

    std::vector<Cartesian> vertices;
    for (Cartesian vertex : m_vertices) {
        vertices.push_back(memoizedRotateVertexAroundPoint(&cache, vertex, angle, point));
    }

    std::vector<Triangle> triangles;
    for (Triangle triangle : m_triangles) {
        triangles.push_back({
            memoizedRotateVertexAroundPoint(&cache, triangle.p1, angle, point),
            memoizedRotateVertexAroundPoint(&cache, triangle.p2, angle, point),
            memoizedRotateVertexAroundPoint(&cache, triangle.p3, angle, point),
        });
    }

    return Polygon(vertices, triangles);
}
m
    m_vertices = vertices;
    m_triangles = triangles;
}

Cartesian Polygon::translateVertex(const Cartesian& vertex, const Coordinate& translation) {
    return vertex + translation;
}

Cartesian Polygon::rotateVertexAroundPoint(const Cartesian& vertex, const Angle& angle, const Coordinate& point) {
    Cartesian relativeVertex(
        vertex.getX() - point.getX(),
        vertex.getY() - point.getY()
    );
    Polar rotatedRelativeVertex(
        relativeVertex.getRho(),
        relativeVertex.getTheta() + angle
    );
    Cartesian rotatedVertex(
        rotatedRelativeVertex.getX() + point.getX(),
        rotatedRelativeVertex.getY() + point.getY()
    );
    return rotatedVertex;
}

Cartesian Polygon::memoizedTranslateVertex(
        std::map<Cartesian, Cartesian>* cache, const Cartesian& vertex, const Coordinate& translation) {
    if (!SimUtilities::mapContains(*cache, vertex)) {
        cache->insert(std::make_pair(vertex, translateVertex(vertex, translation)));
    }
    return cache->at(vertex);
}

Cartesian Polygon::memoizedRotateVertexAroundPoint(
        std::map<Cartesian, Cartesian>* cache, const Cartesian& vertex, const Angle& angle, const Coordinate& point) {
    if (!SimUtilities::mapContains(*cache, vertex)) {
        cache->insert(std::make_pair(vertex, rotateVertexAroundPoint(vertex, angle, point)));
    }
    return cache->at(vertex);
}

std::vector<Triangle> Polygon::triangulate(const std::vector<Cartesian>& vertices) {

    // Populate the TPPLPoly
    TPPLPoly tpplPoly;
    tpplPoly.Init(vertices.size());
    for (int i = 0; i < vertices.size(); i += 1) {
        tpplPoly[i].x = vertices.at(i).getX().getMeters();
        tpplPoly[i].y = vertices.at(i).getY().getMeters();
    }
    tpplPoly.SetOrientation(TPPL_CCW);

    // Perform the triangulation
    TPPLPartition triangulator;
    std::list<TPPLPoly> result;
    triangulator.Triangulate_OPT(&tpplPoly, &result);

    // Populate the output vector
    std::vector<Triangle> triangles;
    for (auto it = result.begin(); it != result.end(); it++) {
        triangles.push_back({
            Cartesian(Meters((*it)[0].x), Meters((*it)[0].y)),
            Cartesian(Meters((*it)[1].x), Meters((*it)[1].y)),
            Cartesian(Meters((*it)[2].x), Meters((*it)[2].y)),
        });
    }

    return triangles;
}

} // namespace sim
