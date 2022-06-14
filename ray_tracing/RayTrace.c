#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <math.h>
#include <float.h>
#include <stdlib.h>

#include "Scene.h"
#include "RayTrace.h"
#include "Geometry.h"


// Initialize the image with the background color specified as input.
// width and height corresponds to the dimension of the image.
static void
initImageWithBackground(Color background_color, Color** image,
			int width, int height)
{
  int i;
  int j;
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      image[i][j]._red = background_color._red;
      image[i][j]._green = background_color._green;
      image[i][j]._blue = background_color._blue;
    }
  }
}


// Clamp c's entries between m and M.
static void clamp(Color* c, float m, float M) {
  c->_red = fminf(fmaxf(c->_red, m), M);
  c->_green = fminf(fmaxf(c->_green, m), M);
  c->_blue = fminf(fmaxf(c->_blue, m), M);
}


// Complete
// Given a ray (origin, direction), check if it intersects a given
// sphere.
// Return 1 if there is an intersection, 0 otherwise.
// *t contains the distance to the closest intersection point, if any.
static int
hitSphere(Vector3 origin, Vector3 direction, Sphere sphere, float* t)
{
	float discriminant;//Determine if it is in contact with a circle.
  float ud;//inner product
  float c;//length of u_sph
  Vector3 u_sph;//this is vector from origin to center.

	sub(origin,sphere._center,&u_sph);
	computeNorm(u_sph,&c);
	computeDotProduct(u_sph,direction,&ud);
	discriminant = ud*ud - c*c + sphere._radius*sphere._radius;
	*t = -ud - sqrt(discriminant);

	if(discriminant>=0 && *t>=0)return 1;
  return 0;
}


// Check if the ray defined by (scene._camera, direction) is intersecting
// any of the spheres defined in the scene.
// Return 0 if there is no intersection, and 1 otherwise.
//
// If there is an intersection:
// - the position of the intersection with the closest sphere is computed
// in hit_pos
// - the normal to the surface at the intersection point in hit_normal
// - the diffuse color and specular color of the intersected sphere
// in hit_color and hit_spec
static int
hitScene(Vector3 origin, Vector3 direction, Scene scene,
	 Vector3* hit_pos, Vector3* hit_normal,
	 Color* hit_color, Color* hit_spec)
{
  Vector3 o = origin;
  Vector3 d = direction;

  float t_min = FLT_MAX;
  int hit_idx = -1;
  Sphere hit_sph;

  // For each sphere in the scene
  int i;
  for (i = 0; i < scene._number_spheres; ++i) {
    Sphere curr = scene._spheres[i];
    float t = 0.0f;
    if (hitSphere(o, d, curr, &t)) {
      if (t < t_min) {
	hit_idx = i;
	t_min = t;
	hit_sph = curr;
      }
    }
  }

  if (hit_idx == -1) return 0;

  Vector3 td;
  mulAV(t_min, d, &td);
  add(o, td, hit_pos);

  Vector3 n;
  sub(*hit_pos, hit_sph._center, &n);
  mulAV(1.0f / hit_sph._radius, n, hit_normal);

  // Save the color of the intersected sphere in hit_color and hit_spec
  *hit_color = hit_sph._color;
  *hit_spec = hit_sph._color_spec;

  return 1;
}


// Save the image in a raw buffer (texture)
// The memory for texture is allocated in this function. It needs to
// be freed in the caller.
static void saveRaw(Color** image, int width, int height, GLubyte** texture) {
  int count = 0;
  int i;
  int j;
  *texture = (GLubyte*)malloc(sizeof(GLubyte) * 3 * width * height);

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      unsigned char red = (unsigned char)(image[i][j]._red * 255.0f);
      unsigned char green = (unsigned char)(image[i][j]._green * 255.0f);
      unsigned char blue = (unsigned char)(image[i][j]._blue * 255.0f);

      (*texture)[count] = red;
      count++;

      (*texture)[count] = green;
      count++;

      (*texture)[count] = blue;
      count++;
    }
  }
}


