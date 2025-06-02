#include <GL/glut.h>
#include <GL/glu.h>
#include <cmath>
#include <iostream>
#include <vector>
#include <stdlib.h>
#include <ctime>
#include <string>
#include <sstream>
#include <iomanip>
#include "imageloader.h" // Include the header for Image and loadBMP

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

GLuint sphereTextureID;

void initSphereTexture() {
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &sphereTextureID);
    glBindTexture(GL_TEXTURE_2D, sphereTextureID);
    Image* image = loadBMP("khoirul.bmp");
    if (!image) {
        std::cerr << "Failed to load texture: khoirul.bmp. Using fallback color." << std::endl;
        glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture if failed
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image->width, image->height, 0, GL_RGB, GL_UNSIGNED_BYTE, image->pixels);
        delete image;
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
}

void drawTexturedSphere(float radius, int slices, int stacks) {
    GLUquadricObj* quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, sphereTextureID);
    glColor3f(0.5f, 0.3f, 0.3f); // Fallback color if texture fails
    gluSphere(quad, radius, slices, stacks);
    glDisable(GL_TEXTURE_2D);
    gluDeleteQuadric(quad);
}

struct Star {
    float x, y, z;
    float r, g, b;
    float size;
};

const int MAX_STARS = 50000;
Star stars[MAX_STARS];
float starfieldDepth = 100.0f;

void initStars() {
    for (int i = 0; i < MAX_STARS; ++i) {
        stars[i].x = (rand() % 2000 / 10.0f) - 100.0f;
        stars[i].y = (rand() % 2000 / 10.0f) - 100.0f;
        stars[i].z = -(rand() % (int)(starfieldDepth * 2));
        float brightness = 0.5f + (rand() % 50 / 100.0f);
        stars[i].r = brightness;
        stars[i].g = brightness;
        stars[i].b = brightness;
        if (rand() % 10 == 0) {
            stars[i].r = 0.8f + (rand() % 20 / 100.0f);
            stars[i].g = 0.8f + (rand() % 20 / 100.0f);
            stars[i].b = 1.0f;
        }
        stars[i].size = 1.0f + (rand() % 20 / 10.0f);
    }
}

void drawStars() {
    GLboolean lightingWasEnabled = glIsEnabled(GL_LIGHTING);
    if (lightingWasEnabled) glDisable(GL_LIGHTING);
    glPointSize(1.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < MAX_STARS; ++i) {
        glColor3f(stars[i].r, stars[i].g, stars[i].b);
        glVertex3f(stars[i].x, stars[i].y, stars[i].z);
    }
    glEnd();
    if (lightingWasEnabled) glEnable(GL_LIGHTING);
}

const float ROCKET_BODY_RADIUS = 0.4f;
const float ROCKET_BODY_HEIGHT = 2.5f;
const float ROCKET_NOSE_RADIUS = ROCKET_BODY_RADIUS;
const float ROCKET_NOSE_HEIGHT = ROCKET_BODY_RADIUS * 4.0f;
const float ROCKET_THRUSTER_BASE_RADIUS = ROCKET_BODY_RADIUS * 0.8f;
const float ROCKET_THRUSTER_HEIGHT = 1.5f;
const float FIN_WIDTH = ROCKET_BODY_RADIUS * 0.1f;
const float FIN_HEIGHT = ROCKET_BODY_RADIUS * 1.5f;
const float FIN_LENGTH = ROCKET_BODY_HEIGHT * 0.4f;

float flameRotationAngle = 0.0f;
const float FLAME_ROTATION_SPEED = 180.0f;

float rocketX = 0.0f;
float rocketY = 0.0f;
float rocketZ = 0.0f;
float rocketSpeed = 5.0f;
float rocketAcceleration = 0.1f;

float targetRocketX = 0.0f;
float targetRocketY = 0.0f;
float currentRocketX = 0.0f;
float currentRocketY = 0.0f;
float currentRollAngle = 0.0f;
float targetRollAngle = 0.0f;
float rollDuration = 0.3f;
float rollTimer = 0.0f;
const float MAX_ROLL_ANGLE = 45.0f;
const float MOVEMENT_SMOOTHNESS = 12.0f;

const float SPAWN_DISTANCE_Z_AHEAD = 70.0f;
const float RECYCLE_DISTANCE_BEHIND_PLAYER = 10.0f;
const float LANE_WIDTH = 2.5f;
const int NUM_LANES = 5;
float SPAWN_INTERVAL = 1.0f;
float rocketCollisionRadius = 0.6f;
bool gameOver = false;
int score = 0;
bool isBoosting = false;
const float BOOST_FACTOR = 5.0f;

