#pragma once
#include <math.h>
#include "math/vec3.h"
#include "math/quat.h"
#include "math/mat4.h"
#include "math/geometry.h"
#include "math/aabb.h"
#include "Frustum.h"
#include <glm/glm.hpp>

struct Camera 
{
	enum Fov   {Horizontal = 0, Vertical = 1};
	enum Space {View = 0, World = 1};

	/**
	 * Constructs a default camera.
	 *
	 *  - camera posittion is world origin
	 *  - camera rotation is identity
	 *  - lense axis is centered over sensor
	 *  - sensor aspect ratio is 1
	 *  - lense fov is 90°
	 *  - projection type is perspective
	 *  - near and far plane are 0.01 and 1000.
	 */
	Camera() = default;
	
	/**
	 * Constructs a camera with given aspect ratio and fov.
	 *
	 *  - camera posittion is world origin
	 *  - camera rotation is identity
	 *  - lense axis is centered over sensor
	 *  - projection type is perpesctive
	 *  - near and far plane are 0.01 and 1000.
	 *
	 * @param aspect_ratio - sensor width / sensor_height.
	 * @param fov - Field of view in degrees.
	 * @param axis - Axis along which fov is understood.
	 */
	Camera(float aspect_ratio, float fov, Fov axis = Vertical);

	/**
	 * Change camera aspect ratio and adapt either horizontal or vertical
	 * fov to avoid distortion.
	 *
	 * @param aspect_ratio - New aspect_ratio.
	 * @param cst_axis - Axis whose fov shall be kept constant.
	 */
	Camera& set_aspect(float aspect_ratio, Fov cst_axis = Vertical);

	/**
	 * Change camera fov.
	 *
	 * @param fov - New fov.
	 * @param axis - Axis along which fov is understood.
	 */
	Camera& set_fov(float fov, Fov axis = Vertical);

	/**
	 * Change lense shift for non centered lenses.
	 *
	 * @param shift_x - Shift ratio along horizontal axis.
	 * @param shiff_y - Shift ratio along vertical axis.
	 *
	 * In practice: If screen is P pixels height and you wish the horizon 
	 *              to appear p pixels from the bottom when the camera is
	 *              leveled set shift_y = 1 - 2 * (p / P).
	 */
	Camera& set_lense_shift(float shift_x, float shift_y);

	/**
	 * Change camera projection type.
	 *
	 * @param IsOrtho - Perspective vs Orthographic projection.
	 */
	Camera& set_orthographic(bool is_ortho);

	/**
	* Get or Set camera position and rotation. 
	*/
	Vec3 get_position() const;
	Quat get_rotation() const;
	Camera& set_position(const Vec3& position);
	Camera& set_rotation(const Quat& rotation);

	/**
	 * Apply a translation to the camera.
	 *
	 * @param t - Vector of translation.
	 * @param coord - Coordinate frame in which t is understood. 
	 */
	Camera& translate(const Vec3& t, Space coord = View);

	/* Apply a rotation to the camera around its center.
	 *
	 * @param delta_rot {Quat} - Additional rotation wrt present.
	 */
	Camera& rotate(const Quat& r);

	/* Roto translate the camera around some pivot point.
	 *
	 * @param r {Quat} - Additional rotation wrt present.
	 * @param pivot - Pivot point in world coordinates.
	 */
	Camera& orbit(const Quat& r, const Vec3& pivot);

	/**
	 * Get and set near and far clip distances for rendering.
	 * - It is your responsability to not set near_plane equal to
	 *   far_plane, or projection matrices will contain NaNs.
	 * - It is legal to set near_plane and far_plane in reversed
	 *   order, or to set any of these two planes to 0 or to +-infty, 
	 *   but one should then understand the impacts, in particular 
	 *   for depth buffer precision.
	 */       
	float get_near() const;
	float get_far() const;
	Camera& set_near(float near_plane);
	Camera& set_far (float far_plane );
	
	/**
	 * Compute matrices between World, View and Clip spaces.
	 * NB) Matrices are recorded in column major order in memory.
	 */
	Mat4 view_to_clip()  const;
	Mat4 clip_to_view()  const;
	Mat4 world_to_view() const;
	Mat4 view_to_world() const;
	Mat4 world_to_clip() const;
	Mat4 clip_to_world() const;

	/**
	 * View ray or world ray starting from the camera and in the direction 
	 * corresponding to the given normalised screen coordinates.
	 *
	 * @param x - normalised horizontal screen coord (left = 0, right = 1)
	 * @param y - normalised vertical screen coord (top = 0, bottom = 1)
	 */
	Ray view_ray_at (float x, float y) const;
	Ray world_ray_at(float x, float y) const;

	/*
	 * Compute view or world coord based on normalised
	 * screen coord and normalised depth.
	 *
	 * @param x - normalised horizontal screen coord (left = 0, right = 1)
	 * @param y - normalised vertical screen coord (top = 0, bottom = 1)
	 * @param depth - normalised depth (as read from depth buffer)
	 */
	Vec3 view_coord_at (float x, float y, float depth) const;
	Vec3 world_coord_at(float x, float y, float depth) const;

	/*transfer Mat4 to glm::mat4*/
	glm::mat4 Mat4_to(Mat4 mat){
		glm::mat4 M(1.0);
		M[0][0] = mat(0, 0);
		M[0][1] = mat(1, 0);
		M[0][2] = mat(2, 0);
		M[0][3] = mat(3, 0);

		M[1][0] = mat(0, 1);
		M[1][1] = mat(1, 1);
		M[1][2] = mat(2, 1);
		M[1][3] = mat(3, 1);

		M[2][0] =  mat(0, 2);
		M[2][1] =  mat(1, 2);
		M[2][2] =  mat(2, 2);
		M[2][3] =  mat(3, 2);

		M[3][0] =  mat(0, 3);
		M[3][1] =  mat(1, 3);
		M[3][2] =  mat(2, 3);
		M[3][3] =  mat(3, 3);
		return M;
	}

	/*transfer Mat4 to glm::mat4*/
	Mat4 GlmMat4_to(glm::mat4 M){
		Mat4 mat;
		mat(0, 0) = M[0][0];
		mat(1, 0) = M[0][1];
		mat(2, 0) = M[0][2];
		mat(3, 0) = M[0][3];

		mat(0, 1) = M[1][0];
		mat(1, 1) = M[1][1];
		mat(2, 1) = M[1][2];
		mat(3, 1) = M[1][3];

		mat(0, 2) = M[2][0];
		mat(1, 2) = M[2][1];
		mat(2, 2) = M[2][2];
		mat(3, 2) = M[2][3];

		mat(0, 3) = M[3][0];
		mat(1, 3) = M[3][1];
		mat(2, 3) = M[3][2];
		mat(3, 3) = M[3][3];
		return mat;
	}

	/* Space configuration */
	Quat  rotation  = Quat::Identity;
	Vec3  position  = Vec3::Zero;
	
	/* Optical configuration */
	CameraFrustum frustum;
};

int is_visible(const Aabb& bbox, const float *pvm);


