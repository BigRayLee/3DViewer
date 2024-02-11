#include "BoundingBoxDraw.h"

BoundingBoxDraw::BoundingBoxDraw(){}

BoundingBoxDraw::~BoundingBoxDraw() {}

void BoundingBoxDraw::InitBuffer(Cube& cube, float length){
    /* Compute the verteices of Cube */
    cube.CalculateBBXVertex(length);

    glGenVertexArrays(1, &bbxVAO);
    glGenBuffers(1, &bbxVBO);
    glGenBuffers(1, &bbxEBO);

    /* Bind buffer */
    glBindVertexArray(bbxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, bbxVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bbxEBO);    

    glBufferData(GL_ARRAY_BUFFER, 30 * 1024 * 1024, NULL, GL_DYNAMIC_DRAW); 
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 30 * 1024 * 1024, NULL, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(cube.bbxVertices), cube.bbxVertices);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(SC_INDICES_BBX), SC_INDICES_BBX);

    glBindBuffer(GL_ARRAY_BUFFER, 0); 
    glBindVertexArray(0); 
}

void BoundingBoxDraw::SetShader(Shader *bbxShader, glm::mat4 projection, glm::mat4 view, glm::mat4 model){
    bbxShader->Use();
    bbxShader->SetMat4("projection",projection);
    bbxShader->SetMat4("view",view);
    bbxShader->SetMat4("model", model);
}

void BoundingBoxDraw::RenderBiggestBBX(Cube& cube, Shader *bbxShader, float length[3], glm::mat4 projection, glm::mat4 view, glm::mat4 model){
    glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
    
    bbxShader->Use();
    bbxShader->SetMat4("projection",projection);
    bbxShader->SetMat4("view",view);
    bbxShader->SetMat4("model", model);
    
    glLineWidth(2.0f);

    glBindVertexArray(bbxVAO);
    glDrawElementsBaseVertex(GL_LINES, 24, GL_UNSIGNED_INT, (GLvoid*)(0),  0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void BoundingBoxDraw::Render(Cube& cube, Shader* bbxShader, float length, int level){
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    uint64_t ijk_info = (uint64_t) (cube.coord[0]) | ((uint64_t)(cube.coord[1])<<16) | ((uint64_t)(cube.coord[2]) << 32) | ((uint64_t)(level) << 48);
    if(vboOffset.count(ijk_info) == 0 && eboOffset.count(ijk_info) == 0){
        vertexOffset = vertexOffset + 8 * 3 * sizeof(float);
        idxOffset = idxOffset + 24 * sizeof(uint32_t);
        vboOffset.insert(make_pair(ijk_info, vertexOffset));
        eboOffset.insert(make_pair(ijk_info, idxOffset));

        cube.CalculateBBXVertex(length);
        glBindBuffer(GL_ARRAY_BUFFER, bbxVBO);
        glBindVertexArray(bbxVAO);

        glBufferSubData(GL_ARRAY_BUFFER, vertexOffset, sizeof(cube.bbxVertices), cube.bbxVertices);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, idxOffset, sizeof(SC_INDICES_BBX), SC_INDICES_BBX);
    }
    
    bbxShader->Use();
    glBindBuffer(GL_ARRAY_BUFFER, bbxVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bbxEBO);
    glBindVertexArray(bbxVAO);
    
    glLineWidth(2.0);
    glDrawElementsBaseVertex(GL_LINES, 24, GL_UNSIGNED_INT, (GLvoid*)(eboOffset[ijk_info]),  vboOffset[ijk_info]/(3 *sizeof(float)));
    
    glBindVertexArray(0); 
    glBindBuffer(GL_ARRAY_BUFFER, 0); 
    glPolygonMode( GL_FRONT_AND_BACK, GL_FILL);
};


void BoundingBoxDraw::FlushBuffer(Cube& cube){
    // bind buffers
    glBindVertexArray(bbxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, bbxVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bbxEBO);    

    // allocate the buffer based on the LRU structure
    glBufferData(GL_ARRAY_BUFFER, 10 * 1024 * 1024, NULL, GL_DYNAMIC_DRAW); 
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 10 * 1024 * 1024, NULL, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(cube.bbxVertices), cube.bbxVertices);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(SC_INDICES_BBX), SC_INDICES_BBX);

    glBindBuffer(GL_ARRAY_BUFFER, 0); 
    glBindVertexArray(0); 
}