// Complete
// Given an intersection point (hit_pos),
// the normal to the surface at the intersection point (hit_normal),
// and the color (diffuse and specular) terms at the intersection point,
// compute the colot intensity at the point by applying the Phong
// shading model.
// Return the color intensity in *color.
static void
shade(Vector3 hit_pos, Vector3 hit_normal,
	Color hit_color, Color hit_spec, Scene scene, Color* color)
{

	float inner;//inner product
	float fai,faim;//fai is Angle, faim is fai^m
	Vector3 v,light,r;
	float shininess=100.0;//shininess
	Vector3 shadow_pos, shadow_normal;//position and normal of intersection
	Color shadow_color,shadow_spec;//color and spec of intersection
	// Complete

	//find v and c_to_h(-v)
	sub(scene._camera, hit_pos, &v);
	normalize(v, &v);

	// ambient component
	//I_a=K_a*I
	color->_red+=hit_color._red * scene._ambient._red;
	color->_green+=hit_color._green * scene._ambient._green;
	color->_blue+=hit_color._blue * scene._ambient._blue;


	// for each light in the scene
	int l;
	for (l = 0; l < scene._number_lights; l++) {

		sub(scene._lights[l]._light_pos,hit_pos,&light);
		normalize(light, &light);
		// Form a shadow ray and check if the hit point is under
		// direct illumination from the light source

		//もし、円のポジションからlightベクトルの直線を引いた時にものが当たったら、何もせず、当たらなかったらshadingする。
		if(hitScene(hit_pos,light,scene,&shadow_pos,&shadow_normal,&shadow_color,&shadow_spec)==0){

			// diffuse component
			sub(scene._lights[l]._light_pos,hit_pos,&light);
			normalize(light, &light);
			computeDotProduct(hit_normal, light, &inner);

			color->_red+=hit_color._red*scene._lights[l]._light_color._red*fmaxf(inner,0.0f);
			color->_blue+=hit_color._blue*scene._lights[l]._light_color._blue*fmaxf(inner,0.0f);
			color->_green+=hit_color._green*scene._lights[l]._light_color._green*fmaxf(inner,0.0f);

			// specular component

			r._x=-light._x + 2*hit_normal._x*inner;
			r._y=-light._y + 2*hit_normal._y*inner;
			r._z=-light._z + 2*hit_normal._z*inner;

			normalize(r, &r);
			computeDotProduct(v,r,&fai);
			faim=pow(fmaxf(fai,0.0f),shininess);
			color->_red+=hit_spec._red*scene._lights[l]._light_color._red*faim;
			color->_blue+=hit_spec._blue*scene._lights[l]._light_color._blue*faim;
			color->_green+=hit_spec._green*scene._lights[l]._light_color._green*faim;
		}
	}
}


static int rayTrace(Vector3 origin, Vector3 direction_normalized,
		    Scene scene, Color* color)
{
  Vector3 hit_pos;
  Vector3 hit_normal;
  Color hit_color;
  Color hit_spec;
  int hit;

  // does the ray intersect an object in the scene?
  hit =
    hitScene(origin, direction_normalized, scene,
	     &hit_pos, &hit_normal, &hit_color,
	     &hit_spec);

  // no hit
  if (!hit) return 0;

  color->_red = 0;
  color->_green = 0;
  color->_blue = 0;

  // otherwise, apply the shading model at the intersection point
  shade(hit_pos, hit_normal, hit_color, hit_spec, scene, color);

  return 1;
}


void rayTraceScene(Scene scene, int width, int height, GLubyte** texture) {
  Color** image;
  int i;
  int j;

  Vector3 camera_pos;
  float screen_scale;

  image = (Color**)malloc(height * sizeof(Color*));
  for (i = 0; i < height; i++) {
    image[i] = (Color*)malloc(width * sizeof(Color));
  }

  // Init the image with the background color
  initImageWithBackground(scene._background_color, image, width, height);

  // get parameters for the camera position and the screen fov
  camera_pos._x = scene._camera._x;
  camera_pos._y = scene._camera._y;
  camera_pos._z = scene._camera._z;

  screen_scale = scene._scale;


  // go through each pixel
  // and check for intersection between the ray and the scene
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      // Compute (x,y) coordinates for the current pixel
      // in scene space
      float x = screen_scale * j - 0.5f * screen_scale * width;
      float y = screen_scale * i - 0.5f * screen_scale * height;

      // Form the vector camera to current pixel
      Vector3 direction;
      Vector3 direction_normalized;

      direction._x = x - camera_pos._x;
      direction._y = y - camera_pos._y;
      direction._z = -camera_pos._z;

      normalize(direction, &direction_normalized);

      Vector3 origin = scene._camera;
      Color color;
      int hit = rayTrace(origin, direction_normalized, scene, &color);
      if (hit) {
	clamp(&color, 0.f, 1.f);
	image[i][j] = color;
      }
    }
  }

  // save image to texture buffer
  saveRaw(image, width, height, texture);

  for (i = 0; i < height; i++) {
    free(image[i]);
  }

  free(image);
}
