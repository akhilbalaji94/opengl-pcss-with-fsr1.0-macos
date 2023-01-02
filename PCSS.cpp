// Defined before OpenGL and GLUT includes to avoid deprecation message in OSX
#define GL_SILENCE_DEPRECATION
#include <string>
#include <iostream>
#include <sstream>
#include <math.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "cyCodeBase/cyTriMesh.h"
#include "cyCodeBase/cyMatrix.h"
#include "cyCodeBase/cyGL.h"
#include "cyCodeBase/cyPoint.h"
#include "cyCodeBase/cySampleElim.h"
#include "lodepng.h"

#define PI 3.14159265

int width = 1920, height = 1080;
cy::TriMesh objectMesh;
cy::TriMesh lightMesh;
cy::TriMesh cubeMesh;
cy::GLTextureCubeMap skybox;
cy::GLSLProgram prog, progPlane, progLight, progShadowMap, progSkybox, progFSRUpscale, progFSRSharpen, progUpscale;
bool doObjRotate = false, doObjZoom = false, doLightRotate = false, doPlaneRotate = false;
double rotX = 5.23599, rotY = 0, distZ = 4;
float objRotX = 5.23599, objRotY= 0, objDistZ = 100.0f;
double lastX, lastY;
double lightX = 5.23599, lightY = 150, lightZ = 0;
float lightScale = 0.25;
float sharpenLevel = 0.0f;
double shadowBias = -0.0000025;
unsigned int objNum = 6;
unsigned int textureWidth = 512, textureHeight = 512;
unsigned int renderWidth = 1280, renderHeight = 720;
unsigned int shadowTextureWidth = 1280, shadowTextureHeight = 720;
unsigned int skyboxtextureWidth = 2048, skyboxtextureHeight = 2048;
std::vector<cy::Vec3f> processedVertices, processedNormals;
std::vector<cy::Vec3f> processedVerticesPlane;
std::vector<cy::Vec3f> processedNormalsPlane;
std::vector<cy::Vec2f> processedTexCoords;
std::vector<cy::Vec2f> processedTexCoordsPlane;
std::vector<cy::Vec3f> processedVerticesLight;
std::vector<cy::Vec3f> processedVerticesCube;
std::vector<cy::Vec2f> processedTexCoordsCube;
std::vector<unsigned char> diffuseTextureData;
std::vector<unsigned char> specularTextureData;
std::vector<std::string> skyboxTexturesNames;
std::vector<cy::Matrix4f> modelTransforms;
GLuint specularTexID, diffuseTexID, tbo;
GLuint msaaID, msaaFBO, msaaColorRBO, msaaDepthRBO;
GLuint msaaFSRID, msaaFSRFBO, msaaFSRColorRBO, msaaFSRDepthRBO;
GLuint normalFSRID, normalFSRFBO, normalFSRColorRBO, normalFSRDepthRBO;
cy::WeightedSampleElimination<cy::Point2f, float, 2, int> wse;
GLfloat poissonDiskArray[16][2];
int numSamples = 16;
bool doMSAA = true;
bool doFSR = false;
bool doUpscale = false;
bool doFSRSharpen = true;
bool doPCSS = true;

GLclampf Red = 0.0f, Green = 0.0f, Blue = 0.0f, Alpha = 1.0f;

double deg2rad (double degrees) {
    return degrees * 4.0 * atan (1.0) / 180.0;
}

bool compileShaders() {
    bool shaderSuccess = prog.BuildFiles("shader.vert", "shader.frag");
    return shaderSuccess;
}

bool compilePlaneShaders() {
    bool shaderSuccess = progPlane.BuildFiles("shaderPlane.vert", "shaderPlane.frag");
    return shaderSuccess;
}

bool compileLightShaders() {
    bool shaderSuccess = progLight.BuildFiles("shaderLight.vert", "shaderLight.frag");
    return shaderSuccess;
}

bool compileDepthShaders() {
    bool shaderSuccess = progShadowMap.BuildFiles("shaderDepth.vert", "shaderDepth.frag");
    return shaderSuccess;
}

bool compileFSRSUpscaleShaders() {
    bool shaderSuccess = progShadowMap.BuildFiles("shaderDepth.vert", "shaderDepth.frag", "fsrPass.geom");
    return shaderSuccess;
}

bool compileFSRSSharpenShaders() {
    bool shaderSuccess = progShadowMap.BuildFiles("shaderDepth.vert", "shaderDepth.frag", "fsrPass.geom");
    return shaderSuccess;
}

bool compileCubeShaders() {
    bool shaderSuccess = progSkybox.BuildFiles("shaderCube.vert", "shaderCube.frag");
    return shaderSuccess;
}

bool compileUpscaleShaders() {
    bool shaderSuccess = progUpscale.BuildFiles("shaderFSR.vert", "shaderUpscale.frag");
    return shaderSuccess;
}

bool compileFSRUpscaleShaders() {
    bool shaderSuccess = progFSRUpscale.BuildFiles("shaderFSR.vert", "shaderFSRUpscale.frag");
    return shaderSuccess;
}

bool compileFSRSharpenShaders() {
    bool shaderSuccess = progFSRSharpen.BuildFiles("shaderFSR.vert", "shaderFSRSharpen.frag");
    return shaderSuccess;
}

