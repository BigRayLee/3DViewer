#include <vector>
#include <time.h>
#include <sys/time.h>

#include "ModelRead.h"
#include "LOD.h"
#include "Display.h"
#include "MeshSimplifier.h"
#include "Chrono.h"

const float errSimplify = 0.01;
const unsigned TargetCubeIndexCount = 1 << 14;

using namespace std;

/**
 * @param   arg1 file path of 3D model
 * @param   arg2 enable/disable quantization 
 * @param   arg3 maximum level of multi-resolution model (optional)
 * @param   arg4 error threshold for mesh simplification (optional)
 * @param   arg5 file path of texture map
 * @return  Description of the return value.
 */

int main(int argc, char *argv[])
{
    string filePath = argv[1];
    string enableQuantization = argv[2];

    bool isQuant = false;
    if(enableQuantization == "-q"){
        isQuant = true;
    }
        
    /* Read geometry data from model */
    ModelReader * modelReader = new ModelReader;
    TimerStart();
    modelReader->InputModel(filePath);
    TimerStop("Model Read time: ");

    /* Set the LOD level automatically */
    int level = 2;
    float errorThreshold = errSimplify;
    if(argc < 5){
        while(((1 << level) * (1 << level) * TargetCubeIndexCount) < 3 * modelReader->triCount)
            level++;
    }
    else{
        level = atoi(argv[3]);
        errorThreshold = atof(argv[4]);
    }

    /* For .ply model, the folder path of texture needs to be indiqued */
    vector<string> texturesPath;
    if (filePath.substr(filePath.length() - 3, filePath.length()) == "ply")
    {
        texturesPath.push_back(argv[argc - 1]);
    }
    else if(filePath.substr(filePath.length() - 3, filePath.length()) == "txt"){
        texturesPath.push_back(argv[argc - 1]);
    }
    else{
        texturesPath = modelReader->texturesPath;
    }
        
    /* Multi-resolution model */
    HLOD multiResoModel;
    multiResoModel.lods[0] = new LOD(level);

    /* Highest resolution LOD construction */
    struct timeval start, end;
    gettimeofday(&start, NULL);

    TimerStart();
    multiResoModel.BuildLODFromInput(modelReader->meshData, modelReader->vertCount, modelReader->triCount);
    
    cout << "\nMulti-Resolution Model building..." << endl;
    cout << "LOD: " << level<<" ";
    TimerStop("build time: ");
    cout<<"cell: " << multiResoModel.lods[0]->cubeTable.size() << " faces: "
         << multiResoModel.lods[0]->CalculateTriangleCounts() << 
        " vertices: " << multiResoModel.lods[0]->CalculateVertexCounts() << endl;

    delete modelReader;
    
    HLODConsructor(&multiResoModel, level, errorThreshold);

    gettimeofday(&end, NULL);
    GetElapsedTime(start, end, "\nModel Reading and Multi-Resolution model build time: ");
    
    /* Mesh quantization */
    // if(isQuant){
    //     for(int i = 0; i <= level; ++i)
    //         for(auto &cb : multiResoModel.lods[i]->cubeTable)
    //             multiResoModel.lods[i]->MeshQuantization(cb.second);
    // }
        
    /* Compute the LOD error */
    //LODErrorCalculation(multiResoModel.lods, level);
    
    /* Display */
    cout << "\nAdpative LOD Rendering..." << endl;
    
    Display(multiResoModel, level, texturesPath, isQuant);
    return 0;
}