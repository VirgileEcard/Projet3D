#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <chrono>

#include "PerlinNoise.h"
#include "WFCEngine.h"
#include "RuleExtractor.h"
#include "VegetationSystem.h"


float g_CameraDistance = 50.0f;
float g_CameraAngleX = 30.0f;
float g_CameraAngleY = 45.0f;
bool g_MousePressed = false;
double g_LastMouseX, g_LastMouseY;

const int GRID_SIZE = 64;
const int GRID_HEIGHT = 32;

bool showSample = true;

struct VoxelData
{
    float density;
    float hardness;
    float humidity;
    int surfaceLevel;
    float waterAmount;
};

VoxelData worldData[GRID_SIZE][GRID_HEIGHT][GRID_SIZE];

enum TileID
{
    AIR = 0,
    SURFACE_FORET = 1,
    SURFACE_HERBE = 2,
    SURFACE_SABLE = 3,
    SURFACE_ROCHE = 4,
    DEEP_CALCAIRE = 5,
    DEEP_GRANITE = 6,
    DEEP_EAU = 7
};

std::vector<TileRule> createGeologyRules()
{
    std::vector<TileRule> tiles(8);

    // 1--- Définition des "tuiles"

    // 0: AIR
    tiles[AIR].id = AIR;
    tiles[AIR].baseWeight = 10.0f;
    tiles[AIR].color[0] = 0.0f;
    tiles[AIR].color[1] = 0.0f;
    tiles[AIR].color[2] = 0.0f;

    // 1: SURFACE FORET
    tiles[SURFACE_FORET].id = SURFACE_FORET;
    tiles[SURFACE_FORET].baseWeight = 1.0f;
    tiles[SURFACE_FORET].color[0] = 0.05f;
    tiles[SURFACE_FORET].color[1] = 0.5f;
    tiles[SURFACE_FORET].color[2] = 0.1f;

    // 2: SURFACE HERBE
    tiles[SURFACE_HERBE].id = SURFACE_HERBE;
    tiles[SURFACE_HERBE].baseWeight = 1.0f;
    tiles[SURFACE_HERBE].color[0] = 0.2f;
    tiles[SURFACE_HERBE].color[1] = 0.8f;
    tiles[SURFACE_HERBE].color[2] = 0.2f;

    // 3: SURFACE SABLE
    tiles[SURFACE_SABLE].id = SURFACE_SABLE;
    tiles[SURFACE_SABLE].baseWeight = 1.0f;
    tiles[SURFACE_SABLE].color[0] = 0.9f;
    tiles[SURFACE_SABLE].color[1] = 0.8f;
    tiles[SURFACE_SABLE].color[2] = 0.5f;

    // 4: SURFACE ROCHE
    tiles[SURFACE_ROCHE].id = SURFACE_ROCHE;
    tiles[SURFACE_ROCHE].baseWeight = 1.0f;
    tiles[SURFACE_ROCHE].color[0] = 0.5f;
    tiles[SURFACE_ROCHE].color[1] = 0.5f;
    tiles[SURFACE_ROCHE].color[2] = 0.5f;

    // 5: DEEP CALCAIRE
    tiles[DEEP_CALCAIRE].id = DEEP_CALCAIRE;
    tiles[DEEP_CALCAIRE].baseWeight = 1.0f;
    tiles[DEEP_CALCAIRE].color[0] = 0.6f;
    tiles[DEEP_CALCAIRE].color[1] = 0.55f;
    tiles[DEEP_CALCAIRE].color[2] = 0.4f;

    // 6: DEEP GRANITE
    tiles[DEEP_GRANITE].id = DEEP_GRANITE;
    tiles[DEEP_GRANITE].baseWeight = 1.0f;
    tiles[DEEP_GRANITE].color[0] = 0.3f;
    tiles[DEEP_GRANITE].color[1] = 0.3f;
    tiles[DEEP_GRANITE].color[2] = 0.35f;

    // 7: DEEP EAU
    tiles[DEEP_EAU].id = DEEP_EAU;
    tiles[DEEP_EAU].baseWeight = 0.0f;
    tiles[DEEP_EAU].color[0] = 0.2f;
    tiles[DEEP_EAU].color[1] = 0.4f;
    tiles[DEEP_EAU].color[2] = 0.9f;

    // 2--- Règles d'adjacence

    // Horizontalement : tout le monde peut toucher tout le monde
    // Le tri se fera par les poids (WeightFunction) et non par interdiction stricte ici
    for (int i = 0; i < 8; i++)
    {
        for (int dir : {0, 1, 4, 5})
        {
            tiles[i].validNeighbors[dir] = {
                AIR, SURFACE_FORET, SURFACE_HERBE, SURFACE_SABLE, SURFACE_ROCHE,
                DEEP_CALCAIRE, DEEP_GRANITE, DEEP_EAU};
        }
    }

    // Verticalement : on définit la stratification

    // AIR
    tiles[AIR].validNeighbors[3] = {AIR};                                                             // Au-dessus : Air
    tiles[AIR].validNeighbors[2] = {AIR, SURFACE_FORET, SURFACE_HERBE, SURFACE_SABLE, SURFACE_ROCHE}; // En-dessous : Air ou Surface

    // SURFACES
    for (int id : {SURFACE_FORET, SURFACE_HERBE, SURFACE_SABLE, SURFACE_ROCHE})
    {
        tiles[id].validNeighbors[3] = {AIR};                                   // Au-dessus : Air obligatoire
        tiles[id].validNeighbors[2] = {DEEP_CALCAIRE, DEEP_GRANITE, DEEP_EAU}; // En-dessous : Sous-sol
    }

    // SOUS-SOL
    for (int id : {DEEP_CALCAIRE, DEEP_GRANITE, DEEP_EAU})
    {
        // Au-dessus : une surface ou d'autres couches sous-sol
        tiles[id].validNeighbors[3] = {
            SURFACE_FORET, SURFACE_HERBE, SURFACE_SABLE, SURFACE_ROCHE,
            DEEP_CALCAIRE, DEEP_GRANITE, DEEP_EAU};
        // En-dessous : Encore du sous-sol
        tiles[id].validNeighbors[2] = {DEEP_CALCAIRE, DEEP_GRANITE, DEEP_EAU};
    }

    return tiles;
}

