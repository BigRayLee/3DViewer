#include "TextureFile.h"

TextureFile::TextureFile(/* args */)
{
}

TextureFile::~TextureFile()
{
}

void TextureFile::Load2DTextureArray(string texture_paths){
        /*get the texture file path*/
        vector<string> texlist;
        ifstream texfile_list;
        texfile_list.open(texture_paths.c_str());
        string line;
        while(getline(texfile_list, line)){
            string tex_path = base_path + line;
            ifstream file_test;
            file_test.open(tex_path.c_str());
            if(!file_test)
                cout<<tex_path<<" does not exist"<<endl;
            current_tex_num++;
            texlist.push_back(tex_path);
        }
        cout<<"texture number: "<<current_tex_num<<endl;

        /*Initialize Texture array*/
        glGenTextures(1, &arrayTexture);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, arrayTexture);
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB, 500, 500, current_tex_num);

        // glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        /*loading image data*/
        Tex texture[current_tex_num];
        for(uint i = 0; i < current_tex_num; ++i){
          // TIFF *tif; 
          // TIFFRGBAImage img;
          // uint32 *raster;
          // size_t npixels;
          // int imgwidth, imgheight, imgcomponents;

          // int hasABGR = 0;
          // char emsg[1024];

          // tif = TIFFOpen(texlist[i].c_str(), "r");
          // if (tif == NULL){
          //   // fprintf(stderr, "tif == NULL\n");
          //   exit(1);
          // }
          // if (TIFFRGBAImageBegin(&img, tif, 0, emsg)){
          //     npixels = img.width*img.height;
          //     raster = (uint32 *)_TIFFmalloc(npixels*sizeof(uint32));
          //     if (raster != NULL){
          //       if (TIFFRGBAImageGet(&img, raster, img.width, img.height) == 0){
          //           // TIFFError(texlist[i].c_str(), emsg);
          //       exit(1);
          //       }
          //     }
          //     TIFFRGBAImageEnd(&img);
          //     // fprintf(stderr, "Read image %s (%d x %d)\n", texlist[i].c_str(), img.width, img.height);
          //   }
          //   else {
          //     // TIFFError(texlist[i].c_str(), emsg);
          //     exit(1);
          //   }
          //   imgwidth = img.width;
          //   imgheight = img.height;
          
          
          //   /* If cannot directly display ABGR format, we need to reverse the component
          //     ordering in each pixel. :-( */
          //   if (!hasABGR) {
          //     int i;

          //     for (i = 0; i < npixels; i++) {
          //       register unsigned char *cp = (unsigned char *) &raster[i];
          //       int t;

          //       t = cp[3];
          //       cp[3] = cp[0];
          //       cp[0] = t;
          //       t = cp[2];
          //       cp[2] = cp[1];
          //       cp[1] = t;
          //     }
          //   }
            stbi_set_flip_vertically_on_load(true);
            
            texture[i].data = stbi_load(texlist[i].c_str(), &texture[i].width, &texture[i].height, &texture[i].channel, 0);
            //cout<<texlist[i].c_str()<<" "<<texture[i].width<<" "<<texture[i].height<<" "<<texture[i].data[0]<<endl;
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                            0, 
                            0, 0, i,
                            texture[i].width, texture[i].height, 1,
                            GL_RGB,
                            GL_UNSIGNED_BYTE,
                            texture[i].data);
            // free(raster);
            // TIFFClose(tif);

            //stbi_image_free(texture[i].data);
        }
        
        
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

        /*generate the mipmap*/
        // glBindTexture(GL_TEXTURE_2D_ARRAY, arrayTexture);
        // glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        // glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
}

void TextureFile::load(const char * file){
    file_path = file;

    stbi_set_flip_vertically_on_load(true);
    data = stbi_load(file, &width, &height, &nrChannels, 0);
    //cout<<"texture info: "<<width<<" "<<height<<" "<<nrChannels<<endl;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    //texture filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    //mipmap setting
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, max_level);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);

    if(data){
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        /*test the texture mipmap*/
        //testMipMap(texture);
    }
    else
        cout<<"failed to load the texture file"<<endl;

    stbi_image_free(data);
}

void TextureFile::load(const char * file, unsigned int &texture_id){
    file_path = file;

    stbi_set_flip_vertically_on_load(true);
    data = stbi_load(file, &width, &height, &nrChannels, 0);
    cout<<"texture info: "<<file<<" "<<width<<" "<<height<<" "<<nrChannels<<endl;

    // GLubyte* pixels = new GLubyte[width * height * nrChannels];
    // string out_file = string(file).substr(0, file_path.find('.')) + ".png";
    // stbi_write_png(out_file.c_str(), width, height, nrChannels, pixels, 0);

    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    //texture filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    //mipmap setting
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, max_level);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);

    if(data){
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        /*test the texture mipmap*/
        //testMipMap(texture);
    }
    else
        cout<<"failed to load the texture file"<<endl;

    stbi_image_free(data);
}