struct GameObject {
    enum ObjectType {
        TYPE_OBSTACLE_CUBE,
        TYPE_OBSTACLE_SPHERE,
        TYPE_ITEM_RING
    };
    ObjectType type;
    float x, y, z;
    float sizeX, sizeY, sizeZ;
    float radius;
    float innerRadius;
    float colorR, colorG, colorB;
    bool isActive;
    GameObject() : isActive(false) {}
};

const int MAX_GAME_OBJECTS = 20;
GameObject gameObjects[MAX_GAME_OBJECTS];

int currentLane = (NUM_LANES - 1) / 2;
int previousLane = currentLane;
const float ROCKET_Y_STEP = 0.3f;
const float MIN_ROCKET_Y = -1.5f;
const float MAX_ROCKET_Y = 1.5f;

float lerp(float current, float target, float deltaTime, float speed) {
    float diff = target - current;
    if (fabs(diff) < 0.01f) return target;
    return current + diff * speed * deltaTime;
}

void drawCustomFin(float root_chord, float tip_chord, float height, float sweep_offset, float thickness) {
    GLfloat v[8][3];
    v[0][0] = 0; v[0][1] = -root_chord / 2.0f; v[0][2] = -thickness / 2.0f;
    v[1][0] = 0; v[1][1] = root_chord / 2.0f; v[1][2] = -thickness / 2.0f;
    v[2][0] = height; v[2][1] = tip_chord / 2.0f - sweep_offset; v[2][2] = -thickness / 2.0f;
    v[3][0] = height; v[3][1] = -tip_chord / 2.0f - sweep_offset; v[3][2] = -thickness / 2.0f;
    for (int i = 0; i < 4; ++i) {
        v[i+4][0] = v[i][0];
        v[i+4][1] = v[i][1];
        v[i+4][2] = thickness / 2.0f;
    }
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3fv(v[0]); glVertex3fv(v[1]); glVertex3fv(v[2]); glVertex3fv(v[3]);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3fv(v[7]); glVertex3fv(v[6]); glVertex3fv(v[5]); glVertex3fv(v[4]);
    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3fv(v[3]); glVertex3fv(v[2]); glVertex3fv(v[6]); glVertex3fv(v[7]);
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3fv(v[4]); glVertex3fv(v[5]); glVertex3fv(v[1]); glVertex3fv(v[0]);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3fv(v[1]); glVertex3fv(v[5]); glVertex3fv(v[6]); glVertex3fv(v[2]);
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3fv(v[0]); glVertex3fv(v[3]); glVertex3fv(v[7]); glVertex3fv(v[4]);
    glEnd();
}

void drawThrusterFlame() {
    float flame_base_radius = ROCKET_THRUSTER_BASE_RADIUS * 0.85f;
    float flame_length = ROCKET_BODY_HEIGHT * 0.8f;
    int segments = 16;
    int num_flame_layers = 3;
    GLfloat hot_color_base[] = {1.0f, 1.0f, 0.7f};
    GLfloat mid_color_base[] = {1.0f, 0.6f, 0.0f};
    GLfloat cool_color_tip[] = {1.0f, 0.2f, 0.0f};
    GLboolean lighting_was_enabled;
    glGetBooleanv(GL_LIGHTING, &lighting_was_enabled);
    if (lighting_was_enabled) glDisable(GL_LIGHTING);
    GLboolean depth_mask_was_enabled;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask_was_enabled);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (int layer = 0; layer < num_flame_layers; ++layer) {
        glPushMatrix();
        float current_layer_rotation = flameRotationAngle * (1.0f + layer * 0.3f);
        glRotatef(current_layer_rotation, 0.0f, 0.0f, 1.0f);
        float alpha_multiplier = 1.0f - (float)layer / num_flame_layers * 0.6f;
        float size_multiplier = 1.0f + layer * 0.1f;
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(cool_color_tip[0], cool_color_tip[1], cool_color_tip[2], 0.3f * alpha_multiplier);
        glVertex3f(0.0f, 0.0f, flame_length * size_multiplier);
        for (int i = 0; i <= segments; ++i) {
            float angle_rad = (float)i / (float)segments * (2.0f * M_PI);
            float r_base = flame_base_radius * size_multiplier;
            float x = cos(angle_rad) * r_base;
            float y = sin(angle_rad) * r_base;
            if ((i + layer) % 2 == 0) {
                glColor4f(hot_color_base[0], hot_color_base[1], hot_color_base[2], 0.9f * alpha_multiplier);
            } else {
                glColor4f(mid_color_base[0], mid_color_base[1], mid_color_base[2], 0.7f * alpha_multiplier);
            }
            glVertex3f(x, y, 0.0f);
        }
        glEnd();
        glPopMatrix();
    }
    glDisable(GL_BLEND);
    if (depth_mask_was_enabled) glDepthMask(GL_TRUE);
    if (lighting_was_enabled) glEnable(GL_LIGHTING);
}