void runWaterSimulation()
{
    std::cout << "Passe 2 : Simulation Hydrologique..." << std::endl;

    // 1. "Pluie"
    // On ajoute de l'eau sur le voxel le plus haut (surface)
    for (int x = 0; x < GRID_SIZE; x++)
    {
        for (int z = 0; z < GRID_SIZE; z++)
        {
            int h = worldData[x][0][z].surfaceLevel; // On récupère la hauteur stockée
            if (h > 0 && h < GRID_HEIGHT)
            {
                worldData[x][h][z].waterAmount = 1.0f; // Il pleut sur la surface
            }
        }
    }

    // 2. Phase de propagation
    // On fait couler l'eau pendant N itérations
    for (int iter = 0; iter < 32; iter++)
    {
        // On parcourt de bas en haut pour éviter de téléporter l'eau instantanément au fond
        for (int x = 0; x < GRID_SIZE; x++)
        {
            for (int z = 0; z < GRID_SIZE; z++)
            {
                for (int y = 0; y < GRID_HEIGHT - 1; y++)
                {

                    VoxelData &current = worldData[x][y + 1][z]; // Voxel du dessus
                    VoxelData &below = worldData[x][y][z];       // Voxel du dessous

                    if (current.waterAmount > 0.01f)
                    {
                        // Calcul de la perméabilité du sol en dessous
                        // Hardness 1.0 (Granite) = Perméabilité 0.0
                        // Hardness 0.0 (Calcaire) = Perméabilité 1.0
                        float permeability = 1.0f - below.hardness*1.2f;

                        // Si le sol est perméable et n'est pas déjà plein d'eau
                        if (permeability > 0.1f && below.waterAmount < 1.0f)
                        {
                            float flow = current.waterAmount * permeability * 0.5f; // Vitesse de flux

                            below.waterAmount += flow;
                            current.waterAmount -= flow;
                        }
                    }
                }
            }
        }
    }

    // 3. Phase de capillarité
    // On calcule l'humidité de surface en fonction de la distance à l'eau accumulée
    for (int x = 0; x < GRID_SIZE; x++)
    {
        for (int z = 0; z < GRID_SIZE; z++)
        {
            int surfaceY = worldData[x][0][z].surfaceLevel;

            // On cherche la nappe phréatique la plus proche en dessous
            int waterTableY = -1;
            for (int y = surfaceY; y >= 0; y--)
            {
                if (worldData[x][y][z].waterAmount > 0.2f)
                { // Seuil de saturation
                    waterTableY = y;
                    break;
                }
            }

            // Calcul de l'humidité de surface
            if (waterTableY != -1)
            {
                int dist = surfaceY - waterTableY;
                // Formule inverse : plus c'est proche, plus c'est humide
                worldData[x][surfaceY][z].humidity = 1.0f / (float)(dist * 0.25f + 1.0f);
            }
            else
            {
                worldData[x][surfaceY][z].humidity = 0.0f;
            }
        }
    }
}

