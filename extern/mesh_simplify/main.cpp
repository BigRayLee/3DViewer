#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>

#include "version.h"
#include "aabb.h"
#include "transform.h"
#include "viewer.h"
#include "mesh_io.h"
#include "mesh_grid.h"
#include "mesh_utils.h"
#include "mesh_stats.h"
#include "mesh_optimize.h"
#include "chrono.h"
#include "shaders.h"
#include "myosotis.h"


void syntax(char *argv[])
{
	printf("Syntax : %s mesh_file_name [max_level]\n", argv[0]);
}

int main(int argc, char **argv)
{
	if (argc <= 1)
	{
		syntax(argv);
		return(EXIT_FAILURE);
	}
	
	/* Load and process mesh */
	timer_start();
	MBuf data;
	Mesh mesh;
	{
		size_t len = strlen(argv[1]);
		const char* ext = argv[1] + (len-3);
		if (strncmp(ext, "obj", 3) == 0)
		{
			if (load_obj(argv[1], data, mesh))
			{
				printf("Error reading Wavefront file.\n");
				return (EXIT_FAILURE);
			}
		}
		else if (strncmp(ext, "ply", 3) == 0)
		{
			if (load_ply(argv[1], data, mesh))
			{
				printf("Error reading PLY file.\n");
				return (EXIT_FAILURE);
			}
		}
		else 
		{
			printf("Unsupported file type extension: %s\n", ext);
			return (EXIT_FAILURE);
		}
	}
	
	printf("Triangles : %d Vertices : %d\n", mesh.index_count / 3,
			mesh.vertex_count);
	timer_stop("loading mesh");

	/* Input mesh stat and optimization */
	if (argc > 2 && *argv[2] == '1')
	{
		timer_start();
		meshopt_statistics("Raw", data, mesh);
		timer_start();
		meshopt_optimize(data, mesh);
		timer_stop("optimize mesh");
		meshopt_statistics("Optimized", data, mesh);
	}

	/* Computing mesh normals */
	if (!(data.vtx_attr & VtxAttr::NML))
	{
		timer_start();
		printf("Computing normals.\n");
		compute_mesh_normals(mesh, data);
		timer_stop("compute_mesh_normals");
	}

	/* Computing mesh bounds */
	timer_start();
	Aabb bbox = compute_mesh_bounds(mesh, data);
	Vec3 model_center = (bbox.min + bbox.max) * 0.5f;
	Vec3 model_extent = (bbox.max - bbox.min);
	float model_size = max(model_extent);
	timer_stop("compute_mesh_bounds");

	/* Building mesh_grid */
	timer_start();
	int max_level = atoi(argv[3]);
	float step = model_size / (1 << max_level);
	Vec3 base = bbox.min;
	LOD mg(base, step, max_level);
	mg.build_from_mesh(data, mesh);
	timer_stop("split_mesh_with_grid");
	
	/* Main window and context */
	Myosotis app;
	
	if (!app.init(1920, 1080))
	{
		return (EXIT_FAILURE);
	}

	/* Init camera position */	
	app.viewer.target = model_center;
	Vec3 start_pos = (model_center + 2.f * Vec3(0, 0, model_size));
	app.viewer.camera.set_position(start_pos);
	app.viewer.camera.set_near(0.001 * model_size);
	app.viewer.camera.set_far(100 * model_size);
	

	glEnable(GL_DEBUG_OUTPUT);

	/* Upload mesh */	

	/* Index buffer */
	GLuint idx;
	glGenBuffers(1, &idx);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idx);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		mesh.index_count * sizeof(uint32_t),
		data.indices + mesh.index_offset,
		GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	/* Position buffer */
	GLuint pos;
	glGenBuffers(1, &pos);
	glBindBuffer(GL_ARRAY_BUFFER, pos);
	glBufferData(GL_ARRAY_BUFFER,
			mesh.vertex_count * sizeof(Vec3),
			data.positions + mesh.vertex_offset,
			GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	/* Normal buffer */
	GLuint nml;
	glGenBuffers(1, &nml);
	glBindBuffer(GL_ARRAY_BUFFER, nml);
	glBufferData(GL_ARRAY_BUFFER,
			mesh.vertex_count * sizeof(Vec3),
			data.normals + mesh.vertex_offset,
			GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	/* Mesh grid Index buffer */
	GLuint mg_idx;
	glGenBuffers(1, &mg_idx);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mg_idx);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		mg.data.idx_capacity * sizeof(uint32_t),
		mg.data.indices,
		GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	/* Mesh grid Position buffer */
	GLuint mg_pos;
	glGenBuffers(1, &mg_pos);
	glBindBuffer(GL_ARRAY_BUFFER, mg_pos);
	glBufferData(GL_ARRAY_BUFFER,
			mg.data.vtx_capacity * sizeof(Vec3),
			mg.data.positions,
			GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	/* Mesh grid Normal buffer */
	GLuint mg_nml;
	glGenBuffers(1, &mg_nml);
	glBindBuffer(GL_ARRAY_BUFFER, mg_nml);
	glBufferData(GL_ARRAY_BUFFER,
			mg.data.vtx_capacity * sizeof(Vec3),
			mg.data.normals,
			GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	/* Setup VAOs */

	/* Default VAO */
	GLuint default_vao;
	glGenVertexArrays(1, &default_vao);
	glBindVertexArray(default_vao);
	glBindBuffer(GL_ARRAY_BUFFER, pos);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GL_FLOAT), 
				(void *)0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, nml);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GL_FLOAT),
				(void *)0);
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idx);
	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	/* Mesh grid Default VAO */
	GLuint mg_default_vao;
	glGenVertexArrays(1, &mg_default_vao);
	glBindVertexArray(mg_default_vao);
	glBindBuffer(GL_ARRAY_BUFFER, mg_pos);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GL_FLOAT), 
				(void *)0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, mg_nml);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GL_FLOAT),
				(void *)0);
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mg_idx);
	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	/* Vertex fetch VAO */
	GLuint fetch_vao;
	glGenVertexArrays(1, &fetch_vao);
	glBindVertexArray(fetch_vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idx);
	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	/* Vertex pull VAO */
	GLuint null_vao;
	glGenVertexArrays(1, &null_vao);
	glBindVertexArray(null_vao);
	glBindVertexArray(0);

	/* Setup programs */
	
	GLint mesh_prg = create_shader("./shaders/default.vert", 
					  "./shaders/default.frag");
	if (mesh_prg < 0) 
	{
		return EXIT_FAILURE;
	}
	
	GLint fetch_mesh_prg = create_shader("./shaders/fetch_mesh.vert", 
					  "./shaders/default.frag");
	if (fetch_mesh_prg < 0) 
	{
		return EXIT_FAILURE;
	}
	
	GLint nml_prg = create_shader("./shaders/face_normals.vert", 
					  "./shaders/face_normals.frag");
	if (nml_prg < 0) 
	{
		return EXIT_FAILURE;
	}
	
	/* Setup some rendering options */

	//glDisable(GL_CULL_FACE);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* Main loop */
	TArray<uint32_t> to_draw;
	while (!app.should_close()) {
		
		app.new_frame();

		
		// glClearColor(app.cfg.clear_color.x, app.cfg.clear_color.y, 
		// 	     app.cfg.clear_color.z, app.cfg.clear_color.w);
		glClearColor(1.0, 1.0, 1.0, 1.0);
		//glClearColor(252.0/255.0, 227.0/255.0, 236.0/255.0, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		 glEnable(GL_DEBUG_OUTPUT);
		glDisable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CW);

		//gl depth test
		glEnable(GL_DEPTH_TEST | GL_DEPTH_BUFFER_BIT);
		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LESS);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
		
		if (app.cfg.wireframe_mode)
		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			app.cfg.smooth_shading = true;
		}
		else
		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}

		/* Update uniform data */
		Mat4 proj = app.viewer.camera.view_to_clip();	
		Mat4 vm = app.viewer.camera.world_to_view();
		Vec3 camera_pos = app.viewer.camera.get_position();
		if (app.cfg.level > max_level) app.cfg.level = max_level; 
		
		/* Draw mesh */
		if (app.cfg.adaptative_lod)
		{
			if (!app.cfg.freeze_vp)
			{
				Vec3 vp = app.viewer.camera.get_position();
				float *pvm = NULL;
				if (app.cfg.frustum_cull) 
				{
					Mat4 proj_vm = app.viewer.camera.world_to_clip();
					pvm = &proj_vm(0,0);
				}
				to_draw.clear();
				mg.select_cells_from_view_point(vp, app.cfg.kappa, pvm, to_draw);
				app.stat.drawn_cells = to_draw.size;
			}



			glUseProgram(mesh_prg);
			glBindVertexArray(mg_default_vao);
			glUniformMatrix4fv(0, 1, 0, &(vm.cols[0][0])); 
			glUniformMatrix4fv(1, 1, 0, &(proj.cols[0][0]));
			glUniform3fv(2, 1, &camera_pos[0]);
			glUniform1i(3, app.cfg.smooth_shading);
			
			
			app.stat.drawn_tris = 0;

			for (int i = to_draw.size - 1; i >= 0; --i)
			{
				Mesh& mesh = mg.cells[to_draw[i]];
				CellCoord coord =  mg.cell_coords[to_draw[i]];
				if (app.cfg.colorize_lod)
				{
					glUniform1i(4, coord.lod);
					int variation = (coord.x & 1) + 2 * (coord.y & 1) + 4 * (coord.z & 1);
					glUniform1i(5, variation);
				}
				else
				{
					/* Fake */
					glUniform1i(4, -1);
					glUniform1i(5, 0);
				}
				glDrawElementsBaseVertex(GL_TRIANGLES, 
					mesh.index_count, 
					GL_UNSIGNED_INT, 
					(void*)(mesh.index_offset * sizeof(uint32_t)),
					mesh.vertex_offset);
				app.stat.drawn_tris += mesh.index_count / 3;
				//Rui
				app.stat.gpu = float(mesh.index_offset * sizeof(uint32_t) + mesh.vertex_offset * sizeof(float) * 2)/float(8 * 1024 * 1024);
			}
			glBindVertexArray(0);
		}
		else
		{
			glUseProgram(mesh_prg);
			glBindVertexArray(mg_default_vao);
			glUniformMatrix4fv(0, 1, 0, &(vm.cols[0][0])); 
			glUniformMatrix4fv(1, 1, 0, &(proj.cols[0][0]));
			glUniform3fv(2, 1, &camera_pos[0]);
			glUniform1i(3, app.cfg.smooth_shading);
			int cell_counts = mg.cell_counts[app.cfg.level];
			int cell_offset = mg.cell_offsets[app.cfg.level];

			app.stat.drawn_tris = 0;

			for (int i = 0; i < cell_counts; ++i)
			{
				Mesh& mesh = mg.cells[cell_offset + i];
				//printf("Drawing cell %i with %d and %d\n",
				//		i, mesh.index_offset, mesh.vertex_offset);
				glDrawElementsBaseVertex(GL_TRIANGLES, 
					mesh.index_count, 
					GL_UNSIGNED_INT, 
					(void*)(mesh.index_offset * sizeof(uint32_t)),
					mesh.vertex_offset);
				app.stat.drawn_tris += mesh.index_count / 3;
			}
			glBindVertexArray(0);
			//glUseProgram(fetch_mesh_prg);
			//glBindVertexArray(fetch_vao);
			//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, pos);
			//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, nml);
			//glUniformMatrix4fv(0, 1, 0, &(vm.cols[0][0])); 
			//glUniformMatrix4fv(1, 1, 0, &(proj.cols[0][0]));
			//glUniform3fv(2, 1, &camera_pos[0]);
			//glUniform1i(3, app.cfg.smooth_shading);
			//glDrawElements(GL_TRIANGLES, mesh.index_count,
			//		GL_UNSIGNED_INT, 0);
			//glBindVertexArray(0);
			
			//glUseProgram(mesh_prg);
			//glBindVertexArray(default_vao);
			//glUniformMatrix4fv(0, 1, 0, &(vm.cols[0][0])); 
			//glUniformMatrix4fv(1, 1, 0, &(proj.cols[0][0]));
			//glUniform3fv(2, 1, &camera_pos[0]);
			//glUniform1i(3, app.cfg.smooth_shading);
			//for (size_t i = 0; i < ; ++i)
			//{
			//	glDrawElements(GL_TRIANGLES, mesh.index_count,
			//		GL_UNSIGNED_INT, 0);
			//}
			//glBindVertexArray(0);
		}


		/* Draw normals */
		if (app.cfg.draw_normals)
		{
			glUseProgram(nml_prg);
			glBindVertexArray(null_vao);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, idx);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, pos);
			glUniformMatrix4fv(0, 1, 0, &(vm.cols[0][0])); 
			glUniformMatrix4fv(1, 1, 0, &(proj.cols[0][0]));
			glUniform3fv(2, 1, &camera_pos[0]);
			glDrawArrays(GL_LINES, 0, 2 * mesh.index_count / 3);
			glBindVertexArray(0);
		}
		
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		
		glfwSwapBuffers(app.window);
	
	}

	/* Cleaning */
	app.clean();
	
	return (EXIT_SUCCESS);
}
