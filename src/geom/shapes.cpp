#include "shapes.h"

#include "mesh.h"

#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include <util/tiny_obj_loader.h>
#endif

using namespace Eigen;

float BOUNDS_PADDING = 1.f;

Plane::Plane(Edge e, const Vector3d &norm, array<double, 6> bbox) {
    // Get the minimum and maximum coordinates of the bounding box
    auto [a, b, c, x, y, z] = bbox;
    Vector3d minCoords(a, b, c);
    Vector3d maxCoords(x, y, z);

    // Get diagonal distance between bounding box
    auto dist_diag = dist(minCoords, maxCoords);

    // Get direction vector
    auto n = (e[1] - e[0]).normalized();

    // Scale edge endpoints by that distance, in the appropriate direction
    e[0] -= dist_diag * n;
    e[1] += dist_diag * n;

    // Take those endpoints, and add/subtract the normal in which we wish to create the plane
    p0 = e[0] + dist_diag * norm;
    p1 = e[0] - dist_diag * norm;
    p2 = e[1] - dist_diag * norm;
    p3 = e[1] + dist_diag * norm;

    // ensure that all points make a plane
    double EPSILON = 1e-9;
    Eigen::Matrix<double, 3, 3> mat;
    mat << (p1 - p0), (p2 - p0), (p3 - p0);
    assert(mat.determinant() < EPSILON);
}

Plane::Plane(const quickhull::Plane<double> &p, std::array<double, 6> bbox) {
    // Convert plane types
    Vector3d planeNormal(p.m_N.x, p.m_N.y, p.m_N.z);
    Vector3d planePoint = -p.m_D * planeNormal;

    // Get the minimum and maximum coordinates of the bounding box
    auto [a, b, c, x, y, z] = bbox;
    Vector3d minCoords(a, b, c);
    Vector3d maxCoords(x, y, z);

    // Define the four corner points of the bounding region plane
    p0 = minCoords - BOUNDS_PADDING * Vector3d::Ones();
    p1 = minCoords - BOUNDS_PADDING * Vector3d::UnitX() +
         (maxCoords.x() - minCoords.x() + 2 * BOUNDS_PADDING) * Vector3d::UnitX();
    p2 = minCoords - BOUNDS_PADDING * Vector3d::UnitY() +
         (maxCoords.y() - minCoords.y() + 2 * BOUNDS_PADDING) * Vector3d::UnitY();
    p3 = planePoint + (p0 - planePoint).dot(planeNormal) / planeNormal.dot(p0 - p3) * (p3 - p0);
}

Plane Plane::load_from_file(const std::string &path) {
    // Load from obj file
    tinyobj::attrib_t attrib;
    vector<tinyobj::shape_t> shapes;
    vector<tinyobj::material_t> materials;

    QFileInfo info(QString(path.c_str()));
    string err;
    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err,
                                info.absoluteFilePath().toStdString().c_str(),
                                (info.absolutePath().toStdString() + "/").c_str(), true);
    if (!err.empty()) {
        cerr << err << endl;
    }

    if (!ret) {
        cerr << "Failed to load/parse .obj file" << endl;
        exit(1);
    }

    // Convert the tinyobj info into vertices; we don't care about faces for planes
    set<Vector3d> verts_set;
    for (size_t i = 0; i < attrib.vertices.size(); i += 3) {
        verts_set.emplace(attrib.vertices[i], attrib.vertices[i + 1], attrib.vertices[i + 2]);
    }

    vector<Vector3d> verts(verts_set.begin(), verts_set.end());

    // Construct and return a mesh from the vertices
    return Plane(verts[0], verts[1], verts[2], verts[3]);
}

void Plane::save_to_file(const std::string &path) {
    ofstream outfile;
    outfile.open(path);

    // Write the four vertices
    for (auto &&v : {p0, p1, p2, p3}) {
        outfile << "v " << v[0] << " " << v[1] << " " << v[2] << endl;
    }

    // Write faces; hardcode to trimesh the plane: (1, 2, 0) and (1, 3, 2); note .obj files are
    // 1-indexed
    for (auto &&f : {array{1, 2, 0}, {1, 3, 2}}) {
        outfile << "f";
        for (auto &&fi : f) outfile << " " << (fi + 1);
        outfile << endl;
    }

    outfile.close();
}