void drawRocket() {
    glPushMatrix();
    glRotatef(currentRollAngle, 0.0f, 0.0f, 1.0f);
    glColor3f(0.75f, 0.75f, 0.75f);
    GLfloat mat_diffuse[] = { 0.75f, 0.75f, 0.75f, 1.0f };
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, ROCKET_BODY_HEIGHT / 2.0f);
    glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
    GLUquadricObj* cylinderQuad = gluNewQuadric();
    gluCylinder(cylinderQuad, ROCKET_BODY_RADIUS, ROCKET_BODY_RADIUS, ROCKET_BODY_HEIGHT, 32, 10);
    gluDisk(cylinderQuad, 0.0f, ROCKET_BODY_RADIUS, 32, 1);
    glTranslatef(0.0f, 0.0f, ROCKET_BODY_HEIGHT);
    gluDisk(cylinderQuad, 0.0f, ROCKET_BODY_RADIUS, 32, 1);
    gluDeleteQuadric(cylinderQuad);
    glPopMatrix();
    glColor3f(1.0f, 0.2f, 0.2f);
    GLfloat nose_diffuse[] = { 1.0f, 0.2f, 0.2f, 1.0f };
    glMaterialfv(GL_FRONT, GL_DIFFUSE, nose_diffuse);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -(ROCKET_BODY_HEIGHT / 2.0f));
    glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
    GLUquadricObj* noseQuad = gluNewQuadric();
    gluCylinder(noseQuad, ROCKET_NOSE_RADIUS, 0.0f, ROCKET_NOSE_HEIGHT, 32, 10);
    gluDeleteQuadric(noseQuad);
    glPopMatrix();
    glColor3f(0.8f, 0.4f, 0.1f);
    GLfloat thruster_diffuse[] = { 0.8f, 0.4f, 0.1f, 1.0f };
    glMaterialfv(GL_FRONT, GL_DIFFUSE, thruster_diffuse);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, ROCKET_BODY_HEIGHT / 1.8f);
    glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
    GLUquadricObj* thrusterQuad = gluNewQuadric();
    gluCylinder(thrusterQuad, ROCKET_THRUSTER_BASE_RADIUS, 0.0f, ROCKET_THRUSTER_HEIGHT, 32, 5);
    gluDeleteQuadric(thrusterQuad);
    glPopMatrix();
    glColor3f(0.1f, 0.1f, 0.2f);
    GLfloat inner_thruster_diffuse[] = { 0.1f, 0.1f, 0.2f, 1.0f };
    glMaterialfv(GL_FRONT, GL_DIFFUSE, inner_thruster_diffuse);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, (ROCKET_BODY_HEIGHT / 1.8f)*1.01f);
    glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
    GLUquadricObj* innerThrusterQuad = gluNewQuadric();
    gluCylinder(innerThrusterQuad, ROCKET_THRUSTER_BASE_RADIUS * 0.9f, 0.0f, ROCKET_THRUSTER_HEIGHT *0.9f, 32, 5);
    gluDeleteQuadric(innerThrusterQuad);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, (ROCKET_NOSE_HEIGHT/2.0f)*1.0f);
    drawThrusterFlame();
    glPopMatrix();
    glColor3f(1.0f, 0.2f, 0.2f);
    GLfloat fin_diffuse[] = { 1.0f, 0.2f, 0.2f, 1.0f };
    glMaterialfv(GL_FRONT, GL_DIFFUSE, fin_diffuse);
    for (int i = 0; i < 3; ++i) {
        glPushMatrix();
        glRotatef(i * (360.0f / 3.0f), 0.0f, 0.0f, 1.0f);
        glTranslatef(0.0f, ROCKET_BODY_RADIUS, ROCKET_BODY_HEIGHT * 0.25f);
        glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
        glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
        float fin_root_chord = FIN_LENGTH;
        float fin_tip_chord = FIN_LENGTH * 0.7f;
        float fin_radial_height = FIN_HEIGHT;
        float fin_sweep = FIN_LENGTH * 0.2f;
        float fin_thickness = FIN_WIDTH;
        drawCustomFin(fin_root_chord, fin_tip_chord, fin_radial_height, fin_sweep, fin_thickness);
        glPopMatrix();
    }
    glPopMatrix();
}

