#pragma once
#include <raylib.h>
#include <vector>

struct GridCoord {
    int x = 0;
    int y = 0;
};

enum class TileKind {
    Empty,
    Road,
    Buildable,
    Fortress,
    Spawn,
    Blocked
};

struct GridTile {
    TileKind kind = TileKind::Empty;
    bool occupied = false;
    float height = 0.12f;
};

struct GridMap {
    int width = 0;
    int height = 0;
    float cellSize = 2.0f;
    Vector3 origin = { 0.0f, 0.0f, 0.0f };
    std::vector<GridTile> tiles;

    bool InBounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < width && y < height;
    }

    GridTile& At(int x, int y) {
        return tiles[(size_t)y * (size_t)width + (size_t)x];
    }

    const GridTile& At(int x, int y) const {
        return tiles[(size_t)y * (size_t)width + (size_t)x];
    }

    Vector3 CellCenter(int x, int y) const {
        return {
            origin.x + x * cellSize + cellSize * 0.5f,
            0.0f,
            origin.z + y * cellSize + cellSize * 0.5f
        };
    }

    GridCoord WorldToCell(Vector3 world) const {
        return {
            (int)((world.x - origin.x) / cellSize),
            (int)((world.z - origin.z) / cellSize)
        };
    }
};