void computeVerticesPlane() {
    processedVerticesPlane.push_back(cy::Vec3f(100.0f, 0.0f, -100.0f));
    processedVerticesPlane.push_back(cy::Vec3f(-100.0f, 0.0f, -100.0f));
    processedVerticesPlane.push_back(cy::Vec3f(-100.0f, 0.0f, 100.0f));
    processedVerticesPlane.push_back(cy::Vec3f(-100.0f, 0.0f, 100.0f));
    processedVerticesPlane.push_back(cy::Vec3f(100.0f, 0.0f, 100.0f));
    processedVerticesPlane.push_back(cy::Vec3f(100.0f, 0.0f, -100.0f));
}

void computeNormalsPlane() {
    processedNormalsPlane.push_back(cy::Vec3f(0.0f, 1.0f, 0.0f));
    processedNormalsPlane.push_back(cy::Vec3f(0.0f, 1.0f, 0.0f));
    processedNormalsPlane.push_back(cy::Vec3f(0.0f, 1.0f, 0.0f));
    processedNormalsPlane.push_back(cy::Vec3f(0.0f, 1.0f, 0.0f));
    processedNormalsPlane.push_back(cy::Vec3f(0.0f, 1.0f, 0.0f));
    processedNormalsPlane.push_back(cy::Vec3f(0.0f, 1.0f, 0.0f));
}

void computePoissonDisk() {
	std::vector<cy::Point2f> inputPoints(1000);
	for (size_t i = 0; i < inputPoints.size(); i++) {
		inputPoints[i].x = (float)(2 * rand() - RAND_MAX) / RAND_MAX;
		inputPoints[i].y = (float)(2 * rand() - RAND_MAX) / RAND_MAX;
	}
	std::vector<cy::Point2f> outputPoints;
	outputPoints.resize(numSamples);
	wse.Eliminate(inputPoints.data(), inputPoints.size(), outputPoints.data(), outputPoints.size());
    for (size_t i = 0; i < numSamples; i++) {
        poissonDiskArray[i][0] = outputPoints[i].x;
        poissonDiskArray[i][1] = outputPoints[i].y;
    }
}

void computeModelTransforms() {
    for (size_t j = 0; j < 3; j++) {
        for (size_t i = 0; i < 6; i++)
        {        
            cy::Matrix4f modelTransform = cy::Matrix4f::Translation(cy::Vec3f(-80 + i * 30.0f, 0.0f, j * 30.0f)) * cy::Matrix4f::RotationXYZ(deg2rad(-90), 0.0f, 0.0f);
            modelTransforms.push_back(modelTransform);
        }
    }
}

void processNormalKeyCB(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GL_TRUE);
            break;
        case GLFW_KEY_F6:
            std::cout << "Recompiling shaders..." << std::endl;
            if (!compileShaders()) {
                std::cout << "Error Recompiling shaders" << std::endl;
            }
            if (!compilePlaneShaders()) {
                std::cout << "Error Recompiling shaders" << std::endl;
            }
            if (!compileLightShaders()) {
                std::cout << "Error Recompiling shaders" << std::endl;
            }
            if (!compileDepthShaders()) {
                std::cout << "Error Recompiling shaders" << std::endl;
            }
            break;
        case GLFW_KEY_UP:
            lightScale += 0.01f;
            std::cout << "Light Scale: " << lightScale << std::endl;
            break;
        case GLFW_KEY_DOWN:
            lightScale -= 0.01f;
            std::cout << "Light Scale: " << lightScale << std::endl;
            break;
        case GLFW_KEY_A:
            shadowBias += 0.000001f;
            std::cout << "Shadow Bias: " << shadowBias << std::endl;
            break;
        case GLFW_KEY_D:
            shadowBias -= 0.000001f;
            std::cout << "Shadow Bias: " << shadowBias << std::endl;
            break;
        case GLFW_KEY_LEFT:
            if (objNum < 18)
            {
                objNum += 1;
                std::cout << "Objects: " << objNum << std::endl;
            }
            break;
        case GLFW_KEY_RIGHT:
            if (objNum > 1)
            {
                objNum -= 1;
                std::cout << "Objects: " << objNum << std::endl;
            }
            break;
        case GLFW_KEY_LEFT_CONTROL:
        case GLFW_KEY_RIGHT_CONTROL:
            doLightRotate = true;
            break;
        case GLFW_KEY_LEFT_ALT:
        case GLFW_KEY_RIGHT_ALT:
            doPlaneRotate = true;
            break;
        case GLFW_KEY_M:
            doMSAA = !doMSAA;
            std::cout << "MSAA: " << doMSAA << std::endl;
            break;
        case GLFW_KEY_F:
            doFSR = !doFSR;
            std::cout << "FSR Upscale: " << doFSR << std::endl;
            break;
        case GLFW_KEY_S:
            doFSRSharpen = !doFSRSharpen;
            std::cout << "FSR Sharpen: " << doFSRSharpen << std::endl;
            break;
        case GLFW_KEY_P:
            doPCSS = !doPCSS;
            std::cout << "PCSS:  " << doPCSS << std::endl;
            break;
        case GLFW_KEY_SPACE:
            doUpscale = !doUpscale;
            std::cout << "Upscale: " << doUpscale << std::endl;
            break;
        case GLFW_KEY_1:
            renderWidth = ceil(width/1.3f);
            renderHeight = ceil(height/1.3f);
            std::cout << "Render Width: " << renderWidth << std::endl;
            std::cout << "Render Height: " << renderHeight << std::endl;
            break;
        case GLFW_KEY_2:
            renderWidth = ceil(width/1.5f);
            renderHeight = ceil(height/1.5f);
            std::cout << "Render Width: " << renderWidth << std::endl;
            std::cout << "Render Height: " << renderHeight << std::endl;
            break;
        case GLFW_KEY_3:
            renderWidth = ceil(width/1.7f);
            renderHeight = ceil(height/1.7f);
            std::cout << "Render Width: " << renderWidth << std::endl;
            std::cout << "Render Height: " << renderHeight << std::endl;
            break;
        case GLFW_KEY_4:
            renderWidth = ceil(width/2.0f);
            renderHeight = ceil(height/2.0f);
            std::cout << "Render Width: " << renderWidth << std::endl;
            std::cout << "Render Height: " << renderHeight << std::endl;
            break;
        case GLFW_KEY_5:
            sharpenLevel = 0.0f;
            std::cout << "Sharpen Level: " << sharpenLevel << std::endl;
            break;
        case GLFW_KEY_6:
            sharpenLevel = 0.67f;
            std::cout << "Sharpen Level: " << sharpenLevel << std::endl;
            break;
        case GLFW_KEY_7:
            sharpenLevel = 1.34f;
            std::cout << "Sharpen Level: " << sharpenLevel << std::endl;
            break;
        case GLFW_KEY_8:
            sharpenLevel = 2.00f;
            std::cout << "Sharpen Level: " << sharpenLevel << std::endl;
            break;
        }

    } else if (action == GLFW_RELEASE) {
        switch (key) {
        case GLFW_KEY_LEFT_CONTROL:
        case GLFW_KEY_RIGHT_CONTROL:
            doLightRotate = false;
            break;
        case GLFW_KEY_LEFT_ALT:
        case GLFW_KEY_RIGHT_ALT:
            doPlaneRotate = false;
            break;
        }
    }
}