void initGameObjects() {
    for (int i = 0; i < MAX_GAME_OBJECTS; ++i) {
        gameObjects[i].isActive = false;
    }
    srand(time(NULL));
}

void spawnObject() {
    int spawnedObjectIndex = -1;
    for (int i = 0; i < MAX_GAME_OBJECTS; ++i) {
        if (!gameObjects[i].isActive) {
            spawnedObjectIndex = i;
            break;
        }
    }
    if (spawnedObjectIndex != -1) {
        GameObject& obj = gameObjects[spawnedObjectIndex];
        obj.isActive = true;
        int objectTypeRoll = rand() % 100;
        if (objectTypeRoll < 30) {
            obj.type = GameObject::TYPE_ITEM_RING;
        } else if (objectTypeRoll < 70) {
            obj.type = GameObject::TYPE_OBSTACLE_CUBE;
        } else {
            obj.type = GameObject::TYPE_OBSTACLE_SPHERE;
        }
        obj.z = rocketZ - SPAWN_DISTANCE_Z_AHEAD;
        int laneChoice = rand() % NUM_LANES;
        if (NUM_LANES == 1) {
            obj.x = 0.0f;
        } else {
            obj.x = (laneChoice - (NUM_LANES - 1) / 2.0f) * LANE_WIDTH;
        }
        obj.y = 0.0f;
        if (obj.type == GameObject::TYPE_ITEM_RING) {
            obj.y = (rand() % 3 - 1) * 0.5f;
        }
        switch (obj.type) {
            case GameObject::TYPE_OBSTACLE_CUBE:
                obj.sizeX = 1.0f + (rand() % 5 / 10.0f);
                obj.sizeY = 1.0f + (rand() % 5 / 10.0f);
                obj.sizeZ = 1.0f + (rand() % 5 / 10.0f);
                obj.colorR = 0.4f; obj.colorG = 0.4f; obj.colorB = 0.5f;
                break;
            case GameObject::TYPE_OBSTACLE_SPHERE:
                obj.radius = 0.5f + (rand() % 3 / 10.0f);
                obj.sizeX = obj.radius;
                obj.colorR = 0.5f; obj.colorG = 0.3f; obj.colorB = 0.3f;
                break;
            case GameObject::TYPE_ITEM_RING:
                obj.radius = 1.2f;
                obj.innerRadius = 0.15f;
                obj.colorR = 1.0f; obj.colorG = 0.85f; obj.colorB = 0.0f;
                break;
        }
    }
}

void checkCollisions() {
    if (gameOver) return;
    for (int i = 0; i < MAX_GAME_OBJECTS; ++i) {
        if (gameObjects[i].isActive) {
            GameObject& obj = gameObjects[i];
            float dx = rocketX - obj.x;
            float dy = rocketY - obj.y;
            float dz = rocketZ - obj.z;
            float distanceSquared = dx*dx + dy*dy + dz*dz;
            float objectCollisionRadius = 0.0f;
            if (obj.type == GameObject::TYPE_OBSTACLE_CUBE) {
                objectCollisionRadius = std::max(obj.sizeX, std::max(obj.sizeY, obj.sizeZ)) / 2.0f;
            } else if (obj.type == GameObject::TYPE_OBSTACLE_SPHERE) {
                objectCollisionRadius = obj.radius;
            } else if (obj.type == GameObject::TYPE_ITEM_RING) {
                objectCollisionRadius = obj.radius;
            }
            if (distanceSquared < (rocketCollisionRadius + objectCollisionRadius) * (rocketCollisionRadius + objectCollisionRadius)) {
                if (obj.type == GameObject::TYPE_OBSTACLE_CUBE || obj.type == GameObject::TYPE_OBSTACLE_SPHERE) {
                    std::cout << "TABRAKAN DENGAN RINTANGAN! Game Over." << std::endl;
                    gameOver = true;
                    obj.isActive = false;
                } else if (obj.type == GameObject::TYPE_ITEM_RING) {
                    score += 10;
                    std::cout << "ITEM CINCIN DIAMBIL! Skor: " << score << std::endl;
                    obj.isActive = false;
                }
            }
        }
    }
}

