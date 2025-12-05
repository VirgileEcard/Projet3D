#pragma once
#include <vector>
#include <set>
#include <random>
#include <optional>
#include <tuple>
#include <climits>
#include <iostream>

struct TileRule
{
    int id;
    std::vector<int> validNeighbors[6];
    float color[3];
    float baseWeight = 1.0f;
};

struct Cell
{
    std::set<int> possibleTiles;
    int collapsedTile = -1;
    bool isCollapsed() const { return collapsedTile != -1; }
    int entropy() const { return (int)possibleTiles.size(); }
};

using WeightFunc = std::function<float(int tileId, int x, int y, int z)>;

class WFCEngine
{
private:
    int width, height, depth;
    std::vector<Cell> grid;
    std::vector<TileRule> tileSet;
    std::mt19937 rng;
    bool failed = false;

    int getIndex(int x, int y, int z) const
    {
        return y * (width * depth) + z * width + x;
    }

    WeightFunc weightOverride = nullptr;

    // Directions: -X, +X, -Y, +Y, -Z, +Z
    const int dx[6] = {-1, 1, 0, 0, 0, 0};
    const int dy[6] = {0, 0, -1, 1, 0, 0};
    const int dz[6] = {0, 0, 0, 0, -1, 1};

    void propagate(int startX, int startY, int startZ)
    {
        std::vector<std::tuple<int, int, int>> stack;
        stack.push_back({startX, startY, startZ});

        while (!stack.empty())
        {
            auto [cx, cy, cz] = stack.back();
            stack.pop_back();

            Cell &currentCell = grid[getIndex(cx, cy, cz)];

            // Pour chaque voisin
            for (int dir = 0; dir < 6; dir++)
            {
                int nx = cx + dx[dir];
                int ny = cy + dy[dir];
                int nz = cz + dz[dir];

                if (nx < 0 || nx >= width || ny < 0 || ny >= height || nz < 0 || nz >= depth)
                    continue;

                Cell &neighbor = grid[getIndex(nx, ny, nz)];
                if (neighbor.isCollapsed())
                    continue; // Déjà fixé

                // Quelles sont les tuiles possibles pour le voisin, étant donné les options actuelles de 'currentCell' ?
                std::set<int> allowedNeighborTiles;

                for (int myTileId : currentCell.possibleTiles)
                {
                    // Sécurité : si myTileId est invalide (ne devrait pas arriver), on ignore
                    if (myTileId >= tileSet.size())
                        continue;

                    const auto &valid = tileSet[myTileId].validNeighbors[dir];
                    allowedNeighborTiles.insert(valid.begin(), valid.end());
                }

                // Intersection : On ne garde que ce qui était déjà possible ET qui est autorisé par le cas présent
                std::vector<int> toRemove;
                for (int nbTileId : neighbor.possibleTiles)
                {
                    if (allowedNeighborTiles.find(nbTileId) == allowedNeighborTiles.end())
                    {
                        toRemove.push_back(nbTileId);
                    }
                }

                if (!toRemove.empty())
                {
                    for (int id : toRemove)
                        neighbor.possibleTiles.erase(id);

                    if (neighbor.possibleTiles.empty())
                    {
                        failed = true; // Contradiction !
                        return;
                    }
                    stack.push_back({nx, ny, nz});
                }
            }
        }
    }

public:
    WFCEngine(int w, int h, int d, const std::vector<TileRule> &tiles, unsigned int seed)
        : width(w), height(h), depth(d), tileSet(tiles), rng(seed)
    {

        grid.resize(width * height * depth);
        reset();
    }

    void reset()
    {
        failed = false;
        for (auto &cell : grid)
        {
            cell.collapsedTile = -1;
            cell.possibleTiles.clear();
            for (const auto &tile : tileSet)
            {
                cell.possibleTiles.insert(tile.id);
            }
        }
    }

    void setWeightFunction(WeightFunc func) { weightOverride = func; }

    // Force une cellule à un état spécifique
    bool forceCollapse(int x, int y, int z, int tileId)
    {
        if (failed)
            return false; // Ne pas continuer si déjà en échec

        int idx = getIndex(x, y, z);
        if (grid[idx].possibleTiles.find(tileId) == grid[idx].possibleTiles.end())
            return false;

        grid[idx].possibleTiles.clear();
        grid[idx].possibleTiles.insert(tileId);
        grid[idx].collapsedTile = tileId;

        propagate(x, y, z);
        return !failed;
    }

