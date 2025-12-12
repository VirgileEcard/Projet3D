#pragma once
#include <vector>
#include <set>
#include <random>
#include <optional>
#include <tuple>
#include <climits>
#include <iostream>
#include <map>

struct VoxelModel
{
    struct Part
    {
        double dx, dy, dz; // Position relative à la racine
        float r, g, b;  // Couleur
    };
    std::vector<Part> parts;
};

// Types de plantes
enum PlantType
{
    PLANT_NONE = 0,
    TREE_OAK,           // Chêne (Moyen)
    TREE_PINE,          // Pin (Sec/Roche)
    TREE_WILLOW,        // Saule (Humide)
    BUSH_BERRY,         // Buisson
    GRASS_TUFT,         // Touffe d'herbe (pas implémenté)
    CACTUS,             // Cactus
    PLANT_MUSHROOM_RED  // Champignon
};

// Notre bibliothèque de modèles
std::map<PlantType, VoxelModel> plantModels;

void initPlantModels()
{
    // 1. Chêne (Tronc + Boule de feuilles)
    VoxelModel oak;
    // Tronc
    for (int y = 1; y <= 4; y++)
        oak.parts.push_back({0.f, (float)y, 0.f, 0.4f, 0.2f, 0.1f});
    // Feuilles (3x3x3 centré en haut)
    for (int x = -2; x <= 2; x++)
    {
        for (int y = 3; y <= 5; y++)
        {
            for (int z = -2; z <= 2; z++)
            {
                if (abs(x) + abs(y - 4) + abs(z) <= 3) // Forme un peu arrondie
                    oak.parts.push_back({(float)x, (float)y, (float)z, 0.1f, 0.6f, 0.1f});
            }
        }
    }
    plantModels[TREE_OAK] = oak;

    // 2. Pin (Tronc haut + Cone)
    VoxelModel pine;
    for (int y = 1; y <= 6; y++)
        pine.parts.push_back({0.f, (float)y, 0.f, 0.3f, 0.15f, 0.05f});
    // Feuilles (Pyramide)
    for (int y = 3; y <= 7; y++)
    {
        int radius = 2 - (y - 3) / 2;
        for (int x = -radius; x <= radius; x++)
        {
            for (int z = -radius; z <= radius; z++)
            {
                if (x == 0 && z == 0 && y < 7)
                    continue; // Pas dans le tronc
                pine.parts.push_back({(float)x, (float)y, (float)z, 0.05f, 0.3f, 0.1f});
            }
        }
    }
    plantModels[TREE_PINE] = pine;

    // 3. Saule (Tronc + branches tombantes)
    VoxelModel willow;
    for (int y = 1; y <= 3; y++)
        willow.parts.push_back({0.f, (float)y, 0.f, 0.35f, 0.25f, 0.15f});
    // Canopée large
    for (int x = -2; x <= 2; x++)
    {
        for (int z = -2; z <= 2; z++)
        {
            willow.parts.push_back({(float)x, 4.f, (float)z, 0.2f, 0.5f, 0.1f});
            // Branches qui tombent
            if (abs(x) == 2 || abs(z) == 2)
            {
                willow.parts.push_back({(float)x, 3.f, (float)z, 0.2f, 0.55f, 0.1f});
                if ((x + z) % 2 != 0)
                    willow.parts.push_back({(float)x, 2.f, (float)z, 0.2f, 0.6f, 0.1f});
            }
        }
    }
    plantModels[TREE_WILLOW] = willow;

    // 4. Buisson (Petit tas)
    VoxelModel bush;
    bush.parts.push_back({0, 1, 0, 0.1f, 0.5f, 0.0f});
    bush.parts.push_back({1, 1, 0, 0.1f, 0.5f, 0.0f});
    bush.parts.push_back({0, 1, 1, 0.1f, 0.5f, 0.0f});
    bush.parts.push_back({0, 2, 0, 0.2f, 0.6f, 0.1f});
    plantModels[BUSH_BERRY] = bush;

    // 6. Cactus (Simple colonne)
    VoxelModel cactus;
    cactus.parts.push_back({0, 1, 0, 0.1f, 0.7f, 0.1f});
    cactus.parts.push_back({0, 2, 0, 0.1f, 0.7f, 0.1f});
    cactus.parts.push_back({0, 3, 0, 0.1f, 0.7f, 0.1f});
    plantModels[CACTUS] = cactus;

    // 7. Champignon (Pied + Chapeau) 
    VoxelModel mushRed;
    // Pied
    mushRed.parts.push_back({0.f, 0.f, 0.f, 0.9f, 0.9f, 0.8f});
    // Chapeau
    mushRed.parts.push_back({0.f, 1.f, 0.f, 0.9f, 0.1f, 0.1f});
    mushRed.parts.push_back({0.5f, 1.f, 0.f, 0.9f, 0.1f, 0.1f});
    mushRed.parts.push_back({-0.5f, 1.f, 0.f, 0.9f, 0.1f, 0.1f});
    mushRed.parts.push_back({0.f, 1.f, 0.5f, 0.9f, 0.1f, 0.1f});
    mushRed.parts.push_back({0.f, 1.f, -0.5f, 0.9f, 0.1f, 0.1f});
    plantModels[PLANT_MUSHROOM_RED] = mushRed;
}

// Liste des plantes placées dans le monde
struct PlantInstance
{
    int x, y, z;
    PlantType type;
};
