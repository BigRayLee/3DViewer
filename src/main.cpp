#include <vector>
#include <sys/time.h>
#include "ModelRead.h"
#include "HLOD.h"
#include "Display.h"
#include "MeshSimplifier.h"
#include "Chrono.h"

const float errSimplify = 0.01;
const unsigned TargetCubeIndexCount = 1 << 15;

using namespace std;

/**
 * @param   arg1 file path of 3D model
 * @param   arg2 enable/disable quantization
 * @param   arg3 maximum level of multi-resolution model (optional)
 * @param   arg4 error threshold for mesh simplification (optional)
 * @return  Description of the return value.
 */

int main(int argc, char *argv[])
{
    string filePath = argv[1];

    /* Read geometry data from model */
    ModelReader *modelReader = new ModelReader;
    TimerStart();
    modelReader->InputModel(filePath);
    TimerStop("Model Read time: ");

    TimerStart();
    if (!modelAttriSatus.hasNormal)
    {
        modelReader->CalculateNormals();
    }
    TimerStop("Nomral Calculation time: ");

    /* Set the LOD level automatically */
    int level = 2;
    float errorThreshold = errSimplify;
    if (argc < 5)
    {
        while (((1 << level) * (1 << level) * TargetCubeIndexCount) < 3 * modelReader->triCount)
        {
            level++;
        }
    }
    else
    {
        level = atoi(argv[3]);
        errorThreshold = atof(argv[4]);
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
    cout << "LOD: " << level << " ";
    TimerStop("build time: ");
    cout << "cell: " << multiResoModel.lods[0]->cubeTable.size() << " faces: "
         << multiResoModel.lods[0]->CalculateTriangleCounts() << " vertices: " << multiResoModel.lods[0]->CalculateVertexCounts() << endl;

    delete modelReader;

    HLODConsructor(&multiResoModel, level, errorThreshold);

    gettimeofday(&end, NULL);
    GetElapsedTime(start, end, "\nModel Reading and Multi-Resolution model build time: ");

    /* Display */
    cout << "\nAdpative LOD Rendering..." << endl;
    Display(multiResoModel, level);
    return 0;
}