float cubeVertices[] = {
    // Face Avant (Normale 0, 0, 1)
    -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
    0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
    0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
    0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
    -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
    -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,

    // Face Arrière (Normale 0, 0, -1)
    -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
    -0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
    0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
    0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
    0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
    -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,

    // Face Gauche (Normale -1, 0, 0)
    -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
    -0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
    -0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
    -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f,

    // Face Droite (Normale 1, 0, 0)
    0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
    0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
    0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
    0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
    0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
    0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,

    // Face Bas (Normale 0, -1, 0)
    -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
    0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
    0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
    0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
    -0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
    -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,

    // Face Haut (Normale 0, 1, 0)
    -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
    -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
    0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
    0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
    0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
    -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f};

// Shaders
const char *vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aNormal;
    layout (location = 2) in vec3 aOffset;
    layout (location = 3) in vec4 aColor;

    out vec4 Color;
    out vec3 Normal;
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main() {
        Color = aColor;
        Normal = aNormal;
        gl_Position = projection * view * model * vec4(aPos + aOffset, 1.0);
    }
)";

const char *fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    in vec4 Color;
    in vec3 Normal;

    void main() {
        vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3)); // Soleil venant de haut-droite
        
        vec3 finalRGB;

        if (Color.a < 0.95) {
            float diff = max(dot(Normal, lightDir), 0.0);
            float waterLight = 0.8 + (diff * 0.2); 
            
            finalRGB = Color.rgb * waterLight;
        } 
        else {
            float diff = max(dot(Normal, lightDir), 0.0);
            float ambient = 0.3;
            
            finalRGB = (ambient + diff) * Color.rgb;
        }

        FragColor = vec4(finalRGB, Color.a);
    }
)";

std::vector<PlantInstance> worldPlants;

float randomFloat()
{
    return (float)rand() / (float)RAND_MAX;
}