    // Interdit une tuile spécifique à une coordonnée
    bool banTile(int x, int y, int z, int tileId)
    {
        if (failed)
            return false;

        int idx = getIndex(x, y, z);
        Cell &cell = grid[idx];

        // Si la tuile n'est déjà pas possible, on ne fait rien
        if (cell.possibleTiles.find(tileId) == cell.possibleTiles.end())
            return true;

        // On retire la tuile
        cell.possibleTiles.erase(tileId);

        // Sécurité : Si c'était la dernière possibilité, c'est un échec
        if (cell.possibleTiles.empty())
        {
            failed = true;
            return false;
        }

        // Si on a réduit les possibilités à 1 seule, on marque comme collapsed
        if (cell.possibleTiles.size() == 1)
        {
            cell.collapsedTile = *cell.possibleTiles.begin();
        }

        // On propage ce changement aux voisins
        propagate(x, y, z);

        return !failed;
    }

    // Une seule étape de l'algo
    bool step()
    {
        if (failed)
            return false;

        // Entropie Min
        int minEntropy = INT_MAX;
        std::vector<std::tuple<int, int, int>> candidates;

        for (int x = 0; x < width; x++)
        {
            for (int z = 0; z < depth; z++)
            {
                for (int y = 0; y < height; y++)
                {
                    Cell &c = grid[getIndex(x, y, z)];
                    if (!c.isCollapsed())
                    {
                        int ent = c.entropy();

                        // Si une cellule n'a plus aucune option, c'est un échec
                        /*if (ent == 0)
                        {
                            /*
                            failed = true;
                            std::cout << "Echec WFC : Contradiction trouvee en " << x << "," << y << "," << z << std::endl;
                            return false;*
                            candidates.clear();
                            candidates.push_back({x, y, z});
                            goto ForceCollapse;
                        }*/

                        if (ent < minEntropy)
                        {
                            minEntropy = ent;
                            candidates.clear();
                            candidates.push_back({x, y, z});
                            break;
                        }
                        else if (ent == minEntropy)
                        {
                            candidates.push_back({x, y, z});
                            break;
                        }
                    }
                }
            }
        }

        if (candidates.empty())
            return false; // Tout est fini

        ForceCollapse:
        // 2. Collapse
        std::uniform_int_distribution<> distIdx(0, (int)candidates.size() - 1);
        auto [tx, ty, tz] = candidates[distIdx(rng)];
        Cell &target = grid[getIndex(tx, ty, tz)];

        // Sécurité : Double vérification
        if (target.possibleTiles.empty())
        {
            for (const auto &t : tileSet)
                target.possibleTiles.insert(t.id);
            //failed = true;
            //return false;
        }

        std::vector<int> options(target.possibleTiles.begin(), target.possibleTiles.end());
        std::vector<float> weights;
        float totalWeight = 0.0f;

        for (int tileId : options)
        {
            float w = tileSet[tileId].baseWeight;

            // Si une fonction de biais est définie, on l'utilise pour modifier le poids localement
            if (weightOverride)
            {
                w *= weightOverride(tileId, tx, ty, tz);
            }

            weights.push_back(w);
            totalWeight += w;
        }

        std::uniform_real_distribution<float> distWeight(0.0f, totalWeight);
        float randomValue = distWeight(rng);
        int pickedTile = options.back();

        float currentSum = 0.0f;
        for (size_t i = 0; i < options.size(); i++)
        {
            currentSum += weights[i];
            if (randomValue <= currentSum)
            {
                pickedTile = options[i];
                break;
            }
        }

        target.possibleTiles.clear();
        target.possibleTiles.insert(pickedTile);
        target.collapsedTile = pickedTile;

        // 3. Propagate
        propagate(tx, ty, tz);

        return true;
    }

    // Accesseurs
    const Cell &getCell(int x, int y, int z) const { return grid[getIndex(x, y, z)]; }
    const TileRule &getTile(int id) const { return tileSet[id]; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getDepth() const { return depth; }
    bool isFailed() const { return failed; }
};