void TextureFile::loadDDS(const char * file, unsigned int& texture_id){
    // unsigned char header[124];

    // FILE *fp;
    
    // /*open the file*/
    // fp = fopen(file, "rb");
    // if(fp == NULL){
    //     cout<<"open dds file failed"<<endl;
    //     return;
    // }
    
    // /*check the dds file type*/
    // char filecode[4];
    // fread(filecode, 1, 4, fp);
    // if(strncmp(filecode, "DDS ", 4) != 0){
    //     fclose(fp);
    //     return;
    // }

    // /*get surface desc*/
    // fread(&header, 124, 1, fp);

    // unsigned int height         = *(unsigned int*)&(header[8]);
    // unsigned int width          = *(unsigned int*)&(header[12]);
    // unsigned int linearSize     = *(unsigned int*)&(header[16]);
    // unsigned int MipMapCount    = *(unsigned int*)&(header[24]);
    // unsigned int fourCC         = *(unsigned int*)&(header[80]);

    // // cout<<"dds info check : "<<height<<" "<<width<<" "<<linearSize<<" "<<MipMapCount<<" "<<fourCC<<endl;
    // unsigned char * buffer;
    // unsigned int bufsize;
    
    // bufsize = MipMapCount > 1? linearSize * 2 : linearSize;
    // buffer = (unsigned char*)malloc(bufsize * sizeof(unsigned char));
    // fread(buffer, 1, bufsize, fp);

    // /*close file pointer*/
    // fclose(fp);


    // unsigned int components = (fourCC == FOURCC_DXT1) ? 3 : 4;
    // unsigned int format;

    // switch (fourCC)
    // {
    // case FOURCC_DXT1:
    //     format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
    //     break;
    // case FOURCC_DXT3:
    //     format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    //     break;
    // case FOURCC_DXT5:
    //     format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    //     break;
    // default:
    //     cout<<"nono"<<endl;
    //     free(buffer);
    //     return;
    // }

    // /*generate the texture buffer*/
    // glGenTextures(1, &texture_id);
    // glBindTexture(GL_TEXTURE_2D, texture_id);
    // glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    // // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, MipMapCount-1); // opengl likes array length of mipmaps
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // don't forget to enable mipmaping
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // unsigned int blockSize = (format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 8 : 16;
    // unsigned int offset = 0;

    // /*load mipmaps*/
    // if(MipMapCount == 0){
    //     cout<<"mipmap count: "<<MipMapCount<<endl;
    //     unsigned int size = ((width + 3) / 4) * ((height + 3)/4) * blockSize;
    //     glCompressedTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, size, buffer);
    // }
    // else{
    //     cout<<"mipmap count: "<<MipMapCount<<endl;
    //     for(unsigned int level = 0; level < MipMapCount&& (width || height); ++level){
    //     unsigned int size = ((width + 3) / 4) * ((height + 3)/4) * blockSize;
    //     glCompressedTexImage2D(GL_TEXTURE_2D, level, format, width, height, 0, size, buffer + offset);

    //     offset += size;
    //     width /= 2;
    //     height /= 2;
    // }
    // }
    
    // // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, MipMapCount-1);
    // // unbind
	// glBindTexture(GL_TEXTURE_2D, 0);

    // free(buffer);
      // lay out variables to be used
      unsigned char* header;
      
      unsigned int width;
      unsigned int height;
      unsigned int mipMapCount;
      
      unsigned int blockSize;
      unsigned int format;
      
      unsigned int w;
      unsigned int h;
      
      unsigned char* buffer = 0;
      
      // open the DDS file for binary reading and get file size
      FILE* f;
      if((f = fopen(file, "rb")) == 0)
        return;
      fseek(f, 0, SEEK_END);
      long file_size = ftell(f);
      fseek(f, 0, SEEK_SET);
      
      // allocate new unsigned char space with 4 (file code) + 124 (header size) bytes
      // read in 128 bytes from the file
      header = (unsigned char *) malloc(128);
      fread(header, 1, 128, f);
      
      // compare the `DDS ` signature
      if(memcmp(header, "DDS ", 4) != 0)
        return;
      
      // extract height, width, and amount of mipmaps - yes it is stored height then width
      height = (header[12]) | (header[13] << 8) | (header[14] << 16) | (header[15] << 24);
      width = (header[16]) | (header[17] << 8) | (header[18] << 16) | (header[19] << 24);
      mipMapCount = (header[28]) | (header[29] << 8) | (header[30] << 16) | (header[31] << 24);
      
      // figure out what format to use for what fourCC file type it is
      // block size is about physical chunk storage of compressed data in file (important)
      if(header[84] == 'D') {
        switch(header[87]) {
          case '1': // DXT1
            format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
            blockSize = 8;
            break;
          case '3': // DXT3
            format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
            blockSize = 16;
            break;
          case '5': // DXT5
            format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            blockSize = 16;
            break;
          case '0': // DX10
            // unsupported, else will error
            // as it adds sizeof(struct DDS_HEADER_DXT10) between pixels
            // so, buffer = malloc((file_size - 128) - sizeof(struct DDS_HEADER_DXT10));
          default: return;
        }
      } else // BC4U/BC4S/ATI2/BC55/R8G8_B8G8/G8R8_G8B8/UYVY-packed/YUY2-packed unsupported
        return;
      
      // allocate new unsigned char space with file_size - (file_code + header_size) magnitude
      // read rest of file
      buffer =  (unsigned char *)malloc(file_size - 128);
      if(buffer == 0)
        return;
      fread(buffer, 1, file_size, f);
      
      // prepare new incomplete texture
      glGenTextures(1, &texture_id);
      if(texture_id == 0)
        return;
      
      // bind the texture
      // make it complete by specifying all needed parameters and ensuring all mipmaps are filled
      glBindTexture(GL_TEXTURE_2D, texture_id);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipMapCount-1); // opengl likes array length of mipmaps
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // don't forget to enable mipmaping
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      
      // prepare some variables
      unsigned int offset = 0;
      unsigned int size = 0;
      w = width;
      h = height;
      
      // loop through sending block at a time with the magic formula
      // upload to opengl properly, note the offset transverses the pointer
      if( mipMapCount == 0){
        size = ((w+3)/4) * ((h+3)/4) * blockSize;
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, size, buffer + offset);
      }
      // assumes each mipmap is 1/2 the size of the previous mipmap
      else{
          for (unsigned int i=0; i<mipMapCount; i++) {
          if(w == 0 || h == 0) { // discard any odd mipmaps 0x1 0x2 resolutions
            mipMapCount--;
            continue;
          }
          size = ((w+3)/4) * ((h+3)/4) * blockSize;
          glCompressedTexImage2D(GL_TEXTURE_2D, i, format, w, h, 0, size, buffer + offset);
          offset += size;
          w /= 2;
          h /= 2;
          }
      }
      
      // discard any odd mipmaps, ensure a complete texture
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipMapCount-1);
      // unbind
      glBindTexture(GL_TEXTURE_2D, 0);
      
      // easy macro to get out quick and uniform (minus like 15 lines of bulk)
      MemoryFree(buffer);
      MemoryFree(header);
      fclose(f);
}