static void error_callback(int error, const char* description)
{
    std::cerr << "Error: " << description << std::endl;
}

void processMouseButtonCB(GLFWwindow* window, int button, int action, int mods)
{
    double xpos, ypos;
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        doObjZoom = true;
        glfwGetCursorPos(window, &xpos, &ypos);
        lastX = xpos;
        lastY = ypos;
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
        doObjZoom = false;
        glfwGetCursorPos(window, &xpos, &ypos);
        lastX = xpos;
        lastY = ypos;
    } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        doObjRotate = true;
        glfwGetCursorPos(window, &xpos, &ypos);
        lastX = xpos;
        lastY = ypos;
    } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        doObjRotate = false;
        glfwGetCursorPos(window, &xpos, &ypos);
        lastX = xpos;
        lastY = ypos;
    }
}

void processMousePosCB(GLFWwindow* window, double xpos, double ypos)
{
    double xDiff = lastX - xpos;
    double yDiff = lastY - ypos;
    if (doLightRotate && doObjRotate) {
        lightX += yDiff * 0.005;
        // lightZ += xDiff * 0.005;
    }
    // Calculate camera zoom based on mouse movement in y direction
    if (doObjZoom && !doPlaneRotate) {
        objDistZ += yDiff * 0.1;
    }

    // Calculate plane zoom based on mouse movement in y direction
    if (doPlaneRotate && doObjZoom) {
        distZ += yDiff * 0.05;
    }

    // Calculate camera rotation based on mouse movement in x direction
    if (doObjRotate && !doLightRotate && !doPlaneRotate) {
        objRotX += yDiff * 0.005;
        objRotY += xDiff * 0.005;
    }

    // Calculate plane rotation based on mouse movement in x direction
    if (doPlaneRotate && doObjRotate) {
        rotX -= yDiff * 0.005;
        rotY -= xDiff * 0.005;
    }

    lastX  = xpos;
    lastY = ypos;
}

