#pragma once
#include <vector>
#include <set>
#include <iostream>
#include "WFCEngine.h"

class RuleExtractor
{
private:
    int width, height, depth;
    std::vector<int> sampleGrid;
    int maxTileID = 0;

    int getIndex(int x, int y, int z) const
    {
        return y * (width * depth) + z * width + x;
    }

    bool isValid(int x, int y, int z) const
    {
        return x >= 0 && x < width && y >= 0 && y < height && z >= 0 && z < depth;
    }

public:
    RuleExtractor(int w, int h, int d) : width(w), height(h), depth(d)
    {
        sampleGrid.resize(w * h * d, 0);
    }

    // Remplit la grille d'exemple (ici on le fait voxel par voxel)
    void setVoxel(int x, int y, int z, int tileID)
    {
        if (isValid(x, y, z))
        {
            sampleGrid[getIndex(x, y, z)] = tileID;
            if (tileID > maxTileID)
                maxTileID = tileID;
        }
    }

    int getVoxel(int x, int y, int z) const
    {
        if (!isValid(x, y, z))
            return 0; // Retourne AIR si hors limites
        return sampleGrid[getIndex(x, y, z)];
    }

    // On "apprend" les règles
    std::vector<TileRule> extractRules(const std::vector<float[3]> &colors)
    {
        // 1. Initialiser les règles vides pour chaque ID de tuile trouvé
        std::vector<TileRule> rules(maxTileID + 1);
        for (int i = 0; i <= maxTileID; i++)
        {
            rules[i].id = i;
            // On copie les couleurs (passées depuis le main pour l'affichage)
            rules[i].color[0] = colors[i][0];
            rules[i].color[1] = colors[i][1];
            rules[i].color[2] = colors[i][2];
        }

        // Directions: -X, +X, -Y, +Y, -Z, +Z
        const int dx[6] = {-1, 1, 0, 0, 0, 0};
        const int dy[6] = {0, 0, -1, 1, 0, 0};
        const int dz[6] = {0, 0, 0, 0, -1, 1};

        // 2. Scanner tout le volume
        for (int x = 0; x < width; x++)
        {
            for (int y = 0; y < height; y++)
            {
                for (int z = 0; z < depth; z++)
                {
                    int currentID = sampleGrid[getIndex(x, y, z)];

                    // Pour ce voxel, on regarde ses 6 voisins
                    for (int dir = 0; dir < 6; dir++)
                    {
                        int nx = x + dx[dir];
                        int ny = y + dy[dir];
                        int nz = z + dz[dir];

                        // Si le voisin est dans la grille, on note la relation
                        if (isValid(nx, ny, nz))
                        {
                            int neighborID = sampleGrid[getIndex(nx, ny, nz)];

                            // On ajoute neighborID à la liste des voisins valides de currentID dans la direction dir
                            // std::vector ne gère pas l'unicité, donc on vérifie si ça existe déjà
                            auto &list = rules[currentID].validNeighbors[dir];
                            bool found = false;
                            for (int val : list)
                                if (val == neighborID)
                                    found = true;

                            if (!found)
                            {
                                list.push_back(neighborID);
                            }
                        }
                    }
                }
            }
        }

        return rules;
    }
};