void renderText(float x, float y, void *font, const char *string, float r, float g, float b) {
    GLboolean isLightingOn = glIsEnabled(GL_LIGHTING);
    GLboolean isDepthTestOn = glIsEnabled(GL_DEPTH_TEST);
    if (isLightingOn) glDisable(GL_LIGHTING);
    if (isDepthTestOn) glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, glutGet(GLUT_WINDOW_WIDTH), 0, glutGet(GLUT_WINDOW_HEIGHT));
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glColor3f(r, g, b);
    glRasterPos2f(x, y);
    for (const char* c = string; *c != '\0'; c++) {
        glutBitmapCharacter(font, *c);
    }
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    if (isLightingOn) glEnable(GL_LIGHTING);
    if (isDepthTestOn) glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);
}

void handleKeyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 'a':
        case 'A':
            if (currentLane > 0) {
                previousLane = currentLane;
                currentLane--;
                targetRollAngle = MAX_ROLL_ANGLE;
                rollTimer = 0.0f;
            }
            break;
        case 'd':
        case 'D':
            if (currentLane < NUM_LANES - 1) {
                previousLane = currentLane;
                currentLane++;
                targetRollAngle = -MAX_ROLL_ANGLE;
                rollTimer = 0.0f;
            }
            break;
        case 'w':
        case 'W':
            targetRocketY += ROCKET_Y_STEP;
            if (targetRocketY > MAX_ROCKET_Y) targetRocketY = MAX_ROCKET_Y;
            break;
        case 's':
        case 'S':
            targetRocketY -= ROCKET_Y_STEP;
            if (targetRocketY < MIN_ROCKET_Y) targetRocketY = MIN_ROCKET_Y;
            break;
        case ' ':
            isBoosting = true;
            break;
        case 27:
            exit(0);
            break;
    }
    if (NUM_LANES == 1) {
        targetRocketX = 0.0f;
    } else {
        targetRocketX = (currentLane - (NUM_LANES - 1) / 2.0f) * LANE_WIDTH;
    }
}

void handleKeyboardUp(unsigned char key, int x, int y) {
    switch (key) {
        case ' ':
            isBoosting = false;
            break;
    }
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Camera setup
    float camOffsetX = 0.5f;
    float camOffsetY = 4.0f;
    float camOffsetZ = 10.0f;
    GLfloat eyeX = rocketX + camOffsetX;
    GLfloat eyeY = rocketY + camOffsetY;
    GLfloat eyeZ = rocketZ + camOffsetZ;
    GLfloat centerX = rocketX;
    GLfloat centerY = rocketY;
    GLfloat centerZ = rocketZ + 1.0f;
    gluLookAt(eyeX, eyeY, eyeZ, centerX, centerY, centerZ, 0.0, 1.0, 0.0);

    // Update light position to follow camera
    GLfloat light_position[] = { eyeX, eyeY, eyeZ, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);

    // Render game objects
    for (int i = 0; i < MAX_GAME_OBJECTS; ++i) {
        if (gameObjects[i].isActive) {
            glPushMatrix();
            glTranslatef(gameObjects[i].x, gameObjects[i].y, gameObjects[i].z);
            glColor3f(gameObjects[i].colorR, gameObjects[i].colorG, gameObjects[i].colorB);
            GLfloat obj_diffuse[] = { gameObjects[i].colorR, gameObjects[i].colorG, gameObjects[i].colorB, 1.0f };
            glMaterialfv(GL_FRONT, GL_DIFFUSE, obj_diffuse);
            switch (gameObjects[i].type) {
                case GameObject::TYPE_OBSTACLE_CUBE:
                    glPushMatrix();
                    glScalef(gameObjects[i].sizeX, gameObjects[i].sizeY, gameObjects[i].sizeZ);
                    glutSolidCube(1.0);
                    glPopMatrix();
                    break;
                case GameObject::TYPE_OBSTACLE_SPHERE:
                    drawTexturedSphere(gameObjects[i].radius, 20, 20);
                    break;
                case GameObject::TYPE_ITEM_RING:
                    glutSolidTorus(gameObjects[i].innerRadius, gameObjects[i].radius, 10, 32);
                    break;
            }
            glPopMatrix();
        }
    }

    // Render rocket
    glPushMatrix();
    glTranslatef(rocketX, rocketY, rocketZ);
    drawRocket();
    glPopMatrix();

    // Render score
    std::ostringstream scoreStringStream;
    scoreStringStream << "Score: " << score;
    std::string scoreText = scoreStringStream.str();
    float scoreX = 10.0f;
    float scoreY = glutGet(GLUT_WINDOW_HEIGHT) - 30.0f;
    renderText(scoreX, scoreY, GLUT_BITMAP_HELVETICA_18, scoreText.c_str(), 1.0f, 1.0f, 0.0f);

    if (gameOver) {
        const char* gameOverMessage = "GAME OVER - Press ESC to Exit";
        float msgX = 50.0f;
        float msgY = glutGet(GLUT_WINDOW_HEIGHT) / 2.0f;
        renderText(msgX, msgY, GLUT_BITMAP_TIMES_ROMAN_24, gameOverMessage, 1.0f, 0.0f, 0.0f);
    }

    drawStars();
    glutSwapBuffers();
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    float aspect_ratio = (float)w / (float)h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, aspect_ratio, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
}