int main(int argc, char** argv)
{
    glfwSetErrorCallback(error_callback);
    
    // Initialize GLFW
    if (!glfwInit())
        exit(EXIT_FAILURE);
        
    // Create a windowed mode window and its OpenGL context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(width, height, "PCSS", NULL, NULL);

    // Get framebuffer size
    glfwGetFramebufferSize(window, &width, &height);
    std::cout << "Framebuffer size: " << width << "x" << height << std::endl;

    // Set render resolution
    renderWidth = ceil(width/1.3f);
    renderHeight = ceil(height/1.3f);
    shadowTextureWidth = renderWidth;
    shadowTextureHeight = renderHeight;

    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window);

    //Initialize GLEW
    glewExperimental = GL_TRUE; 
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        std::cout << "Error: " << glewGetErrorString(err) << std::endl;
        return 1;
    }

    // Print and test OpenGL context infos
    std::cout << glGetString(GL_VERSION) << std::endl;
    std::cout << glGetString(GL_RENDERER) << std::endl;

    // Setup GLFW callbacks
    glfwSetKeyCallback(window, processNormalKeyCB);
    glfwSetMouseButtonCallback(window, processMouseButtonCB);
    glfwSetCursorPosCallback(window, processMousePosCB);
    glfwSwapInterval(1);

    //OpenGL initializations
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    // CY_GL_REGISTER_DEBUG_CALLBACK;

    //Load object mesh and texture mapping
    bool meshSuccess = objectMesh.LoadFromFileObj(argv[1]);
    if (!meshSuccess)
    {
        std::cout << "Error loading mesh" << std::endl;
        return 1;
    }

    //Compute normals
    if (objectMesh.HasNormals()) {
        objectMesh.ComputeNormals();
    }

    //Load vertices for object faces
    for (int i = 0; i < objectMesh.NF(); i++) 
    {
        cy::TriMesh::TriFace face = objectMesh.F(i);
        cy::TriMesh::TriFace textureface;
        if (objectMesh.HasTextureVertices()) {
            textureface = objectMesh.FT(i);
        }
        for (int j = 0; j < 3; j++) 
        {
            cy::Vec3f pos = objectMesh.V(face.v[j]);
            processedVertices.push_back(pos);

            if (objectMesh.HasNormals()) {
                cy::Vec3f norm = objectMesh.VN(face.v[j]);
                processedNormals.push_back(norm);
            }

            if (objectMesh.HasTextureVertices()) {
                cy::Vec3f texcoord = objectMesh.VT(textureface.v[j]);
                processedTexCoords.push_back(cy::Vec2f(texcoord.x, texcoord.y));
            }
        }
    }

    if (objectMesh.NM() > 0) {
        // Load diffuse texture data from png file
        bool ambientTextureError = lodepng::decode(diffuseTextureData, textureWidth, textureHeight, objectMesh.M(0).map_Kd.data);
        if (ambientTextureError)
        {
            std::cout << "Error loading texture" << std::endl;
            return 1;
        }

        // Load sepcular texture data from png file
        bool specularTextureError = lodepng::decode(specularTextureData, textureWidth, textureHeight, objectMesh.M(0).map_Ks.data);
        if (specularTextureError)
        {
            std::cout << "Error loading texture" << std::endl;
            return 1;
        }
    }

    //Compute bounding box
    objectMesh.ComputeBoundingBox();
    cy::Vec3f objectCenter = (objectMesh.GetBoundMax() - objectMesh.GetBoundMin())/2;
    cy::Vec3f objectOrigin = objectMesh.GetBoundMin() + objectCenter;
    
    // Compute vertices and tex coord for plane
    computeVerticesPlane();
    computeNormalsPlane();

    computePoissonDisk();
    computeModelTransforms();

        // Compute vertices and tex coord for cube
    meshSuccess = cubeMesh.LoadFromFileObj("cube.obj");
    if (!meshSuccess)
    {
        std::cout << "Error loading cube mesh" << std::endl;
        return 1;
    }

    //Compute normals
    if (cubeMesh.HasNormals()) {
        cubeMesh.ComputeNormals();
    }

    // Compute bounding box of cube
    cubeMesh.ComputeBoundingBox();
    cy::Vec3f cubeCenter = (cubeMesh.GetBoundMax() - cubeMesh.GetBoundMin())/2;
    cy::Vec3f cubeOrigin = cubeMesh.GetBoundMin() + cubeCenter;

    for (int i = 0; i < cubeMesh.NF(); i++) 
    {
        cy::TriMesh::TriFace face = cubeMesh.F(i);
        for (int j = 0; j < 3; j++) 
        {
            cy::Vec3f pos = cubeMesh.V(face.v[j]);
            processedVerticesCube.push_back(pos);
        }
    }
    // Set skybox texture names
    skyboxTexturesNames.push_back("posx");
    skyboxTexturesNames.push_back("negx");
    skyboxTexturesNames.push_back("posy");
    skyboxTexturesNames.push_back("negy");
    skyboxTexturesNames.push_back("posz");
    skyboxTexturesNames.push_back("negz");

    // Load textures of the skybox
    for (int i = 0; i < 6; i++) 
    {
        std::vector<unsigned char> skyboxtexture;
        bool textureError = lodepng::decode(skyboxtexture, skyboxtextureWidth, skyboxtextureHeight, "cubemap/cubemap_" + skyboxTexturesNames[i] + ".png");
        if (textureError)
        {
            std::cout << "Error loading texture" << std::endl;
            return 1;
        }
        skybox.SetImage((cy::GLTextureCubeMap::Side)i, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, &skyboxtexture[0], skyboxtextureWidth, skyboxtextureHeight);

    }
    skybox.BuildMipmaps();
    skybox.SetSeamless(true);
    skybox.SetFilteringMode(GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR);
    skybox.SetMaxAnisotropy();

    //Load light mesh for light source
    meshSuccess = lightMesh.LoadFromFileObj("sphere.obj");
    if (!meshSuccess)
    {
        std::cout << "Error loading mesh" << std::endl;
        return 1;
    }

    //Compute normals
    lightMesh.ComputeNormals();

    //Load vertices for light faces
    for (int i = 0; i < lightMesh.NF(); i++) 
    {
        cy::TriMesh::TriFace face = lightMesh.F(i);
        for (int j = 0; j < 3; j++) 
        {
            cy::Vec3f pos = lightMesh.V(face.v[j]);
            processedVerticesLight.push_back(pos);
        }
    }

    //Create vertex array object
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    //Create VBO for vertex data
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f)*processedVertices.size(), &processedVertices[0], GL_STATIC_DRAW);
    
    //Create VBO for normal data
    GLuint nbo;
    glGenBuffers(1, &nbo);
    glBindBuffer(GL_ARRAY_BUFFER, nbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f)*processedNormals.size(), &processedNormals[0], GL_STATIC_DRAW);
    
    // Create VBO for plane vertex data
    GLuint vboplane;
    glGenBuffers(1, &vboplane);
    glBindBuffer(GL_ARRAY_BUFFER, vboplane);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f)*processedVerticesPlane.size(), &processedVerticesPlane[0], GL_STATIC_DRAW);

    GLuint nboplane;
    glGenBuffers(1, &nboplane);
    glBindBuffer(GL_ARRAY_BUFFER, nboplane);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f)*processedNormalsPlane.size(), &processedNormalsPlane[0], GL_STATIC_DRAW);

    //Create VBO for plane texture coordinates data
    GLuint tboplane;
    glGenBuffers(1, &tboplane);
    glBindBuffer(GL_ARRAY_BUFFER, tboplane);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f)*processedTexCoordsPlane.size(), &processedTexCoordsPlane[0], GL_STATIC_DRAW);

    //Create VBO for cube vertex data
    GLuint vbocube;
    glGenBuffers(1, &vbocube);
    glBindBuffer(GL_ARRAY_BUFFER, vbocube);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f)*processedVerticesCube.size(), &processedVerticesCube[0], GL_STATIC_DRAW);

    //Create VBO for light vertex data
    GLuint vbolight;
    glGenBuffers(1, &vbolight);
    glBindBuffer(GL_ARRAY_BUFFER, vbolight);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f)*processedVerticesLight.size(), &processedVerticesLight[0], GL_STATIC_DRAW);

    GLuint sampler_state = 0;
    glGenSamplers(1, &sampler_state);
    glSamplerParameteri(sampler_state, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glSamplerParameteri(sampler_state, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glSamplerParameteri(sampler_state, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(sampler_state, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(sampler_state, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
    glSamplerParameteri(sampler_state, GL_TEXTURE_COMPARE_FUNC, GL_GEQUAL);
    glSamplerParameteri(sampler_state, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);

    
    if (objectMesh.NM() > 0) {
        //Create VBO for texture data
        glGenBuffers(1, &tbo);
        glBindBuffer(GL_ARRAY_BUFFER, tbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec2f)*processedTexCoords.size(), &processedTexCoords[0], GL_STATIC_DRAW);

        //Send diffuse texture data to GPU
        glGenTextures(1, &diffuseTexID);
        glBindTexture(GL_TEXTURE_2D, diffuseTexID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, &diffuseTextureData[0]);
        
        //Set diffuse texture parameters
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        //Send specular texture data to GPU
        glGenTextures(1, &specularTexID);
        glBindTexture(GL_TEXTURE_2D, specularTexID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, &specularTextureData[0]);

        //Set ambient texture parameters
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    // Create Multi-Sampled Frame Buffer
    glGenTextures(1, &msaaID);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msaaID);
    glTexParameterf(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_GENERATE_MIPMAP, GL_TRUE);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 8, GL_RGBA, width, height, GL_TRUE);

    // Create a multisampling color render buffer object and a multisampling depth render buffer object
    glGenRenderbuffers(1, &msaaColorRBO);
    glBindRenderbuffer(GL_RENDERBUFFER,msaaColorRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 8, GL_RGBA8, width, height);
    glGenRenderbuffers(1, &msaaDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaDepthRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 8, GL_DEPTH_COMPONENT, width, height);

    // Create and bind the multisampling frame buffer
    glGenFramebuffers(1, &msaaFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO);

    // Attach the multisampling texture, color render buffer, depth render buffer to FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, msaaID, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msaaColorRBO);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msaaDepthRBO);

    // Switch back to back buffer
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    // Create Multi-Sampled Frame Buffer for Lower Render Resolution Image
    glGenTextures(1, &msaaFSRID);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msaaFSRID);
    glTexParameterf(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_GENERATE_MIPMAP, GL_TRUE);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 8, GL_RGBA, renderWidth, renderHeight, GL_TRUE);

    // Create a multisampling color render buffer object and a multisampling depth render buffer object
    glGenRenderbuffers(1, &msaaFSRColorRBO);
    glBindRenderbuffer(GL_RENDERBUFFER,msaaFSRColorRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 8, GL_RGBA8, renderWidth, renderHeight);
    glGenRenderbuffers(1, &msaaFSRDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaFSRDepthRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 8, GL_DEPTH_COMPONENT, renderWidth, renderHeight);

    // Create and bind the multisampling frame buffer
    glGenFramebuffers(1, &msaaFSRFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFSRFBO);

    // Attach the multisampling texture, color render buffer, depth render buffer to FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, msaaFSRID, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msaaFSRColorRBO);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msaaFSRDepthRBO);

    // Switch back to back buffer
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    //Setup Shader program
    std::cout << "Compiling shaders..." << std::endl;
    bool shaderSuccess = compileShaders();
    if (!shaderSuccess)
    {
        std::cout << "Error compiling shaders" << std::endl;
        return 1;
    }

    //Setup Shader program
    std::cout << "Compiling plane shaders..." << std::endl;
    bool planeshaderSuccess = compilePlaneShaders();
    if (!planeshaderSuccess)
    {
        std::cout << "Error compiling plane shaders" << std::endl;
        return 1;
    }


    //Setup Shader program
    std::cout << "Compiling light shaders..." << std::endl;
    bool lightshaderSuccess = compileLightShaders();
    if (!lightshaderSuccess)
    {
        std::cout << "Error compiling light shaders" << std::endl;
        return 1;
    }

    //Setup Shader program
    std::cout << "Compiling depth shaders..." << std::endl;
    bool depthshaderSuccess = compileDepthShaders();
    if (!depthshaderSuccess)
    {
        std::cout << "Error compiling depth shaders" << std::endl;
        return 1;
    }

    //Setup Shader program
    std::cout << "Compiling cube shaders..." << std::endl;
    bool cubeshaderSuccess = compileCubeShaders();
    if (!cubeshaderSuccess)
    {
        std::cout << "Error compiling cube shaders" << std::endl;
        return 1;
    }

    //Setup Shader program
    std::cout << "Compiling upscale shaders..." << std::endl;
    bool upscalehaderSuccess = compileUpscaleShaders();
    if (!upscalehaderSuccess)
    {
        std::cout << "Error compiling upscale shaders" << std::endl;
        return 1;
    }

    //Setup Shader program
    std::cout << "Compiling FSR upscale shaders..." << std::endl;
    bool upscaleFSRshaderSuccess = compileFSRUpscaleShaders();
    if (!upscaleFSRshaderSuccess)
    {
        std::cout << "Error compiling FSR upscale shaders" << std::endl;
        return 1;
    }

    //Setup Shader program
    std::cout << "Compiling FSR sharpen shaders..." << std::endl;
    bool sharpensFSRshaderSuccess = compileFSRSharpenShaders();
    if (!sharpensFSRshaderSuccess)
    {
        std::cout << "Error compiling FSR sharpen shaders" << std::endl;
        return 1;
    }

    //Loop until the user closes the window
    while (!glfwWindowShouldClose(window))
    {
        // Calculate camera transformations for object
        cy::Matrix4f transMatrix = cy::Matrix4f::Translation(-objectOrigin);
        cy::Matrix4f cameraPosTrans = cy::Matrix4f::RotationXYZ(objRotX, objRotY, 0.0f);
        cy::Vec4f cameraPos =  cameraPosTrans * cy::Vec4f(0, 0, objDistZ, 0);
        cy::Matrix4f viewMatrix = cy::Matrix4f::View(cy::Vec3f(cameraPos.x, cameraPos.y, cameraPos.z), cy::Vec3f(0, 0, 0), cy::Vec3f(0, 1, 0));
        cy::Matrix4f projMatrix = cy::Matrix4f::Perspective(deg2rad(60), float(width)/float(height), 0.1f, 1000.0f);
        cy::Matrix4f m =  transMatrix;
        cy::Matrix4f mv = viewMatrix * m;
        cy::Matrix4f mvp = projMatrix * mv;
        cy::Matrix4f mv_3x3_it = mv.GetInverse().GetTranspose();
        cy::Vec4f lightPos = cy::Matrix4f::RotationXYZ(lightX, 0.0f, lightZ) * cy::Vec4f(0, lightY, 0, 0);
        
        // Render to texture binding
        cy::GLRenderDepth2D renderBuffer;
        renderBuffer.Initialize(false, shadowTextureWidth, shadowTextureHeight, GL_DEPTH_COMPONENT32F);
        if (!renderBuffer.IsReady())
        {
            std::cout << "Error initializing render buffer" << std::endl;
            return 1;
        }
        renderBuffer.BindTexture();
        renderBuffer.SetTextureWrappingMode(GL_CLAMP, GL_CLAMP);
        renderBuffer.SetTextureFilteringMode(GL_LINEAR, GL_LINEAR);

        renderBuffer.Bind();
        progShadowMap.Bind();

        cy::Matrix4f modelLight = cy::Matrix4f::Identity();
        cy::Matrix4f viewLight = cy::Matrix4f::View(cy::Vec3f(lightPos.x, lightPos.y, lightPos.z), cy::Vec3f(0, 0, 0), cy::Vec3f(0, 1, 0));
        cy::Matrix4f projLight = cy::Matrix4f::Perspective(deg2rad(50), float(width)/float(height), 0.1f, 1000.0f);;
        cy::Matrix4f mvpLight = projLight * viewLight * modelLight;
        progShadowMap["mvp"] = mvpLight;
        progShadowMap.SetAttribBuffer("pos", vbo, 3);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(Red, Green, Blue, Alpha);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        for(int i = 0; i < objNum; i++)
        {
            std::stringstream ss;
            std::string index;
            ss << i; 
            index = ss.str(); 
            progShadowMap[("model[" + index + "]").c_str()] = modelTransforms[i];
        }
        glDrawArraysInstanced(GL_TRIANGLES, 0, processedVertices.size(), objNum);
        glDisable(GL_CULL_FACE);

        // Check if render buffer is ready
        if (!renderBuffer.IsComplete())
        {
            std::cout << "Error completing render buffer" << std::endl;
            return 1;
        }
        renderBuffer.Unbind();

        // Switch to MSAA frame buffer
        if (!doUpscale) {
            if(doMSAA)
            {
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, msaaFBO);
                glViewport(0, 0, width, height);
            } else {
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                glViewport(0, 0, width, height);
            }
        } else {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, msaaFSRFBO);
            glViewport(0, 0, renderWidth, renderHeight);
        }

        //Set Program and Program Attributes for teapot
        prog.Bind();
        prog["mvp"] = mvp;
        prog["mv"] = mv;
        prog["m"] = m;
        prog["mv_3x3_it"] = mv_3x3_it;
        prog["camera_pos"] = cameraPos;
        prog["light_pos"] =  viewMatrix * cy::Vec4f(lightPos.x, lightPos.y, lightPos.z, 0);
        prog["world_light"] = cy::Vec3f(lightPos.x, lightPos.y, lightPos.z);
        prog["mvp_light"] = cy::Matrix4f::Translation(cy::Vec3f(0.5f, 0.5f, 0.5f - shadowBias)) * cy::Matrix4f::Scale(0.5f, 0.5f, 0.5f) * mvpLight;
        prog["light_world_size"] = lightScale;
        prog["do_pcss"] = doPCSS;
        int uniformIndex = glGetUniformLocation(prog.GetID(), "poissonDisk");
        glUniform2fv(uniformIndex, 2, (GLfloat *)&poissonDiskArray); 
        prog.SetAttribBuffer("pos", vbo, 3);
        if (objectMesh.HasNormals()) {
            prog.SetAttribBuffer("norm", nbo, 3);
        }
        GLint shadmowMapTexLoc = glGetUniformLocation(prog.GetID(), "model_shadow");
        GLint shadowCMPMapTexLoc = glGetUniformLocation(prog.GetID(), "model_shadow_cmp");
        glUniform1i(shadmowMapTexLoc, 0);
        glUniform1i(shadowCMPMapTexLoc, 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderBuffer.GetTextureID());
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, renderBuffer.GetTextureID());
        glBindSampler(1, sampler_state);

        if (objectMesh.NM() > 0) {
            prog.SetAttribBuffer("txc", tbo, 2);
            GLint diffuseTexLoc = glGetUniformLocation(prog.GetID(), "texture_diffuse");
            GLint specularTexLoc = glGetUniformLocation(prog.GetID(), "texture_specular");
            glUniform1i(diffuseTexLoc, 2);
            glUniform1i(specularTexLoc, 3);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, diffuseTexID);
            if (doUpscale && doFSR) {
                glSamplerParameterf(diffuseTexLoc, GL_TEXTURE_LOD_BIAS, -ceil(log2(float(renderWidth)/float(width))));
            } else {
                glSamplerParameterf(diffuseTexLoc, GL_TEXTURE_LOD_BIAS, 0);
            }
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, specularTexID);
            if (doUpscale && doFSR) {
                glSamplerParameterf(specularTexLoc, GL_TEXTURE_LOD_BIAS, -ceil(log2(float(renderWidth)/float(width))));
            } else {
                glSamplerParameterf(specularTexLoc, GL_TEXTURE_LOD_BIAS, 0);
            }
        }
        for(int i = 0; i < objNum; i++)
        {
            std::stringstream ss;
            std::string index;
            ss << i; 
            index = ss.str(); 
            prog[("model[" + index + "]").c_str()] = modelTransforms[i];
            prog[("mv_3x3_it[" + index + "]").c_str()] = (viewMatrix * m * modelTransforms[i]).GetInverse().GetTranspose();
            prog[("mnorm[" + index + "]").c_str()] = (m * modelTransforms[i]).GetInverse().GetTranspose();
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(Red, Green, Blue, Alpha);
        glDrawArraysInstanced(GL_TRIANGLES, 0, processedVertices.size(), objNum);
        // glDrawArrays(GL_TRIANGLES, 0, processedVertices.size());

        // Set Program and Program Attributes for plane
        progPlane.Bind();
        progPlane["mvp"] = mvp;
        progPlane["m"] = m;
        progPlane["mv"] = mv;
        progPlane["mv_3x3_it"] = mv_3x3_it;
        progPlane["camera_pos"] = cameraPos;
        progPlane["light_pos"] =  viewMatrix * cy::Vec4f(lightPos.x, lightPos.y, lightPos.z, 0);
        progPlane["world_light"] = cy::Vec3f(lightPos.x, lightPos.y, lightPos.z);
        progPlane["mvp_light"] = cy::Matrix4f::Translation(cy::Vec3f(0.5f, 0.5f, 0.5f - shadowBias)) * cy::Matrix4f::Scale(0.5f, 0.5f, 0.5f) * mvpLight;
        progPlane["light_world_size"] = lightScale;
        progPlane["do_pcss"] = doPCSS;
        uniformIndex = glGetUniformLocation(progPlane.GetID(), "poissonDisk");
        glUniform2fv(uniformIndex, 2, (GLfloat *)&poissonDiskArray); 
        progPlane.SetAttribBuffer("pos", vboplane, 3);
        progPlane.SetAttribBuffer("norm", nboplane, 3);

        shadmowMapTexLoc = glGetUniformLocation(progPlane.GetID(), "model_shadow");
        shadowCMPMapTexLoc = glGetUniformLocation(progPlane.GetID(), "model_shadow_cmp");
        glUniform1i(shadmowMapTexLoc, 0);
        glUniform1i(shadowCMPMapTexLoc, 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderBuffer.GetTextureID());
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, renderBuffer.GetTextureID());
        glBindSampler(1, sampler_state);

        // Render texture to plane
        glDrawArrays(GL_TRIANGLES, 0, processedVerticesPlane.size());

        // Draw light source
        progLight.Bind();
        modelLight = cy::Matrix4f::RotationXYZ(lightX, 0.0f, lightZ) * cy::Matrix4f::Translation(cy::Vec3f(0, lightY, 0)) * cy::Matrix4f::Scale(lightScale);
        viewLight = viewMatrix;
        projLight = projMatrix;
        mvpLight = projLight * viewLight * modelLight;
        progLight["mvp"] = mvpLight;
        progLight.SetAttribBuffer("pos", vboplane, 3);

        glDrawArrays(GL_TRIANGLES, 0, processedVerticesPlane.size());

        // Draw the cube
        progSkybox.Bind();
        progSkybox["mvp"] = projMatrix * viewMatrix * m * cy::Matrix4f::Translation(-cubeOrigin) * cy::Matrix4f::Scale(50);
        progSkybox.SetAttribBuffer("pos", vbocube, 3);
        progSkybox.SetUniform("env", 2);

        // Draw calls
        glDepthMask(GL_FALSE);
        glDrawArrays(GL_TRIANGLES, 0, processedVerticesCube.size());
        glDepthMask(GL_TRUE);
        
        if (!doUpscale && doMSAA) {
            // Blit MSAA framebuffer to backbuffer
            glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(Red, Green, Blue, Alpha);
            glDrawBuffer(GL_BACK);
            glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        if (doUpscale && !doFSR) {
            // Blit MSAA framebuffer to normal framebuffer
            cy::GLRenderTexture2D renderBuffer;
            renderBuffer.Initialize(true, 3, renderWidth, renderHeight);
            renderBuffer.SetTextureFilteringMode(GL_LINEAR, GL_LINEAR);
            renderBuffer.Bind();

            glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFSRFBO);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(Red, Green, Blue, Alpha);
            // glDrawBuffer(GL_BACK);
            glBlitFramebuffer(0, 0, renderWidth, renderHeight, 0, 0, renderWidth, renderHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);

            progUpscale.Bind();
            GLint framebufferTexLoc = glGetUniformLocation(progUpscale.GetID(), "texture_sampler");
            glUniform1i(framebufferTexLoc, 0);
            glActiveTexture(GL_TEXTURE0);
            // Get the texture attachment of the framebuffer
            glBindTexture(GL_TEXTURE_2D, renderBuffer.GetTextureID());
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(Red, Green, Blue, Alpha);
            glDepthMask(GL_FALSE);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glDepthMask(GL_TRUE);
        }

        if (doUpscale && doFSR) {
            // Blit MSAA framebuffer to normal framebuffer
            cy::GLRenderTexture2D renderBuffer;
            renderBuffer.Initialize(true, 3, renderWidth, renderHeight);
            renderBuffer.SetTextureFilteringMode(GL_LINEAR, GL_LINEAR);
            renderBuffer.Bind();

            glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFSRFBO);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(Red, Green, Blue, Alpha);
            // glDrawBuffer(GL_BACK);
            glBlitFramebuffer(0, 0, renderWidth, renderHeight, 0, 0, renderWidth, renderHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            
            cy::GLRenderTexture2D renderUpscaleBuffer;
            if(doFSRSharpen) {
                renderUpscaleBuffer.Initialize(true, 3, width, height);
                renderUpscaleBuffer.SetTextureFilteringMode(GL_LINEAR, GL_LINEAR);
                renderUpscaleBuffer.Bind();
            } else {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, width, height);
            }

            progFSRUpscale.Bind();
            GLint framebufferTexLoc = glGetUniformLocation(progFSRUpscale.GetID(), "Source");
            glUniform1i(framebufferTexLoc, 0);
            glActiveTexture(GL_TEXTURE0);
            // Get the texture attachment of the framebuffer
            glBindTexture(GL_TEXTURE_2D, renderBuffer.GetTextureID());
            progFSRUpscale["input_resolution"] = cy::Vec2f(renderWidth, renderHeight);
            progFSRUpscale["target_resolution"] = cy::Vec2f(width, height);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(Red, Green, Blue, Alpha);
            glDepthMask(GL_FALSE);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glDepthMask(GL_TRUE);
            if (doFSRSharpen) {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, width, height);
                progFSRSharpen.Bind();
                GLint framebufferTexLoc = glGetUniformLocation(progFSRSharpen.GetID(), "Source");
                glUniform1i(framebufferTexLoc, 0);
                glActiveTexture(GL_TEXTURE0);
                // Get the texture attachment of the framebuffer
                glBindTexture(GL_TEXTURE_2D, renderUpscaleBuffer.GetTextureID());
                progFSRSharpen["output_resolution"] = cy::Vec2f(width, height);
                progFSRSharpen["sharpen_level"] = sharpenLevel;
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glClearColor(Red, Green, Blue, Alpha);
                glDepthMask(GL_FALSE);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                glDepthMask(GL_TRUE);
            }

        }

        renderBuffer.Delete();
        // Swap front and back buffers
        glfwSwapBuffers(window);

        // Poll for and process events
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}