void generateVegetation(WFCEngine &wfc)
{
    std::cout << "Passe 4 : Generation Vegetation..." << std::endl;
    worldPlants.clear();
    initPlantModels();

    for (int x = 0; x < GRID_SIZE; x++)
    {
        for (int z = 0; z < GRID_SIZE; z++)
        {
            // Trouver la surface
            int surfaceY = worldData[x][0][z].surfaceLevel;

            // On récupère le bloc WFC à la surface
            const Cell &c = wfc.getCell(x, surfaceY, z);
            if (!c.isCollapsed())
                continue;

            int tileId = c.collapsedTile;
            float humidity = worldData[x][surfaceY][z].humidity;

            PlantType plantToPlace = PLANT_NONE;
            float r = randomFloat();

            // Règles de distribution des plantes

            // Cas 1 : FORET
            if (tileId == SURFACE_FORET)
            {
                // Forte densité d'arbres
                if (r < 0.025f)
                {
                    // Sélection de l'espèce selon l'humidité
                    if (humidity > 0.7f)
                        plantToPlace = TREE_WILLOW; // Très humide
                    else if (humidity < 0.3f)
                        plantToPlace = TREE_PINE; // Sec
                    else
                        plantToPlace = TREE_OAK; // Standard
                }
                // Sinon des buissons
                else if (r < 0.05f)
                {
                    plantToPlace = BUSH_BERRY;
                }
            }

            // Cas 2 : HERBE
            else if (tileId == SURFACE_HERBE)
            {
                // Arbres rares
                if (r < 0.01f)
                {
                    plantToPlace = TREE_OAK;
                }
                // Buissons ou Herbes hautes
                else if (r < 0.02f)
                {
                    plantToPlace = BUSH_BERRY;
                }
            }

            // Cas 3 : SABLE
            else if (tileId == SURFACE_SABLE)
            {
                // Cactus
                if (r < 0.01f)
                {
                    plantToPlace = CACTUS;
                }
                // Arbustes secs
                else if (r < 0.02f && humidity > 0.4f)
                {
                    plantToPlace = BUSH_BERRY;
                }
            }

            // Cas 4 : ROCHE
            else if (tileId == SURFACE_ROCHE)
            {
                // Pin alpin accroché à la roche (1% chance)
                if (r < 0.01f)
                {
                    plantToPlace = TREE_PINE;
                }
            }

            // --- PLACEMENT ---
            if (plantToPlace != PLANT_NONE)
            {
                worldPlants.push_back({x, surfaceY, z, plantToPlace});
            }
        }
    }
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    g_CameraDistance -= (float)yoffset * 2.0f;
    if (g_CameraDistance < 5.0f)
        g_CameraDistance = 5.0f;
}
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            g_MousePressed = true;
            glfwGetCursorPos(window, &g_LastMouseX, &g_LastMouseY);
        }
        else if (action == GLFW_RELEASE)
            g_MousePressed = false;
    }
}
void cursor_position_callback(GLFWwindow *window, double xpos, double ypos)
{
    if (g_MousePressed)
    {
        float dx = (float)(xpos - g_LastMouseX);
        float dy = (float)(ypos - g_LastMouseY);
        g_CameraAngleY += dx * 0.5f;
        g_CameraAngleX += dy * 0.5f;
        g_LastMouseX = xpos;
        g_LastMouseY = ypos;
    }
}

// Options d'affichage
bool showBlockTypes[8] = {true, true, true, true, true, true, true, true}; // Pour les 8 IDs
bool showWaterMode = false;                                                // Mode visualisation nappes
// IDs pour rappel :
// 0:AIR, 1:FORET, 2:HERBE, 3:SABLE, 4:ROCHE, 5:CALCAIRE, 6:GRANITE, 7:EAU