void TextureFile::testMipMap(GLuint &texture){
    /*based on max level of mesh grid*/
    for(int i = 1; i <= max_level; ++i){
        int factor = 1 << i;
        int width_l = width/factor;
        int height_l = height/factor;
        
        glBindTexture(GL_TEXTURE_2D, texture);
        GLubyte* pixels = new GLubyte[width_l * height_l * nrChannels];
        glGetTexImage(GL_TEXTURE_2D, i, GL_RGB, GL_UNSIGNED_BYTE, pixels);
        //glGetCompressedTexImage(GL_TEXTURE_2D, i, pixels);

        string out_file = file_path.substr(0, file_path.find('.')) + to_string(i) + ".jng";
        //stbi_write_png(out_file.c_str(), width_l, height_l, nrChannels, pixels, 0);
        stbi_write_jpg(out_file.c_str(), width_l, height_l, nrChannels, pixels, 1);
        //stbi_write_bmp(out_file.c_str(), width_l, height_l, nrChannels, pixels);
        free(pixels);
    }
}


/*build texture pyramid*/
void TextureFile::buildTexPyramid(){
    /*based on max level of mesh grid*/
    for(int i = 1; i <= max_level; ++i){
        int factor = 1 << i;
        int width_l = width/factor;
        int height_l = height/factor;
        
        glBindTexture(GL_TEXTURE_2D, texture);
        GLubyte* pixels = new GLubyte[width_l * height_l * nrChannels];
        glGetTexImage(GL_TEXTURE_2D, i, GL_RGB, GL_UNSIGNED_BYTE, pixels);
        //glGetCompressedTexImage(GL_TEXTURE_2D, i, pixels);

        string out_file = file_path.substr(0, file_path.find('.')) + to_string(i) + ".png";
        stbi_write_png(out_file.c_str(), width_l, height_l, nrChannels, pixels, 0);
        //stbi_write_jpg(out_file.c_str(), width_l, height_l, nrChannels, pixels, 1);
        //stbi_write_bmp(out_file.c_str(), width_l, height_l, nrChannels, pixels);
        free(pixels);
    }
}