void initGL() {
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    GLfloat light_ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    GLfloat light_diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    GLfloat light_specular[] = { 0.6f, 0.6f, 0.6f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
    GLfloat mat_ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    GLfloat mat_specular[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    GLfloat mat_shininess[] = { 10.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
    initSphereTexture();
}

static float lastSpawnTime = 0.0f;
static int lastTime = 0;

void timer(int value) {
    if (!gameOver) {
        int currentTime = glutGet(GLUT_ELAPSED_TIME);
        float deltaTime = (float)(currentTime - lastTime)/1000.0f;
        lastTime = currentTime;
        if (deltaTime > 0.1f) {
            deltaTime = 0.016f;
        }
        currentRocketX = lerp(currentRocketX, targetRocketX, deltaTime, MOVEMENT_SMOOTHNESS);
        currentRocketY = lerp(currentRocketY, targetRocketY, deltaTime, MOVEMENT_SMOOTHNESS);
        if (rollTimer < rollDuration) {
            rollTimer += deltaTime;
            currentRollAngle = lerp(currentRollAngle, targetRollAngle, deltaTime, MOVEMENT_SMOOTHNESS);
            if (rollTimer >= rollDuration) {
                targetRollAngle = 0.0f;
            }
        } else {
            currentRollAngle = lerp(currentRollAngle, 0.0f, deltaTime, MOVEMENT_SMOOTHNESS / 2.0f);
        }
        rocketX = currentRocketX;
        rocketY = currentRocketY;
        float currentSpeed = rocketSpeed;
        if (isBoosting) {
            currentSpeed *= BOOST_FACTOR;
        }
        rocketZ -= currentSpeed * deltaTime;
        rocketSpeed += rocketAcceleration * deltaTime;
        if (rocketSpeed < 0.0f) rocketSpeed = 0.0f;
        lastSpawnTime += deltaTime;
        if (lastSpawnTime >= SPAWN_INTERVAL) {
            spawnObject();
            lastSpawnTime = 0.0f;
            SPAWN_INTERVAL = 1.0f + (rand() % 10 / 10.0f);
        }
        float recycleZThreshold = rocketZ + RECYCLE_DISTANCE_BEHIND_PLAYER;
        for (int i = 0; i < MAX_GAME_OBJECTS; ++i) {
            if (gameObjects[i].isActive) {
                if (gameObjects[i].z > recycleZThreshold) {
                    gameObjects[i].isActive = false;
                }
            }
        }
        flameRotationAngle += FLAME_ROTATION_SPEED * deltaTime;
        if (flameRotationAngle >= 360.0f) {
            flameRotationAngle -= 360.0f;
        }
        checkCollisions();
    }
    for (int i = 0; i < MAX_STARS; ++i) {
        if (stars[i].z > rocketZ + 20.0f) {
            stars[i].z = rocketZ - starfieldDepth - (rand() % 50);
            stars[i].x = rocketX + (rand() % 2000 / 10.0f) - 100.0f;
            stars[i].y = rocketY + (rand() % 2000 / 10.0f) - 100.0f;
        }
    }
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(720, 1080);
    glutInitWindowPosition(50, 50);
    glutCreateWindow("Roket 3D Sederhana - OpenGL GLUT");
    initGL();
    initStars();
    initGameObjects();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutTimerFunc(0, timer, 0);
    glutKeyboardFunc(handleKeyboard);
    glutKeyboardUpFunc(handleKeyboardUp);
    glutMainLoop();
    glDeleteTextures(1, &sphereTextureID);
    return 0;
}