int main()
{
    // Init OpenGL
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow *window = glfwCreateWindow(1000, 800, "WFC", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // Set callbacks
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    // Shaders
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    unsigned int seed = std::chrono::steady_clock::now().time_since_epoch().count();
    PerlinNoise noise(seed);

    std::cout << "Passe 1 : Generation Geologie et Topographie..." << std::endl;

    for (int x = 0; x < GRID_SIZE; x++)
    {
        for (int z = 0; z < GRID_SIZE; z++)
        {

            // Topographie
            // On garde une marge en haut et en bas
            double n = noise.fractal(x, z, 16, 0.5, 0.05);
            int height = 8 + (int)(n * (GRID_HEIGHT - 12));
            if (height < 2)
                height = 2;
            if (height >= GRID_HEIGHT - 2)
                height = GRID_HEIGHT - 3;

            for (int y = 0; y < GRID_HEIGHT; y++)
            {
                VoxelData &data = worldData[x][y][z];

                data.surfaceLevel = height;

                // Densité (Air vs Sol)
                if (y > height) data.density = 0.0f; // Ciel
                else data.density = 1.0f;            // Sol

                // Dureté du sous-sol
                // Bruit 3D : x*0.15 donne de grandes veines
                double hardNoise = noise.noise(x * 0.15, y * 0.15, z * 0.15);
                data.hardness = (float)(hardNoise * 0.5 + 0.5);
            }
        }
    }

    // Appel hydro
    runWaterSimulation();

    // Passe 3 : WFC
    // On crée les règles manuelles
    auto rules = createGeologyRules();

    std::cout << "Passe 3 : Initialisation WFC..." << std::endl;
    WFCEngine wfc(GRID_SIZE, GRID_HEIGHT, GRID_SIZE, rules, seed);

    // Mise en place des biais de génération
    wfc.setWeightFunction([&](int tileId, int x, int y, int z) -> float
                          {
        const VoxelData& data = worldData[x][y][z];

        float EPSILON = 0.0001f;

        // Ciel
        if (y > data.surfaceLevel)
            return (tileId == AIR) ? 100.0f : EPSILON;
        
        // Sous-sol
        if (y < data.surfaceLevel) {
            if (tileId == SURFACE_FORET || tileId == SURFACE_SABLE || tileId == SURFACE_HERBE || tileId == SURFACE_ROCHE)
                return 0.0f;

            if (tileId == AIR)
                return 0.f;
            if (data.waterAmount > 0.8f && tileId == DEEP_EAU) return 20.0f;
            
            if (data.hardness > 0.6f)
                return (tileId == DEEP_GRANITE) ? 10.0f : EPSILON;
            else
                return (tileId == DEEP_CALCAIRE) ? 10.0f : EPSILON;
        }

        // Surface (dépend de l'humidité calculée par la simulation passe 2)
        if (y == data.surfaceLevel) {
            if (tileId == AIR)
                return 0.f;

            if (data.hardness > 0.7f) {
                return (tileId == SURFACE_ROCHE) ? 10.0f : EPSILON;
            }

            float h = data.humidity;
            if (h > 0.6f) {
                // Zone Humide -> Forêt
                if (tileId == SURFACE_FORET) return 20.0f;
                if (tileId == SURFACE_HERBE) return 5.0f;
                return EPSILON;
            } 
            else if (h > 0.3f) {
                // Zone Tempérée -> Herbe
                if (tileId == SURFACE_HERBE) return 20.0f;
                if (tileId == SURFACE_FORET) return 2.0f;
                if (tileId == SURFACE_SABLE) return 2.0f;
                return EPSILON;
            } 
            else {
                // Zone Sèche -> Sable
                if (tileId == SURFACE_SABLE) return 20.0f;
                if (tileId == SURFACE_HERBE) return 1.0f;
                return EPSILON;
            }
        }
        return 1.0f;
    });

    // Buffers OpenGL
    unsigned int cubeVAO, cubeVBO, instanceVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &instanceVBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    // Attribs sommets + normales
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Buffer Instances
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    // Taille : GRID^3 * (3 floats Pos + 4 floats Color) = 7 floats par instance
    glBufferData(GL_ARRAY_BUFFER, GRID_SIZE * GRID_HEIGHT * GRID_SIZE * 7 * sizeof(float), NULL, GL_DYNAMIC_DRAW);

    // Attribut 2 : Position (Offset)
    glEnableVertexAttribArray(2);
    // Stride = 7 * sizeof(float)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void *)0);
    glVertexAttribDivisor(2, 1);

    // Attribut 3 : Couleur (RGBA) -> vec4
    glEnableVertexAttribArray(3);
    // Taille = 4, Stride = 7, Offset = 3
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void *)(3 * sizeof(float)));
    glVertexAttribDivisor(3, 1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Boucle principale
    while (!glfwWindowShouldClose(window))
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
        // Toggle mode nappes (Touche W)
        static bool wPressed = false;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        {
            if (!wPressed)
            {
                showWaterMode = !showWaterMode;
                wPressed = true;
            }
        }
        else
            wPressed = false;

        // Toggle Surface (Touche 1) : Active/Désactive les IDs 1, 2, 3, 4
        static bool k1Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
        {
            if (!k1Pressed)
            {
                bool state = !showBlockTypes[1];
                for (int i = 1; i <= 4; i++)
                    showBlockTypes[i] = state;
                k1Pressed = true;
            }
        }
        else
            k1Pressed = false;

        // Toggle Sous-sol (Touche 2) : Active/Désactive les IDs 5, 6, 7
        static bool k2Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
        {
            if (!k2Pressed)
            {
                bool state = !showBlockTypes[5];
                for (int i = 5; i <= 7; i++)
                    showBlockTypes[i] = state;
                k2Pressed = true;
            }
        }
        else
            k2Pressed = false;

        // Toggle Granite (Impermeable) seul (Touche 3)
        static bool k3Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
        {
            if (!k3Pressed)
            {
                showBlockTypes[DEEP_GRANITE] = !showBlockTypes[DEEP_GRANITE];
                k3Pressed = true;
            }
        }
        else
            k3Pressed = false;

        // Active la génération de végétation (mieux vaut attendre la fin du reste)
        static bool vPressed = false;
        if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS)
        {
            if (!vPressed)
            {
                generateVegetation(wfc);
                vPressed = true;
            }
        }
        else
            vPressed = false;

        // Rendu
        if (!wfc.isFailed())
        {
            for (int i = 0; i < 100; i++)
                wfc.step();
        }

        if (wfc.isFailed())
            glClearColor(0.5f, 0.0f, 0.0f, 1.0f); // Rouge = Erreur
        else
            glClearColor(0.5f, 0.7f, 1.0f, 1.0f); // Bleu = OK

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);

        // Caméra orbitale simple
        glm::mat4 view = glm::mat4(1.0f);
        view = glm::translate(view, glm::vec3(0.0f, 0.0f, -g_CameraDistance));
        view = glm::rotate(view, glm::radians(g_CameraAngleX), glm::vec3(1.0f, 0.0f, 0.0f));
        view = glm::rotate(view, glm::radians(g_CameraAngleY), glm::vec3(0.0f, 1.0f, 0.0f));
        // Centrage
        view = glm::translate(view, glm::vec3(-GRID_SIZE / 2.0f, -GRID_HEIGHT / 2.0f, -GRID_SIZE / 2.0f));

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, &glm::perspective(glm::radians(45.0f), 1000.0f / 800.0f, 0.1f, 200.0f)[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, &glm::mat4(1.0f)[0][0]);

        // Construction du buffer d'affichage
        std::vector<float> opaqueData;
        std::vector<float> transparentData;
        int opaqueCount = 0;
        int transparentCount = 0;

        for (int x = 0; x < GRID_SIZE; x++)
        {
            for (int y = 0; y < GRID_HEIGHT; y++)
            {
                for (int z = 0; z < GRID_SIZE; z++)
                {

                    const Cell &c = wfc.getCell(x, y, z);
                    int tid = AIR;

                    // Récupération ID (avec Fallback)
                    if (c.isCollapsed())
                        tid = c.collapsedTile;
                    else if ((false)&&(y <= worldData[x][y][z].surfaceLevel))
                        tid = DEEP_CALCAIRE;

                    if (tid == AIR)
                        continue;
                    if (!showBlockTypes[tid])
                        continue;

                    // Calcul Couleur & Alpha
                    const TileRule &t = wfc.getTile(tid);
                    float r = t.color[0], g = t.color[1], b = t.color[2], a = 1.0f;

                    bool isTransparent = false;

                    // Mode nappes
                    if (showWaterMode)
                    {
                        const VoxelData &data = worldData[x][y][z];
                        if (tid == DEEP_GRANITE)
                        {
                            // Granite reste opaque
                        }
                        else
                        {
                            if (data.waterAmount > 0.05f)
                            {
                                // HUMIDE -> Transparence selon le taux
                                r = 0.0f;
                                g = 0.5f;
                                b = 1.0f;
                                a = std::min(data.waterAmount * 0.8f + 0.2f, 0.8f);
                                isTransparent = true;
                            }
                            else
                            {
                                // SEC -> Transparent (quasi invisible)
                                r = 0.8f;
                                g = 0.8f;
                                b = 0.8f;
                                a = 0.05f;
                                isTransparent = true;
                            }
                        }
                    }
                    else
                    {
                        // Mode Normal : l'Eau profonde est transparente (si utilisée)
                        if (tid == DEEP_EAU)
                        {
                            a = 0.6f;
                            isTransparent = true;
                        }
                    }

                    // Remplissage des vecteurs
                    if (isTransparent)
                    {
                        transparentData.push_back((float)x);
                        transparentData.push_back((float)y);
                        transparentData.push_back((float)z);
                        transparentData.push_back(r);
                        transparentData.push_back(g);
                        transparentData.push_back(b);
                        transparentData.push_back(a);
                        transparentCount++;
                    }
                    else
                    {
                        opaqueData.push_back((float)x);
                        opaqueData.push_back((float)y);
                        opaqueData.push_back((float)z);
                        opaqueData.push_back(r);
                        opaqueData.push_back(g);
                        opaqueData.push_back(b);
                        opaqueData.push_back(a);
                        opaqueCount++;
                    }
                }
            }
        }

            for (const auto &plant : worldPlants)
            {
                // Récupérer le modèle
                if (plantModels.find(plant.type) == plantModels.end())
                    continue;
                const VoxelModel &model = plantModels[plant.type];

                // Pour chaque voxel de l'arbre
                for (const auto &part : model.parts)
                {
                    // Position absolue = Position Plante + Position Relative
                    float px = (float)(plant.x + part.dx);
                    float py = (float)(plant.y + part.dy);
                    float pz = (float)(plant.z + part.dz);

                    // Couleur
                    float r = part.r;
                    float g = part.g;
                    float b = part.b;
                    float a = 1.0f;

                    // On ajoute au tableau Opaque
                    opaqueData.push_back(px);
                    opaqueData.push_back(py);
                    opaqueData.push_back(pz);
                    opaqueData.push_back(r);
                    opaqueData.push_back(g);
                    opaqueData.push_back(b);
                    opaqueData.push_back(a);
                    opaqueCount++;
                }
            }

            // Dessin des voxels opaques
            if (opaqueCount > 0)
            {
                glDepthMask(GL_TRUE);
                glDisable(GL_BLEND);

                glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, opaqueData.size() * sizeof(float), opaqueData.data());

                glBindVertexArray(cubeVAO);
                glDrawArraysInstanced(GL_TRIANGLES, 0, 36, opaqueCount);
            }

            // Dessin des voxels transparents
            if (transparentCount > 0)
            {
                glDepthMask(GL_FALSE);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, transparentData.size() * sizeof(float), transparentData.data());

                glBindVertexArray(cubeVAO);
                glDrawArraysInstanced(GL_TRIANGLES, 0, 36, transparentCount);

                // Restauration des états par défaut pour la prochaine frame
                glDepthMask(GL_TRUE);
                glEnable(GL_CULL_FACE);
                glDisable(GL_BLEND);
            }

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

    glfwTerminate();
    return 0;
}